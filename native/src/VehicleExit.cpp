#include "VehicleExit.h"
#include "VehicleEntry.h"
#include "Symbols.h"
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
#undef LOAD
}
bool TryExit(void* car,int seat,Vec hint){
 if(!car||seat<-1||seat>2||!Finite(hint)||!outCtor||!outProcess||!outDtor||!ground||!worldRemove||!worldAdd||!setTask||!healthy||!canStepOut||!g.FindPlayerPed||!g.FindPlayerVehicle||!g.CPad_GetPad||!g.CWorld_TestSphereAgainstWorld)return false;
 void* ped=g.FindPlayerPed(-1);void* pad=g.CPad_GetPad(0);
 if(!ped||!pad||g.FindPlayerVehicle(-1,false)!=car||vehicleentry::SeatIndex(car,ped)!=seat||Read<std::uint16_t>(pad,0x110)||Read<std::uint8_t>(pad,0x12d)||!healthy(ped)||!canStepOut(car,false))return false;
 const Vec speed=Read<Vec>(car,0x68);if(!Finite(speed)||Length(speed)>.02f)return false;
 const int door=seat+3;
 const float open=Read<float>(car,0x770+door*0x18),angle=Read<float>(car,0x77c+door*0x18);
 if(Read<std::uint8_t>(car,0x758+9+door)!=4&&(!std::isfinite(open)||!std::isfinite(angle)||std::abs(open)<.3f||angle/open<.45f))return false;
 void* intelligence=Read<void*>(ped,0x538);if(!intelligence)return false;
 GameSymbols::Vec3 destination{hint.x,hint.y,hint.z};
 if(!ground(&destination)||!Finite({destination.x,destination.y,destination.z})||std::abs(destination.z-hint.z)>1.5f)return false;
 for(float height:{-.35f,.35f}){
  const GameSymbols::Vec3 centre{destination.x,destination.y,destination.z+height};
  if(g.CWorld_TestSphereAgainstWorld(centre,.3f,ped,true,true,true,true,true,false))return false;
 }
 void* manager=static_cast<char*>(intelligence)+8;
 // Remove only a driving primary task. Mission tasks remain owned by the game.
 if(void* task=Read<void*>(manager,3*8)){
  void* vtable=Read<void*>(task,0);
  const auto type=Read<int(*)(void*)>(vtable,0x28)(task);
  if(type==725||type==711)setTask(manager,nullptr,3,false);
 }
 // Retail ARM64 task is 0x28 bytes; +0x1f skips the engine's perimeter
 // position search. ProcessPed detaches the occupant, removes sit animations
 // and installs PlayerOnFoot immediately, without any leave/close-door task.
 constexpr int nodes[]={10,8,11,9};
 alignas(16) std::uint8_t task[0x28]{};
 outCtor(task,car,nodes[seat+1],false);task[0x1f]=1;
 const bool processed=outProcess(task,ped);outDtor(task);
 if(!processed||vehicleentry::SeatIndex(car,ped)!=-2)return false;
 worldRemove(ped);
 Vec position{destination.x,destination.y,destination.z};
 void* matrix=Read<void*>(ped,0x18);
 std::memcpy(static_cast<char*>(matrix?matrix:ped)+(matrix?0x30:8),&position,sizeof(position));
 const Vec zero{};std::memcpy(static_cast<char*>(ped)+0x68,&zero,sizeof(zero));
 worldAdd(ped);
 return true;
}
}
