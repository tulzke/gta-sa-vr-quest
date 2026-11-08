#pragma once

namespace savr::scopeaim {

// GameThread state after physical ownership: photo-camera full-view viewfinder,
// world-space weapon-optic zoom, and the heat-seeker's real acquisition state.
// RPG and the country rifle/Mosin have no optic.
void Update(bool interactionsBlocked);
void Reset();

// VrFire enables sniper optics only after its exact FireSniper hook and trigger
// route are both installed. A visible reticle must never coexist with vanilla
// camera-derived sniper bullets.
void SetSniperFireAvailable(bool available);
void SetProjectileFireAvailable(bool available);

struct VisualState {
    bool active{};
    int hand{-1};
    int weaponType{-1};
    float zoom{1.0f};
};

VisualState Snapshot();
bool IsActive();
bool IsActiveFor(int hand, int weaponType);

// Weapon optics are surfaces in the world; they never activate the full-view
// photo-camera mode reported by Snapshot/IsActive.
float WeaponZoom(int weaponType);
bool ConsumesZoomInput();
struct HeatLockState {
    bool candidate{}, locked{};
    float progress{};
    float target[3]{};
};
HeatLockState HeatLockSnapshot();
// GameThread only. Validates pool slot + generation again before use.
void* HeatTarget(int hand);

// Full-view FOV adjustment for the photo camera only.
float RenderFovX(float baseFovXDegrees);

// Physical optics converge the muzzle to the optical axis at fixed 100 m zero.
// The photo camera retains its independent HMD-centre ray. All other weapons
// keep their physical barrel direction; no face/proximity gate changes firing.
bool ApplyReticleAim(int hand, int weaponType,
                     const float muzzleOrigin[3], float direction[3]);

} // namespace savr::scopeaim
