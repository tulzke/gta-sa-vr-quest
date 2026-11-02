#include "IgnitionState.h"
#include <cassert>
#include <limits>

using namespace savr::ignition;

int main() {
    assert(!ReleasedEngineOn(0));
    assert(!ReleasedEngineOn(59.99f));
    assert(ReleasedEngineOn(60));
    assert(ReleasedEngineOn(90));
    assert(ClampAngle(-10) == 0 && ClampAngle(100) == 90);
    assert(!ReleasedEngineOn(std::numeric_limits<float>::quiet_NaN()));

    HintState hint;
    assert(hint.Update(10, false, false) == Hint::None);
    assert(hint.Update(14.99, false, false) == Hint::None);
    assert(hint.Update(15, false, false) == Hint::PointToKey);
    assert(hint.Update(15.1, false, true) == Hint::TurnClockwise);
    assert(hint.Update(15.2, false, false) == Hint::PointToKey);
    assert(HintVisible(15, 10) && !HintVisible(15.5, 10));
    assert(HintVisible(16, 10));
    assert(hint.Update(16, true, true) == Hint::None);
    assert(hint.Update(30, false, false) == Hint::None);
    hint.Leave();
    assert(hint.Update(31, false, false) == Hint::None);
    assert(hint.Update(36, false, false) == Hint::PointToKey);
    hint.Leave();
    assert(hint.Update(40, true, false) == Hint::None);
    assert(hint.Update(50, false, false) == Hint::None);
}
