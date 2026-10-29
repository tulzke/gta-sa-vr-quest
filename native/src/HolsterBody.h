#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace savr::holsterbody {
constexpr float kPi = 3.14159265358979323846f;
constexpr float Radians(float degrees) { return degrees * kPi / 180.0f; }
inline float Wrap(float angle) { return std::remainder(angle, 2.0f * kPi); }
struct Vec { float x{}, y{}, z{}; };
inline bool Finite(Vec p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
inline Vec Socket(Vec position, float yaw, float lateral, float vertical, float depth) {
    const float c = std::cos(yaw), s = std::sin(yaw);
    return {position.x + c * lateral - s * depth, position.y + vertical,
            position.z - s * lateral - c * depth};
}

// State is advanced only by the game input tick; consumers copy the result.
// No camera, controller, clock or inventory globals are accessed here.
struct Body {
    Vec position{};
    float yaw{};
    bool valid{}, following{}, frozen{}, bodyMode{};
    double lastTime{}, dwell{}, exitTime{};
    int dwellSide{};
    std::uint64_t epoch{}, resetSerial{}, frame{};

    void Invalidate() { valid = false; following = frozen = false; dwell = exitTime = 0; dwellSide = 0; }

    void Update(Vec targetPosition, float targetYaw, bool usable, bool yawReliable,
                bool useBody, std::uint64_t trackingEpoch, std::uint64_t reset,
                bool nearInner, bool nearOuter, double now) {
        ++frame;
        if (!usable || !Finite(targetPosition) || !std::isfinite(targetYaw) || !std::isfinite(now)) {
            Invalidate(); return;
        }
        const double dt = now - lastTime;
        const bool restart = !valid || epoch != trackingEpoch || resetSerial != reset ||
                             bodyMode != useBody || dt < 0 || dt > 0.25;
        lastTime = now; epoch = trackingEpoch; resetSerial = reset; bodyMode = useBody;
        position = targetPosition;
        if (restart) {
            Invalidate();
            // A vertical gaze has no stable yaw. Wait rather than choosing zero.
            if (!yawReliable) return;
            yaw = Wrap(targetYaw); valid = true;
        }
        if (!useBody) { yaw = Wrap(targetYaw); frozen = following = false; return; }
        if (nearInner) { frozen = true; exitTime = 0; }
        else if (frozen) {
            exitTime = nearOuter ? 0.0 : exitTime + dt;
            if (exitTime >= 0.2) { frozen = false; exitTime = 0; }
        }
        if (frozen || !yawReliable || restart) {
            following = false; dwell = 0; dwellSide = 0; return;
        }
        const float error = Wrap(targetYaw - yaw), magnitude = std::fabs(error);
        const int side = error < 0 ? -1 : 1;
        if (magnitude >= Radians(20)) {
            if (side != dwellSide) dwell = 0;
            dwellSide = side; dwell += dt;
        } else { dwell = 0; dwellSide = 0; }
        if (magnitude >= Radians(45) || dwell >= 0.7) following = true;
        if (magnitude <= Radians(10) + 0.0001f) following = false;
        if (!following) return;
        const float excess = std::max(0.0f, magnitude - Radians(10));
        const float step = std::min(excess * static_cast<float>(-std::expm1(-dt / 0.3)),
                                    Radians(90) * static_cast<float>(dt));
        yaw = Wrap(yaw + side * step);
    }
};
} // namespace savr::holsterbody
