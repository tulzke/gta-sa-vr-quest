#pragma once

#include <algorithm>
#include <cmath>

namespace savr::ignition {

// The key face points opposite cross(right, up). Bike steering frames have
// the opposite handedness from car wheels, including saved debug poses.
inline bool FlipFaceNormal(float handedness) {
    return std::isfinite(handedness) && handedness > 0.0f;
}

// A grab from below the key uses the reverse pose. The caller latches the
// choice at the grip edge so movement while held cannot flip the hand.
inline bool ReverseGrip(float heightAlongKey) {
    return std::isfinite(heightAlongKey) && heightAlongKey < 0.0f;
}

// Clockwise travel in degrees, measured from the vertical OFF position.
// The first 60 degrees snap OFF; the final 30 snap ON upon release.
inline bool ReleasedEngineOn(float angleDegrees) {
    return std::isfinite(angleDegrees) && angleDegrees >= 60.0f;
}

inline float ClampAngle(float angleDegrees) {
    if (!std::isfinite(angleDegrees)) return 0.0f;
    // The remaining 270 degrees are split at 225 degrees: the half next
    // to OFF belongs to 0, and the half next to ON belongs to 90.
    const float aroundMidpoint=std::remainder(angleDegrees-45.0f,360.0f)+45.0f;
    return std::clamp(aroundMidpoint,0.0f,90.0f);
}

// A grab takes a reference rather than jumping to the controller's current
// orientation. Rebase whenever hands join or leave so either hand can take over.
struct Rotation {
    float angle{};
    float reference[2]{};
    float travel[2]{};
    float baseline{};
    unsigned owners{};

    void SetEngine(bool on) {
        if (!owners) angle = on ? 90.0f : 0.0f;
    }

    void Step(unsigned mask, const float handDegrees[2]) {
        mask &= 3u;
        if (mask != owners) {
            owners = mask;
            baseline = angle;
            for (int h=0; h<2; ++h) { reference[h] = handDegrees[h]; travel[h]=0.0f; }
        }
        if (!owners) return;
        float sum = 0.0f;
        int count = 0;
        for (int h=0; h<2; ++h) if (owners & (1u << h)) {
            if (!std::isfinite(handDegrees[h]) || !std::isfinite(reference[h])) return;
            travel[h] += std::remainder(handDegrees[h] - reference[h], 360.0f);
            reference[h] = handDegrees[h];
            sum += travel[h];
            ++count;
        }
        angle = ClampAngle(baseline + sum / count);
    }

    bool Release() {
        const bool on = ReleasedEngineOn(angle);
        angle = on ? 90.0f : 0.0f;
        owners = 0;
        return on;
    }
};

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
