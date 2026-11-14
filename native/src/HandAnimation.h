#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace savr::handanimation {

enum class Gesture { Grip, Pinch, Point, OpenPalm, Beckon, GripPistol,
                     GripPistolOffhand, Fist, GripPistolShoot,
                     GripDeagle, GripDeagleOffhand, GripRifle, GripShotgun,
                     GripShotgunOffhand, Count };
inline constexpr std::array<const char*, static_cast<int>(Gesture::Count)> kNames{
    "Grip", "Pinch", "Point", "OpenPalm", "Beckon", "GripPistol",
    "GripPistolOffhand", "Fist", "GripPistolShoot", "GripDeagle", "GripDeagleOffhand",
    "GripRifle", "GripShotgun", "GripShotgunOffhand"};

inline float Unit(float value) {
    return std::isfinite(value) ? std::clamp(value, 0.f, 1.f) : 0.f;
}

// Two paths meeting at the diagonal produce a continuous analog grip/trigger
// surface. Along either axis, use the authored animation, not endpoint lerp.
struct Selection {
    Gesture first{Gesture::Grip}, second{Gesture::Grip};
    float progress{}, blend{};
};
inline Selection FreeHand(float grip, float trigger) {
    grip=Unit(grip);trigger=Unit(trigger);
    if (grip>=trigger)
        return {Gesture::Grip,Gesture::Fist,grip,grip>0.f?trigger/grip:0.f};
    return {Gesture::Point,Gesture::Fist,trigger,grip/trigger};
}
// Ledge attachment weight, shared with the skeletal vertex shader.
inline float PalmLockWeight(float localX){
 float t=std::clamp((localX-.015f)/.05f,0.f,1.f);return t*t*(3.f-2.f*t);
}
inline bool Firearm(int weaponType) {
    return (weaponType>=22 && weaponType<=34) || weaponType==38;
}
inline bool Pistol(int weaponType) { return weaponType>=22 && weaponType<=24; }
inline bool Launcher(int weaponType) { return weaponType==35 || weaponType==36; }
inline bool Shotgun(int weaponType) { return weaponType>=25 && weaponType<=27; }
inline bool RifleGripWeapon(int weaponType) { return weaponType==33 || weaponType==34; }
inline bool ShotgunOffhandWeapon(int weaponType) {
    return Shotgun(weaponType) || (weaponType>=29 && weaponType<=31) ||
        RifleGripWeapon(weaponType); // shotguns, MP5, AK, M4, both rifles
}
inline bool PistolStyleSupport(int weaponType) {
    return Pistol(weaponType) || weaponType==28 || weaponType==32; // Micro Uzi / Tec-9
}

struct Context {
    bool climb{}, pinchGrab{}, steering{}, ball{}, support{}, holdsWeapon{};
    int weaponType{};
};
inline Selection Select(float grip, float trigger, const Context& context) {
    const auto fixed=[](Gesture gesture,float progress=1.f) {
        return Selection{gesture,gesture,Unit(progress),0.f};
    };
    if (context.climb) return fixed(Gesture::Grip);
    if (context.pinchGrab) return fixed(Gesture::Pinch);
    if (context.steering) return fixed(Gesture::Grip,grip);
    if (context.ball) return fixed(Gesture::OpenPalm);
    if (context.support && Launcher(context.weaponType))
        return fixed(Gesture::GripPistolShoot,trigger);
    if (context.support && ShotgunOffhandWeapon(context.weaponType))
        return fixed(Gesture::GripShotgunOffhand,grip);
    if (context.support)
        return fixed(context.weaponType==24 ? Gesture::GripDeagleOffhand :
            PistolStyleSupport(context.weaponType) ? Gesture::GripPistolOffhand : Gesture::Grip,grip);
    if (context.holdsWeapon) {
        if (Launcher(context.weaponType)) return fixed(Gesture::GripPistol);
        if (context.weaponType==24) return fixed(Gesture::GripDeagle);
        if (context.weaponType==25 || context.weaponType==26)
            return fixed(Gesture::GripShotgun); // SPAS-12 keeps its pistol grip
        if (RifleGripWeapon(context.weaponType)) return fixed(Gesture::GripRifle);
        if (Firearm(context.weaponType)) return fixed(Gesture::GripPistolShoot,trigger);
        return fixed(Gesture::Grip);
    }
    return FreeHand(grip,trigger);
}

} // namespace savr::handanimation
