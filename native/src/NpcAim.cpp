#include "NpcAim.h"
#include "PairedWeapons.h"
#include "PhysicalWeapon.h"
#include "PlayerLife.h"
#include "ScopeAim.h"
#include "Symbols.h"
#include "VrCamera.h"
#include "VrFire.h"
#include "Xr.h"
#include "Log.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

namespace savr::npcaim {
namespace {
using Vec = GameSymbols::Vec3;
using PedAdd = void* (*)(void*, void*, bool);
using GroupAdd = bool (*)(void*, void*);
PedAdd originalPedAdd{};
GroupAdd originalGroupAdd{};
void (*gunCtor)(void*, void*){};
void (*gunDtor)(void*){};
void (*groupCtor)(void*, void*, void*){};
void (*groupDtor)(void*){};
void* (*allocateEvent)(std::size_t){};
void* (*getGroup)(const void*){};
bool (*sameGroup)(const void*, const void*){};
bool (*seeingRange)(const void*, const Vec*){};
void* (*activeTask)(const void*){};
std::uint16_t (*say)(void*, unsigned short, unsigned, float, unsigned char, unsigned char, unsigned char){};
void* gunVtable{};
void* groupVtable{};
bool ready{};
// These are the four *emitters*, not the downstream group/member handlers.
// Filter only their CJ aim events, so group propagation and other events survive.
struct Emitter { void* address{}; std::size_t size{}; } emitters[4];
std::uintptr_t lastPlayer{};
std::uint32_t lastFrame{};
bool frameSeen{};

template<class T> T Read(const void* p, std::size_t offset) {
    T value{};
    std::memcpy(&value, static_cast<const char*>(p) + offset, sizeof(value));
    return value;
}
Vec Position(const void* entity) {
    const void* matrix = Read<void*>(entity, 0x18);
    return matrix ? Read<Vec>(matrix, 0x30) : Read<Vec>(entity, 8);
}
bool Finite(Vec v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
bool Supported(int type) { return (type >= 22 && type <= 34) || type == 38; }

bool OwnsAim(void* source) {
    return ready && source && g.FindPlayerPed && source == g.FindPlayerPed(-1) &&
        vrcam::IsStereoActive() && g.FindPlayerVehicle && !g.FindPlayerVehicle(-1, false) &&
        !vrfire::IsScriptedWeaponCamera() &&
        !(g.CCutsceneMgr_ms_running && *g.CCutsceneMgr_ms_running) &&
        g.TheCamera && !Read<std::uint8_t>(g.TheCamera, 0x43) &&
        !vrcam::CutsceneCameraControlsActive();
}
bool NativeEmitter(std::uintptr_t caller) {
    for (const auto& emitter : emitters) {
        const auto begin = reinterpret_cast<std::uintptr_t>(emitter.address);
        if (begin && caller >= begin && caller < begin + emitter.size) return true;
    }
    return false;
}
bool ReplacesNativeEvent(const void* event, std::uintptr_t caller) {
    return event && Read<void*>(event, 0) == gunVtable && NativeEmitter(caller) &&
        OwnsAim(Read<void*>(event, 0x18));
}
void* AddPedEvent(void* group, void* event, bool valid, std::uintptr_t caller) {
    return ReplacesNativeEvent(event, caller) ? nullptr : originalPedAdd(group, event, valid);
}
bool AddGroupEvent(void* group, void* event, std::uintptr_t caller) {
    if (event && Read<void*>(event, 0) == groupVtable &&
        ReplacesNativeEvent(Read<void*>(event, 0x18), caller)) return false;
    return originalGroupAdd(group, event);
}

__attribute__((noinline)) void* OnPedEvent(void* group, void* event, bool valid) {
    return AddPedEvent(group, event, valid,
        reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)));
}
__attribute__((noinline)) bool OnGroupEvent(void* group, void* event) {
    return AddGroupEvent(group, event,
        reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)));
}

struct LineOptions {
    void* ignore{*g.CWorld_pIgnoreEntity};
    bool dead{*g.CWorld_bIncludeDeadPeds}, tyres{*g.CWorld_bIncludeCarTyres}, bikers{*g.CWorld_bIncludeBikers};
    explicit LineOptions(void* player) {
        *g.CWorld_pIgnoreEntity = player;
        *g.CWorld_bIncludeDeadPeds = true;
        *g.CWorld_bIncludeCarTyres = true;
        *g.CWorld_bIncludeBikers = true;
    }
    ~LineOptions() {
        *g.CWorld_pIgnoreEntity = ignore;
        *g.CWorld_bIncludeDeadPeds = dead;
        *g.CWorld_bIncludeCarTyres = tyres;
        *g.CWorld_bIncludeBikers = bikers;
    }
};
bool Trace(const Vec& from, const Vec& to, void*& entity) {
    alignas(16) std::uint8_t colPoint[0x30]{};
    entity = nullptr;
    return g.CWorld_ProcessLineOfSight(&from, &to, colPoint, &entity,
        true, true, true, true, true, false, false, false);
}
void* Target(void* player, int hand, int type, float range) {
    float o[3]{}, d[3]{}, a[3]{};
    if (!vrcam::GetWeaponFireSegment(hand, type, o, d, a) ||
        !scopeaim::ApplyReticleAim(hand, type, o, d)) return nullptr;
    const Vec origin{o[0], o[1], o[2]}, anchor{a[0], a[1], a[2]};
    const float length = std::sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
    if (!Finite(origin) || !Finite(anchor) || !std::isfinite(length) || length < .001f) return nullptr;
    const Vec end{origin.x + d[0]*range/length, origin.y + d[1]*range/length, origin.z + d[2]*range/length};
    if (!Finite(end)) return nullptr;
    LineOptions options(player);
    void* hit{};
    // The calibrated source may lie beyond a wall, even when the hand is inside.
    if (Trace(anchor, origin, hit) || !Trace(origin, end, hit) || !hit || hit == player ||
        (Read<std::uint8_t>(hit, 0x5a) & 7) != 3 || !g.CPed_IsAlive(hit) ||
        playerlife::Dead(hit) || (Read<std::uint8_t>(hit, 0x585) & 1)) return nullptr;
    return hit;
}
bool Reacting(void* intelligence) {
    void* task = activeTask(static_cast<char*>(intelligence) + 8);
    if (!task) return false;
    // Verified CTask vtable +0x28 = GetTaskType; 601 = ReactToGunAimedAt.
    const void* vtable = Read<void*>(task, 0);
    const auto type = Read<int (*)(const void*)>(vtable, 0x28);
    return type(task) == 601;
}
void Send(void* player, void* target, int weaponType) {
    void* intelligence = Read<void*>(target, 0x538);
    const Vec playerPosition = Position(player);
    if (!intelligence || !Finite(playerPosition) || Reacting(intelligence) ||
        !seeingRange(intelligence, &playerPosition) || sameGroup(target, player)) return;
    // Same native speech context/repeat limits as CheckForTargetToPutHandsUp.
    if (say && weaponType != 23) say(player, 176, 0, 1.f, 0, 0, 0);
    if (void* group = getGroup(target)) {
        void* nested = allocateEvent(0x20);
        if (!nested) return;
        gunCtor(nested, player);
        alignas(16) std::uint8_t event[0x20]{};
        groupCtor(event, target, nested);
        // AddEvent clones both objects. D1 releases the target reference and
        // invokes the nested event's deleting destructor (native pool allocator).
        originalGroupAdd(static_cast<char*>(group) + 0x60, event);
        groupDtor(event);
    } else {
        alignas(16) std::uint8_t event[0x20]{};
        gunCtor(event, player);
        // Add owns AffectsPed, including its crime side effect: never pre-call it.
        originalPedAdd(static_cast<char*>(intelligence) + 0xc8, event, false);
        gunDtor(event);
    }
}

bool CoreReady() {
    return gunCtor && gunDtor && groupCtor && groupDtor && allocateEvent && getGroup && sameGroup &&
        seeingRange && activeTask && gunVtable && groupVtable && originalPedAdd && originalGroupAdd &&
        g.FindPlayerPed && g.FindPlayerVehicle && g.CPad_GetPad && g.CPed_IsAlive &&
        g.CPed_GetWeaponSkill && g.CWeaponInfo_GetWeaponInfo && g.CWorld_ProcessLineOfSight &&
        g.CWorld_pIgnoreEntity && g.CWorld_bIncludeDeadPeds && g.CWorld_bIncludeCarTyres &&
        g.CWorld_bIncludeBikers && g.CTimer_m_FrameCounter && g.CTimer_m_UserPause && g.CTimer_m_CodePause;
}

// Both verified prologues contain only stack/register instructions, no PC-relative
// operands. Prepare both trampolines and writable pages BEFORE changing either entry.
bool InstallHooks() {
    const std::uint32_t pedCode[]{0xa9bb7bfd, 0xf9000bf9, 0xa9025ff8, 0xa90357f6};
    const std::uint32_t groupCode[]{0xa9bd7bfd, 0xf9000bf5, 0xa9024ff4, 0x910003fd};
    auto* ped = reinterpret_cast<void*>(originalPedAdd);
    auto* group = reinterpret_cast<void*>(originalGroupAdd);
    if (std::memcmp(ped, pedCode, 16) || std::memcmp(group, groupCode, 16)) return false;
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) return false;
    auto* tramp = static_cast<std::uint8_t*>(mmap(nullptr, pageSize, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (tramp == MAP_FAILED) return false;
    auto jump = [](void* at, void* target) {
        const std::uint32_t code[]{0x58000051, 0xd61f0220}; // LDR X17,#8; BR X17
        std::memcpy(at, code, 8);
        std::memcpy(static_cast<char*>(at) + 8, &target, 8);
    };
    std::memcpy(tramp, ped, 16); jump(tramp + 16, static_cast<char*>(ped) + 16);
    std::memcpy(tramp + 32, group, 16); jump(tramp + 48, static_cast<char*>(group) + 16);
    __builtin___clear_cache(reinterpret_cast<char*>(tramp), reinterpret_cast<char*>(tramp) + 64);
    if (mprotect(tramp, pageSize, PROT_READ | PROT_EXEC)) { munmap(tramp, pageSize); return false; }
    const auto mask = ~static_cast<std::uintptr_t>(pageSize - 1);
    void* pages[]{reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(ped) & mask),
                  reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(group) & mask)};
    if (mprotect(pages[0], pageSize, PROT_READ | PROT_WRITE | PROT_EXEC)) { munmap(tramp, pageSize); return false; }
    if (mprotect(pages[1], pageSize, PROT_READ | PROT_WRITE | PROT_EXEC)) {
        mprotect(pages[0], pageSize, PROT_READ | PROT_EXEC); munmap(tramp, pageSize); return false;
    }
    originalPedAdd = reinterpret_cast<PedAdd>(tramp);
    originalGroupAdd = reinterpret_cast<GroupAdd>(tramp + 32);
    jump(ped, reinterpret_cast<void*>(&OnPedEvent));
    jump(group, reinterpret_cast<void*>(&OnGroupEvent));
    __builtin___clear_cache(static_cast<char*>(ped), static_cast<char*>(ped) + 16);
    __builtin___clear_cache(static_cast<char*>(group), static_cast<char*>(group) + 16);
    for (void* page : pages) if (mprotect(page, pageSize, PROT_READ | PROT_EXEC))
        LOGW("[npc.aim] unable to restore event adapter page protection");
    return true;
}
} // namespace

void Install(void* handle) {
    if (ready) return;
#define LOAD(v, name) v = reinterpret_cast<decltype(v)>(dlsym(handle, name))
    LOAD(gunCtor, "_ZN16CEventGunAimedAtC1EP4CPed");
    LOAD(gunDtor, "_ZN16CEventGunAimedAtD1Ev");
    LOAD(groupCtor, "_ZN16CEventGroupEventC1EP4CPedP6CEvent");
    LOAD(groupDtor, "_ZN16CEventGroupEventD1Ev");
    LOAD(allocateEvent, "_ZN6CEventnwEm");
    LOAD(getGroup, "_ZN10CPedGroups12GetPedsGroupEPK4CPed");
    LOAD(sameGroup, "_ZN10CPedGroups14AreInSameGroupEPK4CPedS2_");
    LOAD(seeingRange, "_ZNK16CPedIntelligence15IsInSeeingRangeERK7CVector");
    LOAD(activeTask, "_ZNK12CTaskManager13GetActiveTaskEv");
    LOAD(say, "_ZN4CPed3SayEtjfhhh");
    LOAD(originalGroupAdd, "_ZN21CPedGroupIntelligence8AddEventER6CEvent");
    originalPedAdd = g.CEventGroup_Add;
    gunVtable = dlsym(handle, "_ZTV16CEventGunAimedAt");
    groupVtable = dlsym(handle, "_ZTV16CEventGroupEvent");
    if (gunVtable) gunVtable = static_cast<char*>(gunVtable) + 16;
    if (groupVtable) groupVtable = static_cast<char*>(groupVtable) + 16;
    emitters[0] = {dlsym(handle, "_Z26CheckForTargetToPutHandsUpP10CPlayerPedP4CPed"), 0x1cc};
    emitters[1] = {dlsym(handle, "_ZN10CPlayerPed22FindWeaponLockOnTargetEv"), 0x624};
    emitters[2] = {dlsym(handle, "_ZN10CPlayerPed26FindNextWeaponLockOnTargetEP7CEntityb"), 0x5bc};
    emitters[3] = {dlsym(handle, "_ZN10CPlayerPed27Compute3rdPersonMouseTargetEb"), 0x3d4};
#undef LOAD
    // Fingerprints cover the retail emitter/layout revision before any patches.
    const std::uint32_t expected[4][4]{
        {0xd101c3ff, 0xa9037bfd, 0xf90023f7, 0xa90557f6},
        {0xd102c3ff, 0x6d0233ed, 0x6d032beb, 0x6d0423e9},
        {0xd10343ff, 0xfd0023ec, 0x6d052beb, 0x6d0623e9},
        {0xd10303ff, 0xa9087bfd, 0xa9095ff8, 0xa90a57f6}};
    if (!CoreReady()) { LOGW("[npc.aim] missing engine symbols; bridge disabled"); return; }
    for (int i = 0; i < 4; ++i) if (!emitters[i].address || std::memcmp(emitters[i].address, expected[i], 16)) {
        LOGW("[npc.aim] unsupported emitter %d; bridge disabled", i); return;
    }
    const std::uint32_t gunCode[]{0xa9be7bfd, 0xa9014ff4, 0x910003fd, 0xf0001a69};
    const std::uint32_t groupCode[]{0xd0001a88, 0xf945c908, 0xb900081f, 0x3900301f};
    if (std::memcmp(reinterpret_cast<void*>(gunCtor), gunCode, 16) ||
        std::memcmp(reinterpret_cast<void*>(groupCtor), groupCode, 16)) {
        LOGW("[npc.aim] unsupported event layout; bridge disabled"); return;
    }
    if (!InstallHooks()) { LOGW("[npc.aim] event adapters unavailable; bridge disabled"); return; }
    ready = true;
    LOGI("[npc.aim] physical on-foot aim reactions installed (native NPC/group responses)");
}

void Update(bool blocked) {
    void* player = g.FindPlayerPed ? g.FindPlayerPed(-1) : nullptr;
    if (!OwnsAim(player) || blocked || !xr::IsSessionFocused() || xr::IsMenuVisible() ||
        vrcam::IsMobileMenuOpen() || *g.CTimer_m_UserPause || *g.CTimer_m_CodePause ||
        (g.CTimer_bSkipProcessThisFrame && *g.CTimer_bSkipProcessThisFrame) ||
        !vrcam::GroundWalkControlsActive() || !g.CPed_IsAlive(player) || playerlife::Dead(player)) return;
    void* pad = g.CPad_GetPad(0);
    if (!pad || Read<std::uint16_t>(pad, 0x110)) return;
    float head[3]{}, orientation[4]{};
    xr::HandPose hands[2]{};
    std::uint64_t epoch{};
    if (!physicalweapon::GetInteractionTrackingSnapshot(head, orientation, hands, &epoch) ||
        !vrcam::IsWeaponTrackingCurrent(epoch)) return;
    const auto playerId = reinterpret_cast<std::uintptr_t>(player);
    const auto frame = *g.CTimer_m_FrameCounter;
    if (frameSeen && lastPlayer == playerId && lastFrame == frame) return;
    frameSeen = true; lastPlayer = playerId; lastFrame = frame;
    void* targets[2]{}; // never retain entity pointers beyond this update
    for (int hand = 0; hand < 2; ++hand) {
        const int slot = physicalweapon::HeldSlot(hand);
        if (!pairedweapon::Valid(slot) || !hands[hand].valid || !hands[hand].aimValid ||
            physicalweapon::IsPrimaryDetached(hand)) continue;
        const int type = pairedweapon::Type(player, slot);
        if (!Supported(type)) continue;
        const int skill = std::clamp<int>(g.CPed_GetWeaponSkill(player, type), 0, 3);
        const void* info = g.CWeaponInfo_GetWeaponInfo(type, static_cast<signed char>(skill));
        if (!info || Read<int>(info, 0) != 1 || !(Read<unsigned>(info, 0x18) & 1)) continue;
        // Target range (+4), as in native mouse targeting, not damage range (+8).
        const float range = Read<float>(info, 4);
        if (!std::isfinite(range) || range <= 0) continue;
        targets[hand] = Target(player, hand, type, std::min(range, 300.f));
        if (targets[hand] && (hand == 0 || targets[hand] != targets[0])) Send(player, targets[hand], type);
    }
}
} // namespace savr::npcaim
