#include "VehicleExit.h"
#include "VehicleEntry.h"
#include "VehicleTestArea.h"
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
#undef LOAD
}
bool EmergencyAvailable(void* car,int seat){
 if(!car||seat<-1||seat>2||!roomToLeave||!vehicleentry::ExitObstructed(car,seat))return false;
 constexpr int nodes[]={10,8,11,9};
 const int index=seat+1;
 if(roomToLeave(car,nodes[index],nullptr))return false;
 const int opposite=(index^1)-1;
 const bool occupied=Read<void*>(car,opposite<0?0x5a0:0x5a8+opposite*8)!=nullptr;
 return occupied||!roomToLeave(car,nodes[index^1],nullptr);
}
bool TryExit(void* car,int seat,Vec hint,bool emergency){
 if(!car||seat<-1||seat>2||!Finite(hint)||!outCtor||!outProcess||!outDtor||!ground||!worldRemove||!worldAdd||!setTask||!healthy||!canStepOut||!g.FindPlayerPed||!g.FindPlayerVehicle||!g.CPad_GetPad||!g.CWorld_TestSphereAgainstWorld){
  LOGI("[vehicle-exit] missing dependency or invalid request seat=%d",seat);return false;
 }
 void* ped=g.FindPlayerPed(-1);void* pad=g.CPad_GetPad(0);
 if(!ped||!pad||g.FindPlayerVehicle(-1,false)!=car||vehicleentry::SeatIndex(car,ped)!=seat||Read<std::uint16_t>(pad,0x110)||Read<std::uint8_t>(pad,0x12d)||!healthy(ped)||!canStepOut(car,false)){
  LOGI("[vehicle-exit] player, seat or controls no longer eligible");return false;
 }
 const Vec speed=Read<Vec>(car,0x68);if(!Finite(speed)||Length(speed)>.02f)return false;
 const bool bike=Read<std::uint16_t>(car,0x32)==471||
     (g.CVehicle_GetVehicleAppearance&&g.CVehicle_GetVehicleAppearance(car)==2);
 const int door=seat+3;
 if(emergency&&!EmergencyAvailable(car,seat))return false;
 if(!bike&&!emergency){
 const float open=Read<float>(car,0x770+door*0x18),angle=Read<float>(car,0x77c+door*0x18);
 if(Read<std::uint8_t>(car,0x758+9+door)!=4&&(!std::isfinite(open)||!std::isfinite(angle)||std::abs(open)<.3f||angle/open<.45f))return false;
 }
 void* intelligence=Read<void*>(ped,0x538);if(!intelligence)return false;
 // The marker is just outside the door hinge, but body spheres can overlap a
 // wide car there. Search a short distance outward only when that car blocks
 // the initial point; never skip another obstacle or an obstructed path.
 void* matrix=Read<void*>(car,0x18);
 if(!matrix&&!emergency)return false;
 Basis basis=matrix?Basis{Read<Vec>(matrix,0),Read<Vec>(matrix,0x10),Read<Vec>(matrix,0x20),Read<Vec>(matrix,0x30)}:Basis{};
 if(!basis.Valid()&&!emergency)return false;
 const float side=Dot(hint-basis.position,basis.right)<0?-1.f:1.f;
 GameSymbols::Vec3 destination{};
 bool found=false;
 for(float distance:{0.f,.25f,.5f,.75f}) {
  if(emergency)break;
  const Vec candidate=hint+basis.right*(side*distance);
  GameSymbols::Vec3 point{candidate.x,candidate.y,candidate.z};
  float floorZ{};
  if(testarea::FindExitGround(point.x,point.y,point.z,&floorZ))point.z=floorZ;
  else if(!ground(&point))break;
  if(!Finite({point.x,point.y,point.z})||std::abs(point.z-hint.z)>1.5f)break;
  bool carBlocked=false,otherBlocked=false;
  for(float height:{-.35f,.35f}) {
   const GameSymbols::Vec3 centre{point.x,point.y,point.z+height};
   void* hit=g.CWorld_TestSphereAgainstWorld(centre,.3f,ped,true,true,true,true,true,false);
   if(hit==car)carBlocked=true;else if(hit)otherBlocked=true;
  }
  if(otherBlocked)break;
  if(carBlocked)continue;
  if(distance>0) {
   if(!g.CWorld_ProcessLineOfSight||!g.CWorld_pIgnoreEntity)break;
   void* previous=*g.CWorld_pIgnoreEntity;*g.CWorld_pIgnoreEntity=car;
   bool blocked=false;
   for(float height:{-.35f,.35f}) {
    const GameSymbols::Vec3 from{hint.x,hint.y,point.z+height},to{point.x,point.y,point.z+height};
    alignas(8) unsigned char col[0x2c]{};void* hit{};
    if(g.CWorld_ProcessLineOfSight(&from,&to,col,&hit,true,true,true,true,true,false,false,false)) {blocked=true;break;}
   }
   *g.CWorld_pIgnoreEntity=previous;
   if(blocked)break;
  }
  destination=point;found=true;break;
 }
 if(!found&&!emergency){LOGI("[vehicle-exit] no safe placement near open door");return false;}
 void* manager=static_cast<char*>(intelligence)+8;
 // Remove only a driving primary task. Mission tasks remain owned by the game.
 if(void* task=Read<void*>(manager,3*8)){
  void* vtable=Read<void*>(task,0);
  const auto type=Read<int(*)(void*)>(vtable,0x28)(task);
  if(type==725||type==711)setTask(manager,nullptr,3,false);
 }
 // Retail ARM64 task is 0x28 bytes; +0x1f skips the engine's perimeter
 // position search for ordinary exits. Emergency exits use that search.
 // ProcessPed detaches the occupant, removes sit animations
 // and installs PlayerOnFoot immediately, without any leave/close-door task.
 constexpr int nodes[]={10,8,11,9};
 alignas(16) std::uint8_t task[0x28]{};
 outCtor(task,car,nodes[seat+1],false);task[0x1f]=emergency?0:1;
 const bool processed=outProcess(task,ped);outDtor(task);
 if(!processed||vehicleentry::SeatIndex(car,ped)!=-2){LOGI("[vehicle-exit] native detach failed (processed=%d)",processed);return false;}
 if(emergency)return true;
 worldRemove(ped);
 Vec position{destination.x,destination.y,destination.z};
 matrix=Read<void*>(ped,0x18);
 std::memcpy(static_cast<char*>(matrix?matrix:ped)+(matrix?0x30:8),&position,sizeof(position));
 const Vec zero{};std::memcpy(static_cast<char*>(ped)+0x68,&zero,sizeof(zero));
 worldAdd(ped);
 return true;
}
}
