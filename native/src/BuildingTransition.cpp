#include "BuildingTransition.h"

#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <cstring>

#include "Log.h"
#include "Symbols.h"
#include "Xr.h"

namespace savr::buildingtransition {
namespace {

constexpr std::uintptr_t kTransitionStarted = 0x3e8f8c;
constexpr std::uintptr_t kDoorApproach = 0x3e90f4;
constexpr std::uintptr_t kPointApproach = 0x3e9234;
constexpr std::uintptr_t kGetPadPlt = 0x7f4b80;
constexpr std::uintptr_t kReturnAccepted = 0x3e93bc;
constexpr int kWaitingForApproach = 1;

constexpr std::uint32_t Branch(std::int32_t instruction, std::int32_t target,
                               bool link = false) {
    return (link ? 0x94000000u : 0x14000000u) |
           (static_cast<std::uint32_t>((target - instruction) / 4) & 0x03ffffffu);
}

// Only the two paths that would queue GotoDoorAndOpen reach these sites.
// Both keep the native CPad lock. The common path for the prototype returns
// before queuing either ped task, so there is no scripted walk or idle task.
constexpr std::uint32_t kStockDoorApproach = 0xaa0003f5; // mov x21, x0
constexpr std::uint32_t kStockPointApproach[] = {
    0x52800900, 0x9410490e, 0x0f00f600, 0x1e221001, 0xfd401be2, 0xbd403be3,
};
constexpr std::uint32_t kLockAndReturn[] = {
    0x2a1f03e0,                                    // mov w0, wzr
    Branch(kPointApproach + 4, kGetPadPlt, true),    // bl CPad::GetPad(0)
    0x79422008,                                    // ldrh w8, [x0, #0x110]
    0x321d0108,                                    // orr w8, w8, #0x8
    0x79022008,                                    // strh w8, [x0, #0x110]
    Branch(kPointApproach + 20, kReturnAccepted),   // return accepted
};
static_assert(sizeof(kStockPointApproach) == sizeof(kLockAndReturn));

const int* g_entryState = nullptr;
void* const* g_activeEntry = nullptr;
const std::uint8_t* g_nativeOpacity = nullptr;
void* g_transition = nullptr;
bool g_revealing = false;
std::atomic<std::uint8_t> g_opacity{0};

}  // namespace

void Install(void* handle) {
    if (!handle || !g.LoadBase || g_entryState) return;
    auto address = [](std::uintptr_t offset) { return g.LoadBase + offset; };
    const auto start = reinterpret_cast<std::uintptr_t>(
        dlsym(handle, "_ZN10CEntryExit17TransitionStartedEP4CPed"));
    const auto* state = static_cast<const int*>(
        dlsym(handle, "_ZN17CEntryExitManager17ms_exitEnterStateE"));
    auto* entry = static_cast<void* const*>(
        dlsym(handle, "_ZN17CEntryExitManager9mp_ActiveE"));
    const auto* opacity = static_cast<const std::uint8_t*>(
        dlsym(handle, "_ZN5CDraw9FadeValueE"));
    if (start != address(kTransitionStarted) || !state || !entry || !opacity) {
        LOGW("[building] native portal symbols do not match");
        return;
    }

    auto* door = reinterpret_cast<std::uint32_t*>(address(kDoorApproach));
    auto* point = reinterpret_cast<std::uint32_t*>(address(kPointApproach));
    const auto* returnAccepted = reinterpret_cast<const std::uint32_t*>(address(kReturnAccepted));
    if (*door != kStockDoorApproach ||
        std::memcmp(point, kStockPointApproach, sizeof(kStockPointApproach)) != 0 ||
        *returnAccepted != 0x2a1803e0) { // mov w0, w24
        LOGW("[building] native approach instructions do not match");
        return;
    }

    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) return;
    const auto pageMask = static_cast<std::uintptr_t>(pageSize) - 1;
    const auto page = address(kDoorApproach) & ~pageMask;
    if (((address(kPointApproach) + sizeof(kLockAndReturn) - 1) & ~pageMask) != page ||
        mprotect(reinterpret_cast<void*>(page), pageSize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        LOGW("[building] could not patch portal approach page");
        return;
    }

    std::memcpy(point, kLockAndReturn, sizeof(kLockAndReturn));
    *door = Branch(kDoorApproach, kPointApproach);
    const auto clearCache = [&] {
        __builtin___clear_cache(reinterpret_cast<char*>(door),
                                reinterpret_cast<char*>(point + sizeof(kLockAndReturn) / sizeof(*point)));
    };
    clearCache();
    if (mprotect(reinterpret_cast<void*>(page), pageSize, PROT_READ | PROT_EXEC) != 0) {
        *door = kStockDoorApproach;
        std::memcpy(point, kStockPointApproach, sizeof(kStockPointApproach));
        clearCache();
        mprotect(reinterpret_cast<void*>(page), pageSize, PROT_READ | PROT_EXEC);
        LOGW("[building] could not restore portal page protection");
        return;
    }

    g_entryState = state;
    g_activeEntry = entry;
    g_nativeOpacity = opacity;
    LOGI("[building] direct pedestrian portals enabled");
}

void Update() {
    if (!g_entryState) return;
    const auto opacity = *g_nativeOpacity;
    auto* entry = *g_activeEntry;
    if (entry && *g_entryState == kWaitingForApproach) {
        g_transition = entry;
        g_revealing = false;
    } else if (g_transition && entry != g_transition) {
        g_transition = nullptr;
        g_revealing = opacity != 0;
        // The native transition has finished moving CJ. The compositor must
        // wait for stereo images from the destination before exposing it.
        xr::ResetStereoEyeTextures();
    } else if (g_revealing && opacity == 0) {
        g_revealing = false;
    }
    g_opacity.store(Active() ? opacity : 0, std::memory_order_release);
}

bool Active() {
    return g_transition || g_revealing;
}

float Opacity() {
    return static_cast<float>(g_opacity.load(std::memory_order_acquire)) / 255.0f;
}

}  // namespace savr::buildingtransition
