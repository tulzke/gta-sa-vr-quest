#pragma once

#include <algorithm>
#include <cmath>

namespace savr::ignition {

// Clockwise travel in degrees, measured from the vertical OFF position.
// The first 60 degrees snap OFF; the final 30 snap ON upon release.
inline bool ReleasedEngineOn(float angleDegrees) {
    return std::isfinite(angleDegrees) && angleDegrees >= 60.0f;
}

inline float ClampAngle(float angleDegrees) {
    return std::isfinite(angleDegrees) ? std::clamp(angleDegrees, 0.0f, 90.0f) : 0.0f;
}

enum class Hint { None, PointToKey, TurnClockwise };

// One hint opportunity per occupancy. The caller resets on leaving the
// vehicle, not when a menu opens or tracking is temporarily unavailable.
struct HintState {
    bool occupied{};
    bool completed{};
    double enteredAt{};

    void Leave() { *this = {}; }

    Hint Update(double now, bool engineOn, bool keyHeld) {
        if (!occupied) {
            occupied = true;
            enteredAt = now;
        }
        if (engineOn) completed = true;
        if (completed || now - enteredAt < 5.0) return Hint::None;
        return keyHeld ? Hint::TurnClockwise : Hint::PointToKey;
    }
};

// Both arrows use the same one-second cycle, evenly split on/off.
inline bool HintVisible(double now, double enteredAt) {
    const double elapsed = now - enteredAt - 5.0;
    if (!std::isfinite(elapsed) || elapsed < 0.0) return false;
    return std::fmod(elapsed, 1.0) < 0.5;
}

} // namespace savr::ignition
