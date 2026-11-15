#pragma once

namespace savr::audiodiag {
// Configure before the game's first OpenAL device is opened.
void Configure();
// Optional test build: change the retail context request before audio starts.
void PrepareGameAudio(void* gameHandle);
// Called only on GameThread. Never changes AL state.
void Tick(bool focused, bool stereo, bool menu, bool paused);
}
