#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace savr::wheelhaptics {
struct Vec { float x{}, y{}, z{}; };
inline Vec operator-(Vec a, Vec b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline float Dot(Vec a, Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
struct Frame { Vec center{}, right{1,0,0}, up{0,1,0}, normal{0,0,1}; float radius{.18f}; };
struct Error { float radial{}, axial{}; };
inline Error Measure(const Frame& f, Vec p) {
    const auto d=p-f.center;
    return {std::hypot(Dot(d,f.right),Dot(d,f.up))-f.radius,Dot(d,f.normal)};
}
inline float Distance(Error e) {
    // Euclidean distance to the circle, on either side of its rim/plane.
    // Do not exempt the interior between an off-rim initial grip and the rim.
    return std::hypot(e.radial,e.axial);
}
struct State {
    bool seeded{};
    float amplitude{}, error{};
    std::uint64_t epoch{};
    // Tracking loss silences the cue; recovery never learns drift as a new rim.
    float Step(const Frame& f, Vec p, bool held, bool tracked, bool newGrip,
               std::uint64_t trackingEpoch, float dt) {
        if (!held) { *this={}; return 0; }
        const auto e=Measure(f,p);
        if (!tracked || !std::isfinite(e.radial) || !std::isfinite(e.axial) ||
            !std::isfinite(f.radius) || f.radius<=.01f) { amplitude=0; return 0; }
        if (!seeded || newGrip || epoch!=trackingEpoch) {
            seeded=true; epoch=trackingEpoch; amplitude=0; error=0; return 0;
        }
        error=Distance(e);
        // Quest tuning: enabled, zero free zone, 100% of the existing 0.45 cap.
        const float x=std::clamp(error/.35f,0.f,1.f);
        const float target=.45f*x*x*(3.f-2.f*x);
        if (error<=0) return amplitude=0;
        const float tau=target>amplitude?.10f:.035f;
        amplitude+=(target-amplitude)*(1.f-std::exp(-std::clamp(dt,0.f,.1f)/tau));
        return amplitude;
    }
};
} // namespace savr::wheelhaptics
