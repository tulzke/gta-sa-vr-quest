#pragma once
#include "VehicleDoorMotion.h"

namespace savr::vehicledoors {
inline Basis MirrorOrientation(const int* angles) {
    Basis b;b.forward={0,-1,0};
    const auto turn=[](Vec& a,Vec& b,int deg) {
        const float r=deg*.01745329252f,c=std::cos(r),s=std::sin(r);
        const Vec old=a;a=old*c+b*s;b=b*c-old*s;
    };
    turn(b.forward,b.up,angles[0]);turn(b.forward,b.right,angles[1]);turn(b.right,b.up,angles[2]);
    return b;
}
inline void MirrorAngles(const Basis& b,int* angles) {
    const float yaw=std::asin(std::clamp(b.forward.x,-1.f,1.f));
    const float pitch=std::atan2(b.forward.z,-b.forward.y);
    const Vec up{0,std::sin(pitch),std::cos(pitch)};
    const Vec right{std::cos(yaw),std::cos(pitch)*std::sin(yaw),-std::sin(pitch)*std::sin(yaw)};
    const float roll=std::atan2(Dot(b.right,up),Dot(b.right,right));
    angles[0]=std::lround(pitch*57.2957795f);angles[1]=std::lround(yaw*57.2957795f);angles[2]=std::lround(roll*57.2957795f);
}
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
