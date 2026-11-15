#pragma once

namespace savr::npcaim {
// Installs verified event adapters; native aiming outside ordinary VR stays native.
void Install(void* gameHandle);
// GameThread, after physical ownership and scope aim. Does not own NPC tasks.
void Update(bool blocked);
}
