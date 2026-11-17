#pragma once
#include "VehicleDoorMotion.h"
namespace savr::xr { struct HandPose; }
namespace savr::ledgeclimb {
inline constexpr float kSafeFallHeight = 4.f;
void ProbeGrabs(const xr::HandPose hands[2]);
void Install(void* handle);
void Update(bool usable,const xr::HandPose hands[2],std::uint64_t epoch,unsigned occupied);
unsigned ClaimedHands();
// A native jump must not become skydive/parachute camera control mid-flight.
bool JumpInProgress();
void Append(vehicledoors::MarkerFrame& frame);
void ApplyHandVisuals(xr::HandPose hands[2]);
}
