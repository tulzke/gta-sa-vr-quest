#include "VehicleExit.h"
#include "VehicleEntry.h"
#include "VehicleTestArea.h"
#include "VehicleAccess.h"
#include "Driving.h"
#include "Symbols.h"
#include "Log.h"
#include <cstring>
#include <dlfcn.h>
namespace savr::vehicleexit {
namespace {
using namespace vehicledoors;
template<class T>T Read(void* p,int off){T v{};std::memcpy(&v,static_cast<char*>(p)+off,sizeof(v));return v;}
void (*outCtor)(void*,void*,int,bool){};
bool (*outProcess)(void*,void*){};
void (*outDtor)(void*){};
bool (*ground)(GameSymbols::Vec3*){};
void (*worldRemove)(void*){};
void (*worldAdd)(void*){};
void (*setTask)(void*,void*,int,bool){};
bool (*healthy)(const void*){};
bool (*canStepOut)(const void*,bool){};
bool (*roomToLeave)(const void*,int,GameSymbols::Vec3*){};
GameSymbols::Vec3 (*doorPosition)(const void*,int){};
void (*checkDoors)(const void*,int,bool&,bool&){};
bool (*positionOut)(void*,int,void*,bool){};
constexpr int nodes[]={10,8,11,9};
bool DoorsFree(void* car,int node){
 if(!checkDoors)return false;
 bool gettingIn=false,gettingOut=false;
 checkDoors(car,node,gettingIn,gettingOut);
 return !gettingIn&&!gettingOut;
}
bool Bike(void* car){
 return Read<std::uint16_t>(car,0x32)==471||
     (g.CVehicle_GetVehicleAppearance&&g.CVehicle_GetVehicleAppearance(car)==2);
}
int ExitNode(void* car,int seat,Vec hint){
 if(!vehicleaccess::Find(Read<std::uint16_t>(car,0x32)))return nodes[seat+1];
 void* matrix=Read<void*>(car,0x18);if(!matrix)return -1;
 return Dot(hint-Read<Vec>(matrix,0x30),Read<Vec>(matrix,0))<0?10:8;
}
bool Ground(void* car,GameSymbols::Vec3& point){
 if(!Finite({point.x,point.y,point.z})||!ground||!g.CWorld_pIgnoreEntity)return false;
 float floorZ{};
 if(testarea::FindExitGround(point.x,point.y,point.z,&floorZ)){point.z=floorZ;return std::isfinite(floorZ);}
 void* previous=*g.CWorld_pIgnoreEntity;*g.CWorld_pIgnoreEntity=car;
 const bool found=ground(&point);*g.CWorld_pIgnoreEntity=previous;
 return found&&Finite({point.x,point.y,point.z});
}
bool Destination(void* car,int seat,int node,GameSymbols::Vec3& destination){
 destination=doorPosition(car,node);
 const int model=Read<std::uint16_t>(car,0x32);
 float correction[3]{};
 // Special open-vehicle and bike exits keep their existing native placement.
 if(!Bike(car)&&!vehicleaccess::Find(model))driving::GetExitCalibration(model,seat+1,true,correction);
 const bool adjusted=std::abs(correction[0])+std::abs(correction[1])>.0001f;
 if(!adjusted)return Ground(car,destination);
 if(!g.CWorld_TestSphereAgainstWorld||!g.CWorld_ProcessLineOfSight)return false;
 void* matrix=Read<void*>(car,0x18);if(!matrix)return false;
 Basis basis{Read<Vec>(matrix,0),Read<Vec>(matrix,0x10),Read<Vec>(matrix,0x20),Read<Vec>(matrix,0x30)};
 if(!basis.Valid())return false;
 GameSymbols::Vec3 original=destination;
 const Vec offset=basis.Vector({correction[0],correction[1],0});
 destination.x+=offset.x;destination.y+=offset.y;destination.z+=offset.z;
 if(!Ground(car,destination)||!Ground(car,original))return false;
 // Reject drops and steps beyond a normal walking step. The unmodified high-cab
 // path remains native; this limit only governs the optional local adjustment.
 if(std::abs(destination.z-original.z)>.4f)return false;
 // Check a standing capsule (root is ground + 1m), including our own bodywork.
 // Do not skip the car: that would allow an editor target inside its cabin.
 void* previous=*g.CWorld_pIgnoreEntity;*g.CWorld_pIgnoreEntity=nullptr;
 bool clear=true;
 for(float z:{-.65f,-.2f,.25f,.55f}){
  GameSymbols::Vec3 end{destination.x,destination.y,destination.z+z};
  GameSymbols::Vec3 start{original.x,original.y,original.z+z};
  if(g.CWorld_TestSphereAgainstWorld(end,.3f,g.FindPlayerPed(-1),true,true,true,true,true,false)){clear=false;break;}
  // Sweep the capsule cross-section along the short native-door-to-target path.
  const Vec delta{end.x-start.x,end.y-start.y,end.z-start.z};
  const int steps=std::max(1,static_cast<int>(std::ceil(Length(delta)/.1f)));
  for(int step=1;step<steps;++step){
   const float t=float(step)/steps;
   GameSymbols::Vec3 p{start.x+delta.x*t,start.y+delta.y*t,start.z+delta.z*t};
   if(g.CWorld_TestSphereAgainstWorld(p,.3f,g.FindPlayerPed(-1),true,true,true,true,true,false)){clear=false;break;}
  }
  alignas(8) unsigned char collision[0x2c]{};void* hit{};
  if(!clear||g.CWorld_ProcessLineOfSight(&start,&end,collision,&hit,true,true,false,true,true,false,false,false)){clear=false;break;}
 }
 *g.CWorld_pIgnoreEntity=previous;
 return clear;
}
}
void Install(void* h){
#define LOAD(v,n) v=reinterpret_cast<decltype(v)>(dlsym(h,n))
 LOAD(outCtor,"_ZN23CTaskSimpleCarSetPedOutC1EP8CVehicleib");
 LOAD(outProcess,"_ZN23CTaskSimpleCarSetPedOut10ProcessPedEP4CPed");
 LOAD(outDtor,"_ZN23CTaskSimpleCarSetPedOutD1Ev");
 LOAD(ground,"_ZN13CPedPlacement15FindZCoorForPedEP7CVector");
 LOAD(worldRemove,"_ZN6CWorld6RemoveEP7CEntity");LOAD(worldAdd,"_ZN6CWorld3AddEP7CEntity");
 LOAD(setTask,"_ZN12CTaskManager7SetTaskEP5CTaskib");
 LOAD(healthy,"_ZN13CCarEnterExit12IsPedHealthyERK4CPed");
 LOAD(canStepOut,"_ZNK8CVehicle16CanPedStepOutCarEb");
 LOAD(roomToLeave,"_ZN13CCarEnterExit22IsRoomForPedToLeaveCarERK8CVehicleiP7CVector");
 LOAD(doorPosition,"_ZN13CCarEnterExit24GetPositionToOpenCarDoorERK8CVehiclei");
 LOAD(checkDoors,"_ZN37CTaskSimpleCarWaitForDoorNotToBeInUse20CheckDoorsFreeOfPedsERK8CVehicleiRbS3_");
 LOAD(positionOut,"_ZN4CPed25PositionPedOutOfCollisionEiP8CVehicleb");
#undef LOAD
}
bool PlacementPreview(void* car,int seat,Vec& position){
 if(!car||seat<-1||seat>2||!doorPosition)return false;
 GameSymbols::Vec3 destination{};const bool clear=Destination(car,seat,nodes[seat+1],destination);
 position={destination.x,destination.y,destination.z};return clear;
}
bool EmergencyAvailable(void* car,int seat){
 if(!car||seat<-1||seat>2||!roomToLeave||!checkDoors)return false;
 const int index=seat+1;
 if(!DoorsFree(car,nodes[index])||roomToLeave(car,nodes[index],nullptr))return false;
 const int opposite=(index^1)-1;
 const bool occupied=Read<void*>(car,opposite<0?0x5a0:0x5a8+opposite*8)!=nullptr;
 return occupied||!DoorsFree(car,nodes[index^1])||!roomToLeave(car,nodes[index^1],nullptr);
}
Mode Check(void* car,int seat,Vec hint){
 if(!car||seat<-1||seat>2||!Finite(hint)||!outCtor||!outProcess||!outDtor||!ground||!worldRemove||!worldAdd||!setTask||!healthy||!canStepOut||!roomToLeave||!doorPosition||!checkDoors||!g.CWorld_pIgnoreEntity||!g.FindPlayerPed||!g.FindPlayerVehicle||!g.CPad_GetPad)return Mode::Unavailable;
 void* ped=g.FindPlayerPed(-1);void* pad=g.CPad_GetPad(0);
 if(!ped||!pad||g.FindPlayerVehicle(-1,false)!=car||vehicleentry::SeatIndex(car,ped)!=seat||!Read<void*>(ped,0x538)||Read<std::uint16_t>(pad,0x110)||Read<std::uint8_t>(pad,0x12d)||!healthy(ped)||!canStepOut(car,false))return Mode::Unavailable;
 void* matrix=Read<void*>(car,0x18);
 if(!matrix)return Mode::Unavailable;
 Basis basis{Read<Vec>(matrix,0),Read<Vec>(matrix,0x10),Read<Vec>(matrix,0x20),Read<Vec>(matrix,0x30)};
 if(!basis.Valid())return Mode::Unavailable;
 const int node=ExitNode(car,seat,hint);
 if(node<0||!DoorsFree(car,node))return Mode::Unavailable;
 if(const auto* profile=vehicleaccess::Find(Read<std::uint16_t>(car,0x32));profile&&!profile->single){
  const int sideSeat=node==10?-1:0;
  if(sideSeat!=seat&&Read<void*>(car,sideSeat<0?0x5a0:0x5a8))return Mode::Unavailable;
 }
 const bool room=roomToLeave(car,node,nullptr);
 if(room&&(Bike(car)||vehicleaccess::DoorOpen(car,seat+3))){
  float correction[3]{};
  if(!Bike(car)&&!vehicleaccess::Find(Read<std::uint16_t>(car,0x32)))driving::GetExitCalibration(Read<std::uint16_t>(car,0x32),seat+1,true,correction);
  if(std::abs(correction[0])+std::abs(correction[1])>.0001f){GameSymbols::Vec3 destination{};if(!Destination(car,seat,node,destination))return Mode::Unavailable;}
  return Mode::Normal;
 }
 // A closed door alone never enables the native emergency search.
 if(!room&&positionOut&&!Bike(car)&&!vehicleaccess::Find(Read<std::uint16_t>(car,0x32))&&EmergencyAvailable(car,seat))return Mode::Emergency;
 return Mode::Unavailable;
}
bool TryExit(void* car,int seat,Vec hint,bool emergency){
 if(Check(car,seat,hint)!=(emergency?Mode::Emergency:Mode::Normal)){
  LOGI("[vehicle-exit] native eligibility changed seat=%d emergency=%d",seat,emergency);return false;
 }
 void* ped=g.FindPlayerPed(-1);
 const int node=ExitNode(car,seat,hint);
 GameSymbols::Vec3 destination{};
 if(!emergency){
  // CarSetPedOut normally follows the get-out animation. Supply the engine's
  // door position instead of leaving its collision correction inside the seat.
  if(!Destination(car,seat,node,destination)){LOGI("[vehicle-exit] target blocked or no ground seat=%d",seat);return false;}
 }
 void* intelligence=Read<void*>(ped,0x538);
 void* manager=static_cast<char*>(intelligence)+8;
 // Remove only a driving primary task. Mission tasks remain owned by the game.
 if(void* task=Read<void*>(manager,3*8)){
  void* vtable=Read<void*>(task,0);
  const auto type=Read<int(*)(void*)>(vtable,0x28)(task);
  if(type==725||type==711)setTask(manager,nullptr,3,false);
 }
 worldRemove(ped);
 void* previous=*g.CWorld_pIgnoreEntity;
 if(!emergency){
  void* matrix=Read<void*>(ped,0x18);
  const Vec position{destination.x,destination.y,destination.z};
  std::memcpy(static_cast<char*>(matrix?matrix:ped)+(matrix?0x30:8),&position,sizeof(position));
 }else {
  // Unlike the post-animation task correction, this always starts at the
  // requested door and runs the engine's full emergency placement search.
  // Retail CarSetPedOut also proceeds when that best-effort search fails.
  if(!positionOut(ped,node,car,true))LOGI("[vehicle-exit] native emergency search exhausted seat=%d",seat);
 }
 // Keep the constructor's native collision correction enabled (+0x1f = 0).
 // No leave/open/close-door animation task is installed.
 alignas(16) std::uint8_t task[0x28]{};
 outCtor(task,car,node,false);
 const bool processed=outProcess(task,ped);outDtor(task);
 *g.CWorld_pIgnoreEntity=previous;
 worldAdd(ped);
 if(!processed||vehicleentry::SeatIndex(car,ped)!=-2){LOGI("[vehicle-exit] native detach failed (processed=%d)",processed);return false;}
 LOGI("[vehicle-exit] native exit completed seat=%d node=%d emergency=%d",seat,node,emergency);
 return true;
}
}
