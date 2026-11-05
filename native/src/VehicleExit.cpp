#include "VehicleExit.h"
#include "VehicleEntry.h"
#include "VehicleTestArea.h"
#include "VehicleAccess.h"
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
 if(room&&(Bike(car)||vehicleaccess::DoorOpen(car,seat+3)))return Mode::Normal;
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
  destination=doorPosition(car,node);
  if(!Finite({destination.x,destination.y,destination.z}))return false;
  float floorZ{};
  if(testarea::FindExitGround(destination.x,destination.y,destination.z,&floorZ))destination.z=floorZ;
  else {
   void* previous=*g.CWorld_pIgnoreEntity;*g.CWorld_pIgnoreEntity=car;
   const bool found=ground(&destination);*g.CWorld_pIgnoreEntity=previous;
   if(!found){LOGI("[vehicle-exit] native ground lookup failed seat=%d",seat);return false;}
  }
  if(!Finite({destination.x,destination.y,destination.z}))return false;
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
