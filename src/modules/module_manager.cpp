#include "modules/module_manager.h"

#include <algorithm>

namespace neo {

ModuleManager::ModuleManager() = default;

void ModuleManager::add(std::unique_ptr<Module> module) {
    if (module) {
        modules_.push_back(std::move(module));
    }
}

void ModuleManager::applyDefaults(GameAccess& game) {
    for (auto& module : modules_) {
        if (module->defaultEnabled()) {
            module->setEnabled(true, game);
        }
    }
}

void ModuleManager::tick(GameAccess& game, float dt) {
    if (!game.snapshot().available || !game.snapshot().inGame) {
        tickAccumulator_ = 0.0f;
        return;
    }

    tickAccumulator_ += dt;
    constexpr float kTick = 1.0f / 20.0f;
    int guard = 0;
    while (tickAccumulator_ >= kTick && guard++ < 4) {
        tickAccumulator_ -= kTick;
        for (auto& module : modules_) {
            if (module->enabled()) {
                module->tick(game);
            }
        }
    }
}

void ModuleManager::handleKey(int virtualKey, GameAccess& game) {
    if (virtualKey <= 0) {
        return;
    }

    for (auto& module : modules_) {
        if (module->keybind() == virtualKey) {
            module->toggle(game);
        }
    }
}

std::string ModuleManager::handleKeyBind(int virtualKey, GameAccess& game) {
    if (virtualKey <= 0) {
        return {};
    }

    for (auto& module : modules_) {
        if (module->keybind() == virtualKey) {
            module->toggle(game);
            return module->id();
        }
    }
    return {};
}

bool ModuleManager::setEnabled(const std::string& id, bool enabled, GameAccess& game) {
    if (Module* module = find(id)) {
        module->setEnabled(enabled, game);
        return true;
    }
    return false;
}

bool ModuleManager::toggle(const std::string& id, GameAccess& game) {
    if (Module* module = find(id)) {
        module->toggle(game);
        return true;
    }
    return false;
}

Module* ModuleManager::find(const std::string& id) {
    for (auto& module : modules_) {
        if (module->id() == id) {
            return module.get();
        }
    }
    return nullptr;
}

const Module* ModuleManager::find(const std::string& id) const {
    for (const auto& module : modules_) {
        if (module->id() == id) {
            return module.get();
        }
    }
    return nullptr;
}

int ModuleManager::enabledCount() const {
    int count = 0;
    for (const auto& module : modules_) {
        if (module->enabled()) {
            ++count;
        }
    }
    return count;
}

std::vector<Module*> ModuleManager::enabledModules() {
    std::vector<Module*> result;
    for (auto& module : modules_) {
        if (module->enabled()) {
            result.push_back(module.get());
        }
    }
    std::sort(result.begin(), result.end(), [](const Module* a, const Module* b) {
        return a->name() < b->name();
    });
    return result;
}

std::vector<Module*> ModuleManager::modulesFor(Tab tab, Group group) {
    std::vector<Module*> result;
    for (auto& module : modules_) {
        if (module->tab() == tab && module->group() == group) {
            result.push_back(module.get());
        }
    }
    return result;
}

std::vector<Group> ModuleManager::groupsFor(Tab tab) const {
    std::vector<Group> result;
    for (const auto& module : modules_) {
        if (module->tab() != tab) {
            continue;
        }
        if (std::find(result.begin(), result.end(), module->group()) == result.end()) {
            result.push_back(module->group());
        }
    }
    std::sort(result.begin(), result.end(), [](Group a, Group b) {
        return static_cast<int>(a) < static_cast<int>(b);
    });
    return result;
}

} // namespace neo
