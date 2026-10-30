#include "TheaterScreen.h"
#include "FrontendPointer.h"
#include <cassert>
#include <limits>
#include <cstdio>
bool Near(float a,float b) { return std::abs(a-b)<0.0001f; }
int main() {
    savr::theater::Screen s;
    // A translated player looking right and down: height remains eye height,
    // horizontal distance remains 2.2 m despite the shortened forward vector.
    assert(s.Update(true,true,1,1,10,1.7f,20,0.5f,0));
    assert(Near(s.x,12.2f) && Near(s.y,1.7f) && Near(s.z,20));
    float u{},v{};
    assert(s.Hit(10,1.7f,20,1,0,0,u,v) && Near(u,.5f) && Near(v,.5f));
    s.height=s.width/(640.0f/448.0f);
    assert(s.Hit(10,1.7f+s.height*.49f,20+s.width*.49f,1,0,0,u,v));
    assert(Near(u,.99f) && Near(v,.01f));
    assert(!s.Hit(10,1.7f+s.height*.51f,20,1,0,0,u,v));
    assert(!s.Hit(10,1.7f,20,-1,0,0,u,v));
    assert(!s.Hit(10,1.7f,20,0,0,1,u,v));
    // Head movement during the same opening cannot drag the screen.
    assert(s.Update(true,true,1,1,-20,4,9,0,-1));
    assert(Near(s.x,12.2f) && Near(s.y,1.7f));
    // A new flat opening must capture even without an intervening stereo frame.
    assert(s.Update(true,true,2,1,-20,4,9,0,-1));
    assert(Near(s.x,-20) && Near(s.y,4) && Near(s.z,6.8f));
    assert(!s.Update(false,true,2,1,0,0,0,0,-1));
    assert(!s.Hit(-20,4,9,0,0,-1,u,v));
    assert(s.Update(true,true,2,2,1,2,3,0,-1));
    assert(Near(s.x,1) && Near(s.y,2));
    assert(!s.Update(true,false,2,2,1,2,3,0,-1));
    assert(!s.Update(true,true,3,2,1,2,3,0,0));
    assert(!s.Update(true,true,3,2,1,2,3,std::numeric_limits<float>::quiet_NaN(),0));
    assert(s.Update(true,true,3,2,1,2,3,0,-1));
    // Changing the screen blocks/cancels a drag; a held press cannot restart it.
    savr::frontendpointer::State touch;
    using A=savr::frontendpointer::Action;
    assert(touch.Update(false,true,true,.5f,.5f,640,448).action==A::Down);
    assert(touch.Update(true,true,false,0,0,640,448).action==A::Cancel);
    assert(touch.Update(false,true,true,.5f,.5f,640,448).action==A::None);
    touch.Update(false,false,true,.5f,.5f,640,448);
    assert(touch.Update(false,true,true,.5f,.5f,640,448).action==A::Down);
    std::puts("PASS: level placement, fixed pose, flat transitions, recovery, aspect/edge hits and touch cancellation");
}
