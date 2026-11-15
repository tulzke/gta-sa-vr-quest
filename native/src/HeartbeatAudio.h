#pragma once
namespace savr::heartbeataudio {
// GameThread only. Zero stops playback and rewinds the dedicated source.
void Update(float severity);
}
