#include "PassengerReaction.h"
#include "VehicleEntry.h"
#include "VehicleDoorMotion.h"
#include "PlayerLife.h"
#include "Symbols.h"
#include <chrono>
#include <cstring>
#include <dlfcn.h>
namespace savr::passengerreaction {
namespace {
template<class T>T Read(void* p,int off){T v{};std::memcpy(&v,static_cast<char*>(p)+off,sizeof(v));return v;}
void* (*allocate)(std::size_t){};
void (*setTask)(void*,void*,int,bool){};
void (*leaveCtor)(void*,void*,int,int,bool,bool){};
void (*dragCtor)(void*,void*,int){};
void (*fleeCtor)(void*,void*,bool,float,int,int,float){};
void (*meleeCtor)(void*,void*){};
void (*registerRef)(void*,void**){};
void (*cleanRef)(void*,void**){};
void* (*getGroup)(const void*){};
unsigned (*pedFlag)(int){};
struct Reaction {void* driver{};void* car{};void* player{};void* task{};int phase{},seat{-2};bool flee{};double deadline{};} state;
double Now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void Clear(){for(void** p:{&state.driver,&state.car,&state.player})if(*p)cleanRef(*p,p);state={};}
void* Manager(void* ped){void* i=Read<void*>(ped,0x538);return i?static_cast<char*>(i)+8:nullptr;}
bool Friendly(void* driver,void* player){
 void* group=getGroup(driver);if(group&&group==getGroup(player))return true;
 const unsigned mask=pedFlag(Read<int>(player,0x720));
 // CAcquaintance respect/like bitsets, as used by the native acquaintance scanner.
 return ((Read<unsigned>(driver,0x63c)|Read<unsigned>(driver,0x640))&mask)!=0;
}
}
void Install(void* h){
#define LOAD(v,n) v=reinterpret_cast<decltype(v)>(dlsym(h,n))
 LOAD(allocate,"_ZN5CTasknwEm");LOAD(setTask,"_ZN12CTaskManager7SetTaskEP5CTaskib");
 LOAD(leaveCtor,"_ZN20CTaskComplexLeaveCarC1EP8CVehicleiibb");
 LOAD(dragCtor,"_ZN26CTaskComplexDragPedFromCarC1EP4CPedi");
 LOAD(fleeCtor,"_ZN27CTaskComplexSmartFleeEntityC1EP7CEntitybfiif");
 LOAD(meleeCtor,"_ZN30CTaskComplexKillPedOnFootMeleeC1EP4CPed");
 LOAD(registerRef,"_ZN7CEntity17RegisterReferenceEPPS_");LOAD(cleanRef,"_ZN7CEntity19CleanUpOldReferenceEPPS_");
 LOAD(getGroup,"_ZN10CPedGroups12GetPedsGroupEPK4CPed");LOAD(pedFlag,"_ZN8CPedType10GetPedFlagEi");
#undef LOAD
}
void OnBoard(void* car,void* player){
 if(!allocate||!setTask||!leaveCtor||!dragCtor||!fleeCtor||!meleeCtor||!registerRef||!cleanRef||!getGroup||!pedFlag)return;
 void* driver=Read<void*>(car,0x5a0);
 if(!driver||driver==player||playerlife::Dead(driver)||Read<unsigned char>(driver,0x548)!=1||Friendly(driver,player))return;
 void* manager=Manager(driver);if(!manager)return;
 void* stats=Read<void*>(driver,0x728);if(!stats)return;
 void* task=allocate(0x50);if(!task)return;
 Clear();state.driver=driver;state.car=car;state.player=player;
 for(void** p:{&state.driver,&state.car,&state.player})registerRef(*p,p);
 // Retail CPedStats: fear +0x24, temper +0x25, populated by LoadPedStats.
 state.flee=Read<unsigned char>(stats,0x24)>Read<unsigned char>(stats,0x25);
 state.phase=1;state.deadline=Now()+25;state.task=task;
 leaveCtor(task,car,0,0,true,false);setTask(manager,task,3,false);
}
void Update(bool usable){
 if(!state.phase)return;
 if(!state.driver||!state.car||!state.player||playerlife::Dead(state.driver)||playerlife::Dead(state.player)||Now()>state.deadline){Clear();return;}
 if(!usable)return;
 if(Read<unsigned char>(state.driver,0x548)!=1){Clear();return;}
 void* manager=Manager(state.driver);if(!manager){Clear();return;}
 void* active=Read<void*>(manager,3*8);
 if(active&&active!=state.task){Clear();return;} // a new script/event owns the actor
 const int playerSeat=vehicleentry::SeatIndex(state.car,state.player);
 if(state.phase==1){
  if(vehicleentry::SeatIndex(state.car,state.driver)>=-1)return;
  if(state.flee){
   void* task=allocate(0x50);if(!task){Clear();return;}
   fleeCtor(task,state.player,true,60.f,20000,1000,1.f);setTask(manager,task,3,false);Clear();return;
  }
  state.phase=2;state.seat=-3;
 }
 if(playerSeat==-2){
  void* task=allocate(0x48);if(task){meleeCtor(task,state.player);setTask(manager,task,3,false);}Clear();return;
 }
 // Follow the player's new door when they change seats while the driver walks around.
 if(state.seat!=playerSeat){
  void* task=allocate(0x78);if(!task){Clear();return;}
  dragCtor(task,state.player,6);state.task=task;state.seat=playerSeat;setTask(manager,task,3,false);
 }
}
}
