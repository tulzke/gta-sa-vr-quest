#pragma once
#include "VehicleDoorMotion.h"
namespace savr::vehicleentry {
bool ConsumesMove();
void Install(void* handle);
void Update(bool usable, std::uint64_t epoch);
void Append(vehicledoors::MarkerFrame& frame);
}
