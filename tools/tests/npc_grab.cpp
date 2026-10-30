#include "NpcGrab.h"
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
struct HandPose { bool valid{true};float gripPos[3]{};float grip{}; };
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
void* Allocate(std::size_t n) {CHECK(n==0x28);return fall;}
void Fall(void*,int direction,int ms) {CHECK(direction==0);timer=ms;++fallCalls;}
void SetTask(void* m,void* t,int slot,bool) {CHECK(slot==0);Write(m,0,t);}
void Stand(void*,int ms,bool,bool,float) {CHECK(ms==-1);++standCalls;}
void NativeVelocity(void* ped) {Write(ped,0x68,Vec{});Write(ped,0x6bc,0.f);Write(ped,0x6c0,0.f);}
bool IsPlayer(const void* p) {return p==player;}
bool CanOpen(const void*,const void*) {return !locked;}
int Appearance(const void*) {return 1;}
bool Place(void* ped,int node,void* v,bool) {
    CHECK(ped==npc&&v==car);CHECK(node>=8&&node<=11);
    if (placementBlocked) return false;
    placedNode=node;return true;
}
void OutCtor(void*,void* v,int node,bool) {CHECK(v==car);CHECK(node==placedNode);}
bool Out(void*,void* p) {
    ++exits;Write<std::uint8_t>(p,0x585,0);
    if (Read<void*>(car,0x5a0)==p) Write<void*>(car,0x5a0,nullptr);
    for (int i=0;i<3;++i) if (Read<void*>(car,0x5a8+i*8)==p) Write<void*>(car,0x5a8+i*8,nullptr);
    return true;
}
void Dtor(void*) {}
xr::HandPose hands[2];
void Setup() {
    Reset();std::memset(npc,0,sizeof(npc));std::memset(car,0,sizeof(car));std::memset(intelligence,0,sizeof(intelligence));
    flags[0]=1;wall=locked=carOccludes=placementBlocked=false;ignoredEntity=nullptr;placedNode=0;
    xr::focused=vrcam::active=true;physicalweapon::mask=vehicledoors::mask=0;
    hands[0]={};hands[1]={};hands[1].valid=false;hands[0].gripPos[2]=1;
    Write(npc,0x538,static_cast<void*>(intelligence));Write(npc,0x6ac,100.f);Write(player,0x6ac,100.f);
    Write<void*>(poolData,0,npc);Write(poolData,8,flags);Write(poolData,0x10,1);
    pedPool=&poolPtr;getPed=Lookup;isPlayer=IsPlayer;allocate=Allocate;fallCtor=Fall;standCtor=Stand;calculateVelocity=NativeVelocity;setTask=SetTask;
    outCtor=OutCtor;outProcess=Out;outDtor=Dtor;placeOutside=Place;canOpen=CanOpen;ready=true;
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
    CHECK(!npcgrab::ClaimedHands()&&!fallCalls&&!Read<void*>(intelligence,8)); // gentle release stays standing
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
    Setup();Seat();carOccludes=true;Grip();CHECK(npcgrab::ClaimedHands()==1&&ignoredEntity==nullptr);
    Setup();Seat();carOccludes=wall=true;Grip();CHECK(!npcgrab::ClaimedHands()); // wall behind car still blocks
    Setup();Seat();Write(carMatrix,0x30,Vec{-.6f,0,0});Grip();CHECK(!npcgrab::ClaimedHands()); // opposite side
    for (int seat=-1;seat<3;++seat) {
        Setup();Seat(seat);Grip();CHECK(npcgrab::ClaimedHands()==1&&!exits&&!fallCalls&&standCalls==0);
        hands[0].gripPos[0]+=.25f;Tick();CHECK(exits==1&&fallCalls==0&&standCalls==1&&!Seated(npc));
        constexpr int expectedNode[]={10,8,11,9};CHECK(placedNode==expectedNode[seat+1]);
        CHECK(!Read<void*>(car,seat<0?0x5a0:0x5a8+seat*8));
        hands[0].grip=0;Tick();CHECK(!npcgrab::ClaimedHands());
    }
    Setup();Seat();Grip();hands[0].grip=0;Tick();CHECK(!exits&&Seated(npc));
    Setup();Seat();Grip();placementBlocked=true;hands[0].gripPos[0]+=.25f;Tick();
    CHECK(!exits&&Seated(npc)&&!npcgrab::ClaimedHands());
    std::puts("NPC grab: hold, throw, ownership, interruption, pool lifetime and four vehicle seats passed");
}
