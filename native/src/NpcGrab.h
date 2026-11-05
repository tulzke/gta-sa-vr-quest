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
bool BlocksNpcPunch(void* ped,int hand);
// World-space metres/second. Native fall task and pool-safe flight lifetime.
// aliveBeforeHit allows an impulse after this hit kills the NPC, without
// replacing death tasks; already-dead actors are otherwise excluded.
bool KnockdownFromPunch(void* ped,const float velocity[3],const char** reason=nullptr,
                        bool aliveBeforeHit=false);
void RecordJab(void* ped,const float direction[3],float speed);
void ResetJabCombo(void* ped);
}
