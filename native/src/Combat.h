#pragma once
namespace savr::combat {
void Install(void* gameHandle);
void Update(bool blocked);
void Reset();
struct Feedback { float damage{}, block{}; };
// Atomically published events; read once for the pair of eyes.
Feedback GetFeedback();
}
