#include "Predictor.hpp"

#include "Settings.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace geode::prelude;

namespace pib {
    Gamemode gamemodeOf(PlayerObject const* p) {
        if (p->m_isShip) return Gamemode::Ship;
        if (p->m_isBird) return Gamemode::Ufo;
        if (p->m_isBall) return Gamemode::Ball;
        if (p->m_isDart) return Gamemode::Wave;
        if (p->m_isRobot) return Gamemode::Robot;
        if (p->m_isSpider) return Gamemode::Spider;
        if (p->m_isSwing) return Gamemode::Swing;
        return Gamemode::Cube;
    }

    std::string_view gamemodeName(Gamemode mode) {
        switch (mode) {
            case Gamemode::Cube: return "Cube";
            case Gamemode::Ship: return "Ship";
            case Gamemode::Ball: return "Ball";
            case Gamemode::Ufo: return "UFO";
            case Gamemode::Wave: return "Wave";
            case Gamemode::Robot: return "Robot";
            case Gamemode::Spider: return "Spider";
            case Gamemode::Swing: return "Swing";
        }
        return "?";
    }

    bool isHoldMode(Gamemode mode) {
        return mode == Gamemode::Wave;
    }

    bool isBuffered(Gamemode mode) {
        return mode != Gamemode::Ship && mode != Gamemode::Swing;
    }

    namespace {
        // Gamemodes judged jump by jump: a prediction runs until the player
        // lands or reaches an orb.
        bool isJumpMode(Gamemode mode) {
            return mode == Gamemode::Cube || mode == Gamemode::Robot ||
                mode == Gamemode::Ball || mode == Gamemode::Spider;
        }

        // Exponential averages of the player's own timing, in ticks.
        struct Rhythm {
            double held = 12.0;
            double released = 12.0;
            double pressInterval = 24.0;
            long long lastPress = -1;
            long long lastRelease = -1;
        } g_rhythm;

        constexpr double kRhythmWeight = 0.3;
        // Longer gaps are pauses between sections, not rhythm.
        constexpr long long kMaxRhythmGap = 120;

        void blend(double& average, long long sample) {
            average += (static_cast<double>(std::max<long long>(sample, 1)) - average) * kRhythmWeight;
        }

        // Ticks from this input until the player's next relevant input, or
        // -1 for jump modes, where the next decision point is the landing.
        int expectedNextInput(Gamemode mode, bool down) {
            double ticks;
            switch (mode) {
                case Gamemode::Wave:
                    ticks = down ? g_rhythm.held : g_rhythm.released;
                    break;
                case Gamemode::Ufo:
                    ticks = g_rhythm.pressInterval;
                    break;
                default:
                    return -1;
            }
            return std::max(1, static_cast<int>(std::lround(ticks)));
        }

        SimRequest requestFor(PlayerObject* player, bool down, int tick, std::vector<SimInput> const& queued) {
            auto const& cfg = settings();
            auto mode = gamemodeOf(player);
            auto held = averageHoldTicks();

            SimRequest req;
            req.source = player;
            req.inputs = queued;
            req.inputs.push_back({ tick, down });
            req.evaluatedTick = tick;

            switch (mode) {
                case Gamemode::Wave: {
                    // The player flips back at their usual rhythm; a death
                    // caused by this input shows up before or just after that.
                    auto next = std::max(tick + 1, expectedNextInput(mode, down));
                    req.inputs.push_back({ next, !down });
                    req.horizon = std::max(tick + 2, std::min(next + 4, tick + cfg.holdHorizon));
                    break;
                }
                case Gamemode::Ufo: {
                    auto next = std::max(tick + held + 1, expectedNextInput(mode, down));
                    req.inputs.push_back({ tick + held, false });
                    req.inputs.push_back({ next, true });
                    req.inputs.push_back({ next + held, false });
                    req.horizon = std::max(tick + 2, std::min(next + 4, tick + cfg.holdHorizon));
                    break;
                }
                default:
                    req.inputs.push_back({ tick + held, false });
                    req.horizon = tick + cfg.jumpHorizon;
                    req.stopOnLanding = true;
                    req.actWindow = held + 2;
                    break;
            }
            std::stable_sort(req.inputs.begin(), req.inputs.end(),
                [](SimInput const& a, SimInput const& b) { return a.tick < b.tick; });
            return req;
        }
    }

    void noteInput(bool down, long long tick) {
        auto& r = g_rhythm;
        if (down) {
            if (r.lastRelease >= 0 && tick - r.lastRelease <= kMaxRhythmGap) blend(r.released, tick - r.lastRelease);
            if (r.lastPress >= 0 && tick - r.lastPress <= kMaxRhythmGap) blend(r.pressInterval, tick - r.lastPress);
            r.lastPress = tick;
        }
        else {
            if (r.lastPress > r.lastRelease && tick - r.lastPress <= kMaxRhythmGap) blend(r.held, tick - r.lastPress);
            r.lastRelease = tick;
        }
    }

    void resetRhythm() {
        g_rhythm.lastPress = -1;
        g_rhythm.lastRelease = -1;
    }

    int averageHoldTicks() {
        return std::max(1, static_cast<int>(std::lround(g_rhythm.held)));
    }

    Decision chooseFireTick(GJBaseGameLayer* layer, PlayerObject* sim, bool down,
        std::vector<PlayerObject*> const& players, int earliest, int maxDelay,
        std::vector<SimInput> const& queued)
    {
        Decision decision{ earliest, 0 };
        if (players.empty()) return decision;
        auto started = std::chrono::steady_clock::now();
        auto& simulator = Simulator::get();

        // Never hold an input past the point where the player's next one is
        // due: that would cancel the input instead of delaying it.
        int latest = std::max(earliest, maxDelay);
        for (auto player : players) {
            auto next = expectedNextInput(gamemodeOf(player), down);
            if (next > 0) latest = std::min(latest, std::max(earliest, next - 1));
        }

        bool baselineDies = false;
        std::vector<bool> baselineActed;
        for (auto player : players) {
            auto result = simulator.run(layer, sim, requestFor(player, down, earliest, queued));
            baselineDies = baselineDies || result.outcome == SimOutcome::Died;
            baselineActed.push_back(result.acted);
        }

        if (baselineDies) {
            for (int tick = earliest + 1; tick <= latest; ++tick) {
                bool safe = true;
                for (size_t i = 0; i < players.size() && safe; ++i) {
                    auto result = simulator.run(layer, sim, requestFor(players[i], down, tick, queued));
                    // A later timing must be proven safe, and a press that
                    // jumped on time must still jump when delayed (not be
                    // swallowed because the player walked off a ledge).
                    safe = result.outcome == SimOutcome::Survived &&
                        !(down && isJumpMode(gamemodeOf(players[i])) && baselineActed[i] && !result.acted);
                }
                if (safe) {
                    decision.fireTick = tick;
                    break;
                }
            }
        }

        decision.micros = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count();
        return decision;
    }
}
