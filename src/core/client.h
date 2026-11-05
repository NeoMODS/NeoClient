#pragma once

#include "core/config.h"
#include "game/game_access.h"
#include "modules/module_manager.h"
#include "ui/gui.h"

#include <windows.h>

#include <array>
#include <functional>
#include <string>

namespace neo {

// Ties the module manager, configuration and the ImGui menu together.
// Platform code only needs to provide a GameAccess, call frame() once per
// rendered frame and forward keyboard events.
class Client {
public:
    explicit Client(GameAccess& game);
    ~Client();

    void initialize();

    // Called after ImGui::NewFrame(). Updates modules and renders the menu/HUD.
    void frame(float dt);

    // Returns true when the key was consumed by NeoClient.
    bool keyDown(int virtualKey);
    bool keyUp(int virtualKey);

    void toggleMenu();
    void setMenuOpen(bool open);
    bool menuOpen() const { return gui_.open(); }
    bool capturingKey() const { return gui_.capturing(); }

    void setUnloadCallback(std::function<void()> callback);
    void requestUnload();

    // The game window, used to gate global keybind polling to the focused game.
    void setGameWindow(void* hwnd) { gameWindow_ = hwnd; }

    int menuKey() const { return gui_.state().menuKey > 0 ? gui_.state().menuKey : kDefaultMenuKey; }

    ui::Gui& gui() { return gui_; }
    const ui::Gui& gui() const { return gui_; }
    ModuleManager& modules() { return modules_; }
    GameAccess& game() { return game_; }
    const GameAccess& game() const { return game_; }

    static constexpr int kDefaultMenuKey = VK_RSHIFT;
    static constexpr int kEmergencyMenuKey = VK_INSERT;

private:
    void buildActions();
    void pollKeybinds();

    GameAccess& game_;
    ModuleManager modules_;
    Config config_;
    ui::Gui gui_;
    std::function<void()> unloadCallback_;
    bool unloadRequested_ = false;
    ui::UiActions actions_{};
    void* gameWindow_ = nullptr;
    std::array<bool, 256> keyHeld_{};
};

} // namespace neo
