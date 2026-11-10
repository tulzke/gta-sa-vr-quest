#pragma once

namespace savr::vrfire {

// The physical-weapon layer owns holster/hand state. It must return true only
// when `weaponType` is currently held in either usable hand, and write that OpenXR
// hand (0 left, 1 right). A missing weapon or a type mismatch must return false.
using PhysicalFireQuery = bool (*)(int weaponType, int* firingHand);

void SetPhysicalFireQuery(PhysicalFireQuery query);

// Native attached-gunner state (bike/car/helicopter mission sequences).
// Ordinary on-foot rifle aiming does not activate this route.
bool IsScriptedWeaponCamera();

// Installs narrow hooks on player hitscan entry points. Non-player shots,
// vehicle shots, unheld weapons and invalid tracking all call the original game
// functions unchanged.
bool Install();

// GameThread owner for both physical guns, each with its own trigger and ray.
// Native CWeapon owns ammo/sound; per-slot clocks replace shared CJ reload tasks.
void Update(bool blocked);
// Synchronous native-shot context, -1 outside the two-hand firing loop.
int DirectFiringHand();

} // namespace savr::vrfire
