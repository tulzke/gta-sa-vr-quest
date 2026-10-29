#pragma once

#include <algorithm>
#include <cmath>

namespace savr::bikesteering {

struct Vec3 { float x{}, y{}, z{}; };
constexpr float kRadians = 3.14159265358979323846f / 180.0f;

// Chords are measured in OpenXR LOCAL, whose +Y is gravity-up. No rendered
// bike/camera basis participates: model roll must never become new hand input.
inline bool HandRoll(Vec3 left, Vec3 right, float& angle) {
    const float x=right.x-left.x, y=right.y-left.y, z=right.z-left.z;
    if (!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)) return false;
    const float horizontal=std::hypot(x,z), span=std::hypot(horizontal,y);
    if (span<0.15f||span>1.5f||horizontal<0.10f) return false;
    angle=std::atan2(y,horizontal);
    return std::abs(angle)<=65.0f*kRadians;
}

inline float LeanWeight(float forwardSpeed) {
    if (!std::isfinite(forwardSpeed)) return 0.0f;
    // GTA velocity units; blend only when travelling forwards. Reverse and
    // parking retain handlebar steering. Smoothstep has no boundary slope jump.
    const float t=std::clamp((forwardSpeed-0.04f)/0.08f,0.0f,1.0f);
    return t*t*(3.0f-2.0f*t);
}

inline float LeanCommand(float angle, int sensitivityPercent) {
    if (!std::isfinite(angle)) return 0.0f;
    const float scaled=angle*std::clamp(sensitivityPercent,25,200)/100.0f;
    const float magnitude=std::clamp(
        (std::abs(scaled)-2.0f*kRadians)/(23.0f*kRadians),0.0f,1.0f);
    return std::copysign(magnitude,scaled);
}

struct Result { float steering{}, roll{}, weight{}; bool twoHands{}; };

struct State {
    bool referenceValid{}, clockValid{};
    float referenceRoll{}, previousRoll{}, output{}, authority{};
    double previousTime{};

    void Reset() { *this={}; }

    Result Update(Vec3 left, Vec3 right, bool twoHands, float forwardSpeed,
                  float handlebarSteering, int sensitivityPercent, double now) {
        handlebarSteering=std::isfinite(handlebarSteering)?
            std::clamp(handlebarSteering,-1.0f,1.0f):0.0f;
        if (!std::isfinite(now)) { Reset(); return {}; }
        float dt=0.0f;
        if (clockValid&&now>=previousTime&&now-previousTime<=0.25)
            dt=static_cast<float>(now-previousTime);
        else {
            // Tracking stalls and backwards clocks must not preserve a stale
            // neutral or integrate a long interval into an instant full turn.
            referenceValid=false; authority=0.0f; output=handlebarSteering;
        }
        clockValid=true; previousTime=now;
        float roll=0.0f;
        twoHands=twoHands&&HandRoll(left,right,roll);
        if (twoHands) {
            if (!referenceValid||std::abs(roll-previousRoll)>45.0f*kRadians) {
                referenceRoll=roll; referenceValid=true; authority=0.0f;
            }
            previousRoll=roll;
            authority=std::min(1.0f,authority+dt/0.18f);
        } else {
            referenceValid=false; authority=0.0f;
        }
        const float delta=twoHands?roll-referenceRoll:0.0f;
        const float weight=twoHands?LeanWeight(forwardSpeed)*authority:0.0f;
        const float target=(1.0f-weight)*handlebarSteering+
            weight*LeanCommand(delta,sensitivityPercent);
        // GTA also filters its steering query. Keep this short, and preserve
        // output across one/two-hand handovers instead of snapping on regrip.
        output+=(target-output)*(-std::expm1(-dt/0.05f));
        output=std::clamp(output,-1.0f,1.0f);
        return {output,delta,weight,twoHands};
    }
};

} // namespace savr::bikesteering
