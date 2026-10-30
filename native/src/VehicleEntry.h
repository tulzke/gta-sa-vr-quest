#pragma once
#include "VehicleDoorMotion.h"
namespace savr::vehicleentry {
bool ConsumesMove();
unsigned ClaimedHands();
int SeatIndex(void* car,void* ped);
void Install(void* handle);
void Update(bool usable, std::uint64_t epoch);
void Append(vehicledoors::MarkerFrame& frame);
}
