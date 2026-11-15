#pragma once

namespace savr::audiodiag {
// Configure before the game's first OpenAL device is opened.
void Configure();
// Request 48 kHz output before the game's audio context starts.
void PrepareGameAudio(void* gameHandle);
// Called only on GameThread. Never changes AL state.
void Tick(bool focused, bool stereo, bool menu, bool paused);
}
