#include "VehicleEntry.h"
#include "Symbols.h"
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
using savr::vehicledoors::Vec;
double testNow=1;float eyeX=-1.3f;
namespace savr {
GameSymbols g{};
namespace xr { struct InputState {float leftStick[2]{},rightStick[2]{};};InputState input;void GetInput(InputState& out){out=input;}void TriggerHaptic(int,float,float,float){} }
namespace driving { int GetVehicleDebugMode(){return 0;} }
namespace locomotion { void MapGameplaySticks(float x,float y,float,float,float* mx,float* my,float*,float*){*mx=x;*my=y;} }
namespace vrcam { bool GetHeadWorldRay(float* o,float* f){o[0]=eyeX;o[1]=0;o[2]=.6f;f[0]=0;f[1]=1;f[2]=0;return true;} }
namespace boatentry {int calls{},seat{};bool success=true;bool SeatPlayer(void*,int s){++calls;seat=s;return success;} }
}
// PRODUCTION_ENTRY
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
template<class T>void Put(void* p,int off,T v){std::memcpy(static_cast<char*>(p)+off,&v,sizeof(v));}
char cars[110*0xc68]{},pool[32]{},matrix[64]{},doorMatrix[64]{},ped[64]{},pad[0x140]{};
unsigned char flags[110];void* poolPtr=pool;void* current{};
void Tick(float dt=.1f){testNow+=dt;savr::vehicleentry::Update(true,1);}
int main(){
 using namespace savr;using namespace vehicleentry;
 std::memset(flags,0x80,sizeof(flags));flags[0]=0;
 Put(pool,0,static_cast<char*>(cars));Put(pool,8,flags);Put(pool,0x10,110);
 Put(cars,0x18,static_cast<void*>(matrix));Put(cars,0x5a,std::uint8_t(2));
 Put(matrix,0,Vec{1,0,0});Put(matrix,0x10,Vec{0,1,0});Put(matrix,0x20,Vec{0,0,1});
 Put(doorMatrix,0x30,Vec{-1,.35f,.4f});Put(cars,0x800+10*8,static_cast<void*>(doorMatrix));
 Put(cars,0x770+2*0x18,1.f);Put(cars,0x77c+2*0x18,1.f);
 g.CPools_ms_pVehiclePool=&poolPtr;
 g.FindPlayerPed=[](int)->void*{return ped;};g.FindPlayerVehicle=[](int,bool)->void*{return current;};
 g.CPad_GetPad=[](int)->void*{return pad;};g.CVehicle_GetVehicleAppearance=[](const void*){return 1;};
 canOpen=[](const void*,const void*){return true;};healthy=[](const void*){return true;};frameLtm=[](void* p){return p;};
 Tick();Tick();xr::input.leftStick[0]=1;Tick();CHECK(started>0);Tick();Tick();CHECK(boatentry::calls==0);
 xr::input.leftStick[0]=-1;Tick();CHECK(started==0);
 xr::input.leftStick[0]=1;for(int i=0;i<7;++i)Tick();CHECK(boatentry::calls==1);CHECK(boatentry::seat==-1);
 for(int i=0;i<10;++i)Tick();CHECK(boatentry::calls==1);
 xr::input.leftStick[0]=0;Tick();xr::input.leftStick[0]=1;Tick();CHECK(started>0);
 Put(cars,0x5a0,static_cast<void*>(ped));Tick();CHECK(!visible&&started==0);
 Put(cars,0x5a0,static_cast<void*>(nullptr));Tick();CHECK(started>0);
 Tick(.3f);CHECK(started==0); // tracking/input discontinuity
 // Passenger door selects the front passenger, even with a driver present.
 eyeX=1.3f;Put(doorMatrix,0x30,Vec{1,.35f,.4f});Put(cars,0x800+8*8,static_cast<void*>(doorMatrix));
 Put(cars,0x770+3*0x18,1.f);Put(cars,0x77c+3*0x18,1.f);
 xr::input.leftStick[0]=0;Tick();xr::input.leftStick[0]=-1;
 for(int i=0;i<7;++i)Tick();CHECK(boatentry::calls==2&&boatentry::seat==0);
 // A seated passenger can transfer to an empty driver seat with closed doors.
 current=cars;Put(cars,0x5a8,static_cast<void*>(ped));Put(cars,0x77c+2*0x18,0.f);
 for(int i=0;i<3;++i)Tick();xr::input.leftStick[0]=0;Tick();xr::input.leftStick[0]=-1;
 for(int i=0;i<7;++i)Tick();CHECK(boatentry::calls==3&&boatentry::seat==-1);
 std::puts("vehicle entry: continuous hold, direction cancellation, occupancy, flash and release latch passed");
}

