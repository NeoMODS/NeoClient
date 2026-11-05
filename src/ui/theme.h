#pragma once

#include "imgui.h"

namespace neo::ui {

struct Palette {
    ImU32 windowTop = IM_COL32(31, 35, 44, 244);
    ImU32 windowBottom = IM_COL32(15, 18, 24, 248);
    ImU32 panel = IM_COL32(255, 255, 255, 13);
    ImU32 panelHover = IM_COL32(255, 255, 255, 21);
    ImU32 panelActive = IM_COL32(255, 255, 255, 27);
    ImU32 border = IM_COL32(255, 255, 255, 24);
    ImU32 borderSoft = IM_COL32(255, 255, 255, 14);
    ImU32 text = IM_COL32(246, 248, 253, 255);
    ImU32 textDim = IM_COL32(178, 186, 203, 255);
    ImU32 textFaint = IM_COL32(126, 134, 152, 255);
    ImU32 accent = IM_COL32(187, 201, 255, 255);
    ImU32 accent2 = IM_COL32(246, 185, 211, 255);
    ImU32 accent3 = IM_COL32(167, 230, 205, 255);
    ImU32 success = IM_COL32(132, 222, 174, 255);
    ImU32 danger = IM_COL32(244, 145, 152, 255);
    ImU32 track = IM_COL32(255, 255, 255, 26);
};

extern Palette palette;

void setScale(float scale);
float scale();
float S(float value);

void setTextScale(float scale);
float textScale();
float textSize(float baseSize);

void setGlobalAlpha(float alpha);
float globalAlpha();
ImU32 fade(ImU32 color);

ImU32 withAlpha(ImU32 color, float alpha);
ImU32 mixColor(ImU32 a, ImU32 b, float t);
ImVec4 toVec4(ImU32 color);
ImU32 fromVec4(const ImVec4& color);

void applyStyle(float dpiScale);
void drawWindowBackdrop(ImDrawList* drawList, ImVec2 min, ImVec2 max, float rounding, float accentMix = 0.0f);

} // namespace neo::ui
