#pragma once
#include <algorithm>
#include <cmath>

namespace savr::weaponoptic {
constexpr bool Supported(int type) { return type == 34 || type == 36; }
constexpr int FieldCount = 8;
// Half centimetres, half degrees, outer diameter/width and body length.
struct Profile {
    int value[FieldCount]{0, 0, 0, 0, 0, 0, 12, 30};
};
inline Profile Defaults(int type) {
    Profile p;
    p.value[1] = type == 34 ? 12 : 16; // above barrel
    p.value[0] = type == 34 ? -12 : -24; // behind model pivot
    p.value[6] = type == 34 ? 12 : 24;
    p.value[7] = type == 34 ? 30 : 12;
    return p;
}
inline int ClampField(int field, int value) {
    if (field < 0 || field >= FieldCount) return 0;
    if (field == 6) return std::clamp(value, 4, 60);
    if (field == 7) return std::clamp(value, 2, 100);
    return std::clamp(value, field < 3 ? -200 : -720, field < 3 ? 200 : 720);
}
inline float StepZoom(int type, float zoom, float axis, float seconds) {
    const float minimum = type == 34 ? 2.f : 1.f;
    const float maximum = type == 34 ? 8.f : 4.f;
    if (!std::isfinite(zoom)) zoom = minimum;
    if (!std::isfinite(axis) || !std::isfinite(seconds)) return std::clamp(zoom, minimum, maximum);
    const float rate = std::abs(axis) <= .25f ? 0.f
        : std::copysign((std::min(std::abs(axis), 1.f) - .25f) / .75f, axis);
    return std::clamp(zoom * std::exp(rate * std::clamp(seconds, 0.f, .1f) * 1.2f), minimum, maximum);
}
// Magnification is angular relative to the aperture as seen at a comfortable
// 10 cm eye relief, independent of headset FOV and render resolution.
inline float TanHalfFov(float halfWidth, float zoom) {
    return std::clamp(halfWidth / (.1f * std::max(1.f, zoom)), .012f, .8f);
}
constexpr float ZeroDistance = 100.f;
} // namespace savr::weaponoptic
