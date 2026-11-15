#pragma once

#include <algorithm>
#include <cmath>

namespace savr::bikepitch {

inline float EffectiveLean(float lean,bool blockWheelie) {
    if (!std::isfinite(lean)) return 0.0f;
    lean=std::clamp(lean,-1.0f,1.0f);
    return blockWheelie?std::max(0.0f,lean):lean;
}

// Positive game input is forward; negative rotation lowers the fingers over
// the bar. This cue is independent of the bike's lateral cornering lean.
inline float WristAngle(float lean) {
    return -lean*(10.0f*3.14159265358979323846f/180.0f);
}

struct Haptic {
    bool send{};
    float amplitude{},frequency{},durationMs{};
};

struct Feedback {
    int direction{};
    double started{},last{};
    Haptic Update(float lean,double now) {
        const int next=std::isfinite(lean)&&std::isfinite(now)?
            (lean>.01f?1:lean<-.01f?-1:0):0;
        if (!next) {
            const bool stop=direction!=0;
            *this={};
            return {stop,0.0f,0.0f,1.0f};
        }
        const bool changed=next!=direction||now<last;
        if (changed) { direction=next;started=now; }
        if (!changed&&now-last<.04) return {};
        last=now;
        // Repeated 75 ms effects overlap at the 40 ms refresh interval. The
        // rear cue never falls silent: its envelope distinguishes directions
        // even on a controller that ignores the requested frequency.
        const float amplitude=direction>0?.38f:
            .18f+.08f*static_cast<float>(std::cos((now-started)*18.8495559215));
        return {true,amplitude,direction>0?160.0f:80.0f,75.0f};
    }
};

} // namespace savr::bikepitch
