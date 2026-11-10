#pragma once
#include "RecoilModel.h"

namespace savr::recoil {
// GameThread once, before scope/fire queries. Shot only changes spring velocity;
// every consumer in this game frame sees the same published sample.
void Update(bool blocked);
void OnShot(int hand,int weaponType,int skill);
// Complete queued disarms after native CWeapon::Fire has returned. Never change
// the active slot while the engine is still finishing ammo/state for that shot.
void FinishShots();
Sample Snapshot(int hand,int weaponType);
// Shoulder impulse retained by the visual wrist after a recoil disarm. The
// weapon follows its own flying trajectory; the input/controller pose is intact.
Vec ReleasedHandOffset(int hand);
// Called when physical ownership or tracking history is invalidated. Does not
// call back into PhysicalWeapon (safe while its state mutex is held).
void Invalidate(int hand);
}
