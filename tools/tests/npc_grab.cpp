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
alignas(16) char player[0x9c8]{},npc[0x9c8]{},car[0xc68]{},pad[0x200]{},poolData[0x20]{},intelligence[0x100]{},fall[0x28]{};
void* poolPtr=poolData;
std::uint8_t flags[1]{1};
bool wall{},locked{};int fallCalls{},exits{},timer{};
void* Lookup(int ref) {return ref==flags[0]&&!(flags[0]&128)?npc:nullptr;}
void* Player(int) {return player;}
void* Vehicle(int,bool) {return nullptr;}
void* Pad(int) {return pad;}
void BonePos(void* ped,GameSymbols::Vec3* out,unsigned int,bool) {
    const Vec p=Position(ped);*out={p.x,p.y,p.z+1};
}
bool Los(const GameSymbols::Vec3*,const GameSymbols::Vec3*,void*,void** hit,bool,bool,bool,bool,bool,bool,bool,bool) {
    *hit=wall?car:nullptr;return wall;
}
void* Allocate(std::size_t n) {CHECK(n==0x28);return fall;}
void Fall(void*,int direction,int ms) {CHECK(direction==0);timer=ms;++fallCalls;}
void SetTask(void* m,void* t,int slot,bool) {CHECK(slot==0);Write(m,0,t);}
void Timer(void*,int ms) {timer=ms;}
bool IsPlayer(const void* p) {return p==player;}
bool CanOpen(const void*,const void*) {return !locked;}
int Appearance(const void*) {return 1;}
void OutCtor(void*,void* v,int door,bool) {CHECK(v==car);CHECK(door>=2&&door<=5);}
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
    flags[0]=1;wall=locked=false;xr::focused=vrcam::active=true;physicalweapon::mask=vehicledoors::mask=0;
    hands[0]={};hands[1]={};hands[1].valid=false;hands[0].gripPos[2]=1;
    Write(npc,0x538,static_cast<void*>(intelligence));Write(npc,0x6ac,100.f);Write(player,0x6ac,100.f);
    Write<void*>(poolData,0,npc);Write(poolData,8,flags);Write(poolData,0x10,1);
    pedPool=&poolPtr;getPed=Lookup;isPlayer=IsPlayer;allocate=Allocate;fallCtor=Fall;downTime=Timer;setTask=SetTask;
    outCtor=OutCtor;outProcess=Out;outDtor=Dtor;canOpen=CanOpen;ready=true;
    g.FindPlayerPed=Player;g.FindPlayerVehicle=Vehicle;g.CPad_GetPad=Pad;
    g.CPed_GetBonePosition=BonePos;g.CWorld_ProcessLineOfSight=Los;g.CVehicle_GetVehicleAppearance=Appearance;
    previousEpoch=1;updated=1;fallCalls=exits=0;
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
}
}
int main() {
    Setup();Grip();CHECK(npcgrab::ClaimedHands()==1&&fallCalls==1);
    hands[0].gripPos[0]=.04f;Tick();CHECK(Read<Vec>(npc,0x68).x>0);
    hands[0].grip=0;Tick();CHECK(!npcgrab::ClaimedHands()&&timer==1000&&Read<Vec>(npc,0x68).x>0);
    Setup();hands[0].grip=1;Tick();CHECK(!npcgrab::ClaimedHands()); // requires open then squeeze
    Setup();wall=true;Grip();CHECK(!npcgrab::ClaimedHands());
    Setup();physicalweapon::mask=1;Grip();CHECK(!npcgrab::ClaimedHands());
    Setup();vehicledoors::mask=1;Grip();CHECK(!npcgrab::ClaimedHands());
    Setup();Grip();Write(npc,0x68,Vec{.12f,0,0});Tick(true);CHECK(!npcgrab::ClaimedHands()&&timer==1000&&Length(Read<Vec>(npc,0x68))==0);
    Setup();Grip();Write(npc,0x68,Vec{.12f,0,0});Tick(false,2);CHECK(!npcgrab::ClaimedHands()&&Length(Read<Vec>(npc,0x68))==0);
    Setup();Grip();flags[0]=2;timer=42;Tick();CHECK(!npcgrab::ClaimedHands()&&timer==42); // recycled slot untouched
    Setup();Grip();Write<void*>(intelligence,8,car);timer=42;Tick();CHECK(!npcgrab::ClaimedHands()&&timer==42); // task replaced
    Setup();Grip();hands[0].valid=false;Tick();CHECK(!npcgrab::ClaimedHands()&&timer==1000);
    Setup();Grip();hands[0].gripPos[0]=2;Tick();CHECK(!npcgrab::ClaimedHands()); // discontinuity never throws
    Setup();Seat();Write(car,0x770+2*0x18+0xc,0.f);Grip();CHECK(!npcgrab::ClaimedHands()&&!exits);
    Setup();Seat();locked=true;Grip();CHECK(!npcgrab::ClaimedHands());
    Setup();Seat();Write(car,0x68,Vec{.1f,0,0});Grip();CHECK(!npcgrab::ClaimedHands());
    for (int seat=-1;seat<3;++seat) {
        Setup();Seat(seat);Grip();CHECK(npcgrab::ClaimedHands()==1&&!exits&&!fallCalls);
        hands[0].gripPos[0]=.25f;Tick();CHECK(exits==1&&fallCalls==1&&!Seated(npc));
        CHECK(!Read<void*>(car,seat<0?0x5a0:0x5a8+seat*8));
        hands[0].grip=0;Tick();CHECK(!npcgrab::ClaimedHands());
    }
    Setup();Seat();Grip();hands[0].grip=0;Tick();CHECK(!exits&&Seated(npc));
    std::puts("NPC grab: hold, throw, ownership, interruption, pool lifetime and four vehicle seats passed");
}
