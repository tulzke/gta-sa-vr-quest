#pragma once
#include "RadioHandAttachment.h"
#include "TrackingFrame.h"
#include <cmath>

namespace savr::handrender {
using Vec=radio::HandVector;
inline Vec Cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline Vec Unit(Vec v){const float n=std::sqrt(radio::Dot(v,v));return n>1e-6f?v*(1.f/n):Vec{0,0,1};}
inline Vec Rotate(const float q[4],Vec p){const Vec v{q[0],q[1],q[2]};return p+Cross(v,Cross(v,p)+p*q[3])*2.f;}
// The same mirrored wrist frame is used by the hand renderer and by physical
// pointing hit tests. Mesh coordinates map to forward, -up, right.
inline radio::HandPose TrackedPose(const xr::HandPose& h,int hand){
    const Vec gripRight=Unit(Rotate(h.gripOri,{1,0,0})),gripUp=Unit(Rotate(h.gripOri,{0,1,0}));
    const Vec gripForward=Unit(Rotate(h.gripOri,{0,0,-1})),aim=Unit(Rotate(h.aimOri,{0,0,-1}));
    Vec forward=Unit(h.aimValid&&radio::Dot(aim,aim)>.1f?aim:gripUp);
    const float sign=hand==1?1.f:-1.f;
    Vec up=gripRight*sign;up=up-forward*radio::Dot(up,forward);
    if(radio::Dot(up,up)<.0001f)up=gripRight*sign;
    up=Unit(up);Vec right=Unit(Cross(up,forward));
    if(radio::Dot(right,gripForward)<0)right=right*-1.f;
    return {{h.gripPos[0],h.gripPos[1],h.gripPos[2]},right,up,forward};
}
inline Vec MeshPoint(const radio::HandPose& pose,Vec p){
    return pose.position+pose.forward*(p.x-.055f)-pose.up*p.y+pose.right*p.z;
}
}
