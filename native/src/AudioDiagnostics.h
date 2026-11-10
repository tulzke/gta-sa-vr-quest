#pragma once

namespace savr::audiodiag {
// Configure before the game's first OpenAL device is opened.
void Configure();
// Called only on GameThread. Never changes AL state.
void Tick(bool focused, bool stereo, bool menu, bool paused);
}
