#pragma once
#include <algorithm>
#include <cmath>

namespace savr::cockpitboundary {
struct Pulse {bool play{};float amplitude{},durationMs{};};
// Portable hot/cold feedback. Short, unqueued pulses leave other effects free
// to take over immediately; clicks get a separate quiet interval.
struct Feedback {
    float strength{},pulseWait{},quiet{};
    void Reset(){*this=Feedback{};}
    Pulse Step(float errorMetres,bool active,float dt,bool priorityClick=false){
        dt=std::isfinite(dt)?std::clamp(dt,0.f,.1f):0.f;
        if(!active||!std::isfinite(errorMetres)){Reset();return {};}
        if(priorityClick){strength=0;pulseWait=0;quiet=.07f;return {};}
        quiet=std::max(0.f,quiet-dt);pulseWait=std::max(0.f,pulseWait-dt);
        const float t=std::clamp((errorMetres-.002f)/.06f,0.f,1.f);
        const float target=.65f*t*t*(3.f-2.f*t);
        if(target==0){strength=0;pulseWait=0;return {};}
        const float tau=target>strength?.06f:.015f;
        strength+=(target-strength)*(1.f-std::exp(-dt/tau));
        if(quiet>0||pulseWait>0||strength<.015f)return {};
        pulseWait=.025f;return {true,strength,30.f};
    }
};
}
