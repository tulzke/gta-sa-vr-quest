#pragma once
#include "WaterMonitorLogic.h"
#include "ServiceRadioHand.h"
namespace savr::watermonitor {
inline Frame HeldFrame(const xr::HandPose& h,int){
 const Vec f=unit(handrender::Rotate(h.aimValid?h.aimOri:h.gripOri,{0,0,-1}));
 Vec u={0,1,0};u=unit(u-f*dot(u,f));
 if(dot(u,u)<.5f){Vec side=handrender::Rotate(h.gripOri,{1,0,0});side=unit(side-f*dot(side,f));
  if(dot(side,side)<.5f)side=unit(cross(f,{0,0,1}));u=unit(cross(side,f));}
 const Vec r=unit(cross(f,u));u=unit(cross(r,f));
 const Vec p{h.gripPos[0],h.gripPos[1],h.gripPos[2]};
 return {p+f*.042f-u*.09f,r,f,u};
}
inline Frame Blend(const Frame& a,const Frame& b,float t){
 const auto f=serviceradio::Blend({a.center,a.right,a.forward,a.up},{b.center,b.right,b.forward,b.up},std::clamp(t,0.f,1.f));
 return {f.center,f.right,f.forward,f.up};
}
}
