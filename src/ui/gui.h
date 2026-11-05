#pragma once

#include "game/game_access.h"
#include "modules/module_manager.h"
#include "ui/settings.h"
#include "ui/widgets.h"

#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace neo::ui {

struct Toast {
    std::string text;
    bool enabled = true;
    float life = 2.2f;
    float anim = 0.0f;
};

struct UiState {
    bool menuOpen = false;
    Tab tab = Tab::Misc;
    Group group = Group::Movement;
    char search[128]{};
    int menuKey = 0; // virtual key; owned by Client (config-backed)
    float menuAnim = 0.0f;
    float tabIndicatorX = 0.0f;
    float tabIndicatorW = 0.0f;
    bool tabIndicatorInit = false;
    std::string selectedModule;
    std::string captureId;
    std::string pendingBindId;
    int pendingBindKey = 0;
    bool pendingBind = false;
    ImVec2 windowPos{};
    bool windowPosInit = false;
    bool dragging = false;
    bool dirty = false;
    ImVec2 dragOffset{};
    bool scrollSelected = false;
    UiSettings ui{};
    std::deque<Toast> toasts;
    std::vector<float> fpsHistory;
};

struct UiActions {
    std::function<void()> closeMenu;
    std::function<void()> unload;
    std::function<void()> save;
};

class Gui {
public:
    Gui();

    void render(ModuleManager& modules, GameAccess& game, float dt, const UiActions& actions);

    void setOpen(bool open) { state_.menuOpen = open; }
    bool open() const { return state_.menuOpen; }
    void toggle() { state_.menuOpen = !state_.menuOpen; }

    bool handleKeyDown(int virtualKey);
    bool capturing() const { return !state_.captureId.empty(); }

    void toast(const std::string& text, bool enabled);
    UiState& state() { return state_; }
    const UiState& state() const { return state_; }

private:
    void updateAnimations(float dt);
    void drawMenu(ModuleManager& modules, GameAccess& game, float dt, const UiActions& actions);
    void drawHud(ModuleManager& modules, GameAccess& game, float dt);
    void drawWorldMarkers(ModuleManager& modules, GameAccess& game);

    void drawHeader(ModuleManager& modules, GameAccess& game, const UiActions& actions);
    void drawSidebar(ModuleManager& modules, GameAccess& game);
    void drawModules(ModuleManager& modules, GameAccess& game, const UiActions& actions);
    void drawRightPanel(ModuleManager& modules, GameAccess& game, const UiActions& actions);
    void drawMenuSettings(const Rect& content);
    void drawToasts(float dt);

    void drawModuleRow(Module& module, float width, GameAccess& game, const UiActions& actions);
    float moduleRowHeight(const Module& module) const;
    float moduleSettingsHeight(const Module& module) const;
    void selectModule(const std::string& id);
    void drawModuleSettings(Module& module, float width, GameAccess& game, const UiActions& actions);
    bool mouseHoveredKeybind(Module& module, const Rect& row) const;
    void applyCapturedKey(int virtualKey);
    void resolvePendingBind(ModuleManager& modules);
    void handleModuleToggle(Module& module, GameAccess& game, const UiActions& actions);

    std::string activeGroupName(ModuleManager& modules) const;
    std::vector<Module*> visibleModules(ModuleManager& modules) const;

    UiState state_;
    float fpsSmooth_ = 0.0f;
    float fpsTimer_ = 0.0f;
    float toastTimer_ = 0.0f;
};

} // namespace neo::ui
