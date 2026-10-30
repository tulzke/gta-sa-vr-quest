#pragma once
#include <cstdint>
namespace savr::xr { struct HandPose; }
namespace savr::npcgrab {
void Install(void* handle);
void Update(bool blocked,const xr::HandPose hands[2],bool trackingValid,std::uint64_t epoch);
void Reset();
unsigned ClaimedHands();
void PublishMarkers();
void OnDoorOpening(void* vehicle);
// Empty-hand contact with this car belongs to the nearby NPC interaction.
bool BlocksVehicleInteraction(void* vehicle,int hand);
}
