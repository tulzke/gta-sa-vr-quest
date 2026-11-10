#pragma once
#include <cmath>

namespace savr::menu {
// Numeric menu edits run on the input/game thread. Keep the modifier local to
// that thread; rendering and background settings readers never change it.
inline thread_local bool fineAdjustment = false;
inline float SpatialStep() { return fineAdjustment ? .1f : .5f; }
// Preserve each config's existing units while storing decimal values. Quantize
// in displayed cm/degrees so repeated +/- edits do not accumulate float drift.
inline float SpatialValue(float current, int direction, float displayPerRaw=1.f) {
    if (fineAdjustment) direction=(direction>0)-(direction<0);
    if (!std::isfinite(current)) current=0.f;
    return std::round((current*displayPerRaw+direction*SpatialStep())*10.f)
        / (10.f*displayPerRaw);
}
}
