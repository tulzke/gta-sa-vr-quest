#include "Symbols.h"
#include "PhysicalWeapon.h"
#include "Combat.h"
#include "CombatPolicy.h"
#include "PunchTrajectory.h"
#include "Melee.h"
#include "NpcGrab.h"
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
float weaponTilt{};
int thuds{},nativeHits{},voices{};
int knockdowns{},heavySounds{},lastSound{};
int nativeAnimations{},jabContacts{};
float knockVelocity[3]{};
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
    p->up[0]=std::sin(weaponTilt);p->up[1]=std::cos(weaponTilt);p->up[2]=0;return true;
}
}
namespace vehicledoors { unsigned ClaimedHands() { return 0; } }
namespace npcgrab {
unsigned ClaimedHands() { return 0; }
bool BlocksVehicleInteraction(void*,int) { return false; }
bool BlocksNpcPunch(void*,int) { return false; }
void RecordJab(void* ped,const float[3],float) { CHECK(ped==enemy);++jabContacts; }
void ResetJabCombo(void*) {}
bool KnockdownFromPunch(void* target,const float velocity[3],const char** reason,bool aliveBeforeHit) {
    CHECK(target==enemy);++knockdowns;std::copy_n(velocity,3,knockVelocity);
    CHECK(aliveBeforeHit);
    if (reason) *reason="host-applied";
    return true;
}
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
    Write(enemy,8,combat::Vec{-.1f,1.2f,0});
    focused=snapshot=true;inVehicle=false;touching=true;epoch=1;testNow=1;slot=-1;
    nativeCalls=hits=audioCalls=thuds=nativeHits=voices=knockdowns=heavySounds=lastSound=nativeAnimations=jabContacts=0;
    hapticHand=-1;amplitude=duration=weaponTilt=0;
    xr::poses[0]={};xr::poses[1]={};
    xr::poses[0].valid=true;xr::poses[0].grip=1;xr::poses[0].trigger=1;
    xr::poses[0].gripPos[0]=-.15f;xr::poses[0].gripPos[1]=1.4f;xr::poses[0].gripPos[2]=-.35f;
    // Both palms face the player's head: mirrored grip +X/-X normals.
    xr::poses[0].gripOri[1]=-.70710678f;xr::poses[0].gripOri[3]=.70710678f;
    xr::poses[1]=xr::poses[0];xr::poses[1].gripPos[0]=.15f;xr::poses[1].gripOri[1]=.70710678f;
    g.FindPlayerPed=[](int)->void* { return player; };
    g.FindPlayerVehicle=[](int,bool)->void* { return inVehicle?enemy:nullptr; };
    g.CPed_IsAlive=[](void*) { return true; };
    g.CWeaponInfo_GetWeaponInfo=[](int,signed char)->void* { return nullptr; };
    g.CWorld_ProcessLineOfSight=[](const GameSymbols::Vec3*,const GameSymbols::Vec3*,void*,void**,bool,bool,bool,bool,bool,bool,bool,bool) { return false; };
    g.CWorld_TestSphereAgainstWorld=[](GameSymbols::Vec3,float,void*,bool,bool,bool,bool,bool,bool)->void* { return touching?enemy:nullptr; };
    static unsigned char contact[0x2c]{};g.gaTempSphereColPoints=contact;
    static void* ignore{};g.CWorld_pIgnoreEntity=&ignore;
    g.CWeapon_GenerateDamageEvent=[](void* victim,void* source,int weapon,int damage,int,int direction) {
        CHECK(damage==10||damage==15||damage==20);++hits;
        unsigned char event[0x50]{};Write(event,0x18,source);Write(event,0x24,weapon);
        Write<unsigned char>(event,0x2c,static_cast<unsigned char>(direction));Write<unsigned char>(event,0x2d,2);
        combat::OnComputeDamageAnim(event,victim,true);
        return true;
    };
    g.CVehicle_InflictDamage=[](void*,void*,int,float,GameSymbols::Vec3) {};
    g.CPed_GetLocalDirection=[](void*,const GameSymbols::Vec2*) { return 0; };
    g.CEntity_GetBoundCentre=[](void*,GameSymbols::Vec3* out) { *out={0,1.2f,1.4f}; };
    g.AudioEngine=player;
    g.CAudioEngine_ReportCollision=[](void*,void* a,void* b,unsigned char sa,unsigned char sb,
        GameSymbols::Vec3*,GameSymbols::Vec3*,float force,float,unsigned char once,unsigned char loop) {
        CHECK(a==player&&b==enemy&&sa==62&&sb==62&&force==.18f&&once==1&&loop==0);++thuds;
    };
    Write(enemy,0x198+0xa8,static_cast<void*>(enemy));
    Write(player,0x198+0xa8,static_cast<void*>(player));
    combat::pedHit=[](void*,int,void*,unsigned char,float,unsigned) { ++nativeHits; };
    combat::sayEvent=[](void*,int,std::int16_t,unsigned,float,unsigned char,unsigned char,unsigned char)->std::uint16_t { ++voices;return 7; };
    combat::computeDamageAnim=[](void*,void* ped,bool) {
        ++nativeAnimations;Write(ped,0x68,combat::Vec{1,2,3});
    };
    melee::gPedAudioEvent=[](void* entity,int event,float volume,float speed,void* physical,unsigned char,int,unsigned) {
        CHECK(entity==player+0x198&&physical==enemy);
        CHECK(event==65||event==67||event==121);lastSound=event;++audioCalls;
        if (event==121) { CHECK(volume==-5.f&&speed==.9f);++heavySounds; }
    };
    combat::Reset();combat::ready=true;combat::computeDamage=NativeDamage;melee::Reset();
}
void Raise() {
    combat::Update(false);testNow+=.1;combat::Update(false);
    CHECK(combat::guards[0].active&&combat::guards[1].active&&combat::IsBlocking());
}
void Move(float distance,int count=1,double dt=.02) {
    for (int i=0;i<count;++i) {
        xr::poses[0].gripPos[2]+=distance;testNow+=dt;melee::Update(false);
    }
}
void Incoming(int weapon=0,int piece=3) {
    alignas(8) unsigned char calc[0x18]{},response[12]{};
    Write(calc,0,static_cast<void*>(enemy));Write(calc,8,20.f);Write(calc,0xc,piece);Write(calc,0x10,weapon);
    combat::OnComputeDamage(calc,player,response,true);
    CHECK(combat::Read<unsigned char>(response,0xa)==1);
}
int main() {
    Setup();Raise();CHECK(amplitude==.16f&&duration==750);
    float x=1,y=1;combat::LimitMovement(&x,&y);CHECK(std::abs(std::hypot(x,y)-.35f)<.0001f&&x==y);
    Incoming();CHECK(nativeCalls==0&&combat::Read<float>(player,0x6ac)==100);
    unsigned char blockEvent[0x50]{};Write(blockEvent,0x18,static_cast<void*>(enemy));
    Write<unsigned char>(blockEvent,0x2c,1);Write<unsigned char>(blockEvent,0x2d,2);
    Write(player,0x68,combat::Vec{.12f,.03f,-.04f});
    combat::OnComputeDamageAnim(blockEvent,player,true);
    CHECK(nativeAnimations==0&&combat::Read<int>(blockEvent,0x34)==29&&!(blockEvent[0x2d]&2));
    CHECK(combat::Read<float>(player,0x68)==.12f&&combat::Read<float>(player,0x70)==-.04f);
    // Unrelated NPC reactions still use native impulse/animation handling.
    combat::OnComputeDamageAnim(blockEvent,enemy,true);CHECK(nativeAnimations==1);
    CHECK(thuds==1);
    combat::OnPedHit(enemy+0x198,65,player,62,0,0);CHECK(nativeHits==0);
    combat::OnPedHit(player+0x198,65,enemy,62,0,0);CHECK(nativeHits==1); // counterpunch retained
    CHECK(combat::OnSayEvent(player+0x340,52,345,0,1,0,0,0)==0xffff&&voices==0);
    CHECK(combat::OnSayEvent(player+0x340,52,89,0,1,0,0,0)==7&&voices==1);
    CHECK(combat::OnSayEvent(enemy+0x340,52,345,0,1,0,0,0)==7&&voices==2);
    CHECK(amplitude==.9f&&duration==180&&hapticHand==0);CHECK(combat::GetFeedback().block==1);
    Incoming();CHECK(nativeCalls==0); // no cooldown on genuine incoming hits
    Setup();Raise();Incoming(5);CHECK(nativeCalls==1&&combat::Read<float>(player,0x6ac)==92);
    Write(player,0x68,combat::Vec{.2f,.1f,-.03f});
    combat::OnComputeDamageAnim(blockEvent,player,true);
    CHECK(nativeAnimations==0&&combat::Read<float>(player,0x68)==.2f&&combat::Read<float>(player,0x70)==-.03f);
    CHECK(thuds==1&&combat::OnSayEvent(player+0x340,52,345,0,1,0,0,0)==7);
    testNow+=.02;combat::Update(false);CHECK(combat::GetFeedback().damage==1&&combat::GetFeedback().block>0);
    Setup();Raise();Incoming(4);CHECK(combat::Read<float>(player,0x6ac)==87);
    Setup();slot=1;Write(player,0x750,5);Raise();Incoming(5);CHECK(nativeCalls==0);
    Setup();Raise();Incoming(22);CHECK(nativeCalls==1); // bullets cannot be hand-blocked
    Setup();Raise();Incoming();Incoming(22);
    CHECK(combat::OnSayEvent(player+0x340,52,345,0,1,0,0,0)==7); // real follow-up damage is audible
    Setup();Raise();Incoming();Write(player,0x6ac,90.f);
    CHECK(combat::OnSayEvent(player+0x340,52,345,0,1,0,0,0)==7); // non-calculator health loss
    Setup();Raise();Incoming(0,7);CHECK(nativeCalls==1); // raised arms do not cover legs
    Setup();Raise();Write(enemy,8,combat::Vec{0,-1,0});Incoming();CHECK(nativeCalls==1);
    Setup();Raise();testNow+=.2;Incoming();CHECK(nativeCalls==1); // stale pose
    Setup();Raise();++epoch;Incoming();CHECK(nativeCalls==1);
    Setup();Raise();focused=false;Incoming();CHECK(nativeCalls==1);
    Setup();Raise();combat::Update(true);Incoming();CHECK(nativeCalls==1);
    Setup();inVehicle=true;combat::Update(false);CHECK(!combat::guards[0].active);
    Setup();Raise();xr::poses[0].gripPos[2]-=.15f;testNow+=.02;combat::Update(false);CHECK(!combat::guards[0].active);
    CHECK(!combat::guards[1].active&&!combat::IsBlocking());
    Setup();xr::poses[1].valid=false;combat::Update(false);testNow+=.1;combat::Update(false);
    CHECK(!combat::IsBlocking());Incoming();CHECK(nativeCalls==1); // one arm cannot block
    Setup();xr::poses[1].gripPos[1]=.7f;combat::Update(false);testNow+=.1;combat::Update(false);
    CHECK(!combat::IsBlocking());Incoming();CHECK(nativeCalls==1);
    Setup();xr::poses[1].gripOri[1]=-.70710678f;combat::Update(false);testNow+=.1;combat::Update(false);
    CHECK(!combat::IsBlocking());Incoming();CHECK(nativeCalls==1); // raised palm faces away
    Setup();Raise();xr::poses[1].gripOri[1]=-.70710678f;testNow+=.02;combat::Update(false);
    CHECK(!combat::IsBlocking()&&!combat::guards[0].active&&!combat::guards[1].active);
    CHECK(amplitude==0&&duration==1); // weak ready pulse stops when either palm turns away
    x=1;y=0;combat::LimitMovement(&x,&y);CHECK(x==1&&y==0);
    Setup();Raise();testNow+=.2;x=1;y=0;combat::LimitMovement(&x,&y);CHECK(x==1&&y==0); // no stale slowdown
    Setup();combat::Update(false);Write(player,0x6b4,50.f);testNow+=.02;combat::Update(false);
    CHECK(combat::GetFeedback().damage==0);Write(player,0x6b4,40.f);testNow+=.02;combat::Update(false);
    CHECK(combat::GetFeedback().damage==1);testNow+=1;CHECK(combat::GetFeedback().damage==0);
    // Production sweep: a fast twitch/contact is insufficient, every punch needs 40 cm.
    Setup();melee::Update(false);testNow+=.02;melee::Update(false);CHECK(hits==0);
    Move(-.05f,7);CHECK(hits==0);Move(-.05f);CHECK(hits==1&&audioCalls==1);
    CHECK(nativeAnimations==0&&jabContacts==1&&combat::Read<float>(enemy,0x68)==0);
    CHECK(combat::physicalPunchVictim==nullptr); // scoped suppression cannot leak to the next event
    Move(-.05f,8);CHECK(hits==1); // one continuous pass cannot rehit every 40 cm
    Move(.05f);CHECK(hits==1);Move(.05f,6);CHECK(hits==1);
    Move(.05f);CHECK(hits==2&&audioCalls==2); // full-speed return stroke also needs 40 cm
    Setup();melee::Update(false);for (int i=0;i<100;++i) { Move(-.04f);Move(.04f); } CHECK(hits==0);
    // Stop and trigger-release discard the unfinished stroke, not just its speed.
    Setup();melee::Update(false);Move(-.05f,5);Move(0,4);Move(-.05f,3);CHECK(hits==0);
    // A repeated XR sample between producer ticks is not a physical stop.
    Setup();melee::Update(false);Move(-.05f,5);Move(0);Move(-.05f,3);CHECK(hits==1);
    Setup();melee::Update(false);Move(-.05f,5);xr::poses[0].trigger=0;Move(-.05f);
    xr::poses[0].trigger=1;Move(-.05f,3);CHECK(hits==0);
    // Detection distance is invariant across 30/60/72/90 Hz sample rates.
    for (int hz:{30,60,72,90}) {
        Setup();melee::Update(false);const float step=2.f/hz;
        const int before=static_cast<int>(std::ceil(.4f/step))-1;
        Move(-step,before,1./hz);CHECK(hits==0);Move(-step,1,1./hz);CHECK(hits==1);
    }
    // A fast miss followed by a slow touch must not inherit peak damage.
    Setup();touching=false;melee::Update(false);Move(-.05f,9);
    touching=true;Move(-.003f);CHECK(hits==0);
    // Tracking discontinuities seed history, never attack.
    Setup();melee::Update(false);xr::poses[0].gripPos[2]-=.1f;++epoch;testNow+=.02;melee::Update(false);CHECK(hits==0);
    Setup();melee::Update(false);xr::poses[0].gripPos[2]-=1;testNow+=.02;melee::Update(false);CHECK(hits==0);
    Setup();slot=1;Write(player,0x750,5);melee::Update(false);xr::poses[0].gripPos[2]-=.003f;testNow+=.02;melee::Update(false);CHECK(hits==0);
    Move(-.05f,8);CHECK(hits==1);
    Setup();slot=1;Write(player,0x750,5);melee::Update(false);
    for (int i=1;i<=8;++i) { weaponTilt=i*.1f;testNow+=.02;melee::Update(false); } CHECK(hits==1); // rotating weapon arc
    Setup();melee::Update(false);
    for (int i=0;i<40;++i) {
        xr::poses[0].gripOri[1]=std::sin(i*.1f);xr::poses[0].gripOri[3]=std::cos(i*.1f);
        testNow+=.02;melee::Update(false);
    } CHECK(hits==0); // wrist rotation in place cannot build a fist stroke
    // A direct upward punch launches backward; a straight jab never knocks down.
    Setup();melee::Update(false);
    for (int i=0;i<8;++i) {
        xr::poses[0].gripPos[1]+=.05f;xr::poses[0].gripPos[2]-=.02f;
        testNow+=.02;melee::Update(false);
    }
    CHECK(hits==1&&knockdowns==1&&knockVelocity[1]>3.9f&&knockVelocity[2]==5.f&&heavySounds==1);
    // A natural diagonal uppercut, and a fast uppercut in only two samples.
    for (int count:{2,8}) {
        Setup();melee::Update(false);
        for (int i=0;i<count;++i) {
            xr::poses[0].gripPos[1]+=.20f/count;xr::poses[0].gripPos[2]-=.4f/count;
            testNow+=.02;melee::Update(false);
        }
        CHECK(hits==1&&knockdowns==1&&knockVelocity[2]==5.f&&heavySounds==1);
    }
    Setup();melee::Update(false);Move(-.05f,8);CHECK(knockdowns==0&&heavySounds==0);
    // Horizontal arcs, both left and right, topple along their final tangent.
    for (float side:{-1.f,1.f}) {
        Setup();xr::poses[0].gripPos[0]=0;xr::poses[0].gripPos[2]=-.7f;melee::Update(false);
        for (int i=1;i<=10;++i) {
            const float angle=i*.12f;
            xr::poses[0].gripPos[0]=side*.5f*std::sin(angle);
            xr::poses[0].gripPos[2]=-.2f-.5f*std::cos(angle);
            testNow+=.02;melee::Update(false);
        }
        if (!(hits==1&&knockdowns==1&&knockVelocity[0]*side>0&&knockVelocity[2]==.2f&&heavySounds==1))
            std::fprintf(stderr,"hook side=%.1f hits=%d falls=%d velocity=(%.2f %.2f %.2f) heavy=%d path=%.3f turn=%.3f\n",
                side,hits,knockdowns,knockVelocity[0],knockVelocity[1],knockVelocity[2],heavySounds,
                melee::gMotion[0].trajectory.path,melee::gMotion[0].trajectory.turn);
        CHECK(hits==1&&knockdowns==1&&knockVelocity[0]*side>0&&knockVelocity[2]==.2f&&heavySounds==1);
    }
    // Coarse XR sampling: three positions already establish a real lateral arc.
    for (float side:{-1.f,1.f}) {
        Setup();xr::poses[0].gripPos[0]=0;xr::poses[0].gripPos[2]=-.7f;melee::Update(false);
        for (int i=1;i<=2;++i) {
            const float angle=i*.45f;
            xr::poses[0].gripPos[0]=side*.5f*std::sin(angle);
            xr::poses[0].gripPos[2]=-.2f-.5f*std::cos(angle);
            testNow+=.02;melee::Update(false);
        }
        CHECK(hits==1&&knockdowns==1&&knockVelocity[0]*side>0&&heavySounds==1);
    }
    // A brass knuckle is still classified as a fist even if physically held.
    Setup();slot=1;Write(player,0x750,1);melee::Update(false);
    for (int i=0;i<2;++i) {
        xr::poses[0].gripPos[1]+=.1f;xr::poses[0].gripPos[2]-=.2f;
        testNow+=.02;melee::Update(false);
    }
    CHECK(hits==1&&knockdowns==1&&heavySounds==1);
    // A straight lateral strike cannot masquerade as a hook.
    Setup();melee::Update(false);
    for (int i=0;i<8;++i) { xr::poses[0].gripPos[0]+=.055f;testNow+=.02;melee::Update(false); }
    CHECK(hits==1&&knockdowns==0&&heavySounds==0);
    // Preparation must not dilute the final uppercut with earlier downward travel.
    punch::Trajectory prepared;
    for (int i=0;i<20;++i) prepared.Add({0,-.025f,-.04f});
    for (int i=0;i<8;++i) prepared.Add({0,.03f,-.05f});
    CHECK(prepared.path<=.601f&&prepared.Classify({1,0,0})==punch::Kind::Uppercut);
    // Natural sideways arcs remain hooks despite small vertical/controller noise.
    for (float side:{-1.f,1.f}) {
        punch::Trajectory arc;
        for (int i=0;i<60;++i) arc.Add({.01f,-.01f,-.02f});
        float px=0,pz=-.5f;
        for (int i=1;i<=12;++i) {
            const float a=i*.10f,xx=side*.5f*std::sin(a),zz=-.5f*std::cos(a);
            arc.Add({xx-px,(i%2?.004f:-.004f),zz-pz});px=xx;pz=zz;
        }
        CHECK(arc.Classify({1,0,0})==punch::Kind::Hook);
    }
    punch::Trajectory elevatedJab;
    for (int i=0;i<5;++i) elevatedJab.Add({0,.05f,0});
    for (int i=0;i<9;++i) elevatedJab.Add({0,0,-.05f});
    CHECK(elevatedJab.Classify({1,0,0})==punch::Kind::Jab);
    punch::Trajectory noisySideJab;
    for (int i=0;i<12;++i) noisySideJab.Add({.04f,0,(i%2?.003f:-.003f)});
    CHECK(noisySideJab.Classify({1,0,0})==punch::Kind::Jab);
    std::puts("Combat: two-hand inward guard, slowdown, feedback and 40 cm stroke checks passed");
}
