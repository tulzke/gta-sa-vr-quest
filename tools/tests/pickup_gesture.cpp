#include "PickupGesture.h"
#include "GrabArbitration.h"
#include "PickupVisual.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
using namespace savr;
using pickups::Gesture;

int main() {
    // Billboard faces the actual HMD position (not the tracking-space origin),
    // including pitch/roll, vertical views, near contact and marker scaling.
    using namespace vehicledoors;
    for(Vec head:std::array<Vec,5>{{{8,1,5},{5,-2,4},{5,2,1},{5,-2,1.02f},{5,-2,1}}})
    for(Vec headUp:std::array<Vec,3>{{{0,1,0},{-1,0,0},{0,0,1}}})
    for(float scale:{.5f,1.f,2.f}) {
        Marker marker{{5,-2,1},{0,1,0},{1,0,0}};marker.scale=scale;
        const Vec headRight=headUp.x<-.5f?Vec{0,1,0}:Vec{1,0,0};
        const auto hint=pickups::FacingHint(marker,head,headUp,headRight);
        const float distance=Length(head-marker.position);
        const Vec normal=Cross(hint.direction,hint.up)*(1.f/(scale*scale));
        assert(Finite(hint.position)&&Finite(normal));
        assert(std::abs(Length(hint.direction)-scale)<.0001f&&std::abs(Length(hint.up)-scale)<.0001f);
        assert(std::abs(Dot(hint.direction,hint.up))<.0001f);
        assert(Dot(hint.up,headUp)>=-.0001f);
        if(distance>.0001f) {
            assert(Dot(normal,(head-marker.position)*(1.f/distance))>.9999f);
            assert(std::abs(Length(hint.position-marker.position)-std::min(.06f,distance*.25f))<.0001f);
            assert(Length(head-hint.position)<distance); // closer, never crosses the HMD
        }
        assert(marker.position.x==5&&marker.position.y==-2&&marker.position.z==1);
    }
    // Held before contact, before tracking recovery, or before menu closure
    // must never turn into a pickup, even after a long wait inside the zone.
    for(int fps:{36,72,90,120}) {
        Gesture gesture;double t=1;const double dt=1./fps;
        for(int i=0;i<fps*3;++i)assert(!gesture.Step(7,1,true,1,t+=dt));
        gesture.Step(7,0,true,1,t+=dt);
        assert(!gesture.Step(7,1,true,1,t+=dt));
        assert(gesture.active);
        int completed=0;
        float previous=0,peak=0;
        for(int i=0;i<fps*3;++i){
            completed+=gesture.Step(7,1,true,1,t+=dt);
            const float amplitude=gesture.HapticAmplitude();
            assert(amplitude>=0&&amplitude<=.35f);
            if(gesture.active){assert(amplitude>=previous);previous=amplitude;peak=std::max(peak,amplitude);}
            else assert(amplitude==0);
        }
        assert(peak>.34f);
        assert(completed==1&&!gesture.active&&gesture.progress==0);
        gesture.Step(0,0,true,1,t+=dt);
        gesture.Step(0,1,true,1,t+=dt);
        for(int i=0;i<fps*2;++i)assert(!gesture.Step(7,1,true,1,t+=dt));
    }
    for(int interruption=0;interruption<6;++interruption) {
        Gesture g;double t=2;
        auto step=[&](std::uint64_t id,float grip=1,bool usable=true,std::uint64_t epoch=1) {
            return g.Step(id,grip,usable,epoch,t+=.05);
        };
        step(7,0);step(7,0);step(7);
        for(int i=0;i<10;++i)assert(!step(7));
        assert(g.progress>.4f&&g.progress<.6f);
        if(interruption==0)step(0);                    // hand leaves zone
        if(interruption==1)step(8);                    // nearer competing item
        if(interruption==2)step(7,1,false);            // menu/occupied hand
        if(interruption==3)step(7,1,true,2);           // recenter
        if(interruption==4){t+=2;step(7);}             // pause/stalled updates
        if(interruption==5)step(7,std::numeric_limits<float>::quiet_NaN());
        assert(g.HapticAmplitude()==0);
        for(int i=0;i<40;++i)assert(!step(7));
        assert(!g.active);
        step(7,0);step(7);
        int count=0;for(int i=0;i<25;++i)count+=step(7);
        assert(count==1);
    }
    // Hysteresis: partial relaxation does not restart or double-trigger.
    Gesture g;double t=1;g.Step(5,0,true,1,t);g.Step(5,0,true,1,t+=.01);
    g.Step(5,.64f,true,1,t+=.01);assert(!g.active);
    g.Step(5,.65f,true,1,t+=.01);assert(g.active);
    for(int i=0;i<9;++i){assert(!g.Step(5,.45f,true,1,t+=.1));}
    assert(g.progress>.89f&&g.progress<.91f);
    g.Step(5,.3f,true,1,t+=.01);assert(!g.active);

    // Probe order never assigns category priority. Only holsters override
    // the nearest surface, even when the holster itself is farther away.
    using namespace grabarbitration;
    std::array<int,4> order{0,1,2,3};
    const Candidate candidates[]={{Kind::Door,1,.12f},{Kind::Ledge,2,.04f},
        {Kind::Pickup,3,.10f},{Kind::Npc,4,.20f}};
    do {
        Selection choice;choice.Begin(true);
        for(int i:order)choice.Offer(0,candidates[i].kind,candidates[i].id,candidates[i].distance);
        assert(choice.Allows(0,Kind::Ledge));
        assert(!choice.Allows(0,Kind::Pickup));
        choice.Offer(0,Kind::Holster,9,.26f);assert(choice.Allows(0,Kind::Holster));
        choice.Offer(0,Kind::Pickup,7,.001f);assert(choice.Allows(0,Kind::Holster));
        choice.Offer(1,Kind::Pickup,6,.08f);assert(choice.Allows(1,Kind::Pickup));
    }while(std::next_permutation(order.begin(),order.end()));
    Selection choice;choice.Begin(true,1);
    choice.Offer(0,Kind::Pickup,7,.01f);assert(!choice.Allows(0,Kind::Pickup));
    choice.Offer(1,Kind::Pickup,8,.08f);choice.Offer(1,Kind::Pickup,7,.08f);
    assert(choice.hands[1].id==7);
    choice.Offer(1,Kind::Door,0,std::numeric_limits<float>::quiet_NaN());
    assert(choice.hands[1].id==7);
    choice.Begin(false);assert(choice.Allows(0,Kind::Door)); // cabin keeps its existing policy
    assert(pickups::CanWalkCollect(22,22));
    assert(!pickups::CanWalkCollect(23,22)); // same inventory slot is insufficient
    assert(!pickups::CanWalkCollect(22,0));
    assert(!pickups::CanWalkCollect(0,0));
}
