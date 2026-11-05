#pragma once

#include <algorithm>
#include <cmath>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace neo {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

template <typename T>
inline T clamp(T value, T low, T high) {
    return std::max(low, std::min(value, high));
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float approach(float current, float target, float speed, float dt) {
    const float factor = 1.0f - std::exp(-speed * dt);
    return current + (target - current) * factor;
}

inline float easeOutCubic(float t) {
    t = clamp(t, 0.0f, 1.0f);
    return 1.0f - std::pow(1.0f - t, 3.0f);
}

inline float easeInOutCubic(float t) {
    t = clamp(t, 0.0f, 1.0f);
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

inline bool keyPressedOnce(int vk) {
    return (GetAsyncKeyState(vk) & 0x0001) != 0;
}

} // namespace neo
