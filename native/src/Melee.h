#pragma once

namespace savr::melee {
void Install(void* gameHandle);

// Sample tracked hands once per GameThread input tick. A real fast swing is
// swept through GTA world collision and handed to SA's native damage endpoints.
void Update(bool interactionsBlocked);

// Clear velocity/cooldown history across menus, vehicles and lifecycle gaps.
void Reset();
// Same calibrated collision segment as attacks, for the defensive pose.
bool GuardSegment(int hand, float root[3], float tip[3], int* weaponType);

} // namespace savr::melee
