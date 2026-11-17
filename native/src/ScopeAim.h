#pragma once

namespace savr::scopeaim {

// GameThread state after physical ownership: camera and weapon-optic zoom
// and the heat-seeker's real acquisition state.
// RPG and the country rifle/Mosin have no optic.
void Update(bool interactionsBlocked);
void Reset();

// VrFire enables sniper optics only after its exact FireSniper hook and trigger
// route are both installed. A visible reticle must never coexist with vanilla
// camera-derived sniper bullets.
void SetSniperFireAvailable(bool available);
void SetProjectileFireAvailable(bool available);

float WeaponZoom(int weaponType);
int PhotoHand();
bool ConsumesZoomInput();
struct HeatLockState {
    bool candidate{}, locked{};
    float progress{};
    float target[3]{};
};
HeatLockState HeatLockSnapshot();
// GameThread only. Validates pool slot + generation again before use.
void* HeatTarget(int hand);

// Horizontal FOV of the world-space camera viewfinder and shutter.
float PhotoFovX(float baseFovXDegrees);

// Physical optics converge the muzzle to the optical axis at fixed 100 m zero.
// Other weapons keep their physical barrel direction.
bool ApplyReticleAim(int hand, int weaponType,
                     const float muzzleOrigin[3], float direction[3]);

} // namespace savr::scopeaim
