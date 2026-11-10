#pragma once
#include "RecoilModel.h"

namespace savr::recoil {
// GameThread once, before scope/fire queries. Shot only changes spring velocity;
// every consumer in this game frame sees the same published sample.
void Update(bool blocked);
void OnShot(int hand,int weaponType,int skill);
Sample Snapshot(int hand,int weaponType);
// Called when physical ownership or tracking history is invalidated. Does not
// call back into PhysicalWeapon (safe while its state mutex is held).
void Invalidate(int hand);
}
