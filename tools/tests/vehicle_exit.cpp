#include "VehicleExit.h"
#include "VehicleEntry.h"
#include "Symbols.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
namespace savr { GameSymbols g{};
namespace vehicleentry {int SeatIndex(void* car,void* ped){void* p{};std::memcpy(&p,static_cast<char*>(car)+0x5a0,8);if(p==ped)return -1;for(int i=0;i<3;++i){std::memcpy(&p,static_cast<char*>(car)+0x5a8+i*8,8);if(p==ped)return i;}return -2;}}
}
// PRODUCTION_EXIT
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace savr;using namespace vehicleexit;using vehicledoors::Vec;
char car[0xc68]{},ped[0x800]{},pad[0x140]{},matrix[0x40]{},intelligence[0x80]{},drive[8]{};
void* vtable[6]{};void* current{};int selected{},ctorCalls{},processCalls{},dtorCalls{},removes{},adds{},clears{},taskType{};bool wall{},terrain=true;
template<class T>void Put(void* p,int off,T v){std::memcpy(static_cast<char*>(p)+off,&v,sizeof(v));}
void Setup(int seat){
 std::memset(car,0,sizeof(car));std::memset(ped,0,sizeof(ped));std::memset(pad,0,sizeof(pad));std::memset(intelligence,0,sizeof(intelligence));
 current=car;selected=seat;ctorCalls=processCalls=dtorCalls=removes=adds=clears=0;wall=false;terrain=true;taskType=725;
 Put(car,seat<0?0x5a0:0x5a8+seat*8,static_cast<void*>(ped));Put(car,0x770+(seat+3)*0x18,1.f);Put(car,0x77c+(seat+3)*0x18,.8f);
 Put(ped,0x18,static_cast<void*>(matrix));Put(ped,0x538,static_cast<void*>(intelligence));
 vtable[5]=reinterpret_cast<void*>(+[](void*){return taskType;});Put(drive,0,static_cast<void*>(vtable));Put(intelligence,8+3*8,static_cast<void*>(drive));
 g.FindPlayerPed=[](int)->void*{return ped;};g.FindPlayerVehicle=[](int,bool)->void*{return current;};g.CPad_GetPad=[](int)->void*{return pad;};
 g.CWorld_TestSphereAgainstWorld=[](GameSymbols::Vec3,float,void*,bool,bool,bool,bool,bool,bool)->void*{return wall?car:nullptr;};
 outCtor=[](void* t,void* c,int node,bool){CHECK(c==car);const int nodes[]={10,8,11,9};CHECK(node==nodes[selected+1]);++ctorCalls;std::memset(t,0,0x28);};
 outProcess=[](void* t,void* p){CHECK(p==ped&&Read<std::uint8_t>(t,0x1f)==1);++processCalls;Put(car,selected<0?0x5a0:0x5a8+selected*8,static_cast<void*>(nullptr));current=nullptr;return true;};
 outDtor=[](void*){++dtorCalls;};ground=[](GameSymbols::Vec3* p){p->z=1;return terrain;};
 worldRemove=[](void* p){CHECK(p==ped);++removes;};worldAdd=[](void* p){CHECK(p==ped);++adds;CHECK(Read<Vec>(matrix,0x30).x==2&&Read<Vec>(matrix,0x30).z==1);};
 setTask=[](void*,void* t,int slot,bool){CHECK(!t&&slot==3);++clears;};healthy=[](const void*){return true;};canStepOut=[](const void*,bool){return true;};
}
int main(){
 for(int seat=-1;seat<3;++seat){Setup(seat);CHECK(TryExit(car,seat,{2,0,1}));CHECK(ctorCalls==1&&processCalls==1&&dtorCalls==1&&removes==1&&adds==1&&clears==1);CHECK(Read<float>(car,0x77c+(seat+3)*0x18)==.8f);CHECK(!TryExit(car,seat,{2,0,1})&&processCalls==1);}
 Setup(0);taskType=999;CHECK(TryExit(car,0,{2,0,1})&&clears==0); // mission primary preserved
 Setup(-1);Put(car,0x77c+2*0x18,0.f);CHECK(!TryExit(car,-1,{2,0,1})&&ctorCalls==0);
 Setup(-1);Put(pad,0x110,std::uint16_t(1));CHECK(!TryExit(car,-1,{2,0,1})&&ctorCalls==0);
 Setup(-1);wall=true;CHECK(!TryExit(car,-1,{2,0,1})&&ctorCalls==0);
 Setup(-1);terrain=false;CHECK(!TryExit(car,-1,{2,0,1})&&ctorCalls==0);
 Setup(-1);Put(car,0x68,Vec{1,0,0});CHECK(!TryExit(car,-1,{2,0,1})&&ctorCalls==0);
 std::puts("instant exit: four seats, direct detach, on-foot handoff, placement, unchanged door angle, script/control and obstacle gates passed");
}
