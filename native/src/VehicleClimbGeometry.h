#pragma once
#include "VehicleDoorMotion.h"

namespace savr::vehicleclimb {
using vehicledoors::Vec;
using vehicledoors::Dot;
using vehicledoors::Finite;
using vehicledoors::Length;

// All coordinates are in the vehicle's rigid local frame, in metres.
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
// Centre of the Grip mesh opening at local Z=0, measured from the inner
// contour of both BigHand*.uxrh pose-1 meshes (metres). The hand builder
// maps local X to forward with a -0.055 m wrist adjustment and Y to -up.
inline Vec GripPivotOffset(Vec forward,Vec up){return forward*(.09869f-.055f)-up*.01410f;}
// Both baked Grip meshes have the thumb on positive local Z. Local Z is the
// pipe axis, so reflecting it for the right hand turns the thumb downward.
// Mirror the forward axis instead: handedness still differs, but the thumb
// remains on the same side of the pipe for both hands.
inline vehicledoors::Basis PipeGripMeshFrame(vehicledoors::Basis frame,int hand){
    if(hand==1)frame.forward=frame.forward*(-1.f);
    return frame;
}
}
