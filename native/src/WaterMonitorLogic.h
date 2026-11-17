#pragma once
#include "RadioHandAttachment.h"
#include "TrackingRecovery.h"
#include <algorithm>
#include <cmath>

namespace savr::watermonitor {
using Vec=radio::HandVector;
inline float dot(Vec a,Vec b){return radio::Dot(a,b);}
inline Vec cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool finite(Vec v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
inline Vec unit(Vec v){const float n=std::sqrt(dot(v,v));return n>1e-6f?v*(1.f/n):Vec{};}
struct Frame {Vec center{},right{1,0,0},forward{0,0,-1},up{0,1,0};
 Vec Point(Vec p)const{return center+right*p.x+forward*p.y+up*p.z;}};
struct Hand {Frame frame{};float grip{};bool valid{},available{},near{};};
constexpr int ModelId=407;
constexpr float Pi=3.14159265358979323846f,PitchLimit=Pi/9.f;
constexpr float ReturnSeconds=.18f,CameraTail=3.f,CollapseSeconds=.35f;
inline float wrap(float a){return std::remainder(a,2.f*Pi);}
struct Servo {
 float yaw{},pitch{},yawSpeed{},pitchSpeed{};
 static void Axis(float& angle,float& speed,float target,float limit,float accel,float dt,bool circular){
  const float error=circular?wrap(target-angle):target-angle;
  const float wanted=std::copysign(std::min(limit,std::sqrt(2.f*accel*std::abs(error))),error);
  speed+=std::clamp(wanted-speed,-accel*dt,accel*dt);
  const float step=speed*dt;
  if(std::abs(step)>=std::abs(error)&&step*error>=0){angle=target;speed=0;}
  else angle+=step;
  if(circular)angle=wrap(angle);
 }
 void Step(float targetYaw,float targetPitch,float dt){
  if(!std::isfinite(targetYaw)||!std::isfinite(targetPitch)||!std::isfinite(dt)||dt<=0)return;
  dt=std::min(dt,.1f);Axis(yaw,yawSpeed,targetYaw,2.5f,9.f,dt,true);
  Axis(pitch,pitchSpeed,std::clamp(targetPitch,-PitchLimit,PitchLimit),1.f,5.f,dt,false);
  pitch=std::clamp(pitch,-PitchLimit,PitchLimit);
 }
 Vec Direction()const{return {-std::sin(yaw)*std::cos(pitch),std::cos(yaw)*std::cos(pitch),std::sin(pitch)};}
};
struct Screen {float x{},y{},light{};};
struct State {
 tracking::SeatKey seat{};int owner{-1};bool gripDown[2]{true,true},armed{},fire{},block{};
 Frame held{},released{};double returnUntil{},cameraUntil{};
 void Observe(bool b){if(!b)block=false;}
 void Reset(bool b=false){const bool blocked=block||b;*this=State{};block=blocked;}
 bool ConsumesFire()const{return owner>=0||block;}
 void Step(bool enabled,const tracking::SeatKey& key,const Hand hands[2],bool b,double now){
  Observe(b);fire=false;
  if(!enabled||!std::isfinite(now)||!(key==seat)){
   const bool hadOwner=owner>=0;Reset(hadOwner&&b);seat=key;if(!enabled||!std::isfinite(now))return;
  }
  if(owner>=0){const auto& h=hands[owner];
   if(!h.valid||!h.available||!std::isfinite(h.grip)||h.grip<=.30f){
    const bool release=h.valid&&h.available&&h.grip<=.30f;
    released=held;returnUntil=release?now+ReturnSeconds:0;
    cameraUntil=release?now+CameraTail:0;block=block||b;owner=-1;armed=false;
   }
  }
  for(int h=0;h<2;++h){const auto& in=hands[h];const bool valid=in.valid&&std::isfinite(in.grip);
   if(owner<0&&valid&&in.available&&in.near&&!gripDown[h]&&in.grip>=.65f){
    owner=h;armed=!b;cameraUntil=returnUntil=0;
   }
   if(valid&&in.grip<=.30f)gripDown[h]=false;else if(!valid||in.grip>=.65f)gripDown[h]=true;
  }
  if(owner>=0){held=hands[owner].frame;if(!b)armed=true;fire=armed&&b;}
 }
 bool CameraActive(double now)const{return owner>=0||(cameraUntil>0&&now<cameraUntil);}
 Screen Display(double now)const{
  if(CameraActive(now))return {1,1,1};
  if(cameraUntil<=0)return {};
  const float t=static_cast<float>((now-cameraUntil)/CollapseSeconds);
  if(t>=1)return {};
  if(t<.55f)return {1.f,std::max(.018f,1.f-t/.55f),1.f};
  const float p=std::clamp((t-.55f)/.45f,0.f,1.f);
  return {std::max(.003f,1.f-p),.018f*(1.f-p),1.f-p};
 }
};
struct Visual {
 bool visible{},cameraActive{},fire{};int owner{-1};void* vehicle{};
 Frame dock{},body{};Screen screen{};Vec desiredTracking{},actualWorld{},cameraWorld{};
};
inline Vec Circle(Vec direction,const Frame& camera,float tanX=.70020754f){
 const float depth=dot(direction,camera.forward);
 const float x=dot(direction,camera.right),y=dot(direction,camera.up);
 if(depth<=.02f)return {std::copysign(.9f,x==0?1.f:x),std::clamp(y,-.85f,.85f),0};
 return {std::clamp(x/(depth*tanX),-.9f,.9f),std::clamp(y/(depth*tanX*.75f),-.85f,.85f),0};
}
}
