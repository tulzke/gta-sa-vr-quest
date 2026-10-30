#pragma once
#include <cstdint>
namespace savr::xr { struct HandPose; }
namespace savr::vehicledoors {
void Install(void* gameHandle);
// Called before weapon ownership, using that update's exact tracking snapshot.
void Update(bool blocked,const xr::HandPose hands[2],bool trackingValid,std::uint64_t epoch);
void Reset();
// Rebase world hints after the centre camera has advanced for this stereo pair.
void PublishMarkers();
unsigned int ClaimedHands();
bool DebugEditActive();
} // namespace savr::vehicledoors
