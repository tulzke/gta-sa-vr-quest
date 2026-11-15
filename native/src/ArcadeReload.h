#pragma once

#include "PhysicalGunCycle.h"
#include <atomic>
#include <cmath>

namespace savr::arcadereload {

enum Mode { AUTOMATIC, DOWN_UP, TWIST, MODE_COUNT };
inline std::atomic<int> mode{DOWN_UP};
inline int GetMode() { return mode.load(std::memory_order_relaxed); }
inline void SetMode(int value) { mode.store(std::clamp(value, 0, MODE_COUNT-1), std::memory_order_relaxed); }
inline const char* ModeName() {
    constexpr const char* names[]{"AUTOMATIC", "DOWN / UP", "TWIST / RETURN"};
    return names[GetMode()];
}
inline bool Manual(int type) { return GetMode()!=AUTOMATIC && type>=22 && type<=34; }

// Separate from CWeapon: state 2 can mean either empty/waiting or a reload
// explicitly started by the player. Never put invented states into the game.
struct Gun { int type{}; bool requested{}, completed{}; };
inline Gun guns[26]{};
inline void ResetGuns() { for (auto& gun : guns) gun={}; }
inline Gun& Prepare(int slot, int type) {
    auto& gun=guns[slot];
    if (gun.type!=type) gun={type, false, false};
    return gun;
}
inline void Advance(int slot, int type, unsigned& state, unsigned& clip,
                    unsigned total, unsigned deadline, unsigned now, unsigned capacity) {
    auto& gun=Prepare(slot,type);
    if (Manual(type) && !clip && total && state<=1u) state=2u;
    const bool wasReloading=state==2u;
    vrfire::AdvancePhysicalGun(state,clip,total,deadline,now,capacity,
                              !Manual(type) || gun.requested);
    if (wasReloading && state!=2u && gun.requested) gun.completed=true;
    if (state!=2u) gun.requested=false;
}
inline bool Begin(int slot, int type, unsigned& state, unsigned clip,
                  unsigned total, unsigned& deadline, unsigned now,
                  unsigned capacity, unsigned duration) {
    auto& gun=Prepare(slot,type);
    if (!Manual(type) || state>2u || gun.requested || clip>=capacity || total<=clip)
        return false;
    state=2u;
    deadline=now+std::clamp(duration,1u,30000u);
    gun.requested=true;
    gun.completed=false;
    return true;
}
inline bool Completed(int slot) {
    const bool result=guns[slot].completed;
    guns[slot].completed=false;
    return result;
}

struct Vec { float x{},y{},z{}; };
struct Quat { float x{},y{},z{},w{1}; };
inline float Dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec Cross(Vec a,Vec b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline Vec Scale(Vec a,float s) { return {a.x*s,a.y*s,a.z*s}; }
inline Vec Add(Vec a,Vec b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec Rotate(Quat q,Vec v) {
    const Vec u{q.x,q.y,q.z};
    return Add(Add(Scale(u,2*Dot(u,v)),Scale(v,q.w*q.w-Dot(u,u))),Scale(Cross(u,v),2*q.w));
}
inline Quat Delta(Quat to,Quat from) {
    const Vec a{to.x,to.y,to.z},b{-from.x,-from.y,-from.z};
    const Vec v=Add(Add(Scale(b,to.w),Scale(a,from.w)),Cross(a,b));
    return {v.x,v.y,v.z,to.w*from.w-Dot(a,b)};
}
inline float Twist(Quat delta,Vec axis) {
    float a=2*std::atan2(Dot({delta.x,delta.y,delta.z},axis),delta.w);
    constexpr float pi=3.141592654f;
    if(a>pi)a-=2*pi;
    if(a<-pi)a+=2*pi;
    return a;
}
struct Sample { Vec position{},barrel{0,0,-1}; Quat orientation{}; };
inline bool Normalize(Sample& s) {
    const float length=std::sqrt(Dot(s.barrel,s.barrel));
    const auto& q=s.orientation;
    const float qlength=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    if (!std::isfinite(s.position.x) || !std::isfinite(s.position.y) || !std::isfinite(s.position.z) ||
        !std::isfinite(length) || length<.5f || !std::isfinite(qlength) || qlength<.5f)
        return false;
    s.barrel=Scale(s.barrel,1/length);
    s.orientation={q.x/qlength,q.y/qlength,q.z/qlength,q.w/qlength};
    return true;
}

// Raw tracked wrists drive recognition; rendered recoil/hand animations never
// count as gestures. The calibrated barrel only supplies the twist axis.
struct Gesture {
    Sample previous{},start{};
    double previousTime{},startTime{},readyAt{};
    int identity{-1},selected{-1},phase{},path{};
    std::uint64_t epoch{};
    bool valid{};
    void Reset() { *this={}; }
    bool Update(Sample sample,double now,int key,int selection,std::uint64_t trackingEpoch) {
        if (selection==AUTOMATIC || !std::isfinite(now) || !Normalize(sample)) { Reset(); return false; }
        if (!valid || identity!=key || selected!=selection || epoch!=trackingEpoch ||
            now<previousTime || now-previousTime>.12) {
            Reset(); valid=true; identity=key; selected=selection; epoch=trackingEpoch;
            previous=sample;previousTime=now;readyAt=now+.15;return false;
        }
        const float dt=static_cast<float>(now-previousTime);
        if (dt<=0) return false;
        const Sample old=previous;
        previous=sample;previousTime=now;
        if (now<readyAt) return false;
        const float vy=(sample.position.y-old.position.y)/dt;
        const Vec oldBarrel=Rotate(Delta(sample.orientation,old.orientation),old.barrel);
        const float pitchSpeed=(std::asin(std::clamp(oldBarrel.y,-1.f,1.f))-
                                std::asin(std::clamp(old.barrel.y,-1.f,1.f)))/dt;
        const float rollSpeed=Twist(Delta(sample.orientation,old.orientation),old.barrel)/dt;
        if (!phase) {
            path=selection==TWIST?2:-1;
            if (selection==TWIST ? std::abs(rollSpeed)<2.2f : (vy>=-.65f && pitchSpeed>=-2.2f)) return false;
            start=old;startTime=now-dt;phase=1;
        }
        if (now-startTime>.85 || (phase==1 && now-startTime>.35)) {
            phase=0;readyAt=now+.12;return false;
        }
        const Vec barrel=Rotate(Delta(sample.orientation,start.orientation),start.barrel);
        const float pitch=std::asin(std::clamp(barrel.y,-1.f,1.f))-
                          std::asin(std::clamp(start.barrel.y,-1.f,1.f));
        const float drop=start.position.y-sample.position.y;
        const float roll=std::abs(Twist(Delta(sample.orientation,start.orientation),start.barrel));
        // A twist must keep pointing approximately along its original axis.
        if (path==2 && Dot(barrel,start.barrel)<.866f) { phase=0;readyAt=now+.12;return false; }
        if (phase==1) {
            if (path==2) { if (roll>=.785f) phase=2; }
            else if (drop>=.08f) { path=0;phase=2; }
            else if (pitch<=-.52f) { path=1;phase=2; }
            return false;
        }
        const bool returned=path==0 ? drop<=.025f && vy>.4f :
                            path==1 ? std::abs(pitch)<.175f && pitchSpeed>1.f : roll<.21f;
        if (!returned) return false;
        phase=0;readyAt=now+.45;
        return true;
    }
};

} // namespace savr::arcadereload
