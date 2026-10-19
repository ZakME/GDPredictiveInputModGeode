#pragma once

#include <deque>

class GJBaseGameLayer;

namespace pib {
    struct PendingInput {
        bool down;
        int button;
        bool isPlayer1;
        // Physics tick on which the input is handed to the game.
        long long fireTick;
        // How many ticks this input was pushed back by.
        int delay;
    };

    // Holds inputs the predictor decided to delay, and hands them back to the
    // game at the start of the right physics tick. Inputs always leave in the
    // order they arrived.
    class InputQueue {
    public:
        // Tick the game is currently processing.
        long long currentTick() const { return m_currentTick; }
        // Tick the next beginTick will start.
        long long nextTick() const { return m_halfTicks / 2; }

        bool empty() const { return m_inputs.empty(); }
        std::deque<PendingInput> const& pending() const { return m_inputs; }

        // Delay of the most recently queued input. Inputs arriving while
        // others are still queued reuse it, which keeps both their order and
        // the gaps between them (so a short tap stays a short tap).
        int chainDelay() const { return m_inputs.empty() ? 0 : m_inputs.back().delay; }

        void push(bool down, int button, bool isPlayer1, int delay);

        // Called at the start of every processCommands, before the game reads
        // its own input queue. GD splits a tick in two halves when an input
        // lands mid-tick; both halves belong to the same tick.
        void beginTick(GJBaseGameLayer* layer, bool isHalfTick);

        // Drops queued presses and immediately delivers queued releases, so the
        // game never thinks a button is still held after a reset.
        void flush(GJBaseGameLayer* layer);

        void reset();

        // True while the queue itself is calling handleButton, so the hook
        // passes the call straight through.
        static bool isFiring();

    private:
        void fire(GJBaseGameLayer* layer, PendingInput const& input);

        std::deque<PendingInput> m_inputs;
        long long m_halfTicks = 0;
        long long m_currentTick = 0;
    };

    InputQueue& inputQueue();
}
