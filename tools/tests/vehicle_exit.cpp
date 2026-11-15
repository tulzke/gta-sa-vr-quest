#include "VehicleExit.h"
#include "VehicleAccess.h"
#include "VehicleEntry.h"
#include "Symbols.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
namespace savr { GameSymbols g{};
namespace testarea { bool active{}; bool FindExitGround(float,float,float,float* z){if(!active)return false;*z=1;return true;} }
namespace vehicleentry { int SeatIndex(void* car,void* ped){void* p{};std::memcpy(&p,static_cast<char*>(car)+0x5a0,8);if(p==ped)return -1;for(int i=0;i<3;++i){std::memcpy(&p,static_cast<char*>(car)+0x5a8+i*8,8);if(p==ped)return i;}return -2;} }
}
#define LOGI(...) ((void)0)
// PRODUCTION_EXIT
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace savr;using namespace vehicleexit;using vehicledoors::Vec;
char car[0xc68]{},ped[0x800]{},pad[0x140]{},matrix[0x40]{},carMatrix[0x40]{},intelligence[0x80]{},drive[8]{},obstacle[8]{};
void* vtable[6]{};void* current{};void* ignored{};
int selected{},expectedNode{},ctorCalls{},processCalls{},dtorCalls{},removes{},adds{},clears{},taskType{},groundCalls{},emergencyCalls{};
bool terrain{},ownRoom{},oppositeRoom{},stepAllowed{},doorBusy{},otherBusy{},emergencyTask{};
float expectedX{},doorZ{};bool corrected{};
template<class T>void Put(void* p,int off,T v){std::memcpy(static_cast<char*>(p)+off,&v,sizeof(v));}
void Setup(int seat){
 std::memset(car,0,sizeof(car));std::memset(ped,0,sizeof(ped));std::memset(pad,0,sizeof(pad));std::memset(intelligence,0,sizeof(intelligence));
 current=car;selected=seat;expectedNode=nodes[seat+1];ctorCalls=processCalls=dtorCalls=removes=adds=clears=groundCalls=emergencyCalls=0;
 testarea::active=false;terrain=ownRoom=stepAllowed=true;oppositeRoom=doorBusy=otherBusy=emergencyTask=corrected=false;
 ignored=obstacle;expectedX=2;doorZ=1;taskType=725;
 Put(car,seat<0?0x5a0:0x5a8+seat*8,static_cast<void*>(ped));Put(car,0x770+(seat+3)*0x18,1.f);Put(car,0x77c+(seat+3)*0x18,.8f);
 Put(car,0x18,static_cast<void*>(carMatrix));Put(carMatrix,0,Vec{1,0,0});Put(carMatrix,0x10,Vec{0,1,0});Put(carMatrix,0x20,Vec{0,0,1});Put(carMatrix,0x30,Vec{});
 Put(ped,0x18,static_cast<void*>(matrix));Put(matrix,0x30,Vec{});Put(ped,0x538,static_cast<void*>(intelligence));
 vtable[5]=reinterpret_cast<void*>(+[](void*){return taskType;});Put(drive,0,static_cast<void*>(vtable));Put(intelligence,8+3*8,static_cast<void*>(drive));
 g.FindPlayerPed=[](int)->void*{return ped;};g.FindPlayerVehicle=[](int,bool)->void*{return current;};g.CPad_GetPad=[](int)->void*{return pad;};
 g.CVehicle_GetVehicleAppearance=[](const void*){return 1;};g.CWorld_pIgnoreEntity=&ignored;
 // Eligibility must work without our former custom collision queries.
 g.CWorld_TestSphereAgainstWorld=nullptr;g.CWorld_ProcessLineOfSight=nullptr;
 outCtor=[](void* t,void* c,int node,bool){CHECK(c==car&&node==expectedNode);++ctorCalls;std::memset(t,0,0x28);};
 outProcess=[](void* t,void* p){
  CHECK(p==ped&&Read<std::uint8_t>(t,0x1f)==0&&removes==1&&adds==0);
  if(!emergencyTask)CHECK(Read<Vec>(matrix,0x30).x==expectedX&&Read<Vec>(matrix,0x30).z==1);
  // Preserve any final position selected by the native correction.
  if(corrected)Put(matrix,0x30,Vec{3,0,1});
  ignored=nullptr;++processCalls;Put(car,selected<0?0x5a0:0x5a8+selected*8,static_cast<void*>(nullptr));current=nullptr;return true;
 };
 outDtor=[](void*){++dtorCalls;};
 doorPosition=[](const void* c,int node){CHECK(c==car&&node==expectedNode);return GameSymbols::Vec3{expectedX,0,doorZ};};
 ground=[](GameSymbols::Vec3* p){CHECK(ignored==car);++groundCalls;p->z=1;return terrain;};
 worldRemove=[](void* p){CHECK(p==ped);++removes;};
 worldAdd=[](void* p){CHECK(p==ped&&ignored==obstacle);++adds;};
 roomToLeave=[](const void*,int node,GameSymbols::Vec3* offset){CHECK(!offset);return node==expectedNode?ownRoom:oppositeRoom;};
 checkDoors=[](const void*,int node,bool& in,bool& out){in=node==expectedNode?doorBusy:otherBusy;out=false;};
 positionOut=[](void* p,int node,void* c,bool fallback){CHECK(p==ped&&c==car&&node==expectedNode&&fallback&&removes==1);++emergencyCalls;Put(matrix,0x30,Vec{4,0,1});return true;};
 setTask=[](void*,void* t,int slot,bool){CHECK(!t&&slot==3);++clears;};healthy=[](const void*){return true;};canStepOut=[](const void*,bool flag){CHECK(!flag);return stepAllowed;};
}
int main(){
 for(int seat=-1;seat<3;++seat){Setup(seat);CHECK(Check(car,seat,{2,0,1})==Mode::Normal);CHECK(TryExit(car,seat,{2,0,1}));CHECK(ctorCalls==1&&processCalls==1&&dtorCalls==1&&removes==1&&adds==1&&clears==1);CHECK(Read<float>(car,0x77c+(seat+3)*0x18)==.8f);CHECK(!TryExit(car,seat,{2,0,1})&&processCalls==1);}
 Setup(0);taskType=999;CHECK(TryExit(car,0,{2,0,1})&&clears==0);
 Setup(-1);Put(car,0x77c+2*0x18,0.f);CHECK(Check(car,-1,{2,0,1})==Mode::Unavailable&&!TryExit(car,-1,{2,0,1},true));
 Setup(-1);Put(pad,0x110,std::uint16_t(1));CHECK(!TryExit(car,-1,{2,0,1})&&ctorCalls==0);
 Setup(-1);stepAllowed=false;CHECK(Check(car,-1,{2,0,1})==Mode::Unavailable&&!TryExit(car,-1,{2,0,1}));
 Setup(-1);doorBusy=true;CHECK(Check(car,-1,{2,0,1})==Mode::Unavailable);
 Setup(-1);ownRoom=false;oppositeRoom=true;CHECK(Check(car,-1,{2,0,1})==Mode::Unavailable);
 Put(car,0x5a8,static_cast<void*>(obstacle));CHECK(Check(car,-1,{2,0,1})==Mode::Emergency);
 CHECK(!TryExit(car,-1,{2,0,1})&&clears==0);emergencyTask=true;
 CHECK(TryExit(car,-1,{2,0,1},true)&&groundCalls==0&&emergencyCalls==1&&Read<Vec>(matrix,0x30).x==4);
 Setup(-1);ownRoom=false;CHECK(EmergencyAvailable(car,-1));emergencyTask=true;CHECK(TryExit(car,-1,{2,0,1},true));
 Setup(-1);CHECK(!TryExit(car,-1,{2,0,1},true)&&processCalls==0);
 Setup(-1);ownRoom=false;oppositeRoom=true;otherBusy=true;CHECK(Check(car,-1,{2,0,1})==Mode::Emergency);
 Setup(-1);terrain=false;testarea::active=true;CHECK(TryExit(car,-1,{2,0,1})&&groundCalls==0);
 Setup(-1);terrain=false;CHECK(!TryExit(car,-1,{2,0,1})&&ctorCalls==0&&clears==0&&removes==0&&ignored==obstacle);
 for(int model:{403,514,515}){Setup(-1);Put(car,0x32,std::uint16_t(model));doorZ=3.6f;CHECK(TryExit(car,-1,{2,0,3.8f}));}
 Setup(-1);corrected=true;CHECK(TryExit(car,-1,{99,0,9})&&Read<Vec>(matrix,0x30).x==3);
 // Native kinematics owns the decision, including vertical speed allowances.
 Setup(-1);Put(car,0x68,Vec{0,0,.03f});CHECK(TryExit(car,-1,{2,0,1}));
 for(int seat:{-1,0}){Setup(seat);g.CVehicle_GetVehicleAppearance=[](const void*){return 2;};Put(car,0x770+(seat+3)*0x18,0.f);CHECK(TryExit(car,seat,{2,0,1}));}
 Setup(-1);Put(car,0x32,std::uint16_t(471));Put(car,0x770+2*0x18,0.f);CHECK(TryExit(car,-1,{2,0,1}));
 for(int model:{531,424,568,571,457,485,486,530,572})for(int side:{-1,1}){
  Setup(-1);Put(car,0x32,std::uint16_t(model));Put(car,0x77c+2*0x18,0.f);expectedNode=side<0?10:8;expectedX=side*2.f;CHECK(TryExit(car,-1,{side*2.f,0,1}));
 }
 Setup(-1);Put(car,0x32,std::uint16_t(424));Put(car,0x5a8,static_cast<void*>(obstacle));expectedNode=8;CHECK(Check(car,-1,{2,0,1})==Mode::Unavailable);
 Setup(-1);Put(car,0x758+9+2,std::uint8_t(4));Put(car,0x77c+2*0x18,0.f);CHECK(TryExit(car,-1,{2,0,1}));
 std::puts("native exit: four seats, engine eligibility/placement, high cabs, doors, emergency, revalidation and unchanged door angle passed");
}
