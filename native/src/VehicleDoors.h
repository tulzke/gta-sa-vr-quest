#pragma once
#include <cstdint>
namespace savr::xr { struct HandPose; }
namespace savr::vehicledoors {
void Install(void* gameHandle);
bool QuadBarPosition(void* vehicle, float out[3]);
// Called before weapon ownership, using that update's exact tracking snapshot.
void Update(bool blocked,const xr::HandPose hands[2],bool trackingValid,std::uint64_t epoch);
void Reset();
// Rebase world hints after the centre camera has advanced for this stereo pair.
void PublishMarkers();
unsigned int ClaimedHands();
// Visual-only pinch for the hand dragging the ignition/radio debug marker.
unsigned int DebugPinchHands();
// Interior door surface distances in metres; unavailable hands receive infinity.
void InteriorDistances(void* vehicle,const xr::HandPose hands[2],float distances[2]);
bool DebugEditActive();
} // namespace savr::vehicledoors
