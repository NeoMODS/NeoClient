#pragma once

#include "modules/module_manager.h"
#include "ui/settings.h"

namespace neo {

class Config {
public:
    // `menuKey` receives the saved menu key (0 when the config has none;
    // callers keep their default in that case).
    bool load(ModuleManager& modules, GameAccess& game, ui::UiSettings& ui, int& menuKey);
    bool save(const ModuleManager& modules, const ui::UiSettings& ui, int menuKey) const;
};

} // namespace neo
