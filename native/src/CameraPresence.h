#pragma once

#include <cstdint>

namespace savr::camerapresence {

enum class View : std::uint8_t { Unavailable, Player, Scene };

// Advance the generation on each change, even when the camera switches away
// and back before OpenXR has consumed a new pair.
constexpr View ViewOf(std::uint64_t presentation) {
    return static_cast<View>(presentation & 3u);
}

constexpr std::uint64_t Update(std::uint64_t previous, View observed,
                               bool cameraCopied) {
    if (observed == ViewOf(previous) || (observed == View::Player && !cameraCopied))
        return previous;
    return (((previous >> 2) + 1) << 2) | static_cast<std::uint64_t>(observed);
}

} // namespace savr::camerapresence
