#include "VehicleDoorMotion.h"
#include "VehicleDoors.h"
#include "Symbols.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
#define LOGI(...) ((void)0)
namespace savr {
GameSymbols g{};
namespace xr {
struct HandPose { bool valid{true};float gripPos[3]{};float grip{}; };
vehicledoors::MarkerFrame published;
void SetVehicleDoorMarkers(const vehicledoors::MarkerFrame& f) { published=f; }
void TriggerHaptic(int,float,float,float) {}
}
namespace physicalweapon {
unsigned mask{},support{};
unsigned HeldHandMaskRelaxed() { return mask; }
bool IsSupportHand(int h) { return (support&(1u<<h))!=0; }
}
namespace basketball { unsigned held{};unsigned HeldHandMask() { return held; } }
namespace driving {
struct WheelVisualState { bool grabbed[2]{}; };
unsigned grabbed{};
void GetWheelVisualState(WheelVisualState* out) { for(int i=0;i<2;++i)out->grabbed[i]=(grabbed&(1u<<i))!=0; }
}
namespace vrcam {
bool ground=true,tracking=true;
vehicledoors::Vec eye{-1.7f,-.3f,1.3f};
vehicledoors::Vec rebase{};
bool GroundWalkControlsActive() { return ground; }
bool VehicleDoorControlsActive() { return ground; }
bool WorldPointToTracking(const float in[3],float out[3]) {
    for (int i=0;i<3;++i) out[i]=in[i]-(&rebase.x)[i];return tracking;
}
bool TrackingPointToWorld(const float in[3],float out[3]) { std::memcpy(out,in,12);return tracking; }
bool GetHeadWorldRay(float out[3],float[3]) { std::memcpy(out,&eye,12);return tracking; }
}
}
// PRODUCTION_ADAPTER

using namespace savr;
using namespace savr::vehicledoors;
namespace {
alignas(8) char poolMemory[0x20]{},pedMemory[0x100]{},padMemory[0x200]{};
std::vector<char> vehicles(110*0xc68);
std::uint8_t flags[110];
alignas(8) char matrix[0x40]{},frames[4][0x40]{};
unsigned status[6]{};
bool locked{},busy{},wall{},entering{},paused{};
void* seated{};
int writes{},nativeSwings{},invalidWrites{};
void* vehicle=vehicles.data();
void* poolPointer=poolMemory;

void StoreBasis(void* dest,Basis b) {
    Write(dest,0,b.right);Write(dest,0x10,b.forward);Write(dest,0x20,b.up);Write(dest,0x30,b.position);
}
void RefreshFrames() {
    const Basis car=Matrix(matrix);
    for (int i=0;i<4;++i) {
        const Vec pivot{(i&1)?1.f:-1.f,i<2?.6f:-.6f,.4f};
        const float angle=Read<float>(vehicle,0x770+(i+2)*0x18+0xc);
        const Basis panel{car.Vector(Rotate({1,0,0},angle)),car.Vector(Rotate({0,1,0},angle)),car.up,car.Point(pivot)};
        StoreBasis(frames[i],panel);
    }
}
void FakeOpen(void* v,void*,int,int door,float ratio,bool) {
    if (flags[0]&0x80||v!=vehicle) { ++invalidWrites;return; }
    ++writes;
    const float open=Read<float>(v,0x770+door*0x18);
    Write(v,0x770+door*0x18+0xc,open*ratio);
    if (ratio==0) status[door]&=~1u;
    RefreshFrames();
}
void FakeSwing(void*,int,int) { ++nativeSwings; }
bool FakeLocked(const void*,const void*) { return !locked; }
bool FakeBusy(const void*,int,int) { return busy; }
unsigned FakeStatus(const void*,int d) { return status[d]; }
void FakeSetStatus(void*,int d,unsigned s) { status[d]=s; }
void* FakeLtm(void* frame) { return frame; }
bool FakeLos(const GameSymbols::Vec3*,const GameSymbols::Vec3*,void*,void** hit,bool,bool,bool,bool,bool,bool,bool,bool) {
    *hit=wall?static_cast<void*>(pedMemory):vehicle;return true;
}
void Setup() {
    Reset();g_epoch=5;g_updated=Now()-.02;g_ready=true;
    std::memset(flags,0x80,sizeof(flags));flags[0]=3;
    std::memset(vehicle,0,0xc68);std::memset(status,0,sizeof(status));
    std::memset(padMemory,0,sizeof(padMemory));
    Write(poolMemory,0,vehicle);Write(poolMemory,8,static_cast<const std::uint8_t*>(flags));
    Write(poolMemory,0x10,110);Write<std::uint8_t>(poolMemory,0x18,1);
    StoreBasis(matrix,{});Write(vehicle,0x18,static_cast<void*>(matrix));
    Write<std::uint8_t>(vehicle,0x5a,2|(4<<3));Write<std::uint16_t>(vehicle,0x32,400);
    for (int i=0;i<4;++i) {
        Write(vehicle,0x800+kNodes[i]*8,static_cast<void*>(frames[i]));
        Write<float>(vehicle,0x770+(i+2)*0x18,(i&1)?1.2f:-1.2f);
        Write<std::uint8_t>(vehicle,0x770+(i+2)*0x18+0xa,2);
    }
    RefreshFrames();
    g.CPools_ms_pVehiclePool=&poolPointer;
    g.FindPlayerPed=[](int)->void*{return pedMemory;};
    g.FindPlayerVehicle=[](int,bool)->void*{return seated;};
    g.CVehicle_IsDriver=[](const void*,const void*){return true;};
    g.CPad_GetPad=[](int)->void*{return padMemory;};
    g.CVehicle_GetVehicleAppearance=[](const void*){return 1;};
    g.PlayerIsEnteringCar=[](){return entering;};
    g.CTimer_m_UserPause=&paused;
    g.CWorld_ProcessLineOfSight=FakeLos;
    g_open=FakeOpen;g_swing=FakeSwing;g_canOpen=FakeLocked;g_inUse=FakeBusy;
    g_status=FakeStatus;g_setStatus=FakeSetStatus;g_frameLtm=FakeLtm;
    locked=busy=wall=entering=paused=false;vrcam::ground=vrcam::tracking=true;
    physicalweapon::mask=physicalweapon::support=basketball::held=0;
    driving::grabbed=0;seated=nullptr;
    writes=nativeSwings=invalidWrites=0;
    vrcam::eye={-1.7f,-.3f,1.3f};
    vrcam::rebase={};
}
xr::HandPose hands[2];
void Tick(bool blocked=false,std::uint64_t epoch=5,bool valid=true) {
    g_updated=Now()-.02;
    Update(blocked,hands,valid,epoch);
}
void AtHandle(int hand,int index=0) {
    const Vec pivot{(index&1)?1.f:-1.f,index<2?.6f:-.6f,.4f};
    const Vec p=pivot+Vec{(index&1)?.065f:-.065f,-1.2f*.82f,.30f};
    std::memcpy(hands[hand].gripPos,&p,12);
}
void Ready(int hand=0) {
    hands[0]={};hands[1]={};AtHandle(hand);
    Tick(); // open hand arms the edge
    CHECK(xr::published.count==2); // only the near side, automatically
}
void GrabIt(int hand=0) {
    Ready(hand);hands[hand].grip=1;Tick();
    CHECK(ClaimedHands()==(1u<<hand));
}
}
int main() {
    for (float sign:{-1.f,1.f}) for (int hz:{36,72,90}) {
        Motion m;const float dt=1.f/hz;
        CHECK(m.Begin({},{0,-1,0},sign*.5f,0,sign*1.5f));
        float phase=0;
        for (int i=0;i<hz/10;++i) { phase+=sign*3.f*dt;CHECK(m.Step(Rotate({0,-1,0},phase),dt)); }
        CHECK(m.ReleaseVelocity(dt)*sign>0);
        CHECK(std::abs(m.ReleaseVelocity(dt)*sign/dt-3.f)<.7f);
        for (int i=0;i<hz/3;++i) CHECK(m.Step(Rotate({0,-1,0},phase),dt));
        CHECK(m.ReleaseVelocity(dt)==0); // no stale impulse after holding still
        CHECK(m.ReleaseVelocity(.5f)==0);
    }
    // Both hinge signs, no jump at grab and correct closing from partly open.
    for (float sign:{-1.f,1.f}) {
        Motion m;const Vec hand{0,-1,0};
        CHECK(m.Begin({},hand,sign*.4f,0,sign*1.2f));
        CHECK(m.Step(hand,.02f)&&std::abs(m.angle-sign*.4f)<1e-5f);
        CHECK(m.Step(Rotate(hand,sign*.05f),.02f));
        CHECK(std::abs(m.angle-sign*.45f)<1e-4f);
        CHECK(m.Step(hand,.02f)&&std::abs(m.angle-sign*.4f)<1e-4f);
    }
    Motion boundary;CHECK(boundary.Begin({},Rotate({1,0,0},3.13f),.3f,0,1.2f));
    CHECK(boundary.Step(Rotate({1,0,0},-3.13f),.02f));CHECK(boundary.angle>.3f&&boundary.angle<.34f);
    CHECK(!boundary.Step({NAN,0,0},.02f));CHECK(!boundary.Step({1,0,0},.5f));
    CHECK(!boundary.Begin({},{0,0,0},0,0,1));
    for (float sign:{-1.f,1.f}) {
        Motion slam;CHECK(slam.Begin({},{0,-1,0},sign*.8f,0,sign*1.2f));
        slam.velocity=-sign*2.f;
        CHECK(std::abs(slam.ReleaseVelocity(1.f/90))>.1f);
        slam.velocity=-sign*.5f;CHECK(std::abs(slam.ReleaseVelocity(1.f/90))<.1f);
        slam.velocity=-sign*.7f;CHECK(std::abs(slam.ReleaseVelocity(1.f/90))>.1f);
        slam.velocity=sign*.7f;CHECK(std::abs(slam.ReleaseVelocity(1.f/90))<.1f);
    }
    Motion stop;CHECK(stop.Begin({},{0,-1,0},1.19f,0,1.2f));
    CHECK(stop.Step(Rotate({0,-1,0},.1f),.02f)&&stop.angle==1.2f);
    CHECK(stop.Step(Rotate({0,-1,0},.05f),.02f)&&stop.angle<1.2f);
    // A rotated/moving car and its hand preserve the same local angle.
    Basis b{Rotate({1,0,0},.7f),Rotate({0,1,0},.7f),{0,0,1},{7,8,1}};
    CHECK(b.Valid());const Vec p{-.9f,.2f,.7f};CHECK(Length(b.Local(b.Point(p))-p)<1e-5f);
    const auto arrow=Arrow({{}, {1,0,0},{0,0,1},false});
    CHECK(arrow.size()==54);for (auto v:arrow) CHECK(Finite(v)&&v.x>=0&&v.x<=.181f);
    MarkerFrame f;f.count=1;f.epoch=3;CHECK(f.Usable(true,3)&&!f.Usable(true,4)&&!f.Usable(false,3));
    GripEdge edge;CHECK(!edge.Step(1,true));CHECK(!edge.Step(0,true));CHECK(edge.Step(1,true));
    CHECK(!edge.Step(0,false)&&!edge.Step(1,true));

    Setup();Ready();hands[0].grip=1;Tick();CHECK(ClaimedHands()==1);
    const float initial=Read<float>(vehicle,0x770+2*0x18+0xc);
    CHECK(initial==0);
    const Vec hinge{-1,.6f,.4f},hand{hands[0].gripPos[0],hands[0].gripPos[1],hands[0].gripPos[2]};
    const Vec pulled=hinge+Rotate(hand-hinge,-.05f);std::memcpy(hands[0].gripPos,&pulled,12);Tick();
    CHECK(Read<float>(vehicle,0x770+2*0x18+0xc)<-.04f&&status[2]==1);
    const float angle=Read<float>(vehicle,0x770+2*0x18+0xc);
    OnSwing(vehicle,10,2);OnSwing(vehicle,10,2);
    CHECK(nativeSwings==0&&Read<float>(vehicle,0x770+2*0x18+0xc)==angle);
    OnSwing(vehicle,8,3);CHECK(nativeSwings==1); // other door remains native
    hands[0].grip=0;Tick();CHECK(ClaimedHands()==0);
    OnSwing(vehicle,10,2);CHECK(nativeSwings==2); // release restores native physics
    Setup();GrabIt(1);CHECK(ClaimedHands()==2); // either controller
    Setup();GrabIt();
    const auto markerBefore=xr::published.markers[0].position;
    vrcam::rebase={1,2,3};PublishMarkers();
    CHECK(Length(xr::published.markers[0].position-(markerBefore-vrcam::rebase))<1e-5f);
    // A moving/rotating car cannot create a pull when the hand moves with it.
    Setup();GrabIt();
    StoreBasis(matrix,b);RefreshFrames();
    const Vec movedTogether=b.Point({hands[0].gripPos[0],hands[0].gripPos[1],hands[0].gripPos[2]});
    std::memcpy(hands[0].gripPos,&movedTogether,12);Tick();
    CHECK(ClaimedHands()==1&&std::abs(Read<float>(vehicle,0x770+2*0x18+0xc))<1e-5f);
    // The opposite side opens with the opposite signed angle.
    Setup();hands[0]={};hands[1]={};vrcam::eye={1.7f,-.3f,1.3f};AtHandle(1,1);Tick();
    CHECK(xr::published.count==2);hands[1].grip=1;Tick();CHECK(g_grab.target.door==3);
    const Vec rightPivot{1,.6f,.4f};
    const Vec rightHand{hands[1].gripPos[0],hands[1].gripPos[1],hands[1].gripPos[2]};
    const Vec rightPull=rightPivot+Rotate(rightHand-rightPivot,.05f);
    std::memcpy(hands[1].gripPos,&rightPull,12);Tick();
    CHECK(Read<float>(vehicle,0x770+3*0x18+0xc)>.04f&&status[3]==1);
    // A pre-opened door is acquired at its existing angle, with no snap shut.
    Setup();Ready();Write<float>(vehicle,0x770+2*0x18+0xc,-.4f);status[2]=1;RefreshFrames();
    const Vec partlyOpen=hinge+Rotate(hand-hinge,-.4f);
    std::memcpy(hands[0].gripPos,&partlyOpen,12);hands[0].grip=1;Tick();
    CHECK(ClaimedHands()==1&&std::abs(Read<float>(vehicle,0x770+2*0x18+0xc)+.4f)<1e-5f);
    // Opening the panel no longer makes the hint disappear behind its moving
    // outside normal while the player stays beside the body.
    Setup();hands[0]={};hands[1]={};Write<float>(vehicle,0x770+2*0x18+0xc,-1.f);status[2]=1;RefreshFrames();
    Tick();CHECK(xr::published.count>=1);
    Setup();Ready();hands[0].grip=1;Tick(true);Tick();CHECK(ClaimedHands()==0); // blocked squeeze cannot leak
    for (int gate=0;gate<8;++gate) {
        Setup();Ready();hands[0].grip=1;
        if (gate==0) locked=true;if (gate==1) busy=true;if (gate==2) wall=true;
        if (gate==3) physicalweapon::mask=1;if (gate==4) physicalweapon::support=1;
        if (gate==5) basketball::held=1;if (gate==6) Write<std::uint8_t>(padMemory,0x12d,1);
        if (gate==7) paused=true;
        Tick();CHECK(ClaimedHands()==0&&writes==0);
    }
    Setup();GrabIt();const int before=writes;flags[0]=4;Tick();CHECK(ClaimedHands()==0&&writes==before);
    Setup();GrabIt();flags[0]=0x80;OnSwing(vehicle,10,2);CHECK(ClaimedHands()==0&&invalidWrites==0);
    Setup();GrabIt();Tick(false,6);CHECK(ClaimedHands()==0);Tick(false,6);CHECK(ClaimedHands()==0);
    Setup();GrabIt();Tick(false,5,false);CHECK(ClaimedHands()==0);
    Setup();GrabIt();entering=true;Tick();CHECK(ClaimedHands()==0); // stock Y is still available
    Setup();Ready();status[2]=2;hands[0].grip=1;Tick();
    const Vec moved=hinge+Rotate(hand-hinge,-.05f);std::memcpy(hands[0].gripPos,&moved,12);Tick();
    CHECK(status[2]==3); // damaged door was opened, never repaired
    Setup();Ready();status[2]=4;hands[0].grip=1;Tick();CHECK(ClaimedHands()==0); // missing door
    // A seated driver can reach the inner handle; wheel ownership wins.
    Setup();seated=vehicle;vrcam::eye={-.35f,-.3f,1.3f};hands[0]={};hands[1]={};
    const Vec inner{-1.f+.34f,.6f-1.2f*.82f,.7f};
    std::memcpy(hands[0].gripPos,&inner,12);Tick();CHECK(xr::published.count==1);
    CHECK(xr::published.markers[0].direction.x>.9f); // arrow points into the cabin
    driving::grabbed=1;hands[0].grip=1;Tick();CHECK(ClaimedHands()==0);
    driving::grabbed=0;hands[0].grip=0;Tick();
    hands[0].gripPos[2]+=.3f;hands[0].grip=1;Tick();CHECK(ClaimedHands()==1); // larger seated reach
    seated=nullptr;Tick();CHECK(ClaimedHands()==0); // do not retain a grab across seat exit
    // A real release writes signed momentum; cancellation never does.
    Setup();GrabIt();
    for (int i=1;i<=5;++i) {
        const Vec pull=hinge+Rotate(hand-hinge,-.07f*i);
        std::memcpy(hands[0].gripPos,&pull,12);Tick();
    }
    hands[0].grip=0;Tick();
    CHECK(ClaimedHands()==0&&Read<float>(vehicle,0x770+2*0x18+0x14)<-.02f);
    Setup();GrabIt();g_grab.motion.velocity=-4;Tick(true);
    CHECK(Read<float>(vehicle,0x770+2*0x18+0x14)==0);
    std::puts("vehicle doors: production motion, momentum, selection, ownership, cancellation and native handoff passed");
}
