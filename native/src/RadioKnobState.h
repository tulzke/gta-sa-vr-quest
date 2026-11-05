#pragma once

#include <algorithm>
#include <cmath>

namespace savr::radio {

// Controller angle is measured in degrees in the knob's plane. Each excursion
// produces one event; returning to the centre arms the next excursion.
struct KnobState {
    float angle{};
    float previous{};
    float travel{};
    bool held{};
    bool armed{true};

    void Reset() { *this = {}; }

    int Step(bool grab, float handAngle) {
        if (!grab || !std::isfinite(handAngle)) { Reset(); return 0; }
        if (!held) {
            held = true;
            previous = handAngle;
            return 0;
        }
        travel += std::remainder(handAngle - previous, 360.0f);
        previous = handAngle;
        travel = std::clamp(travel, -30.0f, 30.0f);
        angle = travel;
        if (!armed && std::abs(travel) <= 8.0f) armed = true;
        if (armed && std::abs(travel) >= 22.0f) {
            armed = false;
            return travel > 0.0f ? 1 : -1;
        }
        return 0;
    }
};

} // namespace savr::radio
