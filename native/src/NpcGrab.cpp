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
#include <sys/mman.h>
#include <unistd.h>

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
void (*standCtor)(void*,int,bool,bool,float){};
void (*calculateVelocity)(void*){};
void (*setTask)(void*,void*,int,bool){};
void (*outCtor)(void*,void*,int,bool){};
bool (*outProcess)(void*,void*){};
void (*outDtor)(void*){};
bool (*placeOutside)(void*,int,void*,bool){};
bool (*canOpen)(const void*,const void*){};
void (*draggedCtor)(void*,const void*,const void*,bool){};
void (*draggedDtor)(void*){};
void (*addPrimary)(void*,void*,bool){};
std::atomic<unsigned> claimed{};
GripEdge edges[2];
bool intent[2]{};
bool extracted[2]{};
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
    Vec command{};
    double until{};
    bool aggression{};
    bool Live(const Pool& p) const {
        return ped&&p.identity==pool&&p.objects==objects&&ref>=0&&(ref>>8)<p.size&&
            p.flags[ref>>8]==(ref&255)&&!(p.flags[ref>>8]&128)&&getPed(ref)==ped;
    }
} holds[2];
Hold flights[4];
unsigned nextFlight{};
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
void Aggression(Hold& h,void* player) {
    if (h.aggression||!player||playerlife::Dead(h.ped)) return;
    h.aggression=true;
    const Vec relative=Position(player)-Position(h.ped);
    const GameSymbols::Vec2 direction{relative.x,relative.y};
    // The vanilla unarmed damage event reports the player as the attacker and
    // lets the native personality/crime/proof rules decide the response. Zero
    // damage avoids turning a shove into repeated artificial health damage.
    g.CWeapon_GenerateDamageEvent(h.ped,player,0,0,3,
        std::clamp(g.CPed_GetLocalDirection(h.ped,&direction),0,3));
}
void Release(Hold& h,const Pool& pool,bool throwing) {
    if (h.Live(pool)&&h.task&&OwnTask(h)) {
        const Vec velocity=throwing?Limit(h.velocity,8.f):Vec{};
        const bool toss=throwing&&Length(velocity)>=1.2f&&!playerlife::Dead(h.ped);
        // StandStill only suspends AI while held. Fall is a consequence of a
        // throw, never of touching/grabbing someone. Restore the lower tasks.
        if (toss) {
            void* task=allocate(0x28);
            if (task) {
                fallCtor(task,0,1000);
                setTask(Manager(h.ped),task,0,false);
                h.task=Read<void*>(Manager(h.ped),0)==task?task:nullptr;
                if (h.task) {
                    h.until=updated+.85;
                    flights[nextFlight++%4]=h;
                }
            } else setTask(Manager(h.ped),nullptr,0,false);
        } else setTask(Manager(h.ped),nullptr,0,false);
        if (!playerlife::Dead(h.ped)) Write(h.ped,0x68,velocity*.02f);
        if (throwing&&Length(velocity)>=.6f) Aggression(h,owner);
    }
    h={};
}
bool Visible(Vec from,Vec to,void* ped,void* ignored=nullptr) {
    alignas(8) std::uint8_t col[0x2c]{};void* hit{};
    const GameSymbols::Vec3 a{from.x,from.y,from.z},b{to.x,to.y,to.z};
    // A validated open door permits looking/reaching through that occupant's
    // car collision mesh. Buildings, other cars and objects still occlude.
    if (ignored&&!g.CWorld_pIgnoreEntity) return false;
    void* previous=g.CWorld_pIgnoreEntity?*g.CWorld_pIgnoreEntity:nullptr;
    if (g.CWorld_pIgnoreEntity&&ignored) *g.CWorld_pIgnoreEntity=ignored;
    const bool visible=!g.CWorld_ProcessLineOfSight(&a,&b,col,&hit,true,true,true,true,true,false,false,false)||hit==ped;
    if (g.CWorld_pIgnoreEntity&&ignored) *g.CWorld_pIgnoreEntity=previous;
    return visible;
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
bool StartHold(Hold& h) {
    void* manager=Manager(h.ped);
    // Do not delete an existing physical-response task (death, arrest, etc.).
    if (!manager||Read<void*>(manager,0)) return false;
    void* task=allocate(0x28);
    if (!task) return false;
    standCtor(task,-1,false,false,8.f);
    setTask(manager,task,0,false);
    h.task=Read<void*>(manager,0)==task?task:nullptr;
    return h.task!=nullptr;
}
bool SeatReachable(void* ped,int door,Vec eye,Vec hand) {
    void* car=Read<void*>(ped,0x708);
    void* matrix=Read<void*>(car,0x18);
    if (!matrix) return false;
    const Basis basis{Read<Vec>(matrix,0),Read<Vec>(matrix,0x10),Read<Vec>(matrix,0x20),Read<Vec>(matrix,0x30)};
    if (!basis.Valid()) return false;
    const float side=door==2||door==4?-1.f:1.f;
    const Vec localEye=basis.Local(eye),localHand=basis.Local(hand);
    return localEye.x*side>.45f&&localHand.x*side>-.1f&&Length(eye-hand)<1.6f;
}
bool Extract(Hold& h,void* player) {
    const int door=ExitDoor(h.ped,player);
    if (door<0) return false;
    // Set-out normally follows an animation that already moved the ped outside.
    // A direct VR pull must perform the collision-safe placement first. These
    // helpers take component nodes, not damage-manager door indices.
    constexpr int nodes[]={10,8,11,9};
    void* car=Read<void*>(h.ped,0x708);
    const bool wasDriver=Read<void*>(car,0x5a0)==h.ped;
    if (!placeOutside(h.ped,nodes[door-2],car,true)) return false;
    alignas(16) std::uint8_t task[0x30]{};
    outCtor(task,car,nodes[door-2],false);
    const bool done=outProcess(task,h.ped);
    outDtor(task);
    if (!done||Seated(h.ped)) return false;
    void* intelligence=Read<void*>(h.ped,0x538);
    // The normal F/carjack path replaces the victim's primary car task using
    // AddTaskPrimaryMaybeInGroup, then adds DraggedOutCar to intelligence+c8
    // (EnterCar::CreateNextSubTask @635f1c). Merely clearing bInVehicle leaves
    // CarDrive/EnterCar underneath and sends the NPC straight back to the seat.
    if (void* idle=allocate(0x28)) {
        standCtor(idle,0,false,false,8.f);
        addPrimary(intelligence,idle,true);
    }
    // Fall occupies physical response only; personality responses can queue
    // underneath and run after getting up. The hand releases at extraction.
    if (void* fall=allocate(0x28)) {
        fallCtor(fall,0,1200);
        setTask(Manager(h.ped),fall,0,false);
    }
    alignas(16) std::uint8_t event[0x30]{};
    draggedCtor(event,car,player,wasDriver);
    g.CEventGroup_Add(static_cast<char*>(intelligence)+0xc8,event,true);
    draggedDtor(event); // Add clones the event and owns its entity references.
    return true;
}

void OnCalculateVelocity(void* ped) {
    const Vec physical=Read<Vec>(ped,0x68);
    calculateVelocity(ped);
    Pool pool;
    if (!pool.Load()||!xr::IsSessionFocused()) return;
    const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now-updated>.2) return;
    const auto apply=[&](const Hold& h,bool flying) {
        if (h.ped!=ped||!h.Live(pool)||!OwnTask(h)||Seated(ped)||playerlife::Dead(ped)) return false;
        const Vec velocity=flying?physical:h.command;
        if (!Finite(velocity)) return false;
        Write(ped,0x68,velocity);
        // UpdatePosition blends move speed towards m_vecAnimMovingShift.
        // Matching the shift prevents idle/fall root motion braking the body.
        Write(ped,0x6bc,velocity.x);Write(ped,0x6c0,velocity.y);
        return true;
    };
    for (const auto& h:holds) if (!h.seated&&apply(h,false)) return;
    for (auto& h:flights) {
        if (now>=h.until) { h={};continue; }
        if (apply(h,true)) return;
    }
}
bool HookVelocity(void* target) {
    // Only stack saves: no PC-relative instructions need relocating.
    constexpr std::uint32_t expected[]={0x6dbb2beb,0x6d0123e9,0xa9027bfd,0xf9001bf5};
    if (!target||std::memcmp(target,expected,sizeof(expected))) return false;
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
    calculateVelocity=reinterpret_cast<decltype(calculateVelocity)>(trampoline);
    const auto hook=reinterpret_cast<std::uintptr_t>(&OnCalculateVelocity);
    std::memcpy(code+2,&hook,8);code[1]=0xd61f0220;
    __atomic_store_n(code,0x58000051,__ATOMIC_RELEASE);
    __builtin___clear_cache(reinterpret_cast<char*>(code),reinterpret_cast<char*>(code)+16);
    return true;
}
} // namespace

void Install(void* handle) {
#define RESOLVE(target,name) target=reinterpret_cast<decltype(target)>(dlsym(handle,name))
    RESOLVE(pedPool,"_ZN6CPools11ms_pPedPoolE");
    RESOLVE(getPed,"_ZN6CPools6GetPedEi");
    RESOLVE(isPlayer,"_ZNK4CPed8IsPlayerEv");
    RESOLVE(allocate,"_ZN5CTasknwEm");
    RESOLVE(fallCtor,"_ZN24CTaskComplexFallAndGetUpC1Eii");
    RESOLVE(standCtor,"_ZN21CTaskSimpleStandStillC1Eibbf");
    RESOLVE(setTask,"_ZN12CTaskManager7SetTaskEP5CTaskib");
    RESOLVE(outCtor,"_ZN23CTaskSimpleCarSetPedOutC1EP8CVehicleib");
    RESOLVE(outProcess,"_ZN23CTaskSimpleCarSetPedOut10ProcessPedEP4CPed");
    RESOLVE(outDtor,"_ZN23CTaskSimpleCarSetPedOutD1Ev");
    RESOLVE(placeOutside,"_ZN4CPed25PositionPedOutOfCollisionEiP8CVehicleb");
    RESOLVE(canOpen,"_ZNK8CVehicle15CanPedOpenLocksEPK4CPed");
    RESOLVE(draggedCtor,"_ZN19CEventDraggedOutCarC1EPK8CVehiclePK4CPedb");
    RESOLVE(draggedDtor,"_ZN19CEventDraggedOutCarD1Ev");
    RESOLVE(addPrimary,"_ZN16CPedIntelligence26AddTaskPrimaryMaybeInGroupEP5CTaskb");
#undef RESOLVE
    ready=pedPool&&getPed&&isPlayer&&allocate&&fallCtor&&standCtor&&setTask&&
        outCtor&&outProcess&&outDtor&&placeOutside&&canOpen&&g.CPed_GetBonePosition&&
        draggedCtor&&draggedDtor&&addPrimary&&g.CEventGroup_Add&&
        g.CWeapon_GenerateDamageEvent&&g.CPed_GetLocalDirection&&
        g.CWorld_ProcessLineOfSight&&g.CVehicle_GetVehicleAppearance&&
        HookVelocity(dlsym(handle,"_ZN4CPed20CalculateNewVelocityEv"));
    LOGI("[npcgrab] available=%d",ready);
}
unsigned ClaimedHands() { return claimed.load(std::memory_order_relaxed); }
void Reset() {
    Pool pool;pool.Load();
    for (auto& h:holds) Release(h,pool,false);
    for (auto& edge:edges) edge.latched=true;
    for (auto& active:intent) active=false;
    for (auto& released:extracted) released=false;
    for (auto& flight:flights) flight={};
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
        if (fresh) intent[hand]=true;
        if (!good||hands[hand].grip<=.35f) intent[hand]=false;
        if (extracted[hand]) {
            // Finish the same grab gesture before returning the hand to melee
            // or holsters; otherwise the extraction sweep becomes a punch.
            if (good&&hands[hand].grip>.35f) { mask|=1u<<hand;continue; }
            extracted[hand]=false;
        }
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
                    if (Extract(h,player)) {
                        h={};extracted[hand]=true;mask|=1u<<hand;
                    }
                    else Release(h,pool,false);
                    continue;
                }
            }
            if (!h.seated) {
                if (Seated(h.ped)||!OwnTask(h)) { Release(h,pool,false);continue; }
                const Vec anchor=Bone(h.ped,h.bone);
                if (!Finite(anchor)||Length(p-anchor)>1.5f||!Visible(eye,anchor,h.ped)) {
                    Release(h,pool,false);continue;
                }
                // Native CPhysical collision/gravity remains active. Move speed
                // uses metres per stock 50 Hz tick, not metres per second.
                const Vec speed=Limit((p-anchor)*10.f+h.velocity*.65f+Vec{0,0,.4f},6.f);
                h.command=speed*.02f;
                Write(h.ped,0x68,h.command);
                if (Length(p-h.start)>.12f&&Length(h.velocity)>.6f) Aggression(h,player);
            }
        } else if (intent[hand]&&hands[hand].grip>=.65f&&good) {
            float best=.35f;
            Hold chosen{};
            for (int slot=0;slot<pool.size;++slot) {
                if (pool.flags[slot]&128) continue;
                const int ref=(slot<<8)|pool.flags[slot];
                void* ped=getPed(ref);
                if (!ped||ped==player||ped==holds[1-hand].ped||isPlayer(ped)||playerlife::Dead(ped)||
                    !Finite(Position(ped))||Length(Position(ped)-Position(player))>3.f) continue;
                const bool seated=Seated(ped);
                const int door=seated?ExitDoor(ped,player):-1;
                if (seated&&(door<0||!SeatReachable(ped,door,eye,p))) continue;
                void* manager=Manager(ped);
                if (!manager||Read<void*>(manager,0)) continue;
                for (int bone:{3,4,5}) {
                    const Vec point=Bone(ped,bone);
                    const float distance=Length(p-point);
                    void* ignored=seated?Read<void*>(ped,0x708):nullptr;
                    if (Finite(point)&&distance<best&&Visible(eye,point,ped,ignored)&&Visible(p,point,ped,ignored)) {
                        best=distance;chosen={ped,pool.identity,pool.objects,nullptr,ref,bone,p,{},p,seated};
                    }
                }
            }
            if (chosen.ped&&(chosen.seated||StartHold(chosen))) {
                intent[hand]=false;
                h=chosen;xr::TriggerHaptic(hand,.45f,80.f,35.f);
            }
        }
        if (h.ped) mask|=1u<<hand;
    }
    claimed.store(mask,std::memory_order_relaxed);
}
}
