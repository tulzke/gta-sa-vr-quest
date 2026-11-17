#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace savr::pickups {
inline constexpr float kGrabRangeM=.18f;
inline constexpr float kGripClose=.65f, kGripOpen=.30f;
inline constexpr double kHoldSeconds=1.;
// A nonzero target includes the pickup's generation. No dwell, transfer of
// progress or retry is possible after a press outside the eligible zone.
struct Gesture {
    bool latched{true},active{};
    std::uint64_t target{},epoch{};
    double started{},previous{};
    float progress{};
    void Cancel() { active=false;target=0;progress=0; }
    float HapticAmplitude() const {
        if(!active||!std::isfinite(progress))return 0;
        const float t=std::clamp(progress,0.f,1.f);
        return .35f*t*t*(3.f-2.f*t); // soft start/end, below the completion pulse
    }
    bool Step(std::uint64_t candidate,float grip,bool usable,
              std::uint64_t trackingEpoch,double now) {
        const bool discontinuity=epoch!=trackingEpoch||!std::isfinite(now)||
            (previous>0&&(now<previous||now-previous>.15));
        epoch=trackingEpoch;previous=now;
        if(!usable||!std::isfinite(grip)||discontinuity) {
            Cancel();latched=true;return false;
        }
        if(grip<=kGripOpen) { Cancel();latched=false;return false; }
        if(active&&candidate!=target) { Cancel();latched=true; }
        if(!latched&&grip>=kGripClose) {
            latched=true;
            if(candidate) { active=true;target=candidate;started=now; }
        }
        if(!active)return false;
        progress=std::clamp(float((now-started)/kHoldSeconds),0.f,1.f);
        if(progress<1.f)return false;
        Cancel();return true;
    }
};
inline bool CanWalkCollect(int incoming,int owned) { return incoming>0&&incoming==owned; }
} // namespace savr::pickups
