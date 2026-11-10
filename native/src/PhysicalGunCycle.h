#pragma once

#include <algorithm>
#include <cstdint>

namespace savr::vrfire {

// Retail CWeapon fields; total includes the loaded magazine. Each inventory
// slot owns its clock, including during a hand transfer or holster operation.
inline void AdvancePhysicalGun(std::uint32_t& state, std::uint32_t& clip,
                               std::uint32_t total, std::uint32_t deadline,
                               std::uint32_t now, std::uint32_t capacity) {
    clip = std::min({clip, total, capacity});
    if ((state == 1u || state == 2u) &&
        static_cast<std::int32_t>(now - deadline) > 0) {
        if (state == 2u) clip = std::min(total, capacity);
        state = total ? 0u : 3u;
    }
}

inline unsigned PhysicalMagazineCapacity(int nativeCapacity, unsigned flags) {
    // WEAPON_TWIN_PISTOL: skill data contains BOTH native pistols' magazines.
    const int divisor = (flags & 0x800u) ? 2 : 1;
    return static_cast<unsigned>(std::max(1, nativeCapacity / divisor));
}

} // namespace savr::vrfire
