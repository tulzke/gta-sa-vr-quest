#pragma once
#include <algorithm>
#include <cmath>

namespace savr::combatpolicy {
// A new stroke, rather than a timer, permits another full-damage hit.
struct Swing {
    bool spent{};
    float distance{};
    void Sample(float speed, float threshold, float travel, float directionCos) {
        if (!spent) return;
        distance += travel;
        if (speed < threshold * .4f || (distance >= .08f && directionCos < .15f))
            spent = false;
    }
    void Hit() { spent = true; distance = 0; }
};
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
