#include "NpcGrab.h"
#include "Basketball.h"
#include "PhysicalWeapon.h"
#include "PlayerLife.h"
#include "Symbols.h"
#include "VehicleDoors.h"
#include "VehicleDoorMotion.h"
#include "VrCamera.h"
#include "Xr.h"
#include "Log.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <dlfcn.h>

namespace savr::npcgrab {
namespace {
using namespace vehicledoors;
// Retail Android 2.11.311 arm64: CPools::GetPed @4b1fcc,
// CarSetPedOut::ProcessPed @64671c, FallAndGetUp @67a260.
void** pedPool{};
void* (*getPed)(int){};
bool (*isPlayer)(const void*){};
void* (*allocate)(std::size_t){};
void (*fallCtor)(void*,int,int){};
void (*downTime)(void*,int){};
void (*setTask)(void*,void*,int,bool){};
void (*outCtor)(void*,void*,int,bool){};
bool (*outProcess)(void*,void*){};
void (*outDtor)(void*){};
bool (*canOpen)(const void*,const void*){};
std::atomic<unsigned> claimed{};
GripEdge edges[2];
std::uint64_t previousEpoch{};
double updated{};
bool ready{};
void* owner{};

template<class T> T Read(const void* p,int offset) {
    T value{};std::memcpy(&value,static_cast<const char*>(p)+offset,sizeof(value));return value;
}
template<class T> void Write(void* p,int offset,T value) {
    std::memcpy(static_cast<char*>(p)+offset,&value,sizeof(value));
}
Vec Position(void* p) {
    void* matrix=Read<void*>(p,0x18);
    return matrix?Read<Vec>(matrix,0x30):Read<Vec>(p,8);
}
Vec Limit(Vec v,float maximum) {
    const float length=Length(v);
    return Finite(v)&&std::isfinite(length)?v*(maximum/std::max(maximum,length)):Vec{};
}
struct Pool {
    void* identity{};void* objects{};const std::uint8_t* flags{};int size{};
    bool Load() {
        identity=pedPool?*pedPool:nullptr;
        if (!identity) return false;
        size=Read<int>(identity,0x10);objects=Read<void*>(identity,0);
        flags=Read<const std::uint8_t*>(identity,8);
        return size>0&&size<=1024&&objects&&flags;
    }
};
struct Hold {
    void* ped{};void* pool{};void* objects{};void* task{};
    int ref{-1},bone{};Vec previous{},velocity{},start{};
    bool seated{};
    bool Live(const Pool& p) const {
        return ped&&p.identity==pool&&p.objects==objects&&ref>=0&&(ref>>8)<p.size&&
            p.flags[ref>>8]==(ref&255)&&!(p.flags[ref>>8]&128)&&getPed(ref)==ped;
    }
} holds[2];
void* Manager(void* ped) {
    void* intelligence=Read<void*>(ped,0x538);
    return intelligence?static_cast<char*>(intelligence)+8:nullptr;
}
bool OwnTask(const Hold& h) {
    void* manager=Manager(h.ped);
    return h.task&&manager&&Read<void*>(manager,0)==h.task;
}
Vec Bone(void* ped,int bone) {
    GameSymbols::Vec3 p{};g.CPed_GetBonePosition(ped,&p,bone,true);
    return {p.x,p.y,p.z};
}
void Release(Hold& h,const Pool& pool,bool throwing) {
    if (h.Live(pool)&&h.task&&OwnTask(h)) {
        // Keep the native task alive only briefly. No long-lived freeze flag,
        // collision override or replaced mission/default task needs restoring.
        downTime(h.task,1000);
        if (!playerlife::Dead(h.ped))
            Write(h.ped,0x68,throwing?Limit(h.velocity,8.f)*.02f:Vec{});
    }
    h={};
}
bool Visible(Vec from,Vec to,void* ped) {
    alignas(8) std::uint8_t col[0x2c]{};void* hit{};
    const GameSymbols::Vec3 a{from.x,from.y,from.z},b{to.x,to.y,to.z};
    return !g.CWorld_ProcessLineOfSight(&a,&b,col,&hit,true,true,true,true,true,false,false,false)||hit==ped;
}
bool Seated(void* ped) { return (Read<std::uint8_t>(ped,0x585)&1)!=0; }
// Only stationary, unlocked automobiles with the occupant's door already open.
// This also excludes boats, bikes, aircraft and occupants across the cabin.
int ExitDoor(void* ped,void* player) {
    void* car=Read<void*>(ped,0x708);
    if (!car||g.CVehicle_GetVehicleAppearance(car)!=1||!canOpen(car,player)||
        !Finite(Read<Vec>(car,0x68))||Length(Read<Vec>(car,0x68))>.035f) return -1;
    int door=-1;
    if (Read<void*>(car,0x5a0)==ped) door=2;
    for (int i=0;i<3&&door<0;++i)
        if (Read<void*>(car,0x5a8+i*8)==ped) door=i==0?3:(i==1?4:5);
    if (door<0) return -1;
    const auto status=Read<std::uint8_t>(car,0x758+9+door);
    const float angle=Read<float>(car,0x770+door*0x18+0xc);
    if (status!=3&&!(status<=2&&std::isfinite(angle)&&std::abs(angle)>.45f)) return -1;
    return door;
}
bool StartFall(Hold& h) {
    void* manager=Manager(h.ped);
    // Do not delete an existing physical-response task (death, arrest, etc.).
    if (!manager||Read<void*>(manager,0)) return false;
    void* task=allocate(0x28);
    if (!task) return false;
    fallCtor(task,0,1500);
    setTask(manager,task,0,false);
    h.task=Read<void*>(manager,0)==task?task:nullptr;
    return h.task!=nullptr;
}
bool Extract(Hold& h,void* player) {
    const int door=ExitDoor(h.ped,player);
    if (door<0) return false;
    // Use the engine helper, including driver/passenger removal, reference
    // cleanup, collision-safe placement and restoration of on-foot state.
    alignas(16) std::uint8_t task[0x30]{};
    outCtor(task,Read<void*>(h.ped,0x708),door,false);
    const bool done=outProcess(task,h.ped);
    outDtor(task);
    return done&&!Seated(h.ped);
}
} // namespace

void Install(void* handle) {
#define RESOLVE(target,name) target=reinterpret_cast<decltype(target)>(dlsym(handle,name))
    RESOLVE(pedPool,"_ZN6CPools11ms_pPedPoolE");
    RESOLVE(getPed,"_ZN6CPools6GetPedEi");
    RESOLVE(isPlayer,"_ZNK4CPed8IsPlayerEv");
    RESOLVE(allocate,"_ZN5CTasknwEm");
    RESOLVE(fallCtor,"_ZN24CTaskComplexFallAndGetUpC1Eii");
    RESOLVE(downTime,"_ZN24CTaskComplexFallAndGetUp11SetDownTimeEi");
    RESOLVE(setTask,"_ZN12CTaskManager7SetTaskEP5CTaskib");
    RESOLVE(outCtor,"_ZN23CTaskSimpleCarSetPedOutC1EP8CVehicleib");
    RESOLVE(outProcess,"_ZN23CTaskSimpleCarSetPedOut10ProcessPedEP4CPed");
    RESOLVE(outDtor,"_ZN23CTaskSimpleCarSetPedOutD1Ev");
    RESOLVE(canOpen,"_ZNK8CVehicle15CanPedOpenLocksEPK4CPed");
#undef RESOLVE
    ready=pedPool&&getPed&&isPlayer&&allocate&&fallCtor&&downTime&&setTask&&
        outCtor&&outProcess&&outDtor&&canOpen&&g.CPed_GetBonePosition&&
        g.CWorld_ProcessLineOfSight&&g.CVehicle_GetVehicleAppearance;
    LOGI("[npcgrab] available=%d",ready);
}
unsigned ClaimedHands() { return claimed.load(std::memory_order_relaxed); }
void Reset() {
    Pool pool;pool.Load();
    for (auto& h:holds) Release(h,pool,false);
    for (auto& edge:edges) edge.latched=true;
    claimed.store(0,std::memory_order_relaxed);
}
void Update(bool blocked,const xr::HandPose hands[2],bool trackingValid,std::uint64_t epoch) {
    const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const float dt=updated?float(now-updated):0;updated=now;
    void* player=g.FindPlayerPed?g.FindPlayerPed(-1):nullptr;
    if (player!=owner) { Reset();owner=player; }
    void* pad=g.CPad_GetPad?g.CPad_GetPad(0):nullptr;
    Pool pool;
    const bool usable=ready&&!blocked&&trackingValid&&epoch==previousEpoch&&
        dt>0&&dt<=.15f&&xr::IsSessionFocused()&&player&&!playerlife::Dead(player)&&pad&&
        !Read<std::uint16_t>(pad,0x110)&&!Read<std::uint8_t>(pad,0x12d)&&
        vrcam::GroundWalkControlsActive()&&!(g.FindPlayerVehicle&&g.FindPlayerVehicle(-1,false))&&
        !(g.PlayerIsEnteringCar&&g.PlayerIsEnteringCar())&&
        !(g.CTimer_m_UserPause&&*g.CTimer_m_UserPause)&&
        !(g.CTimer_m_CodePause&&*g.CTimer_m_CodePause)&&pool.Load();
    previousEpoch=epoch;
    if (!usable) { Reset();return; }
    unsigned mask=0;
    const unsigned occupied=vehicledoors::ClaimedHands()|physicalweapon::HeldHandMaskRelaxed()|basketball::HeldHandMask();
    float eyeRaw[3]{},forward[3]{};
    if (!vrcam::GetHeadWorldRay(eyeRaw,forward)) { Reset();return; }
    const Vec eye{eyeRaw[0],eyeRaw[1],eyeRaw[2]};
    for (int hand=0;hand<2;++hand) {
        auto& h=holds[hand];
        float world[3]{};
        const bool valid=hands[hand].valid&&std::isfinite(hands[hand].grip)&&
            !(occupied&(1u<<hand))&&!physicalweapon::IsSupportHand(hand)&&
            vrcam::TrackingPointToWorld(hands[hand].gripPos,world);
        const Vec p{world[0],world[1],world[2]};
        const bool good=valid&&Finite(p)&&Finite(eye)&&Length(p-eye)<1.6f;
        const bool fresh=edges[hand].Step(hands[hand].grip,good);
        if (h.ped) {
            if (!h.Live(pool)||!good||playerlife::Dead(h.ped)||Length(p-h.previous)>.6f||
                !Finite(Position(h.ped))||Length(Position(h.ped)-Position(player))>3.f||
                (h.task&&!OwnTask(h))) { Release(h,pool,false);continue; }
            const Vec measured=(p-h.previous)*(1.f/dt);
            h.velocity=h.velocity+(Limit(measured,10.f)-h.velocity)*(1.f-std::exp(-dt/.045f));
            h.previous=p;
            if (hands[hand].grip<=.35f) { Release(h,pool,true);continue; }
            if (h.seated) {
                if (!Seated(h.ped)||ExitDoor(h.ped,player)<0) { Release(h,pool,false);continue; }
                if (Length(p-h.start)>.22f) {
                    if (!Extract(h,player)||!StartFall(h)) { Release(h,pool,false);continue; }
                    h.seated=false;
                }
            }
            if (!h.seated) {
                if (Seated(h.ped)||!OwnTask(h)) { Release(h,pool,false);continue; }
                const Vec anchor=Bone(h.ped,h.bone);
                if (!Finite(anchor)||Length(p-anchor)>1.5f||!Visible(eye,anchor,h.ped)) {
                    Release(h,pool,false);continue;
                }
                downTime(h.task,1500);
                // Native CPhysical collision/gravity remains active. Move speed
                // uses metres per stock 50 Hz tick, not metres per second.
                const Vec speed=Limit((p-anchor)*10.f+h.velocity*.65f+Vec{0,0,.4f},6.f);
                Write(h.ped,0x68,speed*.02f);
            }
        } else if (fresh&&good) {
            float best=.26f;
            Hold chosen{};
            for (int slot=0;slot<pool.size;++slot) {
                if (pool.flags[slot]&128) continue;
                const int ref=(slot<<8)|pool.flags[slot];
                void* ped=getPed(ref);
                if (!ped||ped==player||ped==holds[1-hand].ped||isPlayer(ped)||playerlife::Dead(ped)||
                    !Finite(Position(ped))||Length(Position(ped)-Position(player))>3.f) continue;
                const bool seated=Seated(ped);
                if (seated&&ExitDoor(ped,player)<0) continue;
                void* manager=Manager(ped);
                if (!manager||Read<void*>(manager,0)) continue;
                for (int bone:{3,4,5}) {
                    const Vec point=Bone(ped,bone);
                    const float distance=Length(p-point);
                    if (Finite(point)&&distance<best&&Visible(eye,point,ped)&&Visible(p,point,ped)) {
                        best=distance;chosen={ped,pool.identity,pool.objects,nullptr,ref,bone,p,{},p,seated};
                    }
                }
            }
            if (chosen.ped&&(chosen.seated||StartFall(chosen))) {
                h=chosen;xr::TriggerHaptic(hand,.45f,80.f,35.f);
            }
        }
        if (h.ped) mask|=1u<<hand;
    }
    claimed.store(mask,std::memory_order_relaxed);
}
}
