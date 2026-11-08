#pragma once
#include "VehicleDoorMotion.h"
namespace savr::xr { struct HandPose; }
namespace savr::ledgeclimb {
void Install(void* handle);
void Update(bool usable,const xr::HandPose hands[2],std::uint64_t epoch,unsigned occupied);
unsigned ClaimedHands();
void Append(vehicledoors::MarkerFrame& frame);
void ApplyHandVisuals(xr::HandPose hands[2]);
}
