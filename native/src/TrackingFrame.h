#pragma once

#include <cstdint>

namespace savr::xr {

// Controller poses in OpenXR LOCAL space. Kept independent of JNI/GL so the
// frame-snapshot lifetime and pose readers can be exercised on the host.
struct HandPose {
    bool valid{false};
    bool aimValid{false};
    float gripPos[3]{};
    float gripOri[4]{0, 0, 0, 1}; // x, y, z, w
    float aimPos[3]{};
    float aimOri[4]{0, 0, 0, 1};
    float grip{}; // squeeze [0, 1]
    float trigger{};
    bool wristAnchorValid{};
    float wristGripPos[3]{}; // tracked wrist before a visual grip override
    bool ledgeGrip{}; // fixed palm/fingers with a tracked wrist
    bool climbGrip{}; // render-only conventional hand frame
    float climbRight[3]{}, climbForward[3]{}, climbUp[3]{};
};

struct TrackingSnapshot {
    bool valid{};
    std::uint64_t epoch{}, sequence{};
    std::int64_t displayTime{};
    float headPosition[3]{}, headOrientation[4]{0, 0, 0, 1};
    float eyePosition[2][3]{}, eyeOrientation[2][4]{};
    HandPose hands[2]{};
};

// Pin a value, not the publication mutex: the XR consumer keeps publishing at
// headset cadence while every pose reader on this GameThread sees one sample.
// Invalid samples are pinned too; recovery must not change space mid-frame.
class ScopedTrackingFrame {
public:
    explicit ScopedTrackingFrame(const TrackingSnapshot& snapshot)
        : snapshot_(snapshot), previous_(current_) { current_ = &snapshot_; }
    ~ScopedTrackingFrame() { current_ = previous_; }
    ScopedTrackingFrame(const ScopedTrackingFrame&) = delete;
    ScopedTrackingFrame& operator=(const ScopedTrackingFrame&) = delete;
    static const TrackingSnapshot* Current() { return current_; }

private:
    const TrackingSnapshot snapshot_;
    const TrackingSnapshot* previous_;
    inline static thread_local const TrackingSnapshot* current_{};
};

} // namespace savr::xr
