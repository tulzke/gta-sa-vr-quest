#pragma once
#include <algorithm>
#include <cmath>

namespace savr::combatpolicy {
// Every strike needs a continuous 40 cm stroke, not accumulated idle jitter.
struct Swing {
    bool spent{};
    float distance{};
    void Sample(float speed, float threshold, float travel, float directionCos) {
        if (!std::isfinite(speed) || !std::isfinite(travel) ||
            !std::isfinite(directionCos) || speed < threshold * .4f) {
            *this={};return;
        }
        if (directionCos < .15f) *this={};
        if (!spent) distance += travel;
    }
    bool Ready(float speed,float threshold) const {
        return !spent && speed>=threshold && distance+1.e-5f>=.40f;
    }
    void Hit() { spent = true; distance = 0; }
};
// Mirrored +X grip axis is the anatomical palm normal used by Basketball.
inline bool PalmInward(const float q[4],int hand,const float toHead[3],bool held) {
    const float norm=q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3];
    const float length=std::sqrt(toHead[0]*toHead[0]+toHead[1]*toHead[1]+toHead[2]*toHead[2]);
    if (!std::isfinite(norm) || norm<.5f || norm>1.5f || !std::isfinite(length) || length<.1f)
        return false;
    const float sign=hand==0?1.f:-1.f;
    const float x=1-2*(q[1]*q[1]+q[2]*q[2])/norm;
    const float y=2*(q[0]*q[1]+q[3]*q[2])/norm;
    const float z=2*(q[0]*q[2]-q[3]*q[1])/norm;
    const float alignment=sign*(x*toHead[0]+y*toHead[1]+z*toHead[2])/length;
    return alignment>=(held?.30f:.45f);
}
inline void LimitMovement(bool blocking,float* x,float* y) {
    if (!blocking) return;
    const float length=std::hypot(*x,*y);
    if (!std::isfinite(length)) { *x=*y=0;return; }
    constexpr float maximum=.35f;
    if (length>maximum) { *x*=maximum/length;*y*=maximum/length; }
}
inline bool GuardPose(float height, float reach, float forward, bool held) {
    return std::isfinite(height) && std::isfinite(reach) && std::isfinite(forward) &&
        height >= (held ? -.58f : -.48f) && height <= .25f &&
        reach >= .10f && reach <= (held ? .85f : .75f) &&
        forward >= (held ? .045f : .085f);
}
inline bool Covers(float facingCos, float guardCos, int piece) {
    return std::isfinite(facingCos) && std::isfinite(guardCos) &&
        facingCos > .05f && guardCos >= .55f && piece != 7 && piece != 8;
}
inline float DamageFraction(int incoming, bool weaponGuard) {
    if (incoming < 0 || incoming > 15) return 1;
    if (weaponGuard) return incoming == 9 ? .35f : 0.f;
    if (incoming <= 1) return 0;
    return incoming == 4 || incoming == 8 || incoming == 9 ? .65f : .4f;
}
inline float Fade(double now, double at, double duration) {
    if (at <= 0 || now < at || now >= at + duration) return 0;
    const float t = static_cast<float>((now - at) / duration);
    return (1-t)*(1-t);
}
}
