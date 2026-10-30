#pragma once

#include <cmath>
#include <cstdint>

// Pure interaction state. The caller owns inventory, tracking and rendering.
namespace savr::radialholster {

constexpr int kMaxEntries = 13;
constexpr float kTriggerPress = 0.55f;
constexpr float kTriggerRelease = 0.35f;
constexpr float kInnerRadius = 0.045f;
constexpr float kOuterRadius = 0.32f;
constexpr float kSectorHysteresis = 0.12f; // radians

enum class Result { None, Cancelled, Selected };

struct Frame {
    bool visible{};
    int count{};
    int selected{-1};
    unsigned int atlasTexture{};
    int weaponTypes[kMaxEntries]{};
    float origin[3]{};
    float right[3]{};
    float up[3]{};
    std::uint64_t epoch{};
    bool Usable(bool trackingValid, std::uint64_t trackingEpoch) const {
        return visible && trackingValid && epoch == trackingEpoch &&
               count > 0 && count <= kMaxEntries;
    }
};

struct Menu {
    bool open = false;
    // A release is required after cancellation, including tracking loss.
    bool triggerLatched = true;
    int hand = -1;
    int point = -1;
    int count = 0;
    int selected = -1;
    int slots[kMaxEntries]{};
    float origin[3]{};
    // Orthonormal, fixed at opening in the same tracking space as the hand.
    float right[3]{};
    float up[3]{};
    std::uint64_t epoch = 0;

    void Cancel() {
        open = false;
        hand = point = selected = -1;
        count = 0;
        triggerLatched = true;
    }

    bool Begin(int newHand, int newPoint, const float position[3],
               const float axisRight[3], const float axisUp[3],
               const int* entries, int entryCount, std::uint64_t newEpoch) {
        if (open || triggerLatched || !position || !axisRight || !axisUp ||
            !entries || entryCount <= 0 || entryCount > kMaxEntries) return false;
        for (int i = 0; i < 3; ++i) {
            if (!std::isfinite(position[i]) || !std::isfinite(axisRight[i]) ||
                !std::isfinite(axisUp[i])) return false;
        }
        hand = newHand; point = newPoint; count = entryCount; selected = -1;
        for (int i = 0; i < 3; ++i) {
            origin[i] = position[i]; right[i] = axisRight[i]; up[i] = axisUp[i];
        }
        for (int i = 0; i < count; ++i) slots[i] = entries[i];
        epoch = newEpoch; open = true; triggerLatched = true;
        return true;
    }

    // Call while closed as well, so holding a trigger across a block is not
    // misinterpreted as a new press when interactions become available.
    void ObserveReleased(float trigger) {
        if (!open && std::isfinite(trigger) && trigger <= kTriggerRelease)
            triggerLatched = false;
    }

    Result Update(float trigger, bool grip, bool valid, std::uint64_t currentEpoch,
                  const float position[3], int* selectedSlot) {
        if (!open) { ObserveReleased(trigger); return Result::None; }
        if (!valid || currentEpoch != epoch || grip || !position ||
            !std::isfinite(trigger)) { Cancel(); return Result::Cancelled; }
        float x = 0, y = 0;
        for (int i = 0; i < 3; ++i) {
            if (!std::isfinite(position[i])) { Cancel(); return Result::Cancelled; }
            const float d = position[i] - origin[i];
            x += d * right[i]; y += d * up[i];
        }
        const float radius = std::sqrt(x*x + y*y);
        if (radius > kOuterRadius) { Cancel(); return Result::Cancelled; }
        constexpr float pi = 3.14159265358979323846f;
        constexpr float tau = 2.f * pi;
        if (radius < kInnerRadius) selected = -1;
        else {
            float angle = std::atan2(x, y);
            if (angle < 0) angle += tau;
            const float width = tau / count;
            int candidate = static_cast<int>(std::floor((angle + width * .5f) / width)) % count;
            if (selected >= 0 && candidate != selected) {
                float delta = std::remainder(angle - selected * width, tau);
                if (std::fabs(delta) < width * .5f + kSectorHysteresis)
                    candidate = selected;
            }
            selected = candidate;
        }
        if (trigger <= kTriggerRelease) {
            const bool chosen = selected >= 0;
            if (chosen && selectedSlot) *selectedSlot = slots[selected];
            Cancel();
            // This release itself arms the next gesture.
            triggerLatched = false;
            return chosen ? Result::Selected : Result::Cancelled;
        }
        return Result::None;
    }
};
} // namespace savr::radialholster
