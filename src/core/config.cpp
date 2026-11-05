#include "core/config.h"

#include "core/logger.h"
#include "core/paths.h"
#include "json/json.hpp"

#include <filesystem>
#include <fstream>

namespace neo {
namespace {

using json = nlohmann::json;

} // namespace

bool Config::load(ModuleManager& modules, GameAccess& game, ui::UiSettings& ui, int& menuKey) {
    menuKey = 0;
    const auto path = paths::configFile();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return false;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        NEO_LOG_WARN("Could not open config: %s", path.string().c_str());
        return false;
    }

    json root;
    try {
        file >> root;
    } catch (const std::exception& e) {
        NEO_LOG_ERROR("Config parse failed: %s", e.what());
        return false;
    }

    if (!root.is_object()) {
        return false;
    }

    if (root.contains("menuKey") && root["menuKey"].is_number_integer()) {
        menuKey = root["menuKey"].get<int>();
    }

    if (root.contains("ui") && root["ui"].is_object()) {
        const auto& entry = root["ui"];
        const auto readNumber = [&entry](const char* key, float& target) {
            if (entry.contains(key) && entry[key].is_number()) {
                target = entry[key].get<float>();
            }
        };
        readNumber("fontScale", ui.fontScale);
        readNumber("windowWidth", ui.windowWidth);
        readNumber("windowHeight", ui.windowHeight);
        readNumber("opacity", ui.opacity);
        readNumber("cornerRadius", ui.cornerRadius);
        if (entry.contains("animations") && entry["animations"].is_boolean()) {
            ui.animations = entry["animations"].get<bool>();
        }
        ui::clampUiSettings(ui);
    }

    if (root.contains("modules") && root["modules"].is_object()) {
        for (auto& module : modules.modules()) {
            const std::string& id = module->id();
            if (!root["modules"].contains(id) || !root["modules"][id].is_object()) {
                continue;
            }

            const auto& entry = root["modules"][id];
            if (entry.contains("keybind") && entry["keybind"].is_number_integer()) {
                module->setKeybind(entry["keybind"].get<int>());
            }

            if (entry.contains("settings") && entry["settings"].is_object()) {
                for (auto& setting : module->settings()) {
                    const auto& values = entry["settings"];
                    if (!values.contains(setting.id)) {
                        continue;
                    }
                    try {
                        switch (setting.type) {
                        case Setting::Type::Slider:
                            if (values[setting.id].is_number()) {
                                setting.value = clamp(
                                    values[setting.id].get<float>(),
                                    setting.minValue,
                                    setting.maxValue);
                            }
                            break;
                        case Setting::Type::Toggle:
                            if (values[setting.id].is_boolean()) {
                                setting.toggleValue = values[setting.id].get<bool>();
                            }
                            break;
                        case Setting::Type::Keybind:
                            if (values[setting.id].is_number_integer()) {
                                setting.key = values[setting.id].get<int>();
                            }
                            break;
                        }
                    } catch (...) {
                        NEO_LOG_WARN("Invalid value for %s.%s", id.c_str(), setting.id.c_str());
                    }
                }
            }

            if (entry.contains("enabled") && entry["enabled"].is_boolean()) {
                module->setEnabled(entry["enabled"].get<bool>(), game);
            }
        }
    }

    NEO_LOG_INFO("Loaded config from %s", path.string().c_str());
    return true;
}

bool Config::save(const ModuleManager& modules, const ui::UiSettings& ui, int menuKey) const {
    const auto path = paths::configFile();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    json root;
    root["version"] = 1;
    root["minecraft"] = NEO_MC_VERSION;
    root["menuKey"] = menuKey;
    root["ui"] = {
        {"fontScale", ui.fontScale},
        {"windowWidth", ui.windowWidth},
        {"windowHeight", ui.windowHeight},
        {"opacity", ui.opacity},
        {"cornerRadius", ui.cornerRadius},
        {"animations", ui.animations},
    };
    root["modules"] = json::object();

    for (const auto& module : modules.modules()) {
        json entry;
        entry["enabled"] = module->enabled();
        entry["keybind"] = module->keybind();
        entry["settings"] = json::object();

        for (const auto& setting : module->settings()) {
            switch (setting.type) {
            case Setting::Type::Slider:
                entry["settings"][setting.id] = setting.value;
                break;
            case Setting::Type::Toggle:
                entry["settings"][setting.id] = setting.toggleValue;
                break;
            case Setting::Type::Keybind:
                entry["settings"][setting.id] = setting.key;
                break;
            }
        }

        root["modules"][module->id()] = std::move(entry);
    }

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        NEO_LOG_WARN("Could not write config: %s", path.string().c_str());
        return false;
    }

    file << root.dump(2);
    return true;
}

} // namespace neo
