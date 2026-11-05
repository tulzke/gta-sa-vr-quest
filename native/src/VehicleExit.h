#pragma once
#include "VehicleDoorMotion.h"
namespace savr::vehicleexit {
void Install(void* handle);
enum class Mode { Unavailable, Normal, Emergency };
// The marker and the final action share the same native eligibility checks.
Mode Check(void* car,int seat,vehicledoors::Vec hint);
bool EmergencyAvailable(void* car,int seat);
bool TryExit(void* car,int seat,vehicledoors::Vec hint,bool emergency=false);
}
