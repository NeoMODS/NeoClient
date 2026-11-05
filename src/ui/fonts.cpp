#include "ui/fonts.h"

#include "core/logger.h"
#include "core/paths.h"

#include <filesystem>
#include <string>

namespace neo::ui {
namespace {

Fonts g_fonts;

ImFont* loadOne(float size, const char* file, const ImWchar* ranges) {
    const auto path = paths::fontFile(file);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        NEO_LOG_WARN("Font not found: %s", path.string().c_str());
        return nullptr;
    }

    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 2;
    config.PixelSnapH = false;
    config.RasterizerMultiply = 1.02f;

    ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
        path.string().c_str(), size, &config, ranges);
    if (!font) {
        NEO_LOG_WARN("Could not load font: %s", path.string().c_str());
    }
    return font;
}

} // namespace

Fonts& fonts() {
    return g_fonts;
}

void loadFonts(float dpiScale) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const ImWchar* ranges = io.Fonts->GetGlyphRangesDefault();

    g_fonts.scale = dpiScale;
    g_fonts.regular = loadOne(15.0f * dpiScale, "Poppins-Regular.ttf", ranges);
    g_fonts.medium = loadOne(15.0f * dpiScale, "Poppins-Medium.ttf", ranges);
    g_fonts.semibold = loadOne(15.0f * dpiScale, "Poppins-SemiBold.ttf", ranges);
    g_fonts.small = loadOne(12.0f * dpiScale, "Poppins-Regular.ttf", ranges);
    g_fonts.tiny = loadOne(10.5f * dpiScale, "Poppins-Medium.ttf", ranges);
    g_fonts.title = loadOne(21.0f * dpiScale, "Poppins-SemiBold.ttf", ranges);
    g_fonts.hero = loadOne(27.0f * dpiScale, "Poppins-SemiBold.ttf", ranges);
    g_fonts.hud = loadOne(13.0f * dpiScale, "Poppins-Medium.ttf", ranges);
    g_fonts.hudTitle = loadOne(22.0f * dpiScale, "Poppins-SemiBold.ttf", ranges);

    if (!g_fonts.regular) {
        g_fonts.regular = io.Fonts->AddFontDefault();
    }
    if (!g_fonts.medium) g_fonts.medium = g_fonts.regular;
    if (!g_fonts.semibold) g_fonts.semibold = g_fonts.regular;
    if (!g_fonts.small) g_fonts.small = g_fonts.regular;
    if (!g_fonts.tiny) g_fonts.tiny = g_fonts.regular;
    if (!g_fonts.title) g_fonts.title = g_fonts.semibold;
    if (!g_fonts.hero) g_fonts.hero = g_fonts.semibold;
    if (!g_fonts.hud) g_fonts.hud = g_fonts.regular;
    if (!g_fonts.hudTitle) g_fonts.hudTitle = g_fonts.semibold;
}

} // namespace neo::ui
