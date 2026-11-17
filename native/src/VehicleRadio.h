#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace savr::radio {
constexpr std::size_t kVehicleAudioOffset=0x198;
constexpr std::size_t kAudioSettingsOffset=0x94;
constexpr std::size_t kRadioTypeOffset=0x1b;
// Android 2.11.311: civilian music radio is independent of the OFF station.
inline bool HasTunableRadio(unsigned type) { return type==0; }
inline const void* VehicleRadioSettings(const void* vehicle) {
    return vehicle?static_cast<const char*>(vehicle)+kVehicleAudioOffset+kAudioSettingsOffset:nullptr;
}
// Call only after VehicleRadioLayoutMatches succeeds, on a live CVehicle.
inline bool VehicleHasTunableRadio(const void* vehicle) {
    const auto* settings=static_cast<const std::uint8_t*>(VehicleRadioSettings(vehicle));
    return settings && HasTunableRadio(settings[kRadioTypeOffset]);
}
struct VehicleRadioAbiWord { std::size_t rva; std::uint32_t word; };
constexpr VehicleRadioAbiWord kVehicleRadioAbiWords[]{
    {0x6d8b24,0x91066260u}, // CVehicle ctor: audio at this+198
    {0x3968cc,0x3c894000u}, // Initialise: settings copy at audio+94
    {0x3968d4,0x3c8a4000u}, // next 16 bytes at audio+a4
    {0x3968dc,0xb900b408u}, // last 4 bytes at audio+b4 (settings size 24)
    {0x3968e8,0x3942be68u}, // Initialise: type at audio+af
    {0x398774,0x91025001u}, // TurnOnRadioForVehicle: settings at audio+94
    {0x388aa0,0x39406c08u}, // CheckForStationRetune: type at settings+1b
    {0x388aa4,0x35000888u}, // nonzero type skips civilian station input
};
inline bool VehicleRadioLayoutMatches(const void* image) {
    if(!image)return false;
    for(const auto& check:kVehicleRadioAbiWords) {
        std::uint32_t word{};
        std::memcpy(&word,static_cast<const char*>(image)+check.rva,sizeof(word));
        if(word!=check.word)return false;
    }
    return true;
}
}
