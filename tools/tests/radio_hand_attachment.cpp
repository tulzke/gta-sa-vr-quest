#include "RadioHandAttachment.h"
#include <cassert>
#include <cmath>

using namespace savr::radio;
bool Near(HandVector a,HandVector b) {
    return std::abs(a.x-b.x)<1e-5f && std::abs(a.y-b.y)<1e-5f && std::abs(a.z-b.z)<1e-5f;
}
int main() {
    const HandFrame start{{1,2,3},{1,0,0},{0,1,0},{0,0,-1}};
    const HandPose tracked{{1.04f,2.03f,2.9f},{0,1,0},{-1,0,0},{0,0,-1}};
    HandAttachment hand;
    const auto initial=hand.Update(1,start,tracked);
    assert(Near(initial.position,tracked.position) && Near(initial.right,tracked.right));
    assert(Near(initial.up,tracked.up) && Near(initial.forward,tracked.forward));
    // Clockwise knob rotation and vehicle movement transform the entire pose.
    const HandFrame turned{{2,4,6},{0,-1,0},{1,0,0},{0,0,-1}};
    const HandPose movedTracking{{20,30,40},{1,0,0},{0,1,0},{0,0,1}};
    const auto rotated=hand.Update(1,turned,movedTracking);
    assert(Near(rotated.position,{2.03f,3.96f,5.9f}));
    assert(Near(rotated.right,{1,0,0}) && Near(rotated.up,{0,1,0}));
    assert(Near(rotated.forward,{0,0,-1}));
    // A fresh grab re-anchors immediately even if release was not rendered.
    const auto regrab=hand.Update(2,turned,movedTracking);
    assert(Near(regrab.position,movedTracking.position) && Near(regrab.right,movedTracking.right));
    hand.Reset();
    const auto afterLoss=hand.Update(2,turned,tracked);
    assert(Near(afterLoss.position,tracked.position) && Near(afterLoss.up,tracked.up));
}
