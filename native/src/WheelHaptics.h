#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace savr::wheelhaptics {
// Persisted preset ids: keep the original 0=40 Hz and 1=75 Hz intact.
inline constexpr int kFrequencies[]{40,75,20,30,50,60,90,110,150,200};
inline constexpr int kFrequencyCount=sizeof(kFrequencies)/sizeof(kFrequencies[0]);
inline constexpr int kFrequencyOrder[]{2,3,0,4,5,1,6,7,8,9};
inline int Frequency(int preset) { return kFrequencies[std::clamp(preset,0,kFrequencyCount-1)]; }
inline int CycleFrequency(int preset,int direction) {
    for(int i=0;i<kFrequencyCount;++i)if(kFrequencyOrder[i]==preset)
        return kFrequencyOrder[(i+(direction<0?-1:1)+kFrequencyCount)%kFrequencyCount];
    return 0;
}
inline float RangeFrequency(float mix,int lowHz,int highHz) {
    const float low=static_cast<float>(std::clamp(std::min(lowHz,highHz),20,200));
    const float high=static_cast<float>(std::clamp(std::max(lowHz,highHz),20,200));
    return low+(high-low)*std::clamp(mix,0.f,1.f);
}
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
    float amplitude{}, error{}, frequencyMix{};
    std::uint64_t epoch{};
    // Tracking loss silences the cue; recovery never learns drift as a new rim.
    float Step(const Frame& f, Vec p, bool held, bool tracked, bool newGrip,
               std::uint64_t trackingEpoch, float dt, float freeZone, float strength) {
        if (!held) { *this={}; return 0; }
        const auto e=Measure(f,p);
        if (!tracked || !std::isfinite(e.radial) || !std::isfinite(e.axial) ||
            !std::isfinite(f.radius) || f.radius<=.01f) { amplitude=0; frequencyMix=0; return 0; }
        if (!seeded || newGrip || epoch!=trackingEpoch) {
            seeded=true; epoch=trackingEpoch; amplitude=0; error=0; frequencyMix=0; return 0;
        }
        error=Distance(e);
        const float zone=std::clamp(freeZone,0.f,.15f);
        const float x=std::clamp((error-zone)/.35f,0.f,1.f);
        const float target=std::clamp(strength,0.f,1.f)*.45f*x*x*(3.f-2.f*x);
        if (error<=zone) { frequencyMix=0; return amplitude=0; }
        // Frequency follows distance independently of the intensity preference.
        const float frequencyTau=x>frequencyMix?.10f:.035f;
        frequencyMix+=(x-frequencyMix)*(1.f-std::exp(-std::clamp(dt,0.f,.1f)/frequencyTau));
        const float tau=target>amplitude?.10f:.035f;
        amplitude+=(target-amplitude)*(1.f-std::exp(-std::clamp(dt,0.f,.1f)/tau));
        return amplitude;
    }
};
} // namespace savr::wheelhaptics
