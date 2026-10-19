#pragma once

#include <vector>

class GJBaseGameLayer;
class PlayerObject;
class GameObject;

namespace pib {
    enum class SimOutcome {
        // Reached a safe end: the horizon, a landing, or a new orb.
        Survived,
        Died,
        // Nothing conclusive: hit something the simulation can't model
        // (portals, teleports, speed changes), or ran out of ticks while
        // still airborne in a click mode.
        Unknown,
    };

    // A button event on a given tick (0 = the tick the game is running now).
    struct SimInput {
        int tick;
        bool down;
    };

    struct SimRequest {
        PlayerObject* source = nullptr;
        // Every simulated jump-button event, sorted by tick: inputs already
        // queued, the one being evaluated, and the player's modelled
        // follow-up inputs.
        std::vector<SimInput> inputs;
        // Tick of the input being evaluated; the stop rules below and the
        // "did it act" check only look from here on.
        int evaluatedTick = 0;
        // Ticks after evaluatedTick in which a press should visibly act
        // (jump, flip, orb). 0 disables the check.
        int actWindow = 0;
        // Total ticks to simulate.
        int horizon = 60;
        // End as Survived when the player has been airborne after the input
        // and touches down again; running out of ticks in the air is Unknown.
        bool stopOnLanding = false;
        // End as Survived when the player reaches an orb it wasn't touching
        // at the start: from there the outcome depends on the next click.
        bool stopOnNewOrb = true;
        // End as Survived this many ticks after the player teleports (spider
        // click, spider orb or pad), once the landing spot has proven safe.
        // -1 disables.
        int settleAfterTeleport = -1;
    };

    struct SimResult {
        SimOutcome outcome = SimOutcome::Unknown;
        // Whether the evaluated press made the player jump, flip, or use an
        // orb within actWindow.
        bool acted = false;
    };

    // Runs the game's own per-tick player logic on a hidden copy of a player
    // to see whether an input timing leads to a death.
    class Simulator {
    public:
        static Simulator& get();

        // Tick length (in GD's 60fps units) the game is currently using;
        // captured from the real players every tick.
        void recordTickDelta(float dt) { m_tickDelta = dt; }
        float tickDelta() const { return m_tickDelta; }

        // `simPlayer` is a hidden PlayerObject owned by the current level.
        SimResult run(GJBaseGameLayer* layer, PlayerObject* simPlayer, SimRequest const& request);

        // Where the simulated player ended up in the last run.
        float finalX() const { return m_finalX; }
        float finalY() const { return m_finalY; }

        // Hooks use these to tell simulated calls apart from real ones.
        bool active() const { return m_player != nullptr; }
        bool isSimPlayer(PlayerObject* player) const { return m_player && player == m_player; }
        void markDead() { m_dead = true; }
        void markUnknown() { m_unknown = true; }

        // Builds the object list a simulated collision pass may touch, and
        // flags the run as Unknown if the player overlaps something the
        // simulation can't model.
        void filterObjects(PlayerObject* player, std::vector<GameObject*> const& objects, int count,
            std::vector<GameObject*>& out);

    private:
        void cloneFrom(PlayerObject* source);
        void step();

        GJBaseGameLayer* m_layer = nullptr;
        PlayerObject* m_player = nullptr;
        float m_tickDelta = 0.25f;
        float m_finalX = 0.f;
        float m_finalY = 0.f;
        bool m_dead = false;
        bool m_unknown = false;
    };
}
