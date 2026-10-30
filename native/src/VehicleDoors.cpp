#include "VehicleDoors.h"
#include "NpcGrab.h"
#include "VehicleDoorMotion.h"
#include "VehicleDebugDrag.h"
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
std::uintptr_t g_driveDoorStatusReturn{};
GripEdge g_edges[2];
GripEdge g_editEdges[2];
std::atomic<unsigned> g_claimed{};
std::uint64_t g_epoch{};
double g_updated{};
MarkerFrame g_worldMarkers{};
DebugDrag g_edit;
int g_editMode{},g_editModel{-1},g_editDrivingMode{};
int g_editKind{},g_secondHand{-1},g_scaleBase{};
float g_scaleSpan{};
int g_editBase[3]{};
Vec g_editDoorBase{};
bool g_editDirty{};
void* g_editVehicle{};


double Now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
template<class T> T Read(const void* p,int off) {
    T value{};std::memcpy(&value,static_cast<const char*>(p)+off,sizeof(value));return value;
}
template<class T> void Write(void* p,int off,T value) {
    std::memcpy(static_cast<char*>(p)+off,&value,sizeof(value));
}
unsigned DoorStatusAt(const void* damage,int door,std::uintptr_t caller) {
    const unsigned status=static_cast<unsigned>(door)<=5?Read<std::uint8_t>(damage,9+door):4;
    if (status==1&&door==2&&caller==g_driveDoorStatusReturn&&g_driveDoorStatusReturn&&
        g.FindPlayerPed&&g.FindPlayerVehicle&&g.CVehicle_IsDriver) {
        void* ped=g.FindPlayerPed(-1);
        void* vehicle=g.FindPlayerVehicle(-1,false);
        if (ped&&vehicle&&static_cast<const char*>(damage)==static_cast<char*>(vehicle)+0x758&&
            g.CVehicle_IsDriver(vehicle,ped)) return 0;
    }
    return status;
}
unsigned DoorStatusForDrive(const void* damage,int door) {
    return DoorStatusAt(damage,door,reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)));
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
Target g_editTarget{};
bool g_editInside{};
struct Grab { Target target{}; Motion motion{}; int hand{-1}; bool inside{}; } g_grab;
struct Release { Target target{}; ReleasedMotion motion{}; bool active{},inside{}; } g_release;
struct Door {
    Basis car{},panel{};
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
    if (!Finite(speed)||Length(speed)>.08f) return false; // below 14.4 km/h; opening briefly brakes NPC traffic
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
    d.panel=panel;
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
    float correction[3]{};
    driving::GetDoorCalibration(model,t.door,inside,correction);
    d.handle=d.handle+panel.Vector({correction[0],correction[1],correction[2]});
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
void ApplyDoor(const Target& t,float ratio) {
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
void Apply() { ApplyDoor(g_grab.target,g_grab.motion.Ratio()); }
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
    if (g_release.active&&g_release.target.vehicle==vehicle&&g_release.target.door==door&&
        Now()-g_updated<.2) {
        Pool p;void* ped=g.FindPlayerPed?g.FindPlayerPed(-1):nullptr;Door d;
        if (ped&&p.ReadCurrent()&&g_release.target.Live(p)&&ReadDoor(g_release.target,ped,d,g_release.inside)) {
            const auto& m=g_release.motion;
            ApplyDoor(g_release.target,std::clamp((m.angle-m.closed)/(m.open-m.closed),0.f,1.f));return;
        }
        g_release={};
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
bool HookAutomaticDriverClose(void* status,void* drive) {
    // Retail 2.11.311: ProcessPed asks GetDoorStatus(2), then starts the
    // rolling-close task for status 1. Restrict the override to that callsite.
    constexpr std::uint32_t expectedStatus[]={0x7100143f,0x54000088,0x8b214008,0x39402500,
                                              0xd65f03c0,0x52800080,0xd65f03c0};
    constexpr std::uint32_t expectedCall[]={0x911d6100,0x52800041,0x940712ab,0x7100041f,0x54000f21};
    if (!status||!drive||std::memcmp(status,expectedStatus,sizeof(expectedStatus))||
        std::memcmp(static_cast<char*>(drive)+0x3b8,expectedCall,sizeof(expectedCall))) return false;
    const long page=sysconf(_SC_PAGESIZE);
    if (page<=0) return false;
    const auto address=reinterpret_cast<std::uintptr_t>(status);
    const auto start=address&~(static_cast<std::uintptr_t>(page)-1);
    const auto end=(address+16+page-1)&~(static_cast<std::uintptr_t>(page)-1);
    if (mprotect(reinterpret_cast<void*>(start),end-start,PROT_READ|PROT_WRITE|PROT_EXEC)) return false;
    g_driveDoorStatusReturn=reinterpret_cast<std::uintptr_t>(drive)+0x3c4;
    auto* code=static_cast<std::uint32_t*>(status);
    const auto hook=reinterpret_cast<std::uintptr_t>(&DoorStatusForDrive);
    std::memcpy(code+2,&hook,8);code[1]=0xd61f0220;
    __atomic_store_n(code,0x58000051,__ATOMIC_RELEASE);
    __builtin___clear_cache(reinterpret_cast<char*>(code),reinterpret_cast<char*>(code)+16);
    return true;
}
void AddMarker(MarkerFrame& frame,const Door& door,bool held,bool inside=false) {
    if (frame.count>=MarkerFrame::kCapacity) return;
    Marker marker{door.handle,door.tangent,door.car.up,held};
    frame.markers[frame.count++]=inside?ReverseArrow(marker):marker;
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
    void* drive=dlsym(handle,"_ZN19CTaskSimpleCarDrive10ProcessPedEP4CPed");
    if (!HookAutomaticDriverClose(reinterpret_cast<void*>(g_status),drive))
        LOGE("[doors] automatic driver door close hook unavailable");
    g_ready=g_open&&g_canOpen&&g_inUse&&g_status&&g_setStatus&&g_frameLtm&&swing&&
        g.CPools_ms_pVehiclePool&&g.CVehicle_GetVehicleAppearance&&g.CWorld_ProcessLineOfSight&&Hook(swing);
    if (g_ready) LOGI("[doors] hand interaction installed");
    else LOGE("[doors] required symbols or verified hook unavailable; hand interaction disabled");
}
unsigned ClaimedHands() { return g_claimed.load(std::memory_order_relaxed); }
bool DebugEditActive() { return g_edit.hand>=0; }
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
void EndDebugEdit() {
    if(g_editDirty) driving::SaveDebugCalibration();
    g_edit={}; g_editDirty=false; g_editVehicle=nullptr;
    g_editTarget={}; g_editKind=0; g_secondHand=-1;
    for (auto& edge:g_editEdges) edge.latched=true;
}
void Reset() {
    EndDebugEdit();
    g_grab={};g_release={};g_claimed.store(0,std::memory_order_relaxed);
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
    const bool usable=!blocked&&xr::IsSessionFocused()&&trackingValid&&epoch==g_epoch&&g_ready&&ped&&pad&&
        !Read<std::uint8_t>(pad,0x12d)&&vrcam::VehicleDoorControlsActive()&&
        !(g.PlayerIsEnteringCar&&g.PlayerIsEnteringCar())&&
        !(g.CTimer_m_UserPause&&*g.CTimer_m_UserPause)&&
        !(g.CTimer_m_CodePause&&*g.CTimer_m_CodePause);
    g_epoch=epoch;
    const int editMode=driving::GetVehicleDebugMode();
    if(editMode!=g_editMode) { Reset();g_editMode=editMode; }
    bool fresh[2]{}, editFresh[2]{};
    for (int h=0;h<2;++h) {
        fresh[h]=g_edges[h].Step(hands[h].grip,usable&&hands[h].valid);
        editFresh[h]=g_editEdges[h].Step(hands[h].trigger,editMode&&usable&&hands[h].valid);
    }
    Pool pool;
    Vec eye{};float origin[3]{},forward[3]{};
    if (!usable||!pool.ReadCurrent()||!vrcam::GetHeadWorldRay(origin,forward)) { Reset();return; }
    eye={origin[0],origin[1],origin[2]};
    unsigned occupied=physicalweapon::HeldHandMaskRelaxed()|basketball::HeldHandMask()|npcgrab::ClaimedHands();
    void* seated=g.FindPlayerVehicle?g.FindPlayerVehicle(-1,false):nullptr;
    if (g_release.active) {
        Door d;
        if (!g_release.target.Live(pool)||(g_release.inside&&seated!=g_release.target.vehicle)||
            (!g_release.inside&&seated)||!ReadDoor(g_release.target,ped,d,g_release.inside)) g_release={};
        else {
            auto& m=g_release.motion;m.Step(dt);
            ApplyDoor(g_release.target,std::clamp((m.angle-m.closed)/(m.open-m.closed),0.f,1.f));
            if (m.Done()) g_release={};
        }
    }
    if (seated) {
        driving::WheelVisualState wheel{};
        driving::GetWheelVisualState(&wheel);
        for (int h=0;h<2;++h) if (wheel.grabbed[h]) occupied|=1u<<h;
    }
    for (int h=0;h<2;++h) if (physicalweapon::IsSupportHand(h)) occupied|=1u<<h;
    Vec world[2]{};bool valid[2]{};
    for (int h=0;h<2;++h) valid[h]=hands[h].valid&&ToWorld(hands[h].gripPos,world[h]);
    MarkerFrame markers;markers.epoch=epoch;
    // A normal grip remains authoritative in debug mode. Trigger editing and
    // grip manipulation are distinct gestures.
    if (g_grab.hand>=0) {
        const int h=g_grab.hand;Door d;
        if (!g_grab.target.Live(pool)||!valid[h]||(occupied&(1u<<h))||
            (g_grab.inside ? seated!=g_grab.target.vehicle : seated!=nullptr)||
            !ReadDoor(g_grab.target,ped,d,g_grab.inside)||Length(d.handle-world[h])>.7f||
            !Visible(eye,d.handle,g_grab.target.vehicle)||
            !g_grab.motion.Step(d.car.Local(world[h]),dt)) {
            // Losing the hand, line of sight or reach cancels the grab, but
            // must not hand a door opened from the seat to swinging physics.
            const auto grab=g_grab;
            const bool keep=grab.inside&&seated==grab.target.vehicle&&grab.target.Live(pool)&&
                ReadDoor(grab.target,ped,d,true)&&grab.motion.Ratio()>0;
            Reset();
            if (keep) {
                g_release.target=grab.target;g_release.inside=true;g_release.active=true;
                g_release.motion={grab.motion.angle,grab.motion.closed,grab.motion.open,0.f,false,true};
            }
            return;
        }
        Apply();
        if (hands[h].grip<=.35f) {
            const float velocity=g_grab.motion.ReleaseVelocity(dt);
            Write<float>(g_grab.target.vehicle,0x770+g_grab.target.door*0x18+0x14,velocity);
            const auto target=g_grab.target;const auto motion=g_grab.motion;const bool inside=g_grab.inside;
            const bool slam=motion.velocity*(motion.open-motion.closed)<0&&std::abs(motion.velocity)>=.6f&&motion.Ratio()>0;
            Reset();
            if ((inside&&motion.Ratio()>0)||slam) {
                g_release.target=target;g_release.inside=inside;g_release.active=true;
                // A slow closing movement stops where it is released; only a
                // deliberate throw should continue toward the latch.
                const float carried=inside&&motion.velocity*(motion.open-motion.closed)<0&&!slam?0.f:motion.velocity;
                g_release.motion={motion.angle,motion.closed,motion.open,
                    slam?std::copysign(std::max(std::abs(motion.velocity),.9f),motion.velocity):carried,slam,inside};
            }
            return;
        }
        ReadDoor(g_grab.target,ped,d,g_grab.inside);AddMarker(markers,d,true,g_grab.inside);
        Publish(markers);return;
    }

    int cockpitKind=g_editKind>=2?g_editKind:0;
    if(editMode && seated && g_edit.hand<0 && g.CVehicle_IsDriver && g.CVehicle_IsDriver(seated,ped)) {
        driving::WheelVisualState wheel{}; driving::GetWheelVisualState(&wheel);
        Vec wheelPoint{}; float seatEye[3]{};
        const bool wheelReady=wheel.dashAnchorValid&&!wheel.bike&&ToWorld(wheel.center,wheelPoint);
        const bool seatReady=vrcam::GetNeutralVehicleEyeWorld(seatEye);
        void* matrix=Read<void*>(seated,0x18);
        const Basis car=matrix?Matrix(matrix):Basis{};
        if(matrix&&car.Valid()) {
            if(wheelReady) markers.markers[markers.count++]={wheelPoint,car.forward,car.up,false};
            const Vec seatPoint{seatEye[0],seatEye[1],seatEye[2]};
            if(seatReady) markers.markers[markers.count++]={seatPoint,car.forward,car.up,false};
            float best=.24f;
            for(int h=0;h<2;++h) if(valid[h]&&editFresh[h]&&!(occupied&(1u<<h))) {
                if(wheelReady&&Length(world[h]-wheelPoint)<best) {best=Length(world[h]-wheelPoint);cockpitKind=2;}
                if(seatReady&&Length(world[h]-seatPoint)<best) {best=Length(world[h]-seatPoint);cockpitKind=3;}
            }
            // When the in-cabin door handle overlaps a cockpit marker, the
            // nearest target wins, independent of the order of marker passes.
            for(int slot=0;slot<kPoolSize;++slot) if(!(pool.flags[slot]&0x80)&&pool.objects+slot*kStride==seated) {
                Target target{seated,pool.identity,pool.objects,pool.flags,slot,2,kNodes[0],pool.flags[slot]};
                Door door{};
                if(ReadDoor(target,ped,door,true)&&Visible(eye,door.handle,seated))
                    for(int h=0;h<2;++h) if(valid[h]&&editFresh[h]&&!(occupied&(1u<<h))&&Length(world[h]-door.handle)<best) {
                        best=Length(world[h]-door.handle);cockpitKind=0;
                    }
                break;
            }
        }
    }
    if(editMode && cockpitKind>=2) {
        // Cockpit edits require a live driver, stationary vehicle and first-person
        // wheel geometry. Never retain an engine pointer across a failed check.
        if(!seated||!g.CVehicle_IsDriver) { Reset();return; }
        int slot=-1;
        for(int i=0;i<kPoolSize;++i) if(!(pool.flags[i]&0x80)&&pool.objects+i*kStride==seated) slot=i;
        if(slot<0||!g.CVehicle_IsDriver(seated,ped)||!Finite(Read<Vec>(seated,0x68))||Length(Read<Vec>(seated,0x68))>.01f) { Reset();return; }
        void* matrix=Read<void*>(seated,0x18);
        if(!matrix) { Reset();return; }
        const Basis car=Matrix(matrix);
        driving::WheelVisualState wheel{};
        driving::GetWheelVisualState(&wheel);
        const int model=Read<std::uint16_t>(seated,0x32);
        const int type=driving::GetActiveVehicleType();
        const int mode=driving::GetModeForVehicleType(type);
        Vec point{};
        if(!car.Valid()||!wheel.dashAnchorValid||wheel.modelId!=model||wheel.bike||
           !ToWorld(wheel.center,point)) { Reset();return; }
        if(cockpitKind==3) {
            float neutral[3]{};
            if(!vrcam::GetNeutralVehicleEyeWorld(neutral)) { Reset();return; }
            point={neutral[0],neutral[1],neutral[2]};
            if(!Finite(point)||Length(point-car.position)>15.f) { Reset();return; }
        }
        if(g_edit.hand>=0) {
            const int h=g_edit.hand;
            if(seated!=g_editVehicle||model!=g_editModel||mode!=g_editDrivingMode||
               !g_editTarget.Live(pool)||!valid[h]||!std::isfinite(hands[h].trigger)||
               !g_edit.Step(car.Local(world[h]))) { Reset();return; }
            int value[3]{};
            const float delta[]={g_edit.delta.x,g_edit.delta.y,g_edit.delta.z};
            for(int a=0;a<3;++a) value[a]=g_editBase[a]+std::lround(delta[a]*100.f);
            if(cockpitKind==2 && hands[h].trigger>.35f && g_secondHand<0) {
                const int other=1-h;
                const float distance=valid[other]?Length(world[other]-point):1000.f;
                if(valid[other]&&editFresh[other]&&!(occupied&(1u<<other))&&
                   std::abs(distance-wheel.radius)<.18f) {
                    const float span=Length(world[other]-world[h]);
                    if(std::isfinite(span)&&span>.1f) {
                        g_secondHand=other;g_scaleSpan=span;
                        g_scaleBase=driving::GetWheelCalibrationValue(driving::WHEEL_CAL_RADIUS);
                        xr::TriggerHaptic(other,.25f,0,30);
                    }
                }
            }
            if(cockpitKind==2 && g_secondHand>=0) {
                const int other=g_secondHand;
                if(!valid[other]||!std::isfinite(hands[other].trigger)) { Reset();return; }
                if(hands[other].trigger<=.35f) {
                    g_secondHand=-1;
                    g_edit.Begin(h,car.Local(world[h]));
                    for(int a=0;a<3;++a) value[a]=g_editBase[a]=driving::GetWheelCalibrationValue(a);
                } else {
                    const float span=Length(world[other]-world[h]);
                    if(!std::isfinite(span)||std::abs(span-g_scaleSpan)>1.f) { Reset();return; }
                    driving::SetDebugWheelRadius(model,g_scaleBase+std::lround((span-g_scaleSpan)*50.f));
                    g_editDirty=true;
                }
            }
            // Seat movement moves the tracking camera. Preview the target only;
            // commit on release to avoid dragging against a moving coordinate origin.
            if((cockpitKind==2&&g_secondHand<0)||
               (cockpitKind==3&&hands[h].trigger<=.35f)) {
                driving::SetDebugPosition(model,mode,cockpitKind==3,value);g_editDirty=true;
            }
            if(cockpitKind==3) point=point+car.Vector(g_edit.delta);
            if(hands[h].trigger<=.35f) { EndDebugEdit();g_claimed.store(0);return; }
        } else {
            for(int h=0;h<2;++h) if(valid[h]&&editFresh[h]&&!(occupied&(1u<<h))&&Length(world[h]-point)<.24f) {
                g_edit.Begin(h,car.Local(world[h]));g_editVehicle=seated;
                g_editModel=model;g_editDrivingMode=mode;g_editKind=cockpitKind;
                g_editTarget={seated,pool.identity,pool.objects,pool.flags,slot,0,0,pool.flags[slot]};
                if(cockpitKind==3) {
                    g_editBase[0]=driving::GetCurrentModelSeatSideCm(type);
                    g_editBase[1]=driving::GetCurrentModelSeatForwardCm(type);
                    g_editBase[2]=driving::GetCurrentModelSeatHeightCm(type);
                } else for(int a=0;a<3;++a) g_editBase[a]=driving::GetWheelCalibrationValue(a);
                xr::TriggerHaptic(h,.25f,0,30);break;
            }
        }
        if(g_edit.hand>=0) g_claimed.store((1u<<g_edit.hand)|(g_secondHand>=0?1u<<g_secondHand:0u),std::memory_order_relaxed);
        bool existing=false;
        for(int i=0;i<markers.count;++i) if(Length(markers.markers[i].position-point)<.001f) {
            markers.markers[i].held=g_edit.hand>=0;existing=true;break;
        }
        if(!existing&&markers.count<MarkerFrame::kCapacity)
            markers.markers[markers.count++]={point,car.forward,car.up,g_edit.hand>=0};
        Publish(markers);return;
    }
    if(editMode&&g_editKind==1&&g_edit.hand>=0) {
        const int h=g_edit.hand;Door d;
        if(!g_editTarget.Live(pool)||!valid[h]||!std::isfinite(hands[h].trigger)||
           (g_editInside?seated!=g_editTarget.vehicle:seated!=nullptr)||
           !ReadDoor(g_editTarget,ped,d,g_editInside)||
           !g_edit.Step(d.panel.Local(world[h]))) { Reset();return; }
        const Vec value=g_editDoorBase+g_edit.delta;
        const float correction[]={value.x,value.y,value.z};
        driving::SetDoorCalibration(g_editModel,g_editTarget.door,g_editInside,correction);
        g_editDirty=true;
        if(hands[h].trigger<=.35f) { EndDebugEdit();g_claimed.store(0);return; }
        g_claimed.store(1u<<h,std::memory_order_relaxed);
        ReadDoor(g_editTarget,ped,d,g_editInside);AddMarker(markers,d,true,g_editInside);
        Publish(markers);return;
    }
    float best=seated?.34f:.23f;Target chosen{};Door chosenDoor{};int chosenHand=-1;
    float bestEdit=best;Target editTarget{};Door editDoor{};int editHand=-1;
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
        AddMarker(markers,d,false,seated!=nullptr);
        for (int h=0;h<2;++h) {
            if (!valid[h]||(occupied&(1u<<h))) continue;
            const float distance=Length(world[h]-d.handle);
            if (fresh[h]&&distance<best) { best=distance;chosen=t;chosenDoor=d;chosenHand=h; }
            if (editMode&&editFresh[h]&&distance<bestEdit) {bestEdit=distance;editTarget=t;editDoor=d;editHand=h;}
        }
    }
    if(editMode&&editHand>=0) {
        g_editTarget=editTarget;g_editInside=seated!=nullptr;g_editKind=1;
        g_edit.Begin(editHand,editDoor.panel.Local(world[editHand]));
        g_editModel=Read<std::uint16_t>(editTarget.vehicle,0x32);
        float base[3]{};driving::GetDoorCalibration(g_editModel,editTarget.door,g_editInside,base);
        g_editDoorBase={base[0],base[1],base[2]};
        g_claimed.store(1u<<editHand,std::memory_order_relaxed);
        xr::TriggerHaptic(editHand,.25f,0,30);
        Publish(markers);return;
    }
    if (chosenHand>=0&&g_grab.motion.Begin(chosenDoor.hinge,chosenDoor.car.Local(world[chosenHand]),
                                         chosenDoor.angle,chosenDoor.closed,chosenDoor.open)) {
        if (g_release.active&&g_release.target.vehicle==chosen.vehicle&&g_release.target.door==chosen.door) g_release={};
        g_grab.target=chosen;g_grab.hand=chosenHand;
        g_grab.inside=seated!=nullptr;
        if (!g_grab.inside&&g_grab.motion.Ratio()<.5f) npcgrab::OnDoorOpening(chosen.vehicle);
        g_claimed.store(1u<<chosenHand,std::memory_order_relaxed);
        Apply();markers.count=0;AddMarker(markers,chosenDoor,true,g_grab.inside);
        xr::TriggerHaptic(chosenHand,.25f,0,30);
        LOGI("[doors] grabbed model=%d door=%d hand=%d",Read<std::uint16_t>(chosen.vehicle,0x32),chosen.door,chosenHand);
    }
    Publish(markers);
}
} // namespace savr::vehicledoors
