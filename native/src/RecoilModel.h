#pragma once

#include <algorithm>
#include <cmath>

namespace savr::recoil {
constexpr float kRad = .01745329252f;
constexpr float kReturnOmega = 20.f;
inline bool Supported(int type) { return (type >= 22 && type <= 34) || type == 38; }

struct Vec {
    float x{},y{},z{};
    Vec operator+(Vec b)const{return{x+b.x,y+b.y,z+b.z};}
    Vec operator-(Vec b)const{return{x-b.x,y-b.y,z-b.z};}
    Vec operator*(float s)const{return{x*s,y*s,z*s};}
};
inline Vec Read(const float* p){return{p[0],p[1],p[2]};}
inline void Write(float* p,Vec v){p[0]=v.x;p[1]=v.y;p[2]=v.z;}
inline float Dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec Cross(Vec a,Vec b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float Length(Vec a){return std::sqrt(Dot(a,a));}
inline Vec Unit(Vec a){const float n=Length(a);return std::isfinite(n)&&n>1e-6f?a*(1.f/n):Vec{};}

struct Sample { float pitch{}, yaw{}, rear{}; Vec rearOffset{}; };
struct Profile {
    float kick{}, limit{}, sideways{}, back{}, backLimit{}, omega{kReturnOmega};
};
struct Tuning {
    float verticalDegrees{}, horizontalDegrees{1.9f}, shoulderMm{2.f}, supportReductionPercent{65.f};
    bool disarmOneHanded{};
};
#include "DefaultRecoilCalibration.inl"
struct Shape {float kick,limit;};
inline Shape WeaponShape(int type) {
    float kick = 1.6f, limit = 7.f;
    switch (type) {
    case 23: kick=1.3f; limit=6.f; break;
    case 24: kick=3.6f; limit=11.f; break;
    case 25: case 26: kick=4.2f; limit=12.f; break;
    case 27: kick=2.8f; limit=10.f; break;
    case 28: case 32: kick=.8f; limit=7.f; break;
    case 29: kick=.65f; limit=6.f; break;
    case 30: kick=1.15f; limit=8.f; break;
    case 31: kick=.9f; limit=7.f; break;
    case 33: case 34: kick=3.f; limit=10.f; break;
    case 38: kick=.12f; limit=3.f; break;
    }
    return {kick,limit};
}
inline Tuning Defaults(int type,int skill=0) {
    skill=std::clamp(skill,0,2);
    for(const auto& captured:kCapturedRecoil)
        if(captured.type==type && captured.skill==skill)return captured.tuning;
    const float mastery=1.f-.2f*std::clamp(skill,0,2);
    return {std::round(WeaponShape(type).kick*50.f*mastery)*.1f,
            std::round(19.f*mastery)*.1f,std::round(20.f*mastery)*.1f,65.f,type==25};
}
inline Tuning Sanitize(Tuning t,int type,int skill=0) {
    const Tuning fallback=Defaults(type,skill);
    const auto clamp=[](float v,float fallback,float max){
        return std::isfinite(v)?std::clamp(v,0.f,max):fallback;
    };
    return {clamp(t.verticalDegrees,fallback.verticalDegrees,45.f),
            clamp(t.horizontalDegrees,fallback.horizontalDegrees,20.f),
            clamp(t.shoulderMm,fallback.shoulderMm,200.f),
            clamp(t.supportReductionPercent,fallback.supportReductionPercent,100.f),t.disarmOneHanded};
}
inline Profile ForWeapon(int type, bool support, int level,Tuning tuning) {
    if (!Supported(type) || level <= 0) return {};
    tuning=Sanitize(tuning,type);
    const float scale = (level == 1 ? .6f : level == 2 ? 1.f : 1.4f);
    const float hold = support ? 1.f-tuning.supportReductionPercent*.01f : 1.f;
    // Skill is selected by the caller's profile, with no second hidden mastery
    // multiplier. The displayed values are the actual baseline for that tier.
    const float strength=hold*scale;
    const Shape shape=WeaponShape(type);
    return {tuning.verticalDegrees*kRad*strength,
            std::min(85.f,tuning.verticalDegrees*shape.limit/shape.kick)*kRad*strength,
            tuning.horizontalDegrees*kRad*strength,
            tuning.shoulderMm*.001f*strength,
            tuning.shoulderMm*.003f*strength,
            kReturnOmega}; // The configured percentage is the complete support modifier.
}
inline Profile ForWeapon(int type,float skill,bool support,int level) {
    skill=std::isfinite(skill)?std::clamp(skill,0.f,1.f):.5f;
    return ForWeapon(type,support,level,Defaults(type,static_cast<int>(std::round(skill*2))));
}

struct Spring {
    float x{}, v{};
    void Advance(float dt,float omega) {
        const float c=v+omega*x, decay=std::exp(-omega*dt);
        v=(v-omega*c*dt)*decay;
        x=(x+c*dt)*decay;
        if (x<0) { x=0; v=0; }
    }
};
struct State {
    Spring angle{};
    Vec rearPosition{}, rearVelocity{};
    Profile target{}, live{};
    float heat{}, phase{};
    bool initialized{}, directional{};
    Sample sample{}; // Published only by Advance, never by Shot or a reader.
    void Shot(Profile p, Vec shotDirection = {}) {
        if (p.kick<=0 && p.sideways<=0 && p.back<=0) return;
        if (!initialized) { live=p;initialized=true; }
        target=p;
        constexpr float e=2.718281828f;
        angle.v=std::min(angle.v+p.kick*p.omega*e, p.limit*p.omega*3.f);
        const Vec forward=Unit(shotDirection);
        directional=Dot(forward,forward)>.5f;
        const Vec backward=directional?forward*(-1.f):Vec{-1,0,0};
        rearVelocity=rearVelocity+backward*(p.back*p.omega*e);
        const float speed=Length(rearVelocity), limit=p.backLimit*p.omega*3.f;
        if (speed>limit) rearVelocity=rearVelocity*(limit/speed);
        heat=std::min(heat+.8f,1.f);
    }
    void Advance(float dt) {
        if (!std::isfinite(dt) || dt<0 || dt>.25f) { *this={}; return; }
        if (dt==0 || !initialized) return;
        const float blend=1.f-std::exp(-12.f*dt);
        live.limit+=(target.limit-live.limit)*blend;
        live.backLimit+=(target.backLimit-live.backLimit)*blend;
        live.sideways+=(target.sideways-live.sideways)*blend;
        live.omega+=(target.omega-live.omega)*blend;
        angle.Advance(dt,live.omega);
        const Vec c=rearVelocity+rearPosition*live.omega;
        const float decay=std::exp(-live.omega*dt);
        rearVelocity=(rearVelocity-c*(live.omega*dt))*decay;
        rearPosition=(rearPosition+c*dt)*decay;
        heat*=std::exp(-8.f*dt);
        phase=std::fmod(phase+dt,60.f);
        // Reserve headroom for jitter: the mean approaches a soft ceiling,
        // while the total signal stays strictly inside the weapon's limit.
        const float mean=live.limit>1e-8f ? .88f*live.limit*std::tanh(angle.x/(.88f*live.limit)):0.f;
        const float nearTop=live.limit>1e-8f ? std::clamp((mean/live.limit-.55f)/.25f,0.f,1.f):0.f;
        const float jitter=.06f*live.limit*heat*nearTop*
            (.65f*std::sin(phase*67.f)+.35f*std::sin(phase*101.f));
        sample.pitch=std::max(0.f,mean+jitter);
        sample.yaw=live.sideways*heat*(.6f*std::sin(phase*53.f)+
                                                .4f*std::sin(phase*89.f));
        const float displacement=Length(rearPosition);
        sample.rear=live.backLimit>1e-8f ? live.backLimit*std::tanh(displacement/live.backLimit):0.f;
        sample.rearOffset=directional&&displacement>0?rearPosition*(sample.rear/displacement):Vec{};
        if (heat<.001f && angle.x<.00001f && displacement<.000001f && Length(rearVelocity)<.00002f) *this={};
    }
};

inline Vec Rotate(Vec v,Vec axis,float angle){
    const float c=std::cos(angle),s=std::sin(angle);
    return v*c+Cross(axis,v)*s+axis*(Dot(axis,v)*(1.f-c));
}
// Rigid transform in OpenXR LOCAL. Explicit basis-vector rotation preserves
// the reflected LEFT weapon frame; no model basis is replaced by a quaternion.
struct Transform {
    Vec pivot{}, pitchAxis{}, yawAxis{}, translation{};
    float pitch{},yaw{};
    bool active{};
    Vec Vector(Vec v)const{
        return active?Rotate(Rotate(v,pitchAxis,pitch),yawAxis,yaw):v;
    }
    Vec Point(Vec v)const{return active?pivot+Vector(v-pivot)+translation:v;}
    template<class Pose> void Apply(Pose& p)const{
        Write(p.position,Point(Read(p.position)));
        Write(p.right,Vector(Read(p.right)));
        Write(p.forward,Vector(Read(p.forward)));
        Write(p.up,Vector(Read(p.up)));
    }
};
inline Transform Build(Sample s,const float* barrel,const float* modelTop,const float* pivot,
                       const float* laserOrigin=nullptr){
    const Vec b=Unit(Read(barrel));
    const Vec rotationCenter=Read(pivot);
    Vec lever=laserOrigin?Unit(Read(laserOrigin)-rotationCenter):b;
    if (Dot(lever,lever)<.5f) lever=b;
    // The horizontal pitch axis lifts the laser exit around the grip. A
    // near-vertical lever has no unique up direction, so use the model top.
    const auto upFor=[modelTop](Vec direction){
        Vec up=Vec{0,1,0}-direction*direction.y;
        if (Dot(up,up)<.0001f) {
            const Vec fallback=Read(modelTop);
            up=fallback-direction*Dot(fallback,direction);
        }
        return Unit(up);
    };
    const Vec axis=Unit(Cross(lever,upFor(lever)));
    const Vec yawAxis=upFor(b);
    const Vec translation=Dot(s.rearOffset,s.rearOffset)>0?s.rearOffset:b*(-s.rear);
    const float pitch=Dot(axis,axis)>.5f?s.pitch:0.f;
    const float yaw=Dot(yawAxis,yawAxis)>.5f?s.yaw:0.f;
    return {rotationCenter,axis,yawAxis,translation,pitch,yaw,pitch!=0 || yaw!=0 || s.rear!=0};
}
struct Launch {Vec velocity{},spinAxis{};float spinSpeed{};};
inline Launch DisarmLaunch(Sample kick,const float* barrel,const float* modelTop,const float* pivot){
    const Vec b=Unit(Read(barrel));
    // The backward ejection vector tilts toward the muzzle's upward/sideways
    // recoil deflection. Negate the angles when rotating -barrel, otherwise a
    // positive muzzle rise would launch the weapon downward.
    const auto back=Build({-kick.pitch,-kick.yaw,0},barrel,modelTop,pivot);
    const auto muzzle=Build(kick,barrel,modelTop,pivot);
    const float magnitude=std::sqrt(kick.pitch*kick.pitch+kick.yaw*kick.yaw);
    const float strength=std::clamp(magnitude/(30.f*kRad),.6f,1.8f);
    const Vec axis=Unit(muzzle.pitchAxis*kick.pitch+muzzle.yawAxis*kick.yaw);
    // The shoulder displacement contributes its own backward impulse. For a
    // critically damped spring, peak x = initialVelocity / (omega * e). Convert
    // that peak back to velocity and add it before the shared speed cap, so a
    // stronger shoulder setting flattens the launch toward the shooter instead
    // of increasing an upward throw. Pitch/yaw still supply their deflection.
    constexpr float e=2.718281828f;
    const Vec shoulder=Dot(kick.rearOffset,kick.rearOffset)>0?kick.rearOffset:b*(-std::max(0.f,kick.rear));
    const Vec velocity=back.Vector(b*(-1.f))*(2.f*strength)+shoulder*(kReturnOmega*e);
    return {velocity,axis,
            Dot(axis,axis)>.5f?6.f*strength:0.f};
}
} // namespace savr::recoil
