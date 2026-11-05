#pragma once

#include "game/game_access.h"
#include "ui/icons.h"

#include <string>
#include <string_view>
#include <vector>

namespace neo {

enum class Tab {
    Exploit = 0,
    Misc = 1,
    Player = 2,
    World = 3,
    Menu = 4,
};

enum class Group {
    Movement = 0,
    Visuals = 1,
    Utility = 2,
    World = 3,
};

const char* tabName(Tab tab);
const char* groupName(Group group);
ui::Icon tabIcon(Tab tab);
ui::Icon groupIcon(Group group);

struct Setting {
    enum class Type {
        Slider,
        Toggle,
        Keybind,
    };

    std::string id;
    std::string label;
    Type type = Type::Slider;
    float value = 0.0f;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float step = 0.01f;
    bool toggleValue = false;
    int key = 0;
    std::string suffix;
    std::string format = "%.2f";

    static Setting slider(std::string id, std::string label, float value, float min, float max, float step = 0.01f, std::string suffix = "", std::string format = "%.2f");
    static Setting toggle(std::string id, std::string label, bool value);
    static Setting keybind(std::string id, std::string label, int key = 0);
};

class Module {
public:
    Module(std::string id, std::string name, std::string description, Tab tab, Group group, ui::Icon icon);
    virtual ~Module() = default;

    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& description() const { return description_; }
    Tab tab() const { return tab_; }
    Group group() const { return group_; }
    ui::Icon icon() const { return icon_; }

    bool enabled() const { return enabled_; }
    void setEnabled(bool enabled, GameAccess& game);
    void toggle(GameAccess& game);

    int keybind() const { return keybind_; }
    void setKeybind(int key) { keybind_ = key; }

    std::vector<Setting>& settings() { return settings_; }
    const std::vector<Setting>& settings() const { return settings_; }

    Setting* findSetting(std::string_view id);
    const Setting* findSetting(std::string_view id) const;
    float number(std::string_view id) const;
    bool flag(std::string_view id) const;

    void tick(GameAccess& game);

    bool defaultEnabled() const { return defaultEnabled_; }
    void setDefaultEnabled(bool value) { defaultEnabled_ = value; }

    // Presentation state (kept on the module so it survives filtering/tab changes).
    bool expanded = false;
    float expandAnim = 0.0f;
    float toggleAnim = 0.0f;
    float hoverAnim = 0.0f;

protected:
    virtual void onEnable(GameAccess& game) {}
    virtual void onDisable(GameAccess& game) {}
    virtual void onTick(GameAccess& game) {}

    void addSetting(Setting setting);

private:
    std::string id_;
    std::string name_;
    std::string description_;
    Tab tab_;
    Group group_;
    ui::Icon icon_;
    bool enabled_ = false;
    bool defaultEnabled_ = false;
    int keybind_ = 0;
    std::vector<Setting> settings_;
};

} // namespace neo
