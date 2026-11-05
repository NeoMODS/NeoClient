#include "modules/module.h"

#include <utility>

namespace neo {

const char* tabName(Tab tab) {
    switch (tab) {
    case Tab::Exploit: return "Exploit";
    case Tab::Misc: return "Misc";
    case Tab::Player: return "Player";
    case Tab::World: return "World";
    case Tab::Menu: return "Men\u00fa";
    }
    return "Misc";
}

const char* groupName(Group group) {
    switch (group) {
    case Group::Movement: return "Movement";
    case Group::Visuals: return "Visuals";
    case Group::Utility: return "Utility";
    case Group::World: return "World";
    }
    return "Movement";
}

ui::Icon tabIcon(Tab tab) {
    switch (tab) {
    case Tab::Exploit: return ui::Icon::Exploit;
    case Tab::Misc: return ui::Icon::Misc;
    case Tab::Player: return ui::Icon::Player;
    case Tab::World: return ui::Icon::World;
    case Tab::Menu: return ui::Icon::Menu;
    }
    return ui::Icon::Misc;
}

ui::Icon groupIcon(Group group) {
    switch (group) {
    case Group::Movement: return ui::Icon::Movement;
    case Group::Visuals: return ui::Icon::Visuals;
    case Group::Utility: return ui::Icon::Utility;
    case Group::World: return ui::Icon::World;
    }
    return ui::Icon::Movement;
}

Setting Setting::slider(std::string id, std::string label, float value, float min, float max, float step, std::string suffix, std::string format) {
    Setting setting;
    setting.id = std::move(id);
    setting.label = std::move(label);
    setting.type = Type::Slider;
    setting.value = value;
    setting.minValue = min;
    setting.maxValue = max;
    setting.step = step;
    setting.suffix = std::move(suffix);
    setting.format = std::move(format);
    return setting;
}

Setting Setting::toggle(std::string id, std::string label, bool value) {
    Setting setting;
    setting.id = std::move(id);
    setting.label = std::move(label);
    setting.type = Type::Toggle;
    setting.toggleValue = value;
    return setting;
}

Setting Setting::keybind(std::string id, std::string label, int key) {
    Setting setting;
    setting.id = std::move(id);
    setting.label = std::move(label);
    setting.type = Type::Keybind;
    setting.key = key;
    return setting;
}

Module::Module(std::string id, std::string name, std::string description, Tab tab, Group group, ui::Icon icon)
    : id_(std::move(id)),
      name_(std::move(name)),
      description_(std::move(description)),
      tab_(tab),
      group_(group),
      icon_(icon) {
}

void Module::setEnabled(bool enabled, GameAccess& game) {
    if (enabled_ == enabled) {
        return;
    }

    enabled_ = enabled;
    if (enabled_) {
        onEnable(game);
    } else {
        onDisable(game);
    }
}

void Module::toggle(GameAccess& game) {
    setEnabled(!enabled_, game);
}

Setting* Module::findSetting(std::string_view id) {
    for (auto& setting : settings_) {
        if (setting.id == id) {
            return &setting;
        }
    }
    return nullptr;
}

const Setting* Module::findSetting(std::string_view id) const {
    for (const auto& setting : settings_) {
        if (setting.id == id) {
            return &setting;
        }
    }
    return nullptr;
}

float Module::number(std::string_view id) const {
    if (const Setting* setting = findSetting(id)) {
        return setting->value;
    }
    return 0.0f;
}

bool Module::flag(std::string_view id) const {
    if (const Setting* setting = findSetting(id)) {
        return setting->toggleValue;
    }
    return false;
}

void Module::tick(GameAccess& game) {
    if (enabled_) {
        onTick(game);
    }
}

void Module::addSetting(Setting setting) {
    if (setting.type == Setting::Type::Keybind && setting.key > 0) {
        keybind_ = setting.key;
    }
    settings_.push_back(std::move(setting));
}

} // namespace neo
