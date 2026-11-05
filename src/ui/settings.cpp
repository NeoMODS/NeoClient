#include "ui/settings.h"

#include "core/math.h"

namespace neo::ui {

void clampUiSettings(UiSettings& settings) {
    settings.fontScale = clamp(settings.fontScale, kMinFontScale, kMaxFontScale);
    settings.windowWidth = clamp(settings.windowWidth, kMinWindowWidth, kMaxWindowWidth);
    settings.windowHeight = clamp(settings.windowHeight, kMinWindowHeight, kMaxWindowHeight);
    settings.opacity = clamp(settings.opacity, kMinOpacity, kMaxOpacity);
    settings.cornerRadius = clamp(settings.cornerRadius, kMinCornerRadius, kMaxCornerRadius);
}

} // namespace neo::ui
