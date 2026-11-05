#include "core/client.h"

#include "core/keys.h"
#include "core/logger.h"
#include "modules/builtin_modules.h"

#include <utility>

namespace neo {

Client::Client(GameAccess& game)
    : game_(game) {
}

Client::~Client() = default;

void Client::initialize() {
    Logger::instance().initialize();
    NEO_LOG_INFO("NeoClient %s starting (Minecraft %s)", NEO_VERSION, NEO_MC_VERSION);

    registerBuiltinModules(modules_);
    gui_.state().menuKey = kDefaultMenuKey;

    // Only fall back to the built-in defaults on the very first launch.
    // Otherwise a saved "disabled" state would be overwritten on every start.
    int savedMenuKey = 0;
    if (!config_.load(modules_, game_, gui_.state().ui, savedMenuKey)) {
        modules_.applyDefaults(game_);
        config_.save(modules_, gui_.state().ui, gui_.state().menuKey);
    } else if (savedMenuKey > 0) {
        gui_.state().menuKey = savedMenuKey;
    }

    buildActions();
}

void Client::buildActions() {
    actions_.closeMenu = [this]() {
        gui_.setOpen(false);
    };
    actions_.unload = [this]() {
        requestUnload();
    };
    actions_.save = [this]() {
        config_.save(modules_, gui_.state().ui, gui_.state().menuKey);
    };
}

void Client::frame(float dt) {
    // Keep the game's mouse released and its movement keys cleared while the
    // menu is visible. This also covers focus changes that make Minecraft grab
    // the cursor again behind the overlay.
    const bool menuOpen = gui_.open();
    game_.setMenuCaptured(menuOpen);

    // Poll module keybinds every frame with edge detection. Doing it here (as
    // opposed to the window message) makes a single press toggle both on and
    // off reliably, independent of focus/message quirks.
    pollKeybinds();

    // Pause module logic while the menu is open so nothing (Auto Walk, Speed,
    // Air Jump, ...) can keep acting on the game behind the overlay.
    if (!menuOpen) {
        modules_.tick(game_, dt);
    }

    gui_.render(modules_, game_, dt, actions_);

    if (unloadRequested_ && gui_.open()) {
        gui_.setOpen(false);
        game_.setMenuCaptured(false);
    }
}

bool Client::keyDown(int virtualKey) {
    if (virtualKey <= 0) {
        return false;
    }

    if (gui_.capturing()) {
        gui_.handleKeyDown(virtualKey);
        return true;
    }

    if (virtualKey == gui_.state().menuKey || virtualKey == kEmergencyMenuKey) {
        toggleMenu();
        return true;
    }

    if (gui_.open()) {
        if (virtualKey == VK_ESCAPE) {
            gui_.setOpen(false);
        }
        // The menu owns the keyboard while it is visible.
        return true;
    }

    // Module keybinds are handled by pollKeybinds() with proper edge
    // detection, so a single press reliably toggles both on and off.
    return false;
}

void Client::pollKeybinds() {
    const bool focused = gameWindow_ == nullptr ||
                         GetForegroundWindow() == static_cast<HWND>(gameWindow_);
    const bool canToggle = !gui_.open() && !gui_.capturing() && focused;

    std::array<bool, 256> bound{};
    for (auto& module : modules_.modules()) {
        const int vk = module->keybind();
        if (vk > 0 && vk < 256) {
            bound[vk] = true;
        }
    }

    for (int vk = 1; vk < 256; ++vk) {
        const bool down = canToggle && keys::down(vk);
        const bool pressed = down && !keyHeld_[vk];
        keyHeld_[vk] = down;
        if (!pressed || !bound[vk]) {
            continue;
        }

        bool changed = false;
        for (auto& module : modules_.modules()) {
            if (module->keybind() != vk) {
                continue;
            }
            module->toggle(game_);
            gui_.toast(module->name(), module->enabled());
            changed = true;
        }
        if (changed) {
            gui_.state().dirty = true;
        }
    }
}

bool Client::keyUp(int virtualKey) {
    (void)virtualKey;
    return gui_.open();
}

void Client::toggleMenu() {
    gui_.toggle();
    gui_.state().dirty = true;
    game_.setMenuCaptured(gui_.open());
    NEO_LOG_DEBUG("Menu %s", gui_.open() ? "opened" : "closed");
}

void Client::setMenuOpen(bool open) {
    gui_.setOpen(open);
    game_.setMenuCaptured(open);
}

void Client::setUnloadCallback(std::function<void()> callback) {
    unloadCallback_ = std::move(callback);
}

void Client::requestUnload() {
    if (unloadRequested_) {
        return;
    }

    unloadRequested_ = true;
    game_.setMenuCaptured(false);
    config_.save(modules_, gui_.state().ui, gui_.state().menuKey);
    NEO_LOG_INFO("Unload requested");
    if (unloadCallback_) {
        unloadCallback_();
    }
}

} // namespace neo
