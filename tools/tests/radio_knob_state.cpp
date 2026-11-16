#include "RadioKnobState.h"
#include <cassert>
#include <limits>
#include <cstdio>

int main() {
    savr::radio::KnobState knob;
    assert(knob.Step(true, 350) == 0);
    assert(knob.Step(true, 12) == 0);
    assert(knob.Step(true, 40) == 1); // wrap across 360, exactly fifty degrees
    assert(knob.Step(true, 60) == 0 && knob.angle == 60);
    assert(knob.haptic.play && knob.haptic.amplitude==.85f);
    assert(knob.Step(true, 60) == 0 && !knob.haptic.play);
    assert(knob.Step(true, 3) == 0);
    assert(knob.Step(true, 53) == 1);
    assert(knob.Step(false, 53) == 0 && knob.angle == 0);
    assert(knob.Step(true, 20) == 0);
    assert(knob.Step(true, -30) == -1);
    assert(knob.Step(true, std::numeric_limits<float>::quiet_NaN()) == 0);
    assert(!knob.held && knob.angle == 0);

    // Ratchet teeth require actual motion, work in both directions, and have
    // no effect on station selection below the fifty-degree threshold.
    knob.Reset();assert(knob.Step(true,0,.02f)==0 && !knob.haptic.play);
    for(float angle:{1.f,2.f,4.f,5.f})assert(knob.Step(true,angle,.02f)==0 && !knob.haptic.play);
    assert(knob.Step(true,6,.02f)==0 && knob.haptic.play && knob.haptic.amplitude==.14f && knob.haptic.durationMs==12);
    assert(knob.Step(true,6,.02f)==0 && !knob.haptic.play);
    assert(knob.Step(true,0,.02f)==0 && knob.haptic.play && knob.haptic.amplitude==.14f);
    assert(knob.Step(true,-6,.02f)==0 && knob.haptic.play);
    assert(knob.Step(true,-49,.02f)==0 && knob.armed);
    assert(knob.Step(true,-50,.02f)==-1 && knob.haptic.play && knob.haptic.amplitude==.3f);
    assert(knob.Step(true,-56,.005f)==0 && !knob.haptic.play); // don't replace station click
    assert(knob.Step(true,-56,.1f)==0 && !knob.haptic.play); // no queued pulse while still
    assert(knob.Step(true,-60,.02f)==0 && knob.angle==-60 && knob.haptic.play && knob.haptic.amplitude==.85f);
    assert(knob.Step(true,-90,.02f)==0 && knob.angle==-60 && !knob.haptic.play);
    assert(knob.Step(true,-88,.02f)==0 && !knob.haptic.play); // two degrees off stop
    assert(knob.Step(true,-90,.02f)==0 && !knob.haptic.play); // jitter does not repeat stop
    assert(knob.Step(true,-83,.1f)==0 && knob.angle==-53); // pull away to rearm
    assert(knob.Step(true,-90,.1f)==0 && knob.haptic.play && knob.haptic.amplitude==.85f);
    assert(knob.Step(true,-84,.005f)==0 && !knob.haptic.play); // don't replace stop with ratchet
    knob.Step(false,-84,.02f);assert(!knob.held && !knob.haptic.play && knob.stopSide==0 && knob.feedbackWait==0);

    // A fast sweep to the stop yields one station event and one strong pulse.
    knob.Step(true,0);assert(knob.Step(true,90,.02f)==1 && knob.angle==60 && knob.haptic.amplitude==.85f);
    assert(knob.Step(true,150,.1f)==0 && knob.angle==60 && !knob.haptic.play);
    assert(knob.Step(true,90,.1f)==0 && knob.angle==0); // immediate return from saturation
    assert(knob.Step(true,40,.02f)==-1); // rearmed without releasing grip
    knob.Reset();knob.Step(true,0);
    assert(knob.Step(true,49.9f,.02f)==0 && knob.armed);
    assert(knob.Step(true,50,.02f)==1 && !knob.armed);
    assert(knob.Step(true,55,.1f)==0); // no second switch in the last ten degrees
    std::puts("PASS: radio knob +/-60 degree travel, ratchet, station click, stop hysteresis and haptic priorities");
}
