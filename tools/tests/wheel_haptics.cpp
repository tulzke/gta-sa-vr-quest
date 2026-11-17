#include "WheelHaptics.h"
#include "HapticPolicy.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(c) do { if(!(c)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);std::exit(1);} } while(false)
bool Near(float a,float b,float epsilon=1e-4f){return std::abs(a-b)<epsilon;}
int main() {
    using namespace savr::wheelhaptics;
    Frame f;
    State s;
    auto step=[&](Vec p,bool held=true,bool tracked=true,bool grab=false,std::uint64_t epoch=1){
        return s.Step(f,p,held,tracked,grab,epoch,.02f);
    };
    CHECK(step({.18f,0,0},true,true,true)==0);
    for(int i=0;i<360;++i){const float a=i*.0174532925f;CHECK(Near(step({.18f*std::cos(a),.18f*std::sin(a),0}),0));}
    CHECK(step({.18f,0,.001f})>0);
    CHECK(step({.181f,0,0})>0);
    CHECK(step({.18f,0,.5f})>0);
    for(int i=0;i<100;++i)step({.18f,0,.5f});
    CHECK(Near(s.amplitude,.45f));
    CHECK(step({.18f,0,0})==0);
    step({.18f,0,.5f});CHECK(step({.18f,0,.5f},true,false)==0);
    CHECK(step({.18f,0,.5f})>0); // loss did not learn the drift
    CHECK(step({.18f,0,.5f},false)==0 && !s.seeded);
    CHECK(step({.22f,0,.12f},true,true,true)==0);
    for(int i=0;i<10;++i){float t=i*.1f;CHECK(step({.22f-.04f*t,0,.12f*(1-t)})>0);}
    CHECK(step({.18f,0,-.5f})>0);
    CHECK(step({.18f,0,-.5f},true,true,false,2)==0); // recenter epoch
    CHECK(step({std::numeric_limits<float>::quiet_NaN(),0,0})==0);
    const auto original=Measure(f,{.5f,.2f,.3f});
    Frame moved{{1,2,3},{0,1,0},{0,0,1},{1,0,0},.18f};
    auto transformed=Measure(moved,{1.3f,2.5f,3.2f});
    CHECK(Near(original.radial,transformed.radial)&&Near(original.axial,transformed.axial));
    // Equal departures from the arc have equal feedback: inward, outward,
    // toward the driver and away. The centre must not be a silent special case.
    auto response=[&](Vec p,Vec initial=Vec{.18f,0,0}) {
        State a;a.Step(f,initial,true,true,true,1,0);
        for(int i=0;i<100;++i)a.Step(f,p,true,true,false,1,.02f);
        return a.amplitude;
    };
    {
        const float outward=response({.28f,0,0});
        CHECK(outward>0);
        CHECK(Near(outward,response({.08f,0,0})));
        CHECK(Near(outward,response({.18f,0,.1f})));
        CHECK(Near(outward,response({.18f,0,-.1f})));
    }
    CHECK(response({0,0,0})>0);
    CHECK(response({.17f,0,0})>0); // zero really means zero, not a hidden 10 cm floor
    CHECK(response({.08f,0,0},{.03f,0,0})>0); // no exempt initial-grip-to-rim corridor
    CHECK(response({.18f,0,0},{.03f,0,0})==0);
    for(int i=0;i<360;++i) {
        const float a=i*.0174532925f;
        CHECK(Near(Distance(Measure(f,{.18f*std::cos(a),.18f*std::sin(a),0})),0));
    }
    float amplitudes[3]{};int j=0;
    for(int hz:{72,90,120}) { State a;a.Step(f,{.18f,0,0},true,true,true,1,0);
        for(int i=0;i<hz;++i)a.Step(f,{.18f,0,.3f},true,true,false,1,1.f/hz);
        amplitudes[j++]=a.amplitude; }
    CHECK(Near(amplitudes[0],amplitudes[1])&&Near(amplitudes[1],amplitudes[2]));
    using namespace savr::haptics;
    Hand left,right;
    left.wheel={Source::Wheel,.3f,0,1};right.wheel=left.wheel;
    left.game={Source::Game,.7f,0,.2};
    CHECK(left.Select(.1,true,false).source==Source::Game);
    CHECK(right.Select(.1,true,false).source==Source::Wheel);
    left.wheel={};CHECK(left.Select(.1,true,false).source==Source::Game);
    CHECK(left.Select(.3,true,false).source==Source::None);
    CHECK(right.Select(1.1,true,false).source==Source::None);
    CHECK(right.Select(.1,false,false).source==Source::None);
    CHECK(right.Select(.1,true,true).source==Source::None);
    std::puts("wheel geometry, zero free zone, tracking and arbitration: PASS");
}
