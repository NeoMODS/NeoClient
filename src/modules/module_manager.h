#pragma once

#include "modules/module.h"

#include <memory>
#include <string>
#include <vector>

namespace neo {

class ModuleManager {
public:
    ModuleManager();

    void add(std::unique_ptr<Module> module);
    void applyDefaults(GameAccess& game);
    void tick(GameAccess& game, float dt);
    void handleKey(int virtualKey, GameAccess& game);
    std::string handleKeyBind(int virtualKey, GameAccess& game);

    bool setEnabled(const std::string& id, bool enabled, GameAccess& game);
    bool toggle(const std::string& id, GameAccess& game);

    Module* find(const std::string& id);
    const Module* find(const std::string& id) const;

    std::vector<std::unique_ptr<Module>>& modules() { return modules_; }
    const std::vector<std::unique_ptr<Module>>& modules() const { return modules_; }

    int enabledCount() const;
    std::vector<Module*> enabledModules();
    std::vector<Module*> modulesFor(Tab tab, Group group);
    std::vector<Group> groupsFor(Tab tab) const;

private:
    std::vector<std::unique_ptr<Module>> modules_;
    float tickAccumulator_ = 0.0f;
};

} // namespace neo
