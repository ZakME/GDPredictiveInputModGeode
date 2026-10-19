#pragma once

#include <array>
#include <functional>
#include <string>
#include <string_view>

namespace pib {
    // Buffer modes, in the order the cycle key steps through them.
    inline constexpr std::array<std::string_view, 5> kModeNames = {
        "Disabled", "1 Tick", "2 Ticks", "4 Ticks", "8 Ticks"
    };
    inline constexpr std::array<int, 5> kModeTicks = { 0, 1, 2, 4, 8 };

    struct Settings {
        int maxDelay = 2;
        bool bufferReleases = true;
        bool showStatus = true;
        bool logSaves = true;
        int jumpHorizon = 120;
        int holdHorizon = 12;
        bool selfCheck = false;
    };

    // Current values, refreshed whenever a setting changes.
    Settings const& settings();

    int modeIndexForName(std::string_view name);
    std::string_view currentModeName();

    // Advances to the next mode and persists it as the "mode" setting.
    void cycleMode();

    // Loads the settings and keeps them current; `onChanged` runs after every
    // reload.
    void initSettings(std::function<void()> onChanged);
}
