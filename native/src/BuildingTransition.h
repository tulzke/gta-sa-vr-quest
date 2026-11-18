#pragma once

#include <cstdint>

namespace savr::buildingtransition {

constexpr std::uint16_t kControlLock = 0x8;

void Install(void* handle);

// Game thread. Keep the stereo camera live during the native entry/exit fade.
void Update();
bool Active();

// XR thread. Opacity of the black overlay applied to both eyes.
float Opacity();

}  // namespace savr::buildingtransition
