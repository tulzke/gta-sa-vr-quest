#include "../vendor/openxr/include/openxr/openxr.h"
#include "HapticPolicy.h"
#include "TrackingFrame.h"
#include <cmath>
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#define LOGI(...) ((void)0)
#define LOGW(...) ((void)0)
#define ANDROID_LOG_INFO 4
#define SAVR_TAG "test"
#define __android_log_print(...) ((void)0)
#define CHECK(c) do {if(!(c)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
int applyCount[2]{},stopCount[2]{};
float amplitudes[2]{},frequencies[2]{};
XrDuration durations[2]{};
XRAPI_ATTR XrResult XRAPI_CALL xrApplyHapticFeedback(XrSession,const XrHapticActionInfo* info,const XrHapticBaseHeader* effect){
    const int hand=static_cast<int>(info->subactionPath)-1;++applyCount[hand];
    CHECK(effect->type==XR_TYPE_HAPTIC_VIBRATION);
    const auto& vibration=*reinterpret_cast<const XrHapticVibration*>(effect);
    amplitudes[hand]=vibration.amplitude;frequencies[hand]=vibration.frequency;durations[hand]=vibration.duration;
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrStopHapticFeedback(XrSession,const XrHapticActionInfo* info){
    ++stopCount[static_cast<int>(info->subactionPath)-1];return XR_SUCCESS;
}
namespace savr::xr {
struct Session {
    XrSession session{reinterpret_cast<XrSession>(1)};
    XrAction hapticAction{reinterpret_cast<XrAction>(2)};
    XrPath handPaths[2]{1,2};
    bool running{true};XrSessionState sessionState{XR_SESSION_STATE_FOCUSED};
} s;
double clockTime=1;
bool menu{};
TrackingSnapshot tracking;
std::uint64_t MonotonicNowNs(){return static_cast<std::uint64_t>(clockTime*1e9);}
TrackingSnapshot CaptureTrackingSnapshot(){return tracking;}
bool IsMenuVisible(){return menu;}
bool IsSessionFocused(){return s.sessionState==XR_SESSION_STATE_FOCUSED;}
#include "HapticsXr.inl"
// PRODUCTION_PUBLISHERS
}
int main(){
    using namespace savr::xr;using namespace savr::haptics;
    tracking.valid=true;for(auto& hand:tracking.hands)hand.positionTracked=true;
    PublishWheelHaptic(0,.2f);PublishWheelHaptic(1,.25f);
    CHECK(applyCount[0]==0&&applyCount[1]==0); // producers never call OpenXR
    FlushHaptics();CHECK(amplitudes[0]==.2f&&amplitudes[1]==.25f);
    CHECK(frequencies[0]==XR_FREQUENCY_UNSPECIFIED&&durations[0]==50000000);
    TriggerHaptic(0,.8f,120,100);FlushHaptics();CHECK(amplitudes[0]==.8f&&frequencies[0]==120);
    const auto stops=stopCount[0];PublishWheelHaptic(0,0);FlushHaptics();CHECK(stopCount[0]==stops);
    tracking.hands[1].positionTracked=false;FlushHaptics();CHECK(stopCount[1]>0);
    clockTime=1.4;FlushHaptics();CHECK(stopCount[0]>stops);
    tracking.hands[1].positionTracked=true;
    PublishWheelHaptic(0,1);FlushHaptics();CHECK(amplitudes[0]==.45f);
    s.sessionState=XR_SESSION_STATE_VISIBLE;ResetHapticOutput();
    CHECK(g_hapticApplied[0].source==Source::None&&g_hapticApplied[1].source==Source::None);
    s.sessionState=XR_SESSION_STATE_FOCUSED;
    // A 15 ms pulse survives the publication-to-XR frame delay, at full length.
    clockTime=3;g_hapticHands[0].game={Source::Game,.6f,0,3.1,200,0,.015};
    clockTime=3.025;FlushHaptics();CHECK(amplitudes[0]==.6f);
    CHECK(durations[0]>=14999999&&durations[0]<=15000001);
    CHECK(g_hapticHands[0].game.until>3.039&&g_hapticHands[0].game.pendingDuration==0);
    clockTime=3.05;FlushHaptics();CHECK(g_hapticApplied[0].source==Source::None);
    tracking.epoch=2;g_hapticHands[0].wheel={Source::Wheel,.2f,40,4,201,1};
    const auto calls=applyCount[0];FlushHaptics();CHECK(applyCount[0]==calls);
    g_hapticHands[0].wheel={Source::Wheel,.2f,40,4,202,2};FlushHaptics();CHECK(applyCount[0]>calls);
    menu=true;FlushHaptics();menu=false;FlushHaptics();CHECK(g_hapticApplied[0].source==Source::None);
    clockTime=4;
    TriggerHaptic(0,.7f,0,15);CHECK(g_hapticHands[0].game.pendingDuration==.015);
    PublishWheelHaptic(1,.2f);CHECK(g_hapticHands[1].wheel.epoch==tracking.epoch);
    clockTime+=.03;FlushHaptics();CHECK(amplitudes[0]==.7f&&amplitudes[1]==.2f);
    TriggerHaptic(0,0,0,1);FlushHaptics();CHECK(g_hapticApplied[0].source==Source::None);
    // Pickup is a separate continuous source, bounded and refreshed at 40 Hz.
    ResetHapticOutput();menu=false;clockTime=5;tracking.epoch=3;tracking.valid=true;
    tracking.hands[0].positionTracked=tracking.hands[1].positionTracked=true;
    PublishWheelHaptic(0,.3f);PublishPickupHaptic(0,.1f,3);FlushHaptics();
    PublishWheelHaptic(0,0);
    CHECK(g_hapticApplied[0].source==Source::Pickup&&amplitudes[0]==.1f&&g_hapticApplied[1].source==Source::None);
    clockTime+=.03;PublishPickupHaptic(0,.25f,3);FlushHaptics();CHECK(amplitudes[0]==.25f);
    auto pickupCalls=applyCount[0];clockTime+=.01;PublishPickupHaptic(0,1.f,3);FlushHaptics();
    CHECK(applyCount[0]==pickupCalls&&g_hapticHands[0].pickup.amplitude==.35f);
    clockTime+=.02;FlushHaptics();CHECK(amplitudes[0]==.35f);
    PublishPickupHaptic(0,0,3);FlushHaptics();CHECK(g_hapticApplied[0].source==Source::None);
    // Cancelling pickup cannot cancel completion/damage/other game pulses.
    TriggerHaptic(0,.8f,0,100);PublishPickupHaptic(0,.3f,3);FlushHaptics();CHECK(amplitudes[0]==.8f);
    const auto gameStops=stopCount[0];PublishPickupHaptic(0,0,3);FlushHaptics();
    CHECK(stopCount[0]==gameStops&&g_hapticApplied[0].source==Source::Game);
    clockTime+=.11;FlushHaptics();CHECK(g_hapticApplied[0].source==Source::None);
    PublishPickupHaptic(1,.2f,3);FlushHaptics();CHECK(g_hapticApplied[1].source==Source::Pickup);
    clockTime+=.11;FlushHaptics();CHECK(g_hapticApplied[1].source==Source::None); // publication timeout
    PublishPickupHaptic(1,.2f,2);FlushHaptics();CHECK(g_hapticApplied[1].source==Source::None); // stale epoch
    PublishPickupHaptic(1,.2f,3);FlushHaptics();tracking.hands[1].positionTracked=false;FlushHaptics();
    CHECK(g_hapticApplied[1].source==Source::None);
    tracking.hands[1].positionTracked=true;PublishPickupHaptic(1,.2f,3);FlushHaptics();
    menu=true;FlushHaptics();menu=false;FlushHaptics();CHECK(g_hapticApplied[1].source==Source::None);
    PublishPickupHaptic(1,.2f,3);s.sessionState=XR_SESSION_STATE_VISIBLE;PublishPickupHaptic(1,.2f,3);
    CHECK(g_hapticHands[1].pickup.source==Source::None);
    std::puts("production XR adapter: ordinary vibration, stop, priorities and pickup ramps: PASS");
}
