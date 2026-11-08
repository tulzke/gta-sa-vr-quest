#pragma once
#include <cstdint>

namespace savr::pdablip {
// Retail DrawCoordBlip 0x519fb4: contact points disappear during a mission.
// DrawCoordBlip 0x51a240 and DrawEntityBlip 0x51ad5c: sprite blips with
// short-range bit 2 do not become permanent edge indicators. Bit 4 is a
// separate zoom flag, not permission to ignore short-range visibility.
constexpr bool Suppressed(std::uint16_t flags, int type, bool hasSprite,
                          bool onMission, bool radarZoom) {
    return (type == 5 && onMission) ||
           (hasSprite && (flags & 4) && radarZoom && (type == 4 || type == 5));
}
constexpr bool PinToEdge(std::uint16_t flags, bool hasSprite) {
    return !hasSprite || !(flags & 4);
}

// Context changes on the same sprite must change its visibility; no sprite-ID
// whitelist is involved. These regressions are compiled by the Android build.
static_assert(!PinToEdge(4, true));
static_assert(!PinToEdge(4 | 0x10, true));
static_assert(PinToEdge(0, true));
static_assert(PinToEdge(4, false));
static_assert(Suppressed(0, 5, true, true, false));
static_assert(!Suppressed(0, 5, true, false, false));
static_assert(Suppressed(4, 4, true, false, true));
static_assert(!Suppressed(4, 4, true, false, false));
} // namespace savr::pdablip
