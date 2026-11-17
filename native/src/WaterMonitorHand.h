#pragma once
#include "WaterMonitorLogic.h"
#include "ServiceRadioHand.h"
#include "HandAnimation.h"
namespace savr::watermonitor {
inline handanimation::Selection PistolPose(float press){
 return {handanimation::Gesture::GripPistolShoot,handanimation::Gesture::GripPistolShoot,handanimation::Unit(press),0.f};
}
// Pistol pose in runtime hand-mesh coordinates. The finger opening is placed
// at the authored grip anchor, with the thumb mirrored across the handle for
// the left hand. No tracked pose or per-grab offset changes this attachment.
constexpr Vec GripAnchor{0,.003f,.065f},GripMeshAnchor{.108f,.012f,.016f};
inline radio::HandPose FixedGrip(const Frame& device,int hand){
 const Vec forward=device.forward,up=device.right*(hand==0?-1.f:1.f),right=device.up;
 const Vec position=device.Point(GripAnchor)-forward*(GripMeshAnchor.x-.055f)+up*GripMeshAnchor.y-right*GripMeshAnchor.z;
 return {position,right,up,forward};
}
inline Frame HeldFrame(const xr::HandPose& h,int hand){
 const Vec f=unit(handrender::Rotate(h.aimValid?h.aimOri:h.gripOri,{0,0,-1}));
 Vec u=handrender::Rotate(h.gripOri,{0,1,0});u=unit(u-f*dot(u,f));
 if(dot(u,u)<.5f){Vec side=handrender::Rotate(h.gripOri,{1,0,0});side=unit(side-f*dot(side,f));
  if(dot(side,side)<.5f)side=unit(cross(f,{0,0,1}));u=unit(cross(side,f));}
 const Vec r=unit(cross(f,u));u=unit(cross(r,f));
 const Vec p{h.gripPos[0],h.gripPos[1],h.gripPos[2]};
 Frame device{p,r,f,u};
 // Solve the device origin from its fixed wrist attachment. The hand stays
 // at the tracked wrist while the handle always has the same palm contact.
 device.center=p-(FixedGrip(device,hand).position-p);return device;
}
inline Frame Blend(const Frame& a,const Frame& b,float t){
 const auto f=serviceradio::Blend({a.center,a.right,a.forward,a.up},{b.center,b.right,b.forward,b.up},std::clamp(t,0.f,1.f));
 return {f.center,f.right,f.forward,f.up};
}
}
