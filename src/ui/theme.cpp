#include "ui/theme.h"

#include <algorithm>
#include <cmath>

namespace neo::ui {

Palette palette;
namespace {
float g_scale = 1.0f;
float g_textScale = 1.0f;
float g_globalAlpha = 1.0f;
}

void setScale(float scaleValue) {
    g_scale = std::max(0.75f, std::min(scaleValue, 2.5f));
}

float scale() {
    return g_scale;
}

float S(float value) {
    return value * g_scale;
}

void setTextScale(float scaleValue) {
    g_textScale = std::max(0.5f, std::min(scaleValue, 3.0f));
}

float textScale() {
    return g_textScale;
}

float textSize(float baseSize) {
    return baseSize * g_textScale;
}

void setGlobalAlpha(float alpha) {
    g_globalAlpha = std::clamp(alpha, 0.0f, 1.0f);
}

float globalAlpha() {
    return g_globalAlpha;
}

ImU32 fade(ImU32 color) {
    const float alpha = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
    return withAlpha(color, alpha * g_globalAlpha);
}

ImU32 withAlpha(ImU32 color, float alpha) {
    const int a = static_cast<int>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
    return (color & 0x00FFFFFF) | (static_cast<ImU32>(a) << IM_COL32_A_SHIFT);
}

ImU32 mixColor(ImU32 a, ImU32 b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const int ar = static_cast<int>((a >> IM_COL32_R_SHIFT) & 0xFF);
    const int ag = static_cast<int>((a >> IM_COL32_G_SHIFT) & 0xFF);
    const int ab = static_cast<int>((a >> IM_COL32_B_SHIFT) & 0xFF);
    const int aa = static_cast<int>((a >> IM_COL32_A_SHIFT) & 0xFF);
    const int br = static_cast<int>((b >> IM_COL32_R_SHIFT) & 0xFF);
    const int bg = static_cast<int>((b >> IM_COL32_G_SHIFT) & 0xFF);
    const int bb = static_cast<int>((b >> IM_COL32_B_SHIFT) & 0xFF);
    const int ba = static_cast<int>((b >> IM_COL32_A_SHIFT) & 0xFF);

    const int r = static_cast<int>(ar + (br - ar) * t);
    const int g = static_cast<int>(ag + (bg - ag) * t);
    const int bl = static_cast<int>(ab + (bb - ab) * t);
    const int al = static_cast<int>(aa + (ba - aa) * t);
    return IM_COL32(r, g, bl, al);
}

ImVec4 toVec4(ImU32 color) {
    return ImGui::ColorConvertU32ToFloat4(color);
}

ImU32 fromVec4(const ImVec4& color) {
    return ImGui::ColorConvertFloat4ToU32(color);
}

void applyStyle(float dpiScale) {
    setScale(dpiScale);

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(0.0f, 0.0f);
    style.FramePadding = ImVec2(S(10.0f), S(6.0f));
    style.CellPadding = ImVec2(S(6.0f), S(4.0f));
    style.ItemSpacing = ImVec2(S(8.0f), S(8.0f));
    style.ItemInnerSpacing = ImVec2(S(6.0f), S(6.0f));
    style.ScrollbarSize = S(5.0f);
    style.GrabMinSize = S(8.0f);
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;
    style.WindowRounding = S(18.0f);
    style.ChildRounding = S(12.0f);
    style.FrameRounding = S(9.0f);
    style.PopupRounding = S(10.0f);
    style.ScrollbarRounding = S(999.0f);
    style.GrabRounding = S(999.0f);
    style.TabRounding = S(8.0f);
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.AntiAliasedLines = true;
    style.AntiAliasedLinesUseTex = true;
    style.AntiAliasedFill = true;
    style.CurveTessellationTol = 1.25f;
    style.CircleTessellationMaxError = 0.20f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = toVec4(palette.text);
    colors[ImGuiCol_TextDisabled] = toVec4(palette.textDim);
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.07f, 0.09f, 0.97f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.08f, 0.11f, 0.98f);
    colors[ImGuiCol_Border] = toVec4(palette.border);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = toVec4(palette.panel);
    colors[ImGuiCol_FrameBgHovered] = toVec4(palette.panelHover);
    colors[ImGuiCol_FrameBgActive] = toVec4(palette.panelActive);
    colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(1.0f, 1.0f, 1.0f, 0.14f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.22f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.30f);
    colors[ImGuiCol_CheckMark] = toVec4(palette.accent);
    colors[ImGuiCol_SliderGrab] = toVec4(palette.accent);
    colors[ImGuiCol_SliderGrabActive] = toVec4(palette.accent2);
    colors[ImGuiCol_Button] = toVec4(palette.panel);
    colors[ImGuiCol_ButtonHovered] = toVec4(palette.panelHover);
    colors[ImGuiCol_ButtonActive] = toVec4(palette.panelActive);
    colors[ImGuiCol_Header] = toVec4(palette.panel);
    colors[ImGuiCol_HeaderHovered] = toVec4(palette.panelHover);
    colors[ImGuiCol_HeaderActive] = toVec4(palette.panelActive);
    colors[ImGuiCol_Separator] = toVec4(palette.borderSoft);
    colors[ImGuiCol_SeparatorHovered] = toVec4(palette.border);
    colors[ImGuiCol_SeparatorActive] = toVec4(palette.accent);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_Tab] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TabHovered] = toVec4(palette.panelHover);
    colors[ImGuiCol_TabSelected] = toVec4(palette.panel);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.72f, 0.78f, 1.0f, 0.22f);
    colors[ImGuiCol_NavHighlight] = toVec4(palette.accent);
}

void drawWindowBackdrop(ImDrawList* drawList, ImVec2 min, ImVec2 max, float rounding, float accentMix) {
    (void)accentMix;
    const ImVec2 size(max.x - min.x, max.y - min.y);
    if (size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }

    drawList->AddRectFilled(min, max, fade(IM_COL32(10, 12, 16, 230)), rounding);
    drawList->AddRect(min, max, fade(palette.border), rounding, 0, S(1.0f));
    drawList->AddRect(min + ImVec2(S(1.0f), S(1.0f)), max - ImVec2(S(1.0f), S(1.0f)),
        fade(withAlpha(IM_COL32(255, 255, 255, 255), 0.025f)), rounding, 0, S(1.0f));
}

} // namespace neo::ui
