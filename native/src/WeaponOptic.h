#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace savr::weaponoptic {
constexpr bool Supported(int type) { return type == 34 || type == 36; }
constexpr int PhotoCamera = 43;
constexpr bool Calibratable(int type) { return Supported(type) || type == PhotoCamera; }
constexpr int FieldCount = 10;
// Tenths of centimetres/degrees, diameter/width, length, image flips.
struct Profile {
    int value[FieldCount]{0, 0, 0, 0, 0, 0, 60, 150, 1, 0};
};
inline Profile Defaults(int type) {
    Profile p;
    if (type == PhotoCamera) {
        p.value[0] = -60;
        p.value[6] = 100;
        return p;
    }
    p.value[1] = type == 34 ? 60 : 80; // above barrel
    p.value[0] = type == 34 ? -60 : -120; // behind model pivot
    p.value[6] = type == 34 ? 60 : 120;
    p.value[7] = type == 34 ? 150 : 60;
    return p;
}
inline int ClampField(int field, int value) {
    if (field < 0 || field >= FieldCount) return 0;
    if (field >= 8) return std::clamp(value, 0, 1);
    if (field == 6) return std::clamp(value, 20, 300);
    if (field == 7) return std::clamp(value, 10, 500);
    return std::clamp(value, field < 3 ? -1000 : -3600, field < 3 ? 1000 : 3600);
}
inline bool ParseProfile(const char* line,int& set,int& type,Profile& p) {
    const bool fine=std::strncmp(line,"optic2 ",7)==0;
    if(!fine && std::strncmp(line,"optic ",6)!=0) return false;
    p=Profile{};
    const int fields=std::sscanf(line+(fine?6:5)," %d %d %d %d %d %d %d %d %d %d %d %d",
        &set,&type,&p.value[0],&p.value[1],&p.value[2],&p.value[3],&p.value[4],
        &p.value[5],&p.value[6],&p.value[7],&p.value[8],&p.value[9]);
    if((fields!=10 && fields!=12) || set<0 || set>1 || !Calibratable(type)) return false;
    for(int i=0;i<FieldCount;++i) {
        if(!fine && i<8) p.value[i]=std::clamp(p.value[i],-10000,10000)*5;
        p.value[i]=ClampField(i,p.value[i]);
    }
    return true;
}
inline bool NearEye(float distance,float axial,float lateral,float halfSize,bool previous) {
    return std::isfinite(distance)&&std::isfinite(axial)&&std::isfinite(lateral)&&
        distance<(previous?.45f:.35f) && axial<.03f && lateral<halfSize+.12f;
}
constexpr float NormalFar=450.f, DetailedFar=1200.f;
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
