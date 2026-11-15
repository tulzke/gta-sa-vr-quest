#pragma once

#include "PhysicalGunCycle.h"
#include <atomic>
#include <cmath>

namespace savr::arcadereload {

// Keep saved values 0..2 stable; new profiles are appended.
enum Mode { AUTOMATIC, DOWN_UP, TWIST, DOWN_FLICK, MODE_COUNT };
inline std::atomic<int> mode{DOWN_UP};
inline int GetMode() { return mode.load(std::memory_order_relaxed); }
inline void SetMode(int value) { mode.store(std::clamp(value, 0, MODE_COUNT-1), std::memory_order_relaxed); }
inline const char* ModeName() {
    constexpr const char* names[]{"AUTOMATIC", "DOWN / UP", "TWIST / RETURN", "DOWN FLICK"};
    return names[GetMode()];
}
inline bool Manual(int type) { return GetMode()!=AUTOMATIC && type>=22 && type<=34; }

enum Sound { BOLT, MAGAZINE, SILENT, SOUND_COUNT };
inline std::atomic<int> sound{BOLT};
inline int GetSound() { return sound.load(std::memory_order_relaxed); }
inline void SetSound(int value) { sound.store(std::clamp(value,0,SOUND_COUNT-1),std::memory_order_relaxed); }
inline const char* SoundName() {
    constexpr const char* names[]{"BOLT CLACK", "MAG CLICK", "OFF"};
    return names[GetSound()];
}
// Retail 2.11.311 ReportWeaponEvent, M4 (type 31): 147 -> sample 32,
// 146 -> sample 31. Both are short one-shots from the existing GENRL bank.
inline int SoundEvent() { return GetSound()==SILENT ? 0 : GetSound()==BOLT ? 147 : 146; }

// State 2 means empty/waiting in gesture modes. Only the completed gesture
// refills the magazine; the native automatic mode keeps its original clock.
inline void Advance(int type, unsigned& state, unsigned& clip,
                    unsigned total, unsigned deadline, unsigned now, unsigned capacity) {
    if (Manual(type) && !clip && total && state<=1u) state=2u;
    vrfire::AdvancePhysicalGun(state,clip,total,deadline,now,capacity,!Manual(type));
}
inline bool Reload(int type, unsigned& state, unsigned& clip,
                  unsigned total, unsigned& deadline, unsigned now,
                  unsigned capacity) {
    if (!Manual(type) || state>2u || clip>=capacity || total<=clip)
        return false;
    clip=std::min(total,capacity);
    state=0u;
    deadline=now;
    return true;
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
// count as gestures. The calibrated barrel supplies the twist axis and nod direction.
struct Gesture {
    struct TimedSample { Sample sample{}; double time{}; };
    TimedSample history[48]{};
    int historyCount{},historyNext{};
    Sample previous{},start{};
    double previousTime{},startTime{},readyAt{};
    double flickSpeedTime{},brakeSince{};
    float flickSpeed{},peakSpeed{};
    int identity{-1},selected{-1},phase{},path{};
    std::uint64_t epoch{};
    bool valid{};
    void Reset() { *this={}; }
    void Remember(Sample sample,double now) {
        history[historyNext]={sample,now};
        historyNext=(historyNext+1)%48;
        historyCount=std::min(historyCount+1,48);
    }
    void ClearFlick() {
        historyCount=historyNext=phase=0;
        flickSpeed=peakSpeed=0;flickSpeedTime=brakeSince=0;
    }
    static float PitchDrop(const Sample& from,const Sample& to) {
        const Vec barrel=Rotate(Delta(to.orientation,from.orientation),from.barrel);
        return std::asin(std::clamp(from.barrel.y,-1.f,1.f))-
               std::asin(std::clamp(barrel.y,-1.f,1.f));
    }
    bool DownFlick(Sample sample,double now) {
        if (now<readyAt) { ClearFlick(); return false; }
        // Smooth angular speed over ~30ms, using tracked rotations only.
        // Translation never arms this gesture; arcs, yaw and roll remain free.
        const TimedSample* recent=nullptr;
        double seconds=.076;
        for(int i=0;i<historyCount;++i) {
            const auto& from=history[i];
            const double age=now-from.time;
            if (age>=.025 && age<seconds) { recent=&from;seconds=age; }
        }
        if (recent) {
            const float speed=PitchDrop(recent->sample,sample)/static_cast<float>(seconds);
            const double interval=flickSpeedTime ? now-flickSpeedTime : seconds;
            const float acceleration=(speed-flickSpeed)/static_cast<float>(interval);
            flickSpeed=speed;flickSpeedTime=now;
            if (!phase && speed>=2.2f && acceleration>=8.f) {
                start=recent->sample;startTime=recent->time;
                // Count the beginning of a naturally accelerating nod too.
                // Starting the angle at the speed threshold discarded most
                // of a small wrist flick before it reached peak speed.
                double lead=seconds;
                for(int i=0;i<historyCount;++i) {
                    const double age=now-history[i].time;
                    if (age<=.14 && age>lead) { start=history[i].sample;lead=age; }
                }
                peakSpeed=speed;brakeSince=0;phase=1;
            }
            if (phase) {
                peakSpeed=std::max(peakSpeed,speed);
                const float angle=PitchDrop(start,sample);
                // A long aiming/holstering sweep is not a short wrist snap.
                if (now-startTime>.42 || angle>1.25f || angle<-.12f) {
                    phase=0;brakeSince=0;readyAt=now+.15;
                } else if (angle>=.20f && std::abs(speed)<=std::min(1.5f,peakSpeed*.55f)) {
                    // A clear slowdown is enough; no perfectly still wrist or
                    // upward return. Brief confirmation rejects a noisy frame.
                    if (!brakeSince) brakeSince=now;
                    if (now-brakeSince>=.012) {
                        ClearFlick();readyAt=now+.45;return true;
                    }
                } else brakeSince=0;
            }
        }
        Remember(sample,now);
        return false;
    }
    bool Update(Sample sample,double now,int key,int selection,std::uint64_t trackingEpoch) {
        if (selection==AUTOMATIC || !std::isfinite(now) || !Normalize(sample)) { Reset(); return false; }
        if (!valid || identity!=key || selected!=selection || epoch!=trackingEpoch ||
            now<previousTime || now-previousTime>.12) {
            Reset(); valid=true; identity=key; selected=selection; epoch=trackingEpoch;
            previous=sample;previousTime=now;readyAt=now+(selection==DOWN_FLICK?0:.15);
            if (selection==DOWN_FLICK) Remember(sample,now);
            return false;
        }
        const float dt=static_cast<float>(now-previousTime);
        if (dt<=0) return false;
        const Sample old=previous;
        previous=sample;previousTime=now;
        if (selection==DOWN_FLICK) {
            const Vec jump{sample.position.x-old.position.x,sample.position.y-old.position.y,
                           sample.position.z-old.position.z};
            const auto& a=sample.orientation;const auto& b=old.orientation;
            if (Dot(jump,jump)>.16f || std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w)<.5f) {
                ClearFlick();Remember(sample,now);return false;
            }
            return DownFlick(sample,now);
        }
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
