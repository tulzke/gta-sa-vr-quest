#pragma once
#include <algorithm>
#include <cmath>

namespace savr::locomotion {
// Apply only to headset yaw, after left/right stick mapping. Preserve diagonal
// movement, vehicle axes and menu input. The vertical sector absorbs the small
// sideways deflection that accompanies zooming with a physical thumbstick.
inline float FilterTurnStickX(float x,float y) {
    if(!std::isfinite(x)||!std::isfinite(y)) return 0.f;
    constexpr float centre=.2f;
    const float magnitude=std::min(std::abs(x),1.f);
    if(magnitude<=centre || magnitude<=std::abs(y)*1.25f) return 0.f;
    return std::copysign((magnitude-centre)/(1.f-centre),x);
}
} // namespace savr::locomotion
