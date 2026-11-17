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
        return s.Step(f,p,held,tracked,grab,epoch,.02f,.15f,1.f);
    };
    CHECK(step({.18f,0,0},true,true,true)==0);
    for(int i=0;i<360;++i){const float a=i*.0174532925f;CHECK(step({.18f*std::cos(a),.18f*std::sin(a),0})==0);}
    CHECK(step({.18f,0,.149f})==0);
    CHECK(step({.329f,0,0})==0);
    CHECK(step({.18f,0,.5f})>0);
    for(int i=0;i<100;++i)step({.18f,0,.5f});
    CHECK(Near(s.amplitude,.45f));
    CHECK(step({.18f,0,.1f})==0);
    step({.18f,0,.5f});CHECK(step({.18f,0,.5f},true,false)==0);
    CHECK(step({.18f,0,.5f})>0); // loss did not learn the drift
    CHECK(step({.18f,0,.5f},false)==0 && !s.seeded);
    CHECK(step({.22f,0,.12f},true,true,true)==0);
    for(int i=0;i<10;++i){float t=i*.1f;CHECK(step({.22f-.04f*t,0,.12f*(1-t)})==0);}
    CHECK(step({.18f,0,-.5f})>0);
    CHECK(step({.18f,0,-.5f},true,true,false,2)==0); // recenter epoch
    CHECK(step({std::numeric_limits<float>::quiet_NaN(),0,0})==0);
    const auto original=Measure(f,{.5f,.2f,.3f});
    Frame moved{{1,2,3},{0,1,0},{0,0,1},{1,0,0},.18f};
    auto transformed=Measure(moved,{1.3f,2.5f,3.2f});
    CHECK(Near(original.radial,transformed.radial)&&Near(original.axial,transformed.axial));
    // Equal departures from the arc have equal feedback: inward, outward,
    // toward the driver and away. The centre must not be a silent special case.
    auto response=[&](Vec p,float zone,Vec initial=Vec{.18f,0,0}) {
        State a;a.Step(f,initial,true,true,true,1,0,zone,1);
        for(int i=0;i<100;++i)a.Step(f,p,true,true,false,1,.02f,zone,1);
        return a.amplitude;
    };
    for(float zone:{0.f,.01f,.05f}) {
        const float outward=response({.28f,0,0},zone);
        CHECK(outward>0);
        CHECK(Near(outward,response({.08f,0,0},zone)));
        CHECK(Near(outward,response({.18f,0,.1f},zone)));
        CHECK(Near(outward,response({.18f,0,-.1f},zone)));
    }
    CHECK(response({0,0,0},.15f)>0);
    CHECK(response({.17f,0,0},0)>0); // zero really means zero, not a hidden 10 cm floor
    CHECK(response({.17f,0,0},.02f)==0);
    CHECK(response({.18f,0,.14f},.15f)==0);
    CHECK(response({.08f,0,0},0,{.03f,0,0})>0); // no exempt initial-grip-to-rim corridor
    CHECK(response({.18f,0,0},0,{.03f,0,0})==0);
    for(int i=0;i<360;++i) {
        const float a=i*.0174532925f;
        CHECK(Near(Distance(Measure(f,{.18f*std::cos(a),.18f*std::sin(a),0})),0));
    }
    CHECK(Frequency(0)==40&&Frequency(1)==75); // existing saved presets retain their meaning
    const int ascending[]{20,30,40,50,60,75,90,110,150,200};
    int preset=2;
    for(int hz:ascending) { CHECK(Frequency(preset)==hz);preset=CycleFrequency(preset,1); }
    CHECK(preset==2&&Frequency(CycleFrequency(2,-1))==200);
    CHECK(Frequency(-1)==40&&Frequency(999)==200);
    CHECK(RangeFrequency(0,40,200)==40&&RangeFrequency(1,40,200)==200);
    CHECK(RangeFrequency(.5f,40,200)==120&&RangeFrequency(.5f,200,40)==120);
    CHECK(RangeFrequency(-1,40,200)==40&&RangeFrequency(2,40,200)==200);
    State ramp, quiet;
    ramp.Step(f,{.18f,0,0},true,true,true,1,0,.15f,1);
    quiet.Step(f,{.18f,0,0},true,true,true,1,0,.15f,0);
    float lastMix=0;
    for(int i=0;i<100;++i) {
        const Vec p{.18f,0,.15f+.0035f*i};
        ramp.Step(f,p,true,true,false,1,.02f,.15f,1);
        quiet.Step(f,p,true,true,false,1,.02f,.15f,0);
        CHECK(ramp.frequencyMix>=lastMix&&ramp.frequencyMix<=1);
        CHECK(Near(ramp.frequencyMix,quiet.frequencyMix)&&quiet.amplitude==0);
        lastMix=ramp.frequencyMix;
    }
    for(int i=0;i<100;++i)ramp.Step(f,{.18f,0,.325f},true,true,false,1,.02f,.15f,1);
    CHECK(Near(RangeFrequency(ramp.frequencyMix,40,200),120,.001f));
    ramp.Step(f,{.18f,0,.5f},true,false,false,1,.02f,.15f,1);
    CHECK(ramp.frequencyMix==0&&ramp.amplitude==0);
    ramp.Step(f,{.18f,0,.5f},true,true,false,1,.02f,.15f,1);
    CHECK(ramp.frequencyMix>0);
    ramp.Step(f,{.18f,0,0},true,true,false,1,.02f,.15f,1);
    CHECK(ramp.frequencyMix==0&&ramp.amplitude==0);
    State inward,outward;
    for(auto* a:{&inward,&outward})a->Step(f,{.18f,0,0},true,true,true,1,0,0,1);
    for(int i=0;i<100;++i){
        inward.Step(f,{.08f,0,0},true,true,false,1,.02f,0,1);
        outward.Step(f,{.28f,0,0},true,true,false,1,.02f,0,1);
        CHECK(Near(inward.frequencyMix,outward.frequencyMix));
    }
    float amplitudes[3]{};int j=0;
    for(int hz:{72,90,120}) { State a;a.Step(f,{.18f,0,0},true,true,true,1,0,.10f,1);
        for(int i=0;i<hz;++i)a.Step(f,{.18f,0,.3f},true,true,false,1,1.f/hz,.10f,1);
        amplitudes[j++]=a.amplitude; }
    CHECK(Near(amplitudes[0],amplitudes[1])&&Near(amplitudes[1],amplitudes[2]));
    using namespace savr::haptics;
    PhaseClock phase;
    CHECK(phase.Start(100000,40,false)==0);
    CHECK(std::abs(phase.Start(100000.031,120,false)-.24)<1e-6);
    CHECK(std::abs(phase.Start(100000.061,75,false)-.84)<1e-6);
    CHECK(phase.Start(100001,75,false)==0); // old output cannot bridge a long pause
    CHECK(phase.Start(100001.031,120,true)==0);
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
    right.test={Source::Test,.2f,40,.35,true};
    CHECK(right.Select(.1,true,true).source==Source::Test);
    CHECK(right.Select(.4,true,true).source==Source::None);
    CHECK(!PcmUsable(false,true,2000)&&!PcmUsable(true,false,2000)&&!PcmUsable(true,true,0));
    CHECK(!PcmUsable(true,true,std::numeric_limits<float>::infinity()));
    float out[4000];
    CHECK(Buffer(out,4000,0,40,.3f,0,0)==0);
    CHECK(Buffer(out,4000,100,75,.3f,0,0)==0);
    for(float hz:savr::wheelhaptics::kFrequencies)for(int wave:{0,1,2}){
        const auto n=Buffer(out,4000,2000,hz,.3f,wave,0);CHECK(n>0&&n<=150);
        double sum=0;for(std::size_t i=0;i<n;++i){CHECK(std::abs(out[i])<=.3f);sum+=out[i];}
        CHECK(std::abs(sum/n)<.002);
    }
    double energy[3]{};
    for(int i=0;i<2000;++i)for(int wave=0;wave<3;++wave){const auto x=Sample(i/2000.,wave);energy[wave]+=x*x;}
    CHECK(std::abs(energy[0]-energy[1])<1e-4&&std::abs(energy[1]-energy[2])<.001);
    for(int i=0;i<100;++i)CHECK(Near(Sample(i/100.,0),-Sample(i/100.,1)));
    std::puts("wheel geometry, thresholds, tracking, arbitration and PCM waveforms: PASS");
}
