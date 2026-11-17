#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace savr::haptics {
enum class Source { None, Game, Wheel, Pickup, Shot, WeaponClick, Flame, Holster };
enum class Envelope { Flat, Fade, Irregular };
struct Effect {
    Source source{Source::None};
    float amplitude{}, frequency{};
    double until{};
    std::uint64_t serial{};
    std::uint64_t epoch{};
    double pendingDuration{}; // game pulse starts at consumption, with a short publication TTL
    Envelope envelope{};
    double started{};

    bool Active(double now) const { return until>now && amplitude>0; }
    bool Streamed() const {
        return source==Source::Wheel || source==Source::Pickup || envelope!=Envelope::Flat;
    }
    float AmplitudeAt(double now) const {
        if (envelope==Envelope::Fade) {
            const float remaining=static_cast<float>(std::clamp((until-now)/(until-started),0.,1.));
            return amplitude*remaining*remaining;
        }
        if (envelope==Envelope::Irregular) {
            struct Segment { double end; float gain; };
            constexpr Segment segments[]{{.06f,.55f},{.18f,.83f},{.25f,.47f},
                {.39f,.71f},{.43f,.38f},{.54f,.64f},{.69f,.90f},{.79f,.48f}};
            const double phase=std::fmod(std::max(now-started,0.),segments[7].end);
            for (const auto& segment:segments)
                if (phase<segment.end) return amplitude*segment.gain;
        }
        return amplitude;
    }
};

struct ShotProfile { float amplitude{}; double seconds{}; Envelope envelope{}; };
inline ShotProfile ForShot(int type) {
    switch (type) {
    case 24: return {.95f,.068};                    // Desert Eagle
    case 25: case 26: case 27: return {1.f,.082};    // Shotguns
    case 22: case 23: return {.29f,.027};            // Pistols
    case 28: case 29: case 32: return {.28f,.021};   // SMGs
    case 30: case 31: return {.62f,.039};            // AK, M4
    case 38: return {.59f,.029};                    // Minigun
    case 33: case 34: return {.62f,.052};            // Rifles
    case 35: case 36: return {1.f,.28,Envelope::Fade};
    default: return {};
    }
}

struct HolsterPacer {
    int point{-1};
    double time{}, phase{};
    bool atCenter{};
    struct Pulse { float amplitude{}; double seconds{}; Envelope envelope{}; };

    Pulse Step(int candidate, float proximity, double now) {
        if (candidate!=point || now<time || now-time>.15) {
            *this={}; point=candidate; time=now;
        }
        const double elapsed=now-time;
        time=now;
        if (candidate<0) return {};
        if (proximity>=.90f && !atCenter) {
            atCenter=true; phase=0;
            return {.22f,.044,Envelope::Fade};
        }
        if (atCenter) {
            if (proximity>=.82f) return {};
            atCenter=false;
            phase=1; // Resume the clicks when the hand leaves the center.
        }
        phase+=elapsed*(2.+8.*proximity*proximity);
        if (phase<1.) return {};
        phase-=std::floor(phase);
        return {.10f+.06f*proximity,.018};
    }
};

struct Hand {
    Effect game{}, wheel{}, pickup{};
    Effect shot{}, click{}, flame{}, holster{}, holsterPulse{};
    int holsterPoint{-1};
    HolsterPacer holsterPacer{};
    Effect Select(double now, bool focused, bool menu) const {
        if (!focused || menu) return {};
        if (shot.Active(now)) return shot;
        if (click.Active(now)) return click;
        if (game.Active(now)) return game;
        if (flame.Active(now)) return flame;
        if (pickup.Active(now)) return pickup;
        if (wheel.Active(now)) return wheel;
        return holsterPulse.Active(now)?holsterPulse:Effect{};
    }
};
} // namespace savr::haptics
