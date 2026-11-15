#include "WantedFeedback.h"
#include "PlayerLife.h"
#include "Symbols.h"
#include "VrCamera.h"
#include "Xr.h"

#include <chrono>
#include <cstring>
#include <mutex>

namespace savr::wantedfeedback {
namespace {
// One lock publishes the entire event and its reset/clock/tracking boundaries.
// Engine memory and the head-pose mutex are never accessed while holding it.
std::mutex feedbackMutex;
detail::State state;

double Now() {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

detail::Observation ReadObservation(bool gameplay) {
    detail::Observation input{};
    if (!gameplay || !xr::IsSessionFocused() || xr::IsMenuVisible() ||
        vrcam::IsMobileMenuOpen() ||
        (g.CTimer_m_UserPause && *g.CTimer_m_UserPause) ||
        (g.CTimer_m_CodePause && *g.CTimer_m_CodePause) ||
        (g.CCutsceneMgr_ms_running && *g.CCutsceneMgr_ms_running) ||
        (g.TheCamera && *(static_cast<const std::uint8_t*>(g.TheCamera)+0x43)) ||
        !g.CWorld_Players || !g.CWorld_PlayerInFocus || *g.CWorld_PlayerInFocus>=2 ||
        !g.CTimer_m_FrameCounter || !g.CTimer_m_snTimeInMilliseconds)
        return input;
    float position[3]{},orientation[4]{};
    if (!xr::GetHeadPose(position,orientation,&input.trackingEpoch)) return input;
    void* player=g.FindPlayerPed?g.FindPlayerPed(-1):nullptr;
    if (!player || playerlife::Dead(player)) return input;

    const auto* info=static_cast<const std::uint8_t*>(g.CWorld_Players)+
        *g.CWorld_PlayerInFocus*0x1d8;
    void* infoPlayer{};
    std::memcpy(&infoPlayer,info,sizeof(infoPlayer));
    // Retail 2.11 ArrestPlayer @0x4b0620: CPlayerInfo+0x118 = 2.
    // KillPlayer sets 1, critical mission failure sets 3; playing is 0.
    if (infoPlayer!=player || info[0x118]!=0) return input;

    void* wanted=g.FindPlayerWanted?g.FindPlayerWanted(-1):nullptr;
    if (!wanted) return input;
    std::memcpy(&input.level,static_cast<const std::uint8_t*>(wanted)+0x2c,4);
    input.player=reinterpret_cast<std::uintptr_t>(player);
    input.wanted=reinterpret_cast<std::uintptr_t>(wanted);
    input.gameFrame=*g.CTimer_m_FrameCounter;
    input.gameMs=*g.CTimer_m_snTimeInMilliseconds;
    input.enabled=true;
    return input;
}
} // namespace

void Update(bool gameplay) {
    const auto observation=ReadObservation(gameplay);
    const double now=Now();
    std::lock_guard<std::mutex> lock(feedbackMutex);
    state.Observe(observation,now);
}

Feedback GetFeedback(bool visible,std::uint64_t trackingEpoch) {
    const double now=Now();
    std::lock_guard<std::mutex> lock(feedbackMutex);
    if (!visible) { state.Reset();return {}; }
    return state.Sample(now,trackingEpoch);
}
} // namespace savr::wantedfeedback
