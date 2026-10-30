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
void* (*simplestTask)(void*){};
void (*startDraggedAnimation)(void*,const void*){};
void* draggedAnimationVtable{};
void (*dragTaskCtor)(void*,void*,int){};
void (*registerReference)(void*,void**){};
void (*cleanReference)(void*,void**){};
bool (*originalPlacement)(void*,int,void*,bool){};
GameSymbols::Vec3 (*doorPosition)(const void*,int){};
bool (*groundPosition)(GameSymbols::Vec3*){};
void (*worldRemove)(void*){};
void (*worldAdd)(void*){};
bool (*canOpen)(const void*,const void*){};
void (*draggedCtor)(void*,const void*,const void*,bool){};
void (*draggedDtor)(void*){};
void* (*association)(void*,unsigned){};
void (*setAnimationTime)(void*,float){};
bool (*vehicleAI)(void*,unsigned*){};
void (*vehicleControl)(void*){};
void (*vehicleRails)(void*){};
void (*lineUpPed)(void*,void*,void*,void*){};
void (*seatPosition)(void*){};
void (*realPhysics)(void*){};
std::atomic<unsigned> claimed{};
GripEdge edges[2];
bool intent[2]{};
bool extracted[2]{};
std::uint64_t previousEpoch{};
double updated{};
bool ready{};
void* owner{};
MarkerFrame worldMarkers{};
struct HandMotion { Vec previous{};bool valid{},clenched{}; } handMotion[2];
struct Feedback { Marker marker{};double started{};bool active{}; } feedback[4];
unsigned nextFeedback{};
Vec Bone(void* ped,int bone);
void ActionFeedback(void* ped,bool fist) {
    const Vec head=Bone(ped,fist?5:4);
    for (int i=0;i<worldMarkers.count;++i) {
        const auto& marker=worldMarkers.markers[i];
        if (marker.fist==fist&&Length(marker.position-head)<.5f) {
            feedback[nextFeedback++%4]={marker,updated,true};return;
        }
    }
}

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
        return ped&&p.flags&&p.size>0&&p.size<=1024&&p.identity==pool&&p.objects==objects&&ref>=0&&(ref>>8)<p.size&&
            p.flags[ref>>8]==(ref&255)&&!(p.flags[ref>>8]&128)&&getPed(ref)==ped;
    }
} holds[2];
Hold approach[2];
Hold flights[4];
unsigned nextFlight{};
Hold preparationBlocked[4];
unsigned nextPreparationBlocked{};
bool PreparationBlocked(void* ped) {
    for (const auto& target:preparationBlocked) if (target.ped==ped) return true;
    return false;
}
enum class Phase { Preparing,AwaitHit,Hit,Waiting,Pull };
struct Animation {
    Hold target{};
    void* car{}; // registered engine reference, stable storage
    double until{};
    Phase phase{Phase::Hit};
    double waitUntil{};
    double punchAfter{};
    float speed{1.f};
    void* task{};
    void* pausedAssociation{};
    bool synchronized{};
} animations[4];
struct TrafficStop { void* car{};double until{}; } stops[4];
double Now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
Animation* Waiting(void* ped) {
    for (auto& a:animations) if (a.target.ped==ped&&a.phase==Phase::Waiting) return &a;
    return nullptr;
}
Animation* BeforeHit(void* ped) {
    for (auto& a:animations) if (a.target.ped==ped&&
        (a.phase==Phase::Preparing||a.phase==Phase::AwaitHit)) return &a;
    return nullptr;
}
struct PausePoint { const char* name;float seconds;std::uint32_t key{};float start{}; } pausePoints[]={
    // INU imports IFP time at fixed 30 fps with frame zero at time zero.
    {"CAR_jackedLHS",30.f/30.f,0,15.f/30.f},
    {"CAR_LjackedLHS",45.f/30.f,0,25.f/30.f},
    {"CAR_LjackedRHS",45.f/30.f,0,26.f/30.f},
    {"CAR_jackedRHS",45.f/30.f,0,20.f/30.f},
};
const PausePoint* AnimationPoint(void* anim) {
    void* hierarchy=Read<void*>(anim,0x28);
    if (hierarchy) {
        const auto key=Read<std::uint32_t>(hierarchy,0);
        for (const auto& point:pausePoints) if (point.key&&point.key==key) return &point;
    }
    return nullptr;
}
float PauseTime(void* anim) {
    // Retail arm64 Init stores m_BlendHier at +0x28; SetName stores its
    // uppercase CKeyGen hash at +0. IDs 378/379 alone do not identify the clip.
    if (const auto* point=AnimationPoint(anim)) return point->seconds;
    return .35f; // Unreviewed vehicle-specific clips stay unchanged.
}
void* VictimAnimation(void* ped) {
    void* clump=Read<void*>(ped,0x20);
    if (!clump) return nullptr;
    void* anim=association(clump,378);
    return anim?anim:association(clump,379);
}
void ResumePose(Animation& a) {
    void* anim=VictimAnimation(a.target.ped);
    if (anim&&anim==a.pausedAssociation) {
        Write(anim,0x3c,a.speed);Write(anim,0x40,0.f);
        const auto flags=Read<std::uint16_t>(anim,0x46);
        Write<std::uint16_t>(anim,0x46,(flags&~0x20u)|(a.synchronized?0x20u:0u));
    }
    a.pausedAssociation=nullptr;
}
void PausePose(Animation& a,void* anim,float time) {
    a.speed=Read<float>(anim,0x3c);
    if (!std::isfinite(a.speed)||a.speed<=0) a.speed=1.f;
    setAnimationTime(anim,time);
    a.pausedAssociation=anim;
    const auto flags=Read<std::uint16_t>(anim,0x46);
    a.synchronized=(flags&0x20u)!=0;
    Write<std::uint16_t>(anim,0x46,flags&~0x20u);
    Write(anim,0x3c,0.f);Write(anim,0x40,0.f);
}
void ClearStop(TrafficStop& stop) {
    if (stop.car) cleanReference(stop.car,&stop.car);
    stop={};
}
bool StopCar(void* car,double now) {
    bool scene=false;
    for (const auto& a:animations) if (a.car==car) scene=true;
    void* driver=Read<void*>(car,0x5a0);
    if ((!scene&&!driver)||(driver&&isPlayer(driver))||
        (g.FindPlayerVehicle&&g.FindPlayerVehicle(-1,false)==car)) return false;
    bool stop=scene;
    for (const auto& s:stops) if (s.car==car&&now<s.until) stop=true;
    if (!stop) return false;
    const Vec speed=Read<Vec>(car,0x68);
    // The speed guard applies only to the temporary door-opening stop. Once a
    // slow-car scene has started, acceleration/impact must not release its lock.
    if (!Finite(speed)||(!scene&&Length(speed)>.12f)) return false;
    if ((Read<std::uint8_t>(car,0x5a)>>3)==2) realPhysics(car);
    Write(car,0x68,Vec{0,0,speed.z});
    Write(car,0x608,0.f);Write(car,0x60c,1.f); // gas/brake, retail ProcessAI
    return true;
}
bool OnVehicleAI(void* car,unsigned* flags) {
    StopCar(car,Now()); // convert SIMPLE traffic before its AI can move it on rails
    const bool result=vehicleAI(car,flags);
    StopCar(car,Now()); // after the native driver has chosen acceleration/braking
    return result;
}
void OnVehicleControl(void* car) {
    if (StopCar(car,Now())) {
        Write(car,0x68,Vec{});Write(car,0x74,Vec{});
        return; // no propulsion/integration while this NPC scene owns the car
    }
    vehicleControl(car);
}
void OnVehicleRails(void* car) {
    if (!StopCar(car,Now())) vehicleRails(car);
}
void* Manager(void* ped);
bool Seated(void* ped);
void OnLineUpPed(void* utility,void* ped,void* car,void* anim) {
    Pool pool;
    if (pool.Load()) for (const auto& a:animations) if (a.target.ped==ped&&a.car==car&&
        a.phase!=Phase::Pull&&a.target.Live(pool)&&Seated(ped)) {
        void* manager=Manager(ped);
        if (!manager||Read<void*>(manager,3*8)!=a.task) break;
        void* leaf=simplestTask(a.task);
        if (leaf&&Read<void*>(leaf,0)==draggedAnimationVtable&&Read<void*>(leaf,0x30)==utility) {
            seatPosition(ped);return; // recoil stays in the original seat until the grip
        }
    }
    lineUpPed(utility,ped,car,anim);
}
void ClearAnimation(Animation& animation) {
    if (animation.car) cleanReference(animation.car,&animation.car);
    animation={};
}
void PruneAnimations(const Pool& pool,double now);

bool Animating(void* ped) {
    for (const auto& a:animations) if (a.target.ped==ped&&
        a.phase!=Phase::Waiting&&a.phase!=Phase::AwaitHit) return true;
    return false;
}
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
bool FindDoorExit(void* ped,void* car,int door,Vec& destination) {
    constexpr int nodes[]={10,8,11,9};
    void* matrix=Read<void*>(car,0x18);
    if (!matrix) return false;
    const Basis basis{Read<Vec>(matrix,0),Read<Vec>(matrix,0x10),Read<Vec>(matrix,0x20),Read<Vec>(matrix,0x30)};
    if (!basis.Valid()) return false;
    const auto native=doorPosition(car,nodes[door-2]);
    const Vec origin{native.x,native.y,native.z};
    if (!Finite(origin)||Length(origin-Position(ped))>2.5f) return false;
    const float side=door==2||door==4?-1.f:1.f;
    const Vec outward=basis.right*side;
    // Bounded doorway search, independent of the car's heading or length.
    // PositionPedOutOfCollision searches the entire car perimeter/path nodes
    // when CJ blocks the door and can put the victim behind the boot.
    for (float distance:{0.f,.25f,.5f}) for (float along:{0.f,.25f,-.25f,.5f,-.5f}) {
        Vec point=origin+outward*distance+basis.forward*along;
        GameSymbols::Vec3 ground{point.x,point.y,point.z};
        if (!groundPosition(&ground)) continue;
        point={ground.x,ground.y,ground.z};
        if (!Finite(point)||std::abs(point.z-Position(ped).z)>.75f) continue;
        bool clear=true;
        // Two body spheres cover feet-to-head clearance and include CJ, other
        // peds and this car. Try a nearby point if the player occupies the first.
        for (float height:{-.35f,.35f}) {
            const GameSymbols::Vec3 centre{point.x,point.y,point.z+height};
            if (g.CWorld_TestSphereAgainstWorld(centre,.35f,ped,true,true,true,true,true,false)) {
                clear=false;break;
            }
        }
        if (!clear) continue;
        const Vec start=Position(ped);
        const GameSymbols::Vec3 from{start.x,start.y,start.z},to{point.x,point.y,point.z};
        alignas(8) std::uint8_t col[0x2c]{};void* hit{};
        void* previous=*g.CWorld_pIgnoreEntity;
        *g.CWorld_pIgnoreEntity=car;
        const bool blocked=g.CWorld_ProcessLineOfSight(&from,&to,col,&hit,
            true,true,false,true,true,false,false,false);
        *g.CWorld_pIgnoreEntity=previous;
        if (blocked) continue;
        destination=point;
        return true;
    }
    return false;
}
bool OnPlacement(void* ped,int node,void* car,bool search) {
    Pool pool;
    if (pool.Load()) for (const auto& animation:animations) {
        if (animation.car&&(car==animation.car||!car)&&animation.target.ped==ped&&animation.target.Live(pool)) {
            const int door=node==10?2:node==8?3:node==11?4:node==9?5:-1;
            Vec point{};
            if (door<0||!FindDoorExit(ped,animation.car,door,point)) return false;
            worldRemove(ped);
            if (void* matrix=Read<void*>(ped,0x18)) Write(matrix,0x30,point);
            else Write(ped,8,point);
            Write(ped,0x68,Vec{});Write(ped,0x74,Vec{});
            worldAdd(ped);
            return true;
        }
    }
    return originalPlacement(ped,node,car,search);
}
void ReportCarjack(Animation& pending,void* player) {
    alignas(16) std::uint8_t event[0x30]{};
    draggedCtor(event,pending.car,player,Read<void*>(pending.car,0x5a0)==pending.target.ped);
    void* brain=Read<void*>(pending.target.ped,0x538);
    g.CEventGroup_Add(static_cast<char*>(brain)+0xc8,event,true);
    draggedDtor(event);
}
bool BeginHit(Hold& h,bool preparing=false) {
    const int door=ExitDoor(h.ped,owner);
    if (!preparing) if (auto* pending=BeforeHit(h.ped)) {
        if (door<0||pending->phase!=Phase::AwaitHit||updated<pending->punchAfter) return false;
        void* anim=VictimAnimation(h.ped);
        if (!anim) return false;
        ResumePose(*pending);
        ActionFeedback(h.ped,true);
        pending->phase=Phase::Hit;pending->until=updated+8.0;
        return true;
    }
    if (!preparing) return false; // door-opening motion cannot bypass preparation
    if (door<0||Animating(h.ped)||Waiting(h.ped)) return false;
    if (BeforeHit(h.ped)) return false;
    constexpr int nodes[]={10,8,11,9};
    void* car=Read<void*>(h.ped,0x708);
    Animation* pending=nullptr;
    for (auto& a:animations) if (!a.target.ped) { pending=&a;break; }
    if (!pending) return false;
    void* task=allocate(0x28);
    if (!task) return false;
    dragTaskCtor(task,car,nodes[door-2]);
    pending->target=h;pending->car=car;pending->until=updated+8.0;
    pending->phase=preparing?Phase::Preparing:Phase::Hit;
    pending->punchAfter=updated+.35;
    registerReference(car,&pending->car);
    // Prepare the hit pose while the occupant remains seated. Traffic is braked
    // until a fresh grip confirms the pull or the post-hit wait expires.
    // Install synchronously so a menu/recenter can cancel the hit before any
    // queued script command starts an unconfirmed full extraction later.
    setTask(Manager(h.ped),task,3,false);
    pending->task=Read<void*>(Manager(h.ped),3*8)==task?task:nullptr;
    if (!pending->task) { ClearAnimation(*pending);return false; }
    StopCar(car,updated);
    // SetTask has already built the native subtree. Start its owned leaf now,
    // rather than waiting for the next scheduled ped AI pass. Keep its callback
    // and lineup utility, so the native task still owns exit/fall completion.
    void* leaf=simplestTask(task);
    if (leaf&&Read<void*>(leaf,0)==draggedAnimationVtable) {
        if (!Read<void*>(leaf,0x18)) startDraggedAnimation(leaf,h.ped);
    }
    return true;
}
bool PunchAtSeat(const Pool& pool,void* player,Vec eye,Vec p,Vec previous) {
            Hold struck{};float nearest=.30f;
            for (int slot=0;slot<pool.size;++slot) {
                if (pool.flags[slot]&128) continue;
                const int ref=(slot<<8)|pool.flags[slot];void* ped=getPed(ref);
                if (!ped||ped==player||playerlife::Dead(ped)||!Seated(ped)||
                    (Animating(ped)&&!BeforeHit(ped))||Waiting(ped)||isPlayer(ped)) continue;
                const int door=ExitDoor(ped,player);if (door<0) continue;
                const Vec head=Bone(ped,5),travel=p-previous;
                const float length=Dot(travel,travel);
                const Vec closest=previous+travel*(length>1.e-6f?std::clamp(Dot(head-previous,travel)/length,0.f,1.f):0.f);
                const float distance=Length(head-closest);
                if (Finite(head)&&distance<nearest&&SeatReachable(ped,door,eye,p)&&Visible(eye,head,ped,Read<void*>(ped,0x708))) {
                    nearest=distance;struck={ped,pool.identity,pool.objects,nullptr,ref,5,p,{},p,true};
                }
            }
    return struck.ped&&BeginHit(struck);
}
Hold ApproachAtSeat(const Pool& pool,void* player,Vec eye,Vec hand) {
    Hold target{};float nearest=.95f;
    for (int slot=0;slot<pool.size;++slot) {
        if (pool.flags[slot]&128) continue;
        const int ref=(slot<<8)|pool.flags[slot];void* ped=getPed(ref);
        if (!ped||ped==player||isPlayer(ped)||playerlife::Dead(ped)||!Seated(ped)) continue;
        if (!Finite(Position(ped))||Length(Position(ped)-Position(player))>3.f) continue;
        const int door=ExitDoor(ped,player);if (door<0) continue;
        const Vec head=Bone(ped,5);const float distance=Length(head-hand);
        if (Finite(head)&&distance<nearest&&SeatReachable(ped,door,eye,hand)&&
            Visible(eye,head,ped,Read<void*>(ped,0x708))) {
            nearest=distance;target={ped,pool.identity,pool.objects,nullptr,ref,5};
        }
    }
    return target;
}
bool ContinuePull(Animation& pending,void* player) {
    if (pending.phase!=Phase::Waiting) return false;
    const int door=ExitDoor(pending.target.ped,player);
    Vec destination{};
    if (door<0||!FindDoorExit(pending.target.ped,pending.car,door,destination)) return false;
    ResumePose(pending);
    ActionFeedback(pending.target.ped,false);
    pending.phase=Phase::Pull;pending.until=updated+8.0;
    ReportCarjack(pending,player);
    return true;
}
void CancelHit(Animation& a,bool react) {
    ResumePose(a);
    if (void* manager=Manager(a.target.ped)) {
        if (a.task&&Read<void*>(manager,3*8)==a.task) setTask(manager,nullptr,3,false);
    }
    // No carjack happened. Native unarmed aggression chooses leaving to fight
    // or escaping in the car, with the victim still seated at this point.
    if (react) Aggression(a.target,owner);
    // Do not immediately start a new preparation over the native reaction.
    // Walking away or closing the door permits automatic preparation again.
    preparationBlocked[nextPreparationBlocked++%4]=a.target;
    ClearAnimation(a);
}
void PruneAnimations(const Pool& pool,double now) {
    for (auto& stop:stops) if (stop.car&&now>=stop.until) ClearStop(stop);
    for (auto& target:preparationBlocked) if (target.ped&&
        (!target.Live(pool)||!owner||!Seated(target.ped)||
         Length(Position(target.ped)-Position(owner))>3.f||ExitDoor(target.ped,owner)<0)) target={};
    for (auto& a:animations) if (a.target.ped) {
        const bool live=a.target.Live(pool);
        if (!live||!a.car||playerlife::Dead(a.target.ped)||!Seated(a.target.ped)) {
            if (live&&(a.phase==Phase::Waiting||a.phase==Phase::AwaitHit)) ResumePose(a);
            ClearAnimation(a);continue;
        }
        if (a.phase==Phase::Pull) {
            if (now>=a.until) ClearAnimation(a);
            continue;
        }
        void* manager=Manager(a.target.ped);
        if (!manager||Read<void*>(manager,3*8)!=a.task||now>=a.until) { CancelHit(a,false);continue; }
        if (a.phase==Phase::Preparing||a.phase==Phase::AwaitHit) {
            StopCar(a.car,now);
            if (!owner||!Finite(Position(a.target.ped))||Length(Position(a.target.ped)-Position(owner))>3.f||
                ExitDoor(a.target.ped,owner)<0) { CancelHit(a,false);continue; }
            a.until=now+8.0; // stay ready while the player remains at the open door
            if (a.phase==Phase::Preparing) if (void* anim=VictimAnimation(a.target.ped)) {
                const auto* point=AnimationPoint(anim);
                const float time=Read<float>(anim,0x38),pause=point?point->start:0.f;
                if (std::isfinite(time)&&time>=pause) {
                    PausePose(a,anim,pause);a.phase=Phase::AwaitHit;
                    a.punchAfter=std::max(a.punchAfter,now+.2);
                }
            }
        } else if (a.phase==Phase::Hit) {
            if (void* anim=VictimAnimation(a.target.ped)) {
                const float time=Read<float>(anim,0x38);
                const float pause=PauseTime(anim);
                if (std::isfinite(time)&&time>=pause) {
                    PausePose(a,anim,pause);
                    a.phase=Phase::Waiting;a.waitUntil=now+2.0;
                }
            }
        } else if (now>=a.waitUntil) CancelHit(a,true);
    }
    for (const auto& stop:stops) if (stop.car) StopCar(stop.car,now);
    for (const auto& a:animations) if (a.car) StopCar(a.car,now);
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
bool Hook(void* target,const std::uint32_t (&expected)[4],void* replacement,void*& original) {
    // Both supported prologues only save stack/registers.
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
    original=trampoline;
    const auto hook=reinterpret_cast<std::uintptr_t>(replacement);
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
    RESOLVE(simplestTask,"_ZN12CTaskManager15GetSimplestTaskEP5CTask");
    RESOLVE(startDraggedAnimation,"_ZN30CTaskSimpleCarSlowBeDraggedOut9StartAnimEPK4CPed");
    if (void* table=dlsym(handle,"_ZTV30CTaskSimpleCarSlowBeDraggedOut"))
        draggedAnimationVtable=static_cast<char*>(table)+16;
    RESOLVE(dragTaskCtor,"_ZN41CTaskComplexCarSlowBeDraggedOutAndStandUpC1EP8CVehiclei");
    RESOLVE(registerReference,"_ZN7CEntity17RegisterReferenceEPPS_");
    RESOLVE(cleanReference,"_ZN7CEntity19CleanUpOldReferenceEPPS_");
    RESOLVE(doorPosition,"_ZN13CCarEnterExit24GetPositionToOpenCarDoorERK8CVehiclei");
    RESOLVE(groundPosition,"_ZN13CPedPlacement15FindZCoorForPedEP7CVector");
    RESOLVE(worldRemove,"_ZN6CWorld6RemoveEP7CEntity");
    RESOLVE(worldAdd,"_ZN6CWorld3AddEP7CEntity");
    RESOLVE(canOpen,"_ZNK8CVehicle15CanPedOpenLocksEPK4CPed");
    RESOLVE(draggedCtor,"_ZN19CEventDraggedOutCarC1EPK8CVehiclePK4CPedb");
    RESOLVE(draggedDtor,"_ZN19CEventDraggedOutCarD1Ev");
    RESOLVE(association,"_Z30RpAnimBlendClumpGetAssociationP7RpClumpj");
    RESOLVE(setAnimationTime,"_ZN21CAnimBlendAssociation14SetCurrentTimeEf");
    RESOLVE(realPhysics,"_ZN8CCarCtrl26SwitchVehicleToRealPhysicsEP8CVehicle");
    RESOLVE(seatPosition,"_ZN4CPed19SetPedPositionInCarEv");
    auto nameKey=reinterpret_cast<std::uint32_t(*)(const char*)>(dlsym(handle,"_ZN7CKeyGen15GetUppercaseKeyEPKc"));
    if (nameKey) for (auto& point:pausePoints) point.key=nameKey(point.name);
#undef RESOLVE
    ready=pedPool&&getPed&&isPlayer&&allocate&&fallCtor&&standCtor&&setTask&&
        simplestTask&&startDraggedAnimation&&draggedAnimationVtable&&seatPosition&&
        dragTaskCtor&&registerReference&&cleanReference&&doorPosition&&groundPosition&&worldRemove&&worldAdd&&
        canOpen&&g.CPed_GetBonePosition&&g.CWorld_TestSphereAgainstWorld&&g.CWorld_pIgnoreEntity&&
        draggedCtor&&draggedDtor&&association&&setAnimationTime&&realPhysics&&nameKey&&g.CEventGroup_Add&&
        g.CWeapon_GenerateDamageEvent&&g.CPed_GetLocalDirection&&
        g.CWorld_ProcessLineOfSight&&g.CVehicle_GetVehicleAppearance;
    if (ready) {
        constexpr std::uint32_t velocity[]={0x6dbb2beb,0x6d0123e9,0xa9027bfd,0xf9001bf5};
        constexpr std::uint32_t placement[]={0xd10343ff,0xfd002bea,0x6d0623e9,0xa9077bfd};
        void* original{};
        ready=Hook(dlsym(handle,"_ZN4CPed20CalculateNewVelocityEv"),velocity,
            reinterpret_cast<void*>(&OnCalculateVelocity),original);
        calculateVelocity=reinterpret_cast<decltype(calculateVelocity)>(original);
        if (ready) {
            ready=Hook(dlsym(handle,"_ZN4CPed25PositionPedOutOfCollisionEiP8CVehicleb"),placement,
                reinterpret_cast<void*>(&OnPlacement),original);
            originalPlacement=reinterpret_cast<decltype(originalPlacement)>(original);
        }
        if (ready) {
            constexpr std::uint32_t ai[]={0xd10343ff,0x6d072beb,0x6d0823e9,0xa9097bfd};
            ready=Hook(dlsym(handle,"_ZN11CAutomobile9ProcessAIERj"),ai,reinterpret_cast<void*>(&OnVehicleAI),original);
            vehicleAI=reinterpret_cast<decltype(vehicleAI)>(original);
        }
        if (ready) {
            constexpr std::uint32_t control[]={0xd105c3ff,0x6d0d3bef,0x6d0e33ed,0x6d0f2beb};
            ready=Hook(dlsym(handle,"_ZN11CAutomobile14ProcessControlEv"),control,
                reinterpret_cast<void*>(&OnVehicleControl),original);
            vehicleControl=reinterpret_cast<decltype(vehicleControl)>(original);
        }
        if (ready) {
            constexpr std::uint32_t lineup[]={0x6db73bef,0x6d0133ed,0x6d022beb,0x6d0323e9};
            ready=Hook(dlsym(handle,"_ZN28CTaskUtilityLineUpPedWithCar10ProcessPedEP4CPedP8CVehicleP21CAnimBlendAssociation"),lineup,
                reinterpret_cast<void*>(&OnLineUpPed),original);
            lineUpPed=reinterpret_cast<decltype(lineUpPed)>(original);
        }
        if (ready) {
            constexpr std::uint32_t rails[]={0xd10343ff,0x6d082beb,0x6d0923e9,0xa90a7bfd};
            ready=Hook(dlsym(handle,"_ZN8CCarCtrl16UpdateCarOnRailsEP8CVehicle"),rails,
                reinterpret_cast<void*>(&OnVehicleRails),original);
            vehicleRails=reinterpret_cast<decltype(vehicleRails)>(original);
        }
    }
    LOGI("[npcgrab] available=%d",ready);
}
void OnDoorOpening(void* car) {
    if (!ready||!car||g.CVehicle_GetVehicleAppearance(car)!=1) return;
    const Vec speed=Read<Vec>(car,0x68);
    void* driver=Read<void*>(car,0x5a0);
    if (!driver||isPlayer(driver)||!Finite(speed)||Length(speed)>.08f) return;
    TrafficStop* chosen=nullptr;
    for (auto& stop:stops) if (stop.car==car) { chosen=&stop;break; }
    if (!chosen) for (auto& stop:stops) if (!stop.car) { chosen=&stop;break; }
    if (!chosen) return;
    if (!chosen->car) { chosen->car=car;registerReference(car,&chosen->car); }
    chosen->until=Now()+3.0;
    StopCar(car,Now());
}
unsigned ClaimedHands() { return claimed.load(std::memory_order_relaxed); }
bool BlocksVehicleInteraction(void* car,int hand) {
    if (!car||hand<0||hand>1||!ready||Now()-updated>.2||
        (physicalweapon::HeldHandMaskRelaxed()&(1u<<hand))||physicalweapon::IsSupportHand(hand)) return false;
    Pool pool;
    if (!pool.Load()) return false;
    for (const auto& a:animations)
        if (a.car==car&&a.target.Live(pool)) return true;
    const auto& target=approach[hand];
    return target.Live(pool)&&Seated(target.ped)&&Read<void*>(target.ped,0x708)==car;
}
bool BlocksNpcPunch(void* ped,int hand) {
    if (!ped||hand<0||hand>1||!ready||Now()-updated>.2||
        (physicalweapon::HeldHandMaskRelaxed()&(1u<<hand))||physicalweapon::IsSupportHand(hand)) return false;
    Pool pool;
    if (!pool.Load()) return false;
    for (const auto& a:animations) if (a.target.ped==ped&&a.target.Live(pool)) return true;
    return false;
}
void PublishMarkers() {
    MarkerFrame frame{};frame.epoch=worldMarkers.epoch;
    for (int i=0;i<worldMarkers.count;++i) {
        const auto& marker=worldMarkers.markers[i];
        float p[3]{},r[3]{},u[3]{};
        const Vec rp=marker.position+marker.direction,up=marker.position+marker.up;
        const float world[3]={marker.position.x,marker.position.y,marker.position.z};
        const float right[3]={rp.x,rp.y,rp.z},above[3]={up.x,up.y,up.z};
        if (!vrcam::WorldPointToTracking(world,p)||!vrcam::WorldPointToTracking(right,r)||
            !vrcam::WorldPointToTracking(above,u)) continue;
        const Vec position{p[0],p[1],p[2]},direction{r[0]-p[0],r[1]-p[1],r[2]-p[2]},vertical{u[0]-p[0],u[1]-p[1],u[2]-p[2]};
        if (Finite(position)&&Finite(direction)&&Finite(vertical))
            frame.markers[frame.count++]={position,direction,vertical,marker.held,marker.fist,
                marker.opacity,marker.highlight,marker.scale};
    }
    xr::SetNpcGrabMarkers(frame);
}
void Reset() {
    Pool pool;pool.Load();
    for (auto& h:holds) Release(h,pool,false);
    for (auto& edge:edges) edge.latched=true;
    for (auto& active:intent) active=false;
    for (auto& released:extracted) released=false;
    for (auto& motion:handMotion) motion={};
    for (auto& target:approach) target={};
    for (auto& effect:feedback) effect={};
    Pool current;current.Load();
    for (auto& a:animations) if (a.target.Live(current)&&a.phase!=Phase::Pull) CancelHit(a,false);
    for (auto& flight:flights) flight={};
    claimed.store(0,std::memory_order_relaxed);
    worldMarkers={};xr::SetNpcGrabMarkers({});
}
void Update(bool blocked,const xr::HandPose hands[2],bool trackingValid,std::uint64_t epoch) {
    const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const float dt=updated?float(now-updated):0;updated=now;
    void* player=g.FindPlayerPed?g.FindPlayerPed(-1):nullptr;
    if (player!=owner) { Reset();owner=player; }
    void* pad=g.CPad_GetPad?g.CPad_GetPad(0):nullptr;
    Pool pool;pool.Load();
    PruneAnimations(pool,now);
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
    unsigned preparationHands=~occupied&3u;
    for (int hand=0;hand<2;++hand) if (!hands[hand].valid||physicalweapon::IsSupportHand(hand))
        preparationHands&=~(1u<<hand);
    for (auto& a:animations) if (a.target.ped&&BeforeHit(a.target.ped)) {
        const int door=ExitDoor(a.target.ped,player);
        const Vec head=Bone(a.target.ped,5);
        if (!preparationHands||door<0||!Finite(head)||!SeatReachable(a.target.ped,door,eye,head)||
            !Visible(eye,head,a.target.ped,a.car)) CancelHit(a,false);
    }
    for (int hand=0;hand<2;++hand) {
        auto& h=holds[hand];
        float world[3]{};
        const bool valid=hands[hand].valid&&std::isfinite(hands[hand].grip)&&
            !(occupied&(1u<<hand))&&!physicalweapon::IsSupportHand(hand)&&
            vrcam::TrackingPointToWorld(hands[hand].gripPos,world);
        const Vec p{world[0],world[1],world[2]};
        const bool good=valid&&Finite(p)&&Finite(eye)&&Length(p-eye)<1.6f;
        // Reserve the punch corridor before the ordinary melee sweep reaches
        // a door/window collider, not only once the fist reaches the face.
        approach[hand]=good&&hands[hand].grip>=.8f?
            ApproachAtSeat(pool,player,eye,p):Hold{};
        const Vec previous=handMotion[hand].previous;
        const bool wasClenched=handMotion[hand].clenched;
        const bool haveMotion=handMotion[hand].valid&&good&&Length(p-previous)<.6f;
        const float handSpeed=haveMotion?Length(p-previous)/dt:0.f;
        handMotion[hand]={p,good,good&&hands[hand].grip>=.8f};
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
            if (h.seated) { Release(h,pool,false);continue; }
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
        } else if (good&&haveMotion&&wasClenched&&Length(p-previous)>=.01f&&hands[hand].grip>=.8f&&handSpeed>=1.25f&&
                   PunchAtSeat(pool,player,eye,p,previous)) {
            intent[hand]=false;extracted[hand]=true;mask|=1u<<hand;
            xr::TriggerHaptic(hand,.6f,80.f,45.f);
        } else if (intent[hand]&&hands[hand].grip>=.65f&&good) {
            float best=.35f;
            Hold chosen{};
            for (int slot=0;slot<pool.size;++slot) {
                if (pool.flags[slot]&128) continue;
                const int ref=(slot<<8)|pool.flags[slot];
                void* ped=getPed(ref);
                if (!ped||ped==player||ped==holds[1-hand].ped||isPlayer(ped)||playerlife::Dead(ped)||
                    !Finite(Position(ped))||Length(Position(ped)-Position(player))>3.f||Animating(ped)) continue;
                const bool seated=Seated(ped);
                const int door=seated?ExitDoor(ped,player):-1;
                if (seated&&(!Waiting(ped)||door<0||!SeatReachable(ped,door,eye,p))) continue;
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
                if (chosen.seated) {
                    if (ContinuePull(*Waiting(chosen.ped),player)) {
                        extracted[hand]=true;mask|=1u<<hand;
                    }
                } else h=chosen;
                xr::TriggerHaptic(hand,.45f,80.f,35.f);
            }
        }
        if (h.ped) mask|=1u<<hand;
    }
    claimed.store(mask,std::memory_order_relaxed);
    // Offer one real torso grab point per eligible seated occupant. This uses
    // the same door, side, pool, distance and occlusion gates as acquisition.
    worldMarkers={};worldMarkers.epoch=epoch;
    for (auto& effect:feedback) if (effect.active) {
        const float t=std::clamp(float((now-effect.started)/.5),0.f,1.f);
        if (t>=1.f) { effect={};continue; }
        auto marker=effect.marker;marker.held=true;
        marker.opacity=1.f-t*t*(3.f-2.f*t);
        marker.highlight=1.f-.5f*t;marker.scale=1.f+.18f*t;
        worldMarkers.markers[worldMarkers.count++]=marker;
    }
    unsigned free=~(occupied|mask)&3u;
    for (int hand=0;hand<2;++hand) if (!hands[hand].valid||physicalweapon::IsSupportHand(hand)) free&=~(1u<<hand);
    if (free) for (int slot=0;slot<pool.size&&worldMarkers.count<MarkerFrame::kCapacity;++slot) {
        if (pool.flags[slot]&128) continue;
        void* ped=getPed((slot<<8)|pool.flags[slot]);
        if (!ped||ped==player||isPlayer(ped)||playerlife::Dead(ped)||!Seated(ped)||
            !Finite(Position(ped))||Length(Position(ped)-Position(player))>3.f||
            (Animating(ped)&&!BeforeHit(ped))) continue;
        void* manager=Manager(ped);
        if (!manager||Read<void*>(manager,0)) continue;
        const int door=ExitDoor(ped,player);
        const bool waiting=Waiting(ped)!=nullptr;
        const Vec point=Bone(ped,waiting?4:5);
        if (door<0||!Finite(point)||!SeatReachable(ped,door,eye,point)||
            !Visible(eye,point,ped,Read<void*>(ped,0x708))) continue;
        if (!waiting&&!BeforeHit(ped)&&!PreparationBlocked(ped)) {
            Hold target{ped,pool.identity,pool.objects,nullptr,(slot<<8)|pool.flags[slot],5};
            if (!BeginHit(target,true)) continue;
        }
        Vec right=Cross(eye-point,Vec{0,0,1});
        if (Length(right)<.1f) continue;
        right=right*(1.f/Length(right));
        bool near=false;
        for (int hand=0;hand<2;++hand) if ((free&(1u<<hand))&&hands[hand].valid&&
            !physicalweapon::IsSupportHand(hand)) {
            float p[3]{};
            if (vrcam::TrackingPointToWorld(hands[hand].gripPos,p)&&Length(Vec{p[0],p[1],p[2]}-point)<.35f) near=true;
        }
        // Offset toward the viewer so the silhouette rests over the clothing.
        const Vec facing=(eye-point)*(1.f/std::max(.1f,Length(eye-point)));
        worldMarkers.markers[worldMarkers.count++]={point+facing*.18f,right,{0,0,1},near,!waiting};
    }
}
}
