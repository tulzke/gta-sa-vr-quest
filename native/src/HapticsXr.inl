// Included inside Xr.cpp's private namespace. All runtime haptic calls execute
// on the XR consumer; producers only publish bounded, per-hand requests.
std::mutex g_hapticMutex;
haptics::Hand g_hapticHands[2];
std::uint64_t g_hapticSerial{};
double g_hapticAppliedAt[2]{};
haptics::Effect g_hapticApplied[2];
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
    }
}
void FlushHaptics() {
    if (!s.running || s.sessionState!=XR_SESSION_STATE_FOCUSED ||
        s.hapticAction==XR_NULL_HANDLE) return;
    const auto tracking=CaptureTrackingSnapshot();
    const double now=HapticNow();
    const bool menu=IsMenuVisible();
    std::lock_guard<std::mutex> lock(g_hapticMutex);
    for (int hand=0;hand<2;++hand) {
        auto& requests=g_hapticHands[hand];
        if (menu) { requests.game={}; requests.wheel={}; requests.pickup={}; }
        if (requests.game.until>now && requests.game.pendingDuration>0) {
            requests.game.until=now+requests.game.pendingDuration;
            requests.game.pendingDuration=0;
        }
        auto desired=g_hapticHands[hand].Select(now,true,menu);
        if ((desired.source==haptics::Source::Wheel ||
             desired.source==haptics::Source::Pickup) &&
            (desired.epoch!=tracking.epoch || !tracking.valid || !tracking.hands[hand].positionTracked)) {
            requests.wheel={}; requests.pickup={}; desired={};
        }
        if (desired.amplitude<=0 || desired.source==haptics::Source::None) {
            StopHapticOutput(hand); continue;
        }
        const auto& last=g_hapticApplied[hand];
        const bool same=last.source==desired.source &&
            (desired.source==haptics::Source::Wheel || desired.source==haptics::Source::Pickup || last.serial==desired.serial);
        if (same && (desired.source==haptics::Source::Game || now-g_hapticAppliedAt[hand]<.025)) continue;
        auto info=HapticInfo(hand);
        XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
        vibration.amplitude=desired.amplitude;
        vibration.frequency=desired.source==haptics::Source::Game?desired.frequency:XR_FREQUENCY_UNSPECIFIED;
        const double duration=desired.source==haptics::Source::Game?desired.until-now:.05;
        vibration.duration=static_cast<XrDuration>(std::max(.001,duration)*1e9);
        const auto applied=xrApplyHapticFeedback(s.session,&info,
            reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
        g_hapticApplied[hand]=desired; g_hapticAppliedAt[hand]=now;
        if (XR_FAILED(applied)) {
            // Suppress a failing output until a fresh producer request/reconnect.
            g_hapticHands[hand]={};
        }
    }
}
