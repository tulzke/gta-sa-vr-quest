#pragma once

#include <algorithm>
#include <cstdint>

namespace savr::respray {

// CGarage states in the Android Pay 'n' Spray update.
constexpr std::uint8_t kGarageType = 5;
constexpr std::uint16_t kControlLock = 0x4;
constexpr std::uint8_t kClosed = 0;
constexpr std::uint8_t kOpen = 1;
constexpr std::uint8_t kClosing = 2;
constexpr std::uint8_t kOpening = 3;
constexpr std::uint8_t kOpenAfterService = 4;

inline bool Accepted(std::uint8_t before, std::uint8_t after,
                     std::uint16_t controlsBefore, std::uint16_t controlsAfter) {
    return before == kOpen && after == kClosing &&
           !(controlsBefore & kControlLock) && (controlsAfter & kControlLock);
}

enum class Phase : std::uint8_t { Idle, Painting, Reveal };

inline Phase GetPhase(std::uint64_t cue) {
    return static_cast<Phase>(cue & 3u);
}

inline std::uint64_t Cue(std::uint64_t serial, Phase phase) {
    return (serial << 2) | static_cast<std::uint64_t>(phase);
}

// Owned by the XR thread. Advance the reveal only with a live projection.
// After an interruption, require another freshly rendered head-tracked pair.
class Fade {
public:
    float Update(std::uint64_t cue, double nowMs, bool projectionReady,
                 bool freshRevealPair) {
        if (cue != cue_) {
            cue_ = cue;
            startMs_ = nowMs;
            initialOpacity_ = opacity_;
            revealing_ = false;
            finished_ = false;
        }
        switch (GetPhase(cue)) {
            case Phase::Idle:
                opacity_ = 0.0f;
                break;
            case Phase::Painting:
                opacity_ = initialOpacity_ + (1.0f - initialOpacity_) *
                    SmoothStep((nowMs - startMs_) / 200.0);
                break;
            case Phase::Reveal:
                if (finished_) break;
                if (!projectionReady) revealing_ = false;
                if (!revealing_ && projectionReady && freshRevealPair) {
                    revealing_ = true;
                    startMs_ = nowMs;
                }
                opacity_ = revealing_
                    ? 1.0f - SmoothStep((nowMs - startMs_) / 900.0)
                    : 1.0f;
                finished_ = opacity_ == 0.0f;
                break;
        }
        return opacity_;
    }

private:
    static float SmoothStep(double progress) {
        const auto t = static_cast<float>(std::clamp(progress, 0.0, 1.0));
        return t * t * (3.0f - 2.0f * t);
    }

    std::uint64_t cue_{};
    double startMs_{};
    float initialOpacity_{}, opacity_{};
    bool revealing_{}, finished_{};
};

}  // namespace savr::respray
