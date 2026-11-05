#include "VehicleClimbGeometry.h"
#include <cassert>
#include <cmath>
#include <limits>
using namespace savr::vehicleclimb;
int main() {
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
