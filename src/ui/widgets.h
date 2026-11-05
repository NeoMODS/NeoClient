#pragma once

#include "imgui.h"
#include "ui/icons.h"
#include "ui/theme.h"

namespace neo::ui {

struct Rect {
    ImVec2 min;
    ImVec2 max;

    ImVec2 size() const { return ImVec2(max.x - min.x, max.y - min.y); }
    ImVec2 center() const { return ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f); }
    bool contains(const ImVec2& point) const {
        return point.x >= min.x && point.y >= min.y && point.x <= max.x && point.y <= max.y;
    }
};

Rect makeRect(ImVec2 min, ImVec2 size);

void text(ImDrawList* drawList, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* value);
void textShadow(ImDrawList* drawList, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* value);
void gradientText(ImDrawList* drawList, ImFont* font, float size, ImVec2 pos, ImU32 from, ImU32 to, const char* value);
void letterSpacedText(ImDrawList* drawList, ImFont* font, float size, ImVec2 pos, float spacing, ImU32 color, const char* value);
float measure(ImFont* font, float size, const char* value);

void drawChip(ImDrawList* drawList, ImVec2 pos, const char* value, ImU32 textColor, ImU32 backgroundColor, ImFont* font, float fontSize, float height);
void drawToggle(ImDrawList* drawList, const Rect& rect, float animation, bool hovered);
void drawSlider(ImDrawList* drawList, const Rect& rect, float t, bool hovered, bool active);
bool slider(const char* id, const Rect& rect, float& value, float minValue, float maxValue, float step, bool enabled);
void drawKeycap(ImDrawList* drawList, const Rect& rect, const char* value, bool hovered, bool capturing, ImFont* font, float fontSize);
void drawTooltip(const char* text);
void drawSparkline(ImDrawList* drawList, const Rect& rect, const float* values, int count, ImU32 color, ImU32 fillColor);
void drawCheckbox(ImDrawList* drawList, const Rect& rect, bool value, float animation);

bool mouseHovered(const Rect& rect);
bool mouseClicked(const Rect& rect);
bool mouseDown();

ImU32 accentGradient(float t);
ImU32 moduleTint(int index);

} // namespace neo::ui
