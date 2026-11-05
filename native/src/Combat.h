#pragma once
namespace savr::combat {
void Install(void* gameHandle);
void Update(bool blocked);
void Reset();
bool IsBlocking();
// Limit on-foot stick magnitude while guarding; sprint is separately muted.
void LimitMovement(float* x,float* y);
struct Feedback { float damage{}, block{}; };
// Atomically published events; read once for the pair of eyes.
Feedback GetFeedback();
}
