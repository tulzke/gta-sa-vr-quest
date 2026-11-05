#include "Symbols.h"
#include "PhysicalWeapon.h"
#include "Combat.h"
#include "CombatPolicy.h"
#include "Melee.h"
#include "PlayerLife.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#define LOGI(...) ((void)0)
#define LOGE(...) ((void)0)
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1); } } while(0)
double testNow=1;
alignas(16) unsigned char player[0xb00]{},enemy[0xb00]{};
bool focused=true,snapshot=true,inVehicle=false,touching=true;
std::uint64_t epoch=1;
float head[3]{0,1.6f,0};
int slot=-1,nativeCalls{},hits{},audioCalls{},hapticHand=-1;
float amplitude{},duration{};
template<class T> void Write(void* p,int off,T value) { std::memcpy(static_cast<char*>(p)+off,&value,sizeof(value)); }
namespace savr {
GameSymbols g{};
namespace xr {
struct HandPose {
    bool valid{},aimValid{};
    float gripPos[3]{},gripOri[4]{0,0,0,1},aimPos[3]{},aimOri[4]{0,0,0,1};
    float grip{},trigger{};
};
HandPose poses[2]{};
bool IsSessionFocused() { return focused; }
bool GetHeadPose(float p[3],float q[4],std::uint64_t* e) {
    std::copy_n(head,3,p);q[0]=q[1]=q[2]=0;q[3]=1;*e=epoch;return snapshot;
}
void TriggerHaptic(int h,float a,float,float ms) { hapticHand=h;amplitude=a;duration=ms; }
}
namespace physicalweapon {
bool GetHandPosesSnapshot(xr::HandPose out[2]) { std::copy_n(xr::poses,2,out);return snapshot; }
bool GetInteractionTrackingSnapshot(float p[3],float q[4],xr::HandPose out[2],std::uint64_t* e) {
    GetHandPosesSnapshot(out);return xr::GetHeadPose(p,q,e);
}
int HeldSlot(int h) { return h==0?slot:-1; }
bool IsSupportHand(int) { return false; }
bool GetHeldVisualPoseTracking(int h,int,TrackingPose* p) {
    std::copy_n(xr::poses[h].gripPos,3,p->position);
    p->up[0]=0;p->up[1]=1;p->up[2]=0;return true;
}
}
namespace vehicledoors { unsigned ClaimedHands() { return 0; } }
namespace npcgrab {
unsigned ClaimedHands() { return 0; }
bool BlocksVehicleInteraction(void*,int) { return false; }
bool BlocksNpcPunch(void*,int) { return false; }
}
namespace vrcam {
bool TrackingPointToWorld(const float p[3],float w[3]) { w[0]=p[0];w[1]=-p[2];w[2]=p[1];return true; }
}
}
// PRODUCTION_COMBAT
// PRODUCTION_MELEE
using namespace savr;
void NativeDamage(void* calc,void* victim,void* response,bool) {
    ++nativeCalls;float value{};std::memcpy(&value,static_cast<char*>(calc)+8,4);
    const float old=combat::Read<float>(victim,0x6ac);
    Write(victim,0x6ac,old-value);Write(response,0,value);Write<unsigned char>(response,0xa,1);
}
void Setup() {
    std::memset(player,0,sizeof(player));std::memset(enemy,0,sizeof(enemy));
    Write(player,0x6ac,100.f);Write(enemy,0x6ac,100.f);
    Write<unsigned char>(player,0x5a,3);Write<unsigned char>(enemy,0x5a,3);
    Write(enemy,8,combat::Vec{0,1.2f,0});
    focused=snapshot=true;inVehicle=false;touching=true;epoch=1;testNow=1;slot=-1;
    nativeCalls=hits=audioCalls=0;hapticHand=-1;amplitude=duration=0;
    xr::poses[0]={};xr::poses[1]={};
    xr::poses[0].valid=true;xr::poses[0].grip=1;xr::poses[0].trigger=1;
    xr::poses[0].gripPos[0]=-.15f;xr::poses[0].gripPos[1]=1.4f;xr::poses[0].gripPos[2]=-.35f;
    g.FindPlayerPed=[](int)->void* { return player; };
    g.FindPlayerVehicle=[](int,bool)->void* { return inVehicle?enemy:nullptr; };
    g.CPed_IsAlive=[](void*) { return true; };
    g.CWeaponInfo_GetWeaponInfo=[](int,signed char)->void* { return nullptr; };
    g.CWorld_ProcessLineOfSight=[](const GameSymbols::Vec3*,const GameSymbols::Vec3*,void*,void**,bool,bool,bool,bool,bool,bool,bool,bool) { return false; };
    g.CWorld_TestSphereAgainstWorld=[](GameSymbols::Vec3,float,void*,bool,bool,bool,bool,bool,bool)->void* { return touching?enemy:nullptr; };
    static unsigned char contact[0x2c]{};g.gaTempSphereColPoints=contact;
    static void* ignore{};g.CWorld_pIgnoreEntity=&ignore;
    g.CWeapon_GenerateDamageEvent=[](void*,void*,int,int damage,int,int) { CHECK(damage==10||damage==20);++hits;return true; };
    g.CVehicle_InflictDamage=[](void*,void*,int,float,GameSymbols::Vec3) {};
    g.CPed_GetLocalDirection=[](void*,const GameSymbols::Vec2*) { return 0; };
    g.CEntity_GetBoundCentre=[](void*,GameSymbols::Vec3* out) { *out={0,1.2f,1.4f}; };
    melee::gPedAudioEvent=[](void*,int event,float,float,void*,unsigned char,int,unsigned) { CHECK(event==65);++audioCalls; };
    combat::Reset();combat::ready=true;combat::computeDamage=NativeDamage;melee::Reset();
}
void Raise() { combat::Update(false);testNow+=.1;combat::Update(false);CHECK(combat::guards[0].active); }
void Incoming(int weapon=0,int piece=3) {
    alignas(8) unsigned char calc[0x18]{},response[12]{};
    Write(calc,0,static_cast<void*>(enemy));Write(calc,8,20.f);Write(calc,0xc,piece);Write(calc,0x10,weapon);
    combat::OnComputeDamage(calc,player,response,true);
    CHECK(combat::Read<unsigned char>(response,0xa)==1);
}
int main() {
    Setup();Raise();CHECK(amplitude==.16f&&duration==750);
    Incoming();CHECK(nativeCalls==0&&combat::Read<float>(player,0x6ac)==100);
    CHECK(amplitude==.9f&&duration==180&&hapticHand==0);CHECK(combat::GetFeedback().block==1);
    Incoming();CHECK(nativeCalls==0); // no cooldown on genuine incoming hits
    Setup();Raise();Incoming(5);CHECK(nativeCalls==1&&combat::Read<float>(player,0x6ac)==92);
    testNow+=.02;combat::Update(false);CHECK(combat::GetFeedback().damage==1&&combat::GetFeedback().block>0);
    Setup();Raise();Incoming(4);CHECK(combat::Read<float>(player,0x6ac)==87);
    Setup();slot=1;Write(player,0x750,5);Raise();Incoming(5);CHECK(nativeCalls==0);
    Setup();Raise();Incoming(22);CHECK(nativeCalls==1); // bullets cannot be hand-blocked
    Setup();Raise();Incoming(0,7);CHECK(nativeCalls==1); // raised arms do not cover legs
    Setup();Raise();Write(enemy,8,combat::Vec{0,-1,0});Incoming();CHECK(nativeCalls==1);
    Setup();Raise();testNow+=.2;Incoming();CHECK(nativeCalls==1); // stale pose
    Setup();Raise();++epoch;Incoming();CHECK(nativeCalls==1);
    Setup();Raise();focused=false;Incoming();CHECK(nativeCalls==1);
    Setup();Raise();combat::Update(true);Incoming();CHECK(nativeCalls==1);
    Setup();inVehicle=true;combat::Update(false);CHECK(!combat::guards[0].active);
    Setup();Raise();xr::poses[0].gripPos[2]-=.15f;testNow+=.02;combat::Update(false);CHECK(!combat::guards[0].active);
    Setup();combat::Update(false);Write(player,0x6b4,50.f);testNow+=.02;combat::Update(false);
    CHECK(combat::GetFeedback().damage==0);Write(player,0x6b4,40.f);testNow+=.02;combat::Update(false);
    CHECK(combat::GetFeedback().damage==1);testNow+=1;CHECK(combat::GetFeedback().damage==0);
    // Production sweep: still contact never deals damage; a fast punch does.
    Setup();melee::Update(false);testNow+=.02;melee::Update(false);CHECK(hits==0);
    xr::poses[0].gripPos[2]-=.04f;testNow+=.02;melee::Update(false);CHECK(hits==1&&audioCalls==1);
    xr::poses[0].gripPos[2]-=.04f;testNow+=.02;melee::Update(false);CHECK(hits==1);
    xr::poses[0].gripPos[2]+=.09f;testNow+=.02;melee::Update(false);CHECK(hits==2&&audioCalls==2);
    // A fast miss followed by a slow touch must not inherit peak damage.
    Setup();touching=false;melee::Update(false);xr::poses[0].gripPos[2]-=.04f;testNow+=.02;melee::Update(false);
    touching=true;xr::poses[0].gripPos[2]-=.003f;testNow+=.02;melee::Update(false);CHECK(hits==0);
    // Tracking discontinuities seed history, never attack.
    Setup();melee::Update(false);xr::poses[0].gripPos[2]-=.1f;++epoch;testNow+=.02;melee::Update(false);CHECK(hits==0);
    Setup();melee::Update(false);xr::poses[0].gripPos[2]-=1;testNow+=.02;melee::Update(false);CHECK(hits==0);
    Setup();slot=1;Write(player,0x750,5);melee::Update(false);xr::poses[0].gripPos[2]-=.003f;testNow+=.02;melee::Update(false);CHECK(hits==0);
    xr::poses[0].gripPos[2]-=.04f;testNow+=.02;melee::Update(false);CHECK(hits==1);
    std::puts("Combat: production block, feedback, damage and melee sweep checks passed");
}
