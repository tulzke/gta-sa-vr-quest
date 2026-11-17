#include "ScopeAim.h"

#include "Log.h"
#include "PhotoScreen.h"
#include "PhysicalWeapon.h"
#include "PairedWeapons.h"
#include "Symbols.h"
#include "VrCamera.h"
#include "Xr.h"
#include "WeaponOptic.h"
#include "Calib.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace savr::scopeaim {
namespace {

using GameVec3 = GameSymbols::Vec3;

constexpr int kSniperType = 34;
constexpr int kRpgType = 35;
constexpr int kHeatSeekerType = 36;
constexpr int kCameraType = 43;
struct V3 {
    float x{}, y{}, z{};
};

struct ColPoint {
    alignas(4) std::uint8_t bytes[0x2c]{};
};

V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator*(V3 v, float s) { return {v.x * s, v.y * s, v.z * s}; }

float Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float LengthSquared(V3 v) { return Dot(v, v); }
bool Finite(V3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
V3 Normalized(V3 v) {
    const float lengthSq = LengthSquared(v);
    if (!std::isfinite(lengthSq) || lengthSq < 1.0e-8f) return {};
    return v * (1.0f / std::sqrt(lengthSq));
}

std::uint64_t NowMs() {
    using Clock = std::chrono::steady_clock;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now().time_since_epoch()).count());
}

std::atomic<int> gCameraHand{-1};
std::atomic<bool> gSniperFireAvailable{false};
std::atomic<bool> gProjectileFireAvailable{false};
std::atomic<float> gCameraZoom{photoscreen::MinZoom};
std::atomic<float> gSniperZoom{2.5f}, gHeatZoom{1.8f};
std::atomic<bool> gWeaponZoomInput{false};
std::uint64_t gWeaponZoomTime{};
HeatLockState gHeatLock{}; // GameThread, like the scene producer/launcher hook
void* gHeatPool{};
int gHeatIndex{-1}, gHeatHand{-1};
unsigned char gHeatGeneration{};
std::uint64_t gHeatSince{};
std::uint64_t gCameraZoomTime = 0;
bool gMissingFireHookLogged = false;

void UpdateCameraZoom(std::uint64_t now) {
    const float elapsed = gCameraZoomTime != 0 && now >= gCameraZoomTime
        ? std::min(static_cast<float>(now - gCameraZoomTime) * 0.001f, 0.1f)
        : 0.0f;
    gCameraZoomTime = now;
    xr::InputState input{};
    xr::GetInput(input);
    const float axis = input.rightStick[1];
    if (!std::isfinite(axis) || std::abs(axis) <= 0.25f) return;
    const float rate = std::copysign(
        (std::min(std::abs(axis), 1.0f) - 0.25f) / 0.75f, axis);
    const float zoom = gCameraZoom.load(std::memory_order_relaxed);
    gCameraZoom.store(std::clamp(zoom * std::exp(rate * elapsed * 1.2f),
                               photoscreen::MinZoom, photoscreen::MaxZoom),
                      std::memory_order_relaxed);
}

bool FireRouteAvailable(int type) {
    if (type == kRpgType || type == kHeatSeekerType)
        return gProjectileFireAvailable.load(std::memory_order_acquire);
    return gSniperFireAvailable.load(std::memory_order_acquire);
}

template<class T> T Read(const void* p, std::size_t offset) {
    T value{};
    if(p) std::memcpy(&value, static_cast<const char*>(p)+offset, sizeof(value));
    return value;
}
void ClearHeat() {
    gHeatLock={}; gHeatPool=nullptr; gHeatIndex=-1; gHeatHand=-1; gHeatSince=0;
}
void* ValidHeatEntity() {
    void* pool=g.CPools_ms_pVehiclePool ? *g.CPools_ms_pVehiclePool : nullptr;
    if(!pool || pool!=gHeatPool || gHeatIndex<0 || gHeatIndex>=Read<int>(pool,0x10)) return nullptr;
    const auto* flags=Read<const unsigned char*>(pool,8);
    auto* objects=Read<char*>(pool,0);
    if(!flags || !objects || (flags[gHeatIndex]&0x80) || flags[gHeatIndex]!=gHeatGeneration) return nullptr;
    return objects+gHeatIndex*0xc68;
}
void UpdateHeat(void* ped, int hand, std::uint64_t now) {
    float o[3]{}, d[3]{};
    if(!vrcam::GetWeaponOpticRay(hand,kHeatSeekerType,o,d)) { ClearHeat(); return; }
    void* pool=g.CPools_ms_pVehiclePool ? *g.CPools_ms_pVehiclePool : nullptr;
    const int count=Read<int>(pool,0x10);
    const auto* flags=Read<const unsigned char*>(pool,8);
    auto* objects=Read<char*>(pool,0);
    if(!pool || !flags || !objects || count<=0 || count>20000) { ClearHeat(); return; }
    const V3 origin{o[0],o[1],o[2]}, direction{d[0],d[1],d[2]};
    const float cone=std::min(.10f, weaponoptic::TanHalfFov(
        calib::SnapshotOptic(kHeatSeekerType).value[6]*.0005f,WeaponZoom(kHeatSeekerType))*.65f);
    int best=-1; float bestScore=1e9f; V3 bestPosition{};
    for(int i=0;i<count;++i) {
        if(flags[i]&0x80) continue;
        void* entity=objects+i*0xc68;
        if(!(Read<std::uint32_t>(entity,0x28)&(1u<<7))) continue;
        V3 pos{};
        if(g.CEntity_GetBoundCentre) {
            GameVec3 centre{}; g.CEntity_GetBoundCentre(entity,&centre);
            pos={centre.x,centre.y,centre.z};
        } else {
            const void* matrix=Read<void*>(entity,0x18);
            pos=matrix ? Read<V3>(matrix,0x30) : Read<V3>(entity,8);
        }
        const V3 delta=pos-origin;
        const float depth=Dot(delta,direction), lengthSq=LengthSquared(delta);
        if(!Finite(pos) || depth<3.f || lengthSq>400.f*400.f) continue;
        const float offAxis=std::sqrt(std::max(0.f,lengthSq-depth*depth))/depth;
        if(offAxis>cone) continue;
        ColPoint col{}; void* hitEntity=nullptr;
        void* savedIgnore=*g.CWorld_pIgnoreEntity; *g.CWorld_pIgnoreEntity=ped;
        const GameVec3 from{origin.x,origin.y,origin.z}, to{pos.x,pos.y,pos.z};
        const bool hit=g.CWorld_ProcessLineOfSight(&from,&to,&col,&hitEntity,
            true,true,false,true,true,false,false,false);
        *g.CWorld_pIgnoreEntity=savedIgnore;
        if(hit && hitEntity!=entity) continue;
        const float score=offAxis+(i==gHeatIndex && pool==gHeatPool ? -.025f : 0.f);
        if(score<bestScore) { best=i;bestScore=score;bestPosition=pos; }
    }
    if(best<0) { ClearHeat(); return; }
    if(pool!=gHeatPool || best!=gHeatIndex || flags[best]!=gHeatGeneration || hand!=gHeatHand) {
        gHeatSince=now;gHeatPool=pool;gHeatIndex=best;gHeatGeneration=flags[best];gHeatHand=hand;
    }
    gHeatLock.candidate=true;
    gHeatLock.progress=std::clamp(static_cast<float>(now-gHeatSince)/.8e3f,0.f,1.f);
    gHeatLock.locked=gHeatLock.progress>=1.f;
    gHeatLock.target[0]=bestPosition.x;gHeatLock.target[1]=bestPosition.y;gHeatLock.target[2]=bestPosition.z;
}
void UpdateWeaponOptics(void* ped, std::uint64_t now, bool photoZoom) {
    int selected=-1, heat=-1;
    for(int h=0;h<2;++h) {
        const int slot = physicalweapon::HeldSlot(h);
        if (!pairedweapon::Valid(slot)) continue;
        const int type=pairedweapon::Type(ped,slot);
        if(!weaponoptic::Supported(type) || !FireRouteAvailable(type)) continue;
        xr::HandPose hands[2]{};
        if(!physicalweapon::GetHandPosesSnapshot(hands) || !hands[h].valid || !hands[h].aimValid) continue;
        if(selected<0 || h==physicalweapon::PreferredHeldHand()) selected=h;
        if(type==kHeatSeekerType) heat=h;
    }
    gWeaponZoomInput.store(selected>=0,std::memory_order_relaxed);
    const float seconds=gWeaponZoomTime && now>=gWeaponZoomTime ? (now-gWeaponZoomTime)*.001f : 0.f;
    gWeaponZoomTime=now;
    if(selected>=0 && !photoZoom) {
        xr::InputState input{};xr::GetInput(input);
        const int type=pairedweapon::Type(ped,physicalweapon::HeldSlot(selected));
        auto& zoom=type==kSniperType ? gSniperZoom : gHeatZoom;
        zoom.store(weaponoptic::StepZoom(type,zoom.load(),input.rightStick[1],seconds));
    }
    if (heat >= 0 && g.CWorld_ProcessLineOfSight && g.CWorld_pIgnoreEntity)
        UpdateHeat(ped, heat, now);
    else
        ClearHeat();
}

} // namespace

void Reset() {
    gCameraHand.store(-1, std::memory_order_relaxed);
    gCameraZoomTime = 0;
    gWeaponZoomTime=0;gWeaponZoomInput.store(false,std::memory_order_relaxed);ClearHeat();
}

void SetSniperFireAvailable(bool available) {
    gSniperFireAvailable.store(available, std::memory_order_release);
    if (available) gMissingFireHookLogged = false;
    if (!available) Reset();
}

void SetProjectileFireAvailable(bool available) {
    gProjectileFireAvailable.store(available, std::memory_order_release);
    if (available) gMissingFireHookLogged = false;
    if (!available) Reset();
}

void Update(bool interactionsBlocked) {
    void* const ped = g.FindPlayerPed ? g.FindPlayerPed(-1) : nullptr;
    if (interactionsBlocked || !vrcam::IsStereoActive() || !ped ||
        !g.CPed_IsAlive || !g.CPed_IsAlive(ped) || !g.FindPlayerVehicle ||
        g.FindPlayerVehicle(-1, false)) {
        Reset();
        return;
    }

    const std::uint64_t now = NowMs();
    int cameraHand = -1;
    xr::HandPose hands[2]{};
    if (physicalweapon::GetHandPosesSnapshot(hands)) {
        for (int hand = 0; hand < 2; ++hand) {
            const int slot = physicalweapon::HeldSlot(hand);
            if (pairedweapon::Valid(slot) && pairedweapon::Type(ped, slot) == kCameraType &&
                hands[hand].valid && hands[hand].aimValid) {
                cameraHand = hand;
                break;
            }
        }
    }
    gCameraHand.store(cameraHand, std::memory_order_relaxed);
    if (cameraHand >= 0) UpdateCameraZoom(now);
    else gCameraZoomTime = 0;

    if (gSniperFireAvailable.load(std::memory_order_acquire) ||
        gProjectileFireAvailable.load(std::memory_order_acquire)) {
        UpdateWeaponOptics(ped, now, cameraHand >= 0);
    } else if (!gMissingFireHookLogged) {
        gMissingFireHookLogged = true;
        LOGW("[scope] no verified physical weapon-optic routes");
    }
}

int PhotoHand() { return gCameraHand.load(std::memory_order_relaxed); }

float WeaponZoom(int type) {
    return type==kSniperType ? gSniperZoom.load(std::memory_order_relaxed)
        : type==kHeatSeekerType ? gHeatZoom.load(std::memory_order_relaxed) : 1.f;
}
bool ConsumesZoomInput() { return gWeaponZoomInput.load(std::memory_order_relaxed) || PhotoHand() >= 0; }
HeatLockState HeatLockSnapshot() { return gHeatLock; }
void* HeatTarget(int hand) { return gHeatLock.locked && hand==gHeatHand ? ValidHeatEntity() : nullptr; }

float PhotoFovX(float baseFovXDegrees) {
    constexpr float kPi = 3.14159265358979323846f;
    const float halfRadians = baseFovXDegrees * kPi / 360.0f;
    const float zoom = gCameraZoom.load(std::memory_order_relaxed);
    return 2.0f * std::atan(std::tan(halfRadians) / zoom) * 180.0f / kPi;
}

bool ApplyReticleAim(int hand, int weaponType,
                     const float muzzleOrigin[3], float direction[3]) {
    if(weaponoptic::Supported(weaponType)) {
        float o[3]{}, d[3]{};
        if(!muzzleOrigin || !direction || !vrcam::GetWeaponOpticRay(hand,weaponType,o,d)) return false;
        const V3 target=V3{o[0],o[1],o[2]}+V3{d[0],d[1],d[2]}*weaponoptic::ZeroDistance;
        const V3 aim=Normalized(target-V3{muzzleOrigin[0],muzzleOrigin[1],muzzleOrigin[2]});
        if(!Finite(aim) || LengthSquared(aim)<.9f) return false;
        direction[0]=aim.x;direction[1]=aim.y;direction[2]=aim.z;
        return true;
    }
    return true;
}

} // namespace savr::scopeaim
