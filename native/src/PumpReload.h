#pragma once
#include "ArcadeReload.h"

namespace savr::pumpreload {
using namespace arcadereload;
inline constexpr float kStroke=.12f;
enum Checkpoint { None, Rear, Front };
// Project real relative controller motion onto the calibrated barrel. Whole-gun
// translation/rotation leaves that coordinate unchanged; wrist rotation alone
// must not become a substitute for moving the support controller.
struct Pump {
    int identity{-1},support{-1};std::uint64_t epoch{};
    bool valid{},rear{};float travel{};double time{};
    Vec previous{};float coordinate{};
    void Reset() {*this={};}
    Checkpoint Update(Sample primary,Vec offhand,int key,int supportHand,
                      std::uint64_t trackingEpoch,double now,bool empty) {
        if (!Normalize(primary) || !std::isfinite(offhand.x) || !std::isfinite(offhand.y) ||
            !std::isfinite(offhand.z) || !std::isfinite(now)) {Reset();return None;}
        const Vec relative=Add(offhand,Scale(primary.position,-1));
        const float current=Dot(relative,primary.barrel);
        if (!valid || key!=identity || supportHand!=support || trackingEpoch!=epoch ||
            now<time || now-time>.20) {
            Reset();valid=true;identity=key;support=supportHand;epoch=trackingEpoch;
            previous=relative;coordinate=current;time=now;return None;
        }
        const Vec delta=Add(previous,Scale(relative,-1));
        const float stroke=coordinate-current;coordinate=current;previous=relative;time=now;
        // Never convert a tracking jump into a checkpoint. Loaded guns stay at
        // the forward stop, continually consuming raw motion without banking it.
        if (!empty || Dot(delta,delta)>.25f*.25f) {travel=0;rear=false;return None;}
        if (Dot(delta,delta)<.000000001f) return None;
        travel=std::clamp(travel+stroke,0.f,kStroke);
        if (!rear && travel>=kStroke-.002f) {travel=kStroke;rear=true;return Rear;}
        if (rear && travel<=.008f) {travel=0;rear=false;return Front;}
        return None;
    }
};
inline float HintTravel(unsigned phaseMs) {
    const float t=(phaseMs%1600)*.001f;
    const auto smooth=[](float v){v=std::clamp(v,0.f,1.f);return v*v*(3-2*v);};
    return kStroke*(t<.25f?0:t<.48f?smooth((t-.25f)/.23f):
        t<.70f?1:t<.94f?1-smooth((t-.70f)/.24f):0);
}
} // namespace savr::pumpreload
