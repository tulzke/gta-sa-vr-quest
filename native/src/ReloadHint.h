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
struct Pose { Vec position{},barrel{1,0,0},top{0,1,0},side{0,0,1};float scale{1}; };
struct Visual { std::shared_ptr<const Model> model;Pose pose{}; };
struct VisualFrame {
    std::array<Visual,2> items{};
    std::uint64_t epoch{};
    bool Visible() const { return bool(items[0].model) || bool(items[1].model); }
};
inline float Smooth(float x) { x=std::clamp(x,0.f,1.f);return x*x*(3-2*x); }
struct Motion { float down{},roll{}; };
inline Motion Animation(int mode,unsigned phaseMs) {
    const float t=(phaseMs%kLoopMs)*.001f;
    if (mode==arcadereload::TWIST) {
        const float amount=t<.25f?0:t<.48f?Smooth((t-.25f)/.23f):
                           t<.65f?1:t<.88f?1-Smooth((t-.65f)/.23f):0;
        return {0,.98f*amount};
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
inline Pose BuildPose(Vec head,Quat orientation,int hand,bool both,int mode,unsigned phaseMs,float extent) {
    using namespace arcadereload;
    const Vec right=Rotate(orientation,{1,0,0}),up=Rotate(orientation,{0,1,0}),forward=Rotate(orientation,{0,0,-1});
    const float sign=hand==0?-1.f:1.f;
    Pose pose;
    pose.position=Add(Add(head,Scale(forward,.80f)),Add(Scale(up,-.13f),Scale(right,both?sign*.25f:0.f)));
    pose.barrel=Add(Scale(forward,.82f),Scale(right,sign*.5723635f));
    pose.top=up;pose.side=Cross(pose.barrel,pose.top);
    pose.scale=extent>.001f?std::min(1.f,(both?.38f:.50f)/extent):1.f;
    const Motion motion=Animation(mode,phaseMs);
    pose.barrel=Rotated(pose.barrel,pose.side,-motion.down);
    pose.top=Rotated(pose.top,pose.side,-motion.down);
    pose.position=Add(pose.position,Scale(up,-motion.down*(mode==DOWN_FLICK?.30f:.10f)));
    pose.top=Rotated(pose.top,pose.barrel,sign*motion.roll);
    pose.side=Rotated(pose.side,pose.barrel,sign*motion.roll);
    return pose;
}
inline std::array<float,16> ModelMatrix(const Model& model,const Pose& pose) {
    using namespace arcadereload;
    const Vec x=Scale(pose.barrel,pose.scale),y=Scale(pose.top,pose.scale),z=Scale(pose.side,pose.scale);
    const Vec p=Add(pose.position,Scale(Add(Add(Scale(x,model.center.x),Scale(y,model.center.y)),Scale(z,model.center.z)),-1));
    return {x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,p.x,p.y,p.z,1};
}
} // namespace savr::reloadhint
