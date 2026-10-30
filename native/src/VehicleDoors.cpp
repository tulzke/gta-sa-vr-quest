#include "VehicleDoors.h"
#include "VehicleDoorMotion.h"
#include "Basketball.h"
#include "Driving.h"
#include "Log.h"
#include "PhysicalWeapon.h"
#include "Symbols.h"
#include "VrCamera.h"
#include "Xr.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

namespace savr::vehicledoors {
namespace {
// Retail 2.11.311 arm64. Pool generation is checked before every retained
// entity access; the interaction never owns an engine pointer or task.
constexpr int kPoolSize=110,kStride=0xc68;
constexpr int kNodes[4]={10,8,11,9};
using OpenFn=void(*)(void*,void*,int,int,float,bool);
using SwingFn=void(*)(void*,int,int);
OpenFn g_open{};
SwingFn g_swing{};
bool (*g_canOpen)(const void*,const void*){};
bool (*g_inUse)(const void*,int,int){};
unsigned (*g_status)(const void*,int){};
void (*g_setStatus)(void*,int,unsigned){};
void* (*g_frameLtm)(void*){};
bool g_ready{};
GripEdge g_edges[2];
std::atomic<unsigned> g_claimed{};
std::uint64_t g_epoch{};
double g_updated{};
MarkerFrame g_worldMarkers{};

double Now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
template<class T> T Read(const void* p,int off) {
    T value{};std::memcpy(&value,static_cast<const char*>(p)+off,sizeof(value));return value;
}
template<class T> void Write(void* p,int off,T value) {
    std::memcpy(static_cast<char*>(p)+off,&value,sizeof(value));
}
struct Pool {
    void* identity{};
    char* objects{};
    const std::uint8_t* flags{};
    bool ReadCurrent() {
        identity=g.CPools_ms_pVehiclePool?*g.CPools_ms_pVehiclePool:nullptr;
        if (!identity||Read<int>(identity,0x10)!=kPoolSize||Read<std::uint8_t>(identity,0x18)!=1) return false;
        objects=Read<char*>(identity,0);
        flags=Read<const std::uint8_t*>(identity,8);
        return objects&&flags;
    }
};
struct Target {
    void* vehicle{};
    void* pool{};
    char* objects{};
    const std::uint8_t* flags{};
    int slot{-1},door{},node{};
    std::uint8_t generation{};
    bool Live(const Pool& p) const {
        return vehicle&&p.identity==pool&&p.objects==objects&&p.flags==flags&&
            slot>=0&&slot<kPoolSize&&p.flags[slot]==generation&&(generation&0x80)==0;
    }
};
struct Grab { Target target{}; Motion motion{}; int hand{-1}; bool inside{}; } g_grab;
struct Door {
    Basis car{};
    Vec hinge{},handle{},outward{},tangent{};
    float angle{},closed{},open{};
};

Basis Matrix(void* matrix) {
    return {Read<Vec>(matrix,0),Read<Vec>(matrix,0x10),Read<Vec>(matrix,0x20),Read<Vec>(matrix,0x30)};
}
bool ReadDoor(const Target& t,void* ped,Door& d,bool inside=false) {
    if (!g.CVehicle_GetVehicleAppearance||g.CVehicle_GetVehicleAppearance(t.vehicle)!=1||
        !g_canOpen(t.vehicle,ped)||g_inUse(t.vehicle,t.node,0)) return false;
    const auto kind=Read<std::uint8_t>(t.vehicle,0x5a);
    if ((kind&7)!=2||(kind>>3)==5) return false; // no wrecks / other classes
    const Vec speed=Read<Vec>(t.vehicle,0x68);
    if (!Finite(speed)||Length(speed)>.035f) return false; // about 1.75 m/s at stock 50 Hz
    const int model=Read<std::uint16_t>(t.vehicle,0x32);
    if (model==431||model==437||model==432||model==539) return false; // special/automatic doors
    void* matrix=Read<void*>(t.vehicle,0x18);
    if (!matrix) return false;
    d.car=Matrix(matrix);
    if (!d.car.Valid()) return false;
    void* frame=Read<void*>(t.vehicle,0x800+t.node*8);
    const auto status=g_status(static_cast<char*>(t.vehicle)+0x758,t.door);
    if (!frame||status>3) return false;
    const auto* door=static_cast<char*>(t.vehicle)+0x770+t.door*0x18;
    d.open=Read<float>(door,0);
    d.closed=Read<float>(door,4);
    d.angle=Read<float>(door,0xc);
    // OpenDoor uses angle = ratio * openAngle, so non-zero rest angles and
    // non-vertical hinges require a different adapter and stay native for now.
    if (Read<std::uint8_t>(door,0xa)!=2||!std::isfinite(d.angle)||
        !std::isfinite(d.open)||!std::isfinite(d.closed)||std::abs(d.closed)>.001f||
        std::abs(d.open)<.3f||std::abs(d.open)>2.8f) return false;
    void* ltm=g_frameLtm(frame);
    if (!ltm) return false;
    const Basis panel=Matrix(ltm);
    if (!panel.Valid()) return false;
    d.hinge=d.car.Local(panel.position);
    if (!Finite(d.hinge)||std::abs(d.hinge.x)<.3f||std::abs(d.hinge.x)>2.3f||
        std::abs(d.hinge.y)>5||std::abs(d.hinge.z)>3) return false;
    float length=.85f;
    // Front/rear hinge spacing adapts the approximate latch to the loaded
    // body. No universal handle dummy exists in the verified interface.
    const int other=t.door<=3?t.node+1:t.node-1;
    if (void* adjacent=Read<void*>(t.vehicle,0x800+other*8)) {
        void* adjacentLtm=g_frameLtm(adjacent);
        const Vec p=adjacentLtm?d.car.Local(Read<Vec>(adjacentLtm,0x30)):d.hinge;
        if (Finite(p)&&std::abs(p.y-d.hinge.y)>.5f&&std::abs(p.y-d.hinge.y)<1.6f)
            length=std::abs(p.y-d.hinge.y);
    }
    const float side=(t.door==2||t.door==4)?-1.f:1.f;
    // In the seat, offer a reachable grip inside the cabin, away from the
    // door's collision surface. The same point drives both hint and hit test.
    d.handle=panel.Point({side*(inside?-.34f:.065f),-length*.82f,.30f});
    d.outward=panel.right*side;
    Vec tangent=Cross(d.car.up,d.handle-panel.position)*(d.open>0?1.f:-1.f);
    const float n=Length(tangent);
    if (n<.2f) return false;
    d.tangent=inside?d.car.right*(-side):tangent*(1.f/n);
    return Finite(d.handle)&&Finite(d.tangent);
}
bool Visible(Vec eye,Vec handle,void* vehicle) {
    alignas(8) std::uint8_t col[0x2c]{};
    void* hit{};
    const GameSymbols::Vec3 start{eye.x,eye.y,eye.z},end{handle.x,handle.y,handle.z};
    return !g.CWorld_ProcessLineOfSight(&start,&end,col,&hit,true,true,false,true,true,false,false,false)||hit==vehicle;
}
bool ToTracking(Vec world,Vec& local) {
    const float in[3]={world.x,world.y,world.z};float out[3]{};
    if (!vrcam::WorldPointToTracking(in,out)) return false;
    local={out[0],out[1],out[2]};return Finite(local);
}
bool ToWorld(const float in[3],Vec& world) {
    float out[3]{};if (!vrcam::TrackingPointToWorld(in,out)) return false;
    world={out[0],out[1],out[2]};return Finite(world);
}
void Apply() {
    const auto& t=g_grab.target;
    const float ratio=g_grab.motion.Ratio();
    // Preserve intact/damaged identity; only the open bit changes. The native
    // OpenDoor closes the latch when ratio reaches zero and handles its sound.
    if (ratio>0) {
        auto* damage=static_cast<char*>(t.vehicle)+0x758;
        const auto status=g_status(damage,t.door);
        if (status==0||status==2) g_setStatus(damage,t.door,status|1u);
    }
    g_open(t.vehicle,nullptr,t.node,t.door,ratio,true);
    Write<float>(t.vehicle,0x770+t.door*0x18+0x14,0.f);
}
void OnSwing(void* vehicle,int node,int door) {
    // The native function integrates its door in PreRender. While held, use
    // only our input-tick angle (never integrate once per stereo eye).
    if (g_grab.hand>=0&&g_grab.target.vehicle==vehicle&&g_grab.target.door==door&&
        Now()-g_updated<.2) {
        Pool p;void* ped=g.FindPlayerPed?g.FindPlayerPed(-1):nullptr;Door d;
        if (ped&&p.ReadCurrent()&&g_grab.target.Live(p)&&ReadDoor(g_grab.target,ped,d,g_grab.inside)) {
            Apply();return;
        }
        Reset();
    }
    g_swing(vehicle,node,door);
}
bool Hook(void* target) {
    constexpr std::uint32_t expected[]={0xd10543ff,0xa90f7bfd,0xf90083fc,0xa91167fa};
    if (std::memcmp(target,expected,sizeof(expected))) {
        LOGE("[doors] unsupported ProcessSwingingDoor prologue; interaction disabled");return false;
    }
    const long page=sysconf(_SC_PAGESIZE);
    if (page<=0) return false;
    void* trampoline=mmap(nullptr,page,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if (trampoline==MAP_FAILED) return false;
    auto* code=static_cast<std::uint32_t*>(target);
    auto* t=static_cast<std::uint32_t*>(trampoline);
    std::memcpy(t,code,16);t[4]=0x58000051;t[5]=0xd61f0220;
    const auto resume=reinterpret_cast<std::uintptr_t>(target)+16;
    std::memcpy(t+6,&resume,8);
    __builtin___clear_cache(reinterpret_cast<char*>(t),reinterpret_cast<char*>(t)+32);
    const auto address=reinterpret_cast<std::uintptr_t>(target);
    const auto start=address&~(static_cast<std::uintptr_t>(page)-1);
    const auto end=(address+16+page-1)&~(static_cast<std::uintptr_t>(page)-1);
    if (mprotect(reinterpret_cast<void*>(start),end-start,PROT_READ|PROT_WRITE|PROT_EXEC)) {
        munmap(trampoline,page);return false;
    }
    g_swing=reinterpret_cast<SwingFn>(trampoline);
    const auto hook=reinterpret_cast<std::uintptr_t>(&OnSwing);
    std::memcpy(code+2,&hook,8);code[1]=0xd61f0220;
    __atomic_store_n(code,0x58000051,__ATOMIC_RELEASE);
    __builtin___clear_cache(reinterpret_cast<char*>(code),reinterpret_cast<char*>(code)+16);
    return true;
}
void AddMarker(MarkerFrame& frame,const Door& door,bool held) {
    if (frame.count>=MarkerFrame::kCapacity) return;
    frame.markers[frame.count++]={door.handle,door.tangent,door.car.up,held};
}
void Publish(const MarkerFrame& frame) {
    g_worldMarkers=frame;PublishMarkers();
}
} // namespace

void Install(void* handle) {
    if (!handle||g_ready) return;
    g_open=reinterpret_cast<OpenFn>(dlsym(handle,"_ZN11CAutomobile8OpenDoorEP4CPedi6eDoorsfb"));
    g_canOpen=reinterpret_cast<decltype(g_canOpen)>(dlsym(handle,"_ZNK8CVehicle15CanPedOpenLocksEPK4CPed"));
    g_inUse=reinterpret_cast<decltype(g_inUse)>(dlsym(handle,"_ZN13CCarEnterExit14IsCarDoorInUseERK8CVehicleii"));
    g_status=reinterpret_cast<decltype(g_status)>(dlsym(handle,"_ZNK14CDamageManager13GetDoorStatusE6eDoors"));
    g_setStatus=reinterpret_cast<decltype(g_setStatus)>(dlsym(handle,"_ZN14CDamageManager13SetDoorStatusE6eDoorsj"));
    g_frameLtm=reinterpret_cast<decltype(g_frameLtm)>(dlsym(handle,"_Z13RwFrameGetLTMP7RwFrame"));
    void* swing=dlsym(handle,"_ZN11CAutomobile19ProcessSwingingDoorEi6eDoors");
    g_ready=g_open&&g_canOpen&&g_inUse&&g_status&&g_setStatus&&g_frameLtm&&swing&&
        g.CPools_ms_pVehiclePool&&g.CVehicle_GetVehicleAppearance&&g.CWorld_ProcessLineOfSight&&Hook(swing);
    if (g_ready) LOGI("[doors] hand interaction installed");
    else LOGE("[doors] required symbols or verified hook unavailable; hand interaction disabled");
}
unsigned ClaimedHands() { return g_claimed.load(std::memory_order_relaxed); }
void PublishMarkers() {
    MarkerFrame frame;frame.epoch=g_worldMarkers.epoch;
    for (int i=0;i<g_worldMarkers.count;++i) {
        const auto& world=g_worldMarkers.markers[i];Marker m;Vec tip,up;
        if (!ToTracking(world.position,m.position)||!ToTracking(world.position+world.direction,tip)||
            !ToTracking(world.position+world.up,up)) continue;
        m.direction=tip-m.position;m.up=up-m.position;m.held=world.held;
        frame.markers[frame.count++]=m;
    }
    xr::SetVehicleDoorMarkers(frame);
}
void Reset() {
    g_grab={};g_claimed.store(0,std::memory_order_relaxed);
    for (auto& edge:g_edges) edge.latched=true;
    g_worldMarkers={};
    xr::SetVehicleDoorMarkers({});
}
void Update(bool blocked,const xr::HandPose hands[2],bool trackingValid,std::uint64_t epoch) {
    const double now=Now();
    const float dt=g_updated>0?static_cast<float>(now-g_updated):0.f;
    g_updated=now;
    void* ped=g.FindPlayerPed?g.FindPlayerPed(-1):nullptr;
    void* pad=g.CPad_GetPad?g.CPad_GetPad(0):nullptr;
    const bool usable=!blocked&&trackingValid&&epoch==g_epoch&&g_ready&&ped&&pad&&
        !Read<std::uint8_t>(pad,0x12d)&&vrcam::VehicleDoorControlsActive()&&
        !(g.PlayerIsEnteringCar&&g.PlayerIsEnteringCar())&&
        !(g.CTimer_m_UserPause&&*g.CTimer_m_UserPause)&&
        !(g.CTimer_m_CodePause&&*g.CTimer_m_CodePause);
    g_epoch=epoch;
    bool fresh[2]{};
    for (int h=0;h<2;++h) fresh[h]=g_edges[h].Step(hands[h].grip,usable&&hands[h].valid);
    Pool pool;
    Vec eye{};float origin[3]{},forward[3]{};
    if (!usable||!pool.ReadCurrent()||!vrcam::GetHeadWorldRay(origin,forward)) { Reset();return; }
    eye={origin[0],origin[1],origin[2]};
    unsigned occupied=physicalweapon::HeldHandMaskRelaxed()|basketball::HeldHandMask();
    void* seated=g.FindPlayerVehicle?g.FindPlayerVehicle(-1,false):nullptr;
    if (seated) {
        driving::WheelVisualState wheel{};
        driving::GetWheelVisualState(&wheel);
        for (int h=0;h<2;++h) if (wheel.grabbed[h]) occupied|=1u<<h;
    }
    for (int h=0;h<2;++h) if (physicalweapon::IsSupportHand(h)) occupied|=1u<<h;
    Vec world[2]{};bool valid[2]{};
    for (int h=0;h<2;++h) valid[h]=hands[h].valid&&ToWorld(hands[h].gripPos,world[h]);
    MarkerFrame markers;markers.epoch=epoch;
    if (g_grab.hand>=0) {
        const int h=g_grab.hand;Door d;
        if (!g_grab.target.Live(pool)||!valid[h]||(occupied&(1u<<h))||
            (g_grab.inside ? seated!=g_grab.target.vehicle : seated!=nullptr)||
            !ReadDoor(g_grab.target,ped,d,g_grab.inside)||Length(d.handle-world[h])>.7f||
            !Visible(eye,d.handle,g_grab.target.vehicle)||
            !g_grab.motion.Step(d.car.Local(world[h]),dt)) {
            Reset();return;
        }
        Apply();
        if (hands[h].grip<=.35f) {
            // Only a deliberate release of a valid grab transfers momentum.
            // Reset/menus/tracking loss/deleted entities never write an impulse.
            const float velocity=g_grab.motion.ReleaseVelocity(dt);
            Write<float>(g_grab.target.vehicle,0x770+g_grab.target.door*0x18+0x14,velocity);
            Reset();return;
        }
        ReadDoor(g_grab.target,ped,d,g_grab.inside);
        AddMarker(markers,d,true);
        Publish(markers);return;
    }
    float best=seated?.34f:.23f;Target chosen{};Door chosenDoor{};int chosenHand=-1;
    // Show only the nearest car's reachable side. Four arrows at most, and no
    // distant traffic markers. Selection is spatial; there is no enter-button gate.
    float nearest=2.2f;int nearestSlot=-1;
    for (int slot=0;slot<kPoolSize;++slot) {
        if (pool.flags[slot]&0x80) continue;
        void* vehicle=pool.objects+slot*kStride;
        if (seated && (vehicle!=seated || !g.CVehicle_IsDriver || !g.CVehicle_IsDriver(vehicle,ped))) continue;
        if ((Read<std::uint8_t>(vehicle,0x5a)&7)!=2||g.CVehicle_GetVehicleAppearance(vehicle)!=1) continue;
        void* matrix=Read<void*>(vehicle,0x18);
        if (!matrix) continue;
        const Vec position=Read<Vec>(matrix,0x30);
        if (!Finite(position)||Length(position-eye)>4.5f) continue;
        for (int i=0;i<4;++i) {
            const Target t{vehicle,pool.identity,pool.objects,pool.flags,slot,i+2,kNodes[i],pool.flags[slot]};
            Door d;
            if (seated && i!=0) continue;
            if (!ReadDoor(t,ped,d,seated!=nullptr)) continue;
            const float side=(i==0||i==2)?-1.f:1.f;
            if (!seated && Dot(eye-d.car.position,d.car.right)*side<-.12f) continue;
            const float distance=Length(d.handle-eye);
            if (distance<nearest&&Visible(eye,d.handle,vehicle)) { nearest=distance;nearestSlot=slot; }
        }
    }
    if (nearestSlot>=0) for (int i=0;i<4;++i) {
        const int slot=nearestSlot;
        Target t{pool.objects+slot*kStride,pool.identity,pool.objects,pool.flags,slot,i+2,kNodes[i],pool.flags[slot]};
        if (seated && i!=0) continue;
        Door d;
        if (!ReadDoor(t,ped,d,seated!=nullptr)||Length(eye-d.handle)>2.2f||
            (!seated && Dot(eye-d.car.position,d.car.right)*((i==0||i==2)?-1.f:1.f)<-.12f)||
            !Visible(eye,d.handle,t.vehicle)) continue;
        AddMarker(markers,d,false);
        for (int h=0;h<2;++h) {
            if (!fresh[h]||!valid[h]||(occupied&(1u<<h))) continue;
            const float distance=Length(world[h]-d.handle);
            if (distance<best) { best=distance;chosen=t;chosenDoor=d;chosenHand=h; }
        }
    }
    if (chosenHand>=0&&g_grab.motion.Begin(chosenDoor.hinge,chosenDoor.car.Local(world[chosenHand]),
                                         chosenDoor.angle,chosenDoor.closed,chosenDoor.open)) {
        g_grab.target=chosen;g_grab.hand=chosenHand;
        g_grab.inside=seated!=nullptr;
        g_claimed.store(1u<<chosenHand,std::memory_order_relaxed);
        Apply();markers.count=0;AddMarker(markers,chosenDoor,true);
        xr::TriggerHaptic(chosenHand,.25f,0,30);
        LOGI("[doors] grabbed model=%d door=%d hand=%d",Read<std::uint16_t>(chosen.vehicle,0x32),chosen.door,chosenHand);
    }
    Publish(markers);
}
} // namespace savr::vehicledoors
