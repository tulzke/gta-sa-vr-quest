#include "HandAnimation.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace savr::handanimation;
bool Near(float a,float b) {return std::abs(a-b)<1e-6f;}
int main() {
    const auto zero=FreeHand(0,0);
    assert(zero.first==Gesture::Grip && zero.progress==0 && zero.blend==0);
    const auto grip=FreeHand(1,0);
    assert(grip.first==Gesture::Grip && grip.progress==1 && grip.blend==0);
    const auto point=FreeHand(0,1);
    assert(point.first==Gesture::Point && point.progress==1 && point.blend==0);
    const auto fist=FreeHand(1,1);
    assert(fist.second==Gesture::Fist && fist.progress==1 && fist.blend==1);
    const auto middle=FreeHand(.4f,.4f);
    assert(Near(middle.progress,.4f) && middle.blend==1);
    assert(FreeHand(std::numeric_limits<float>::quiet_NaN(),0).progress==0);
    for (int type=0;type<48;++type) {
        const bool firearm=(type>=22 && type<=34)||type==38;
        assert(Firearm(type)==firearm);
        const auto selected=Select(1,.7f,{false,false,false,false,false,true,type});
        assert(selected.first==(type==24?Gesture::GripDeagle:
            (type==35||type==36)?Gesture::GripPistol:
            (type==25||type==26)?Gesture::GripShotgun:
            RifleGripWeapon(type)?Gesture::GripRifle:
            firearm?Gesture::GripPistolShoot:Gesture::Grip));
        if(type==25||type==26||RifleGripWeapon(type)) {
            assert(selected.progress==1.f);
            assert(Select(1,0,{false,false,false,false,false,true,type}).first==selected.first);
        }
    }
    Context context;
    context.holdsWeapon=true;context.weaponType=22;
    assert(Select(1,.5f,context).first==Gesture::GripPistolShoot);
    assert(Near(Select(1,.5f,context).progress,.5f));
    context.weaponType=8; // Katana keeps the ordinary grip.
    assert(Select(1,1,context).first==Gesture::Grip);
    context.support=true;context.weaponType=22;
    assert(Select(1,1,context).first==Gesture::GripPistolOffhand);
    for (int type=0;type<48;++type) {
        context.weaponType=type;
        const auto support=Select(.4f,1,context);
        const bool foregrip=(type>=25&&type<=27)||(type>=29&&type<=31)||type==33||type==34;
        assert(support.first==((type==35||type==36)?Gesture::GripPistolShoot:
            type==24?Gesture::GripDeagleOffhand:
            foregrip?Gesture::GripShotgunOffhand:
            PistolStyleSupport(type)?Gesture::GripPistolOffhand:Gesture::Grip));
        assert(Near(support.progress,Launcher(type)?1.f:.4f));
        if(Launcher(type)) {
            assert(Select(1,0,context).progress==0.f);
            assert(Near(Select(1,.6f,context).progress,.6f));
        }
    }
    context.ball=true;
    assert(Select(1,1,context).first==Gesture::OpenPalm);
    context.steering=true;
    assert(Select(1,1,context).first==Gesture::Grip);
    context.pinchGrab=true;
    assert(Select(1,1,context).first==Gesture::Pinch);
    context.climb=true;
    assert(Select(1,1,context).first==Gesture::Grip);
    context={};context.horn=1.f;
    assert(Select(1,1,context).first==Gesture::OpenPalm);
    context.horn=.4f;
    assert(Near(Select(1,1,context).openPalmBlend,.4f));
    context.holdsWeapon=true;context.weaponType=22;
    assert(Select(1,1,context).first==Gesture::GripPistolShoot);
    assert(Select(1,1,context).openPalmBlend==0);
    context.climb=true;
    assert(Select(1,1,context).first==Gesture::Grip);
    std::puts("PASS gesture selection: analog grip/trigger and interaction priorities");
}
