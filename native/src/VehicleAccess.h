#pragma once
#include <cstdint>
#include <cstring>

namespace savr::vehicleaccess {
// Provisional seat positions in vehicle-local metres; calibrate in Quest.
struct Profile { int model; bool single; float halfWidth, seatX, seatY, seatZ; };
inline const Profile* Find(int model) {
    static constexpr Profile profiles[]={
        {424,false,.85f,.38f,.0f,.65f}, // BF Injection
        {457,false,.75f,.32f,.0f,.75f}, // Caddy
        {485,true,.80f,0.f,.0f,.80f},   // Baggage
        {486,true,1.45f,0.f,.0f,1.45f}, // Dozer
        {530,true,.75f,0.f,.0f,.85f},   // Forklift
        {531,true,1.0f,0.f,.0f,1.05f}, // Tractor
        {568,false,.90f,.38f,.0f,.65f}, // Bandito
        {571,true,.65f,0.f,.0f,.65f},  // Kart
        {572,true,.60f,0.f,.0f,.65f},  // Mower
    };
    for (const auto& profile:profiles) if (profile.model==model) return &profile;
    return nullptr;
}
// Door index is the engine's eDoors value: 2..5, not a seat index.
inline bool DoorAbsent(const void* vehicle,int door) {
    if(!vehicle||door<2||door>5)return false;
    std::uint16_t model{};std::uint8_t status{};
    std::memcpy(&model,static_cast<const char*>(vehicle)+0x32,sizeof(model));
    if(Find(model))return true;
    std::memcpy(&status,static_cast<const char*>(vehicle)+0x758+9+door,sizeof(status));
    return status==4;
}
// The ordinary locked-door state cannot obstruct a physically missing door.
// Preserve every other native lock state, including player/script locks.
inline bool LockAllows(const void* vehicle,int door,bool nativeAllows) {
    if(nativeAllows)return true;
    if(!vehicle||!DoorAbsent(vehicle,door))return false;
    std::uint32_t lock{};
    std::memcpy(&lock,static_cast<const char*>(vehicle)+0x684,sizeof(lock));
    return lock==2;
}
}
