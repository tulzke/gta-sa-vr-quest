#include "RecoilModel.h"
#include "WeaponCalibrationMenu.h"
#include <cassert>
#include <array>

using namespace savr::recoil;
bool Near(float a,float b,float e=1e-5f){return std::abs(a-b)<e;}
struct Pose {float position[3]{},right[3]{1,0,0},forward[3]{0,1,0},up[3]{0,0,1};};
float Det(const Pose& p){return Dot(Cross(Read(p.right),Read(p.forward)),Read(p.up));}
int main(){
    // User-verified correction: five times the original angular response,
    // keeping the deliberately subtle backward translation unchanged.
    const auto pistol=ForWeapon(22,0,false,2);
    assert(Near(pistol.kick,8.f*kRad));
    assert(Near(pistol.limit,35.f*kRad));
    assert(Near(pistol.sideways,1.9f*kRad));
    assert(Near(pistol.back,.002f));
    for(int type=0;type<47;++type){
        const auto p=ForWeapon(type,0,false,2);
        assert((p.kick>0)==Supported(type));
        assert(ForWeapon(type,0,false,0).kick==0);
        if(!Supported(type))continue;
        const auto skilled=ForWeapon(type,1,false,2),supported=ForWeapon(type,0,true,2);
        assert(skilled.kick<p.kick && skilled.sideways<p.sideways);
        assert(supported.kick<p.kick*.5f && supported.limit<p.limit*.5f);
        State s;s.Shot(p);
        assert(s.sample.pitch==0 && s.sample.yaw==0 && s.sample.rear==0);
        s.Advance(1.f/90);
        assert(s.sample.pitch>0 && s.sample.rear>0);
        for(int i=0;i<200;++i)s.Advance(1.f/90);
        assert(s.sample.pitch==0 && s.sample.yaw==0 && s.sample.rear==0);
    }
    // Exact spring integration and time-based jitter agree across display rates.
    const auto p=ForWeapon(30,.5f,true,2);
    Sample reference{};
    for(int rate:{36,72,90,120}){
        State s;s.Shot(p);
        for(int i=0;i<rate/6;++i)s.Advance(1.f/rate);
        if(rate==36)reference=s.sample;
        assert(Near(s.sample.pitch,reference.pitch));
        assert(Near(s.sample.yaw,reference.yaw));
        assert(Near(s.sample.rear,reference.rear));
    }
    // Stress a long burst against the soft ceiling. Motion must remain alive
    // at saturation, without allowing the jitter to exceed the hard envelope.
    State burst;float lo=100,hi=0;
    for(int i=0;i<4000;++i){
        burst.Shot(p);burst.Advance(1.f/180);
        assert(std::isfinite(burst.sample.pitch));
        assert(burst.sample.pitch>=0 && burst.sample.pitch<=p.limit);
        assert(std::abs(burst.sample.yaw)<=p.sideways);
        assert(burst.sample.rear>=0 && burst.sample.rear<=p.backLimit);
        if(i>3000){lo=std::min(lo,burst.sample.pitch);hi=std::max(hi,burst.sample.pitch);}
    }
    assert(hi-lo>p.limit*.005f);
    // A grip change changes the following impulse and recovery, not the pose
    // being used for the current shot. Same rule for live skill changes.
    const Sample before=burst.sample;
    burst.Shot(ForWeapon(30,1,false,3));
    assert(burst.sample.pitch==before.pitch && burst.sample.rear==before.rear);
    burst.Advance(.3f);
    assert(burst.sample.pitch==0 && burst.sample.yaw==0 && burst.sample.rear==0);
    // Rigid motion carries the ray, both wrist contacts and reflected LEFT
    // basis together, without changing model scale or determinant parity.
    const float pivot[]{.2f,.3f,.4f};
    for(float parity:{-1.f,1.f}){
        Pose model;model.up[2]=parity;model.position[0]=.45f;
        const auto kick=Build({.08f,.01f,.003f},model.right,model.forward,pivot);
        const float determinant=Det(model);
        const Vec muzzle=Read(model.position)+Read(model.right)*.7f;
        const Vec expected=kick.Point(muzzle);
        kick.Apply(model);
        const Vec actual=Read(model.position)+Read(model.right)*.7f;
        assert(Near(actual.x,expected.x)&&Near(actual.y,expected.y)&&Near(actual.z,expected.z));
        assert(Near(Det(model),determinant));
        assert(Near(Dot(Read(model.right),Read(model.forward)),0));
        assert(model.right[1]>0); // pitch raises the barrel for either parity
        const auto copy=model;Transform{}.Apply(model);
        assert(Near(model.position[0],copy.position[0]));
    }
    assert(savr::weaponmenu::Field(savr::weaponmenu::EFFECTS,0)==41);
    assert(savr::weaponmenu::Field(savr::weaponmenu::EFFECTS,1)==42);
    assert(savr::weaponmenu::Field(savr::weaponmenu::EFFECTS,2)==36);
    assert(savr::weaponmenu::Field(savr::weaponmenu::EFFECTS,3)==37);
    assert(savr::weaponmenu::Field(savr::weaponmenu::EFFECTS,7)==43);
    assert(savr::weaponmenu::Field(savr::weaponmenu::EFFECTS,8)==22);
    assert(Defaults(25,0).disarmOneHanded && Defaults(25,2).disarmOneHanded);
    assert(!Defaults(22).disarmOneHanded && !Defaults(38).disarmOneHanded);
    // Each component can be disabled independently; no zero-envelope division
    // may turn shoulder-only or horizontal-only recoil into NaNs or silence.
    for(int component=0;component<3;++component){
        Tuning t{0,0,0,65};
        if(component==0)t.verticalDegrees=10;
        if(component==1)t.horizontalDegrees=2;
        if(component==2)t.shoulderMm=3;
        State s;s.Shot(ForWeapon(22,false,2,t));s.Advance(.02f);
        assert(std::isfinite(s.sample.pitch)&&std::isfinite(s.sample.yaw)&&std::isfinite(s.sample.rear));
        assert((s.sample.pitch>0)==(component==0));
        assert((std::abs(s.sample.yaw)>0)==(component==1));
        assert((s.sample.rear>0)==(component==2));
    }
    for(float reduction:{0.f,25.f,65.f,100.f}){
        const Tuning t{10,2,3,reduction};
        State one,two;
        one.Shot(ForWeapon(22,false,2,t));
        two.Shot(ForWeapon(22,true,2,t));
        one.Advance(.03f);two.Advance(.03f);
        const float retained=1.f-reduction*.01f;
        assert(Near(two.sample.pitch,one.sample.pitch*retained));
        assert(Near(two.sample.yaw,one.sample.yaw*retained));
        assert(Near(two.sample.rear,one.sample.rear*retained));
    }
    // Actual XR convention: -Z is a level barrel, +Y is physical up. The
    // model axis previously used as "top" is allowed to point LEFT/RIGHT
    // after calibration. Pure pitch must never turn into horizontal recoil.
    for(float yaw:{0.f,.7f,1.8f,-2.4f}){
        const Vec barrel{std::sin(yaw),0,-std::cos(yaw)};
        const Vec sideways{std::cos(yaw),0,std::sin(yaw)};
        float b[3],top[3];Write(b,barrel);
        for(float parity:{-1.f,1.f}){
            Write(top,sideways*parity);
            const auto kick=Build({.2f,0,.002f},b,top,pivot);
            const Vec raised=kick.Vector(barrel);
            assert(Near(raised.y,std::sin(.2f)));
            assert(Near(Dot(raised,sideways),0));
            assert(Near(kick.translation.y,0));
            assert(Near(Dot(kick.translation,barrel),-.002f));
            const auto jitter=Build({0,.02f,0},b,top,pivot);
            assert(Near(jitter.Vector(barrel).y,0));
            assert(std::abs(Dot(jitter.Vector(barrel),sideways))>.01f);
        }
    }
    // The ejection angle is derived from recoil pitch/yaw, not a constant
    // upward/backward velocity ratio. Zero pitch has no upward launch.
    const float barrel[]{0,0,-1},modelTop[]{1,0,0};
    for(float pitch:{0.f,.1f,.4f,.8f}){
        const auto launch=DisarmLaunch({pitch,0,0},barrel,modelTop,pivot);
        const Vec direction=Unit(launch.velocity);
        assert(Near(direction.y,std::sin(pitch)));
        assert(Near(direction.z,std::cos(pitch)));
        assert(Near(direction.x,0));
    }
    for(float yaw:{-.25f,.25f}){
        const auto launch=DisarmLaunch({0,yaw,0},barrel,modelTop,pivot);
        assert(Near(launch.velocity.y,0));
        assert(launch.velocity.x*yaw<0); // same signed deflection as the muzzle
        assert(launch.spinSpeed>0);
    }
    // Shoulder recoil adds backward momentum, not upward momentum. Increasing
    // it must flatten the ejection even with large pitch and horizontal jitter.
    Vec previous=DisarmLaunch({.8f,.2f,0},barrel,modelTop,pivot).velocity;
    for(float rear:{.05f,.2f}){
        const auto launch=DisarmLaunch({.8f,.2f,rear},barrel,modelTop,pivot);
        assert(launch.velocity.z>previous.z);
        assert(Near(launch.velocity.y,previous.y));
        assert(Near(launch.velocity.x,previous.x));
        assert(launch.velocity.z>std::abs(launch.velocity.y));
        assert(std::atan2(launch.velocity.y,launch.velocity.z)<std::atan2(previous.y,previous.z));
        previous=launch.velocity;
    }
    assert(Sanitize({10,2,200,65},25).shoulderMm==200);
    assert(Sanitize({10,2,900,65},25).shoulderMm==200);
}
