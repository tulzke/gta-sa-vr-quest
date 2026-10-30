#include "VehicleEntry.h"
#include "PassengerReaction.h"
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
bool (*canStepOut)(const void*,bool){};
void* (*frameLtm)(void*){};
void* target{};int targetSeat{};std::uint8_t generation{};
double started{},last{},flashUntil{};std::uint64_t trackingEpoch{};
Marker marker{};bool visible{},released{true};
MarkerFrame icons;
int editHand=-1,editModel=-1,editSeat=-1;Vec editStart{},editBase{};
bool editLatch[2]{true,true};unsigned claimed{};
constexpr int nodes[4]={10,8,11,9};
double Now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void EndEdit(){if(editHand>=0)driving::SaveDebugCalibration();editHand=-1;claimed=0;}
bool DoorOpen(void* car,int index){
 const auto status=Read<std::uint8_t>(car,0x758+9+index+2);
 if(status==4)return true; // detached door
 const float open=Read<float>(car,0x770+(index+2)*0x18),angle=Read<float>(car,0x77c+(index+2)*0x18);
 return std::isfinite(open)&&std::isfinite(angle)&&std::abs(open)>.3f&&angle/open>=.45f;
}
bool ClearExit(Vec eye,Vec p,void* car){
 if(!g.CWorld_ProcessLineOfSight)return false;
 GameSymbols::Vec3 a{eye.x,eye.y,eye.z},b{p.x,p.y,p.z};
 alignas(8) unsigned char col[0x2c]{};void* hit{};
 if(g.CWorld_ProcessLineOfSight(&a,&b,col,&hit,true,true,false,true,true,false,false,false)&&hit!=car)return false;
 // Clearance outside the sill, at torso and head height; vehicle itself is ignored.
 for(float z:{-.3f,.3f}) {
  GameSymbols::Vec3 s{p.x,p.y,p.z+z},e{p.x,p.y,p.z+z+.4f};hit=nullptr;
  if(g.CWorld_ProcessLineOfSight(&s,&e,col,&hit,true,true,true,true,true,false,false,false)&&hit!=car)return false;
 }
 return true;
}
}
int SeatIndex(void* car,void* ped){
 if(!car||!ped)return -2;
 if(Read<void*>(car,0x5a0)==ped)return -1;
 for(int i=0;i<3;++i)if(Read<void*>(car,0x5a8+i*8)==ped)return i;
 return -2;
}
unsigned ClaimedHands(){return claimed;}
void Install(void* h){
 frameLtm=reinterpret_cast<decltype(frameLtm)>(dlsym(h,"_Z13RwFrameGetLTMP7RwFrame"));
 canOpen=reinterpret_cast<decltype(canOpen)>(dlsym(h,"_ZNK8CVehicle15CanPedOpenLocksEPK4CPed"));
 healthy=reinterpret_cast<decltype(healthy)>(dlsym(h,"_ZN13CCarEnterExit12IsPedHealthyERK4CPed"));
 canStepOut=reinterpret_cast<decltype(canStepOut)>(dlsym(h,"_ZNK8CVehicle16CanPedStepOutCarEb"));
}
bool ConsumesMove(){return started>0||Now()<flashUntil;}
void Update(bool usable,std::uint64_t epoch){
 passengerreaction::Update(usable);
 const double now=Now();visible=false;icons={};icons.epoch=epoch;claimed=0;
 xr::InputState input{};xr::GetInput(input);
 float x{},y{};locomotion::MapGameplaySticks(input.leftStick[0],input.leftStick[1],input.rightStick[0],input.rightStick[1],&x,&y,nullptr,nullptr);
 if(std::hypot(x,y)<.25f)released=true;
 void* ped=g.FindPlayerPed?g.FindPlayerPed(-1):nullptr;
 void* pad=g.CPad_GetPad?g.CPad_GetPad(0):nullptr;
 const bool editing=driving::GetVehicleDebugMode()!=0;
 float o[3]{},f[3]{};
 const bool valid=usable&&epoch==trackingEpoch&&now-last<.15&&frameLtm&&canOpen&&healthy&&ped&&pad&&
 !Read<std::uint16_t>(pad,0x110)&&!Read<std::uint8_t>(pad,0x12d)&&healthy(ped)&&
 (editing||!driving::GetVehicleDebugMode())&&vrcam::GetHeadWorldRay(o,f);
 trackingEpoch=epoch;last=now;
 if(!valid){target=nullptr;started=0;flashUntil=0;released=false;EndEdit();return;}
 if(now<flashUntil){visible=true;marker.highlight=float((flashUntil-now)/.2);icons.markers[icons.count++]=marker;return;}
 void* pool=g.CPools_ms_pVehiclePool?*g.CPools_ms_pVehiclePool:nullptr;
 if(!pool||Read<int>(pool,0x10)!=110){target=nullptr;started=0;EndEdit();return;}
 auto objects=Read<char*>(pool,0);auto flags=Read<std::uint8_t*>(pool,8);
 if(!objects||!flags){started=0;EndEdit();return;}
 void* seated=g.FindPlayerVehicle(-1,false);const int currentSeat=SeatIndex(seated,ped);
 const Vec eye{o[0],o[1],o[2]};
 Vec forward{f[0],f[1],0};float length=Length(forward);
 if(length<.1f){target=nullptr;started=0;EndEdit();return;}forward=forward*(1/length);
 const Vec right{forward.y,-forward.x,0};const Vec move=right*x+forward*y;
 xr::HandPose hands[2]{};if(editing)xr::GetHandPoses(hands);
 bool editFresh[2]{};
 for(int h=0;h<2;++h){if(!editing||!hands[h].valid)editLatch[h]=true;else if(hands[h].trigger<.35f)editLatch[h]=false;else if(hands[h].trigger>.55f&&!editLatch[h]){editLatch[h]=true;editFresh[h]=true;}}
 void* bestCar=nullptr;int bestSeat=-1,bestIcon=-1;std::uint8_t bestGeneration{};Vec point{};float bestDirection=-1;float bestDistance=100;
 bool editContinued=false;
 for(int slot=0;slot<110;++slot){
  if(flags[slot]&0x80)continue;
  void* car=objects+slot*0xc68;
  if(seated&&seated!=car)continue;
  if((Read<std::uint8_t>(car,0x5a)&7)!=2||!g.CVehicle_GetVehicleAppearance||g.CVehicle_GetVehicleAppearance(car)!=1||!canOpen(car,ped))continue;
  const Vec speed=Read<Vec>(car,0x68);if(!Finite(speed)||(!seated&&Length(speed)>.02f))continue;
  const int model=Read<std::uint16_t>(car,0x32);if(model==431||model==437||model==432||model==539)continue;
  void* matrix=Read<void*>(car,0x18);if(!matrix)continue;
  Basis b{Read<Vec>(matrix,0),Read<Vec>(matrix,0x10),Read<Vec>(matrix,0x20),Read<Vec>(matrix,0x30)};
  if(!b.Valid()||Length(eye-b.position)>4.5f)continue;
  for(int index=0;index<4;++index){
   const int seat=index-1;const bool exit=seated&&seat==currentSeat;
   if(seated&&currentSeat<-1)continue;
   if(!editing&&!exit&&Read<void*>(car,seat<0?0x5a0:0x5a8+seat*8))continue;
   // A row is a pair: no front/rear teleports, including through empty places.
   if(seated&&!exit&&((index<2)!=(currentSeat<1)))continue;
   void* frame=Read<void*>(car,0x800+nodes[index]*8);if(!frame)continue;
   void* doorMatrix=frameLtm(frame);if(!doorMatrix)continue;
   const Vec hinge=b.Local(Read<Vec>(doorMatrix,0x30));if(!Finite(hinge))continue;
   const float side=(index==0||index==2)?-1.f:1.f;
   float correction[3]{};driving::GetEntryCalibration(model,index,correction);
   Vec local={hinge.x*.5f+correction[0],hinge.y-.35f+correction[1],hinge.z+.2f+correction[2]};
   Vec p=b.Point(local);
   if(!seated&&!editing){
    if(!DoorOpen(car,index)||Dot(eye-b.position,b.right)*side<.2f)continue;
    if(Length(eye-b.Point({hinge.x,hinge.y-.35f,hinge.z}))>1.5f)continue;
   }
   if(exit&&!editing){
    if(Length(speed)>.02f||!DoorOpen(car,index)||!canStepOut||!canStepOut(car,false))continue;
    p=b.Point({hinge.x+side*.45f,hinge.y-.35f,hinge.z+.35f});
    if(!ClearExit(eye,p,car))continue;
   }
   if(Length(p-eye)>2.5f)continue;
   if(editing){
    for(int h=0;h<2;++h){
     float w[3]{};if(!hands[h].valid||!vrcam::TrackingPointToWorld(hands[h].gripPos,w))continue;
     Vec hand=b.Local({w[0],w[1],w[2]});
     if(editHand<0&&editFresh[h]&&Length(hand-local)<.23f){editHand=h;editModel=model;editSeat=index;editStart=hand;editBase={correction[0],correction[1],correction[2]};target=car;generation=flags[slot];}
     if(editHand==h&&editModel==model&&editSeat==index&&target==car&&generation==flags[slot]){
      editContinued=true;claimed=1u<<h;
      Vec value=editBase+hand-editStart;const float c[]={value.x,value.y,value.z};driving::SetEntryCalibration(model,index,c);p=b.Point(local+(value-Vec{correction[0],correction[1],correction[2]}));
      if(hands[h].trigger<.35f)EndEdit();
     }
    }
   }
   Marker m{p,forward,{0,0,1}};m.seatIcon=exit&&!editing?3:(seat<0?1:2);
   if(icons.count>=icons.kCapacity)continue;
   const int icon=icons.count;icons.markers[icons.count++]=m;
   Vec delta=p-eye;delta.z=0;const float distance=Length(delta);
   const float direction=distance>.05f?Dot(move,delta)/distance:-1;
   if(direction>bestDirection+.01f||(std::abs(direction-bestDirection)<=.01f&&distance<bestDistance)){
    bestDirection=direction;bestDistance=distance;bestCar=car;bestSeat=exit?3:seat;point=p;bestGeneration=flags[slot];bestIcon=icon;
   }
  }
 }
 if(editing){started=0;if(editHand>=0&&!editContinued)EndEdit();visible=icons.count>0;return;}
 if(!released||!bestCar){target=nullptr;started=0;return;}
 if(target!=bestCar||targetSeat!=bestSeat||generation!=bestGeneration){started=0;target=bestCar;targetSeat=bestSeat;generation=bestGeneration;}
 const bool holding=std::isfinite(x)&&std::isfinite(y)&&Length(move)>.65f&&bestDirection>.55f;
 if(!holding)started=0;else if(!started)started=now;
 marker=icons.markers[bestIcon];marker.progress=started?std::clamp(float((now-started)/(bestSeat==3?1.5:1.0)),0.f,1.f):0;
 icons.markers[bestIcon]=marker;visible=true;
 if(marker.progress>=1){
  bool success=false;
  if(bestSeat==3){vrcam::RequestVehicleExit();success=true;}
  else success=boatentry::SeatPlayer(bestCar,bestSeat);
  if(success){marker.highlight=1;flashUntil=now+.2;xr::TriggerHaptic(0,.3f,0,35);}
  released=false;started=0;
 }
}
void Append(vehicledoors::MarkerFrame& frame){for(int i=0;i<icons.count&&frame.count<frame.kCapacity;++i)frame.markers[frame.count++]=icons.markers[i];}
}
