#pragma once

#include "imgui.h"

namespace neo::ui {

struct Fonts {
    ImFont* regular = nullptr;
    ImFont* medium = nullptr;
    ImFont* semibold = nullptr;
    ImFont* small = nullptr;
    ImFont* tiny = nullptr;
    ImFont* title = nullptr;
    ImFont* hero = nullptr;
    ImFont* hud = nullptr;
    ImFont* hudTitle = nullptr;
    float scale = 1.0f;
};

Fonts& fonts();
void loadFonts(float dpiScale);

} // namespace neo::ui
