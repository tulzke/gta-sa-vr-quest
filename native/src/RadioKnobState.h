#pragma once

#include <algorithm>
#include <cmath>

namespace savr::radio {

constexpr float kKnobLimitDegrees=60.f;
constexpr float kKnobRatchetDegrees=6.f;
struct KnobHaptic {bool play{};float amplitude{},durationMs{};};
// Controller angle is measured in degrees in the knob's plane. Each excursion
// produces one event; returning to the centre arms the next excursion.
struct KnobState {
    float angle{};
    float previous{};
    float travel{};
    bool held{};
    bool armed{true};
    float ratchetAnchor{},feedbackWait{};
    int stopSide{},feedbackPriority{};
    KnobHaptic haptic{};

    void Reset() { *this = {}; }

    void Pulse(int priority,float amplitude,float durationMs) {
        // OpenXR replaces the current vibration. Never cut a strong stop or
        // station click short with a weaker ratchet tooth on the next frame.
        if(feedbackWait>0 && priority<=feedbackPriority)return;
        haptic={true,amplitude,durationMs};feedbackPriority=priority;
        feedbackWait=durationMs*.001f+.005f;
    }

    int Step(bool grab, float handAngle,float dt=1.f/72.f) {
        haptic={};
        dt=std::isfinite(dt)?std::clamp(dt,0.f,.1f):0.f;
        feedbackWait=std::max(0.f,feedbackWait-dt);
        if (!grab || !std::isfinite(handAngle)) { Reset(); return 0; }
        if (!held) {
            held = true;
            previous = handAngle;
            return 0;
        }
        travel += std::remainder(handAngle - previous, 360.0f);
        previous = handAngle;
        travel = std::clamp(travel, -kKnobLimitDegrees, kKnobLimitDegrees);
        angle = travel;
        // Distance-based teeth work in both directions and stay silent when
        // stationary. Updating the anchor even during quiet time avoids queued
        // catch-up vibrations after the hand stops moving.
        const bool tooth=std::abs(angle-ratchetAnchor)>=kKnobRatchetDegrees;
        if(tooth)ratchetAnchor=angle;
        if((stopSide>0 && angle<=kKnobLimitDegrees-4.f) ||
           (stopSide<0 && angle>=-kKnobLimitDegrees+4.f))stopSide=0;
        const int atStop=angle>=kKnobLimitDegrees?1:angle<=-kKnobLimitDegrees?-1:0;
        const bool hitStop=atStop!=0 && atStop!=stopSide;
        if(atStop)stopSide=atStop;
        int step=0;
        if (!armed && std::abs(travel) <= 8.0f) armed = true;
        if (armed && std::abs(travel) >= 50.0f) {
            armed = false;
            step=travel > 0.0f ? 1 : -1;
        }
        if(hitStop)Pulse(3,.85f,75.f);
        else if(step)Pulse(2,.3f,30.f);
        else if(tooth)Pulse(1,.14f,12.f);
        return step;
    }
};

} // namespace savr::radio
