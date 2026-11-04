#pragma once
#include <algorithm>
#include <cmath>

namespace savr::rear_mirror {
constexpr int Width=512, Height=160;
constexpr float Far=70.f, Near=.15f;
constexpr float HalfWidth=.16f, HalfHeight=.05f;
constexpr int MaxMap=256, MaxTraffic=32, MaxRequests=2, MaxCreates=2;

inline float TanX(int degrees) {
    return std::tan(std::clamp(degrees,40,120)*.00872664626f);
}
// Sphere versus the four side planes and near/far planes of the rear camera.
// Inputs use camera-local left, up, forward coordinates. Never reject a road
// merely because its centre is off screen while its bounds cross the frustum.
inline bool InView(float left,float up,float forward,float radius,float tanX) {
    if (!std::isfinite(left)||!std::isfinite(up)||!std::isfinite(forward)||
        !std::isfinite(radius)||radius<=0.f||radius>500.f||
        !std::isfinite(tanX)||tanX<=0.f) return false;
    const float tanY=tanX*Height/Width;
    return forward+radius>=Near && forward-radius<=Far &&
        std::fabs(left)<=forward*tanX+radius*std::sqrt(1.f+tanX*tanX) &&
        std::fabs(up)<=forward*tanY+radius*std::sqrt(1.f+tanY*tanY);
}
inline bool SmallProp(float radius,float distance) {
    return radius<1.5f && distance>15.f;
}
}
