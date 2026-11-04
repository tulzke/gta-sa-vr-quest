#pragma once
#include "VehicleDoorMotion.h"

namespace savr::vehicleclimb {
using vehicledoors::Vec;
using vehicledoors::Dot;
using vehicledoors::Finite;
using vehicledoors::Length;

// All coordinates are in the vehicle's rigid local frame, in metres.
struct Step {
    Vec min{}, max{};
    bool Valid() const {
        return Finite(min) && Finite(max) && max.x-min.x>=.05f &&
            max.y-min.y>=.05f && max.z-min.z>=.02f;
    }
    bool Contains(Vec foot) const {
        return Valid() && Finite(foot) && foot.x>=min.x && foot.x<=max.x &&
            foot.y>=min.y && foot.y<=max.y && foot.z>=min.z && foot.z<=max.z;
    }
    // Translation required to place this foot at the upper surface.
    float Lift(Vec foot) const { return Contains(foot) ? max.z-foot.z : 0.f; }
    bool Contact(Vec previous,Vec foot,float& lift) const {
        lift=0;
        if(!Valid()||!Finite(previous)||!Finite(foot)||Length(foot-previous)>.5f)return false;
        const float a[]={previous.x,previous.y,previous.z},b[]={foot.x,foot.y,foot.z};
        const float lo[]={min.x,min.y,min.z},hi[]={max.x,max.y,max.z};
        float enter=0,leave=1;
        for(int i=0;i<3;++i){
            const float d=b[i]-a[i];
            if(std::abs(d)<.00001f){if(a[i]<lo[i]||a[i]>hi[i])return false;continue;}
            float t0=(lo[i]-a[i])/d,t1=(hi[i]-a[i])/d;if(t0>t1)std::swap(t0,t1);
            enter=std::max(enter,t0);leave=std::min(leave,t1);if(enter>leave)return false;
        }
        const float distance=max.z-foot.z;
        if(distance<-.04f||distance>.20f)return false;
        lift=std::max(0.f,distance);return true;
    }
};

struct Pipe {
    Vec bottom{}, top{};
    bool Valid() const {
        return Finite(bottom) && Finite(top) && Length(top-bottom)>=.10f;
    }
    float Distance(Vec hand) const {
        if (!Valid() || !Finite(hand)) return 1e6f;
        const Vec axis=top-bottom;
        const float t=std::clamp(Dot(hand-bottom,axis)/Dot(axis,axis),0.f,1.f);
        return Length(hand-(bottom+axis*t));
    }
};

// A tracked hand must not inject discontinuities into player movement.
inline bool PullDelta(Vec previous, Vec current, float dt, Vec& delta) {
    delta={};
    if (!Finite(previous) || !Finite(current) || !std::isfinite(dt) ||
        dt<=0.f || dt>.15f) return false;
    const Vec motion=current-previous;
    if (Length(motion)>.35f) return false;
    delta=motion*(-1.f);
    return true;
}
inline vehicledoors::Basis PipeHandFrame(Vec axis,Vec heading,float pitch,float roll){
    using namespace vehicledoors;
    axis=axis*(1.f/Length(axis));
    Vec forward=heading-axis*Dot(heading,axis);
    if(Length(forward)<.05f){forward=Cross(axis,{1,0,0});if(Length(forward)<.05f)forward=Cross(axis,{0,1,0});}
    forward=forward*(1.f/Length(forward));
    // The Grip mesh's finger-ring axis is local Z (rendered as right),
    // not local Y. Align it with the pipe before applying locked tilts.
    Vec right=axis,up=Cross(axis,forward);
    const float p=pitch*.01745329252f,r=roll*.01745329252f;
    const Vec f=forward;forward=f*std::cos(p)+right*std::sin(p);right=right*std::cos(p)-f*std::sin(p);
    const Vec oldRight=right;right=right*std::cos(r)+up*std::sin(r);up=up*std::cos(r)-oldRight*std::sin(r);
    return {right,forward,up,{}};
}
inline float ControllerTwist(const float current[4],const float reference[4],Vec axis){
    using namespace vehicledoors;
    const Vec a{current[0],current[1],current[2]},b{reference[0],reference[1],reference[2]};
    const Vec v=a*reference[3]-b*current[3]-Cross(a,b);
    const float w=current[3]*reference[3]+Dot(a,b);
    if(!Finite(v)||!std::isfinite(w)||Length(axis)<.5f)return 0.f;
    const float projected=Dot(v,axis*(1.f/Length(axis)));
    if(std::hypot(projected,w)<.001f)return 0.f;
    return std::remainder(2.f*std::atan2(projected,w),6.28318530718f);
}
inline Vec RotateAround(Vec v,Vec axis,float angle){
    using namespace vehicledoors;
    return v*std::cos(angle)+Cross(axis,v)*std::sin(angle)+axis*(Dot(axis,v)*(1.f-std::cos(angle)));
}
// Centre of the Grip mesh opening at local Z=0, measured from the inner
// contour of both BigHand*.uxrh pose-1 meshes (metres). The hand builder
// maps local X to forward with a -0.055 m wrist adjustment and Y to -up.
inline Vec GripPivotOffset(Vec forward,Vec up){return forward*(.09869f-.055f)-up*.01410f;}
}
