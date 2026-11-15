#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace savr::wantedfeedback {

struct Feedback {
    float red{.08f}, green{.32f}, blue{1.f}, opacity{};
};

// GameThread observation, independent of weapon/hand interaction gates.
void Update(bool gameplay);
// XR samples once per eye pair. No engine pointers cross to the XR thread.
Feedback GetFeedback(bool visible, std::uint64_t trackingEpoch);

namespace detail {

inline float Smooth(double value) {
    const float t=static_cast<float>(std::clamp(value,0.0,1.0));
    return t*t*(3.f-2.f*t);
}
inline Feedback Mix(Feedback from,Feedback to,float t) {
    return {from.red+(to.red-from.red)*t,
            from.green+(to.green-from.green)*t,
            from.blue+(to.blue-from.blue)*t,
            from.opacity+(to.opacity-from.opacity)*t};
}

struct Effect {
    int direction{};
    double start{};
    Feedback initial{};

    Feedback Sample(double now) const {
        const double age=now-start;
        const bool rising=direction>0;
        const double attack=rising?.06:.10, holdEnd=rising?.15:.25;
        const double fadeStart=rising?.15:.85, end=rising?.55:1.50;
        if (!direction || !std::isfinite(now) || age<0 || age>=end) return {};
        const Feedback blue{.08f,.32f,1.f,rising?.22f:.18f};
        if (age<attack) return Mix(initial,blue,Smooth(age/attack));
        Feedback result=blue;
        if (!rising) {
            const Feedback grey{.55f,.55f,.55f,blue.opacity};
            result=Mix(blue,grey,Smooth((age-holdEnd)/(fadeStart-holdEnd)));
        }
        if (age>fadeStart)
            result.opacity*=1.f-Smooth((age-fadeStart)/(end-fadeStart));
        return result;
    }
};

struct Observation {
    std::uintptr_t player{}, wanted{};
    int level{-1};
    bool enabled{};
    std::uint64_t trackingEpoch{};
    std::uint32_t gameFrame{}, gameMs{};
};

// Both clocks reset on save/new-game load; natural uint32 wrap is forward.
inline bool MovedBackward(std::uint32_t previous,std::uint32_t current) {
    return current-previous>0x7fffffffU;
}

class State {
    bool valid_{};
    Observation previous_{};
    double updated_{};
    Effect effect_{};
public:
    void Reset() { *this={}; }

    void Observe(const Observation& input,double now) {
        if (!input.enabled || !input.player || !input.wanted ||
            input.level<0 || input.level>6 || !std::isfinite(now)) {
            Reset();return;
        }
        const bool baseline=!valid_ || input.player!=previous_.player ||
            input.wanted!=previous_.wanted || input.trackingEpoch!=previous_.trackingEpoch ||
            now<updated_ || now-updated_>.5 ||
            MovedBackward(previous_.gameFrame,input.gameFrame) ||
            MovedBackward(previous_.gameMs,input.gameMs);
        if (baseline) effect_={};
        else if (input.level!=previous_.level) {
            // Carry both colour and alpha, including a reverse mid-animation.
            const Feedback current=effect_.Sample(now);
            effect_={input.level>previous_.level?1:-1,now,current};
        }
        previous_=input;updated_=now;valid_=true;
    }

    Feedback Sample(double now,std::uint64_t trackingEpoch) const {
        if (!valid_ || trackingEpoch!=previous_.trackingEpoch ||
            now<updated_ || now-updated_>.5) return {};
        return effect_.Sample(now);
    }
};

} // namespace detail
} // namespace savr::wantedfeedback
