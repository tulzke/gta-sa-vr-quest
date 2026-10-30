#pragma once
#include <cstdint>
namespace savr::xr { struct HandPose; }
namespace savr::testarea {
// Engine mutations are confined to the game input thread. Menu calls queue work.
void Install(void* handle);
void RequestToggle();
void RequestPage(int direction);
const char* MenuLabel();
const char* PageLabel();
void Tick(bool gameplay, bool menuOpen);
// Returns true while navigation owns the trigger gesture (before door editing).
bool UpdateNavigation(bool blocked, const xr::HandPose hands[2],
                      const float head[3], const float orientation[4],
                      bool trackingValid, std::uint64_t epoch);
void PublishMarkers();
}
