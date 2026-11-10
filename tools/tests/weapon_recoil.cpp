#include "RecoilModel.h"
#include "WeaponCalibrationMenu.h"
#include <cassert>
#include <array>

using namespace savr::recoil;
bool Near(float a,float b,float e=1e-5f){return std::abs(a-b)<e;}
struct Pose {float position[3]{},right[3]{1,0,0},forward[3]{0,1,0},up[3]{0,0,1};};
float Det(const Pose& p){return Dot(Cross(Read(p.right),Read(p.forward)),Read(p.up));}
int main(){
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
    assert(savr::weaponmenu::Field(savr::weaponmenu::EFFECTS,0)==36);
    assert(savr::weaponmenu::Field(savr::weaponmenu::EFFECTS,1)==22);
}
