#include "Combat.h"
#include "CombatPolicy.h"
#include "Melee.h"
#include "PhysicalWeapon.h"
#include "PlayerLife.h"
#include "Symbols.h"
#include "VrCamera.h"
#include "Xr.h"
#include "Log.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

namespace savr::combat {
namespace {
struct Vec { float x{},y{},z{}; };
Vec operator-(Vec a,Vec b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
float Dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
float Length(Vec a) { return std::sqrt(Dot(a,a)); }
Vec Horizontal(Vec a) {
    const float n=std::hypot(a.x,a.y);
    return n>.001f?Vec{a.x/n,a.y/n,0}:Vec{};
}
template<class T> T Read(const void* p,int offset) {
    T value{};std::memcpy(&value,static_cast<const char*>(p)+offset,sizeof(value));return value;
}
double Now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
struct Guard {
    bool active{}, weapon{}, sampled{};
    double candidateSince{}, strongUntil{};
    Vec previous{}, direction{};
};
Guard guards[2]{};
void* owner{};
double updated{};
std::uint64_t trackingEpoch{};
Vec headWorld{},forwardWorld{};
float previousHealth{},previousArmour{};
bool vitalsValid{},ready{};
std::atomic<double> damageAt{},blockAt{};
void (*computeDamage)(void*,void*,void*,bool){};

Vec Position(void* entity) {
    void* matrix=Read<void*>(entity,0x18);
    return matrix?Read<Vec>(matrix,0x30):Read<Vec>(entity,8);
}
void ClearGuards() { for (auto& guard:guards) guard={}; }

// Retail 2.11.311 constructor stores creator at +0, damage +8, piece +c,
// weapon +10. ComputeDamageResponse initialises its 12-byte response before
// subtracting armour (+6b4) and health (+6ac). Only melee against CJ is filtered.
void OnComputeDamage(void* calculator,void* victim,void* response,bool speak) {
    const double now=Now();
    void* player=g.FindPlayerPed?g.FindPlayerPed(-1):nullptr;
    const int weapon=Read<int>(calculator,0x10);
    const float damage=Read<float>(calculator,8);
    void* source=Read<void*>(calculator,0);
    float head[3]{},orientation[4]{};std::uint64_t epoch{};
    int chosen=-1;
    float best=.55f;
    if (ready && victim && victim==player && victim==owner &&
        now-updated>=0 && now-updated<.15 && !playerlife::Dead(victim) &&
        xr::IsSessionFocused() && xr::GetHeadPose(head,orientation,&epoch) && epoch==trackingEpoch &&
        !Read<unsigned char>(response,0xa) && weapon>=0 && weapon<=15 &&
        std::isfinite(damage) && damage>0 && source && source!=victim &&
        (Read<unsigned char>(source,0x5a)&7)==3) {
        const Vec incoming=Horizontal(Position(source)-headWorld);
        for (int h=0;h<2;++h) {
            const float alignment=Dot(incoming,guards[h].direction);
            if (guards[h].active && alignment>=best &&
                combatpolicy::Covers(Dot(incoming,forwardWorld),alignment,Read<int>(calculator,0xc))) {
                best=alignment;chosen=h;
            }
        }
    }
    if (chosen<0) { computeDamage(calculator,victim,response,speak);return; }

    const float fraction=combatpolicy::DamageFraction(weapon,guards[chosen].weapon);
    // Do not restore health after native damage: intercept before armour/death
    // bookkeeping. A fully blocked response is calculated with zero damage.
    if (fraction==0) {
        std::memset(response,0,12);
        const std::uint32_t flags=0x01010000;
        std::memcpy(static_cast<char*>(response)+8,&flags,4);
    } else {
        alignas(8) unsigned char reduced[0x18]{};
        std::memcpy(reduced,calculator,sizeof(reduced));
        const float amount=damage*fraction;
        std::memcpy(reduced+8,&amount,sizeof(amount));
        computeDamage(reduced,victim,response,speak);
    }
    blockAt.store(now,std::memory_order_relaxed);
    guards[chosen].strongUntil=now+.22;
    xr::TriggerHaptic(chosen,.9f,0,180);
}

bool Hook(void* target) {
    // Relocatable stack/register saves only; reject other game revisions.
    constexpr std::uint32_t expected[]={0xd10283ff,0xa9067bfd,0xf9003bf7,0xa90857f6};
    if (!target || std::memcmp(target,expected,sizeof(expected))) return false;
    const long page=sysconf(_SC_PAGESIZE);
    if (page<=0) return false;
    auto* trampoline=static_cast<std::uint32_t*>(mmap(nullptr,page,
        PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    if (trampoline==MAP_FAILED) return false;
    std::memcpy(trampoline,target,16);trampoline[4]=0x58000051;trampoline[5]=0xd61f0220;
    const auto resume=reinterpret_cast<std::uintptr_t>(target)+16;
    std::memcpy(trampoline+6,&resume,8);
    __builtin___clear_cache(reinterpret_cast<char*>(trampoline),reinterpret_cast<char*>(trampoline)+32);
    const auto address=reinterpret_cast<std::uintptr_t>(target);
    const auto start=address&~(static_cast<std::uintptr_t>(page)-1);
    const auto end=(address+16+page-1)&~(static_cast<std::uintptr_t>(page)-1);
    if (mprotect(reinterpret_cast<void*>(start),end-start,PROT_READ|PROT_WRITE|PROT_EXEC)) {
        munmap(trampoline,page);return false;
    }
    computeDamage=reinterpret_cast<decltype(computeDamage)>(trampoline);
    auto* code=static_cast<std::uint32_t*>(target);
    const auto replacement=reinterpret_cast<std::uintptr_t>(&OnComputeDamage);
    std::memcpy(code+2,&replacement,8);code[1]=0xd61f0220;
    __atomic_store_n(code,0x58000051,__ATOMIC_RELEASE);
    __builtin___clear_cache(reinterpret_cast<char*>(code),reinterpret_cast<char*>(code)+16);
    return true;
}
}

void Install(void* handle) {
    ready=Hook(dlsym(handle,"_ZN28CPedDamageResponseCalculator21ComputeDamageResponseEP4CPedR18CPedDamageResponseb"));
    LOGI("[combat] melee block hook=%d",ready);
}
void Reset() {
    ClearGuards();owner=nullptr;updated=0;vitalsValid=false;
    damageAt.store(0,std::memory_order_relaxed);blockAt.store(0,std::memory_order_relaxed);
}
void Update(bool blocked) {
    void* player=g.FindPlayerPed?g.FindPlayerPed(-1):nullptr;
    if (blocked || !player || !xr::IsSessionFocused()) { Reset();return; }
    const double now=Now(),dt=now-updated;
    if (owner!=player || dt<=0 || dt>.15) {
        Reset();owner=player;
    }
    const float health=Read<float>(player,0x6ac),armour=Read<float>(player,0x6b4);
    if (std::isfinite(health) && std::isfinite(armour)) {
        if (vitalsValid && (health<previousHealth-.01f || armour<previousArmour-.01f))
            damageAt.store(now,std::memory_order_relaxed);
        previousHealth=health;previousArmour=armour;vitalsValid=true;
    } else vitalsValid=false;
    updated=now;
    float head[3]{},q[4]{};xr::HandPose hands[2]{};std::uint64_t epoch{};
    if (!ready || playerlife::Dead(player) || !g.FindPlayerVehicle ||
        g.FindPlayerVehicle(-1,false) ||
        !physicalweapon::GetInteractionTrackingSnapshot(head,q,hands,&epoch)) {
        ClearGuards();return;
    }
    if (epoch!=trackingEpoch) { ClearGuards();trackingEpoch=epoch; }
    float worldHead[3]{},worldForward[3]{};
    // Quaternion-rotated LOCAL forward (-Z), projected horizontally for guard.
    const float fx=-2*(q[0]*q[2]+q[3]*q[1]);
    const float fz=-(1-2*(q[0]*q[0]+q[1]*q[1]));
    const float length=std::hypot(fx,fz);
    if (!std::isfinite(length) || length<.2f) { ClearGuards();return; }
    const float fwd[3]{head[0]+fx/length,head[1],head[2]+fz/length};
    if (!vrcam::TrackingPointToWorld(head,worldHead) ||
        !vrcam::TrackingPointToWorld(fwd,worldForward)) { ClearGuards();return; }
    headWorld={worldHead[0],worldHead[1],worldHead[2]};
    forwardWorld=Horizontal(Vec{worldForward[0],worldForward[1],worldForward[2]}-headWorld);
    for (int h=0;h<2;++h) {
        auto& guard=guards[h];
        float root[3]{},tip[3]{};int weapon{};
        if (!hands[h].valid || !melee::GuardSegment(h,root,tip,&weapon) ||
            (weapon<=1 && hands[h].grip<.45f)) { guard={};continue; }
        const Vec relative{root[0]-head[0],root[1]-head[1],root[2]-head[2]};
        const float speed=guard.sampled && dt>0 && dt<=.15?Length(relative-guard.previous)/static_cast<float>(dt):0;
        guard.previous=relative;guard.sampled=true;
        bool pose=false;float point[3]{};
        // A raised blade/shaft can protect even when its handle is lower.
        for (int i=0;i<=4;++i) {
            const float t=weapon>1?i*.25f:0;
            float p[3]{root[0]+(tip[0]-root[0])*t,root[1]+(tip[1]-root[1])*t,root[2]+(tip[2]-root[2])*t};
            const float x=p[0]-head[0],z=p[2]-head[2];
            if (combatpolicy::GuardPose(p[1]-head[1],std::hypot(x,z),(x*fx+z*fz)/length,guard.active)) {
                std::copy_n(p,3,point);pose=true;break;
            }
        }
        pose=pose && std::isfinite(speed) && speed<1.1f;
        if (!pose) { guard.active=false;guard.candidateSince=0;continue; }
        float world[3]{};
        if (!vrcam::TrackingPointToWorld(point,world)) { guard={};continue; }
        guard.direction=Horizontal(Vec{world[0],world[1],world[2]}-headWorld);
        guard.weapon=weapon>1;
        if (!guard.candidateSince) guard.candidateSince=now;
        if (!guard.active && now-guard.candidateSince>=.08) {
            guard.active=true;
            if (now>=guard.strongUntil) xr::TriggerHaptic(h,.16f,0,750);
        }
    }
}
Feedback GetFeedback() {
    const double now=Now();
    return {combatpolicy::Fade(now,damageAt.load(std::memory_order_relaxed),.7),
            combatpolicy::Fade(now,blockAt.load(std::memory_order_relaxed),.5)};
}
}
