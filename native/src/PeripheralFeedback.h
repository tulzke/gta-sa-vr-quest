#pragma once

#include "Combat.h"
#include "WantedFeedback.h"

namespace savr::peripheral {
struct Halo {
    float red{},green{},blue{},opacity{};
    float inner{.45f},outer{.90f};
    bool Active() const { return opacity>0; }
};

// One colour, one alpha budget: damage > guard impact > wanted change.
// The wanted animation runs on wall time even while hidden by a combat cue.
inline Halo Select(const combat::Feedback& combat,const wantedfeedback::Feedback& wanted) {
    if (combat.damage>0)
        return {1.f,.025f,.015f,.55f*combat.damage,.48f,1.12f};
    if (combat.block>0)
        return {.55f,.55f,.55f,.42f*combat.block};
    return {wanted.red,wanted.green,wanted.blue,wanted.opacity};
}
} // namespace savr::peripheral
