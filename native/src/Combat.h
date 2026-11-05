#pragma once
namespace savr::combat {
void Install(void* gameHandle);
void Update(bool blocked);
void Reset();
bool IsBlocking();
// Limit on-foot stick magnitude while guarding; sprint is separately muted.
void LimitMovement(float* x,float* y);
// Suppress native melee knockback only while generating this physical hit.
class PunchImpactScope {
    void* previous{};
public:
    explicit PunchImpactScope(void* victim);
    ~PunchImpactScope();
    PunchImpactScope(const PunchImpactScope&)=delete;
    PunchImpactScope& operator=(const PunchImpactScope&)=delete;
};
struct Feedback { float damage{}, block{}; };
// Atomically published events; read once for the pair of eyes.
Feedback GetFeedback();
}
