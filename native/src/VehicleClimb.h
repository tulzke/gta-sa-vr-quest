#pragma once
#include "VehicleDoorMotion.h"
namespace savr::xr { struct HandPose; }
namespace savr::vehicleclimb {
void Install(void* handle);
void Update(bool usable,const xr::HandPose hands[2],std::uint64_t epoch,unsigned occupied);
void Append(vehicledoors::MarkerFrame& frame);
unsigned ClaimedHands();
bool Editing();
}
