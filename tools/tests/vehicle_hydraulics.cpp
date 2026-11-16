#include "VehicleHydraulics.h"
#include "VehicleHydraulicsModel.h"
#include "VehicleHydraulicsHand.h"
#include "Symbols.h"
#include "HandSkeleton.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace savr::hydrocontrol;
namespace savr {GameSymbols g{};}
namespace savr::vrcam {bool IsStereoActive(){return true;}}
namespace savr::hydraulics {bool dancing{};bool DanceRunning(){return dancing;}}
namespace savr::driving {
std::atomic<void*> g_hydroManagedVehicle{nullptr};std::atomic<bool> g_hydroToggle{};bool g_hydroHooksReady{};
constexpr int MODE_IMMERSIVE=1;int mode=MODE_IMMERSIVE,debug{};bool allowed=true,third=false;
void* controlled{};
int ModeForAppearance(int){return mode;}int GetVehicleDebugMode(){return debug;}
bool VrGameplayInputAllowed(){return allowed;}bool ThirdPersonViewActive(){return third;}
void* GetControlledVehicle(){return controlled;}
void* InstallVerifiedTrampoline(void*,void*,const std::uint32_t*,const char*){return nullptr;}
// COMPATIBILITY_FUNCTION
// EQUIPMENT_FUNCTION
#include "VehicleHydraulicsRuntime.inl"
int hornCalls{},toggleCalls{},controlCalls{};
bool OriginalHorn(void*){++hornCalls;return true;}
void OriginalControl(void*){++controlCalls;if(OnHydraulicHorn(nullptr))++toggleCalls;}
void OriginalBeat(void*,void*){}
bool IsCar(int m){return m==400||m==567;}
}

void Capture(State& s,Hand h[2],bool four=false){
    h[0]={Grip(0,0),{},0,true,true};h[1]={};
    s.Step(h,.02f,true,four);h[0].grip=1;s.Step(h,.02f,true,four);CHECK(s.owner==0);
}
void Detents(){
    for(int slot=1;slot<=8;++slot){State s;Hand h[2];Capture(s,h);const auto d=Direction(slot);
        h[0].point=Grip(d.x*kTravel,d.y*kTravel);s.Step(h,.02f,true,false);CHECK(s.slot==slot);CHECK(Length(s.Command()-d)<1e-6f);
        const int next=slot==8?1:slot+1;const auto n=Direction(next);h[0].point=Grip(n.x*kTravel,n.y*kTravel);s.Step(h,.02f,true,false);CHECK(s.slot==slot);
        h[0].point=Grip(0,0);s.Step(h,.02f,true,false);CHECK(s.slot==0&&Length(s.Command())==0);
        h[0].point=Grip(n.x*kTravel,n.y*kTravel);s.Step(h,.02f,true,false);CHECK(s.slot==next);
        h[0].point=Grip(n.x*(kTravel+.01f),n.y*(kTravel+.01f));s.Step(h,.02f,true,false);CHECK(s.slot==next&&Length(s.Command()-n)<1e-6f);
        h[0].grip=0;s.Step(h,.02f,true,false);CHECK(s.owner==-1&&s.slot==0&&Length(s.Command())==0);
        for(int i=0;i<80;++i)s.Step(h,.02f,true,false);CHECK(std::hypot(s.visualX,s.visualY)<1e-5f);
    }
    for(int i=0;i<8;++i){State s;Hand h[2];Capture(s,h,true);const auto d=Direction(i+1);
        h[0].point=Grip(d.x*kTravel,d.y*kTravel);s.Step(h,.02f,true,true);CHECK(s.slot%2==1);
        s.Step(h,.02f,true,false);CHECK(s.owner==-1&&s.slot==0); // mission boundary cannot send a beat
    }
    State s;Hand h[2];Capture(s,h);h[0].available=false;s.Step(h,.02f,true,false);CHECK(s.owner==-1);
    h[0].available=true;s.Step(h,.02f,true,false);CHECK(s.owner==-1); // held grip cannot steal it back
    h[0].grip=0;s.Step(h,.02f,true,false);h[0].grip=1;s.Step(h,.02f,true,false);CHECK(s.owner==0);
    h[0].point.x=std::numeric_limits<float>::quiet_NaN();s.Step(h,.02f,true,false);CHECK(s.owner==-1&&s.slot==0);
    State held;Hand q[2]{{Grip(0,0),{},1,true,true},{}};held.Step(q,.02f,true,false);CHECK(held.owner==-1);
    State path;Hand points[2];Capture(path,points);points[0].point=Grip(0,kTravel);
    for(int i=0;i<8;++i)path.Step(points,.01f,true,false);
    points[0].point=Grip(0,0);path.Step(points,.005f,true,false);points[0].point=Grip(kTravel,0);
    bool sawCenter=false;
    for(int i=0;i<12;++i){path.Step(points,.005f,true,false);
        if(std::hypot(path.visualX,path.visualY)<1e-6f)sawCenter=true;
        if(path.visualX>.001f)CHECK(sawCenter);}
    CHECK(sawCenter&&path.slot==3);
    std::puts("PASS detents: nine positions; neutral before change; fixed amplitude with slack; four-way mission; fresh grip and invalid/occupied hand");
}
void Buttons(){
    Button b;b.Step({0,0,-.005f},true,true,.02f);CHECK(!b.clicked&&!b.pressed);
    b.Step({0,0,.03f},true,true,.1f);CHECK(b.hover==1&&!b.clicked);
    b.Step({0,0,-.005f},true,true,.02f);CHECK(b.clicked&&b.pressed);
    b.Step({0,0,-.005f},true,true,.02f);CHECK(!b.clicked&&b.pressed);
    b.Step({0,0,.02f},true,true,.02f);b.Step({0,0,-.005f},true,true,.02f);CHECK(b.clicked);
    b.Step({0,0,-.005f},false,true,.02f);b.Step({0,0,-.005f},true,true,.02f);CHECK(!b.clicked&&!b.pressed);
    b.Step({0,0,.03f},true,true,.02f);b.Step({.04f,0,.01f},true,true,.02f);b.Step({0,0,-.005f},true,true,.02f);CHECK(!b.clicked);
    b.Step({0,0,.03f},true,true,.02f);b.Step({0,0,-.005f},true,false,.02f);CHECK(!b.clicked);
    Button hover;hover.Step({.044f,0,.079f},true,true,.1f);CHECK(hover.pointing&&hover.hover==1);
    hover.Step({.050f,0,.085f},true,true,.02f);CHECK(hover.pointing&&hover.hover==1); // exit hysteresis
    hover.Step({0,0,-.006f},true,true,.02f);CHECK(hover.pointing&&hover.hover==1&&!hover.clicked);
    hover.Step({.065f,0,.02f},true,true,.1f);CHECK(!hover.pointing&&hover.hover==0);
    std::puts("PASS button: approach is gesture only; front arming, physical travel, withdrawal, no repeats, side/rear/tracking/occupied rejection");
}
void MovingCockpit(){
    for(int hand=0;hand<2;++hand)for(int slot=1;slot<=8;++slot){
        State s;InputFrame start{{.3f,.2f,-.1f}},moving=start;
        Hand h[2]{};h[hand]={start.World(Grip(0,0)),{},0,true,true};
        s.StepTracked(h,start,.02f,true,false,7);h[hand].grip=1;s.StepTracked(h,start,.02f,true,false,7);CHECK(s.owner==hand);
        const auto d=Direction(slot);h[hand].point=start.World(Grip(d.x*kTravel,d.y*kTravel));
        s.StepTracked(h,start,.02f,true,false,7);CHECK(s.slot==slot);
        for(int i=0;i<100;++i){const float angle=std::sin(i*.1f)*.8f;
            moving.pivot={.3f+std::sin(i*.3f),.2f+std::cos(i*.2f),-.1f+std::sin(i*.15f)*.6f};
            moving.right=Swing({1,0,0},angle,-angle*.4f);moving.forward=Swing({0,1,0},angle,-angle*.4f);moving.up=Swing({0,0,1},angle,-angle*.4f);
            s.StepTracked(h,moving,.02f,true,false,7);CHECK(s.owner==hand&&s.slot==slot&&Length(s.Command()-d)<1e-5f);}
        h[hand].point=start.World(Grip(0,0));s.StepTracked(h,moving,.02f,true,false,7);CHECK(s.slot==0&&s.owner==hand);
        h[hand].grip=0;s.StepTracked(h,moving,.02f,true,false,7);CHECK(s.owner==-1);
        for(int i=0;i<20;++i)s.StepTracked(h,moving,.02f,true,false,7);
        // A new grab uses the new cockpit, not the old frozen frame.
        h[hand].point=moving.World(Grip(0,0));h[hand].grip=1;s.StepTracked(h,moving,.02f,true,false,7);CHECK(s.owner==hand&&s.slot==0);
        s.StepTracked(h,moving,.02f,true,false,8);CHECK(s.owner==-1&&s.slot==0); // changed tracking origin
        s.StepTracked(h,moving,.02f,true,false,8);CHECK(s.owner==-1); // fresh squeeze required
    }
    std::puts("PASS frozen grab: both hands/eight detents under 1 m chassis motion and roll; neutral/release/regrab; tracking epoch reset");
}
void AttachedHand(){
    using namespace savr::radio;
    savr::hydrocontrol::HandAttachment s;HandPose tracked{{.2f,.3f,.4f},{1,0,0},{0,1,0},{0,0,1}};
    HandFrame frame{{.2f,.3f,.4f},{1,0,0},{0,1,0},{0,0,1}};
    const auto first=s.Grab(1,frame,tracked);CHECK(Dot(first.position-tracked.position,first.position-tracked.position)<1e-10f);
    frame.center.x+=.4f;const auto attached=s.Grab(1,frame,tracked);CHECK(std::abs(attached.position.x-.6f)<1e-6f);
    const auto released=s.Free(tracked,.02f);CHECK(std::abs(released.position.x-attached.position.x)<1e-6f);
    float previous=released.position.x;
    for(int i=0;i<10;++i){const auto p=s.Free(tracked,.02f);CHECK(p.position.x<=previous+1e-6f);previous=p.position.x;}
    CHECK(std::abs(previous-tracked.position.x)<1e-6f);
    s.Reset();CHECK(std::abs(s.Free(tracked,.02f).position.x-tracked.position.x)<1e-6f);
    std::puts("PASS visual hand follows moving ball; release returns continuously to tracked wrist in 120 ms");
}
void Runtime(){
    using namespace savr;using namespace savr::driving;
    alignas(16) std::array<unsigned char,0xc68> vehicle{},other{};
    controlled=vehicle.data();g_hydroHooksReady=true;int model=400;std::memcpy(vehicle.data()+0x32,&model,2);
    static void* models[612]{};models[400]=vehicle.data();g.CModelInfo_ms_modelInfoPtrs=models;g.CModelInfo_IsCarModel=IsCar;
    auto& flags=*reinterpret_cast<std::uint32_t*>(vehicle.data()+0x4b8);
    flags=0;CHECK(HydraulicsCompatible(400));CHECK(!HydroEquipment(vehicle.data()));
    flags=0x40000;CHECK(!HydraulicsCompatible(400));flags=0x20000;CHECK(HydroEquipment(vehicle.data()));
    CHECK(!HydraulicsCompatible(567)&&!HydraulicsCompatible(399));
    g_originalHydraulicHorn=OriginalHorn;g_originalHydraulicControl=OriginalControl;
    CHECK(OnHydraulicHorn(nullptr)&&hornCalls==1);g_hydroManagedVehicle.store(controlled);
    OnHydraulicControl(controlled);CHECK(controlCalls==1&&toggleCalls==0&&hornCalls==1);
    g_hydroToggle=true;OnHydraulicControl(controlled);CHECK(toggleCalls==1&&!g_hydroToggle);
    OnHydraulicControl(controlled);CHECK(toggleCalls==1);
    OnHydraulicControl(other.data());CHECK(toggleCalls==2&&hornCalls==2); // ordinary caller still sees original horn
    debug=1;g_hydroToggle=true;OnHydraulicControl(controlled);CHECK(controlCalls==4&&!g_hydroToggle);debug=0;
    allowed=false;OnHydraulicControl(controlled);CHECK(controlCalls==4);allowed=true;
    alignas(16) std::array<unsigned char,0xd94> hardware{};std::array<unsigned char,0xac> output{};
    for(int i=0;i<20;++i){std::uint32_t time=999-i,key=11;std::memcpy(output.data()+i*8,&time,4);std::memcpy(output.data()+i*8+4,&key,4);}
    std::uint32_t last=16,index=42;std::memcpy(output.data()+0xa4,&last,4);std::memcpy(output.data()+0xa8,&index,4);
    std::memcpy(hardware.data()+0xce8,output.data(),output.size());g_originalHydraulicBeat=OriginalBeat;
    hydraulics::dancing=false;OnHydraulicBeat(hardware.data(),output.data());CHECK(*reinterpret_cast<std::uint32_t*>(output.data()+4)==11);
    hydraulics::dancing=true;OnHydraulicBeat(hardware.data(),output.data());CHECK(*reinterpret_cast<std::uint32_t*>(output.data()+4)==9);
    CHECK(*reinterpret_cast<std::uint32_t*>(hardware.data()+0xcec)==9);
    CHECK(*reinterpret_cast<std::uint32_t*>(output.data()+0xa4)==14);
    CHECK(*reinterpret_cast<std::uint32_t*>(output.data())==999&&*reinterpret_cast<std::uint32_t*>(output.data()+0xa8)==42);
    for(std::uint32_t key:{1u,2u,3u,4u,9u,10u,13u,14u})CHECK(MissionKey(key)==key);
    std::puts("PASS native adapter: compatibility vs equipment; scoped reverse; unrelated horn; preview/menu blocking; native/hint four-way beat windows with intact timing/index");
}
std::vector<unsigned char> Read(const char* path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void Geometry(const char* meshPath,const char* fixturePath){
    const auto bytes=Read(meshPath);Model model;CHECK(model.Load(bytes.data(),bytes.size()));CHECK(model.indices.size()/3==4912);
    int ball=0,labels=0;
    for(const auto& v:model.vertices){
        if(v.uv[0]>=0){++labels;CHECK(v.role==0&&std::abs(v.position[2]-.0148f)<1e-6f);}
        if(v.role==1&&std::abs(Length(Point{v.position[0],v.position[1],v.position[2]-.055f})-.0225f)<1e-6f){
            ++ball;CHECK(v.uv[0]<0&&v.uv[1]<0&&v.color[0]>.7f);}}
    CHECK(ball>300&&labels==24);
    const auto fixture=Read(fixturePath);CHECK(fixture.size()>12);std::uint32_t header[3];std::memcpy(header,fixture.data(),12);CHECK(header[0]==0x46525948);
    std::size_t offset=12;float worst=0;
    for(unsigned s=0;s<header[1];++s){float xy[2];std::memcpy(xy,fixture.data()+offset,8);offset+=8;
        for(unsigned i=0;i<header[2];++i){std::uint32_t index;float expected[3];std::memcpy(&index,fixture.data()+offset,4);std::memcpy(expected,fixture.data()+offset+4,12);offset+=16;
            CHECK(index<model.vertices.size());auto v=Model::Deform(model.vertices[index],xy[0],xy[1],0);
            for(int a=0;a<3;++a){const float error=std::abs(v.position[a]-expected[a]);worst=std::max(worst,error);CHECK(error<2e-6f);}
            float length=0;for(float n:v.normal)length+=n*n;CHECK(std::abs(length-1)<1e-4f);}}
    CHECK(offset==fixture.size());auto invalid=bytes;invalid[0]=0;CHECK(!model.Load(invalid.data(),invalid.size()));
    std::printf("PASS native deformation vs independent Blender evaluated vertices; worst error %.8f m\n",worst);
}
void PointPose(const char* path){
    using namespace savr;handskeleton::Library library;CHECK(library.Load(path));
    handskeleton::Palette fist{},point{},blend{},forced{};
    library.Evaluate({handanimation::Gesture::Fist,handanimation::Gesture::Fist,1,0},0,0,fist);
    library.Evaluate({handanimation::Gesture::Point,handanimation::Gesture::Point,1,0},0,0,point);
    handanimation::Selection override{handanimation::Gesture::Fist,handanimation::Gesture::Fist,1,0};
    override.pointBlend=1;library.Evaluate(override,0,0,forced);
    for(std::size_t j=0;j<library.joints.size();++j)for(int a=0;a<16;++a)CHECK(std::abs(forced[j][a]-point[j][a])<1e-5f);
    override.pointBlend=.5f;library.Evaluate(override,0,0,blend);
    handskeleton::Vec a{},b{},c{};CHECK(library.IndexTip(fist,a)&&library.IndexTip(point,b)&&library.IndexTip(blend,c));
    const auto distance=[](const auto& u,const auto& v){float s=0;for(int i=0;i<3;++i)s+=(u[i]-v[i])*(u[i]-v[i]);return std::sqrt(s);};
    CHECK(distance(a,b)>.01f&&distance(a,c)>.001f&&distance(b,c)>.001f);
    for(int hand=0;hand<2;++hand){xr::HandPose h{};h.valid=true;h.aimValid=true;
        h.gripPos[0]=.2f;h.gripPos[1]=-.1f;h.gripPos[2]=.4f;
        h.gripOri[1]=std::sin(.3f);h.gripOri[3]=std::cos(.3f);
        h.aimOri[1]=std::sin(.5f);h.aimOri[3]=std::cos(.5f);
        const auto pose=handrender::TrackedPose(h,hand);const auto tip=handrender::MeshPoint(pose,{b[0],b[1],b[2]});
        // Independent application of the renderer's mesh matrix to real GLB tip.
        const float m[16]{pose.forward.x,pose.forward.y,pose.forward.z,0,-pose.up.x,-pose.up.y,-pose.up.z,0,
            pose.right.x,pose.right.y,pose.right.z,0,pose.position.x-.055f*pose.forward.x,pose.position.y-.055f*pose.forward.y,pose.position.z-.055f*pose.forward.z,1};
        const float actual[3]{tip.x,tip.y,tip.z};
        for(int i=0;i<3;++i)CHECK(std::abs(actual[i]-(m[i]*b[0]+m[4+i]*b[1]+m[8+i]*b[2]+m[12+i]))<1e-6f);
    }
    std::puts("PASS automatic point pose: real hand asset, full override and continuous intermediate fingertip pose");
}
int main(int argc,char** argv){CHECK(argc==4);Detents();Buttons();MovingCockpit();AttachedHand();Runtime();Geometry(argv[1],argv[2]);PointPose(argv[3]);}
