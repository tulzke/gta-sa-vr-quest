#include "VehicleHorn.h"
#include "HandSkeleton.h"
#include "Driving.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace savr;
using namespace vehiclehorn;
int checks=0;
#define CHECK(c) do{++checks;if(!(c)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
bool Near(float a,float b){return std::abs(a-b)<1e-5f;}
bool Near(Vec a,Vec b){return Length(a-b)<1e-5f;}
const Frame wheel{{1,2,3},{1,0,0},{0,1,0},{0,0,-1},.2f};
void Save(Vec v,float out[3]){out[0]=v.x;out[1]=v.y;out[2]=v.z;}
// Independent matrix-to-quaternion conversion for synthetic controller poses.
void Basis(Vec x,Vec y,float out[4]){
    const Vec z=Cross(x,y);const float trace=x.x+y.y+z.z;
    if(trace>0){const float s=std::sqrt(trace+1.f)*2;out[3]=s/4;out[0]=(y.z-z.y)/s;out[1]=(z.x-x.z)/s;out[2]=(x.y-y.x)/s;}
    else if(x.x>y.y&&x.x>z.z){const float s=std::sqrt(1.f+x.x-y.y-z.z)*2;out[3]=(y.z-z.y)/s;out[0]=s/4;out[1]=(y.x+x.y)/s;out[2]=(z.x+x.z)/s;}
    else if(y.y>z.z){const float s=std::sqrt(1.f+y.y-x.x-z.z)*2;out[3]=(z.x-x.z)/s;out[0]=(y.x+x.y)/s;out[1]=s/4;out[2]=(z.y+y.z)/s;}
    else{const float s=std::sqrt(1.f+z.z-x.x-y.y)*2;out[3]=(x.y-y.x)/s;out[0]=(z.x+x.z)/s;out[1]=(z.y+y.z)/s;out[2]=s/4;}
}
xr::HandPose Hand(int index,Vec point,const Frame& frame=wheel,Vec fingers={0,1,0}){
    xr::HandPose hand;hand.valid=true;hand.trigger=1;
    const Vec forward=frame.right*fingers.x+frame.up*fingers.y;
    Basis(frame.normal*(index==0?-1.f:1.f),forward,hand.gripOri);
    Save(frame.World(point)-forward*kPalmForward-frame.normal*kPalmUp,hand.gripPos);
    CHECK(Tracked(hand,index).valid);
    CHECK(Near(PalmPoint(Tracked(hand,index)),frame.World(point)));
    return hand;
}
void Step(HandState& state,Vec p,int hand=0,float grip=0,float dt=.08f){state.Step(wheel,Hand(hand,p),hand,true,grip,dt);}
#include "horn_integration.inc"
Vec Skin(const handskeleton::Vertex& vertex,const handskeleton::Palette& palette){
    float p[3]{};
    for(int j=0;j<8;++j)for(int a=0;a<3;++a){const auto& m=palette[vertex.joints[j]];p[a]+=vertex.weights[j]*(m[12+a]+m[a]*vertex.pos[0]+m[4+a]*vertex.pos[1]+m[8+a]*vertex.pos[2]);}
    return {p[0],-p[2],p[1]}; // exactly the skeletal vertex shader conversion
}
int main(int argc,char** argv){
    CHECK(argc==2);CHECK(wheel.Valid());
    for(int h=0;h<2;++h){
        HandState state;
        Step(state,{0,0,.031f},h);CHECK(!state.visual.attached);
        Step(state,{0,0,.02999f},h);CHECK(state.visual.attached&&!state.visual.pressed);
        CHECK(Near(state.visual.weight,1));
        const Pose pinned=AttachedPose(wheel,state.visual);
        CHECK(pinned.valid&&Near(PalmPoint(pinned),wheel.World({0,0,.02999f})));
        CHECK(Near(pinned.up,wheel.normal));
        Step(state,{0,0,.004f},h,1);CHECK(state.visual.pressed&&state.justPressed);
        Step(state,{0,0,.007f},h,1);CHECK(state.visual.pressed&&!state.justPressed);
        Step(state,{0,0,.011f},h,1);CHECK(!state.visual.pressed&&state.visual.attached);
        Step(state,{0,0,.004f},h,1);CHECK(state.visual.pressed&&state.justPressed);
        Step(state,{.039f,0,.004f},h,1);CHECK(state.visual.attached&&Near(state.visual.point.x,0));
        Step(state,{.041f,0,.004f},h,1,.02f);CHECK(!state.visual.attached&&!state.visual.pressed);
        CHECK(state.visual.weight>0&&state.visual.weight<1);
        Step(state,{.041f,0,.004f},h);CHECK(!state.visual.attached); // no immediate recapture on another part of the button
        Step(state,{0,0,.1f},h);Step(state,{0,0,.02f},h);CHECK(state.visual.attached);
        Step(state,{0,0,.081f},h);CHECK(!state.visual.attached);
        Step(state,{0,0,.02f},h);CHECK(state.visual.attached);
        Step(state,{0,0,-.041f},h);CHECK(!state.visual.attached&&!state.visual.pressed);
        state.Reset();Step(state,{0,0,.02f},h);CHECK(!state.visual.attached);
        Step(state,{0,0,.04f},h);Step(state,{0,0,.02f},h);CHECK(state.visual.attached);
        auto raw=Hand(h,{0,0,.02f});raw.valid=false;
        state.Step(wheel,raw,h,true,0,.08f);CHECK(!state.visual.attached&&state.visual.weight==0);
        Step(state,{0,0,.02f},h);CHECK(!state.visual.attached); // tracking recovery needs a fresh approach
    }
    {HandState s;Step(s,{0,0,.02f},0,.31f);CHECK(!s.visual.attached);Step(s,{0,0,.02f},0,.3f);CHECK(s.visual.attached);}
    for(Vec p:{Vec{0,0,-.006f},Vec{.121f,0,0},Vec{0,0,.031f}}){HandState s;Step(s,p);CHECK(!s.visual.attached);}
    {HandState s;Step(s,{.1f,0,0});CHECK(s.visual.attached);CHECK(Near(s.visual.point.x,.09f));}
    for(int h=0;h<2;++h){
        HandState s;Step(s,{0,0,.02f},h);
        const auto original=Hand(h,{0,0,.02f});
        // Pure wrist turns, including 180 degrees, cannot translate/release the palm.
        for(float angle:{.5f,1.5f,3.14159265f,-1.2f}){
            auto raw=Hand(h,{0,0,.02f},wheel,{std::sin(angle),std::cos(angle),0});
            for(int a=0;a<3;++a)raw.gripPos[a]=original.gripPos[a];
            s.Step(wheel,raw,h,true,1,.08f);CHECK(s.visual.attached);
            const auto pose=AttachedPose(wheel,s.visual);
            CHECK(Near(PalmPoint(pose),wheel.World({0,0,.02f})));
            CHECK(Near(pose.up,wheel.normal));
            CHECK(Near(pose.forward,wheel.right*std::sin(angle)+wheel.up*std::cos(angle)));
        }
        // Tilt is filtered out; it does not become a button press or a release.
        auto tilted=original;
        Basis(Rotate(wheel.normal*(h==0?-1.f:1.f),wheel.right,.7f),Rotate(wheel.up,wheel.right,.7f),tilted.gripOri);
        s.Step(wheel,tilted,h,true,0,.08f);CHECK(s.visual.attached&&!s.visual.pressed);
        CHECK(Near(AttachedPose(wheel,s.visual).up,wheel.normal));
    }
    for(int h=0;h<2;++h){
        HandState s;Step(s,{.02f,-.01f,.004f},h);
        for(float angle:{.2f,1.f,2.7f}){
            Frame moved=wheel;const Vec axis=Unit({1,2,3});
            moved.center={-3,5,1};moved.right=Rotate(wheel.right,axis,angle);moved.up=Rotate(wheel.up,axis,angle);moved.normal=Rotate(wheel.normal,axis,angle);
            auto hand=Hand(h,{.02f,-.01f,.004f},moved);
            s.Step(moved,hand,h,true,0,.08f);CHECK(s.visual.attached&&s.visual.pressed);
            const auto pose=AttachedPose(moved,s.visual);
            CHECK(Near(PalmPoint(pose),moved.World({.02f,-.01f,.004f})));
        }
    }
    {State s;for(int h=0;h<2;++h)Step(s.hands[h],{0,0,.004f},h);CHECK(s.Pressed()&&s.Claimed()==3);
        Step(s.hands[0],{0,0,.1f});CHECK(s.Pressed()&&s.Claimed()==2);s.Reset();CHECK(!s.Pressed()&&s.Claimed()==0);}
    for(int kind=0;kind<6;++kind){
        HandState s;Step(s,{0,0,.004f});auto raw=Hand(0,{0,0,.004f});Frame frame=wheel;
        float grip=0;bool available=true;const float nan=std::numeric_limits<float>::quiet_NaN();
        if(kind==0)raw.gripPos[0]=nan;if(kind==1)raw.gripOri[3]=nan;if(kind==2)frame.radius=nan;
        if(kind==3)grip=nan;if(kind==4)available=false;if(kind==5)for(auto& x:raw.gripOri)x=0;
        s.Step(frame,raw,0,available,grip,.08f);CHECK(!s.visual.attached&&!s.visual.pressed&&s.visual.weight==0);
    }
    {HandState s;auto raw=Hand(0,{0,0,.02f});Basis(wheel.normal*-1.f,wheel.up,raw.gripOri); // left hand: up points INTO wheel
        Basis(wheel.normal,wheel.up,raw.gripOri);s.Step(wheel,raw,0,true,0,.08f);CHECK(!s.visual.attached);}
    // Frame-rate independent transitions and finite, orthonormal turns.
    for(int hz:{72,90,120}){HandState s;for(int i=0;i<hz/2;++i)Step(s,{0,0,.02f},0,0,1.f/hz);CHECK(Near(s.visual.weight,1));}
    for(int h=0;h<2;++h){
        auto from=Tracked(Hand(h,{0,0,.02f}),h);Visual visual;visual.point={0,0,.02f};visual.fingers={0,-1,0};
        visual.mirrored=Dot(Cross(from.up,from.forward),from.right)<0;
        const auto to=AttachedPose(wheel,visual);
        for(float t:{0.f,.1f,.5f,.9f,.9999f,1.f}){auto p=BlendPose(from,to,t);CHECK(Finite(p.position)&&Finite(p.forward)&&Finite(p.up)&&Finite(p.right));CHECK(Near(Length(p.forward),1)&&Near(Length(p.up),1)&&Near(Length(p.right),1));CHECK(std::abs(Dot(p.forward,p.up))<1e-5f);CHECK(std::abs(Dot(p.forward,p.right))<1e-5f);CHECK(Dot(Cross(p.up,p.forward),p.right)*Dot(Cross(from.up,from.forward),from.right)>.99f);}
    }
    handskeleton::Library lib;CHECK(lib.Load(argv[1]));
    handskeleton::Palette palette;lib.Evaluate(handanimation::Select(1,1,{false,false,false,false,false,false,0,1}),0,0,palette);
    float minimum=100;int patch=0;
    for(const auto& vertex:lib.vertices){const Vec v=Skin(vertex,palette);if(v.x>.065f&&v.x<.09f&&std::abs(v.z)<.015f){minimum=std::min(minimum,v.y);++patch;}}
    CHECK(patch>10&&std::abs(minimum+kPalmUp)<.002f);
    for(int h=0;h<2;++h){
        Visual v;v.point={0,0,0};v.fingers={1,0,0};v.mirrored=h==0;const auto p=AttachedPose(wheel,v);
        for(const auto& vertex:lib.vertices){const Vec local=Skin(vertex,palette);
            const Vec world=p.position+p.forward*(local.x-.055f)-p.up*local.y+p.right*local.z;
            CHECK(Dot(world-wheel.center,wheel.normal)<.003f); // no mesh penetration beyond the palm patch tolerance
        }
    }
    // Animated openness interpolates actual bones rather than matrix palettes.
    handanimation::Context context;context.horn=.5f;lib.Evaluate(handanimation::Select(1,1,context),0,0,palette);
    for(const auto& m:palette)for(float value:m)CHECK(std::isfinite(value));
    {
        using namespace integration;
        using namespace savr::driving;
        WheelVisualState visual;visual.active=true;visual.radius=wheel.radius;
        Save(wheel.center,visual.center);Save(wheel.right,visual.right);Save(wheel.up,visual.up);Save(wheel.normal,visual.normal);
        savr::xr::HandPose poses[2]{Hand(0,{0,0,.02f}),Hand(1,{0,0,.2f})};
        Tick(visual,poses);CHECK(visual.horn[0].attached&&!g_hornPressed.load());
        poses[0]=Hand(0,{0,0,.004f});Tick(visual,poses);CHECK(g_hornPressed.load()&&haptics[0]==1);
        Tick(visual,poses);CHECK(haptics[0]==1);
        visual.interactionTarget[0]=3;Save(Read(poses[0].gripPos),visual.handlePosition[0]);
        Input input;input.grip[0]=1;Tick(visual,poses,input);
        CHECK(!g_grabbed[0]&&g_hornPressed.load()&&visual.horn[0].attached); // grip cannot steal a latched hand
        poses[0]=Hand(0,{0,0,.02f});Tick(visual,poses);CHECK(!g_hornPressed.load()&&visual.horn[0].attached);
        poses[0]=Hand(0,{0,0,.004f});Tick(visual,poses);CHECK(g_hornPressed.load()&&haptics[0]==2);
        for(int kind=0;kind<5;++kind){
            g_hornState=vehiclehorn::State{};g_grabbed[0]=false;visual.ignitionGrabbed[0]=visual.radioGrabbed[0]=visual.liftGrabbed[0]=false;
            if(kind==0)visual.ignitionGrabbed[0]=true;if(kind==1)visual.radioGrabbed[0]=true;if(kind==2)visual.liftGrabbed[0]=true;
            if(kind==3)g_grabbed[0]=true;
            input.grip[0]=kind==3?1.f:0.f;
            Tick(visual,poses,input,kind==4?1u:0u);CHECK(!g_hornPressed.load()&&!visual.horn[0].attached);
        }
        // Existing non-car wheel proximity input remains intact.
        poses[0].gripPos[0]=wheel.center.x;poses[0].gripPos[1]=wheel.center.y;poses[0].gripPos[2]=wheel.center.z;
        g_grabbed[0]=false;Tick(visual,poses,{},0,VEHICLE_BOAT);CHECK(g_hornPressed.load());
        visual.bike=true;Tick(visual,poses,{},0,VEHICLE_BIKE);CHECK(!g_hornPressed.load());
    }
    std::printf("PASS: %d horn state, frame, mirrored palm geometry and animation checks\n",checks);
}
