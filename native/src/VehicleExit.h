#pragma once
#include "VehicleDoorMotion.h"
namespace savr::vehicleexit {
void Install(void* handle);
bool TryExit(void* car,int seat,vehicledoors::Vec hint);
}
