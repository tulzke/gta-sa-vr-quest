#pragma once
#include "VehicleDoorMotion.h"
namespace savr::vehicleexit {
void Install(void* handle);
bool EmergencyAvailable(void* car,int seat);
bool TryExit(void* car,int seat,vehicledoors::Vec hint,bool emergency=false);
}
