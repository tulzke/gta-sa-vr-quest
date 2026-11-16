#pragma once
#include "HandRenderFrame.h"
#include "VehicleHydraulics.h"
#include "VehicleHydraulicsHandConfig.h"
#include <algorithm>

namespace savr::hydrocontrol {
inline radio::HandFrame GripFrame(const InputFrame& pivot,float x,float y){
    const auto vec=[](Point p){return radio::HandVector{p.x,p.y,p.z};};
    return {vec(pivot.World(Grip(x,y))),vec(pivot.Vector(Swing({1,0,0},x,y))),
        vec(pivot.Vector(Swing({0,1,0},x,y))),vec(pivot.Vector(Swing({0,0,1},x,y)))};
}
inline radio::HandPose FixedGrip(const radio::HandFrame& ball,int hand,float radius,const hydrohand::Fit& fit={}){
    const auto r=ball.right,f=ball.up,u=ball.normal;
    // Same mirrored Grip mesh axes as the calibrated lift lever. Its opening
    // rests on the driver's upper hemisphere, with the palm outside the ball.
    const auto anchor=ball.center-f*std::max(0.f,radius-.008f)+u*.012f+
        r*(fit[0]*.01f)+f*(fit[1]*.01f)+u*(fit[2]*.01f);
    auto right=r*(hand==0?1.f:-1.f),up=u,forward=f;
    const radio::HandVector axes[]{r,f,u};
    for(int i=0;i<3;++i){const auto axis=axes[i];const float a=fit[3+i]*.017453292519943295f;
        const auto rotate=[&](radio::HandVector v){return v*std::cos(a)+handrender::Cross(axis,v)*std::sin(a)+axis*(radio::Dot(axis,v)*(1.f-std::cos(a)));};
        right=rotate(right);up=rotate(up);forward=rotate(forward);}
    const auto wrist=anchor-forward*(.09869f-.055f)+up*.01410f;
    return {wrist,right,up,forward};
}
struct HandAttachment {
    radio::HandPose last{},delta{};bool held{};float release{};
    void Reset(){*this=HandAttachment{};}
    radio::HandPose Grab(const radio::HandFrame& frame,int hand,float radius,const hydrohand::Fit& fit={}){
        held=true;release=0;last=FixedGrip(frame,hand,radius,fit);return last;
    }
    radio::HandPose Free(const radio::HandPose& tracked,float dt){
        if(held){held=false;release=1.f;
            delta={last.position-tracked.position,last.right-tracked.right,last.up-tracked.up,last.forward-tracked.forward};}
        const auto pose=radio::HandPose{tracked.position+delta.position*release,
            handrender::Unit(tracked.right+delta.right*release),handrender::Unit(tracked.up+delta.up*release),
            handrender::Unit(tracked.forward+delta.forward*release)};
        release=std::max(0.f,release-std::clamp(dt,0.f,.1f)/.12f);
        return pose;
    }
};
}
