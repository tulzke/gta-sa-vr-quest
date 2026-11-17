#pragma once
#include "WaterMonitorLogic.h"
#include <array>
namespace savr::watermonitor {
// Authored misc_a pivot in the currently shipped firetruk.dff. The live node
// supplies the origin when available; this is the verified fallback, not a
// camera calibration or a replacement for the native water emission point.
constexpr Vec NozzlePivot{0,1.63911438f,1.64957905f};
using CameraSettings=std::array<float,6>; // right/forward/up cm, pitch/yaw/roll deg
inline constexpr CameraSettings DefaultCamera{35,0,30,0,0,0};
inline float ClampCamera(int field,float value){
 if(field<0||field>=6||!std::isfinite(value))return field>=0&&field<6?DefaultCamera[field]:0.f;
 return std::clamp(value,field<3?-200.f:-180.f,field<3?200.f:180.f);
}
inline Frame NozzleFrame(const Frame& car,Vec origin,float yaw,float pitch){
 const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
 return {origin,car.right*cy+car.forward*sy,
  car.right*(-sy*cp)+car.forward*(cy*cp)+car.up*sp,
  car.right*(sy*sp)+car.forward*(-cy*sp)+car.up*cp};
}
inline Frame LevelAim(Vec forward){
 forward=unit(forward);Vec right=unit(cross(forward,{0,0,1}));
 // Deterministic pole basis: rotating the device around a vertical aim must
 // not rotate the camera. Device up/right never enter this calculation.
 if(dot(right,right)<.5f)right={1,0,0};
 return {{},right,forward,unit(cross(right,forward))};
}
inline Frame CameraFrame(const Frame& nozzle,Frame desired,const CameraSettings& settings){
 desired.center=nozzle.Point({settings[0]*.01f,settings[1]*.01f,settings[2]*.01f});
 const auto rotate=[](Vec& a,Vec& b,float degrees){const float angle=degrees*Pi/180.f,c=std::cos(angle),s=std::sin(angle);const Vec old=a;a=old*c+b*s;b=b*c-old*s;};
 // Calibrate camera optics independently of water direction and origin. Camera
 // rotation follows desired hand aim immediately, not the slower nozzle servo.
 rotate(desired.forward,desired.up,settings[3]);
 rotate(desired.forward,desired.right,settings[4]);
 rotate(desired.right,desired.up,settings[5]);
 return desired;
}
}
