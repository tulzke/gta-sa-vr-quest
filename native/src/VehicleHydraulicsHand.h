#pragma once
#include "HandRenderFrame.h"
#include <algorithm>

namespace savr::hydrocontrol {
inline radio::HandPose FixedGrip(const radio::HandFrame& ball,int hand,float radius){
    const auto r=ball.right,f=ball.up,u=ball.normal;
    // Same mirrored Grip mesh axes as the calibrated lift lever. Its opening
    // rests on the driver's upper hemisphere, with the palm outside the ball.
    const auto anchor=ball.center-f*std::max(0.f,radius-.008f)+u*.012f;
    const auto wrist=anchor-f*(.09869f-.055f)+u*.01410f;
    return {wrist,r*(hand==0?1.f:-1.f),u,f};
}
struct HandAttachment {
    radio::HandPose last{},delta{};bool held{};float release{};
    void Reset(){*this=HandAttachment{};}
    radio::HandPose Grab(const radio::HandFrame& frame,int hand,float radius){
        held=true;release=0;last=FixedGrip(frame,hand,radius);return last;
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
