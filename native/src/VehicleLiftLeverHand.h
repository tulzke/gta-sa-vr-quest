#pragma once
#include "VehicleClimbGeometry.h"

namespace savr::liftlever {
// User-calibrated on Quest, 2026-11-15: left/right; position in cm along the
// moving lever's right/forward/up axes, then rotations in degrees around XYZ.
// These are fixed fits, not INI defaults: temporary hand calibration is removed.
inline constexpr float kHandFit[2][6]{
    {0.f,-.5f,0.f,0.f,-180.f,180.f},
    {0.f,-.5f,0.f,0.f,-180.f,0.f}
};
inline vehicledoors::Basis HandFrame(const vehicledoors::Basis& lever,float angle,
                                     float gripX,int hand) {
    using namespace vehicledoors;
    if(hand<0||hand>1)return lever;
    const float* adjustment=kHandFit[hand];
    const Vec r=lever.right;
    const Vec f=lever.forward*std::cos(angle)+lever.up*std::sin(angle);
    const Vec u=lever.up*std::cos(angle)-lever.forward*std::sin(angle);
    auto frame=vehicleclimb::PipeGripMeshFrame(vehicleclimb::PipeHandFrame(r,f*(-1.f),0,0),hand);
    const Vec axes[3]{r,f,u};
    for(int axis=0;axis<3;++axis){
        const float a=adjustment[axis+3]*.017453292519943295f;
        const auto rotate=[&](Vec v){return v*std::cos(a)+Cross(axes[axis],v)*std::sin(a)+axes[axis]*Dot(axes[axis],v)*(1.f-std::cos(a));};
        frame.right=rotate(frame.right);frame.forward=rotate(frame.forward);frame.up=rotate(frame.up);
    }
    const Vec anchor=lever.position+lever.up*.05f+u*.18f+r*(gripX+adjustment[0]*.01f)+
                     f*(adjustment[1]*.01f)+u*(adjustment[2]*.01f);
    // Rotate about the opening of the fingers, keeping the grasp on the grip
    // while changing wrist orientation. Position edits then offset that anchor.
    frame.position=anchor-vehicleclimb::GripPivotOffset(frame.forward,frame.up);
    return frame;
}
}
