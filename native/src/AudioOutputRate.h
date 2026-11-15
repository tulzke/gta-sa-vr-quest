#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace savr::audiodiag::outputrate {
#ifdef SAVR_EXPERIMENTAL_AUDIO_48KHZ
inline constexpr int kRequestedHz = 48000;
#else
inline constexpr int kRequestedHz = 22050;
#endif
// InitOpenALListener+0x24 constructs int attributes[]{ALC_FREQUENCY,22050,0}
// in x8 and stores it to the stack. Verified against 2.11.311 arm64 retail.
inline constexpr std::size_t kAttributeOffset = 0x24;
inline constexpr std::array<std::uint32_t, 4> kRetailAttributes{
    0xd28200e8u, 0xf2cac448u, 0xb90023ffu, 0xf9000fe8u};
// MOVK X8,#48000,LSL #32. Changes only the ALC_FREQUENCY value.
inline constexpr std::uint32_t k48kInstruction = 0xf2c00008u | (48000u << 5u);
inline bool Matches(const std::uint32_t* words) {
    if (!words) return false;
    for (std::size_t i = 0; i < kRetailAttributes.size(); ++i)
        if (words[i] != kRetailAttributes[i]) return false;
    return true;
}
}
