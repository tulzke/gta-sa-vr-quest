#pragma once

#include "TrackingFrame.h"
#include "VehicleHornVisual.h"
#include <algorithm>
#include <cmath>

namespace savr::vehiclehorn {

inline Vec operator+(Vec a,Vec b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec operator-(Vec a,Vec b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec operator*(Vec a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float Dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec Cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool Finite(Vec v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
inline float Length(Vec v){return std::sqrt(Dot(v,v));}
inline Vec Unit(Vec v){const float d=Length(v);return std::isfinite(d)&&d>1e-6f?v*(1.f/d):Vec{};}
inline Vec Read(const float v[3]){return {v[0],v[1],v[2]};}
inline Vec Rotate(Vec v,Vec axis,float angle){
    return v*std::cos(angle)+Cross(axis,v)*std::sin(angle)+axis*(Dot(axis,v)*(1.f-std::cos(angle)));
}
inline bool Quaternion(const float in[4],float out[4]){
    float sum=0;for(int i=0;i<4;++i){if(!std::isfinite(in[i]))return false;sum+=in[i]*in[i];}
    if(!std::isfinite(sum)||sum<1e-8f)return false;
    for(int i=0;i<4;++i)out[i]=in[i]/std::sqrt(sum);
    return true;
}
inline Vec Rotate(const float q[4],Vec v){const Vec u{q[0],q[1],q[2]};return v+Cross(u,Cross(u,v)+v*q[3])*2.f;}

// The same anatomical decode and canonical mesh axes as BuildHandFrames.
struct Pose { Vec position{},right{},up{},forward{}; bool valid{}; };
inline Pose Tracked(const xr::HandPose& hand,int index){
    Pose pose;float grip[4]{},aim[4]{};
    if(index<0||index>1||!hand.valid||!Finite(Read(hand.gripPos))||!Quaternion(hand.gripOri,grip))return pose;
    pose.position=Read(hand.gripPos);
    pose.forward=hand.aimValid&&Quaternion(hand.aimOri,aim)?Rotate(aim,{0,0,-1}):Rotate(grip,{0,1,0});
    pose.forward=Unit(pose.forward);
    pose.up=Rotate(grip,{index==0?-1.f:1.f,0,0});
    pose.up=Unit(pose.up-pose.forward*Dot(pose.up,pose.forward));
    pose.right=Unit(Cross(pose.up,pose.forward));
    if(Dot(pose.right,Rotate(grip,{0,0,-1}))<0)pose.right=pose.right*-1.f;
    pose.valid=Length(pose.up)>.5f&&Length(pose.right)>.5f;
    return pose;
}

// OpenPalm canonical centre is (0.080,-0.026,0) metres. The skeletal
// shader applies the Y-up conversion, then model(pos - .055 * forward).
// Central palm vertices in hands.glb lie within 2 mm of this contact patch.
inline constexpr float kPalmForward=.080f-.055f,kPalmUp=.026f;
inline Vec PalmPoint(const Pose& pose){return pose.position+pose.forward*kPalmForward+pose.up*kPalmUp;}

struct Frame {
    Vec center{},right{},up{},normal{}; // normal points INTO the wheel
    float radius{};
    bool Valid()const {
        return Finite(center)&&Finite(right)&&Finite(up)&&Finite(normal)&&std::isfinite(radius)&&radius>.01f&&
            std::abs(Dot(right,right)-1)<.01f&&std::abs(Dot(up,up)-1)<.01f&&
            std::abs(Dot(normal,normal)-1)<.01f&&std::abs(Dot(right,up))<.01f&&
            std::abs(Dot(right,normal))<.01f&&std::abs(Dot(up,normal))<.01f;
    }
    Vec LocalVector(Vec v)const{return {Dot(v,right),Dot(v,up),-Dot(v,normal)};}
    Vec Local(Vec p)const{return LocalVector(p-center);}
    Vec World(Vec p)const{return center+right*p.x+up*p.y-normal*p.z;}
};
template<class Wheel> inline Frame WheelFrame(const Wheel& wheel){
    return {Read(wheel.center),Read(wheel.right),Read(wheel.up),Read(wheel.normal),wheel.radius};
}
inline constexpr float kAttach=.03f,kSideRelease=.04f,kNormalRelease=.08f;
inline constexpr float kPress=.005f,kUnpress=.01f,kDeepRelease=.04f,kTransition=.08f;

inline Pose AttachedPose(const Frame& frame,const Visual& visual){
    Pose p;
    if(!frame.Valid()||!Finite(visual.point)||!Finite(visual.fingers))return p;
    p.up=frame.normal;
    p.forward=Unit(frame.right*visual.fingers.x+frame.up*visual.fingers.y);
    p.right=Unit(Cross(p.up,p.forward))*(visual.mirrored?-1.f:1.f);
    p.position=frame.World(visual.point)-p.forward*kPalmForward-p.up*kPalmUp;
    p.valid=Length(p.forward)>.5f;return p;
}

// Preserve orthonormal axes, including 180-degree turns; interpolating the
// matrix columns directly would collapse the hand midway through a turn.
inline Pose BlendPose(const Pose& from,const Pose& to,float weight){
    if(weight<=0)return from;
    if(weight>=1)return to;
    Pose p=from;
    Vec axis=Cross(from.up,to.up);const float sine=Length(axis),cosine=std::clamp(Dot(from.up,to.up),-1.f,1.f);
    axis=sine>1e-5f?axis*(1.f/sine):Unit(from.forward);
    const float angle=std::atan2(sine,cosine)*weight;
    p.up=Unit(Rotate(from.up,axis,angle));
    p.forward=Unit(Rotate(from.forward,axis,angle));
    p.right=Unit(Rotate(from.right,axis,angle));
    Vec target=Unit(to.forward-p.up*Dot(to.forward,p.up));
    if(Length(target)>.5f){
        const float turn=std::atan2(Dot(Cross(p.forward,target),p.up),Dot(p.forward,target))*weight;
        p.forward=Unit(Rotate(p.forward,p.up,turn));p.right=Unit(Rotate(p.right,p.up,turn));
    }
    p.position=from.position*(1.f-weight)+to.position*weight;
    return p;
}

struct HandState {
    Visual visual{};
    Vec controllerOffset{};
    bool armed{true},justPressed{};
    void Reset(){*this=HandState{};armed=false;}
    void Step(const Frame& frame,const xr::HandPose& hand,int index,bool available,float grip,float dt){
        justPressed=false;
        const Pose tracked=Tracked(hand,index);
        if(!available||!frame.Valid()||!tracked.valid||!std::isfinite(grip)){Reset();return;}
        dt=std::isfinite(dt)?std::clamp(dt,0.f,.1f):0.f;
        const Vec palm=frame.Local(PalmPoint(tracked));
        const Vec controller=frame.Local(tracked.position);
        const float radius=std::min(.45f*frame.radius,.09f),radial=std::hypot(palm.x,palm.y);
        const float outside=std::max(0.f,radial-radius);
        const float distance=std::hypot(outside,palm.z);
        const bool near=distance<=kAttach&&palm.z>=-.005f;
        if(!visual.attached&&!near)armed=true;
        if(!visual.attached&&armed&&near&&grip<=.30f&&Dot(tracked.up,frame.normal)>=.5f){
            visual.attached=true;armed=false;
            const float scale=radial>radius?radius/radial:1.f;
            visual.point={palm.x*scale,palm.y*scale,std::max(0.f,palm.z)};
            // Keep the exact parity of the existing controller-to-mesh decode;
            // changing reflection while blending would flip the thumb.
            visual.mirrored=Dot(Cross(tracked.up,tracked.forward),tracked.right)<0;
            // Retain the captured controller-to-palm offset: wrist rotation
            // alone must not turn into lateral/normal translation or release.
            controllerOffset=palm-controller;
            const Vec f=frame.LocalVector(tracked.forward);
            visual.fingers=Unit({f.x,f.y,0});
        }
        if(visual.attached){
            const Vec raw=controller+controllerOffset;
            if(std::hypot(raw.x-visual.point.x,raw.y-visual.point.y)>kSideRelease||
               raw.z>kNormalRelease||raw.z<-kDeepRelease){
                visual.attached=false;visual.pressed=false;armed=!near;
            }else{
                visual.point.z=std::max(0.f,raw.z);
                const Vec f=frame.LocalVector(tracked.forward);
                if(std::hypot(f.x,f.y)>.1f)visual.fingers=Unit({f.x,f.y,0});
                const bool pressed=raw.z<=(visual.pressed?kUnpress:kPress);
                justPressed=pressed&&!visual.pressed;visual.pressed=pressed;
            }
        }
        visual.weight=std::clamp(visual.weight+(visual.attached?dt:-dt)/kTransition,0.f,1.f);
    }
};
struct State {
    HandState hands[2]{};
    void Reset(){for(auto& h:hands)h.Reset();}
    unsigned Claimed()const{return (hands[0].visual.attached?1u:0u)|(hands[1].visual.attached?2u:0u);}
    bool Pressed()const{return hands[0].visual.pressed||hands[1].visual.pressed;}
};
} // namespace savr::vehiclehorn
