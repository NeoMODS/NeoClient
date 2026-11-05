#pragma once

#include "imgui.h"

namespace neo::ui {

enum class Icon {
    None,
    AirJump,
    BoatFly,
    Fly,
    Speed,
    NoClip,
    AutoJump,
    AutoWalk,
    Sprint,
    Fullbright,
    Hud,
    NoFall,
    Respawn,
    Zoom,

    Exploit,
    Misc,
    Player,
    World,
    Movement,
    Visuals,
    Utility,

    Search,
    Close,
    ChevronRight,
    ChevronDown,
    Check,
    Settings,
    Power,
    Spark,
    Info,
    Folder,
    Key,
    Sliders,
    Trash,
    Refresh,
    Unload,
    Dot,
    Menu,
};

void drawIcon(ImDrawList* drawList, Icon icon, ImVec2 center, float size, ImU32 color, float thickness);

} // namespace neo::ui
