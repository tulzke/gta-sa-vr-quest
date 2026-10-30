#include "NpcGrab.h"
#include "NpcGrabVisual.h"
#include "Symbols.h"
#include "PlayerLife.h"
#include "VehicleDoorMotion.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1); } } while(0)
namespace savr {
GameSymbols g{};
namespace xr {
struct HandPose { bool valid{true};float gripPos[3]{};float grip{};float trigger{}; };
vehicledoors::MarkerFrame markers;
void SetNpcGrabMarkers(const vehicledoors::MarkerFrame& frame) {markers=frame;}
bool focused=true;
bool IsSessionFocused() {return focused;}
void TriggerHaptic(int,float,float,float) {}
}
namespace physicalweapon {
unsigned mask{};unsigned HeldHandMaskRelaxed() {return mask;}
bool IsSupportHand(int) {return false;}
}
namespace basketball { unsigned HeldHandMask() {return 0;} }
namespace vehicledoors { unsigned mask{};unsigned ClaimedHands() {return mask;} }
namespace vrcam {
bool active=true;
bool GroundWalkControlsActive() {return active;}
bool WorldPointToTracking(const float in[3],float out[3]) {std::memcpy(out,in,12);return true;}
bool TrackingPointToWorld(const float in[3],float out[3]) {std::memcpy(out,in,12);return true;}
bool GetHeadWorldRay(float out[3],float[3]) {out[0]=0;out[1]=0;out[2]=1;return true;}
}
}
// PRODUCTION_ADAPTER
using namespace savr;
using namespace savr::npcgrab;
using savr::vehicledoors::Vec;
namespace {
alignas(16) char player[0x9c8]{},npc[0x9c8]{},car[0xc68]{},pad[0x200]{},poolData[0x20]{},intelligence[0x100]{},fall[0x28]{},carMatrix[0x40]{};
alignas(16) char taskMemory[32][0x28]{};
int allocations{},aggressions{},dragEvents{},dragDestroyed{},primaryReplaced{},animatedNode{},refAdds{},refRemoves{},nativePlacements{};
bool eventDriver{};
bool firstSpotBlocked{},groundMissing{};
int removed{},added{},sphereTests{};
Vec expectedExit{};
GameSymbols::Vec2 lastDirection{};
int lastFallDirection{};
alignas(16) char reaction[0x60]{};
const void* eventPlayer{};const void* eventCar{};
void* poolPtr=poolData;
std::uint8_t flags[1]{1};
bool wall{},locked{},carOccludes{},placementBlocked{};int fallCalls{},standCalls{},exits{},timer{},placedNode{};void* ignoredEntity{};
void* Lookup(int ref) {return ref==flags[0]&&!(flags[0]&128)?npc:nullptr;}
void* Player(int) {return player;}
void* Vehicle(int,bool) {return nullptr;}
void* Pad(int) {return pad;}
void BonePos(void* ped,GameSymbols::Vec3* out,unsigned int,bool) {
    const Vec p=Position(ped);*out={p.x,p.y,p.z+1};
}
bool Los(const GameSymbols::Vec3*,const GameSymbols::Vec3*,void*,void** hit,bool,bool,bool,bool,bool,bool,bool,bool) {
    *hit=wall?player:(carOccludes&&ignoredEntity!=car?car:nullptr);return *hit!=nullptr;
}
void* Allocate(std::size_t n) {CHECK(n==0x28);CHECK(allocations<32);return taskMemory[allocations++];}
void Fall(void*,int direction,int ms) {CHECK(direction>=0&&direction<=3);lastFallDirection=direction;timer=ms;++fallCalls;}
void SetTask(void* m,void* t,int slot,bool) {
    CHECK(slot==0||slot==3);Write(m,slot*8,t);
    if (slot==3&&t) ++primaryReplaced;
}
void Stand(void*,int ms,bool,bool,float) {CHECK(ms==-1||ms==0);++standCalls;}
void Primary(void* brain,void* task,bool force) {
    CHECK(brain==intelligence&&force);CHECK(Seated(npc));
    ++primaryReplaced;Write(brain,8+3*8,task);
}
void DragCtor(void*,const void* vehicle,const void* attacker,bool driver) {
    eventCar=vehicle;eventPlayer=attacker;eventDriver=driver;
}
void DragDtor(void*) {++dragDestroyed;}
void* AddEvent(void* group,void* event,bool valid) {
    CHECK(group==intelligence+0xc8&&valid);
    CHECK(eventCar==car&&eventPlayer==player&&Seated(npc));
    CHECK(primaryReplaced==1&&fallCalls==0&&animatedNode); // stock animated task precedes reaction
    ++dragEvents;return event;
}
int Direction(void* ped,const GameSymbols::Vec2* relative) {CHECK(ped==npc);lastDirection=*relative;return 2;}
bool Damage(void* victim,void* attacker,int weapon,int damage,int piece,int direction) {
    CHECK(victim==npc&&attacker==player&&weapon==0&&damage==0&&piece==3&&direction==2);
    ++aggressions;return true;
}
void* Assoc(void* clump,unsigned id) {return clump==npc&&id==378&&Read<void*>(intelligence,8+3*8)?reaction:nullptr;}
void AnimTime(void* anim,float t) {Write(anim,0x38,t);}
void Physics(void*) {}
bool NativeAI(void* v,unsigned*) {Write(v,0x608,1.f);Write(v,0x60c,0.f);Write(v,0x68,Vec{.02f,0,0});return true;}
void NativeVelocity(void* ped) {Write(ped,0x68,Vec{});Write(ped,0x6bc,0.f);Write(ped,0x6c0,0.f);}
bool IsPlayer(const void* p) {return p==player;}
bool CanOpen(const void*,const void*) {return !locked;}
int Appearance(const void*) {return 1;}
GameSymbols::Vec3 DoorPoint(const void* v,int node) {
    CHECK(v==car&&node>=8&&node<=11);placedNode=node;
    const float side=node==10||node==11?-1.f:1.f;
    const Vec right=Read<Vec>(carMatrix,0);
    expectedExit=Position(npc)+right*(side*.5f);
    return {expectedExit.x,expectedExit.y,expectedExit.z};
}
bool Ground(GameSymbols::Vec3*) {return !groundMissing;}
void* Sphere(GameSymbols::Vec3 p,float radius,void* ignored,bool buildings,bool vehicles,bool peds,bool objects,bool dummies,bool seeThrough) {
    CHECK(radius==.35f&&ignored==npc&&buildings&&vehicles&&peds&&objects&&dummies&&!seeThrough);
    ++sphereTests;
    if (placementBlocked) return car;
    if (firstSpotBlocked&&std::hypot(p.x-expectedExit.x,p.y-expectedExit.y)<.18f) return player;
    return nullptr;
}
void Remove(void* ped) {CHECK(ped==npc);++removed;}
void Add(void* ped) {
    CHECK(ped==npc&&removed==added+1);++added;
    const Vec delta=Position(ped)-expectedExit;
    CHECK(std::hypot(delta.x,delta.y)<=.71f); // no perimeter/boot fallback
}
void OutCtor(void*,void* v,int node,bool) {CHECK(v==car);CHECK(node==placedNode);}
bool Out(void* task,void* p) {
    CHECK(Read<std::uint8_t>(task,0x1f)==1); // never run automatic placement a second time
    ++exits;Write<std::uint8_t>(p,0x585,0);
    if (Read<void*>(car,0x5a0)==p) Write<void*>(car,0x5a0,nullptr);
    for (int i=0;i<3;++i) if (Read<void*>(car,0x5a8+i*8)==p) Write<void*>(car,0x5a8+i*8,nullptr);
    return true;
}
void DragTask(void*,void* vehicle,int node) {CHECK(vehicle==car);animatedNode=node;}
void Register(void* vehicle,void** reference) {CHECK(vehicle==car&&*reference==car);++refAdds;}
void Clean(void* vehicle,void** reference) {CHECK(vehicle==car&&*reference==car);++refRemoves;}
bool NativePlace(void*,int,void*,bool) {++nativePlacements;return true;}
void FinishExtraction() {
    CHECK(Animating(npc)&&Seated(npc));
    CHECK(OnPlacement(npc,animatedNode,car,true));
    alignas(16) char task[0x30]{};Write<std::uint8_t>(task,0x1f,1);Out(task,npc);
}
xr::HandPose hands[2];
void Setup() {
    Reset();for (auto& a:animations) ClearAnimation(a);for (auto& stop:stops) ClearStop(stop);std::memset(npc,0,sizeof(npc));std::memset(car,0,sizeof(car));std::memset(intelligence,0,sizeof(intelligence));
    flags[0]=1;wall=locked=carOccludes=placementBlocked=false;ignoredEntity=nullptr;placedNode=0;
    firstSpotBlocked=groundMissing=false;removed=added=sphereTests=0;
    allocations=aggressions=dragEvents=dragDestroyed=primaryReplaced=animatedNode=refAdds=refRemoves=nativePlacements=0;eventCar=eventPlayer=nullptr;
    xr::focused=vrcam::active=true;physicalweapon::mask=vehicledoors::mask=0;
    hands[0]={};hands[1]={};hands[1].valid=false;hands[0].gripPos[2]=1;
    Write(npc,0x20,static_cast<void*>(npc));Write(npc,0x538,static_cast<void*>(intelligence));
    std::memset(reaction,0,sizeof(reaction));Write(reaction,0x3c,1.f);Write(npc,0x6ac,100.f);Write(player,0x6ac,100.f);
    Write<void*>(poolData,0,npc);Write(poolData,8,flags);Write(poolData,0x10,1);
    pedPool=&poolPtr;getPed=Lookup;isPlayer=IsPlayer;allocate=Allocate;fallCtor=Fall;standCtor=Stand;calculateVelocity=NativeVelocity;setTask=SetTask;
    dragTaskCtor=DragTask;registerReference=Register;cleanReference=Clean;originalPlacement=NativePlace;doorPosition=DoorPoint;groundPosition=Ground;
    worldRemove=Remove;worldAdd=Add;g.CWorld_TestSphereAgainstWorld=Sphere;canOpen=CanOpen;ready=true;
    draggedCtor=DragCtor;draggedDtor=DragDtor;association=Assoc;setAnimationTime=AnimTime;realPhysics=Physics;vehicleAI=NativeAI;
    g.CEventGroup_Add=AddEvent;g.CPed_GetLocalDirection=Direction;g.CWeapon_GenerateDamageEvent=Damage;
    g.FindPlayerPed=Player;g.FindPlayerVehicle=Vehicle;g.CPad_GetPad=Pad;
    g.CWorld_pIgnoreEntity=&ignoredEntity;g.CPed_GetBonePosition=BonePos;g.CWorld_ProcessLineOfSight=Los;g.CVehicle_GetVehicleAppearance=Appearance;
    previousEpoch=1;updated=1;fallCalls=standCalls=exits=0;
}
void Tick(bool blocked=false,int epoch=1) {
    updated=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count()-.01;
    Update(blocked,hands,true,epoch);
}
void Grip() {hands[0].grip=0;Tick();hands[0].grip=1;Tick();}
void Seat(int passenger=-1) {
    Write<std::uint8_t>(npc,0x585,1);Write<void*>(npc,0x708,car);
    Write<void*>(car,passenger<0?0x5a0:0x5a8+passenger*8,npc);
    const int door=passenger<0?2:passenger==0?3:passenger==1?4:5;
    Write(car,0x770+door*0x18+0xc,.9f);
    const float side=door==2||door==4?-1.f:1.f;
    Write(npc,8,Vec{side*.6f,0,0});
    hands[0].gripPos[0]=side*.7f;
    Write<void*>(car,0x18,carMatrix);
    Write(carMatrix,0,Vec{1,0,0});Write(carMatrix,0x10,Vec{0,1,0});
    Write(carMatrix,0x20,Vec{0,0,1});Write(carMatrix,0x30,Vec{-side*.6f,0,0});
}
void Punch() {
    const float side=hands[0].gripPos[0]<0?-1.f:1.f;
    hands[0].gripPos[0]+=side*.3f;hands[0].grip=hands[0].trigger=0;Tick();
    hands[0].grip=hands[0].trigger=1;hands[0].gripPos[0]-=side*.15f;Tick();
}
void WaitForGrip() {
    CHECK(animations[0].phase==Phase::Hit);
    Write<std::uint16_t>(reaction,0x46,0x21);Write(reaction,0x38,.4f);Tick();
    CHECK(animations[0].phase==Phase::Waiting&&Read<float>(reaction,0x3c)==0);
    CHECK((Read<std::uint16_t>(reaction,0x46)&0x20)==0);
    hands[0].grip=hands[0].trigger=0;Tick();
}
void ConfirmGrip() {hands[0].grip=1;hands[0].trigger=0;Tick();}

}
int main() {
    Setup();Grip();CHECK(npcgrab::ClaimedHands()==1&&fallCalls==0&&standCalls==1);
    hands[0].gripPos[0]=.04f;Tick();CHECK(Read<Vec>(npc,0x68).x>0);
    const Vec heldSpeed=Read<Vec>(npc,0x68);
    OnCalculateVelocity(npc); // native callback zeros velocity and animation shift
    CHECK(Length(Read<Vec>(npc,0x68)-heldSpeed)<.0001f);
    CHECK(Read<float>(npc,0x6bc)==heldSpeed.x&&Read<float>(npc,0x6c0)==heldSpeed.y);
    Write(player,0x68,Vec{.1f,0,0});OnCalculateVelocity(player);
    CHECK(Length(Read<Vec>(player,0x68))==0); // unrelated peds retain original behavior
    hands[0].gripPos[0]=.12f;hands[0].grip=0;Tick();CHECK(!npcgrab::ClaimedHands()&&fallCalls==1&&Read<Vec>(npc,0x68).x>0);
    Write(npc,0x68,Vec{.07f,.02f,-.01f});OnCalculateVelocity(npc);
    CHECK(Length(Read<Vec>(npc,0x68)-Vec{.07f,.02f,-.01f})<.0001f); // preserve physics including gravity
    for (auto& flight:flights) flight.until=updated-1;
    OnCalculateVelocity(npc);CHECK(Length(Read<Vec>(npc,0x68))==0); // bounded override expires
    Setup();Grip();hands[0].grip=0;Tick();
    CHECK(!npcgrab::ClaimedHands()&&!fallCalls&&!Read<void*>(intelligence,8)&&!aggressions); // gentle release stays standing
    Setup();Grip();CHECK(!aggressions);
    hands[0].gripPos[0]=.18f;Tick();CHECK(aggressions==1);
    hands[0].gripPos[0]=.25f;Tick();CHECK(aggressions==1); // one event per continuous hold
    hands[0].grip=0;Tick();CHECK(aggressions==1); // release does not duplicate aggression
    Setup();hands[0].grip=1;Tick();CHECK(!npcgrab::ClaimedHands()); // interruption requires reopening
    Setup();hands[0].gripPos[0]=.8f;Grip();CHECK(!npcgrab::ClaimedHands());
    hands[0].gripPos[0]=0;Tick();CHECK(npcgrab::ClaimedHands()==1&&fallCalls==0); // early squeeze reaches contact
    Setup();wall=true;Grip();CHECK(!npcgrab::ClaimedHands());
    Setup();physicalweapon::mask=1;Grip();CHECK(!npcgrab::ClaimedHands());
    Setup();vehicledoors::mask=1;Grip();CHECK(!npcgrab::ClaimedHands());
    Setup();Grip();Write(npc,0x68,Vec{.12f,0,0});Tick(true);CHECK(!npcgrab::ClaimedHands()&&Length(Read<Vec>(npc,0x68))==0);
    Setup();Grip();Write(npc,0x68,Vec{.12f,0,0});Tick(false,2);CHECK(!npcgrab::ClaimedHands()&&Length(Read<Vec>(npc,0x68))==0);
    Setup();Grip();flags[0]=2;timer=42;Tick();CHECK(!npcgrab::ClaimedHands()&&timer==42); // recycled slot untouched
    Setup();Grip();Write<void*>(intelligence,8,car);timer=42;Tick();CHECK(!npcgrab::ClaimedHands()&&timer==42); // task replaced
    Setup();Grip();hands[0].valid=false;Tick();CHECK(!npcgrab::ClaimedHands()&&Read<void*>(intelligence,8)==nullptr);
    Setup();Grip();hands[0].gripPos[0]=2;Tick();CHECK(!npcgrab::ClaimedHands()); // discontinuity never throws
    Setup();Seat();Write(car,0x770+2*0x18+0xc,0.f);Grip();CHECK(!npcgrab::ClaimedHands()&&!exits);
    Setup();Seat();locked=true;Grip();CHECK(!npcgrab::ClaimedHands());
    Setup();Seat();Write(car,0x68,Vec{.1f,0,0});Grip();CHECK(!npcgrab::ClaimedHands());
    Setup();Seat();carOccludes=true;Punch();CHECK(npcgrab::ClaimedHands()==1&&ignoredEntity==nullptr);
    Setup();Seat();carOccludes=wall=true;Grip();CHECK(!npcgrab::ClaimedHands()); // wall behind car still blocks
    Setup();Seat();Write(carMatrix,0x30,Vec{-.6f,0,0});Grip();CHECK(!npcgrab::ClaimedHands()); // opposite side
    for (int seat=-1;seat<3;++seat) {
        Setup();Seat(seat);hands[0].grip=0;Tick();npcgrab::PublishMarkers();
        CHECK(xr::markers.count==1&&xr::markers.markers[0].fist);
        const Vec original=Position(npc);Punch();
        CHECK(Seated(npc)&&!exits&&!removed&&!added&&Length(Position(npc)-original)==0);
        CHECK(npcgrab::ClaimedHands()==1&&primaryReplaced==1&&!dragEvents&&!aggressions);
        constexpr int expectedNode[]={10,8,11,9};CHECK(animatedNode==expectedNode[seat+1]);
        CHECK(refAdds==1&&refRemoves==0);
        WaitForGrip();npcgrab::PublishMarkers();
        CHECK(xr::markers.count==1&&!xr::markers.markers[0].fist&&Seated(npc));
        ConfirmGrip();CHECK(animations[0].phase==Phase::Pull&&Read<float>(reaction,0x3c)==1);
        CHECK((Read<std::uint16_t>(reaction,0x46)&0x20)!=0);
        CHECK(dragEvents==1&&dragDestroyed==1&&eventDriver==(seat<0)&&!aggressions);
        FinishExtraction();Tick();CHECK(!Animating(npc)&&refRemoves==1);
        hands[0].grip=0;Tick();CHECK(!npcgrab::ClaimedHands());
    }
    Setup();Seat();Grip();CHECK(!animatedNode&&!npcgrab::ClaimedHands()); // no hit from a grip alone
    Setup();Seat();Punch();WaitForGrip();
    animations[0].waitUntil=updated-1;Tick();
    CHECK(Seated(npc)&&!Read<void*>(intelligence,8+3*8)&&aggressions==1&&!dragEvents);
    CHECK(Read<float>(reaction,0x3c)==1&&!animations[0].target.ped); // no frozen animation or driving lock
    Setup();Seat();Punch();WaitForGrip();Tick(true);
    CHECK(!Read<void*>(intelligence,8+3*8)&&!aggressions&&Read<float>(reaction,0x3c)==1); // menu cancels safely
    Setup();Seat();placementBlocked=true;Punch();WaitForGrip();ConfirmGrip();
    CHECK(animations[0].phase==Phase::Waiting&&!dragEvents&&!removed&&!added);
    Setup();Seat();groundMissing=true;Punch();WaitForGrip();ConfirmGrip();
    CHECK(animations[0].phase==Phase::Waiting&&!dragEvents&&!removed&&!added);
    Setup();Seat();wall=true;Punch();CHECK(!animatedNode&&!removed&&!added);
    Setup();Seat();Punch();WaitForGrip();ConfirmGrip();flags[0]=2;
    CHECK(OnPlacement(npc,10,car,true)&&nativePlacements==1&&!removed);
    Tick();CHECK(refRemoves==1); // recycled target untouched
    Setup();Seat();Punch();animations[0].car=nullptr;Tick();CHECK(!animations[0].target.ped&&!removed);
    Setup();Seat();hands[0].grip=0;Tick();npcgrab::PublishMarkers();CHECK(xr::markers.count==1);
    Tick(true);npcgrab::PublishMarkers();CHECK(xr::markers.count==0);
    Setup();Seat();locked=true;Tick();npcgrab::PublishMarkers();CHECK(xr::markers.count==0);
    Setup();Seat();physicalweapon::mask=3;Tick();npcgrab::PublishMarkers();CHECK(xr::markers.count==0);
    // Low-speed opening stops traffic for 1.5 s even after AI chooses to accelerate.
    Setup();Seat();Write(car,0x68,Vec{.07f,0,0});OnDoorOpening(car);
    CHECK(Read<Vec>(car,0x68).x==0&&Read<float>(car,0x608)==0&&Read<float>(car,0x60c)==1);
    unsigned flagsAI{};CHECK(OnVehicleAI(car,&flagsAI));CHECK(Read<float>(car,0x608)==0);
    stops[0].until=Now()-1;OnVehicleAI(car,&flagsAI);CHECK(Read<float>(car,0x608)==1);
    Setup();Seat();Write(car,0x68,Vec{.2f,0,0});OnDoorOpening(car);CHECK(!stops[0].car&&Read<Vec>(car,0x68).x==.2f);
    Setup();Seat();Write<void*>(car,0x5a0,player);OnDoorOpening(car);CHECK(!stops[0].car);
    Setup();Seat();Punch();WaitForGrip();OnVehicleAI(car,&flagsAI);
    CHECK(Seated(npc)&&Read<float>(car,0x608)==0&&Read<Vec>(car,0x68).x==0); // waiting driver cannot flee early
    const auto palm=npcgrab::Palm({{}, {1,0,0}, {0,1,0}, false});
    const auto fist=npcgrab::Fist({{}, {1,0,0}, {0,1,0}, false,true});
    CHECK(palm.count>60&&palm.count<=120&&fist.count>24&&fist.count<=120);
    FILE* preview=std::fopen("palm.csv","w");CHECK(preview);
    for (int i=0;i<palm.count;++i) {
        const auto p=palm.vertices[i];CHECK(Finite(p)&&std::abs(p.x)<.07f&&std::abs(p.y)<.10f&&p.z==0);
        std::fprintf(preview,"%f,%f\n",p.x,p.y);
    }
    std::fclose(preview);
    preview=std::fopen("fist.csv","w");CHECK(preview);
    for (int i=0;i<fist.count;++i) {
        const auto p=fist.vertices[i];CHECK(Finite(p)&&std::abs(p.x)<.07f&&std::abs(p.y)<.10f&&p.z==0);
        std::fprintf(preview,"%f,%f\n",p.x,p.y);
    }
    std::fclose(preview);
    std::puts("NPC grab: hit/wait/grip phases, traffic stop, timeout reaction, holds, throws and hints passed");
}
