#pragma once

namespace neo::ui {

struct UiSettings {
    float fontScale = 1.0f;
    float windowWidth = 1080.0f;
    float windowHeight = 660.0f;
    float opacity = 1.0f;
    float cornerRadius = 18.0f;
    bool animations = true;
};

constexpr float kMinFontScale = 0.85f;
constexpr float kMaxFontScale = 1.30f;
constexpr float kMinWindowWidth = 980.0f;
constexpr float kMaxWindowWidth = 1500.0f;
constexpr float kMinWindowHeight = 560.0f;
constexpr float kMaxWindowHeight = 900.0f;
constexpr float kMinOpacity = 0.55f;
constexpr float kMaxOpacity = 1.0f;
constexpr float kMinCornerRadius = 6.0f;
constexpr float kMaxCornerRadius = 26.0f;

void clampUiSettings(UiSettings& settings);

} // namespace neo::ui
