#include "ui/widgets.h"

#include "core/math.h"
#include "ui/fonts.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace neo::ui {
namespace {

float charAdvance(ImFont* font, float size, const char* begin, const char* end) {
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, begin, end).x;
}

void renderGlyphRange(ImDrawList* drawList, ImFont* font, float size, ImVec2 pos, ImU32 from, ImU32 to, const char* text) {
    const char* begin = text;
    const char* end = text + std::strlen(text);
    if (begin == end) {
        return;
    }

    const float total = static_cast<float>(std::max<size_t>(1, end - begin - 1));
    float x = 0.0f;
    int index = 0;
    while (begin < end) {
        const char* glyphEnd = begin + 1;
        while (glyphEnd < end && (static_cast<unsigned char>(*glyphEnd) & 0xC0) == 0x80) {
            ++glyphEnd;
        }
        const float t = static_cast<float>(index) / total;
        drawList->AddText(font, size, ImVec2(pos.x + x, pos.y), mixColor(from, to, t), begin, glyphEnd);
        x += charAdvance(font, size, begin, glyphEnd);
        begin = glyphEnd;
        ++index;
    }
}

void roundedGradientRect(ImDrawList* drawList, ImVec2 min, ImVec2 max, float rounding, ImU32 left, ImU32 right) {
    if (max.x <= min.x || max.y <= min.y) {
        return;
    }

    const float radius = std::clamp(rounding, 0.0f, std::min(max.x - min.x, max.y - min.y) * 0.5f);
    if (radius <= 0.01f) {
        drawList->AddRectFilledMultiColor(min, max, left, right, right, left);
        return;
    }

    constexpr int kSegments = 7;
    ImVector<ImVec2> points;
    points.reserve(4 * (kSegments + 1));

    const auto addCorner = [&points, radius](float cx, float cy, float from, float to) {
        for (int i = 0; i <= kSegments; ++i) {
            const float angle = from + (to - from) * static_cast<float>(i) / static_cast<float>(kSegments);
            points.push_back(ImVec2(cx + std::cos(angle) * radius, cy + std::sin(angle) * radius));
        }
    };

    constexpr float kPi = 3.14159265358979323846f;
    addCorner(min.x + radius, min.y + radius, kPi, kPi * 1.5f);
    addCorner(max.x - radius, min.y + radius, kPi * 1.5f, kPi * 2.0f);
    addCorner(max.x - radius, max.y - radius, 0.0f, kPi * 0.5f);
    addCorner(min.x + radius, max.y - radius, kPi * 0.5f, kPi);

    const int count = points.Size;
    const ImDrawIdx base = static_cast<ImDrawIdx>(drawList->_VtxCurrentIdx);
    const ImVec2 uv = ImGui::GetIO().Fonts->TexUvWhitePixel;
    drawList->PrimReserve(count * 3, count + 1);

    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    drawList->PrimWriteVtx(center, uv, mixColor(left, right, 0.5f));
    for (const ImVec2& point : points) {
        const float t = std::clamp((point.x - min.x) / (max.x - min.x), 0.0f, 1.0f);
        drawList->PrimWriteVtx(point, uv, mixColor(left, right, t));
    }
    for (int i = 0; i < count; ++i) {
        drawList->PrimWriteIdx(base);
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1 + i));
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1 + (i + 1) % count));
    }
}

} // namespace

Rect makeRect(ImVec2 min, ImVec2 size) {
    return Rect{min, ImVec2(min.x + size.x, min.y + size.y)};
}

void text(ImDrawList* drawList, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* value) {
    if (drawList && font && value) {
        drawList->AddText(font, size, pos, color, value);
    }
}

void textShadow(ImDrawList* drawList, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* value) {
    if (!drawList || !font || !value) {
        return;
    }
    drawList->AddText(font, size, ImVec2(pos.x, pos.y + S(1.0f)), withAlpha(IM_COL32(0, 0, 0, 255), 0.45f), value);
    drawList->AddText(font, size, pos, color, value);
}

void gradientText(ImDrawList* drawList, ImFont* font, float size, ImVec2 pos, ImU32 from, ImU32 to, const char* value) {
    if (!drawList || !font || !value) {
        return;
    }
    renderGlyphRange(drawList, font, size, pos, from, to, value);
}

void letterSpacedText(ImDrawList* drawList, ImFont* font, float size, ImVec2 pos, float spacing, ImU32 color, const char* value) {
    if (!drawList || !font || !value) {
        return;
    }

    float x = 0.0f;
    const char* cursor = value;
    while (*cursor) {
        const char* end = cursor + 1;
        while (*end && (static_cast<unsigned char>(*end) & 0xC0) == 0x80) {
            ++end;
        }
        drawList->AddText(font, size, ImVec2(pos.x + x, pos.y), color, cursor, end);
        x += charAdvance(font, size, cursor, end) + spacing;
        cursor = end;
    }
}

float measure(ImFont* font, float size, const char* value) {
    if (!font || !value) {
        return 0.0f;
    }
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, value).x;
}

void drawChip(ImDrawList* drawList, ImVec2 pos, const char* value, ImU32 textColor, ImU32 backgroundColor, ImFont* font, float fontSize, float height) {
    if (!drawList || !font || !value) {
        return;
    }

    const float width = measure(font, fontSize, value) + S(14.0f);
    const Rect rect{pos, ImVec2(pos.x + width, pos.y + height)};
    drawList->AddRectFilled(rect.min, rect.max, backgroundColor, height * 0.5f);
    drawList->AddRect(rect.min, rect.max, withAlpha(IM_COL32(255, 255, 255, 255), 0.05f), height * 0.5f);
    const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, value);
    drawList->AddText(font, fontSize,
        ImVec2(rect.min.x + (width - textSize.x) * 0.5f, rect.min.y + (height - textSize.y) * 0.5f),
        textColor, value);
}

void drawToggle(ImDrawList* drawList, const Rect& rect, float animation, bool hovered) {
    const float height = rect.max.y - rect.min.y;
    const float rounding = height * 0.5f;
    const float t = clamp(animation, 0.0f, 1.0f);

    const ImU32 off = fade(mixColor(IM_COL32(255, 255, 255, 22), IM_COL32(255, 255, 255, 34), hovered ? 1.0f : 0.0f));
    drawList->AddRectFilled(rect.min, rect.max, off, rounding);

    if (t > 0.001f) {
        const ImU32 left = fade(withAlpha(mixColor(palette.accent, palette.accent2, 0.15f), t));
        const ImU32 right = fade(withAlpha(mixColor(palette.accent, palette.accent2, 0.85f), t));
        roundedGradientRect(drawList, rect.min, rect.max, rounding, left, right);
        drawList->AddRect(rect.min, rect.max, fade(withAlpha(mixColor(palette.accent, palette.accent2, 0.5f), 0.45f * t)), rounding);
    }

    const float knobRadius = height * 0.5f - S(3.0f);
    const float travel = (rect.max.x - rect.min.x) - height;
    const ImVec2 knobCenter(rect.min.x + height * 0.5f + travel * t, rect.center().y);
    drawList->AddCircleFilled(knobCenter, knobRadius + S(1.0f), fade(withAlpha(IM_COL32(0, 0, 0, 255), 0.18f * t)), 18);
    drawList->AddCircleFilled(knobCenter, knobRadius, fade(IM_COL32(250, 250, 253, 255)), 18);
}

void drawCheckbox(ImDrawList* drawList, const Rect& rect, bool value, float animation) {
    const float rounding = S(5.0f);
    drawList->AddRectFilled(rect.min, rect.max, fade(value ? withAlpha(palette.accent, 0.85f) : IM_COL32(255, 255, 255, 14)), rounding);
    drawList->AddRect(rect.min, rect.max, fade(value ? withAlpha(palette.accent, 0.55f) : palette.border), rounding);
    if (value) {
        const ImU32 ink = IM_COL32(16, 18, 24, 255);
        const ImVec2 center = rect.center();
        drawList->AddLine(ImVec2(center.x - S(4.0f), center.y), ImVec2(center.x - S(1.0f), center.y + S(3.0f)), ink, S(1.6f));
        drawList->AddLine(ImVec2(center.x - S(1.0f), center.y + S(3.0f)), ImVec2(center.x + S(5.0f), center.y - S(3.0f)), ink, S(1.6f));
    }
}

void drawSlider(ImDrawList* drawList, const Rect& rect, float t, bool hovered, bool active) {
    const float trackHeight = S(4.0f);
    const float y = rect.center().y - trackHeight * 0.5f;
    const ImVec2 min(rect.min.x, y);
    const ImVec2 max(rect.max.x, y + trackHeight);

    drawList->AddRectFilled(min, max, fade(withAlpha(IM_COL32(255, 255, 255, 255), 0.10f)), trackHeight * 0.5f);

    t = clamp(t, 0.0f, 1.0f);
    if (t > 0.001f) {
        const ImVec2 fillMax(min.x + (max.x - min.x) * t, max.y);
        roundedGradientRect(drawList, min, fillMax, trackHeight * 0.5f, fade(palette.accent), fade(palette.accent2));
    }

    const float knobRadius = S(6.0f) + (hovered || active ? S(1.0f) : 0.0f);
    const ImVec2 knobCenter(min.x + (max.x - min.x) * t, rect.center().y);
    drawList->AddCircleFilled(knobCenter, knobRadius + S(2.0f), fade(withAlpha(IM_COL32(0, 0, 0, 255), 0.20f)), 20);
    drawList->AddCircleFilled(knobCenter, knobRadius, fade(IM_COL32(248, 249, 253, 255)), 20);
}

bool slider(const char* id, const Rect& rect, float& value, float minValue, float maxValue, float step, bool enabled) {
    ImGui::SetCursorScreenPos(ImVec2(rect.min.x, rect.min.y - S(8.0f)));
    ImGui::InvisibleButton(id, ImVec2(rect.size().x, rect.size().y + S(16.0f)));
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const bool active = ImGui::IsItemActive();

    bool changed = false;
    if (enabled && active && rect.size().x > 0.0f) {
        const float mouseX = ImGui::GetIO().MousePos.x;
        float t = (mouseX - rect.min.x) / rect.size().x;
        t = clamp(t, 0.0f, 1.0f);
        float next = minValue + t * (maxValue - minValue);
        if (step > 0.0f) {
            next = std::round(next / step) * step;
        }
        next = clamp(next, minValue, maxValue);
        if (std::abs(next - value) > 0.0001f) {
            value = next;
            changed = true;
        }
    }

    const float t = (maxValue - minValue) > 0.0001f ? (value - minValue) / (maxValue - minValue) : 0.0f;
    drawSlider(ImGui::GetWindowDrawList(), rect, t, hovered, active);
    return changed;
}

void drawKeycap(ImDrawList* drawList, const Rect& rect, const char* value, bool hovered, bool capturing, ImFont* font, float fontSize) {
    const ImU32 fill = capturing
        ? withAlpha(palette.accent, 0.20f)
        : withAlpha(IM_COL32(255, 255, 255, 255), hovered ? 0.10f : 0.065f);
    const ImU32 border = capturing
        ? withAlpha(palette.accent, 0.60f)
        : withAlpha(IM_COL32(255, 255, 255, 255), hovered ? 0.16f : 0.09f);

    drawList->AddRectFilled(rect.min, rect.max, fade(fill), S(7.0f));
    drawList->AddRect(rect.min, rect.max, fade(border), S(7.0f));

    const ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, value);
    drawList->AddText(font, fontSize,
        ImVec2(rect.center().x - size.x * 0.5f, rect.center().y - size.y * 0.5f),
        fade(capturing ? palette.accent : palette.textDim), value);
}

void drawTooltip(const char* value) {
    if (!value || !*value) {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(11.0f), S(8.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(9.0f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.055f, 0.063f, 0.08f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, toVec4(withAlpha(palette.border, 0.9f)));
    if (ImGui::BeginTooltip()) {
        ImGui::PushFont(fonts().small, textSize(fonts().small->LegacySize));
        ImGui::PushStyleColor(ImGuiCol_Text, toVec4(palette.textDim));
        ImGui::PushTextWrapPos(S(260.0f));
        ImGui::TextUnformatted(value);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::EndTooltip();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void drawSparkline(ImDrawList* drawList, const Rect& rect, const float* values, int count, ImU32 color, ImU32 fillColor) {
    if (!drawList || !values || count < 2) {
        return;
    }

    float minValue = values[0];
    float maxValue = values[0];
    for (int i = 1; i < count; ++i) {
        minValue = std::min(minValue, values[i]);
        maxValue = std::max(maxValue, values[i]);
    }
    const float range = std::max(1.0f, maxValue - minValue);

    ImVector<ImVec2> points;
    points.reserve(count + 2);
    for (int i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(count - 1);
        const float y = rect.max.y - ((values[i] - minValue) / range) * rect.size().y;
        points.push_back(ImVec2(rect.min.x + rect.size().x * t, y));
    }

    ImVector<ImVec2> fill = points;
    fill.push_back(ImVec2(rect.max.x, rect.max.y));
    fill.push_back(ImVec2(rect.min.x, rect.max.y));
    drawList->AddConvexPolyFilled(fill.Data, fill.Size, fillColor);
    drawList->AddPolyline(points.Data, points.Size, color, S(1.4f), 0);

    const ImVec2 last = points.back();
    drawList->AddCircleFilled(last, S(2.0f), color, 10);
}

bool mouseHovered(const Rect& rect) {
    return ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
           ImGui::IsMouseHoveringRect(rect.min, rect.max);
}

bool mouseClicked(const Rect& rect) {
    return mouseHovered(rect) && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
}

bool mouseDown() {
    return ImGui::IsMouseDown(ImGuiMouseButton_Left);
}

ImU32 accentGradient(float t) {
    return mixColor(palette.accent, palette.accent2, clamp(t, 0.0f, 1.0f));
}

ImU32 moduleTint(int index) {
    static const ImU32 colors[] = {
        IM_COL32(187, 201, 255, 255),
        IM_COL32(246, 185, 211, 255),
        IM_COL32(167, 230, 205, 255),
        IM_COL32(255, 216, 168, 255),
        IM_COL32(196, 181, 255, 255),
    };
    return colors[index % 5];
}

} // namespace neo::ui
