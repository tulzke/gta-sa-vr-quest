#pragma once
#include "VehicleDoorMotion.h"

namespace savr::vehicledoors {
// Absolute displacement in a rigid local frame avoids accumulating rounding
// error and ignores vehicle translation/rotation during a drag.
// Half-degree pitch delta from a vertical two-hand line in vehicle coordinates.
inline int WheelPitchDelta(Vec line,float start) {
    constexpr float pi=3.14159265358979323846f;
    const float angle=std::remainder(std::atan2(line.y,line.z)-start,2.f*pi);
    return -std::lround(angle*360.f/pi);
}
struct DebugDrag {
    int hand{-1};
    Vec start{},delta{};
    bool Begin(int h,Vec local) {
        if(h<0||h>1||!Finite(local)) return false;
        hand=h;start=local;delta={};return true;
    }
    bool Step(Vec local) {
        if(hand<0||!Finite(local)||Length(local-start-delta)>.35f||
           Length(local-start)>2.f) return false;
        delta=local-start;return true;
    }
};
}
