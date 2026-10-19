#include "InputQueue.hpp"
#include "Predictor.hpp"
#include "Settings.hpp"
#include "Simulator.hpp"

#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace geode::prelude;
using namespace pib;

namespace {
    std::string lastSave;
    int g_saves = 0;

    // The level being played. PlayLayer::get() can't be used: it reads
    // GameManager::m_playLayer, which is misplaced in the 2.2091 bindings.
    // Deliberately a raw pointer: Geode's WeakRef retains the layer and frees
    // it later from whatever code next locks it, which crashes GD. It is
    // cleared in onQuit and when the layer's fields are destroyed.
    PlayLayer* g_playLayer = nullptr;

    PlayLayer* currentPlayLayer() {
        return g_playLayer;
    }

    // A real jump input reached the game during the current tick.
    bool g_inputThisTick = false;

    // Prediction self-check: compares a 30-tick no-input prediction with what
    // the real player actually does.
    constexpr int kCheckTicks = 30;
    struct PendingCheck {
        bool pending = false;
        long long dueTick = 0;
        float x = 0.f;
        float y = 0.f;
        bool predictedDeath = false;
        Gamemode mode = Gamemode::Cube;
    } g_check;
    struct CheckStats {
        int samples = 0;
        double sumError = 0.0;
        double maxError = 0.0;
        int deathsAgreed = 0;
        int deathsMissed = 0;
        int deathsFalse = 0;
    } g_stats;

    void logCheckSummary() {
        log::info(
            "self-check: {} samples, mean error {:.3f}, max error {:.3f} units; deaths predicted+happened {}, "
            "happened but not predicted {}, predicted but didn't happen {}; inputs saved {}",
            g_stats.samples, g_stats.samples ? g_stats.sumError / g_stats.samples : 0.0, g_stats.maxError,
            g_stats.deathsAgreed, g_stats.deathsMissed, g_stats.deathsFalse, g_saves
        );
    }
}

class $modify(PIBPlayLayer, PlayLayer) {
    struct Fields {
        Ref<CCLayer> simContainer;
        Ref<PlayerObject> simPlayer;
        Ref<CCLabelBMFont> status;
        PlayLayer* owner = nullptr;

        // Runs while GD destroys the level.
        ~Fields() {
            if (owner && g_playLayer == owner) g_playLayer = nullptr;
        }
    };

    bool init(GJGameLevel* level, bool dontCreateObjects) {
        inputQueue().reset();
        lastSave.clear();
        if (!PlayLayer::init(level, dontCreateObjects)) return false;
        g_playLayer = this;
        m_fields->owner = this;

        // The simulated player and everything it spawns (particles, trails)
        // live in an invisible layer that shares the object layer's space.
        auto container = CCLayer::create();
        container->setVisible(false);
        m_objectLayer->addChild(container);
        auto sim = PlayerObject::create(1, 1, this, container, true);
        sim->setVisible(false);
        container->addChild(sim);
        m_fields->simContainer = container;
        m_fields->simPlayer = sim;

        auto label = CCLabelBMFont::create("", "chatFont.fnt");
        label->setAnchorPoint({ 0.f, 1.f });
        label->setScale(0.5f);
        label->setOpacity(180);
        label->setPosition({ 4.f, CCDirector::get()->getWinSize().height - 14.f });
        label->setZOrder(1000);
        this->addChild(label);
        m_fields->status = label;
        this->refreshStatus();
        return true;
    }

    PlayerObject* simPlayer() {
        return m_fields->simPlayer;
    }

    void refreshStatus() {
        auto label = m_fields->status.data();
        if (!label) return;
        label->setVisible(settings().showStatus);
        auto text = fmt::format("Input Buffer: {}", currentModeName());
        if (!lastSave.empty()) text += fmt::format("  |  {}", lastSave);
        label->setString(text.c_str());
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (Simulator::get().isSimPlayer(player)) {
            Simulator::get().markDead();
            return;
        }
        if (g_check.pending && player == m_player1 && !player->m_isDead) {
            g_check.pending = false;
            if (g_check.predictedDeath) {
                g_stats.deathsAgreed++;
            }
            else {
                g_stats.deathsMissed++;
                log::warn("self-check: died in {} but the prediction survived", gamemodeName(g_check.mode));
            }
        }
        PlayLayer::destroyPlayer(player, object);
    }

    void playEndAnimationToPos(CCPoint position) {
        if (Simulator::get().active()) {
            Simulator::get().markUnknown();
            return;
        }
        PlayLayer::playEndAnimationToPos(position);
    }

    void resetLevel() {
        inputQueue().flush(this);
        resetRhythm();
        g_check.pending = false;
        PlayLayer::resetLevel();
    }

    void onQuit() {
        inputQueue().reset();
        if (g_playLayer == this) g_playLayer = nullptr;
        PlayLayer::onQuit();
    }
};

namespace {
    void refreshStatus() {
        if (auto layer = currentPlayLayer()) static_cast<PIBPlayLayer*>(layer)->refreshStatus();
    }

    bool shouldIntercept(GJBaseGameLayer* layer, int button) {
        if (button != 1) return false;
        if (settings().maxDelay <= 0) return false;
        if (InputQueue::isFiring() || Simulator::get().active()) return false;
        auto play = currentPlayLayer();
        if (!play || play != layer) return false;
        if (layer->m_useReplay || layer->m_playbackMode != PlaybackMode::Not) return false;
        if (!static_cast<PIBPlayLayer*>(play)->simPlayer()) return false;
        return true;
    }

    // Players a jump input reaches, following GJBaseGameLayer::handleButton.
    std::vector<PlayerObject*> affectedPlayers(GJBaseGameLayer* layer, bool isPlayer1) {
        std::vector<PlayerObject*> players;
        bool twoPlayer = layer->m_levelSettings && layer->m_levelSettings->m_twoPlayerMode;
        bool dual = layer->m_gameState.m_isDualMode;
        auto add = [&](PlayerObject* p) {
            if (p && !p->m_isDead && !p->m_isLocked && !p->m_controlsDisabled) players.push_back(p);
        };
        if (!twoPlayer || isPlayer1) add(layer->m_player1);
        if (dual && (!twoPlayer || !isPlayer1)) add(layer->m_player2);
        return players;
    }

    void finishSelfCheck(GJBaseGameLayer* layer, long long tick) {
        if (!g_check.pending || tick < g_check.dueTick) return;
        g_check.pending = false;
        auto player = layer->m_player1;
        if (!player) return;
        if (g_check.predictedDeath) {
            g_stats.deathsFalse++;
            log::warn("self-check: predicted a death in {} that didn't happen", gamemodeName(g_check.mode));
            return;
        }
        auto dx = player->getPositionX() - g_check.x;
        auto dy = player->getPositionY() - g_check.y;
        auto error = std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy);
        g_stats.samples++;
        g_stats.sumError += error;
        g_stats.maxError = std::max(g_stats.maxError, error);
        log::debug("self-check {}: predicted ({:.2f}, {:.2f}) actual ({:.2f}, {:.2f}) error {:.3f}",
            gamemodeName(g_check.mode), g_check.x, g_check.y, player->getPositionX(), player->getPositionY(), error);
        if (g_stats.samples % 10 == 0) logCheckSummary();
    }

    void startSelfCheck(PlayLayer* play, PlayerObject* sim, long long tick) {
        if (!settings().selfCheck) return;
        if (g_check.pending || g_inputThisTick || !inputQueue().empty() || tick % kCheckTicks != 0) return;
        auto player = play->m_player1;
        if (!player || player->m_isDead || !sim) return;

        SimRequest request;
        request.source = player;
        request.horizon = kCheckTicks;
        request.stopOnNewOrb = false;
        auto outcome = Simulator::get().run(play, sim, request).outcome;
        if (outcome == SimOutcome::Unknown) return;

        g_check.pending = true;
        g_check.dueTick = tick + kCheckTicks;
        g_check.x = Simulator::get().finalX();
        g_check.y = Simulator::get().finalY();
        g_check.predictedDeath = outcome == SimOutcome::Died;
        g_check.mode = gamemodeOf(player);
    }
}

class $modify(PIBGameLayer, GJBaseGameLayer) {
    // Every input reaches this from processQueuedButtons, inside the tick it
    // takes effect on.
    void handleButton(bool down, int button, bool isPlayer1) {
        auto& queue = inputQueue();
        if (button == 1 && !Simulator::get().active()) {
            g_inputThisTick = true;
            g_check.pending = false;
            // Learn the player's rhythm from when they actually clicked.
            if (!InputQueue::isFiring()) noteInput(down, queue.currentTick());
        }
        if (!shouldIntercept(this, button)) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        // Inputs already queued reach the game first. An opposite input needs
        // at least one tick after the last one so the two don't cancel out on
        // the same tick.
        auto now = queue.currentTick();
        int earliest = 0;
        std::vector<SimInput> queued;
        for (auto const& pending : queue.pending()) {
            queued.push_back({ static_cast<int>(pending.fireTick - now), pending.down });
        }
        if (!queue.empty()) {
            auto const& last = queue.pending().back();
            earliest = std::max(0, static_cast<int>(last.fireTick - now) + (last.down != down ? 1 : 0));
        }

        auto players = affectedPlayers(this, isPlayer1);
        // One button drives both players in dual, so if either is in ship or
        // swing the input is left alone.
        bool relevant = !players.empty() && std::all_of(players.begin(), players.end(),
            [](PlayerObject* p) { return isBuffered(gamemodeOf(p)); });
        if (relevant && !down) {
            // Releases only matter in hold modes.
            relevant = settings().bufferReleases && std::any_of(players.begin(), players.end(),
                [](PlayerObject* p) { return isHoldMode(gamemodeOf(p)); });
        }

        int fireTick = earliest;
        if (!relevant && !queue.empty()) {
            // Not judged (e.g. a cube release): keep it the same distance
            // behind the queued press so the press length is preserved.
            fireTick = std::max(earliest, queue.chainDelay());
        }
        else if (relevant) {
            auto sim = static_cast<PIBPlayLayer*>(currentPlayLayer())->simPlayer();
            auto decision = chooseFireTick(this, sim, down, players, earliest, settings().maxDelay, queued);
            fireTick = decision.fireTick;

            if (fireTick > earliest) {
                auto mode = gamemodeName(gamemodeOf(players.front()));
                g_saves++;
                lastSave = fmt::format("saved +{}t {}", fireTick, down ? "press" : "release");
                if (settings().logSaves) {
                    log::info(
                        "*** SAVED +{} TICK{} ({:.2f} frames @ 60fps) {} in {}! (tick {} -> {}) [Limit: {}] [{} us] ***",
                        fireTick, fireTick == 1 ? "" : "S", fireTick / 4.0, down ? "PRESS" : "RELEASE", mode,
                        now, now + fireTick, settings().maxDelay, decision.micros
                    );
                }
                refreshStatus();
            }
            else {
                log::debug("{} checked in {} us: no delay", down ? "press" : "release", decision.micros);
            }
        }

        if (fireTick > 0) {
            queue.push(down, button, isPlayer1, fireTick);
            return;
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto play = currentPlayLayer();
        if (play && static_cast<GJBaseGameLayer*>(play) == this) {
            auto& queue = inputQueue();
            auto tick = queue.nextTick();
            if (!isHalfTick) {
                g_inputThisTick = false;
                finishSelfCheck(this, tick);
            }
            queue.beginTick(this, isHalfTick);
            if (!isHalfTick) {
                startSelfCheck(play, static_cast<PIBPlayLayer*>(play)->simPlayer(), tick);
            }
        }
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }

    void collisionCheckObjects(PlayerObject* player, std::vector<GameObject*>* objects, int count, float dt) {
        auto& sim = Simulator::get();
        if (!sim.isSimPlayer(player) || !objects) {
            return GJBaseGameLayer::collisionCheckObjects(player, objects, count, dt);
        }
        std::vector<GameObject*> filtered;
        sim.filterObjects(player, *objects, count, filtered);
        GJBaseGameLayer::collisionCheckObjects(player, &filtered, static_cast<int>(filtered.size()), dt);
    }

    void flipGravity(PlayerObject* player, bool flip, bool noEffects) {
        // The layer-level version also flashes the screen and plays effects.
        if (Simulator::get().isSimPlayer(player)) {
            player->flipGravity(flip, true);
            return;
        }
        GJBaseGameLayer::flipGravity(player, flip, noEffects);
    }
};

class $modify(PIBPlayerObject, PlayerObject) {
    void update(float dt) {
        auto layer = currentPlayLayer();
        if (layer && this == layer->m_player1 && !Simulator::get().active()) {
            Simulator::get().recordTickDelta(dt);
        }
        PlayerObject::update(dt);
    }

    void playSpiderDashEffect(CCPoint from, CCPoint to) {
        if (Simulator::get().isSimPlayer(this)) return;
        PlayerObject::playSpiderDashEffect(from, to);
    }

    void incrementJumps() {
        if (Simulator::get().isSimPlayer(this)) return;
        PlayerObject::incrementJumps();
    }
};

class $modify(PIBGameObject, GameObject) {
    void playShineEffect() {
        if (Simulator::get().active()) return;
        GameObject::playShineEffect();
    }
};

class $modify(PIBEffectGameObject, EffectGameObject) {
    void triggerObject(GJBaseGameLayer* layer, int uniqueID, std::vector<int> const* remapKeys) {
        if (Simulator::get().active()) return;
        EffectGameObject::triggerObject(layer, uniqueID, remapKeys);
    }
};

namespace {
    // GJBaseGameLayer::gameEventTriggered drives Event triggers in levels. Its
    // 2.2091 binding is a placeholder, so it is hooked by address (found by
    // decompiling handleButton, which raises JumpPush/JumpRelease through it).
    constexpr uintptr_t kGameEventTriggered = 0x33ac80;
    using GameEventFn = void (*)(GJBaseGameLayer*, int, int, int);

    void gameEventTriggeredHook(GJBaseGameLayer* self, int event, int material, int playerID) {
        if (Simulator::get().active()) return;
        reinterpret_cast<GameEventFn>(base::get() + kGameEventTriggered)(self, event, material, playerID);
    }
}

$on_mod(Loaded) {
    initSettings([] { refreshStatus(); });

    auto hooked = Mod::get()->hook(
        reinterpret_cast<void*>(base::get() + kGameEventTriggered),
        &gameEventTriggeredHook,
        "GJBaseGameLayer::gameEventTriggered"
    );
    if (!hooked) log::error("Failed to hook gameEventTriggered: {}", hooked.unwrapErr());

    listenForKeybindSettingPresses("cycle-mode", [](Keybind const&, bool down, bool repeat, double) {
        if (!down || repeat || !currentPlayLayer()) return false;
        cycleMode();
        log::info("Mode -> {} ({} tick limit)", currentModeName(), settings().maxDelay);
        refreshStatus();
        return true;
    });

    log::info("Predictive Input Buffer loaded. Mode: {}", currentModeName());
}
