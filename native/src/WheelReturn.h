#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace savr::wheelreturn {

inline float TimeConstant(float forwardSpeed) {
    // CPhysical velocity is metres per stock 50 Hz tick: *50*3.6 = km/h.
    const float kmh=std::isfinite(forwardSpeed)?std::abs(forwardSpeed)*180.0f:0.0f;
    if (kmh<=20.0f) return 0.75f+(0.4f-0.75f)*(kmh/20.0f);
    return 0.4f+(0.175f-0.4f)*std::clamp((kmh-20.0f)/40.0f,0.0f,1.0f);
}

inline float ReleasedAngle(float angle,float forwardSpeed,float dt) {
    if (!std::isfinite(angle)) return 0.0f;
    if (!std::isfinite(dt)||dt<=0.0f||dt>0.25f) return angle;
    const float result=angle*std::exp(-dt/TimeConstant(forwardSpeed));
    constexpr float stopAngle=0.1f*3.14159265358979323846f/180.0f;
    return std::abs(result)<stopAngle?0.0f:result;
}

// Sample every input update, including blocked frames. Game time freezes on
// pause; duplicate samples, backwards clocks and stalls cannot catch up in a jump.
struct Clock {
    bool valid{};
    std::uint32_t previous{};
    float Step(std::uint32_t now) {
        const auto elapsed=now-previous;
        const float dt=valid&&now>=previous&&elapsed<=250?elapsed*0.001f:0.0f;
        previous=now; valid=true;
        return dt;
    }
};

} // namespace savr::wheelreturn
