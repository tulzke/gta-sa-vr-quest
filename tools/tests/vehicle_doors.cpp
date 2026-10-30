#include "VehicleDoorMotion.h"
#include "VehicleDoors.h"
#include "VehicleDebugDrag.h"
#include "Driving.h"
#include <map>
#include <array>
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
namespace savr::npcgrab {
int stopsRequested{};unsigned claimedHands{};
void OnDoorOpening(void*) {++stopsRequested;}
unsigned ClaimedHands() {return claimedHands;}
}
namespace savr::vehicleentry { int SeatIndex(void* car,void* ped){ if(!car||!ped)return -2;void* v{};std::memcpy(&v,static_cast<char*>(car)+0x5a0,8);if(v==ped)return -1;for(int i=0;i<3;++i){std::memcpy(&v,static_cast<char*>(car)+0x5a8+i*8,8);if(v==ped)return i;}return -2;} unsigned ClaimedHands(){return 0;} void Update(bool,std::uint64_t) {} void Append(vehicledoors::MarkerFrame&) {} }
namespace savr {
GameSymbols g{};
namespace xr {
struct HandPose { bool valid{true};float gripPos[3]{};float grip{};float trigger{}; };
vehicledoors::MarkerFrame published;
void SetVehicleDoorMarkers(const vehicledoors::MarkerFrame& f) { published=f; }
void TriggerHaptic(int,float,float,float) {}
bool focused=true;bool IsSessionFocused() {return focused;}
}
namespace physicalweapon {
unsigned mask{},support{};
unsigned HeldHandMaskRelaxed() { return mask; }
bool IsSupportHand(int h) { return (support&(1u<<h))!=0; }
}
namespace basketball { unsigned held{};unsigned HeldHandMask() { return held; } }
namespace driving {
int debugMode{},saves{};
std::map<int,std::array<float,24>> corrections;
int GetVehicleDebugMode() {return debugMode;}
void GetDoorCalibration(int model,int door,bool inside,float out[3]) {
    for(int a=0;a<3;++a) out[a]=corrections[model][(door-2)*6+inside*3+a];
}
void SetDoorCalibration(int model,int door,bool inside,const float in[3]) {
    for(int a=0;a<3;++a) corrections[model][(door-2)*6+inside*3+a]=in[a];
}
void SaveDebugCalibration() {++saves;}
int GetActiveVehicleType() {return VEHICLE_CAR;}
int GetModeForVehicleType(int) {return MODE_IMMERSIVE;}
int GetActiveSeatSideCm() {return 0;}
int GetActiveSeatForwardCm() {return 0;}
int GetActiveSeatHeightCm() {return 0;}
int GetCurrentModelSeatSideCm(int) {return 0;}
int GetCurrentModelSeatForwardCm(int) {return 0;}
int GetCurrentModelSeatHeightCm(int) {return 0;}
int GetWheelCalibrationValue(int) {return 0;}
int radiusValue{},radiusWrites{};
void SetDebugWheelRadius(int,int value) {radiusValue=value;++radiusWrites;}
int positions[3]{},positionWrites{};
void SetDebugPosition(int,int,bool,const int value[3]) { std::copy_n(value,3,positions);++positionWrites; }

unsigned grabbed{};
bool GetWheelVisualState(WheelVisualState* out) {
    for(int i=0;i<2;++i)out->grabbed[i]=(grabbed&(1u<<i))!=0;
    out->dashAnchorValid=true;out->modelId=400;out->radius=.2f;out->center[0]=-.4f;out->center[1]=.2f;out->center[2]=1.f;return true;
}
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
bool GetNeutralVehicleEyeWorld(float out[3]) { const float p[]{-.4f,.2f,1.4f};std::memcpy(out,p,12);return tracking; }
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
    Write(vehicle,0x5a0,static_cast<void*>(pedMemory));
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
    driving::grabbed=0;driving::debugMode=0;driving::corrections.clear();driving::saves=0;xr::focused=true;seated=nullptr;
    writes=nativeSwings=invalidWrites=0;
    npcgrab::stopsRequested=0;
    npcgrab::claimedHands=0;
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
    Setup();
    auto* damage=static_cast<char*>(vehicle)+0x758;
    Write<std::uint8_t>(damage,11,1);g_driveDoorStatusReturn=1234;
    CHECK(DoorStatusAt(damage,2,1234)==1); // no player vehicle
    seated=vehicle;
    CHECK(DoorStatusAt(damage,2,1234)==0); // only the automatic-close call
    CHECK(DoorStatusAt(damage,2,1235)==1);
    Write<std::uint8_t>(damage,12,1);
    CHECK(DoorStatusAt(damage,3,1234)==1);
    CHECK(DoorStatusAt(damage,6,1234)==4);
    g_driveDoorStatusReturn=0;
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
    const auto reverse=ReverseArrow({{}, {1,0,0},{0,0,1},false});
    CHECK(std::abs(reverse.position.x-.18f)<1e-5f&&reverse.direction.x==-1);
    for (auto v:Arrow(reverse)) CHECK(v.x>=-.001f&&v.x<=.181f);
    for (float sign:{-1.f,1.f}) {
        ReleasedMotion thrown{sign*1.2f,0,sign*1.2f,-sign*.9f,true,false};
        for(int i=0;i<100&&!thrown.Done();++i) thrown.Step(.02f);
        CHECK(thrown.Done()&&thrown.angle==0);
        ReleasedMotion held{sign*.5f,0,sign*1.2f,sign*.5f,false,true};
        for(int i=0;i<200;++i) held.Step(.02f);
        CHECK(!held.Done()&&std::abs(held.angle)>.5f&&held.velocity==0);
    }
    MarkerFrame f;f.count=1;f.epoch=3;CHECK(f.Usable(true,3)&&!f.Usable(true,4)&&!f.Usable(false,3));
    GripEdge edge;CHECK(!edge.Step(1,true));CHECK(!edge.Step(0,true));CHECK(edge.Step(1,true));
    CHECK(!edge.Step(0,false)&&!edge.Step(1,true));

    Setup();Ready();hands[0].grip=1;Tick();CHECK(ClaimedHands()==1);
    CHECK(npcgrab::stopsRequested==1);
    npcgrab::claimedHands=1;Tick();CHECK(ClaimedHands()==0); // one hand cannot own both NPC and door
    Setup();Ready();npcgrab::claimedHands=3;hands[0].grip=1;Tick();CHECK(ClaimedHands()==0&&xr::published.count>0);
    Setup();Ready();npcgrab::claimedHands=2;hands[0].grip=1;Tick();CHECK(ClaimedHands()==1); // the free hand can still open the door
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
    CHECK(xr::published.markers[0].direction.x<-.9f); // arrow points toward the door
    CHECK(Length(xr::published.markers[0].position-(inner+Vec{.18f,0,0}))<1e-5f);
    driving::grabbed=1;hands[0].grip=1;Tick();CHECK(ClaimedHands()==0);
    driving::grabbed=0;hands[0].grip=0;Tick();
    hands[0].gripPos[2]+=.3f;hands[0].grip=1;Tick();CHECK(ClaimedHands()==1); // larger seated reach
    seated=nullptr;Tick();CHECK(ClaimedHands()==0); // do not retain a grab across seat exit
    // A released interior door remains open even if the game overwrites its angle.
    Setup();seated=vehicle;vrcam::eye={-.35f,-.3f,1.3f};hands[0]={};hands[1]={};
    std::memcpy(hands[0].gripPos,&inner,12);Tick();hands[0].grip=1;Tick();
    const Vec innerHinge{-1,.6f,.4f};
    for(int i=1;i<=4;++i) {
        const Vec opened=innerHinge+Rotate(inner-innerHinge,-.1f*i);
        std::memcpy(hands[0].gripPos,&opened,12);Tick();
    }
    hands[0].grip=0;Tick();
    CHECK(g_release.active&&g_release.inside);
    Write<float>(vehicle,0x770+2*0x18+0xc,0);OnSwing(vehicle,10,2);
    CHECK(Read<float>(vehicle,0x770+2*0x18+0xc)<-.3f);
    seated=nullptr;Tick();CHECK(!g_release.active);
    // If an open seated grab loses tracking or visibility, preserve its last
    // angle without imparting a throw, rather than letting native physics close it.
    for(int failure=0;failure<2;++failure) {
        Setup();seated=vehicle;vrcam::eye={-.35f,-.3f,1.3f};hands[0]={};hands[1]={};
        std::memcpy(hands[0].gripPos,&inner,12);Tick();hands[0].grip=1;Tick();
        const Vec opened=innerHinge+Rotate(inner-innerHinge,-.12f);
        std::memcpy(hands[0].gripPos,&opened,12);Tick();
        CHECK(ClaimedHands()==1);
        if(failure==0) hands[0].valid=false; else wall=true;
        Tick();CHECK(ClaimedHands()==0&&g_release.active&&g_release.motion.velocity==0);
        wall=false;hands[0].valid=true;
        for(int i=0;i<30;++i) Tick();
        CHECK(g_release.active&&std::abs(g_release.motion.angle)>.1f);
    }
    // A closing throw from the fully open stop reaches and latches the door.
    Setup();hands[0]={};hands[1]={};Write<float>(vehicle,0x770+2*0x18+0xc,-1.2f);
    status[2]=1;RefreshFrames();
    const Vec openHand=hinge+Rotate(hand-hinge,-1.2f);
    std::memcpy(hands[0].gripPos,&openHand,12);Tick();hands[0].grip=1;Tick();
    CHECK(ClaimedHands()==1);
    for(int i=1;i<=4;++i) {
        const Vec closing=hinge+Rotate(hand-hinge,-1.2f+.06f*i);
        std::memcpy(hands[0].gripPos,&closing,12);Tick();
    }
    hands[0].grip=0;Tick();CHECK(g_release.active&&g_release.motion.slam);
    for(int i=0;i<100&&g_release.active;++i) Tick();
    CHECK(!g_release.active&&Read<float>(vehicle,0x770+2*0x18+0xc)==0&&status[2]==0);
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
    // Editor: either trigger edits a panel-local point without opening the door,
    // persists once at release, and never transfers angular momentum.
    for(int h=0;h<2;++h) {
        Setup();driving::debugMode=1;Ready(h);
        hands[h].trigger=1;Tick();CHECK(g_edit.hand==h&&writes==0);
        hands[h].gripPos[2]+=.1f;Tick();
        CHECK(std::abs(driving::corrections[400][2]-.1f)<1e-4f&&driving::saves==0);
        hands[h].trigger=0;Tick();CHECK(g_edit.hand<0&&driving::saves==1&&writes==0);
        // Correction follows panel rotation, and only this door/model/side changes.
        Write<float>(vehicle,0x770+2*0x18+0xc,-.5f);RefreshFrames();
        Door d;Target t{vehicle,poolMemory,vehicles.data(),flags,0,2,10,flags[0]};
        CHECK(ReadDoor(t,pedMemory,d));
        CHECK(std::abs(d.panel.Local(d.handle).z-.4f)<1e-4f);
        CHECK(driving::corrections[400][5]==0&&driving::corrections[412][2]==0);
    }
    Setup();driving::debugMode=1;Ready();hands[0].trigger=1;Tick();
    hands[0].gripPos[2]+=.1f;Tick();Tick(true);
    CHECK(g_edit.hand<0&&driving::saves==1&&writes==0);
    Tick();CHECK(g_edit.hand<0); // held trigger cannot reacquire after cancellation
    hands[0].trigger=0;Tick();hands[0].trigger=1;Tick();CHECK(g_edit.hand==0);
    Tick(false,6);CHECK(g_edit.hand<0&&writes==0);
    Setup();driving::debugMode=1;Ready();hands[0].trigger=1;Tick();
    flags[0]=0x80;Tick();CHECK(g_edit.hand<0&&invalidWrites==0);
    Setup();driving::debugMode=1;Ready();hands[0].trigger=1;Tick();
    xr::focused=false;Tick();CHECK(g_edit.hand<0);
    // OFF returns to ordinary grip opening; held triggers cannot leak.
    Setup();driving::debugMode=1;Ready();hands[0].trigger=1;Tick();
    driving::debugMode=0;Tick();CHECK(ClaimedHands()==0&&g_edit.hand<0);
    hands[0].grip=0;Tick();hands[0].grip=1;Tick();CHECK(ClaimedHands()==1);
    // Ordinary door grip works while debug is enabled.
    Setup();driving::debugMode=1;Ready();hands[0].grip=1;Tick();
    CHECK(g_grab.hand==0&&ClaimedHands()==1);
    hands[0].grip=0;Tick();CHECK(g_grab.hand<0);
    // Wheel edits apply live; seat previews commit only on release.
    for(int editMode:{2,3}) for(int h:{0,1}) {
        Setup();driving::debugMode=1;seated=vehicle;
        driving::positionWrites=0;
        g.CPed_GetBonePosition=[](void*,GameSymbols::Vec3* p,unsigned int,bool) { *p={-.4f,.2f,1.4f}; };
        hands[0]={};hands[1]={};
        hands[h].gripPos[0]=-.4f;hands[h].gripPos[1]=.2f;hands[h].gripPos[2]=editMode==3?1.4f:1.f;
        Tick();CHECK(xr::published.count>=2);
        bool seatVisible=false,wheelVisible=false;
        for(int i=0;i<xr::published.count;++i) {
            const Vec p=xr::published.markers[i].position;
            seatVisible|=Length(p-Vec{-.4f,.2f,1.4f})<.001f;
            wheelVisible|=Length(p-Vec{-.4f,.2f,1.f})<.001f;
        }
        CHECK(seatVisible&&wheelVisible);
        hands[h].trigger=1;Tick();CHECK(g_edit.hand==h);
        hands[h].gripPos[0]+=.12f;Tick();
        CHECK(editMode==3?driving::positionWrites==0:driving::positions[0]==12);
        hands[h].trigger=0;Tick();
        CHECK(driving::positions[0]==12&&driving::saves==1&&g_edit.hand<0&&writes==0);
    }
    Setup();driving::debugMode=1;seated=vehicle;driving::positionWrites=0;
    hands[0]={};hands[1]={};hands[0].gripPos[0]=-.4f;hands[0].gripPos[1]=.2f;hands[0].gripPos[2]=1.4f;
    Tick();hands[0].trigger=1;Tick();hands[0].gripPos[0]+=.12f;Tick();
    Tick(true);CHECK(driving::positionWrites==0&&driving::saves==0&&g_edit.hand<0);
    // A second trigger on the opposite side resizes; releasing it returns to
    // translation without a position jump. Releasing the primary persists.
    Setup();driving::debugMode=1;seated=vehicle;driving::positionWrites=0;driving::radiusWrites=0;
    hands[0]={};hands[1]={};
    hands[0].gripPos[0]=-.5f;hands[0].gripPos[1]=.2f;hands[0].gripPos[2]=1.f;
    hands[1].gripPos[0]=-.2f;hands[1].gripPos[1]=.2f;hands[1].gripPos[2]=1.f;
    Tick();hands[0].trigger=1;Tick();CHECK(g_editKind==2&&g_edit.hand==0);
    hands[1].trigger=1;Tick();CHECK(g_secondHand==1&&ClaimedHands()==3);
    hands[1].gripPos[0]+=.1f;Tick();CHECK(driving::radiusValue==5&&driving::radiusWrites>0);
    hands[1].trigger=0;Tick();CHECK(g_secondHand<0&&driving::positions[0]==0);
    hands[0].trigger=0;Tick();CHECK(g_edit.hand<0&&driving::saves==1);
    DebugDrag drag;CHECK(drag.Begin(1,b.Local(b.Point({.1f,.2f,.3f}))));
    CHECK(drag.Step(b.Local(b.Point({.2f,.3f,.4f}))));
    CHECK(Length(drag.delta-Vec{.1f,.1f,.1f})<1e-4f);
    CHECK(!drag.Step({NAN,0,0})&&!drag.Step({9,0,0}));
    std::puts("vehicle doors: production motion, momentum, selection, ownership, cancellation and native handoff passed");
}
