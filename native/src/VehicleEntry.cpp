#include "VehicleEntry.h"
#include "BoatEntry.h"
#include "Symbols.h"
#include "VrCamera.h"
#include "Xr.h"
#include "Driving.h"
#include "Locomotion.h"
#include <chrono>
#include <cstring>
#include <dlfcn.h>
namespace savr::vehicleentry {
namespace {
using namespace vehicledoors;
template<class T>T Read(void* p,int off){T v{};std::memcpy(&v,static_cast<char*>(p)+off,sizeof(v));return v;}
bool (*canOpen)(const void*,const void*){};
bool (*healthy)(const void*){};
void* (*frameLtm)(void*){};
void* target{};int targetSeat{};std::uint8_t generation{};
double started{},last{},flashUntil{};std::uint64_t trackingEpoch{};
Marker marker{};bool visible{},released{true};
double Now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
}
void Install(void* h){
 frameLtm=reinterpret_cast<decltype(frameLtm)>(dlsym(h,"_Z13RwFrameGetLTMP7RwFrame"));
 canOpen=reinterpret_cast<decltype(canOpen)>(dlsym(h,"_ZNK8CVehicle15CanPedOpenLocksEPK4CPed"));
 healthy=reinterpret_cast<decltype(healthy)>(dlsym(h,"_ZN13CCarEnterExit12IsPedHealthyERK4CPed"));
}
bool ConsumesMove(){return started>0||Now()<flashUntil;}
void Update(bool usable,std::uint64_t epoch){
 const double now=Now();visible=false;
 xr::InputState input{};xr::GetInput(input);
 float x{},y{};locomotion::MapGameplaySticks(input.leftStick[0],input.leftStick[1],input.rightStick[0],input.rightStick[1],&x,&y,nullptr,nullptr);
 if(std::hypot(x,y)<.25f)released=true;
 void* ped=g.FindPlayerPed?g.FindPlayerPed(-1):nullptr;
 void* pad=g.CPad_GetPad?g.CPad_GetPad(0):nullptr;
 float o[3]{},f[3]{};
 const bool valid=usable&&epoch==trackingEpoch&&now-last<.15&&frameLtm&&canOpen&&healthy&&ped&&pad&&
 !Read<std::uint16_t>(pad,0x110)&&!Read<std::uint8_t>(pad,0x12d)&&healthy(ped)&&
 !driving::GetVehicleDebugMode()&&vrcam::GetHeadWorldRay(o,f);
 trackingEpoch=epoch;last=now;
 if(!valid){target=nullptr;started=0;flashUntil=0;released=false;return;}
 if(now<flashUntil){visible=true;marker.highlight=float((flashUntil-now)/.2);return;}
 if(!released){target=nullptr;started=0;return;}
 void* pool=g.CPools_ms_pVehiclePool?*g.CPools_ms_pVehiclePool:nullptr;
 if(!pool||Read<int>(pool,0x10)!=110){target=nullptr;started=0;return;}
 auto objects=Read<char*>(pool,0);auto flags=Read<std::uint8_t*>(pool,8);
 if(!objects||!flags)return;
 void* seated=g.FindPlayerVehicle(-1,false);
 const Vec eye{o[0],o[1],o[2]};
 Vec forward{f[0],f[1],0};float length=Length(forward);
 if(length<.1f){target=nullptr;started=0;return;}forward=forward*(1/length);
 const Vec right{forward.y,-forward.x,0};const Vec move=right*x+forward*y;
 void* bestCar=nullptr;int bestSeat=-1;std::uint8_t bestGeneration{};Vec point{};float best=2.2f;
 for(int slot=0;slot<110;++slot){
  if(flags[slot]&0x80)continue;
  void* car=objects+slot*0xc68;
  if(seated&&seated!=car)continue;
  if((Read<std::uint8_t>(car,0x5a)&7)!=2||!g.CVehicle_GetVehicleAppearance||g.CVehicle_GetVehicleAppearance(car)!=1||!canOpen(car,ped))continue;
  const Vec speed=Read<Vec>(car,0x68);
  if(!Finite(speed)||Length(speed)>.02f)continue;
  const int model=Read<std::uint16_t>(car,0x32);
  if(model==431||model==437||model==432||model==539)continue;
  void* matrix=Read<void*>(car,0x18);if(!matrix)continue;
  Basis b{Read<Vec>(matrix,0),Read<Vec>(matrix,0x10),Read<Vec>(matrix,0x20),Read<Vec>(matrix,0x30)};
  if(!b.Valid()||Length(eye-b.position)>4.5f)continue;
  for(int seat=-1;seat<=0;++seat){
   if(Read<void*>(car,seat<0?0x5a0:0x5a8))continue;
   const int door=seat<0?2:3;const int node=seat<0?10:8;
   void* frame=Read<void*>(car,0x800+node*8);if(!frame)continue;
   void* doorMatrix=frameLtm(frame);if(!doorMatrix)continue;
   const Vec hinge=b.Local(Read<Vec>(doorMatrix,0x30));
   const float side=seat<0?-1.f:1.f;
   const Vec p=b.Point({hinge.x*.5f,hinge.y-.35f,hinge.z+.2f});
   if(!seated){
    const float open=Read<float>(car,0x770+door*0x18),angle=Read<float>(car,0x77c+door*0x18);
    if(!std::isfinite(open)||!std::isfinite(angle)||std::abs(open)<.3f||angle/open<.45f)continue;
    if(Dot(eye-b.position,b.right)*side<.2f)continue;
    const Vec approach=b.Point({hinge.x,hinge.y-.35f,hinge.z});
    if(Length(eye-approach)>1.5f)continue;
   }else if(Read<void*>(car,0x5a0)!=ped&&Read<void*>(car,0x5a8)!=ped)continue;
   GameSymbols::Vec3 start{eye.x,eye.y,eye.z},end{p.x,p.y,p.z};
   alignas(8) unsigned char col[0x2c]{};void* hit{};
   if(!seated&&g.CWorld_ProcessLineOfSight&&g.CWorld_ProcessLineOfSight(&start,&end,col,&hit,true,true,false,true,true,false,false,false)&&hit!=car)continue;
   const float distance=Length(p-eye);if(distance<best){best=distance;bestCar=car;bestSeat=seat;point=p;bestGeneration=flags[slot];}
  }
 }
 if(!bestCar){target=nullptr;started=0;return;}
 if(target!=bestCar||targetSeat!=bestSeat||generation!=bestGeneration){started=0;target=bestCar;targetSeat=bestSeat;generation=bestGeneration;}
 Vec delta=point-eye;delta.z=0;
 const bool holding=std::isfinite(x)&&std::isfinite(y)&&Length(move)>.65f&&Length(delta)>.05f&&Dot(move,delta)/Length(delta)>.55f;
 if(!holding)started=0;else if(!started)started=now;
 marker={point,forward,{0,0,1}};marker.seatIcon=bestSeat<0?1:2;marker.progress=started?std::clamp(float((now-started)/.5),0.f,1.f):0;
 visible=true;
 if(marker.progress>=1){
  if(boatentry::SeatPlayer(bestCar,bestSeat)){marker.highlight=1;flashUntil=now+.2;xr::TriggerHaptic(0,.3f,0,35);}
  released=false;started=0;
 }
}
void Append(vehicledoors::MarkerFrame& frame){if(visible&&frame.count<frame.kCapacity)frame.markers[frame.count++]=marker;}
}
