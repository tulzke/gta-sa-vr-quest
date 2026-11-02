#pragma once

namespace savr::doorless {
// Provisional seat positions in vehicle-local metres; calibrate in Quest.
struct Profile { int model; bool single; float halfWidth, seatX, seatY, seatZ; };
inline const Profile* Find(int model) {
    static constexpr Profile profiles[]={
        {424,false,.85f,.38f,.0f,.65f}, // BF Injection
        {531,true,1.0f,0.f,.0f,1.05f}, // Tractor
        {568,false,.90f,.38f,.0f,.65f}, // Bandito
        {571,true,.65f,0.f,.0f,.65f},  // Kart
    };
    for (const auto& profile:profiles) if (profile.model==model) return &profile;
    return nullptr;
}
}
