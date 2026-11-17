// Included inside Xr.cpp's private namespace. All runtime haptic calls execute
// on the XR consumer; producers only publish bounded, per-hand requests.
std::mutex g_hapticMutex;
haptics::Hand g_hapticHands[2];
std::uint64_t g_hapticSerial{};
bool g_hasPcmExt{};
PFN_xrGetDeviceSampleRateFB g_getPcmRate{};
std::atomic<int> g_pcmStatus[2]{}; // 0 unavailable, 1 ready, 2 failed
float g_pcmRate[2]{};
bool g_pcmFailed[2]{};
XrPath g_pcmProfile[2]{};
double g_pcmProbeAt{}, g_hapticAppliedAt[2]{};
haptics::Effect g_hapticApplied[2];
bool g_hapticTestHeld{};
unsigned g_hapticRandom{};

double HapticNow() { return MonotonicNowNs()*1e-9; }
XrHapticActionInfo HapticInfo(int hand) {
    XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};
    info.action=s.hapticAction; info.subactionPath=s.handPaths[hand]; return info;
}
void StopHapticOutput(int hand) {
    if (g_hapticApplied[hand].source!=haptics::Source::None &&
        s.session!=XR_NULL_HANDLE && s.hapticAction!=XR_NULL_HANDLE) {
        auto info=HapticInfo(hand);
        xrStopHapticFeedback(s.session,&info);
    }
    g_hapticApplied[hand]={}; g_hapticAppliedAt[hand]=0;
}
void ResetHapticOutput() {
    std::lock_guard<std::mutex> lock(g_hapticMutex);
    for (int hand=0;hand<2;++hand) {
        StopHapticOutput(hand); g_hapticHands[hand]={};
        g_pcmRate[hand]=0; g_pcmFailed[hand]=false; g_pcmStatus[hand]=0;
    }
    // Require a fresh release before another diagnostic pulse after focus loss.
    g_hapticTestHeld=true; g_pcmProbeAt=0;
}
void ProbePcm(double now) {
    if (now<g_pcmProbeAt) return;
    g_pcmProbeAt=now+2;
    for (int hand=0;hand<2;++hand) {
        XrInteractionProfileState profile{XR_TYPE_INTERACTION_PROFILE_STATE};
        const auto profileResult=xrGetCurrentInteractionProfile(s.session,s.handPaths[hand],&profile);
        float rate=0;
        if (g_hasPcmExt && g_getPcmRate) {
            auto info=HapticInfo(hand);
            XrDevicePcmSampleRateGetInfoFB result{XR_TYPE_DEVICE_PCM_SAMPLE_RATE_GET_INFO_FB};
            if (XR_SUCCEEDED(g_getPcmRate(s.session,&info,&result)) &&
                haptics::PcmUsable(true,true,result.sampleRate)) rate=result.sampleRate;
        }
        const XrPath path=XR_SUCCEEDED(profileResult)?profile.interactionProfile:XR_NULL_PATH;
        if (rate!=g_pcmRate[hand] || path!=g_pcmProfile[hand]) {
            LOGI("[haptics] hand=%d PCM rate=%.0f profile=%llu",hand,rate,
                static_cast<unsigned long long>(path));
            g_pcmFailed[hand]=false;
        }
        g_pcmRate[hand]=rate; g_pcmProfile[hand]=path;
        g_pcmStatus[hand]=g_pcmFailed[hand]?2:(rate>0?1:0);
    }
}
void FlushHaptics() {
    if (!s.running || s.sessionState!=XR_SESSION_STATE_FOCUSED ||
        s.hapticAction==XR_NULL_HANDLE) return;
    const auto tracking=CaptureTrackingSnapshot();
    const double now=HapticNow();
    const bool menu=IsMenuVisible();
    std::lock_guard<std::mutex> lock(g_hapticMutex);
    ProbePcm(now);
    for (int hand=0;hand<2;++hand) {
        auto& requests=g_hapticHands[hand];
        if (menu) { requests.game={}; requests.wheel={}; requests.pickup={}; }
        if (requests.game.until>now && requests.game.pendingDuration>0) {
            requests.game.until=now+requests.game.pendingDuration;
            requests.game.pendingDuration=0;
        }
        auto desired=g_hapticHands[hand].Select(now,true,menu);
        if ((desired.source==haptics::Source::Wheel || desired.source==haptics::Source::Test ||
             desired.source==haptics::Source::Pickup) &&
            (desired.epoch!=tracking.epoch || !tracking.valid || !tracking.hands[hand].positionTracked)) {
            requests.wheel={}; requests.test={}; requests.pickup={}; desired={};
        }
        if (desired.amplitude<=0 || desired.source==haptics::Source::None) {
            StopHapticOutput(hand); continue;
        }
        const bool pcm=desired.pcm && g_pcmStatus[hand].load()==1;
        // Diagnostic PCM tests never silently play the fallback.
        if (desired.source==haptics::Source::Test && !pcm) { StopHapticOutput(hand); continue; }
        const auto& last=g_hapticApplied[hand];
        const bool same=last.source==desired.source && last.pcm==pcm &&
            (desired.source==haptics::Source::Wheel || desired.source==haptics::Source::Pickup || last.serial==desired.serial);
        if (same && (desired.source==haptics::Source::Game || now-g_hapticAppliedAt[hand]<.025)) continue;
        auto info=HapticInfo(hand);
        XrResult applied=XR_SUCCESS;
        bool usedPcm=pcm;
        if (pcm) {
            std::array<float,XR_MAX_HAPTIC_PCM_BUFFER_SIZE_FB> samples{};
            const auto count=haptics::Buffer(samples.data(),samples.size(),g_pcmRate[hand],
                desired.frequency,desired.amplitude,desired.wave,now*desired.frequency);
            std::uint32_t consumed=0;
            XrHapticPcmVibrationFB vibration{XR_TYPE_HAPTIC_PCM_VIBRATION_FB};
            vibration.buffer=samples.data(); vibration.bufferSize=static_cast<std::uint32_t>(count);
            vibration.sampleRate=g_pcmRate[hand]; vibration.append=XR_FALSE;
            vibration.samplesConsumed=&consumed;
            applied=count?xrApplyHapticFeedback(s.session,&info,
                reinterpret_cast<const XrHapticBaseHeader*>(&vibration)):XR_ERROR_VALIDATION_FAILURE;
            if (XR_FAILED(applied) || consumed!=count) {
                LOGW("[haptics] PCM fallback hand=%d result=%d samples=%u/%u",hand,
                    static_cast<int>(applied),consumed,static_cast<unsigned>(count));
                g_pcmFailed[hand]=true; g_pcmStatus[hand]=2; usedPcm=false;
                // A partially accepted buffer must not survive a failed test.
                xrStopHapticFeedback(s.session,&info);
                if (desired.source==haptics::Source::Test) { g_hapticApplied[hand]={}; continue; }
            }
        }
        if (!usedPcm) {
            XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
            vibration.amplitude=desired.amplitude;
            vibration.frequency=desired.source==haptics::Source::Game?desired.frequency:XR_FREQUENCY_UNSPECIFIED;
            const double duration=desired.source==haptics::Source::Game?desired.until-now:.05;
            vibration.duration=static_cast<XrDuration>(std::max(.001,duration)*1e9);
            applied=xrApplyHapticFeedback(s.session,&info,
                reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
        }
        desired.pcm=usedPcm; g_hapticApplied[hand]=desired; g_hapticAppliedAt[hand]=now;
        if (XR_FAILED(applied)) {
            // Suppress a failing output until a fresh producer request/reconnect.
            g_hapticHands[hand]={};
        }
    }
}
