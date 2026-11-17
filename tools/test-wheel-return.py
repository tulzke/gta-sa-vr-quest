"""Compile production wheel return and circular grip solver with synthetic poses."""
from pathlib import Path
import runpy
import tempfile

ROOT = Path(__file__).resolve().parents[1]
helpers = runpy.run_path(str(ROOT / 'tools/test-bike-steering.py'))
source = helpers['SOURCE']
function = helpers['function']
start = source.index('        if (circular) {\n            const int mask=')
end = source.index(' else if (left&&rightGrabbed)', start)
solver = source[start:end]
start = source.index('    const float maxSteering=(visual.bike?35.0f:80.0f)')
conversion = source[start:source.index('    if (leanSteering&&!preview)', start)]
test = r'''
#include "Driving.h"
#include "WheelReturn.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace savr;
using namespace savr::driving;
#define CHECK(c) do {if(!(c)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
bool Near(float a,float b){return std::abs(a-b)<0.0001f;}
''' + source[source.index('struct V3 {'):source.index('double MonotonicSeconds()')] + '\n'.join(
    function(s) for s in ['float WrapAngle(', 'float UnwrapAngle(']
) + function('struct RimSteering {') + r''';
bool g_twoHandAngleValid{};
RimSteering g_rimSteering{};
V3 velocity{};
bool ReadEntityMatrix(void*,V3*,V3* f,V3*,V3*){*f={0,1,0};return true;}
WheelVisualState Solve(float previousAngle,int mask,float dt,V3 p0,V3 p1,
                       int type=VEHICLE_CAR,V3 center={},V3 right={1,0,0},
                       V3 up={0,1,0},bool tracking=true){
    alignas(16) char vehicle[256]{};
    *reinterpret_cast<V3*>(vehicle+0x68)=velocity;
    void* activeVehicle=vehicle;
    const int activeType=type;
    const bool circular=true,left=(mask&1)!=0,rightGrabbed=(mask&2)!=0;
    struct Pose {bool positionTracked;};
    Pose poses[2]={{tracking},{tracking}};
    V3 positions[2]={p0,p1};
    const float wheelReturnDt=dt;
    float steeringAngle=0;
    WheelVisualState visual{};
''' + solver + conversion + r'''
    return visual;
}
int main(){
    using namespace wheelreturn;
    CHECK(ReturnRate(0)==0);CHECK(Near(ReturnRate(20.f/180),1.f/.4f));
    CHECK(Near(ReturnRate(60.f/180),1.f/.175f));CHECK(Near(ReturnRate(100),1.f/.175f));
    CHECK(Near(ReturnRate(-20.f/180),1.f/.4f));
    CHECK(ReturnRate(std::numeric_limits<float>::quiet_NaN())==0);
    CHECK(ReturnRate(std::numeric_limits<float>::infinity())==0);
    for(float kmh:{.001f,1.f,5.f,10.f,15.f,19.999f,20.f}){
        CHECK(Near(ReturnRate(kmh/180),kmh/20*ReturnRate(20.f/180)));
        CHECK(Near(ReturnRate(-kmh/180),ReturnRate(kmh/180)));
    }
    CHECK(Near(ReturnRate(20.001f/180),ReturnRate(20.f/180)));
    for(float angle:{-1.f,1.f,-.0001f,.0001f}){
        float parked=angle;
        for(int i=0;i<10000;++i)parked=ReleasedAngle(parked,0,.01f);
        CHECK(parked==angle);
    }
    CHECK(Near(ReleasedAngle(1,0,0),1));CHECK(Near(ReleasedAngle(1,0,1),1));
    CHECK(Near(ReleasedAngle(1,0,-1),1));
    Clock clock;
    CHECK(clock.Step(1000)==0);CHECK(Near(clock.Step(1010),.01f));
    CHECK(clock.Step(1010)==0);CHECK(clock.Step(3000)==0);
    CHECK(clock.Step(2000)==0);CHECK(Near(clock.Step(2010),.01f));
    for(int hz:{72,90,120}){
        float a=1;
        for(int i=0;i<hz;++i)a=ReleasedAngle(a,10.f/180,1.f/hz);
        CHECK(Near(a,std::exp(-1.f*1.25f)));
    }
    for(int mask:{1,2,3})for(float sign:{-1.f,1.f}){
        g_rimSteering={};velocity={0,20.f/180,0};
        const V3 p0{-.2f,0,0},p1{.2f,0,0};
        auto v=Solve(sign,mask,.01f,p0,p1);
        CHECK(Near(v.physicalAngle,sign)); // first grab keeps current angle
        v=Solve(v.physicalAngle,0,.01f,p0,p1);
        CHECK(std::abs(v.physicalAngle)<1&&std::abs(v.physicalAngle)>.9f);
        CHECK(v.steering*sign>0);
        const float mid=v.physicalAngle;
        v=Solve(mid,mask,.01f,p0,p1);
        CHECK(Near(v.physicalAngle,mid)); // regrab stops return in this frame
        v=Solve(mid,mask,.01f,{0,0,0},{0,0,0});
        CHECK(Near(v.physicalAngle,mid)); // hands at axis still own wheel
        velocity={};
        const auto parked=Solve(mid,0,.01f,p0,p1);
        CHECK(parked.physicalAngle==mid);
        CHECK(Near(parked.steering,v.steering));
        velocity={0,20.f/180,0};
        for(int i=0;i<1000;++i)v=Solve(v.physicalAngle,0,.01f,p0,p1);
        CHECK(v.physicalAngle==0&&v.steering==0);
    }
    velocity={0,.4f,0};const float fast=Solve(1,0,.01f,{},{}).physicalAngle;
    velocity={0,-.4f,0};CHECK(Near(Solve(1,0,.01f,{},{}).physicalAngle,fast));
    velocity={.4f,0,.4f};const float slow=Solve(1,0,.01f,{},{}).physicalAngle;
    CHECK(fast<slow);
    CHECK(slow==1); // lateral/vertical motion cannot self-centre a parked car
    velocity={0,.4f,0};CHECK(Near(Solve(1,0,.01f,{},{},VEHICLE_BOAT).physicalAngle,std::exp(-.01f/.75f)));
    velocity={0,10.f/180,0};const auto halfSpeed=Solve(1,0,.1f,{},{});
    velocity={0,20.f/180,0};const auto fullSpeed=Solve(1,0,.1f,{},{});
    CHECK(Near(-std::log(halfSpeed.physicalAngle),-.5f*std::log(fullSpeed.physicalAngle)));
    // 1 -> 2 -> 1 and restored valid angles keep current wheel orientation.
    g_rimSteering={};
    for(int mask:{1,3,2,3,1})CHECK(Near(Solve(.7f,mask,.01f,{-.2f,0,0},{.2f,0,0}).physicalAngle,.7f));
    CHECK(Near(Solve(.7f,1,.01f,{0,0,0},{}).physicalAngle,.7f));
    CHECK(Near(Solve(.7f,1,.01f,{-.2f,0,0},{}).physicalAngle,.7f));

    // A calibrated, tilted wheel starts at a nonzero angle. Translate and
    // rotate its live cabin frame independently of the controller positions.
    constexpr float pi=3.14159265358979323846f;
    const float stop=80.f*pi/180.f;
    const V3 center{.22f,1.1f,-.4f}, right{1,0,0}, up{0,.6f,.8f};
    const V3 normal{0,-.8f,.6f};
    const V3 bumpedCenter=center+V3{.32f,-.21f,.45f};
    const V3 bumpedRight=up, bumpedUp=normal;
    auto position=[&](float angle,float axial){
        return center+(right*std::cos(angle)+up*std::sin(angle))*.20f+normal*axial;
    };
    for (int mask:{1,2,3}) {
        g_rimSteering={};
        const float startAngle=.2f;
        const float leftStart=pi-.12f, rightStart=-.12f;
        auto v=Solve(startAngle,mask,.01f,position(leftStart,0),position(rightStart,0),
                     VEHICLE_CAR,center,right,up);
        CHECK(Near(v.physicalAngle,startAngle));
        for (int cycle=0;cycle<128;++cycle) {
            for (float turn:{0.f,.35f,1.2f,1.8f,2.5f,3.05f,3.5f,3.05f,2.2f,1.5f,.8f,0.f,
                             -.5f,-1.3f,-2.1f,-2.8f,-3.5f,-2.9f,-2.1f,-1.4f,-.7f,0.f}) {
                v=Solve(v.physicalAngle,mask,.01f,position(leftStart+turn,.12f),
                        position(rightStart+turn,-.08f),VEHICLE_CAR,bumpedCenter,
                        bumpedRight,bumpedUp);
                CHECK(Near(v.physicalAngle,std::clamp(startAngle+turn,-stop,stop)));
            }
        }
        // A near-axis sample or valid-but-untracked pose must not reset the
        // angle reference. On recovery, read the current absolute position.
        v=Solve(v.physicalAngle,mask,.01f,center,center,VEHICLE_CAR,bumpedCenter,bumpedRight,bumpedUp);
        CHECK(Near(v.physicalAngle,startAngle));
        v=Solve(v.physicalAngle,mask,.01f,position(leftStart+1,0),position(rightStart+1,0),
                VEHICLE_CAR,bumpedCenter,bumpedRight,bumpedUp,false);
        CHECK(Near(v.physicalAngle,startAngle));
        v=Solve(v.physicalAngle,mask,.01f,position(leftStart+.4f,0),position(rightStart+.4f,0),
                VEHICLE_CAR,bumpedCenter,bumpedRight,bumpedUp);
        CHECK(Near(v.physicalAngle,startAngle+.4f));
        // Joining or releasing the other hand preserves the current angle
        // while keeping the first plane through the handover.
        for (int nextMask:{3,1,2}) {
            v=Solve(v.physicalAngle,nextMask,.01f,position(leftStart+.4f,0),position(rightStart+.4f,0),
                    VEHICLE_CAR,bumpedCenter,bumpedRight,bumpedUp);
            CHECK(Near(v.physicalAngle,startAngle+.4f));
        }
        v=Solve(v.physicalAngle,2,.01f,position(leftStart+.6f,0),position(rightStart+.6f,0),
                VEHICLE_CAR,bumpedCenter,bumpedRight,bumpedUp);
        CHECK(Near(v.physicalAngle,startAngle+.6f));
        Solve(v.physicalAngle,0,0,{},{});
        v=Solve(v.physicalAngle,2,0,{},bumpedCenter+bumpedRight*.2f,
                VEHICLE_CAR,bumpedCenter,bumpedRight,bumpedUp);
        v=Solve(v.physicalAngle,2,0,{},bumpedCenter+bumpedUp*.2f,
                VEHICLE_CAR,bumpedCenter,bumpedRight,bumpedUp);
        CHECK(Near(v.physicalAngle,stop));
    }
    std::puts("PASS: wheel return, fixed grab plane, handovers, tracking recovery and both hard stops");
}
'''
with tempfile.TemporaryDirectory(prefix='savr-wheel-return-') as tmp:
    output = Path(tmp)
    cpp = output / 'wheel-return.cpp'
    cpp.write_text(test, encoding='utf8')
    helpers['compile_run'](cpp, 'wheel-return', output)
