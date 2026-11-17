#pragma once
#include <cstdint>

namespace savr::haptics {
enum class Source { None, Game, Wheel, Pickup };
struct Effect {
    Source source{Source::None};
    float amplitude{}, frequency{};
    double until{};
    std::uint64_t serial{};
    std::uint64_t epoch{};
    double pendingDuration{}; // game pulse starts at consumption, with a short publication TTL
};
struct Hand {
    Effect game{}, wheel{}, pickup{};
    Effect Select(double now, bool focused, bool menu) const {
        if (!focused || menu) return {};
        if (game.until>now && game.amplitude>0) return game;
        if (pickup.until>now && pickup.amplitude>0) return pickup;
        return wheel.until>now?wheel:Effect{};
    }
};
} // namespace savr::haptics
