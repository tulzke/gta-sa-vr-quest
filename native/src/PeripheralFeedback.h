#pragma once

#include "Combat.h"
#include "WantedFeedback.h"
#include "VitalsFeedback.h"

namespace savr::peripheral {
struct Halo {
    float red{},green{},blue{},opacity{};
    float inner{.45f},outer{.90f};
    bool Active() const { return opacity>0; }
};

// One colour, one alpha budget: health hit > armour hit > block > wanted change.
// The wanted animation runs on wall time even while hidden by a combat cue.
inline Halo Select(const combat::Feedback& combat,const wantedfeedback::Feedback& wanted,
                   const vitalsfeedback::Feedback& vitals={}) {
    Halo cue{wanted.red,wanted.green,wanted.blue,wanted.opacity};
    if (combat.block>0) cue={.55f,.55f,.55f,.42f*combat.block};
    if (vitals.armourHit>0) cue={.78f,.78f,.78f,.42f*vitals.armourHit,.48f,1.12f};
    if (vitals.healthHit>0) cue={1.f,.025f,.015f,.55f*vitals.healthHit,.48f,1.12f};
    if (vitals.lowHealth<=0) return cue;
    // Persistent, non-flashing red underlay survives other cues. Composite into
    // one pass so simultaneous armour/block/wanted effects cannot stack opacity.
    const float base=.08f+.10f*std::clamp(vitals.lowHealth,0.f,1.f);
    const float under=base*(1-cue.opacity),alpha=cue.opacity+under;
    return {(cue.red*cue.opacity+under)/alpha,
            (cue.green*cue.opacity+.025f*under)/alpha,
            (cue.blue*cue.opacity+.015f*under)/alpha,alpha,.48f,1.12f};
}
} // namespace savr::peripheral
