#pragma once
#include "CombatPolicy.h"
#include "WantedFeedback.h"
#include <cstdint>

namespace savr::vitalsfeedback {
struct Feedback { float healthHit{}, armourHit{}, lowHealth{}; };
void Update(bool gameplay);
Feedback GetFeedback(bool visible, std::uint64_t trackingEpoch);

namespace detail {
struct Observation {
    std::uintptr_t player{};
    std::uint64_t trackingEpoch{};
    std::uint32_t gameFrame{}, gameMs{};
    float health{}, armour{};
    bool enabled{};
};
class State {
    Observation previous_{};
    bool valid_{}, low_{};
    double updated_{}, healthAt_{}, armourAt_{};
public:
    void Reset() { *this={}; }
    void Observe(const Observation& input,double now) {
        if (!input.enabled || !input.player || !std::isfinite(now) ||
            !std::isfinite(input.health) || !std::isfinite(input.armour) ||
            input.health<=0 || input.armour<0) { Reset();return; }
        const bool baseline=!valid_ || input.player!=previous_.player ||
            input.trackingEpoch!=previous_.trackingEpoch || now<updated_ || now-updated_>.5 ||
            wantedfeedback::detail::MovedBackward(previous_.gameFrame,input.gameFrame) ||
            wantedfeedback::detail::MovedBackward(previous_.gameMs,input.gameMs);
        if (baseline) { healthAt_=armourAt_=0;low_=false; }
        else {
            if (input.health<previous_.health-.01f) healthAt_=now;
            if (input.armour<previous_.armour-.01f) armourAt_=now;
        }
        if (input.health<=25) low_=true;
        else if (input.health>=30) low_=false;
        previous_=input;updated_=now;valid_=true;
    }
    Feedback Sample(double now,std::uint64_t epoch) const {
        if (!valid_ || !std::isfinite(now) || epoch!=previous_.trackingEpoch ||
            now<updated_ || now-updated_>.5) return {};
        return {combatpolicy::Fade(now,healthAt_,.7),
                combatpolicy::Fade(now,armourAt_,.7),
                low_?std::clamp((30.f-previous_.health)/25.f,.1f,1.f):0.f};
    }
};
} // namespace detail
} // namespace savr::vitalsfeedback
