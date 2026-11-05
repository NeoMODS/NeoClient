#include "ui/gui.h"

#include "core/keys.h"
#include "core/math.h"
#include "game/ores.h"
#include "ui/fonts.h"
#include "ui/icons.h"
#include "ui/theme.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace neo::ui {
namespace {

constexpr float kHeaderHeight = 74.0f;
constexpr int kTabCount = 5;
constexpr float kTabWidth = 88.0f;
constexpr float kTabGap = 4.0f;
constexpr float kTabsOriginX = 176.0f;

ImU32 F(ImU32 color) {
    return fade(color);
}

float fs(ImFont* font) {
    return font ? textSize(font->LegacySize) : textSize(14.0f);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool matches(const Module& module, const char* query) {
    if (!query || !*query) {
        return true;
    }
    const std::string needle = lower(query);
    return lower(module.name()).find(needle) != std::string::npos ||
           lower(module.description()).find(needle) != std::string::npos;
}

std::string formatValue(const Setting& setting) {
    char buf[32]{};
    if (setting.type == Setting::Type::Slider) {
        std::snprintf(buf, sizeof(buf), "%.2f", setting.value);
    } else {
        std::snprintf(buf, sizeof(buf), "%s", setting.toggleValue ? "on" : "off");
    }
    return std::string(buf);
}




void drawLogo(ImDrawList* drawList, ImVec2 min, float size) {
    const ImVec2 max(min.x + size, min.y + size);
    drawList->AddRectFilled(min, max, F(withAlpha(palette.accent, 0.16f)), size * 0.30f);
    drawList->AddRectFilledMultiColor(min, max,
        F(withAlpha(palette.accent, 0.42f)), F(withAlpha(palette.accent2, 0.28f)),
        F(withAlpha(palette.accent2, 0.10f)), F(withAlpha(palette.accent, 0.14f)));
    drawList->AddRect(min, max, F(withAlpha(palette.accent, 0.32f)), size * 0.30f);

    const ImU32 ink = F(withAlpha(IM_COL32(255, 255, 255, 255), 0.94f));
    const float thickness = std::max(1.2f, size * 0.075f);
    const ImVec2 a(min.x + size * 0.30f, min.y + size * 0.70f);
    const ImVec2 b(min.x + size * 0.30f, min.y + size * 0.30f);
    const ImVec2 c(min.x + size * 0.70f, min.y + size * 0.70f);
    const ImVec2 d(min.x + size * 0.70f, min.y + size * 0.30f);
    drawList->AddLine(a, b, ink, thickness);
    drawList->AddLine(b, c, ink, thickness);
    drawList->AddLine(c, d, ink, thickness);
    drawList->AddCircleFilled(ImVec2(max.x - size * 0.20f, max.y - size * 0.20f), size * 0.045f, F(palette.accent2), 8);
}

void drawSectionLabel(ImDrawList* drawList, ImVec2 pos, const char* label) {
    letterSpacedText(drawList, fonts().tiny, fs(fonts().tiny), pos, S(1.35f), F(withAlpha(palette.textDim, 0.72f)), label);
}

void drawCardSheen(ImDrawList* drawList, const Rect& rect, float alpha = 0.42f) {
    (void)drawList; (void)rect; (void)alpha;
}

void drawHoverBackground(ImDrawList* drawList, const Rect& rect, bool hovered, bool active, bool enabled) {
    ImU32 fill = active ? palette.panelActive : (hovered ? palette.panelHover : palette.panel);
    drawList->AddRectFilled(rect.min, rect.max, F(fill), S(8.0f));
    drawList->AddRect(rect.min, rect.max, F(hovered || active ? palette.border : palette.borderSoft), S(8.0f));
    if (enabled) {
        drawList->AddRectFilled(ImVec2(rect.min.x, rect.min.y + S(8.0f)),
            ImVec2(rect.min.x + S(2.0f), rect.max.y - S(8.0f)),
            F(withAlpha(palette.accent, 0.8f)), S(2.0f));
    }
}

// Shared header geometry so the tab strip, drag area and search field keep
// their spacing when the window is narrower than the default width.
struct HeaderMetrics {
    bool showSearch = true;
    float tabOriginX = 0.0f;
    float tabWidth = 0.0f;
    float tabGap = 0.0f;
    float searchX = 0.0f;
    float searchWidth = 0.0f;
};

HeaderMetrics headerMetrics(float width) {
    HeaderMetrics metrics;
    metrics.tabGap = S(kTabGap);
    metrics.tabOriginX = S(kTabsOriginX);
    metrics.searchWidth = S(205.0f);
    metrics.showSearch = width >= S(790.0f);

    const float searchEnd = width - S(132.0f);
    const float tabsEnd = metrics.showSearch ? searchEnd - metrics.searchWidth - S(12.0f) : width - S(120.0f);
    const float available = std::max(0.0f, tabsEnd - metrics.tabOriginX);
    const float rawWidth = (available - static_cast<float>(kTabCount - 1) * metrics.tabGap) / static_cast<float>(kTabCount);
    metrics.tabWidth = clamp(rawWidth, S(52.0f), S(kTabWidth));
    metrics.searchX = metrics.showSearch ? searchEnd - metrics.searchWidth : 0.0f;
    return metrics;
}

struct Layout {
    ImVec2 pos;
    ImVec2 size;
    float pad;
    float headerHeight;
    float bodyTop;
    float bodyBottom;
    float sidebarWidth;
    float centerWidth;
    float rightWidth;
    Rect sidebar;
    Rect center;
    Rect right;
};

Layout makeLayout(ImVec2 pos, ImVec2 size) {
    Layout layout;
    layout.pos = pos;
    layout.size = size;
    layout.pad = S(26.0f);
    layout.headerHeight = S(kHeaderHeight);
    layout.bodyTop = pos.y + layout.headerHeight + S(12.0f);
    layout.bodyBottom = pos.y + size.y - S(20.0f);
    layout.sidebarWidth = S(190.0f);
    layout.rightWidth = S(286.0f);

    const float sidebarX = pos.x + layout.pad;
    const float rightX = pos.x + size.x - layout.pad - layout.rightWidth;
    const float centerX = sidebarX + layout.sidebarWidth + S(16.0f);
    layout.centerWidth = rightX - centerX - S(16.0f);

    const float bodyHeight = layout.bodyBottom - layout.bodyTop;
    layout.sidebar = Rect{ImVec2(sidebarX, layout.bodyTop), ImVec2(sidebarX + layout.sidebarWidth, layout.bodyTop + bodyHeight)};
    layout.center = Rect{ImVec2(centerX, layout.bodyTop), ImVec2(centerX + layout.centerWidth, layout.bodyTop + bodyHeight)};
    layout.right = Rect{ImVec2(rightX, layout.bodyTop), ImVec2(rightX + layout.rightWidth, layout.bodyTop + bodyHeight)};
    return layout;
}

void drawEmptyState(ImDrawList* drawList, const Rect& rect, Icon icon, const char* title, const char* subtitle) {
    const ImVec2 center(rect.center().x, rect.min.y + rect.size().y * 0.42f);
    const float radius = S(34.0f);
    drawList->AddCircleFilled(center, radius, F(withAlpha(palette.accent, 0.055f)), 40);
    drawList->AddCircle(center, radius, F(withAlpha(palette.accent, 0.14f)), 40, S(1.0f));
    drawIcon(drawList, icon, center, S(22.0f), F(palette.accent), S(1.6f));

    const ImVec2 titleSize = fonts().title->CalcTextSizeA(fs(fonts().title), FLT_MAX, 0.0f, title);
    text(drawList, fonts().title, fs(fonts().title),
        ImVec2(center.x - titleSize.x * 0.5f, center.y + radius + S(18.0f)),
        F(palette.text), title);

    const ImVec2 subtitleSize = fonts().small->CalcTextSizeA(fs(fonts().small), FLT_MAX, 0.0f, subtitle);
    text(drawList, fonts().small, fs(fonts().small),
        ImVec2(center.x - subtitleSize.x * 0.5f, center.y + radius + S(45.0f)),
        F(palette.textFaint), subtitle);
}

} // namespace

Gui::Gui() {
    state_.fpsHistory.reserve(64);
}

void Gui::toast(const std::string& message, bool enabled) {
    Toast toast;
    toast.text = message;
    toast.enabled = enabled;
    toast.life = 2.4f;
    toast.anim = 0.0f;
    state_.toasts.push_back(std::move(toast));
    while (state_.toasts.size() > 3) {
        state_.toasts.pop_front();
    }
}

bool Gui::handleKeyDown(int virtualKey) {
    if (state_.captureId.empty()) {
        return false;
    }

    if (virtualKey == VK_ESCAPE) {
        state_.captureId.clear();
        return true;
    }

    if (virtualKey == VK_DELETE || virtualKey == VK_BACK) {
        applyCapturedKey(0);
    } else {
        applyCapturedKey(virtualKey);
    }
    state_.captureId.clear();
    state_.dirty = true;
    return true;
}

void Gui::applyCapturedKey(int virtualKey) {
    if (state_.captureId.empty()) {
        return;
    }
    // ModuleManager is not reachable from the input callback, so queue the bind
    // and resolve it against the live module list during the next render.
    state_.pendingBindId = state_.captureId;
    state_.pendingBindKey = virtualKey;
    state_.pendingBind = true;
}

void Gui::resolvePendingBind(ModuleManager& modules) {
    if (!state_.pendingBind) {
        return;
    }
    state_.pendingBind = false;

    const std::string& target = state_.pendingBindId;
    const int key = state_.pendingBindKey;

    if (target == "menu") {
        if (key > 0) {
            const int previous = state_.menuKey;
            state_.menuKey = key;
            state_.dirty = true;
            if (key != previous) {
                toast("Menu key: " + keys::name(key), true);
            }
        }
        return;
    }

    if (target.rfind("module:", 0) == 0) {
        if (Module* module = modules.find(target.substr(7))) {
            module->setKeybind(key);
            state_.dirty = true;
        }
        return;
    }

    if (target.rfind("keybind:", 0) == 0) {
        if (Module* module = modules.find(target.substr(8))) {
            module->setKeybind(key);
            state_.dirty = true;
        }
        return;
    }

    if (target.rfind("setting:", 0) == 0) {
        const std::string rest = target.substr(8);
        const auto separator = rest.find(':');
        if (separator == std::string::npos) {
            return;
        }
        Module* module = modules.find(rest.substr(0, separator));
        if (!module) {
            return;
        }
        if (Setting* setting = module->findSetting(rest.substr(separator + 1))) {
            setting->key = key;
            module->setKeybind(key);
            state_.dirty = true;
        }
    }
}

void Gui::updateAnimations(float dt) {
    state_.menuAnim = state_.ui.animations
        ? approach(state_.menuAnim, state_.menuOpen ? 1.0f : 0.0f, 18.0f, dt)
        : (state_.menuOpen ? 1.0f : 0.0f);

    for (auto& toast : state_.toasts) {
        toast.life -= dt;
        toast.anim = approach(toast.anim, 1.0f, 14.0f, dt);
    }
    state_.toasts.erase(
        std::remove_if(state_.toasts.begin(), state_.toasts.end(), [](const Toast& toast) {
            return toast.life <= 0.0f;
        }),
        state_.toasts.end());
}

void Gui::render(ModuleManager& modules, GameAccess& game, float dt, const UiActions& actions) {
    clampUiSettings(state_.ui);
    setTextScale(1.0f);
    updateAnimations(dt);
    resolvePendingBind(modules);

    const int fps = game.snapshot().available ? game.snapshot().fps : 0;
    fpsSmooth_ = approach(fpsSmooth_, static_cast<float>(fps), 7.0f, dt);
    fpsTimer_ += dt;
    if (fpsTimer_ >= 0.12f) {
        fpsTimer_ = 0.0f;
        state_.fpsHistory.push_back(fpsSmooth_);
        if (state_.fpsHistory.size() > 64) {
            state_.fpsHistory.erase(state_.fpsHistory.begin());
        }
    }

    // X-Ray world markers sit behind everything (including the HUD and dim),
    // so they are rendered first on the background draw list.
    drawWorldMarkers(modules, game);

    if (state_.menuAnim < 0.995f) {
        drawHud(modules, game, dt);
    }

    if (state_.menuAnim > 0.001f && game.snapshot().inGame) {
        ImDrawList* background = ImGui::GetBackgroundDrawList();
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const int dim = static_cast<int>(std::round(96.0f * state_.menuAnim));
        background->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(6, 8, 12, dim));
    }

    if (state_.menuOpen || state_.menuAnim > 0.004f) {
        drawMenu(modules, game, dt, actions);
    }

    if (state_.dirty && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && state_.captureId.empty()) {
        if (actions.save) {
            actions.save();
        }
        state_.dirty = false;
    }
}

void Gui::drawMenu(ModuleManager& modules, GameAccess& game, float dt, const UiActions& actions) {
    // The font size setting is scoped to the menu: the HUD uses fixed layout
    // offsets that would overlap if its text grew with the menu scale.
    setTextScale(state_.ui.fontScale);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    // The saved size is clamped to the viewport so the window can never hang
    // off-screen when the game runs at a smaller resolution than the setting.
    const float maxWidth = std::max(S(560.0f), viewport->WorkSize.x - S(16.0f));
    const float maxHeight = std::max(S(360.0f), viewport->WorkSize.y - S(16.0f));
    const ImVec2 size(
        std::min(S(state_.ui.windowWidth), maxWidth),
        std::min(S(state_.ui.windowHeight), maxHeight));

    if (!state_.windowPosInit) {
        state_.windowPos = ImVec2(
            viewport->WorkPos.x + (viewport->WorkSize.x - size.x) * 0.5f,
            viewport->WorkPos.y + (viewport->WorkSize.y - size.y) * 0.5f - S(6.0f));
        state_.windowPosInit = true;
    }

    const ImVec2 maxPos(
        viewport->WorkPos.x + viewport->WorkSize.x - size.x - S(8.0f),
        viewport->WorkPos.y + viewport->WorkSize.y - size.y - S(8.0f));
    state_.windowPos.x = clamp(state_.windowPos.x, viewport->WorkPos.x + S(8.0f), std::max(viewport->WorkPos.x + S(8.0f), maxPos.x));
    state_.windowPos.y = clamp(state_.windowPos.y, viewport->WorkPos.y + S(8.0f), std::max(viewport->WorkPos.y + S(8.0f), maxPos.y));

    const ImVec2 drawPos(state_.windowPos.x, state_.windowPos.y + (1.0f - easeOutCubic(state_.menuAnim)) * S(14.0f));

    const Rect dragArea(ImVec2(drawPos.x, drawPos.y), ImVec2(drawPos.x + size.x, drawPos.y + S(kHeaderHeight)));
    const bool searchFocused = ImGui::GetIO().WantTextInput;
    if (!state_.dragging && !searchFocused && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && dragArea.contains(ImGui::GetIO().MousePos)) {
        const HeaderMetrics metrics = headerMetrics(size.x);
        const float tabStart = drawPos.x + metrics.tabOriginX;
        const float tabEnd = tabStart + static_cast<float>(kTabCount) * metrics.tabWidth +
                             static_cast<float>(kTabCount - 1) * metrics.tabGap;
        const bool overTabs = ImGui::GetIO().MousePos.x >= tabStart && ImGui::GetIO().MousePos.x <= tabEnd;
        const bool overSearch = metrics.showSearch &&
            ImGui::GetIO().MousePos.x >= drawPos.x + metrics.searchX &&
            ImGui::GetIO().MousePos.x <= drawPos.x + metrics.searchX + metrics.searchWidth;
        const float controlsStart = metrics.showSearch
            ? drawPos.x + size.x - S(26.0f) - S(235.0f)
            : drawPos.x + size.x - S(132.0f);
        const bool overControls = ImGui::GetIO().MousePos.x >= controlsStart;
        if (!overTabs && !overSearch && !overControls) {
            state_.dragging = true;
            state_.dragOffset = ImVec2(ImGui::GetIO().MousePos.x - state_.windowPos.x, ImGui::GetIO().MousePos.y - state_.windowPos.y);
        }
    }
    if (state_.dragging) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            state_.windowPos = ImVec2(ImGui::GetIO().MousePos.x - state_.dragOffset.x, ImGui::GetIO().MousePos.y - state_.dragOffset.y);
        } else {
            state_.dragging = false;
        }
    }

    ImGui::SetNextWindowPos(drawPos);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, state_.menuAnim);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::Begin("##neo_menu", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground);

    setGlobalAlpha(state_.menuAnim * state_.ui.opacity);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawWindowBackdrop(drawList, drawPos, ImVec2(drawPos.x + size.x, drawPos.y + size.y), S(state_.ui.cornerRadius), 1.0f);
    drawList->AddRectFilledMultiColor(
        ImVec2(drawPos.x + S(18.0f), drawPos.y + S(1.0f)),
        ImVec2(drawPos.x + size.x - S(18.0f), drawPos.y + S(2.2f)),
        F(withAlpha(palette.accent, 0.55f)), F(withAlpha(palette.accent2, 0.42f)),
        F(withAlpha(palette.accent2, 0.42f)), F(withAlpha(palette.accent, 0.55f)));

    drawHeader(modules, game, actions);
    drawSidebar(modules, game);
    if (state_.tab == Tab::Menu) {
        const Layout layout = makeLayout(ImGui::GetWindowPos(), ImGui::GetWindowSize());
        Rect content = layout.center;
        content.max.x = layout.right.max.x;
        drawMenuSettings(content);
    } else {
        drawModules(modules, game, actions);
        drawRightPanel(modules, game, actions);
    }
    drawToasts(dt);

    ImGui::End();
    setGlobalAlpha(1.0f);
    setTextScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void Gui::drawHeader(ModuleManager& modules, GameAccess& game, const UiActions& actions) {
    const Layout layout = makeLayout(ImGui::GetWindowPos(), ImGui::GetWindowSize());
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 pos = layout.pos;

    drawLogo(drawList, ImVec2(pos.x + layout.pad, pos.y + S(20.0f)), S(32.0f));
    gradientText(drawList, fonts().semibold, fs(fonts().semibold), ImVec2(pos.x + layout.pad + S(44.0f), pos.y + S(17.0f)),
        F(palette.text), F(mixColor(palette.text, palette.accent2, 0.45f)), "NEO");
    letterSpacedText(drawList, fonts().tiny, fs(fonts().tiny),
        ImVec2(pos.x + layout.pad + S(45.0f), pos.y + S(41.0f)), S(1.25f),
        F(withAlpha(palette.textFaint, 0.9f)), "UTILITY CLIENT");

    const HeaderMetrics metrics = headerMetrics(layout.size.x);
    const ImVec2 tabOrigin(pos.x + metrics.tabOriginX, pos.y + S(19.0f));
    const float tabWidth = metrics.tabWidth;
    const float tabHeight = S(35.0f);
    const float tabGap = metrics.tabGap;
    ImFont* tabFont = tabWidth >= S(76.0f) ? fonts().medium : fonts().small;
    const float tabFontSize = fs(tabFont);

    float targetIndicatorX = tabOrigin.x;
    float targetIndicatorW = tabWidth;
    const Tab tabs[kTabCount] = {Tab::Exploit, Tab::Misc, Tab::Player, Tab::World, Tab::Menu};

    for (int i = 0; i < kTabCount; ++i) {
        const Tab tab = tabs[i];
        const Rect tabRect(ImVec2(tabOrigin.x + i * (tabWidth + tabGap), tabOrigin.y),
            ImVec2(tabOrigin.x + i * (tabWidth + tabGap) + tabWidth, tabOrigin.y + tabHeight));
        const bool active = state_.tab == tab;
        const bool hovered = mouseHovered(tabRect);

        if (active) {
            targetIndicatorX = tabRect.min.x;
            targetIndicatorW = tabRect.size().x;
        }

        if (hovered && !active) {
            drawList->AddRectFilled(tabRect.min, tabRect.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.04f)), S(9.0f));
        }
        if (active) {
            drawList->AddRectFilled(tabRect.min, tabRect.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.075f)), S(9.0f));
            drawList->AddRect(tabRect.min, tabRect.max, F(withAlpha(palette.accent, 0.16f)), S(9.0f));
        }

        const ImVec2 labelSize = tabFont->CalcTextSizeA(tabFontSize, FLT_MAX, 0.0f, tabName(tab));
        text(drawList, tabFont, tabFontSize,
            ImVec2(tabRect.center().x - labelSize.x * 0.5f, tabRect.center().y - labelSize.y * 0.5f),
            active ? F(palette.text) : (hovered ? F(palette.textDim) : F(withAlpha(palette.textDim, 0.65f))),
            tabName(tab));

        if (mouseClicked(tabRect)) {
            state_.tab = tab;
            state_.selectedModule.clear();
            state_.search[0] = '\0';
            const auto groups = modules.groupsFor(tab);
            if (!groups.empty()) {
                state_.group = groups.front();
            }
        }
    }

    if (!state_.tabIndicatorInit) {
        state_.tabIndicatorX = targetIndicatorX;
        state_.tabIndicatorW = targetIndicatorW;
        state_.tabIndicatorInit = true;
    }
    state_.tabIndicatorX = approach(state_.tabIndicatorX, targetIndicatorX, 20.0f, ImGui::GetIO().DeltaTime);
    state_.tabIndicatorW = approach(state_.tabIndicatorW, targetIndicatorW, 20.0f, ImGui::GetIO().DeltaTime);
    drawList->AddRectFilledMultiColor(
        ImVec2(state_.tabIndicatorX + S(7.0f), tabOrigin.y + tabHeight - S(2.0f)),
        ImVec2(state_.tabIndicatorX + state_.tabIndicatorW - S(7.0f), tabOrigin.y + tabHeight),
        F(palette.accent), F(palette.accent2), F(palette.accent2), F(palette.accent));

    const float right = pos.x + layout.size.x - layout.pad;
    const float closeX = right - S(28.0f);
    const float keybindX = closeX - S(8.0f) - S(58.0f);
    const float searchX = pos.x + metrics.searchX;
    const float searchW = metrics.searchWidth;

    if (metrics.showSearch) {
        const Rect searchRect(ImVec2(searchX, pos.y + S(20.0f)), ImVec2(searchX + searchW, pos.y + S(54.0f)));
        drawList->AddRectFilled(searchRect.min, searchRect.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.045f)), S(9.0f));
        drawList->AddRect(searchRect.min, searchRect.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.07f)), S(9.0f));
        drawIcon(drawList, Icon::Search, ImVec2(searchRect.min.x + S(15.0f), searchRect.center().y), S(13.0f), F(palette.textFaint), S(1.35f));

        ImGui::PushFont(fonts().medium, fs(fonts().medium));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, S(0.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, toVec4(palette.text));
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, toVec4(withAlpha(palette.textFaint, 0.9f)));
        ImGui::SetCursorScreenPos(ImVec2(searchRect.min.x + S(31.0f), searchRect.center().y - fs(fonts().medium) * 0.5f - S(1.0f)));
        ImGui::SetNextItemWidth(searchW - S(54.0f));
        ImGui::InputTextWithHint("##module_search", "Search modules", state_.search, IM_ARRAYSIZE(state_.search),
            ImGuiInputTextFlags_NoHorizontalScroll);
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);
        ImGui::PopFont();

        if (state_.search[0] != '\0') {
            const Rect clearRect(ImVec2(searchRect.max.x - S(25.0f), searchRect.center().y - S(9.0f)),
                ImVec2(searchRect.max.x - S(7.0f), searchRect.center().y + S(9.0f)));
            if (mouseClicked(clearRect)) {
                state_.search[0] = '\0';
            }
            drawIcon(drawList, Icon::Close, clearRect.center(), S(10.0f), F(palette.textFaint), S(1.2f));
        }
    }

    const Rect keybindRect(ImVec2(keybindX, pos.y + S(24.0f)), ImVec2(keybindX + S(58.0f), pos.y + S(50.0f)));
    const bool menuCapturing = state_.captureId == "menu";
    const std::string menuKeyName = menuCapturing ? "..." : keys::name(state_.menuKey);
    drawKeycap(drawList, keybindRect, menuKeyName.c_str(), mouseHovered(keybindRect), menuCapturing, fonts().small, fs(fonts().small));
    if (mouseHovered(keybindRect)) {
        drawTooltip(menuCapturing ? "Press any key for the menu. Esc cancels, Delete clears."
                                  : "Menu keybind - click to change");
    }
    if (mouseClicked(keybindRect)) {
        state_.captureId = "menu";
    }

    const Rect closeRect(ImVec2(closeX, pos.y + S(21.0f)), ImVec2(closeX + S(28.0f), pos.y + S(49.0f)));
    const bool closeHovered = mouseHovered(closeRect);
    if (closeHovered) {
        drawList->AddCircleFilled(closeRect.center(), S(14.0f), F(withAlpha(palette.danger, 0.12f)), 20);
    }
    drawIcon(drawList, Icon::Close, closeRect.center(), S(13.0f), closeHovered ? F(palette.danger) : F(palette.textDim), S(1.35f));
    if (mouseClicked(closeRect)) {
        if (actions.closeMenu) {
            actions.closeMenu();
        }
    }

    drawList->AddLine(ImVec2(pos.x + layout.pad, pos.y + layout.headerHeight), ImVec2(pos.x + layout.size.x - layout.pad, pos.y + layout.headerHeight), F(palette.borderSoft), S(1.0f));
}

void Gui::drawSidebar(ModuleManager& modules, GameAccess& game) {
    const Layout layout = makeLayout(ImGui::GetWindowPos(), ImGui::GetWindowSize());
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const Rect& sidebar = layout.sidebar;

    const auto groups = modules.groupsFor(state_.tab);
    if (!groups.empty()) {
        drawSectionLabel(drawList, ImVec2(sidebar.min.x + S(2.0f), sidebar.min.y), "MODULES");
        if (std::find(groups.begin(), groups.end(), state_.group) == groups.end()) {
            state_.group = groups.front();
        }
    }

    float y = groups.empty() ? sidebar.min.y - S(16.0f) : sidebar.min.y + S(24.0f);
    for (Group group : groups) {
        const auto groupModules = modules.modulesFor(state_.tab, group);
        const Rect row(ImVec2(sidebar.min.x, y), ImVec2(sidebar.max.x, y + S(46.0f)));
        const bool active = state_.group == group;
        const bool hovered = mouseHovered(row);

        drawList->AddRectFilled(row.min, row.max,
            F(active ? withAlpha(palette.accent, 0.09f) : (hovered ? withAlpha(IM_COL32(255, 255, 255, 255), 0.05f) : withAlpha(IM_COL32(255, 255, 255, 255), 0.025f))),
            S(11.0f));
        drawList->AddRect(row.min, row.max,
            F(active ? withAlpha(palette.accent, 0.20f) : withAlpha(IM_COL32(255, 255, 255, 255), 0.04f)),
            S(11.0f));
        if (active) {
            drawList->AddRectFilled(ImVec2(row.min.x, row.min.y + S(11.0f)), ImVec2(row.min.x + S(2.5f), row.max.y - S(11.0f)),
                F(palette.accent), S(1.5f));
        }

        const ImVec2 iconCenter(row.min.x + S(25.0f), row.center().y);
        drawList->AddCircleFilled(iconCenter, S(15.0f), F(withAlpha(active ? palette.accent : palette.textDim, active ? 0.12f : 0.06f)), 20);
        drawIcon(drawList, groupIcon(group), iconCenter, S(16.0f), active ? F(palette.accent) : F(palette.textDim), S(1.35f));

        text(drawList, active ? fonts().medium : fonts().regular, fs(fonts().regular),
            ImVec2(row.min.x + S(48.0f), row.center().y - fs(fonts().regular) * 0.5f),
            active ? F(palette.text) : F(palette.textDim), groupName(group));

        char count[16]{};
        std::snprintf(count, sizeof(count), "%d", static_cast<int>(groupModules.size()));
        const float countWidth = fonts().small->CalcTextSizeA(fs(fonts().small), FLT_MAX, 0.0f, count).x;
        text(drawList, fonts().small, fs(fonts().small),
            ImVec2(row.max.x - S(14.0f) - countWidth, row.center().y - fs(fonts().small) * 0.5f),
            F(withAlpha(active ? palette.accent : palette.textFaint, 0.9f)), count);

        if (mouseClicked(row)) {
            state_.group = group;
            state_.selectedModule.clear();
            state_.search[0] = '\0';
        }

        y += S(52.0f);
    }

    const Rect shortcuts(ImVec2(sidebar.min.x, y + S(16.0f)), ImVec2(sidebar.max.x, y + S(110.0f)));
    drawList->AddRectFilled(shortcuts.min, shortcuts.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.03f)), S(12.0f));
    drawList->AddRect(shortcuts.min, shortcuts.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.055f)), S(12.0f));
    drawCardSheen(drawList, shortcuts, 0.35f);
    drawSectionLabel(drawList, ImVec2(shortcuts.min.x + S(14.0f), shortcuts.min.y + S(12.0f)), "SHORTCUTS");

    const auto shortcut = [&](float top, const char* label, const char* key) {
        const Rect row(ImVec2(shortcuts.min.x + S(14.0f), top), ImVec2(shortcuts.max.x - S(14.0f), top + S(22.0f)));
        text(drawList, fonts().small, fs(fonts().small),
            ImVec2(row.min.x, row.center().y - fs(fonts().small) * 0.5f), F(palette.textDim), label);
        const float keyWidth = std::max(S(46.0f), measure(fonts().small, fs(fonts().small), key) + S(12.0f));
        const Rect keyRect(ImVec2(row.max.x - keyWidth, row.min.y), row.max);
        drawKeycap(drawList, keyRect, key, false, false, fonts().small, fs(fonts().small));
    };
    const std::string menuKeyName = keys::name(state_.menuKey);
    shortcut(shortcuts.min.y + S(32.0f), "Open menu", menuKeyName.c_str());
    shortcut(shortcuts.min.y + S(60.0f), "Close", "Esc");

    const ImVec2 sessionMin(sidebar.min.x, sidebar.max.y - S(100.0f));
    const Rect session(sessionMin, ImVec2(sidebar.max.x, sidebar.max.y));
    drawList->AddRectFilled(session.min, session.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.035f)), S(12.0f));
    drawList->AddRect(session.min, session.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.06f)), S(12.0f));
    drawList->AddRectFilledMultiColor(ImVec2(session.min.x + S(14.0f), session.min.y + S(1.0f)), ImVec2(session.max.x - S(14.0f), session.min.y + S(2.0f)),
        F(withAlpha(palette.accent3, 0.55f)), F(withAlpha(palette.accent, 0.35f)), F(withAlpha(palette.accent, 0.35f)), F(withAlpha(palette.accent3, 0.55f)));

    drawSectionLabel(drawList, ImVec2(session.min.x + S(14.0f), session.min.y + S(13.0f)), "SESSION");
    drawList->AddCircleFilled(ImVec2(session.min.x + S(18.0f), session.min.y + S(42.0f)), S(3.5f),
        F(game.snapshot().available ? palette.success : palette.textFaint), 10);
    text(drawList, fonts().medium, fs(fonts().medium), ImVec2(session.min.x + S(28.0f), session.min.y + S(34.0f)),
        F(palette.text), game.snapshot().singleplayer ? "Singleplayer" : "Minecraft");
    drawChip(drawList, ImVec2(session.min.x + S(14.0f), session.min.y + S(60.0f)), "26.3",
        F(palette.textDim), F(withAlpha(IM_COL32(255, 255, 255, 255), 0.06f)), fonts().small, fs(fonts().small), S(20.0f));
    char fps[32]{};
    std::snprintf(fps, sizeof(fps), "%d FPS", static_cast<int>(fpsSmooth_));
    drawChip(drawList, ImVec2(session.min.x + S(61.0f), session.min.y + S(60.0f)), fps,
        F(withAlpha(palette.accent3, 0.85f)), F(withAlpha(palette.accent3, 0.07f)), fonts().small, fs(fonts().small), S(20.0f));
}

float Gui::moduleRowHeight(const Module& module) const {
    (void)module;
    return S(56.0f);
}

std::vector<Module*> Gui::visibleModules(ModuleManager& modules) const {
    if (state_.search[0] != '\0') {
        std::vector<Module*> result;
        for (auto& module : modules.modules()) {
            if (module->tab() == state_.tab && matches(*module, state_.search)) {
                result.push_back(module.get());
            }
        }
        return result;
    }
    return modules.modulesFor(state_.tab, state_.group);
}

void Gui::drawModules(ModuleManager& modules, GameAccess& game, const UiActions& actions) {
    const Layout layout = makeLayout(ImGui::GetWindowPos(), ImGui::GetWindowSize());
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const Rect& center = layout.center;

    const auto visible = visibleModules(modules);
    const bool searching = state_.search[0] != '\0';
    const char* title = searching ? "Search" : groupName(state_.group);
    const char* description = searching
        ? "Matching modules in this tab."
        : (state_.group == Group::Movement ? "Move with intent. Every action stays smooth." :
           state_.group == Group::Visuals ? "A cleaner view of the world." :
           state_.group == Group::Utility ? "Small tools, quietly useful." :
           "Shape the world around you.");

    const float titleWidth = measure(fonts().title, fs(fonts().title), title);

    char countLabel[32]{};
    std::snprintf(countLabel, sizeof(countLabel), "%d %s", static_cast<int>(visible.size()), visible.size() == 1 ? "module" : "modules");
    drawChip(drawList, ImVec2(center.min.x + titleWidth + S(12.0f), center.min.y + S(6.0f)), countLabel,
        F(palette.textDim), F(withAlpha(IM_COL32(255, 255, 255, 255), 0.045f)), fonts().small, fs(fonts().small), S(21.0f));

    text(drawList, fonts().small, fs(fonts().small), ImVec2(center.min.x, center.min.y + S(34.0f)), F(palette.textFaint), description);

    gradientText(drawList, fonts().title, fs(fonts().title), ImVec2(center.min.x, center.min.y),
        F(mixColor(palette.text, palette.accent, 0.55f)),
        F(mixColor(palette.text, palette.accent2, 0.68f)), title);

    drawList->AddLine(ImVec2(center.min.x, center.min.y + S(54.0f)),
        ImVec2(center.max.x, center.min.y + S(54.0f)), F(palette.borderSoft), S(1.0f));

    const float listTop = center.min.y + S(66.0f);
    const float listHeight = center.max.y - listTop;

    ImGui::SetCursorScreenPos(ImVec2(center.min.x, listTop));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##module_list", ImVec2(center.size().x, listHeight), false, ImGuiWindowFlags_NoBackground);
    ImDrawList* listDraw = ImGui::GetWindowDrawList();

    if (visible.empty()) {
        drawEmptyState(listDraw, Rect(ImVec2(center.min.x, listTop), ImVec2(center.max.x, listTop + listHeight)),
            searching ? Icon::Search : groupIcon(state_.group),
            searching ? "No matches" : "Nothing here yet",
            searching ? "Try a different word." : "This space is ready for the next drop.");
    } else {
        for (Module* module : visible) {
            const float rowHeight = moduleRowHeight(*module);
            drawModuleRow(*module, center.size().x, game, actions);
            ImGui::Dummy(ImVec2(center.size().x, rowHeight + S(8.0f)));
            if (state_.selectedModule == module->id()) {
                // drawModuleSettings already advances the cursor to the card
                // bottom, so only the trailing gap is added here.
                drawModuleSettings(*module, center.size().x, game, actions);
                ImGui::Dummy(ImVec2(center.size().x, S(12.0f)));
                // Make sure a freshly opened settings card is scrolled fully into
                // view instead of being cut by the bottom of the list.
                if (state_.scrollSelected) {
                    ImGui::SetScrollHereY(1.0f);
                    state_.scrollSelected = false;
                }
            }
        }
        // Breathing room so the last row never touches the window edge.
        ImGui::Dummy(ImVec2(center.size().x, S(10.0f)));
    }

    // Slim scroll indicator so it is obvious when more content sits below the
    // fold instead of the list looking cut off at the window edge.
    const float scrollMax = ImGui::GetScrollMaxY();
    if (scrollMax > 0.0f) {
        const float trackTop = listTop + S(4.0f);
        const float trackBottom = center.max.y - S(4.0f);
        const float trackHeight = std::max(S(20.0f), trackBottom - trackTop);
        const float visibleRatio = clamp(ImGui::GetWindowHeight() / (ImGui::GetWindowHeight() + scrollMax), 0.0f, 1.0f);
        const float knobHeight = std::max(S(26.0f), trackHeight * visibleRatio);
        const float scrollT = clamp(ImGui::GetScrollY() / scrollMax, 0.0f, 1.0f);
        const float knobTop = trackTop + (trackHeight - knobHeight) * scrollT;
        const ImVec2 knobMin(center.max.x - S(4.0f), knobTop);
        const ImVec2 knobMax(center.max.x - S(1.0f), knobTop + knobHeight);
        listDraw->AddRectFilled(knobMin, knobMax, F(withAlpha(palette.textDim, 0.28f)), S(2.0f));
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void Gui::drawModuleRow(Module& module, float width, GameAccess& game, const UiActions& actions) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();

    const float rowHeight = moduleRowHeight(module);
    const Rect row(rowMin, ImVec2(rowMin.x + width, rowMin.y + rowHeight));
    const bool hovered = mouseHovered(row);

    module.toggleAnim = approach(module.toggleAnim, module.enabled() ? 1.0f : 0.0f, 12.0f, ImGui::GetIO().DeltaTime);
    module.hoverAnim = approach(module.hoverAnim, hovered ? 1.0f : 0.0f, 10.0f, ImGui::GetIO().DeltaTime);

    drawHoverBackground(drawList, row, hovered, false, module.enabled());

    const ImVec2 iconMin(rowMin.x + S(12.0f), rowMin.y + S(10.0f));
    const Rect iconRect(iconMin, ImVec2(iconMin.x + S(32.0f), iconMin.y + S(32.0f)));
    const ImU32 tint = moduleTint(static_cast<int>(std::hash<std::string>{}(module.id()) & 0x7FFFFFFF));
    drawList->AddRectFilled(iconRect.min, iconRect.max, F(withAlpha(tint, module.enabled() ? 0.12f : 0.06f)), S(8.0f));
    drawList->AddRect(iconRect.min, iconRect.max, F(withAlpha(tint, module.enabled() ? 0.20f : 0.08f)), S(8.0f));
    drawIcon(drawList, module.icon(), iconRect.center(), S(16.0f), module.enabled() ? F(tint) : F(palette.textDim), S(1.3f));

    const float nameX = iconRect.max.x + S(10.0f);
    const float nameSize = fs(fonts().medium);
    std::string name = module.name();
    const float nameMax = row.max.x - S(50.0f);
    while (!name.empty() && measure(fonts().medium, nameSize, name.c_str()) > nameMax - nameX) {
        name.pop_back();
    }
    if (name.size() != module.name().size() && !name.empty()) {
        name += "..";
    }
    text(drawList, fonts().medium, nameSize, ImVec2(nameX, rowMin.y + S(14.0f)),
        module.enabled() ? F(palette.text) : F(withAlpha(palette.text, 0.75f)), name.c_str());

    const float descSize = fs(fonts().small);
    text(drawList, fonts().small, descSize, ImVec2(nameX, rowMin.y + S(30.0f)),
        F(withAlpha(palette.textFaint, 0.8f)), module.description().c_str());

    const float toggleWidth = S(36.0f);
    const float toggleHeight = S(18.0f);
    const float rightEdge = row.max.x - S(14.0f);
    const float gearSize = S(18.0f);

    // Right side layout (left → right): keycap, gear, toggle. All three are
    // separate hit targets so binding/settings never toggle the module.
    const ImVec2 toggleMin(rightEdge - toggleWidth, rowMin.y + (rowHeight - toggleHeight) * 0.5f);
    const Rect toggleRect(toggleMin, ImVec2(toggleMin.x + toggleWidth, toggleMin.y + toggleHeight));

    const Rect gearRect(
        ImVec2(toggleMin.x - S(6.0f) - gearSize, rowMin.y + (rowHeight - gearSize) * 0.5f),
        ImVec2(toggleMin.x - S(6.0f), rowMin.y + (rowHeight - gearSize) * 0.5f + gearSize));

    const bool capturing = state_.captureId == "keybind:" + module.id();
    const std::string realName = keys::name(module.keybind());
    const std::string keyName = capturing ? "..." : realName;
    const float keyWidth = std::max(S(34.0f), measure(fonts().small, fs(fonts().small), realName.c_str()) + S(10.0f));
    const Rect keyRect(
        ImVec2(gearRect.min.x - S(6.0f) - keyWidth, rowMin.y + (rowHeight - S(20.0f)) * 0.5f),
        ImVec2(gearRect.min.x - S(6.0f), rowMin.y + (rowHeight - S(20.0f)) * 0.5f + S(20.0f)));

    drawKeycap(drawList, keyRect, keyName.c_str(), mouseHovered(keyRect), capturing, fonts().small, fs(fonts().small));
    if (mouseClicked(keyRect)) {
        state_.captureId = "keybind:" + module.id();
        state_.dirty = true;
    }

    const bool gearHovered = mouseHovered(gearRect);
    drawIcon(drawList, Icon::Settings, gearRect.center(), S(12.0f),
        gearHovered ? F(palette.text) : F(withAlpha(palette.textFaint, 0.7f)), S(1.2f));
    if (mouseClicked(gearRect)) {
        const bool opening = state_.selectedModule != module.id();
        state_.selectedModule = opening ? module.id() : "";
        state_.scrollSelected = opening;
        state_.dirty = true;
    }

    drawToggle(drawList, toggleRect, module.toggleAnim, mouseHovered(toggleRect));

    if (mouseClicked(row) && !mouseHovered(keyRect) && !mouseHovered(gearRect)) {
        handleModuleToggle(module, game, actions);
    }
}

bool Gui::mouseHoveredKeybind(Module& module, const Rect& row) const {
    const float toggleWidth = S(36.0f);
    const float gearSize = S(18.0f);
    const float rightEdge = row.max.x - S(14.0f);
    const float toggleMinX = rightEdge - toggleWidth;
    const float gearMinX = toggleMinX - S(6.0f) - gearSize;
    const float keyRight = gearMinX - S(6.0f);
    const std::string keyName = keys::name(module.keybind());
    const float keyWidth = std::max(S(34.0f), measure(fonts().small, fs(fonts().small), keyName.c_str()) + S(10.0f));
    const Rect keyRect(
        ImVec2(keyRight - keyWidth, row.min.y + (row.size().y - S(20.0f)) * 0.5f),
        ImVec2(keyRight, row.min.y + (row.size().y - S(20.0f)) * 0.5f + S(20.0f)));
    return mouseHovered(keyRect);
}

float Gui::moduleSettingsHeight(const Module& module) const {
    float height = S(28.0f);
    for (const auto& setting : module.settings()) {
        height += setting.type == Setting::Type::Slider ? S(44.0f) : S(34.0f);
    }
    return height;
}

void Gui::drawModuleSettings(Module& module, float width, GameAccess& game, const UiActions& actions) {
    (void)game; (void)actions;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const float height = moduleSettingsHeight(module);

    const Rect card(ImVec2(rowMin.x + S(16.0f), rowMin.y), ImVec2(rowMin.x + width - S(16.0f), rowMin.y + height));
    drawList->AddRectFilled(card.min, card.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.032f)), S(8.0f));
    drawList->AddRect(card.min, card.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.055f)), S(8.0f));

    float y = rowMin.y + S(14.0f);
    for (auto& setting : module.settings()) {
        if (setting.type == Setting::Type::Slider) {
            text(drawList, fonts().small, fs(fonts().small), ImVec2(rowMin.x + S(24.0f), y), F(palette.textDim), setting.label.c_str());
            const std::string value = formatValue(setting);
            const float valueWidth = measure(fonts().small, fs(fonts().small), value.c_str());
            text(drawList, fonts().small, fs(fonts().small),
                ImVec2(card.max.x - S(24.0f) - valueWidth, y), F(palette.text), value.c_str());

            const Rect sliderRect(ImVec2(rowMin.x + S(24.0f), y + S(18.0f)),
                ImVec2(card.max.x - S(24.0f), y + S(30.0f)));
            if (slider(setting.id.c_str(), sliderRect, setting.value, setting.minValue, setting.maxValue, setting.step, true)) {
                state_.dirty = true;
            }
            y += S(44.0f);
        } else if (setting.type == Setting::Type::Toggle) {
            text(drawList, fonts().small, fs(fonts().small), ImVec2(rowMin.x + S(24.0f), y + S(6.0f)), F(palette.textDim), setting.label.c_str());
            const Rect settingToggle(ImVec2(card.max.x - S(24.0f) - S(36.0f), y + S(2.0f)),
                ImVec2(card.max.x - S(24.0f), y + S(2.0f) + S(18.0f)));
            ImGuiID animId = ImGui::GetID((std::string("##setting_anim_") + setting.id).c_str());
            float* anim = ImGui::GetStateStorage()->GetFloatRef(animId, setting.toggleValue ? 1.0f : 0.0f);
            *anim = approach(*anim, setting.toggleValue ? 1.0f : 0.0f, 14.0f, ImGui::GetIO().DeltaTime);
            drawToggle(drawList, settingToggle, *anim, mouseHovered(settingToggle));
            if (mouseClicked(settingToggle)) {
                setting.toggleValue = !setting.toggleValue;
                state_.dirty = true;
            }
            y += S(34.0f);
        } else if (setting.type == Setting::Type::Keybind) {
            const std::string keyName = state_.captureId == "setting:" + module.id() + ":" + setting.id ? "..." : keys::name(setting.key);
            text(drawList, fonts().small, fs(fonts().small), ImVec2(rowMin.x + S(24.0f), y + S(6.0f)), F(palette.textDim), setting.label.c_str());
            const float keyWidth = std::max(S(34.0f), measure(fonts().small, fs(fonts().small), keyName.c_str()) + S(10.0f));
            const Rect keyRect(ImVec2(card.max.x - S(24.0f) - keyWidth, y + S(2.0f)),
                ImVec2(card.max.x - S(24.0f), y + S(2.0f) + S(22.0f)));
            const bool capturing = state_.captureId == "setting:" + module.id() + ":" + setting.id;
            drawKeycap(drawList, keyRect, keyName.c_str(), mouseHovered(keyRect), capturing, fonts().small, fs(fonts().small));
            if (mouseClicked(keyRect)) {
                state_.captureId = "setting:" + module.id() + ":" + setting.id;
                state_.dirty = true;
            }
            y += S(34.0f);
        }
    }

    // Advance the ImGui cursor past the card so the parent list reserves the
    // full height (previously the card was drawn but never laid out, which let
    // the scroll region cut it off).
    ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMin.y + height));
}

void Gui::selectModule(const std::string& id) {
    if (id.empty() || state_.selectedModule == id) {
        state_.selectedModule.clear();
        return;
    }

    state_.selectedModule = id;
    state_.dirty = true;
}

void Gui::handleModuleToggle(Module& module, GameAccess& game, const UiActions& actions) {
    module.toggle(game);
    toast(module.name(), module.enabled());
    state_.dirty = true;
    if (actions.save) {
        actions.save();
    }
}

std::string Gui::activeGroupName(ModuleManager& modules) const {
    return groupName(state_.group);
}

void Gui::drawRightPanel(ModuleManager& modules, GameAccess& game, const UiActions& actions) {
    const Layout layout = makeLayout(ImGui::GetWindowPos(), ImGui::GetWindowSize());
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const Rect& right = layout.right;

    auto active = modules.enabledModules();
    active.erase(std::remove_if(active.begin(), active.end(), [](Module* module) {
        return module->id() == "hud";
    }), active.end());

    // The right column is laid out bottom-up so the footer and the cards keep
    // their spacing when the window height changes; cards that no longer fit
    // are dropped instead of being clipped by the window edge.
    const float footerY = right.max.y - S(28.0f);
    const float labelGap = S(14.0f);
    const float labelOffset = S(20.0f);
    const float performanceHeight = S(160.0f);
    const float positionHeight = S(80.0f);
    const float minActiveHeight = S(120.0f);
    const float activeTop = right.min.y + S(22.0f);
    const float bottomLimit = footerY - S(12.0f);
    const float available = std::max(0.0f, bottomLimit - activeTop);
    const bool showPosition = available - (2.0f * (labelGap + labelOffset) + performanceHeight + positionHeight) >= minActiveHeight;
    const bool showPerformance = available - (labelGap + labelOffset + performanceHeight) >= minActiveHeight;

    float contentBottom = bottomLimit;
    float positionLabelY = 0.0f;
    float positionCardTop = 0.0f;
    if (showPosition) {
        positionCardTop = contentBottom - positionHeight;
        positionLabelY = positionCardTop - labelOffset;
        contentBottom = positionLabelY - labelGap;
    }

    float performanceLabelY = 0.0f;
    float performanceCardTop = 0.0f;
    if (showPerformance) {
        performanceCardTop = contentBottom - performanceHeight;
        performanceLabelY = performanceCardTop - labelOffset;
        contentBottom = performanceLabelY - labelGap;
    }

    drawSectionLabel(drawList, ImVec2(right.min.x + S(2.0f), right.min.y), "ACTIVE");
    char count[16]{};
    std::snprintf(count, sizeof(count), "%d", static_cast<int>(active.size()));
    const float countWidth = measure(fonts().small, fs(fonts().small), count);
    text(drawList, fonts().small, fs(fonts().small), ImVec2(right.max.x - countWidth, right.min.y - S(2.0f)), F(palette.textFaint), count);

    const Rect activeCard(ImVec2(right.min.x, activeTop), ImVec2(right.max.x, std::max(activeTop + S(60.0f), contentBottom)));
    drawList->AddRectFilled(activeCard.min, activeCard.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.032f)), S(12.0f));
    drawList->AddRect(activeCard.min, activeCard.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.055f)), S(12.0f));
    drawCardSheen(drawList, activeCard);

    if (active.empty()) {
        drawEmptyState(drawList, Rect(ImVec2(activeCard.min.x, activeCard.min.y + S(6.0f)), ImVec2(activeCard.max.x, activeCard.max.y - S(6.0f))),
            Icon::Power, "Nothing enabled", "Toggle a module to see it here.");
    } else {
        float y = activeCard.min.y + S(15.0f);
        int shown = 0;
        const int maxRows = std::max(1, static_cast<int>((activeCard.size().y - S(46.0f)) / S(32.0f)) + 1);
        const bool overflow = static_cast<int>(active.size()) > maxRows;
        const int rowLimit = overflow ? std::max(1, maxRows - 1) : maxRows;
        for (Module* module : active) {
            if (shown >= rowLimit) {
                char more[32]{};
                std::snprintf(more, sizeof(more), "+%d more", static_cast<int>(active.size()) - shown);
                text(drawList, fonts().small, fs(fonts().small), ImVec2(activeCard.min.x + S(16.0f), y + S(3.0f)), F(palette.textFaint), more);
                break;
            }

            const Rect row(ImVec2(activeCard.min.x + S(8.0f), y), ImVec2(activeCard.max.x - S(8.0f), y + S(31.0f)));
            const bool hovered = mouseHovered(row);
            if (hovered) {
                drawList->AddRectFilled(row.min, row.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.045f)), S(8.0f));
            }
            const ImU32 tint = moduleTint(static_cast<int>(std::hash<std::string>{}(module->id()) & 0x7FFFFFFF));
            drawList->AddCircleFilled(ImVec2(row.min.x + S(13.0f), row.center().y), S(3.0f), F(tint), 10);
            gradientText(drawList, fonts().medium, fs(fonts().medium), ImVec2(row.min.x + S(24.0f), row.center().y - fs(fonts().medium) * 0.5f),
                F(tint), F(palette.accent2), module->name().c_str());
            drawIcon(drawList, Icon::ChevronRight, ImVec2(row.max.x - S(16.0f), row.center().y), S(10.0f), F(withAlpha(palette.textFaint, 0.85f)), S(1.2f));
            if (mouseClicked(row)) {
                state_.tab = module->tab();
                state_.group = module->group();
                state_.search[0] = '\0';
                state_.selectedModule = module->id();
            }
            y += S(32.0f);
            ++shown;
        }
    }

    if (showPerformance) {
        drawSectionLabel(drawList, ImVec2(right.min.x + S(2.0f), performanceLabelY), "PERFORMANCE");
        const Rect performanceCard(ImVec2(right.min.x, performanceCardTop), ImVec2(right.max.x, performanceCardTop + performanceHeight));
        drawList->AddRectFilled(performanceCard.min, performanceCard.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.032f)), S(12.0f));
        drawList->AddRect(performanceCard.min, performanceCard.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.055f)), S(12.0f));
        drawCardSheen(drawList, performanceCard);

        char fps[16]{};
        std::snprintf(fps, sizeof(fps), "%d", static_cast<int>(std::round(fpsSmooth_)));
        gradientText(drawList, fonts().hero, fs(fonts().hero), ImVec2(performanceCard.min.x + S(14.0f), performanceCard.min.y + S(9.0f)),
            F(palette.accent), F(palette.accent2), fps);
        text(drawList, fonts().tiny, fs(fonts().tiny), ImVec2(performanceCard.min.x + S(16.0f), performanceCard.min.y + S(49.0f)),
            F(palette.textFaint), "FRAMES PER SECOND");

        const Rect sparkRect(ImVec2(performanceCard.min.x + S(110.0f), performanceCard.min.y + S(16.0f)),
            ImVec2(performanceCard.max.x - S(14.0f), performanceCard.min.y + S(65.0f)));
        if (state_.fpsHistory.size() >= 2) {
            drawSparkline(drawList, sparkRect, state_.fpsHistory.data(), static_cast<int>(state_.fpsHistory.size()),
                F(withAlpha(palette.accent3, 0.9f)), F(withAlpha(palette.accent3, 0.08f)));
        }

        char frame[48]{};
        std::snprintf(frame, sizeof(frame), "%.1f ms frame time", fpsSmooth_ > 0.0f ? 1000.0f / std::max(1.0f, fpsSmooth_) : 0.0f);
        text(drawList, fonts().small, fs(fonts().small), ImVec2(performanceCard.min.x + S(16.0f), performanceCard.min.y + S(78.0f)),
            F(palette.textDim), frame);
        drawList->AddCircleFilled(ImVec2(performanceCard.min.x + S(17.0f), performanceCard.min.y + S(105.0f)), S(3.0f), F(palette.success), 10);
        text(drawList, fonts().small, fs(fonts().small), ImVec2(performanceCard.min.x + S(27.0f), performanceCard.min.y + S(99.0f)),
            F(palette.textDim), game.snapshot().available ? "Linked to Minecraft" : "Waiting for Minecraft");
    }

    if (showPosition) {
        drawSectionLabel(drawList, ImVec2(right.min.x + S(2.0f), positionLabelY), "POSITION");
        const Rect positionCard(ImVec2(right.min.x, positionCardTop), ImVec2(right.max.x, positionCardTop + positionHeight));
        drawList->AddRectFilled(positionCard.min, positionCard.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.032f)), S(12.0f));
        drawList->AddRect(positionCard.min, positionCard.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.055f)), S(12.0f));
        drawCardSheen(drawList, positionCard);

        const char* labels[] = {"X", "Y", "Z"};
        const double values[] = {game.snapshot().x, game.snapshot().y, game.snapshot().z};
        for (int i = 0; i < 3; ++i) {
            const float x = positionCard.min.x + S(15.0f) + i * S(88.0f);
            text(drawList, fonts().tiny, fs(fonts().tiny), ImVec2(x, positionCard.min.y + S(12.0f)), F(palette.textFaint), labels[i]);
            char value[32]{};
            std::snprintf(value, sizeof(value), "%.1f", values[i]);
            text(drawList, fonts().medium, fs(fonts().medium), ImVec2(x, positionCard.min.y + S(28.0f)), F(palette.text), value);
        }
        drawChip(drawList, ImVec2(positionCard.min.x + S(15.0f), positionCard.min.y + S(54.0f)),
            game.snapshot().singleplayer ? "SINGLEPLAYER" : "MINECRAFT",
            F(withAlpha(palette.success, 0.85f)), F(withAlpha(palette.success, 0.07f)), fonts().small, fs(fonts().small), S(21.0f));
    }

    text(drawList, fonts().tiny, fs(fonts().tiny), ImVec2(right.min.x + S(2.0f), footerY + S(5.0f)),
        F(withAlpha(palette.textFaint, 0.75f)), "NEO 1.0.0  /  MC 26.3");

    const Rect unloadRect(ImVec2(right.max.x - S(26.0f), footerY), ImVec2(right.max.x, footerY + S(24.0f)));
    const bool unloadHovered = mouseHovered(unloadRect);
    drawIcon(drawList, Icon::Unload, unloadRect.center(), S(13.0f), unloadHovered ? F(palette.danger) : F(withAlpha(palette.textFaint, 0.8f)), S(1.3f));
    if (unloadHovered) {
        drawTooltip("Unload NeoClient");
    }
    if (mouseClicked(unloadRect)) {
        if (actions.unload) {
            actions.unload();
        }
    }
}

void Gui::drawMenuSettings(const Rect& content) {
    if (content.size().x <= 0.0f || content.size().y <= 0.0f) {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    UiSettings& ui = state_.ui;

    gradientText(drawList, fonts().title, fs(fonts().title), ImVec2(content.min.x, content.min.y),
        F(mixColor(palette.text, palette.accent, 0.55f)),
        F(mixColor(palette.text, palette.accent2, 0.68f)), tabName(Tab::Menu));
    drawChip(drawList, ImVec2(content.min.x + measure(fonts().title, fs(fonts().title), tabName(Tab::Menu)) + S(12.0f), content.min.y + S(6.0f)),
        "UI", F(palette.textDim), F(withAlpha(IM_COL32(255, 255, 255, 255), 0.045f)), fonts().small, fs(fonts().small), S(21.0f));
    text(drawList, fonts().small, fs(fonts().small), ImVec2(content.min.x, content.min.y + S(34.0f)),
        F(palette.textFaint), "Font size, window, transparency and motion.");

    drawList->AddLine(ImVec2(content.min.x, content.min.y + S(54.0f)),
        ImVec2(content.max.x, content.min.y + S(54.0f)), F(palette.borderSoft), S(1.0f));

    const float listTop = content.min.y + S(66.0f);
    ImGui::SetCursorScreenPos(ImVec2(content.min.x, listTop));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##menu_settings", ImVec2(content.size().x, content.max.y - listTop), false, ImGuiWindowFlags_NoBackground);
    drawList = ImGui::GetWindowDrawList();

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = content.size().x;
    const float colGap = S(16.0f);
    const float colW = (width - colGap) * 0.5f;
    const float leftX = origin.x;
    const float rightX = origin.x + colW + colGap;

    const auto card = [&](const Rect& rect, const char* label) {
        drawList->AddRectFilled(rect.min, rect.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.032f)), S(12.0f));
        drawList->AddRect(rect.min, rect.max, F(withAlpha(IM_COL32(255, 255, 255, 255), 0.055f)), S(12.0f));
        drawSectionLabel(drawList, ImVec2(rect.min.x + S(16.0f), rect.min.y + S(13.0f)), label);
    };

    const auto formatPercent = [](float value) {
        char buffer[16]{};
        std::snprintf(buffer, sizeof(buffer), "%d%%", static_cast<int>(std::round(value * 100.0f)));
        return std::string(buffer);
    };
    const auto formatPixels = [](float value) {
        char buffer[16]{};
        std::snprintf(buffer, sizeof(buffer), "%d px", static_cast<int>(std::round(value)));
        return std::string(buffer);
    };

    const auto sliderRow = [&](float x, float y, float rowWidth, const char* id, const char* label,
                               float& value, float minValue, float maxValue, float step, const std::string& valueText) {
        text(drawList, fonts().small, fs(fonts().small), ImVec2(x, y), F(palette.textDim), label);
        const float valueWidth = measure(fonts().small, fs(fonts().small), valueText.c_str());
        text(drawList, fonts().small, fs(fonts().small), ImVec2(x + rowWidth - valueWidth, y), F(palette.text), valueText.c_str());
        const Rect sliderRect(ImVec2(x, y + S(20.0f)), ImVec2(x + rowWidth, y + S(32.0f)));
        if (slider(id, sliderRect, value, minValue, maxValue, step, true)) {
            state_.dirty = true;
        }
    };

    // Window size sliders are committed on release: resizing the window while
    // dragging would change the slider's own width and make it oscillate.
    const auto sizeSliderRow = [&](float x, float y, float rowWidth, const char* id, const char* stageId,
                                   const char* label, float& value, float minValue, float maxValue) {
        float* staged = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID(stageId), value);
        int* dragging = ImGui::GetStateStorage()->GetIntRef(ImGui::GetID(stageId) + 1u, 0);
        if (*dragging == 0) {
            *staged = value;
        }

        text(drawList, fonts().small, fs(fonts().small), ImVec2(x, y), F(palette.textDim), label);
        const std::string valueText = formatPixels(*staged);
        const float valueWidth = measure(fonts().small, fs(fonts().small), valueText.c_str());
        text(drawList, fonts().small, fs(fonts().small), ImVec2(x + rowWidth - valueWidth, y), F(palette.text), valueText.c_str());

        const Rect sliderRect(ImVec2(x, y + S(20.0f)), ImVec2(x + rowWidth, y + S(32.0f)));
        slider(id, sliderRect, *staged, minValue, maxValue, 10.0f, true);

        const bool active = ImGui::IsItemActive();
        if (active) {
            *dragging = 1;
        } else if (*dragging != 0) {
            if (*staged != value) {
                value = *staged;
                state_.dirty = true;
            }
            *dragging = 0;
        }
    };

    const auto toggleRow = [&](float x, float y, float rowWidth, const char* label, bool& value, const char* id) {
        text(drawList, fonts().small, fs(fonts().small), ImVec2(x, y + S(4.0f)), F(palette.textDim), label);
        const Rect toggleRect(ImVec2(x + rowWidth - S(36.0f), y + S(2.0f)),
            ImVec2(x + rowWidth, y + S(20.0f)));
        const ImGuiID animId = ImGui::GetID(id);
        float* anim = ImGui::GetStateStorage()->GetFloatRef(animId, value ? 1.0f : 0.0f);
        *anim = approach(*anim, value ? 1.0f : 0.0f, 14.0f, ImGui::GetIO().DeltaTime);
        drawToggle(drawList, toggleRect, *anim, mouseHovered(toggleRect));
        if (mouseClicked(toggleRect)) {
            value = !value;
            state_.dirty = true;
        }
    };

    float leftY = origin.y;
    float rightY = origin.y;

    const Rect textCard(ImVec2(leftX, leftY), ImVec2(leftX + colW, leftY + S(92.0f)));
    card(textCard, "TEXT");
    sliderRow(leftX + S(16.0f), leftY + S(38.0f), colW - S(32.0f), "##ui_font_size", "Font size",
        ui.fontScale, kMinFontScale, kMaxFontScale, 0.01f, formatPercent(ui.fontScale));
    leftY = textCard.max.y + S(16.0f);

    const Rect windowCard(ImVec2(leftX, leftY), ImVec2(leftX + colW, leftY + S(180.0f)));
    card(windowCard, "WINDOW");
    float windowY = leftY + S(38.0f);
    sizeSliderRow(leftX + S(16.0f), windowY, colW - S(32.0f), "##ui_window_width", "##ui_window_width_stage",
        "Window width", ui.windowWidth, kMinWindowWidth, kMaxWindowWidth);
    windowY += S(44.0f);
    sizeSliderRow(leftX + S(16.0f), windowY, colW - S(32.0f), "##ui_window_height", "##ui_window_height_stage",
        "Window height", ui.windowHeight, kMinWindowHeight, kMaxWindowHeight);
    windowY += S(44.0f);
    sliderRow(leftX + S(16.0f), windowY, colW - S(32.0f), "##ui_corner_radius", "Corner radius",
        ui.cornerRadius, kMinCornerRadius, kMaxCornerRadius, 1.0f, formatPixels(ui.cornerRadius));
    leftY = windowCard.max.y;

    const Rect appearanceCard(ImVec2(rightX, rightY), ImVec2(rightX + colW, rightY + S(132.0f)));
    card(appearanceCard, "APPEARANCE");
    sliderRow(rightX + S(16.0f), rightY + S(38.0f), colW - S(32.0f), "##ui_opacity", "Opacity",
        ui.opacity, kMinOpacity, kMaxOpacity, 0.01f, formatPercent(ui.opacity));
    toggleRow(rightX + S(16.0f), rightY + S(86.0f), colW - S(32.0f), "Menu animations", ui.animations, "##ui_animations");
    rightY = appearanceCard.max.y + S(16.0f);

    const Rect actionsCard(ImVec2(rightX, rightY), ImVec2(rightX + colW, rightY + S(92.0f)));
    card(actionsCard, "ACTIONS");
    const Rect resetRect(ImVec2(rightX + S(16.0f), rightY + S(38.0f)), ImVec2(rightX + colW - S(16.0f), rightY + S(68.0f)));
    const bool resetHovered = mouseHovered(resetRect);
    drawList->AddRectFilled(resetRect.min, resetRect.max,
        F(withAlpha(palette.accent, resetHovered ? 0.18f : 0.10f)), S(9.0f));
    drawList->AddRect(resetRect.min, resetRect.max,
        F(withAlpha(palette.accent, resetHovered ? 0.42f : 0.24f)), S(9.0f));
    const char* resetLabel = "Reset to defaults";
    const ImVec2 resetTextSize = fonts().small->CalcTextSizeA(fs(fonts().small), FLT_MAX, 0.0f, resetLabel);
    text(drawList, fonts().small, fs(fonts().small),
        ImVec2(resetRect.center().x - resetTextSize.x * 0.5f, resetRect.center().y - resetTextSize.y * 0.5f),
        F(palette.text), resetLabel);
    if (mouseClicked(resetRect)) {
        state_.ui = UiSettings{};
        state_.windowPosInit = false;
        state_.dirty = true;
        toast("Menu settings reset", true);
    }
    rightY = actionsCard.max.y;

    ImGui::SetCursorScreenPos(ImVec2(origin.x, std::max(leftY, rightY)));
    ImGui::Dummy(ImVec2(width, S(8.0f)));

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void Gui::drawToasts(float dt) {
    if (state_.toasts.empty()) {
        return;
    }

    const Layout layout = makeLayout(ImGui::GetWindowPos(), ImGui::GetWindowSize());
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    int index = 0;
    for (auto it = state_.toasts.rbegin(); it != state_.toasts.rend(); ++it) {
        const Toast& toast = *it;
        const float alpha = clamp(toast.life / 0.6f, 0.0f, 1.0f) * toast.anim;
        const float width = measure(fonts().small, fs(fonts().small), toast.text.c_str()) + S(64.0f);
        const float height = S(38.0f);
        const ImVec2 min(
            layout.center.center().x - width * 0.5f,
            layout.center.max.y - S(18.0f) - height - index * S(44.0f) + (1.0f - toast.anim) * S(10.0f));
        const Rect rect(min, ImVec2(min.x + width, min.y + height));

        const ImU32 fill = withAlpha(IM_COL32(12, 14, 18, 255), 0.92f * alpha);
        drawList->AddRectFilled(rect.min, rect.max, fill, height * 0.5f);
        drawList->AddRect(rect.min, rect.max, withAlpha(toast.enabled ? palette.accent3 : palette.textFaint, 0.20f * alpha), height * 0.5f);

        const ImVec2 iconCenter(rect.min.x + S(20.0f), rect.center().y);
        drawList->AddCircleFilled(iconCenter, S(10.0f), withAlpha(toast.enabled ? palette.accent3 : palette.textFaint, 0.12f * alpha), 18);
        drawIcon(drawList, toast.enabled ? Icon::Check : Icon::Power, iconCenter, S(10.0f),
            withAlpha(toast.enabled ? palette.accent3 : palette.textFaint, alpha), S(1.2f));

        text(drawList, fonts().small, fs(fonts().small),
            ImVec2(rect.min.x + S(37.0f), rect.center().y - fs(fonts().small) * 0.5f),
            withAlpha(palette.text, alpha), toast.text.c_str());

        ++index;
        if (index >= 3) {
            break;
        }
    }
}

void Gui::drawWorldMarkers(ModuleManager& modules, GameAccess& game) {
    const Module* xray = modules.find("xray");
    if (!xray || !xray->enabled()) {
        return;
    }
    const GameSnapshot& snap = game.snapshot();
    if (!snap.available || !snap.inGame) {
        return;
    }
    const std::vector<OreHit>& markers = game.oreMarkers();
    if (markers.empty()) {
        return;
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    if (display.x < 1.0f || display.y < 1.0f) {
        return;
    }

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    constexpr double kDeg = 0.017453292519943295;
    // Prefer the game's interpolated render camera (exact pose + effective FOV
    // incl. sprint/zoom). Falls back to the player pose when unavailable.
    const bool useCam = snap.hasCamera;
    const float yawDeg = useCam ? snap.camYaw : snap.yaw;
    const float pitchDeg = useCam ? snap.camPitch : snap.pitch;
    const double yawRad = yawDeg * kDeg;
    const float clampedPitch = std::clamp(pitchDeg, -89.9f, 89.9f);
    const double pitchRad = clampedPitch * kDeg;
    const double cosPitch = std::cos(pitchRad);
    // Forward vector, matching Minecraft's calculateViewVector(pitch, yaw):
    //   x = -sin(yaw) * cos(pitch), y = -sin(pitch), z = cos(yaw) * cos(pitch)
    const double fx = -std::sin(yawRad) * cosPitch;
    const double fy = -std::sin(pitchRad);
    const double fz = std::cos(yawRad) * cosPitch;
    // Right vector = normalize(forward × worldUp). Its continuous limit at the
    // poles is (-cos(yaw), 0, -sin(yaw)), so deriving it from yaw alone keeps
    // the basis valid when looking straight up/down (no mirror flip).
    const double rx = -std::cos(yawRad);
    const double rz = -std::sin(yawRad);
    // Up vector = right × forward.
    const double ux = -rz * fy;
    const double uy = rz * fx - rx * fz;
    const double uz = rx * fy;

    const double eyeX = useCam ? snap.camX : snap.x;
    const double eyeY = useCam ? snap.camY : snap.y + snap.eyeHeight;
    const double eyeZ = useCam ? snap.camZ : snap.z;

    const double effectiveFov = (useCam && snap.camFov > 1.0f) ? snap.camFov : static_cast<double>(snap.fov);
    const double tanY = std::tan(std::max(30.0, effectiveFov) * 0.5 * kDeg);
    const double tanX = tanY * (display.x / std::max(1.0f, display.y));

    const double w = static_cast<double>(display.x);
    const double h = static_cast<double>(display.y);

    auto project = [&](double wx, double wy, double wz, double cam[3]) {
        const double relX = wx - eyeX;
        const double relY = wy - eyeY;
        const double relZ = wz - eyeZ;
        cam[0] = relX * rx + relZ * rz;
        cam[1] = relX * ux + relY * uy + relZ * uz;
        cam[2] = relX * fx + relY * fy + relZ * fz;
    };

    constexpr double kNear = 0.2;
    const bool labels = xray->flag("labels");

    // --- Cluster connected ore blocks: same mineral, 6-neighbour adjacency ---
    const std::size_t count = markers.size();
    std::vector<int> parent(count);
    for (std::size_t i = 0; i < count; ++i) {
        parent[i] = static_cast<int>(i);
    }
    auto findRoot = [&parent](int x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };
    auto unite = [&parent, &findRoot](int a, int b) {
        a = findRoot(a);
        b = findRoot(b);
        if (a != b) {
            parent[b] = a;
        }
    };
    auto packPos = [](int x, int y, int z) -> std::uint64_t {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x) & 0x1FFFFFu) << 42) |
               (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y) & 0x1FFFFFu) << 21) |
               (static_cast<std::uint64_t>(static_cast<std::uint32_t>(z) & 0x1FFFFFu));
    };

    bool groupSelected[ores::groupCount] = {};
    for (int g = 0; g < ores::groupCount; ++g) {
        groupSelected[g] = xray->flag(ores::kGroups[g].settingId);
    }

    std::unordered_map<std::uint64_t, int> posIndex;
    posIndex.reserve(count * 2 + 16);
    std::vector<char> selected(count, 0);
    for (std::size_t i = 0; i < count; ++i) {
        const OreHit& hit = markers[i];
        if (hit.group < 0 || hit.group >= ores::groupCount || !groupSelected[hit.group]) {
            continue;
        }
        selected[i] = 1;
        posIndex[packPos(hit.x, hit.y, hit.z)] = static_cast<int>(i);
    }

    static const int kNeighborDirs[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };
    for (std::size_t i = 0; i < count; ++i) {
        if (!selected[i]) {
            continue;
        }
        const OreHit& hit = markers[i];
        for (const auto& dir : kNeighborDirs) {
            const auto it = posIndex.find(packPos(hit.x + dir[0], hit.y + dir[1], hit.z + dir[2]));
            if (it != posIndex.end() && markers[it->second].group == hit.group) {
                unite(static_cast<int>(i), it->second);
            }
        }
    }

    std::unordered_map<int, std::vector<int>> clusters;
    for (std::size_t i = 0; i < count; ++i) {
        if (selected[i]) {
            clusters[findRoot(static_cast<int>(i))].push_back(static_cast<int>(i));
        }
    }

    // Nearest clusters first so the closest groups win contours/labels.
    struct ClusterEntry {
        int root;
        double dist2;
    };
    std::vector<ClusterEntry> clusterOrder;
    clusterOrder.reserve(clusters.size());
    for (const auto& entry : clusters) {
        double best = 1e300;
        for (int idx : entry.second) {
            const OreHit& h = markers[idx];
            const double dx = h.x + 0.5 - eyeX;
            const double dy = h.y + 0.5 - eyeY;
            const double dz = h.z + 0.5 - eyeZ;
            const double d = dx * dx + dy * dy + dz * dz;
            if (d < best) {
                best = d;
            }
        }
        clusterOrder.push_back({entry.first, best});
    }
    std::sort(clusterOrder.begin(), clusterOrder.end(),
        [](const ClusterEntry& a, const ClusterEntry& b) { return a.dist2 < b.dist2; });

    std::vector<ImVec2> hull;
    std::vector<ImVec2> poly;
    std::vector<std::pair<int, ImVec2>> placedLabels;
    constexpr int kMaxClusters = 500;

    int drawnClusters = 0;
    for (const ClusterEntry& cluster : clusterOrder) {
        if (drawnClusters >= kMaxClusters) {
            break;
        }
        const std::vector<int>& members = clusters[cluster.root];
        if (members.empty()) {
            continue;
        }
        const int group = markers[members.front()].group;
        if (group < 0 || group >= ores::groupCount) {
            continue;
        }
        const ores::OreGroupDef& ore = ores::kGroups[group];

        hull.clear();
        bool any = false;
        double topX = 0.0;
        double topY = 1e18;
        const std::size_t memberLimit = std::min<std::size_t>(members.size(), 600);

        for (std::size_t mi = 0; mi < memberLimit; ++mi) {
            const OreHit& hit = markers[members[mi]];
            double corners[8][3];
            bool front = true;
            for (int c = 0; c < 8; ++c) {
                const double wx = hit.x + ((c & 1) ? 1.0 : 0.0);
                const double wy = hit.y + ((c & 2) ? 1.0 : 0.0);
                const double wz = hit.z + ((c & 4) ? 1.0 : 0.0);
                project(wx, wy, wz, corners[c]);
                if (corners[c][2] < kNear) {
                    front = false;
                    break;
                }
            }
            if (!front) {
                continue;
            }
            for (int c = 0; c < 8; ++c) {
                const double depth = corners[c][2];
                const double px = (0.5 + 0.5 * corners[c][0] / (depth * tanX)) * w;
                const double py = (0.5 - 0.5 * corners[c][1] / (depth * tanY)) * h;
                if (!std::isfinite(px) || !std::isfinite(py) ||
                    std::abs(px) > w * 3.0 || std::abs(py) > h * 3.0) {
                    front = false;
                    break;
                }
                hull.emplace_back(static_cast<float>(px), static_cast<float>(py));
                if (py < topY) {
                    topY = py;
                    topX = px;
                }
            }
            if (front) {
                any = true;
            }
        }

        if (!any || hull.size() < 3) {
            continue;
        }

        // Convex hull of the projected corners = one outline around the cluster.
        std::sort(hull.begin(), hull.end(), [](const ImVec2& a, const ImVec2& b) {
            return a.x < b.x || (a.x == b.x && a.y < b.y);
        });
        auto cross2 = [](const ImVec2& o, const ImVec2& a, const ImVec2& b) {
            return static_cast<double>(a.x - o.x) * (b.y - o.y) -
                   static_cast<double>(a.y - o.y) * (b.x - o.x);
        };
        poly.clear();
        poly.reserve(hull.size() * 2 + 2);
        for (const ImVec2& p : hull) {
            while (poly.size() >= 2 && cross2(poly[poly.size() - 2], poly.back(), p) <= 0.0) {
                poly.pop_back();
            }
            poly.push_back(p);
        }
        const std::size_t lowerSize = poly.size();
        for (std::size_t k = hull.size() - 1; k-- > 0;) {
            const ImVec2& p = hull[k];
            while (poly.size() > lowerSize && cross2(poly[poly.size() - 2], poly.back(), p) <= 0.0) {
                poly.pop_back();
            }
            poly.push_back(p);
        }
        if (poly.size() >= 2) {
            poly.pop_back();
        }
        if (poly.size() < 3) {
            continue;
        }

        const ImU32 solid = IM_COL32(ore.r, ore.g, ore.b, 235);
        const ImU32 fill = IM_COL32(ore.r, ore.g, ore.b, 36);

        drawList->AddConvexPolyFilled(poly.data(), static_cast<int>(poly.size()), fill);
        drawList->AddPolyline(poly.data(), static_cast<int>(poly.size()),
            IM_COL32(0, 0, 0, 150), ImDrawFlags_Closed, 3.0f);
        drawList->AddPolyline(poly.data(), static_cast<int>(poly.size()),
            solid, ImDrawFlags_Closed, 1.6f);

        ++drawnClusters;

        if (labels) {
            const float size = 12.0f;
            const float labelWidth = measure(fonts().hud, size, ore.label);
            float labelX = static_cast<float>(topX) - labelWidth * 0.5f;
            labelX = clamp(labelX, 2.0f, std::max(2.0f, display.x - labelWidth - 2.0f));
            float labelY = clamp(static_cast<float>(topY) - size - 4.0f, 2.0f,
                std::max(2.0f, display.y - size - 4.0f));

            bool tooClose = false;
            for (const auto& placed : placedLabels) {
                const float dx = placed.second.x - labelX;
                const float dy = placed.second.y - labelY;
                if (dx * dx + dy * dy < S(70.0f) * S(70.0f)) {
                    tooClose = true;
                    break;
                }
            }
            if (!tooClose && placedLabels.size() < 28) {
                placedLabels.emplace_back(group, ImVec2(labelX, labelY));
                drawList->AddRectFilled(
                    ImVec2(labelX - 4.0f, labelY - 2.0f),
                    ImVec2(labelX + labelWidth + 4.0f, labelY + size + 2.0f),
                    IM_COL32(0, 0, 0, 165), 3.0f);
                textShadow(drawList, fonts().hud, size, ImVec2(labelX, labelY), solid, ore.label);
            }
        }
    }
}
void Gui::drawHud(ModuleManager& modules, GameAccess& game, float dt) {
    const Module* hud = modules.find("hud");
    if (!hud || !hud->enabled() || !game.snapshot().available || !game.snapshot().inGame) {
        return;
    }

    setGlobalAlpha(1.0f - state_.menuAnim);
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 base(viewport->Pos.x + S(18.0f), viewport->Pos.y + S(18.0f));

    auto active = modules.enabledModules();
    active.erase(std::remove_if(active.begin(), active.end(), [](Module* module) {
        return module->id() == "hud";
    }), active.end());

    const bool showWatermark = hud->flag("watermark");
    const bool showModules = hud->flag("modules");
    const bool showCoords = hud->flag("coords");
    const bool showFps = hud->flag("fps");

    if (showWatermark) {
        float width = S(170.0f);
        if (showModules) {
            for (Module* module : active) {
                width = std::max(width, measure(fonts().small, fs(fonts().small), module->name().c_str()) + S(60.0f));
            }
        }
        const float height = S(48.0f) + (showModules ? S(10.0f) + active.size() * S(19.0f) : 0.0f) + S(10.0f);
        const Rect card(base, ImVec2(base.x + width, base.y + height));
        drawList->AddRectFilled(card.min + ImVec2(0.0f, S(2.0f)), card.max + ImVec2(0.0f, S(2.0f)), IM_COL32(0, 0, 0, 18), S(13.0f));
        drawList->AddRectFilled(card.min, card.max, withAlpha(IM_COL32(8, 10, 13, 255), 0.30f), S(13.0f));
        drawList->AddRect(card.min, card.max, withAlpha(IM_COL32(255, 255, 255, 255), 0.06f), S(13.0f));
        drawList->AddRectFilledMultiColor(ImVec2(card.min.x + S(14.0f), card.min.y + S(1.0f)), ImVec2(card.max.x - S(14.0f), card.min.y + S(2.0f)),
            withAlpha(palette.accent, 0.55f), withAlpha(palette.accent2, 0.40f), withAlpha(palette.accent2, 0.40f), withAlpha(palette.accent, 0.55f));

        drawLogo(drawList, ImVec2(card.min.x + S(12.0f), card.min.y + S(11.0f)), S(20.0f));
        gradientText(drawList, fonts().hudTitle, fs(fonts().hudTitle), ImVec2(card.min.x + S(39.0f), card.min.y + S(8.0f)),
            withAlpha(palette.text, 1.0f), withAlpha(palette.accent, 1.0f), "NEO");
        text(drawList, fonts().tiny, fs(fonts().tiny), ImVec2(card.min.x + S(40.0f), card.min.y + S(30.0f)),
            withAlpha(palette.textFaint, 0.9f), "MC 26.3");

        if (showModules) {
            float y = card.min.y + S(52.0f);
            if (active.empty()) {
                text(drawList, fonts().small, fs(fonts().small), ImVec2(card.min.x + S(14.0f), y),
                    withAlpha(palette.textFaint, 0.75f), "Ready");
            } else {
                for (Module* module : active) {
                    const ImU32 tint = moduleTint(static_cast<int>(std::hash<std::string>{}(module->id()) & 0x7FFFFFFF));
                    drawList->AddCircleFilled(ImVec2(card.min.x + S(17.0f), y + S(7.0f)), S(2.0f), tint, 8);
                    gradientText(drawList, fonts().hud, fs(fonts().hud), ImVec2(card.min.x + S(25.0f), y),
                        tint, withAlpha(palette.accent2, 0.95f), module->name().c_str());
                    y += S(19.0f);
                }
            }
        }
    }

    if (showCoords || showFps) {
        const ImVec2 bottomBase(viewport->Pos.x + S(18.0f), viewport->Pos.y + viewport->Size.y - S(18.0f));
        float x = bottomBase.x;
        if (showCoords) {
            char coordinates[96]{};
            std::snprintf(coordinates, sizeof(coordinates), "X %.1f   Y %.1f   Z %.1f",
                game.snapshot().x, game.snapshot().y, game.snapshot().z);
            const float width = measure(fonts().small, fs(fonts().small), coordinates) + S(24.0f);
            const Rect chip(ImVec2(x, bottomBase.y - S(25.0f)), ImVec2(x + width, bottomBase.y));
            drawList->AddRectFilled(chip.min, chip.max, withAlpha(IM_COL32(8, 10, 13, 255), 0.30f), S(8.0f));
            drawList->AddRect(chip.min, chip.max, withAlpha(IM_COL32(255, 255, 255, 255), 0.06f), S(8.0f));
            text(drawList, fonts().small, fs(fonts().small), ImVec2(chip.min.x + S(12.0f), chip.min.y + S(5.0f)),
                withAlpha(palette.textDim, 0.95f), coordinates);
            x = chip.max.x + S(8.0f);
        }
        if (showFps) {
            char fps[32]{};
            std::snprintf(fps, sizeof(fps), "%d FPS", static_cast<int>(std::round(fpsSmooth_)));
            const float width = measure(fonts().small, fs(fonts().small), fps) + S(24.0f);
            const Rect chip(ImVec2(x, bottomBase.y - S(25.0f)), ImVec2(x + width, bottomBase.y));
            drawList->AddRectFilled(chip.min, chip.max, withAlpha(IM_COL32(8, 10, 13, 255), 0.30f), S(8.0f));
            drawList->AddRect(chip.min, chip.max, withAlpha(IM_COL32(255, 255, 255, 255), 0.06f), S(8.0f));
            text(drawList, fonts().small, fs(fonts().small), ImVec2(chip.min.x + S(12.0f), chip.min.y + S(5.0f)),
                withAlpha(palette.accent3, 0.95f), fps);
        }
    }

    setGlobalAlpha(1.0f);
}

} // namespace neo::ui




