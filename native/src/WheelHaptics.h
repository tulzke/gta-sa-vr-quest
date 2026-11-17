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
inline float Distance(Error e, Error seed) {
    const float length2=seed.radial*seed.radial+seed.axial*seed.axial;
    const float t=length2>1e-8f?std::clamp(
        (e.radial*seed.radial+e.axial*seed.axial)/length2,0.f,1.f):0.f;
    return std::hypot(e.radial-t*seed.radial,e.axial-t*seed.axial);
}
struct State {
    bool seeded{};
    Error seed{};
    float amplitude{}, error{};
    std::uint64_t epoch{};
    // Tracking loss silences the cue but preserves a held grip's baseline.
    float Step(const Frame& f, Vec p, bool held, bool tracked, bool newGrip,
               std::uint64_t trackingEpoch, float dt, float freeZone, float strength) {
        if (!held) { *this={}; return 0; }
        const auto e=Measure(f,p);
        if (!tracked || !std::isfinite(e.radial) || !std::isfinite(e.axial) ||
            !std::isfinite(f.radius) || f.radius<=.01f) { amplitude=0; return 0; }
        if (!seeded || newGrip || epoch!=trackingEpoch) {
            seed=e; seeded=true; epoch=trackingEpoch; amplitude=0; error=0; return 0;
        }
        error=Distance(e,seed);
        const float zone=std::clamp(freeZone,.10f,.15f);
        const float x=std::clamp((error-zone)/.35f,0.f,1.f);
        const float target=std::clamp(strength,0.f,1.f)*.45f*x*x*(3.f-2.f*x);
        if (error<=zone) return amplitude=0;
        const float tau=target>amplitude?.10f:.035f;
        amplitude+=(target-amplitude)*(1.f-std::exp(-std::clamp(dt,0.f,.1f)/tau));
        return amplitude;
    }
};
} // namespace savr::wheelhaptics
