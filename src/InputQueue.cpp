#include "InputQueue.hpp"

#include <Geode/binding/GJBaseGameLayer.hpp>

namespace pib {
    namespace {
        bool g_firing = false;
    }

    InputQueue& inputQueue() {
        static InputQueue queue;
        return queue;
    }

    bool InputQueue::isFiring() {
        return g_firing;
    }

    void InputQueue::push(bool down, int button, bool isPlayer1, int delay) {
        m_inputs.push_back({ down, button, isPlayer1, m_currentTick + delay, delay });
    }

    void InputQueue::fire(GJBaseGameLayer* layer, PendingInput const& input) {
        g_firing = true;
        layer->handleButton(input.down, input.button, input.isPlayer1);
        g_firing = false;
    }

    void InputQueue::beginTick(GJBaseGameLayer* layer, bool isHalfTick) {
        m_currentTick = m_halfTicks / 2;
        m_halfTicks += isHalfTick ? 1 : 2;
        while (!m_inputs.empty() && m_inputs.front().fireTick <= m_currentTick) {
            auto input = m_inputs.front();
            m_inputs.pop_front();
            this->fire(layer, input);
        }
    }

    void InputQueue::flush(GJBaseGameLayer* layer) {
        auto inputs = std::move(m_inputs);
        m_inputs.clear();
        for (auto const& input : inputs) {
            if (!input.down) this->fire(layer, input);
        }
    }

    void InputQueue::reset() {
        m_inputs.clear();
        m_halfTicks = 0;
        m_currentTick = 0;
    }
}
