#include "../vendor/openxr/include/openxr/openxr.h"
#include "HapticPolicy.h"
#include "TrackingFrame.h"
#include <array>
#include <atomic>
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
float rates[2]{2000,0};
int applyCount[2]{},stopCount[2]{},pcmCount[2]{};
float amplitudes[2]{};
bool pcmError{},partial{};
XRAPI_ATTR XrResult XRAPI_CALL xrApplyHapticFeedback(XrSession,const XrHapticActionInfo* info,const XrHapticBaseHeader* effect){
    const int hand=static_cast<int>(info->subactionPath)-1;++applyCount[hand];
    if(effect->type==XR_TYPE_HAPTIC_PCM_VIBRATION_FB){
        ++pcmCount[hand];const auto& p=*reinterpret_cast<const XrHapticPcmVibrationFB*>(effect);
        CHECK(p.append==XR_FALSE&&p.bufferSize>0&&p.bufferSize<=XR_MAX_HAPTIC_PCM_BUFFER_SIZE_FB);
        *p.samplesConsumed=partial?p.bufferSize/2:p.bufferSize;
        return pcmError?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;
    }
    amplitudes[hand]=reinterpret_cast<const XrHapticVibration*>(effect)->amplitude;
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrStopHapticFeedback(XrSession,const XrHapticActionInfo* info){
    ++stopCount[static_cast<int>(info->subactionPath)-1];return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetCurrentInteractionProfile(XrSession,XrPath path,XrInteractionProfileState* result){
    result->interactionProfile=path+10;return XR_SUCCESS;
}
XrResult XRAPI_CALL FakeRate(XrSession,const XrHapticActionInfo* info,XrDevicePcmSampleRateGetInfoFB* result){
    result->sampleRate=rates[static_cast<int>(info->subactionPath)-1];return XR_SUCCESS;
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
    g_hapticHands[0].wheel={Source::Wheel,.2f,40,1.1,true};
    FlushHaptics();CHECK(pcmCount[0]==0&&amplitudes[0]==.2f); // no extension
    g_hasPcmExt=true;g_getPcmRate=FakeRate;g_pcmProbeAt=0;
    clockTime+=.03;FlushHaptics();CHECK(pcmCount[0]==1&&g_pcmStatus[0]==1&&g_pcmStatus[1]==0);
    g_hapticHands[1].wheel={Source::Wheel,.25f,40,2,true};
    FlushHaptics();CHECK(amplitudes[1]==.25f&&pcmCount[1]==0); // per-hand fallback
    g_hapticHands[0].game={Source::Game,.8f,0,1.3,false,0,99};
    FlushHaptics();CHECK(amplitudes[0]==.8f);
    const auto stops=stopCount[0];g_hapticHands[0].wheel={};FlushHaptics();CHECK(stopCount[0]==stops);
    tracking.hands[1].positionTracked=false;FlushHaptics();CHECK(stopCount[1]>0);
    clockTime=1.4;FlushHaptics();CHECK(stopCount[0]>stops); // finished game, no stale wheel
    tracking.hands[1].positionTracked=true;
    menu=true;g_hapticHands[0].test={Source::Test,.2f,40,1.75,true,1,101};
    FlushHaptics();CHECK(pcmCount[0]==2);
    g_hapticHands[0].test={};FlushHaptics();CHECK(g_hapticApplied[0].source==Source::None);
    g_hapticHands[1].test={Source::Test,.2f,40,1.75,true,1,102};
    const auto rightCalls=applyCount[1];FlushHaptics();CHECK(applyCount[1]==rightCalls); // test never lies with fallback
    menu=false;clockTime=2;
    g_hapticHands[0].wheel={Source::Wheel,.2f,40,2.1,true};
    partial=true;FlushHaptics();CHECK(g_pcmStatus[0]==2&&amplitudes[0]==.2f);
    const auto failedCalls=pcmCount[0];clockTime+=.03;FlushHaptics();CHECK(pcmCount[0]==failedCalls);
    partial=false;rates[0]=0;g_pcmProbeAt=0;FlushHaptics();CHECK(g_pcmStatus[0]==0);
    rates[0]=2000;g_pcmProbeAt=0;clockTime+=.03;FlushHaptics();CHECK(g_pcmStatus[0]==1&&pcmCount[0]>failedCalls);
    pcmError=true;clockTime+=.03;FlushHaptics();CHECK(g_pcmStatus[0]==2);
    ResetHapticOutput();CHECK(g_hapticHands[0].wheel.source==Source::None&&g_hapticTestHeld);
    CHECK(g_hapticApplied[0].source==Source::None&&g_hapticApplied[1].source==Source::None);
    // A 15 ms pulse survives the publication-to-XR frame delay, at full length.
    clockTime=3;g_hapticHands[0].game={Source::Game,.6f,0,3.1,false,0,200,0,.015};
    clockTime=3.025;FlushHaptics();CHECK(amplitudes[0]==.6f);
    CHECK(g_hapticHands[0].game.until>3.039&&g_hapticHands[0].game.pendingDuration==0);
    clockTime=3.05;FlushHaptics();CHECK(g_hapticApplied[0].source==Source::None);
    tracking.epoch=2;g_hapticHands[0].wheel={Source::Wheel,.2f,40,4,false,0,201,1};
    const auto calls=applyCount[0];FlushHaptics();CHECK(applyCount[0]==calls);
    g_hapticHands[0].wheel={Source::Wheel,.2f,40,4,false,0,202,2};FlushHaptics();CHECK(applyCount[0]>calls);
    menu=true;FlushHaptics();menu=false;FlushHaptics();CHECK(g_hapticApplied[0].source==Source::None);
    clockTime=4;
    TriggerHaptic(0,.7f,0,15);CHECK(g_hapticHands[0].game.pendingDuration==.015);
    PublishWheelHaptic(1,.2f,false,0,40);CHECK(g_hapticHands[1].wheel.epoch==tracking.epoch);
    clockTime+=.03;FlushHaptics();CHECK(amplitudes[0]==.7f&&amplitudes[1]==.2f);
    TriggerHaptic(0,0,0,1);FlushHaptics();CHECK(g_hapticApplied[0].source==Source::None);
    UpdatePcmHapticTest(false,0,.5f,3,40);
    UpdatePcmHapticTest(true,0,.5f,3,40);const auto trial=g_hapticHands[0].test.serial;
    CHECK(g_hapticHands[0].test.wave>=0&&g_hapticHands[0].test.wave<3);
    UpdatePcmHapticTest(true,0,.5f,3,40);CHECK(g_hapticHands[0].test.serial==trial);
    s.sessionState=XR_SESSION_STATE_VISIBLE;ResetHapticOutput();
    UpdatePcmHapticTest(true,0,.5f,3,40);CHECK(g_hapticTestHeld);
    UpdatePcmHapticTest(false,0,.5f,3,40);CHECK(g_hapticTestHeld); // inactive input is not a release
    s.sessionState=XR_SESSION_STATE_FOCUSED;
    UpdatePcmHapticTest(true,0,.5f,3,40);CHECK(g_hapticHands[0].test.source==Source::None);
    UpdatePcmHapticTest(false,0,.5f,3,40);UpdatePcmHapticTest(true,0,.5f,3,40);
    CHECK(g_hapticHands[0].test.source==Source::Test);
    menu=true;tracking.hands[0].positionTracked=false;FlushHaptics();
    CHECK(g_hapticHands[0].test.source==Source::None);
    tracking.hands[0].positionTracked=true;UpdatePcmHapticTest(true,0,.5f,3,40);
    CHECK(g_hapticHands[0].test.source==Source::None); // no queued test after tracking returns
    UpdatePcmHapticTest(false,0,.5f,3,40);CHECK(g_hapticHands[0].test.source==Source::None);
    std::puts("production XR adapter: capabilities, PCM failure, partial buffers, stop and priorities: PASS");
}
