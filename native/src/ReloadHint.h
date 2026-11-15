#pragma once

#include "ArcadeReload.h"
#include <array>
#include <memory>
#include <vector>

namespace savr::reloadhint {
using Vec=arcadereload::Vec;
using Quat=arcadereload::Quat;
inline constexpr unsigned kDelayMs=3000;
inline constexpr unsigned kLoopMs=1600;
inline constexpr float kOpacity=.20f;
inline constexpr float kModelScale=1.05f;
struct Request {
    bool visible{};
    int slot{-1},type{},mode{},hand{};
    unsigned phaseMs{};
    std::uint64_t epoch{};
};
using Requests=std::array<Request,2>;
struct Tracker {
    struct Wait { bool active{};int slot{-1},type{},mode{};unsigned since{};std::uint64_t epoch{}; } waits[2];
    Requests requests{};
    void Reset() { *this={}; }
    void Step(int hand,int slot,int type,int mode,bool emptyWithReserve,unsigned now,std::uint64_t epoch) {
        if (hand<0 || hand>1) return;
        auto& wait=waits[hand];auto& request=requests[hand];request={};
        if (!emptyWithReserve || slot<=0 || mode==arcadereload::AUTOMATIC) { wait={};return; }
        if (!wait.active || wait.slot!=slot || wait.type!=type || wait.mode!=mode || wait.epoch!=epoch)
            wait={true,slot,type,mode,now,epoch};
        const unsigned elapsed=now-wait.since;
        if (elapsed>=kDelayMs) request={true,slot,type,mode,hand,(elapsed-kDelayMs)%kLoopMs,epoch};
    }
};
struct Model {
    std::vector<float> vertices;
    std::vector<unsigned short> indices;
    Vec center{};
    float extent{};
};
struct Pose { Vec position{},barrel{1,0,0},top{0,1,0},side{0,0,1}; };
struct Visual { std::shared_ptr<const Model> model;Pose pose{};float opacity{kOpacity}; };
struct VisualFrame {
    std::array<Visual,2> items{};
    std::uint64_t epoch{};
    bool Visible() const { return (items[0].model && items[0].opacity>0) || (items[1].model && items[1].opacity>0); }
};
inline float Smooth(float x) { x=std::clamp(x,0.f,1.f);return x*x*(3-2*x); }
struct Motion { float down{},roll{},visibility{1}; };
inline Motion Animation(int mode,unsigned phaseMs) {
    const float t=(phaseMs%kLoopMs)*.001f;
    if (mode==arcadereload::TWIST) {
        const float amount=t<.25f?0:t<.48f?Smooth((t-.25f)/.23f):1;
        // Show only the outward gesture. Reset to the hand pose while hidden,
        // rather than teaching an obsolete reverse movement.
        const float visibility=std::min(Smooth(t/.12f),1-Smooth((t-1.15f)/.20f));
        return {0,.98f*amount,visibility};
    }
    // The fast downward stroke is the gesture; a slower reset makes the loop
    // readable without teaching an obligatory upward return for DOWN_FLICK.
    const float reset=mode==arcadereload::DOWN_UP?.20f:.55f;
    const float amount=t<.25f?0:t<.43f?Smooth((t-.25f)/.18f):
                       t<.72f?1:t<.72f+reset?1-Smooth((t-.72f)/reset):0;
    return {.58f*amount,0};
}
inline Vec Rotated(Vec v,Vec axis,float angle) {
    const float s=std::sin(angle*.5f);
    return arcadereload::Rotate({axis.x*s,axis.y*s,axis.z*s,std::cos(angle*.5f)},v);
}
inline Vec Unit(Vec v,Vec fallback) {
    const float length=std::sqrt(arcadereload::Dot(v,v));
    return std::isfinite(length) && length>.0001f?arcadereload::Scale(v,1/length):fallback;
}
inline Pose BuildPose(const Pose& weapon,Vec wrist,int hand,int mode,unsigned phaseMs) {
    using namespace arcadereload;
    const float sign=hand==0?-1.f:1.f;
    Pose pose=weapon;
    const Motion motion=Animation(mode,phaseMs);
    if (motion.down==0 && motion.roll==0) return pose;
    const Vec up{0,1,0};
    const Vec barrel=Unit(weapon.barrel,{0,0,-1});
    const Vec nodAxis=Unit(Cross(barrel,up),Unit(weapon.side,{1,0,0}));
    const float down=std::min(motion.down,std::max(0.f,std::asin(std::clamp(barrel.y,-1.f,1.f))+1.540796327f));
    const auto rotate=[&](Vec axis,float angle) {
        pose.position=Add(wrist,Rotated(Add(pose.position,Scale(wrist,-1)),axis,angle));
        pose.barrel=Rotated(pose.barrel,axis,angle);
        pose.top=Rotated(pose.top,axis,angle);
        pose.side=Rotated(pose.side,axis,angle);
    };
    rotate(nodAxis,-down);
    if (mode==DOWN_FLICK) {
        const float peakDown=std::min(.58f,std::max(0.f,std::asin(std::clamp(barrel.y,-1.f,1.f))+1.540796327f));
        const float tipDrop=.20f*(barrel.y-Rotated(barrel,nodAxis,-peakDown).y);
        const float stroke=std::max(.03f,NodDistanceMetres()*1.25f-tipDrop);
        pose.position=Add(pose.position,Scale(up,-stroke*(motion.down/.58f)));
    } else pose.position=Add(pose.position,Scale(up,-motion.down*.10f));
    rotate(barrel,sign*motion.roll);
    return pose;
}
inline std::array<float,16> ModelMatrix(const Pose& pose,const Model& model) {
    using namespace arcadereload;
    // Expand around the geometry bounds center, not the calibrated grip origin:
    // the shell then extends on both sides of the real weapon without drifting.
    const Vec offset=Scale(model.center,1-kModelScale);
    const Vec p=Add(pose.position,Add(Scale(pose.barrel,offset.x),
                                   Add(Scale(pose.top,offset.y),Scale(pose.side,offset.z))));
    const Vec x=Scale(pose.barrel,kModelScale),y=Scale(pose.top,kModelScale),z=Scale(pose.side,kModelScale);
    return {x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,p.x,p.y,p.z,1};
}
} // namespace savr::reloadhint
