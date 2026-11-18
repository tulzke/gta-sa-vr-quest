// Retail world pickups keep their object lifecycle, sounds and grant rules.
// Only the contact predicate is gated; new weapon types require a deliberate
// one-second hand gesture. Non-weapon pickups keep their previous behaviour.
#include "Pickups.h"
#include "PickupGesture.h"
#include "GrabArbitration.h"
#include "PhysicalWeapon.h"
#include "VehicleDoors.h"
#include "VehicleDoorMotion.h"
#include "NpcGrab.h"
#include "Basketball.h"
#include "Driving.h"
#include "Symbols.h"
#include "VrCamera.h"
#include "Xr.h"
#include "Log.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

extern "C" {
__attribute__((visibility("hidden"))) void* g_savrPickupGateContinue=nullptr;
void SavrPickupContactGate();
}
namespace savr::pickups {
namespace {
constexpr int kObjOff=4,kPosXOff=0x14,kPosYOff=0x16,kPosZOff=0x18;
constexpr int kEntityMatrix=0x18,kMatrixPos=0x30,kPoolCount=620,kStride=0x24;
constexpr float kRiseZoneM=3.5f,kRiseHeightM=.80f,kNonWeaponBobM=.06f;
using UpdateFn=bool(*)(void*,void*,void*,int);
UpdateFn g_origUpdate{};
const std::uint32_t* g_timeMs{};
std::int32_t* g_collectBuffer{};
int (*g_weaponForModel)(int){};
void (*g_updateRwMatrix)(void*){};
void (*g_updateRwFrame)(void*){};
std::uint8_t* g_pool{};
thread_local void* g_authorizedPickup{};
Gesture g_gestures[2];
vehicledoors::MarkerFrame g_markers;
struct Item {void* pickup{};void* object{};std::uint64_t id{};int type{},slot{-1};float point[3]{};};
Item g_targets[2];
struct Pending {void* player{};int hand{-1},type{},slot{-1};std::uint64_t epoch{};double until{};} g_pending;

double Now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
template<class T>T Read(const void* p,int off){T v{};if(p)std::memcpy(&v,static_cast<const std::uint8_t*>(p)+off,sizeof(v));return v;}
template<class T>void Write(void* p,int off,T v){std::memcpy(static_cast<std::uint8_t*>(p)+off,&v,sizeof(v));}
void* MatrixOf(const void* entity){return Read<void*>(entity,kEntityMatrix);}
// CPlaceable may retain only CSimpleTransform: position at +8, heading at +14.
// Never require allocation of a full CMatrix merely to offer a pickup.
bool EntityPose(const void* entity,std::array<float,16>& pose){
    if(!entity)return false;
    if(const void* matrix=MatrixOf(entity))std::memcpy(pose.data(),matrix,sizeof(pose));
    else {
        const float heading=Read<float>(entity,0x14);
        if(!std::isfinite(heading))return false;
        pose={};const float c=std::cos(heading),s=std::sin(heading);
        pose[0]=c;pose[1]=s;pose[4]=-s;pose[5]=c;pose[10]=1;
        for(int i=0;i<3;++i)pose[12+i]=Read<float>(entity,8+i*4);
    }
    for(int base:{0,4,8,12})for(int i=0;i<3;++i)if(!std::isfinite(pose[base+i]))return false;
    return true;
}
void SetObjectPosition(void* entity,const float position[3]){
    void* matrix=MatrixOf(entity);
    for(int i=0;i<3;++i)Write(matrix?matrix:entity,(matrix?kMatrixPos:8)+i*4,position[i]);
    // Retail synced the frame before our lift. Publish again so visible mesh,
    // grab point and icon refer to the same position, including simple objects.
    if(g_updateRwMatrix)g_updateRwMatrix(entity);
    if(g_updateRwFrame)g_updateRwFrame(entity);
}
int SlotForWeaponType(int type){
    if(type==1)return 0;
    if(type>=2&&type<=9)return 1;
    if(type>=10&&type<=15)return 10;
    if(type==16||type==17||type==18||type==39)return 8;
    if(type>=22&&type<=24)return 2;
    if(type>=25&&type<=27)return 3;
    if(type==28||type==29||type==32)return 4;
    if(type==30||type==31)return 5;
    if(type==33||type==34)return 6;
    if(type>=35&&type<=38)return 7;
    if(type==40)return 12;
    if(type>=41&&type<=43)return 9;
    if(type>=44&&type<=46)return 11;
    return -1;
}
int WeaponType(const void* pickup){
    const int type=g_weaponForModel?g_weaponForModel(Read<std::int16_t>(pickup,0x1c)):0;
    return SlotForWeaponType(type)>=0?type:0;
}
int OwnedType(const void* player,int slot){return slot>=0&&slot<13?Read<int>(player,0x730+slot*0x20):0;}
float ReadPos16(const void* p,int off){return Read<std::int16_t>(p,off)*.125f;}
float Lift(void* pickup,void* player){
    std::array<float,16> pose{};if(!EntityPose(player,pose))return 0;
    float sum=0;for(int i=0;i<3;++i){const float d=pose[12+i]-ReadPos16(pickup,kPosXOff+i*2);sum+=d*d;}
    const float distance=std::sqrt(sum);if(!std::isfinite(distance)||distance>kRiseZoneM)return 0;
    const float bob=!WeaponType(pickup)&&g_timeMs?std::sin(float(*g_timeMs)*.003f)*kNonWeaponBobM:0;
    return (1.f-distance/kRiseZoneM)*(kRiseHeightM+bob);
}
std::uint64_t Identity(void* pickup){
    if(!g_pool)return 0;
    const auto delta=reinterpret_cast<std::uintptr_t>(pickup)-reinterpret_cast<std::uintptr_t>(g_pool);
    if(delta>=kPoolCount*kStride||delta%kStride)return 0;
    return (delta/kStride+1)|(std::uint64_t(Read<std::uint16_t>(pickup,0x1e))<<16)|
        (std::uint64_t(Read<std::uint16_t>(pickup,0x1c))<<32)|
        (std::uint64_t(Read<std::uint8_t>(pickup,0x20))<<48);
}
bool Live(const Item& item){return item.id&&Identity(item.pickup)==item.id&&
    Read<void*>(item.pickup,kObjOff)==item.object&&!(Read<std::uint8_t>(item.pickup,0x21)&1)&&
    Read<std::uint8_t>(item.pickup,0x20)!=0;}
bool LoadItem(void* pickup,void* player,Item& item){
    if(!Read<std::uint8_t>(pickup,0x20)||(Read<std::uint8_t>(pickup,0x21)&1))return false;
    const int type=WeaponType(pickup),slot=SlotForWeaponType(type);
    void* obj=Read<void*>(pickup,kObjOff);
    if(!type||!obj||physicalweapon::IsSlotDropped(slot))return false;
    // Identical inventory weapons are ammunition sources, handled by walking.
    if(CanWalkCollect(type,OwnedType(player,slot)))return false;
    // Anchor contact and the marker at the visible object's rotation pivot.
    // A model-local grip offset orbits this axis on asymmetric weapons and
    // can cancel a stationary hand's hold as the retail model spins.
    std::array<float,16> pose{};if(!EntityPose(obj,pose))return false;
    item={pickup,obj,Identity(pickup),type,slot,{}};
    std::copy_n(pose.data()+12,3,item.point);
    return item.id!=0;
}
unsigned OtherOwners(){return vehicledoors::ClaimedHands()|npcgrab::ClaimedHands()|
    basketball::HeldHandMask()|physicalweapon::RadialMenuHandMask()|driving::ServiceRadioClaimedHands()|driving::WaterMonitorClaimedHands();}

bool OnPickupUpdate(void* self,void* player,void* vehicle,int playerId){
    const int type=self?WeaponType(self):0;
    // Retail may auto-select a weapon even when only its ammunition was needed.
    // Preserve both the active slot and the player's delayed desired slot.
    const bool ammoOnly=type&&CanWalkCollect(type,OwnedType(player,SlotForWeaponType(type)))&&g_authorizedPickup!=self;
    const auto active=Read<std::uint8_t>(player,0x8dc);
    void* data=Read<void*>(player,0x540);const auto desired=Read<std::uint8_t>(data,0x2c);
    const int collect=g_collectBuffer?*g_collectBuffer:0;
    const bool result=g_origUpdate?g_origUpdate(self,player,vehicle,playerId):false;
    if(type&&g_collectBuffer)*g_collectBuffer=collect;
    if(ammoOnly&&player){Write(player,0x8dc,active);if(data)Write(data,0x2c,desired);}
    if(!self||!player)return result;
    void* obj=Read<void*>(self,kObjOff);std::array<float,16> pose{};
    if(!EntityPose(obj,pose))return result;
    pose[14]+=Lift(self,player);SetObjectPosition(obj,pose.data()+12);
    if(type)return result; // Weapon grabs are resolved once per input frame.
    // Preserve the existing hand path for non-weapon pickups (e.g. property).
    xr::HandPose hands[2]{};
    if(!physicalweapon::GetHandPosesSnapshot(hands)||!g_origUpdate)return result;
    int hand=-1;
    for(int h=0;h<2&&hand<0;++h){
        if(!hands[h].valid||hands[h].grip<.60f||(vehicledoors::ClaimedHands()&(1u<<h)))continue;
        float p[3]{};if(!vrcam::TrackingPointToWorld(hands[h].gripPos,p))continue;
        float sum=0;for(int i=0;i<3;++i){float d=p[i]-pose[12+i];sum+=d*d;}
        if(std::isfinite(sum)&&sum<=.32f*.32f)hand=h;
    }
    std::array<float,16> playerPose{};if(hand<0||!EntityPose(player,playerPose))return result;
    std::int16_t saved[3]{};std::memcpy(saved,static_cast<std::uint8_t*>(self)+kPosXOff,sizeof(saved));
    for(int i=0;i<3;++i)Write(self,kPosXOff+i*2,static_cast<std::int16_t>(playerPose[12+i]*8));
    if(g_collectBuffer)*g_collectBuffer=6;
    const bool collected=g_origUpdate(self,player,vehicle,playerId);
    std::memcpy(static_cast<std::uint8_t*>(self)+kPosXOff,saved,sizeof(saved));
    return collected;
}

bool InstallContactGate(void* update){
    auto* code=reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(update)+0x65c);
    constexpr std::uint32_t expected[]={0xf9400288u,0xb941a508u,0x7207011fu,0x1a9f17e8u};
    if(std::memcmp(code,expected,sizeof(expected))){LOGE("[pickups] contact gate ABI mismatch");return false;}
    const long page=sysconf(_SC_PAGESIZE);if(page<=0)return false;
    const auto start=reinterpret_cast<std::uintptr_t>(code)&~(std::uintptr_t(page)-1);
    const auto end=(reinterpret_cast<std::uintptr_t>(code)+16+page-1)&~(std::uintptr_t(page)-1);
    if(mprotect(reinterpret_cast<void*>(start),end-start,PROT_READ|PROT_WRITE|PROT_EXEC))return false;
    g_savrPickupGateContinue=reinterpret_cast<std::uint8_t*>(code)+16;
    void* target=reinterpret_cast<void*>(&SavrPickupContactGate);
    std::memcpy(code+2,&target,sizeof(target));code[1]=0xD61F0220u;
    __atomic_store_n(code,0x58000051u,__ATOMIC_RELEASE);
    __builtin___clear_cache(reinterpret_cast<char*>(code),reinterpret_cast<char*>(code)+16);
    return true;
}

bool InstallUpdateHook(void* target) {
    if (g_origUpdate) return true;
    // CPickup::Update prologue on 2.11.311 arm64 (must match before we patch).
    constexpr std::uint32_t kExpected[4] = {
        0xd10283ffu, 0xfd0013eau, 0x6d0323e9u, 0xa9047bfdu};
    auto* code = reinterpret_cast<std::uint32_t*>(target);
    std::uint32_t observed[4]{};
    std::memcpy(observed, code, sizeof(observed));
    if (std::memcmp(observed, kExpected, sizeof(observed)) != 0) {
        LOGE("[pickups] CPickup::Update prologue mismatch: %08x %08x %08x %08x",
             observed[0], observed[1], observed[2], observed[3]);
        return false;
    }

    const long pageLong = sysconf(_SC_PAGESIZE);
    if (pageLong <= 0) return false;
    const std::size_t pageSize = static_cast<std::size_t>(pageLong);
    void* tramp = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (tramp == MAP_FAILED) return false;
    auto* t = reinterpret_cast<std::uint32_t*>(tramp);
    std::memcpy(t, code, 16);
    t[4] = 0x58000051u;   // LDR X17, resume literal
    t[5] = 0xD61F0220u;   // BR X17
    *reinterpret_cast<void**>(t + 6) = reinterpret_cast<void*>(
        reinterpret_cast<std::uintptr_t>(target) + 16);
    __builtin___clear_cache(reinterpret_cast<char*>(t),
                            reinterpret_cast<char*>(t) + 32);

    const std::uintptr_t start =
        reinterpret_cast<std::uintptr_t>(code) & ~(pageSize - 1);
    const std::uintptr_t end =
        (reinterpret_cast<std::uintptr_t>(code) + 16 + pageSize - 1) &
        ~(pageSize - 1);
    if (mprotect(reinterpret_cast<void*>(start), end - start,
                 PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        munmap(tramp, pageSize);
        return false;
    }
    // Trampoline first, then the literal, then arm the branch: a call landing
    // mid-patch must never reach the hook with a null original, nor branch
    // through a literal slot that still holds the original instructions.
    g_origUpdate = reinterpret_cast<UpdateFn>(tramp);
    *reinterpret_cast<void**>(code + 2) = reinterpret_cast<void*>(&OnPickupUpdate);
    code[1] = 0xD61F0220u;   // BR X17
    __atomic_store_n(&code[0], 0x58000051u, __ATOMIC_RELEASE);   // LDR X17, #8
    __builtin___clear_cache(reinterpret_cast<char*>(code),
                            reinterpret_cast<char*>(code) + 16);
    LOGI("[pickups] CPickup::Update rise hook installed");
    return true;
}


} // namespace

// Called from the ARM64 bridge after retail computes spatial contact, before
// any extraction, grant, purchase or regeneration changes. Newly created
// objects pass through this same gate on their very first update.
extern "C" __attribute__((visibility("hidden"))) bool SavrPickupContactAllowed(void* self,void* player){
    if(!g_origUpdate)return true; // Installer failure leaves retail collection usable.
    const int type=WeaponType(self);
    if(!type)return true;
    if(!player||Read<int>(player,0x8d4)!=55)return false; // retail delayed-grant queue occupied
    return self==g_authorizedPickup||CanWalkCollect(type,OwnedType(player,SlotForWeaponType(type)));
}

void Install(void* handle){
    if(!handle||g_origUpdate)return;
    void* target=dlsym(handle,"_ZN7CPickup6UpdateEP10CPlayerPedP8CVehiclei");
    g_timeMs=static_cast<const std::uint32_t*>(dlsym(handle,"_ZN6CTimer22m_snTimeInMillisecondsE"));
    g_collectBuffer=static_cast<std::int32_t*>(dlsym(handle,"CollectPickupBuffer"));
    g_weaponForModel=reinterpret_cast<int(*)(int)>(dlsym(handle,"_ZN8CPickups14WeaponForModelEi"));
    g_updateRwMatrix=reinterpret_cast<void(*)(void*)>(dlsym(handle,"_ZN7CEntity14UpdateRwMatrixEv"));
    g_updateRwFrame=reinterpret_cast<void(*)(void*)>(dlsym(handle,"_ZN7CEntity13UpdateRwFrameEv"));
    g_pool=static_cast<std::uint8_t*>(dlsym(handle,"_ZN8CPickups8aPickUpsE"));
    if(!target||!g_pool||!g_weaponForModel||!g_updateRwMatrix||!g_updateRwFrame||!g_collectBuffer||!g_timeMs){
        LOGE("[pickups] missing imports: update=%d pool=%d weapon=%d rwMatrix=%d rwFrame=%d collect=%d timer=%d",
            !target,!g_pool,!g_weaponForModel,!g_updateRwMatrix,!g_updateRwFrame,!g_collectBuffer,!g_timeMs);
        return;
    }
    if(!InstallContactGate(target)||!InstallUpdateHook(target)){
        LOGE("[pickups] verified hook installation failed; manual weapon pickup disabled");return;
    }
    // One startup message remains visible in player builds so a disabled
    // installer can be distinguished from a failed gameplay gesture.
    __android_log_print(ANDROID_LOG_INFO,SAVR_TAG,
        "[pickups] ready: rise/contact hooks installed, 1 second hold");
}

void ProbeGrabs(const xr::HandPose hands[2]){
    g_targets[0]={};g_targets[1]={};
    void* player=g.FindPlayerPed?g.FindPlayerPed(-1):nullptr;
    if(!g_origUpdate||!g_pool||!player||g_pending.hand>=0||physicalweapon::HeldHandMaskRelaxed()||
       Read<int>(player,0x8d4)!=55||!grabarbitration::current.enabled)return;
    const unsigned occupied=OtherOwners();
    float world[2][3]{};bool valid[2]{};float best[2]={kGrabRangeM,kGrabRangeM};
    for(int h=0;h<2;++h)valid[h]=hands[h].valid&&!(occupied&(1u<<h))&&
        vrcam::TrackingPointToWorld(hands[h].gripPos,world[h]);
    for(int i=0;i<kPoolCount;++i){
        Item item{};if(!LoadItem(g_pool+i*kStride,player,item))continue;
        for(int h=0;h<2;++h)if(valid[h]){
            float sum=0;for(int j=0;j<3;++j){float d=world[h][j]-item.point[j];sum+=d*d;}
            const float distance=std::sqrt(sum);
            if(distance<best[h]||(distance==best[h]&&item.id<g_targets[h].id)){
                best[h]=distance;g_targets[h]=item;
            }
        }
    }
    for(int h=0;h<2;++h)if(g_targets[h].id)
        grabarbitration::current.Offer(h,grabarbitration::Kind::Pickup,g_targets[h].id,best[h]);
}

void FinishProbes(){
    auto& selection=grabarbitration::current;
    if(!g_targets[0].id||g_targets[0].id!=g_targets[1].id||
       !selection.Allows(0,grabarbitration::Kind::Pickup)||!selection.Allows(1,grabarbitration::Kind::Pickup))return;
    // Decide after holster/door/etc. eligibility. An unavailable closer hand
    // must not hide an otherwise reachable weapon from the other hand.
    const bool holding0=g_gestures[0].active&&g_gestures[0].target==g_targets[0].id;
    const bool holding1=g_gestures[1].active&&g_gestures[1].target==g_targets[1].id;
    const int loser=holding0?1:holding1?0:selection.hands[0].distance<=selection.hands[1].distance?1:0;
    g_targets[loser]={};selection.Reject(loser,grabarbitration::Kind::Pickup);
}

unsigned ClaimedHands(){return g_pending.hand>=0?1u<<g_pending.hand:0;}
void Reset(){
    g_pending={};for(auto& gesture:g_gestures)gesture=Gesture{};
    for(int h=0;h<2;++h)xr::PublishPickupHaptic(h,0,0);
    g_targets[0]={};g_targets[1]={};g_markers={};
}
void Append(vehicledoors::MarkerFrame& frame){
    if(frame.count==0)frame.epoch=g_markers.epoch;
    if(frame.epoch==g_markers.epoch)
    for(int i=0;i<g_markers.count&&frame.count<frame.kCapacity;++i)frame.markers[frame.count++]=g_markers.markers[i];}

void Tick(){
    const double now=Now();float head[3]{},orientation[4]{};xr::HandPose hands[2]{};std::uint64_t epoch{};
    void* player=g.FindPlayerPed?g.FindPlayerPed(-1):nullptr;
    const bool usable=player&&physicalweapon::GetInteractionTrackingSnapshot(head,orientation,hands,&epoch)&&
        xr::IsSessionFocused()&&grabarbitration::current.enabled&&
        !(g.FindPlayerVehicle&&g.FindPlayerVehicle(-1,false));
    g_markers={};g_markers.epoch=epoch;
    const unsigned occupied=OtherOwners();
    const bool empty=physicalweapon::HeldHandMaskRelaxed()==0;
    if(g_pending.hand>=0){
        const int h=g_pending.hand;
        if(!usable||!empty||g_pending.player!=player||g_pending.epoch!=epoch||now>g_pending.until||
           !hands[h].valid||hands[h].grip<=kGripOpen||(occupied&(1u<<h)))g_pending={};
        else if(OwnedType(player,g_pending.slot)==g_pending.type&&Read<int>(player,0x8d4)==55&&
                physicalweapon::HoldCollectedWeapon(h,g_pending.slot,g_pending.type)){
            xr::TriggerHaptic(h,.4f,0,35);g_pending={};
        }
    }
    for(int h=0;h<2;++h){
        Item item=g_targets[h];
        const bool available=usable&&empty&&g_pending.hand<0&&hands[h].valid&&!(occupied&(1u<<h))&&
            grabarbitration::Allows(h,grabarbitration::Kind::Pickup)&&Live(item);
        const bool completed=g_gestures[h].Step(available?item.id:0,hands[h].grip,
            usable&&empty&&!(occupied&(1u<<h)),epoch,now);
        xr::PublishPickupHaptic(h,available?g_gestures[h].HapticAmplitude():0,epoch);
        if(available){
            using namespace vehicledoors;
            Marker marker{{item.point[0],item.point[1],item.point[2]},{1,0,0},{0,0,1}};
            marker.doorGrab=true;marker.hideArrow=true;marker.pickupHold=true;
            marker.progress=g_gestures[h].progress;marker.held=g_gestures[h].active;
            marker.highlight=marker.held?1.f:0.f;
            g_markers.markers[g_markers.count++]=marker;
        }
        if(!completed||!available||!g_origUpdate||Read<int>(player,0x8d4)!=55)continue;
        Item live{};if(!LoadItem(item.pickup,player,live)||live.id!=item.id||live.object!=item.object)continue;
        float world[3]{};if(!vrcam::TrackingPointToWorld(hands[h].gripPos,world))continue;
        float sum=0;for(int j=0;j<3;++j){float d=world[j]-live.point[j];sum+=d*d;}
        if(!std::isfinite(sum)||sum>kGrabRangeM*kGrabRangeM)continue;
        std::array<float,16> playerPose{},visiblePose{};
        if(!EntityPose(player,playerPose)||!EntityPose(item.object,visiblePose))continue;
        std::int16_t saved[3]{};std::memcpy(saved,static_cast<std::uint8_t*>(item.pickup)+kPosXOff,sizeof(saved));
        const int buffer=*g_collectBuffer;
        for(int j=0;j<3;++j)Write(item.pickup,kPosXOff+j*2,static_cast<std::int16_t>(playerPose[12+j]*8));
        *g_collectBuffer=6;g_authorizedPickup=item.pickup;
        const bool collected=g_origUpdate(item.pickup,player,nullptr,0);
        g_authorizedPickup=nullptr;*g_collectBuffer=buffer;
        std::memcpy(static_cast<std::uint8_t*>(item.pickup)+kPosXOff,saved,sizeof(saved));
        // Restore the visible position too if retail declined to consume it.
        if(Live(item))SetObjectPosition(item.object,visiblePose.data()+12);
        if(collected&&item.slot>0&&(Read<int>(player,0x8d4)==item.type||OwnedType(player,item.slot)==item.type)){
            g_pending={player,h,item.type,item.slot,epoch,now+1.5};
            for(auto& gesture:g_gestures){gesture.Cancel();gesture.latched=true;}
            for(int hand=0;hand<2;++hand)xr::PublishPickupHaptic(hand,0,epoch);
            g_markers={};g_markers.epoch=epoch;
        }
        LOGI("[pickups] deliberate grab hand=%d type=%d collected=%d",h,item.type,collected);
    }
}
} // namespace savr::pickups
