#pragma once

#include "Simulator.hpp"

#include <string_view>
#include <vector>

class GJBaseGameLayer;
class PlayerObject;

namespace pib {
    enum class Gamemode { Cube, Ship, Ball, Ufo, Wave, Robot, Spider, Swing };

    Gamemode gamemodeOf(PlayerObject const* player);
    std::string_view gamemodeName(Gamemode mode);

    // Modes where the button is held to fly, so releases matter as much as
    // presses.
    bool isHoldMode(Gamemode mode);

    // Whether the mod touches inputs in this gamemode at all. Ship and swing
    // inputs always pass straight through.
    bool isBuffered(Gamemode mode);

    // Records when the player's own inputs arrive (not when the mod lets
    // them through), to learn their rhythm: how long they hold, how long
    // they let go, and how often they click.
    void noteInput(bool down, long long tick);
    void resetRhythm();

    // Average press length in ticks.
    int averageHoldTicks();

    struct Decision {
        // Tick (relative to now) to hand the input to the game.
        int fireTick = 0;
        // Microseconds spent simulating.
        long long micros = 0;
    };

    // Picks when an input should reach the game. `earliest` is the first
    // tick it may fire on (later than 0 when inputs are already queued);
    // `queued` are those inputs, relative to now. Moves the input later than
    // `earliest`, by at most `maxDelay` ticks from now, only if firing on
    // `earliest` kills a player and the later tick is confirmed safe.
    Decision chooseFireTick(
        GJBaseGameLayer* layer, PlayerObject* simPlayer, bool down,
        std::vector<PlayerObject*> const& players, int earliest, int maxDelay,
        std::vector<SimInput> const& queued
    );
}
