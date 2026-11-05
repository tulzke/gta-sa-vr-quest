#include "RadioKnobState.h"
#include <cassert>
#include <limits>

int main() {
    savr::radio::KnobState knob;
    assert(knob.Step(true, 350) == 0);
    assert(knob.Step(true, 12) == 1);
    assert(knob.Step(true, 25) == 0 && knob.angle == 30);
    assert(knob.Step(true, 25) == 0);
    assert(knob.Step(true, 3) == 0);
    assert(knob.Step(true, 27) == 1);
    assert(knob.Step(false, 27) == 0 && knob.angle == 0);
    assert(knob.Step(true, 20) == 0);
    assert(knob.Step(true, -2) == -1);
    assert(knob.Step(true, std::numeric_limits<float>::quiet_NaN()) == 0);
    assert(!knob.held && knob.angle == 0);
}
