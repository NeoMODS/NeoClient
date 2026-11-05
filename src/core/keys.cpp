#include "core/keys.h"

#include <windows.h>

#include <array>
#include <cstdio>

namespace neo::keys {
namespace {

const char* simpleName(int vk) {
    switch (vk) {
    case 0: return "None";
    case VK_LBUTTON: return "LMB";
    case VK_RBUTTON: return "RMB";
    case VK_MBUTTON: return "MMB";
    case VK_XBUTTON1: return "MB4";
    case VK_XBUTTON2: return "MB5";
    case VK_BACK: return "Backspace";
    case VK_TAB: return "Tab";
    case VK_CLEAR: return "Clear";
    case VK_RETURN: return "Enter";
    case VK_SHIFT: case VK_LSHIFT: return "LShift";
    case VK_RSHIFT: return "RShift";
    case VK_CONTROL: return "Ctrl";
    case VK_LCONTROL: return "LCtrl";
    case VK_RCONTROL: return "RCtrl";
    case VK_MENU: return "Alt";
    case VK_LMENU: return "LAlt";
    case VK_RMENU: return "RAlt";
    case VK_PAUSE: return "Pause";
    case VK_CAPITAL: return "Caps";
    case VK_ESCAPE: return "Esc";
    case VK_SPACE: return "Space";
    case VK_PRIOR: return "PageUp";
    case VK_NEXT: return "PageDown";
    case VK_END: return "End";
    case VK_HOME: return "Home";
    case VK_LEFT: return "Left";
    case VK_UP: return "Up";
    case VK_RIGHT: return "Right";
    case VK_DOWN: return "Down";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_NUMLOCK: return "NumLock";
    case VK_SCROLL: return "Scroll";
    case VK_SNAPSHOT: return "Print";
    case VK_OEM_1: return ";";
    case VK_OEM_PLUS: return "=";
    case VK_OEM_COMMA: return ",";
    case VK_OEM_MINUS: return "-";
    case VK_OEM_PERIOD: return ".";
    case VK_OEM_2: return "/";
    case VK_OEM_3: return "`";
    case VK_OEM_4: return "[";
    case VK_OEM_5: return "\\";
    case VK_OEM_6: return "]";
    case VK_OEM_7: return "'";
    default: break;
    }

    if (vk >= VK_F1 && vk <= VK_F24) {
        static thread_local std::array<char, 8> buffer{};
        std::snprintf(buffer.data(), buffer.size(), "F%d", vk - VK_F1 + 1);
        return buffer.data();
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        static thread_local std::array<std::array<char, 8>, 10> buffers{};
        auto& buffer = buffers[vk - VK_NUMPAD0];
        std::snprintf(buffer.data(), buffer.size(), "Num%d", vk - VK_NUMPAD0);
        return buffer.data();
    }
    if (vk >= 'A' && vk <= 'Z') {
        static thread_local std::array<std::array<char, 2>, 26> buffers{};
        auto& buffer = buffers[vk - 'A'];
        buffer[0] = static_cast<char>(vk);
        buffer[1] = '\0';
        return buffer.data();
    }
    if (vk >= '0' && vk <= '9') {
        static thread_local std::array<std::array<char, 2>, 10> buffers{};
        auto& buffer = buffers[vk - '0'];
        buffer[0] = static_cast<char>(vk);
        buffer[1] = '\0';
        return buffer.data();
    }

    return "Unknown";
}

} // namespace

std::string name(int virtualKey) {
    if (virtualKey <= 0) {
        return "None";
    }
    return simpleName(virtualKey);
}

bool down(int virtualKey) {
    if (virtualKey <= 0) {
        return false;
    }
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

bool pressed(int virtualKey) {
    if (virtualKey <= 0) {
        return false;
    }
    return (GetAsyncKeyState(virtualKey) & 0x0001) != 0;
}

int fromImGuiKey(int imguiKey) {
    switch (imguiKey) {
    case 512: return VK_TAB;
    case 513: return VK_LEFT;
    case 514: return VK_RIGHT;
    case 515: return VK_UP;
    case 516: return VK_DOWN;
    case 517: return VK_PRIOR;
    case 518: return VK_NEXT;
    case 519: return VK_HOME;
    case 520: return VK_END;
    case 521: return VK_INSERT;
    case 522: return VK_DELETE;
    case 523: return VK_BACK;
    case 524: return VK_SPACE;
    case 525: return VK_RETURN;
    case 526: return VK_ESCAPE;
    case 527: return VK_LCONTROL;
    case 528: return VK_LSHIFT;
    case 529: return VK_LMENU;
    case 530: return VK_LWIN;
    case 531: return VK_RCONTROL;
    case 532: return VK_RSHIFT;
    case 533: return VK_RMENU;
    case 534: return VK_RWIN;
    case 535: return VK_APPS;
    case 536: return VK_CAPITAL;
    case 537: return VK_SCROLL;
    case 538: return VK_NUMLOCK;
    case 539: return VK_SNAPSHOT;
    case 540: return VK_PAUSE;
    default: break;
    }

    if (imguiKey >= 546 && imguiKey <= 571) {
        return VK_F1 + (imguiKey - 546);
    }
    if (imguiKey >= 572 && imguiKey <= 581) {
        return '0' + (imguiKey - 572);
    }
    if (imguiKey >= 582 && imguiKey <= 607) {
        return 'A' + (imguiKey - 582);
    }
    return 0;
}

} // namespace neo::keys
