#include "VehicleClimbGeometry.h"
#include <cassert>
#include <cmath>
#include <limits>
using namespace savr::vehicleclimb;
int main() {
    Step s{{-1,-.4f,.2f},{-.5f,.4f,.4f}};
    assert(s.Valid() && s.Contains({-.7f,0,.3f}));
    assert(std::abs(s.Lift({-.7f,0,.3f})-.1f)<.0001f);
    assert(!s.Contains({-.4f,0,.3f}) && s.Lift({-.4f,0,.3f})==0);
    float lift{};
    assert(s.Contact({-.7f,0,.45f},{-.7f,0,.35f},lift)&&lift>.049f);
    assert(!s.Contact({-.7f,0,.45f},{.5f,0,.35f},lift));
    assert((!Step{{0,0,0},{0,1,1}}.Valid()));
    Pipe p{{0,0,0},{0,0,1}};
    assert(p.Valid() && std::abs(p.Distance({.2f,0,.5f})-.2f)<.0001f);
    assert(std::abs(p.Distance({0,0,1.2f})-.2f)<.0001f);
    assert((!Pipe{{0,0,0},{0,0,0}}.Valid()));
    Vec delta{};
    assert(PullDelta({0,0,0},{0,0,-.1f},.01f,delta) && delta.z>.099f);
    assert(!PullDelta({0,0,0},{1,0,0},.01f,delta));
    assert(!PullDelta({0,0,0},{0,0,0},.2f,delta));
    assert(!PullDelta({0,0,0},{std::numeric_limits<float>::quiet_NaN(),0,0},.01f,delta));
}
