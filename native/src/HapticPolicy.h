#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace savr::haptics {
enum class Source { None, Game, Wheel, Test, Pickup };
struct Effect {
    Source source{Source::None};
    float amplitude{}, frequency{};
    double until{};
    bool pcm{};
    int wave{}; // 0 asymmetric, 1 polarity reversed, 2 symmetric control
    std::uint64_t serial{};
    std::uint64_t epoch{};
    double pendingDuration{}; // game pulse starts at consumption, with a short publication TTL
};
struct Hand {
    Effect game{}, wheel{}, test{}, pickup{};
    Effect Select(double now, bool focused, bool menu) const {
        if (!focused) return {};
        if (menu) return test.until>now?test:Effect{};
        if (game.until>now && game.amplitude>0) return game;
        if (pickup.until>now && pickup.amplitude>0) return pickup;
        return wheel.until>now?wheel:Effect{};
    }
};
inline bool PcmUsable(bool extension, bool function, float rate) {
    return extension && function && std::isfinite(rate) && rate>=40 && rate<=192000;
}
// Keep waveform phase continuous across changing frequencies. Multiplying the
// absolute clock by a changing frequency would introduce arbitrary phase jumps.
struct PhaseClock {
    double phase{}, started{};
    float frequency{};
    bool active{};
    double Start(double now,float hz,bool restart) {
        const double dt=now-started;
        if(!active||restart||dt<0||dt>.2) phase=0;
        else phase=std::fmod(phase+dt*frequency,1.0);
        active=true; started=now; frequency=hz;
        return phase;
    }
};
// Candidate input waveforms, NOT calibrated acceleration or a known spatial pull.
// Positive/negative candidates have identical RMS; control is RMS matched.
inline float Sample(double phase, int wave) {
    constexpr double pi=3.14159265358979323846;
    const double a=phase*2*pi;
    const float signal=wave==2?static_cast<float>(std::sin(a)*std::sqrt(1.25)/1.5):
        static_cast<float>((std::sin(a)+.5*std::sin(2*a))/1.5);
    return wave==1?-signal:signal;
}
inline std::size_t Buffer(float* dst, std::size_t capacity, float rate,
                          float frequency, float amplitude, int wave, double phase) {
    if (!PcmUsable(true,true,rate) || frequency<=0 || frequency*4>rate) return 0;
    // Whole cycles (at least 50 ms) allow a bounded replacement cadence.
    const double cycles=std::ceil(frequency*.05);
    const auto count=std::min(capacity,static_cast<std::size_t>(std::lround(rate*cycles/frequency)));
    for (std::size_t i=0;i<count;++i) dst[i]=std::clamp(amplitude,0.f,.45f)*
        Sample(phase+i*static_cast<double>(frequency)/rate,wave);
    return count;
}
} // namespace savr::haptics
