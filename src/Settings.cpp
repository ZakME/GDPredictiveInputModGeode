#include "Settings.hpp"

#include <Geode/loader/Mod.hpp>
#include <Geode/loader/SettingV3.hpp>

using namespace geode::prelude;

namespace pib {
    namespace {
        Settings g_settings;

        void reload() {
            auto mod = Mod::get();
            auto mode = mod->getSettingValue<std::string>("mode");
            g_settings.maxDelay = kModeTicks[modeIndexForName(mode)];
            g_settings.bufferReleases = mod->getSettingValue<bool>("buffer-releases");
            g_settings.showStatus = mod->getSettingValue<bool>("show-status");
            g_settings.logSaves = mod->getSettingValue<bool>("log-saves");
            g_settings.jumpHorizon = static_cast<int>(mod->getSettingValue<int64_t>("jump-horizon"));
            g_settings.holdHorizon = static_cast<int>(mod->getSettingValue<int64_t>("hold-horizon"));
            g_settings.selfCheck = mod->getSettingValue<bool>("self-check");
            g_settings.ignoreTwoPlayer = mod->getSettingValue<bool>("ignore-two-player");
        }
    }

    Settings const& settings() {
        return g_settings;
    }

    int modeIndexForName(std::string_view name) {
        for (size_t i = 0; i < kModeNames.size(); ++i) {
            if (kModeNames[i] == name) return static_cast<int>(i);
        }
        return 0;
    }

    std::string_view currentModeName() {
        return kModeNames[modeIndexForName(Mod::get()->getSettingValue<std::string>("mode"))];
    }

    void cycleMode() {
        auto next = (modeIndexForName(currentModeName()) + 1) % kModeNames.size();
        // Fires the setting-changed listener, which calls reload().
        Mod::get()->setSettingValue<std::string>("mode", std::string(kModeNames[next]));
    }

    void initSettings(std::function<void()> onChanged) {
        reload();
        listenForAllSettingChanges([onChanged = std::move(onChanged)](std::string_view, std::shared_ptr<SettingV3>) {
            reload();
            if (onChanged) onChanged();
        });
    }
}
