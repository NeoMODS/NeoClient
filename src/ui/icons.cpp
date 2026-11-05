#include "ui/icons.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "ui/theme.h"

#include <cmath>

namespace neo::ui {
namespace {

ImVec2 point(ImVec2 center, float size, float x, float y) {
    return ImVec2(center.x + x * size, center.y + y * size);
}

void line(ImDrawList* drawList, ImVec2 a, ImVec2 b, ImU32 color, float thickness) {
    drawList->AddLine(a, b, color, thickness);
}

void polyline(ImDrawList* drawList, const ImVec2* points, int count, ImU32 color, float thickness, bool closed = false) {
    drawList->AddPolyline(points, count, color, thickness, closed ? ImDrawFlags_Closed : 0);
}

void arc(ImDrawList* drawList, ImVec2 center, float radius, float a0, float a1, ImU32 color, float thickness, int segments = 24) {
    drawList->PathArcTo(center, radius, a0, a1, segments);
    drawList->PathStroke(color, 0, thickness);
}

void ellipse(ImDrawList* drawList, ImVec2 center, float rx, float ry, float a0, float a1, ImU32 color, float thickness, int segments = 28) {
    drawList->PathEllipticalArcTo(center, ImVec2(rx, ry), 0.0f, a0, a1, segments);
    drawList->PathStroke(color, 0, thickness);
}

void chevron(ImDrawList* drawList, ImVec2 center, float size, bool down, ImU32 color, float thickness) {
    const float sign = down ? 1.0f : -1.0f;
    line(drawList, point(center, size, -0.22f, -0.08f * sign), point(center, size, 0.0f, 0.14f * sign), color, thickness);
    line(drawList, point(center, size, 0.0f, 0.14f * sign), point(center, size, 0.22f, -0.08f * sign), color, thickness);
}

} // namespace

void drawIcon(ImDrawList* drawList, Icon icon, ImVec2 center, float size, ImU32 color, float thickness) {
    if (!drawList || icon == Icon::None) {
        return;
    }

    switch (icon) {
    case Icon::AirJump: {
        line(drawList, point(center, size, 0.0f, 0.34f), point(center, size, 0.0f, -0.34f), color, thickness);
        line(drawList, point(center, size, -0.24f, -0.10f), point(center, size, 0.0f, -0.36f), color, thickness);
        line(drawList, point(center, size, 0.0f, -0.36f), point(center, size, 0.24f, -0.10f), color, thickness);
        arc(drawList, point(center, size, 0.0f, 0.30f), size * 0.30f, 0.15f, 3.0f, withAlpha(color, 0.55f), thickness, 18);
        break;
    }
    case Icon::BoatFly: {
        const ImVec2 hull[] = {
            point(center, size, -0.36f, 0.20f),
            point(center, size, 0.30f, 0.20f),
            point(center, size, 0.16f, 0.36f),
            point(center, size, -0.22f, 0.36f),
        };
        polyline(drawList, hull, 4, color, thickness, true);
        line(drawList, point(center, size, 0.0f, 0.12f), point(center, size, 0.0f, -0.32f), color, thickness);
        line(drawList, point(center, size, -0.20f, -0.12f), point(center, size, 0.0f, -0.34f), color, thickness);
        line(drawList, point(center, size, 0.0f, -0.34f), point(center, size, 0.20f, -0.12f), color, thickness);
        break;
    }
    case Icon::Fly: {
        const ImVec2 wingA[] = {
            point(center, size, -0.38f, 0.18f),
            point(center, size, -0.06f, -0.02f),
            point(center, size, -0.18f, 0.34f),
        };
        const ImVec2 wingB[] = {
            point(center, size, 0.38f, 0.18f),
            point(center, size, 0.06f, -0.02f),
            point(center, size, 0.18f, 0.34f),
        };
        drawList->AddTriangleFilled(wingA[0], wingA[1], wingA[2], withAlpha(color, 0.32f));
        drawList->AddTriangleFilled(wingB[0], wingB[1], wingB[2], withAlpha(color, 0.32f));
        line(drawList, point(center, size, -0.34f, 0.16f), point(center, size, 0.0f, -0.02f), color, thickness);
        line(drawList, point(center, size, 0.34f, 0.16f), point(center, size, 0.0f, -0.02f), color, thickness);
        line(drawList, point(center, size, 0.0f, -0.02f), point(center, size, 0.0f, 0.30f), color, thickness);
        break;
    }
    case Icon::Speed: {
        arc(drawList, point(center, size, 0.0f, 0.18f), size * 0.34f, 3.35f, 6.08f, color, thickness, 22);
        line(drawList, point(center, size, 0.0f, 0.18f), point(center, size, 0.22f, -0.12f), color, thickness);
        drawList->AddCircleFilled(point(center, size, 0.0f, 0.18f), size * 0.055f, color, 12);
        break;
    }
    case Icon::NoClip: {
        drawList->AddRect(point(center, size, -0.32f, -0.25f), point(center, size, 0.32f, 0.25f), color, size * 0.06f);
        line(drawList, point(center, size, -0.38f, 0.30f), point(center, size, 0.32f, -0.32f), withAlpha(color, 0.55f), thickness);
        line(drawList, point(center, size, 0.10f, -0.32f), point(center, size, 0.34f, -0.32f), color, thickness);
        line(drawList, point(center, size, 0.34f, -0.32f), point(center, size, 0.34f, -0.08f), color, thickness);
        break;
    }
    case Icon::AutoJump: {
        line(drawList, point(center, size, -0.34f, 0.34f), point(center, size, 0.34f, 0.34f), withAlpha(color, 0.55f), thickness);
        line(drawList, point(center, size, 0.0f, 0.22f), point(center, size, 0.0f, -0.30f), color, thickness);
        line(drawList, point(center, size, -0.20f, -0.10f), point(center, size, 0.0f, -0.32f), color, thickness);
        line(drawList, point(center, size, 0.0f, -0.32f), point(center, size, 0.20f, -0.10f), color, thickness);
        break;
    }
    case Icon::AutoWalk: {
        line(drawList, point(center, size, -0.32f, 0.28f), point(center, size, 0.28f, -0.28f), color, thickness);
        line(drawList, point(center, size, 0.04f, -0.32f), point(center, size, 0.32f, -0.32f), color, thickness);
        line(drawList, point(center, size, 0.32f, -0.32f), point(center, size, 0.32f, -0.04f), color, thickness);
        line(drawList, point(center, size, -0.32f, 0.28f), point(center, size, -0.02f, 0.28f), withAlpha(color, 0.55f), thickness);
        break;
    }
    case Icon::Sprint: {
        line(drawList, point(center, size, -0.34f, -0.24f), point(center, size, -0.02f, 0.0f), color, thickness);
        line(drawList, point(center, size, -0.02f, 0.0f), point(center, size, -0.34f, 0.24f), color, thickness);
        line(drawList, point(center, size, 0.04f, -0.24f), point(center, size, 0.36f, 0.0f), withAlpha(color, 0.55f), thickness);
        line(drawList, point(center, size, 0.36f, 0.0f), point(center, size, 0.04f, 0.24f), withAlpha(color, 0.55f), thickness);
        break;
    }
    case Icon::Fullbright: {
        drawList->AddCircleFilled(center, size * 0.15f, color, 18);
        for (int i = 0; i < 8; ++i) {
            const float angle = static_cast<float>(i) * (2.0f * IM_PI / 8.0f);
            const ImVec2 a(center.x + std::cos(angle) * size * 0.24f, center.y + std::sin(angle) * size * 0.24f);
            const ImVec2 b(center.x + std::cos(angle) * size * 0.38f, center.y + std::sin(angle) * size * 0.38f);
            line(drawList, a, b, color, thickness);
        }
        break;
    }
    case Icon::Hud: {
        drawList->AddRect(point(center, size, -0.36f, -0.26f), point(center, size, 0.36f, 0.26f), color, size * 0.07f);
        line(drawList, point(center, size, -0.24f, 0.06f), point(center, size, 0.24f, 0.06f), withAlpha(color, 0.55f), thickness);
        line(drawList, point(center, size, -0.24f, 0.16f), point(center, size, 0.10f, 0.16f), withAlpha(color, 0.35f), thickness);
        break;
    }
    case Icon::NoFall: {
        line(drawList, point(center, size, 0.0f, -0.34f), point(center, size, 0.0f, 0.26f), color, thickness);
        line(drawList, point(center, size, -0.20f, 0.08f), point(center, size, 0.0f, 0.30f), color, thickness);
        line(drawList, point(center, size, 0.0f, 0.30f), point(center, size, 0.20f, 0.08f), color, thickness);
        line(drawList, point(center, size, -0.34f, 0.34f), point(center, size, 0.34f, 0.34f), withAlpha(color, 0.45f), thickness);
        break;
    }
    case Icon::Respawn: {
        arc(drawList, center, size * 0.30f, 0.4f, 4.9f, color, thickness, 22);
        line(drawList, point(center, size, 0.24f, -0.30f), point(center, size, 0.36f, -0.22f), color, thickness);
        line(drawList, point(center, size, 0.36f, -0.22f), point(center, size, 0.30f, -0.08f), color, thickness);
        break;
    }
    case Icon::Zoom: {
        drawList->AddCircle(point(center, size, -0.08f, -0.08f), size * 0.24f, color, 20, thickness);
        line(drawList, point(center, size, 0.10f, 0.10f), point(center, size, 0.34f, 0.34f), color, thickness);
        line(drawList, point(center, size, -0.08f, -0.18f), point(center, size, -0.08f, 0.02f), withAlpha(color, 0.55f), thickness);
        line(drawList, point(center, size, -0.18f, -0.08f), point(center, size, 0.02f, -0.08f), withAlpha(color, 0.55f), thickness);
        break;
    }
    case Icon::Exploit: {
        const ImVec2 bolt[] = {
            point(center, size, 0.08f, -0.38f),
            point(center, size, -0.22f, 0.04f),
            point(center, size, -0.02f, 0.04f),
            point(center, size, -0.12f, 0.38f),
            point(center, size, 0.24f, -0.08f),
            point(center, size, 0.03f, -0.08f),
        };
        drawList->AddConvexPolyFilled(bolt, 6, color);
        break;
    }
    case Icon::Misc: {
        drawList->AddCircleFilled(point(center, size, -0.18f, -0.18f), size * 0.07f, color, 12);
        drawList->AddCircleFilled(point(center, size, 0.18f, -0.18f), size * 0.07f, withAlpha(color, 0.75f), 12);
        drawList->AddCircleFilled(point(center, size, -0.18f, 0.18f), size * 0.07f, withAlpha(color, 0.75f), 12);
        drawList->AddCircleFilled(point(center, size, 0.18f, 0.18f), size * 0.07f, color, 12);
        break;
    }
    case Icon::Player: {
        drawList->AddCircle(point(center, size, 0.0f, -0.16f), size * 0.14f, color, 18, thickness);
        ellipse(drawList, point(center, size, 0.0f, 0.34f), size * 0.28f, size * 0.20f, 3.15f, 6.0f, color, thickness, 20);
        break;
    }
    case Icon::World: {
        drawList->AddCircle(center, size * 0.32f, color, 24, thickness);
        ellipse(drawList, center, size * 0.14f, size * 0.30f, 0.0f, 6.283f, color, thickness, 22);
        line(drawList, point(center, size, -0.30f, 0.0f), point(center, size, 0.30f, 0.0f), withAlpha(color, 0.55f), thickness);
        break;
    }
    case Icon::Movement: {
        line(drawList, point(center, size, 0.0f, -0.34f), point(center, size, 0.0f, 0.34f), color, thickness);
        line(drawList, point(center, size, -0.34f, 0.0f), point(center, size, 0.34f, 0.0f), color, thickness);
        line(drawList, point(center, size, -0.10f, -0.24f), point(center, size, 0.0f, -0.36f), color, thickness);
        line(drawList, point(center, size, 0.0f, -0.36f), point(center, size, 0.10f, -0.24f), color, thickness);
        line(drawList, point(center, size, -0.10f, 0.24f), point(center, size, 0.0f, 0.36f), color, thickness);
        line(drawList, point(center, size, 0.0f, 0.36f), point(center, size, 0.10f, 0.24f), color, thickness);
        line(drawList, point(center, size, -0.24f, -0.10f), point(center, size, -0.36f, 0.0f), color, thickness);
        line(drawList, point(center, size, -0.36f, 0.0f), point(center, size, -0.24f, 0.10f), color, thickness);
        line(drawList, point(center, size, 0.24f, -0.10f), point(center, size, 0.36f, 0.0f), color, thickness);
        line(drawList, point(center, size, 0.36f, 0.0f), point(center, size, 0.24f, 0.10f), color, thickness);
        break;
    }
    case Icon::Visuals: {
        arc(drawList, point(center, size, 0.0f, 0.10f), size * 0.30f, 3.35f, 6.08f, color, thickness, 20);
        arc(drawList, point(center, size, 0.0f, 0.10f), size * 0.30f, 0.20f, 2.94f, color, thickness, 20);
        drawList->AddCircleFilled(point(center, size, 0.0f, 0.10f), size * 0.075f, color, 12);
        break;
    }
    case Icon::Utility: {
        line(drawList, point(center, size, -0.34f, -0.18f), point(center, size, 0.34f, -0.18f), withAlpha(color, 0.55f), thickness);
        drawList->AddCircleFilled(point(center, size, -0.10f, -0.18f), size * 0.075f, color, 12);
        line(drawList, point(center, size, -0.34f, 0.14f), point(center, size, 0.34f, 0.14f), color, thickness);
        drawList->AddCircleFilled(point(center, size, 0.16f, 0.14f), size * 0.075f, color, 12);
        break;
    }
    case Icon::Search: {
        drawList->AddCircle(point(center, size, -0.06f, -0.06f), size * 0.24f, color, 20, thickness);
        line(drawList, point(center, size, 0.12f, 0.12f), point(center, size, 0.34f, 0.34f), color, thickness);
        break;
    }
    case Icon::Close: {
        line(drawList, point(center, size, -0.22f, -0.22f), point(center, size, 0.22f, 0.22f), color, thickness);
        line(drawList, point(center, size, 0.22f, -0.22f), point(center, size, -0.22f, 0.22f), color, thickness);
        break;
    }
    case Icon::ChevronRight: {
        chevron(drawList, center, size, false, color, thickness);
        break;
    }
    case Icon::ChevronDown: {
        chevron(drawList, center, size, true, color, thickness);
        break;
    }
    case Icon::Check: {
        line(drawList, point(center, size, -0.24f, 0.02f), point(center, size, -0.05f, 0.22f), color, thickness);
        line(drawList, point(center, size, -0.05f, 0.22f), point(center, size, 0.28f, -0.22f), color, thickness);
        break;
    }
    case Icon::Settings: {
        line(drawList, point(center, size, -0.34f, -0.22f), point(center, size, 0.34f, -0.22f), withAlpha(color, 0.55f), thickness);
        drawList->AddCircleFilled(point(center, size, 0.14f, -0.22f), size * 0.08f, color, 12);
        line(drawList, point(center, size, -0.34f, 0.08f), point(center, size, 0.34f, 0.08f), color, thickness);
        drawList->AddCircleFilled(point(center, size, -0.12f, 0.08f), size * 0.08f, color, 12);
        line(drawList, point(center, size, -0.34f, 0.32f), point(center, size, 0.34f, 0.32f), withAlpha(color, 0.55f), thickness);
        drawList->AddCircleFilled(point(center, size, 0.04f, 0.32f), size * 0.08f, color, 12);
        break;
    }
    case Icon::Power: {
        arc(drawList, center, size * 0.28f, 0.9f, 5.35f, color, thickness, 24);
        line(drawList, point(center, size, 0.0f, -0.36f), point(center, size, 0.0f, 0.02f), color, thickness);
        break;
    }
    case Icon::Spark: {
        const ImVec2 star[] = {
            point(center, size, 0.0f, -0.38f),
            point(center, size, 0.09f, -0.09f),
            point(center, size, 0.38f, 0.0f),
            point(center, size, 0.09f, 0.09f),
            point(center, size, 0.0f, 0.38f),
            point(center, size, -0.09f, 0.09f),
            point(center, size, -0.38f, 0.0f),
            point(center, size, -0.09f, -0.09f),
        };
        drawList->AddConvexPolyFilled(star, 8, color);
        break;
    }
    case Icon::Info: {
        drawList->AddCircle(center, size * 0.32f, color, 24, thickness);
        drawList->AddCircleFilled(point(center, size, 0.0f, -0.14f), size * 0.045f, color, 10);
        line(drawList, point(center, size, 0.0f, -0.01f), point(center, size, 0.0f, 0.20f), color, thickness);
        break;
    }
    case Icon::Folder: {
        line(drawList, point(center, size, -0.34f, -0.20f), point(center, size, -0.10f, -0.20f), color, thickness);
        line(drawList, point(center, size, -0.10f, -0.20f), point(center, size, -0.01f, -0.09f), color, thickness);
        drawList->AddRect(point(center, size, -0.34f, -0.20f), point(center, size, 0.34f, 0.26f), color, size * 0.05f, 0, thickness);
        break;
    }
    case Icon::Key: {
        drawList->AddCircle(point(center, size, -0.18f, -0.08f), size * 0.14f, color, 16, thickness);
        line(drawList, point(center, size, -0.06f, 0.02f), point(center, size, 0.32f, 0.32f), color, thickness);
        line(drawList, point(center, size, 0.18f, 0.18f), point(center, size, 0.28f, 0.08f), color, thickness);
        break;
    }
    case Icon::Sliders: {
        line(drawList, point(center, size, -0.34f, -0.24f), point(center, size, 0.34f, -0.24f), withAlpha(color, 0.55f), thickness);
        drawList->AddCircleFilled(point(center, size, -0.08f, -0.24f), size * 0.075f, color, 12);
        line(drawList, point(center, size, -0.34f, 0.24f), point(center, size, 0.34f, 0.24f), color, thickness);
        drawList->AddCircleFilled(point(center, size, 0.16f, 0.24f), size * 0.075f, color, 12);
        break;
    }
    case Icon::Trash: {
        drawList->AddRect(point(center, size, -0.22f, -0.18f), point(center, size, 0.22f, 0.30f), color, size * 0.05f, 0, thickness);
        line(drawList, point(center, size, -0.30f, -0.28f), point(center, size, 0.30f, -0.28f), color, thickness);
        line(drawList, point(center, size, -0.10f, -0.36f), point(center, size, 0.10f, -0.36f), color, thickness);
        break;
    }
    case Icon::Refresh: {
        arc(drawList, center, size * 0.28f, 0.55f, 5.55f, color, thickness, 24);
        line(drawList, point(center, size, 0.25f, -0.26f), point(center, size, 0.37f, -0.20f), color, thickness);
        line(drawList, point(center, size, 0.37f, -0.20f), point(center, size, 0.31f, -0.06f), color, thickness);
        break;
    }
    case Icon::Unload: {
        line(drawList, point(center, size, -0.30f, -0.26f), point(center, size, -0.30f, 0.28f), color, thickness);
        line(drawList, point(center, size, -0.30f, 0.28f), point(center, size, 0.12f, 0.28f), color, thickness);
        line(drawList, point(center, size, -0.02f, -0.02f), point(center, size, 0.34f, -0.02f), color, thickness);
        line(drawList, point(center, size, 0.18f, -0.18f), point(center, size, 0.36f, -0.02f), color, thickness);
        line(drawList, point(center, size, 0.36f, -0.02f), point(center, size, 0.18f, 0.14f), color, thickness);
        break;
    }
    case Icon::Dot: {
        drawList->AddCircleFilled(center, size * 0.12f, color, 12);
        break;
    }
    case Icon::Menu: {
        line(drawList, point(center, size, -0.34f, -0.22f), point(center, size, 0.34f, -0.22f), color, thickness);
        line(drawList, point(center, size, -0.34f, 0.0f), point(center, size, 0.34f, 0.0f), color, thickness);
        line(drawList, point(center, size, -0.34f, 0.22f), point(center, size, 0.34f, 0.22f), color, thickness);
        break;
    }
    case Icon::None:
        break;
    }
}

} // namespace neo::ui
