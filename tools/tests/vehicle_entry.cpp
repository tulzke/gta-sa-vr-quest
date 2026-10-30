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
namespace passengerreaction { void Update(bool){} }
namespace xr { struct HandPose {bool valid{};float trigger{},gripPos[3]{};};HandPose handPoses[2]{};void GetHandPoses(HandPose* out){out[0]=handPoses[0];out[1]=handPoses[1];} struct InputState {float leftStick[2]{},rightStick[2]{};};InputState input;void GetInput(InputState& out){out=input;}void TriggerHaptic(int,float,float,float){} }
namespace driving { int debug{},saves{};float calibration[4][3]{};int GetVehicleDebugMode(){return debug;}void SaveDebugCalibration(){++saves;} void GetEntryCalibration(int,int seat,float* out){for(int i=0;i<3;++i)out[i]=calibration[seat][i];}void SetEntryCalibration(int,int seat,const float* in){for(int i=0;i<3;++i)calibration[seat][i]=in[i];} }
namespace locomotion { void MapGameplaySticks(float x,float y,float,float,float* mx,float* my,float*,float*){*mx=x;*my=y;} }
namespace vrcam { int exits{};void RequestVehicleExit(){++exits;}bool TrackingPointToWorld(const float* in,float* out){for(int i=0;i<3;++i)out[i]=in[i];return true;} bool GetHeadWorldRay(float* o,float* f){o[0]=eyeX;o[1]=0;o[2]=.6f;f[0]=0;f[1]=1;f[2]=0;return true;} }
namespace vehicleexit { bool TryExit(void*,int,vehicledoors::Vec){++vrcam::exits;return true;} }
namespace boatentry {int calls{},seat{};bool success=true;bool SeatPlayer(void*,int s){++calls;seat=s;return success;} }
}
// PRODUCTION_ENTRY
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
template<class T>void Put(void* p,int off,T v){std::memcpy(static_cast<char*>(p)+off,&v,sizeof(v));}
char cars[110*0xc68]{},pool[32]{},matrix[64]{},doorMatrix[64]{},rearMatrix[64]{},ped[64]{},pad[0x140]{};
unsigned char flags[110];void* poolPtr=pool;void* current{};
bool Ray(const savr::GameSymbols::Vec3*,const savr::GameSymbols::Vec3*,void*,void** hit,bool,bool,bool,bool,bool,bool,bool,bool){*hit=nullptr;return false;}
void Tick(float dt=.1f){testNow+=dt;savr::vehicleentry::Update(true,1);}
int main(){
 using namespace savr;using namespace vehicleentry;
 std::memset(flags,0x80,sizeof(flags));flags[0]=0;
 Put(pool,0,static_cast<char*>(cars));Put(pool,8,flags);Put(pool,0x10,110);
 Put(cars,0x18,static_cast<void*>(matrix));Put(cars,0x5a,std::uint8_t(2));
 Put(matrix,0,Vec{1,0,0});Put(matrix,0x10,Vec{0,1,0});Put(matrix,0x20,Vec{0,0,1});
 Put(doorMatrix,0x30,Vec{-1,.35f,.4f});Put(cars,0x800+10*8,static_cast<void*>(doorMatrix));
 Put(cars,0x770+2*0x18,1.f);Put(cars,0x77c+2*0x18,1.f);
 g.CPools_ms_pVehiclePool=&poolPtr;g.CWorld_ProcessLineOfSight=&Ray;
 canStepOut=[](const void*,bool){return true;};
 g.FindPlayerPed=[](int)->void*{return ped;};g.FindPlayerVehicle=[](int,bool)->void*{return current;};
 g.CPad_GetPad=[](int)->void*{return pad;};g.CVehicle_GetVehicleAppearance=[](const void*){return 1;};
 canOpen=[](const void*,const void*){return true;};healthy=[](const void*){return true;};frameLtm=[](void* p){return p;};
 Tick();Tick();xr::input.leftStick[0]=1;Tick();CHECK(started>0);Tick();Tick();CHECK(boatentry::calls==0);
 xr::input.leftStick[0]=-1;Tick();CHECK(started==0);
 xr::input.leftStick[0]=1;for(int i=0;i<12;++i)Tick();CHECK(boatentry::calls==1);CHECK(boatentry::seat==-1);
 for(int i=0;i<10;++i)Tick();CHECK(boatentry::calls==1);
 xr::input.leftStick[0]=0;Tick();xr::input.leftStick[0]=1;Tick();CHECK(started>0);
 Put(cars,0x5a0,static_cast<void*>(ped));Tick();CHECK(!visible&&started==0);
 Put(cars,0x5a0,static_cast<void*>(nullptr));Tick();CHECK(started>0);
 Tick(.3f);CHECK(started==0); // tracking/input discontinuity
 // Passenger door selects the front passenger, even with a driver present.
 eyeX=1.3f;Put(doorMatrix,0x30,Vec{1,.35f,.4f});Put(cars,0x800+8*8,static_cast<void*>(doorMatrix));
 Put(cars,0x770+3*0x18,1.f);Put(cars,0x77c+3*0x18,1.f);
 xr::input.leftStick[0]=0;Tick();xr::input.leftStick[0]=-1;
 for(int i=0;i<12;++i)Tick();CHECK(boatentry::calls==2&&boatentry::seat==0);
 // A seated passenger can transfer to an empty driver seat with closed doors.
 current=cars;Put(cars,0x5a8,static_cast<void*>(ped));Put(cars,0x77c+2*0x18,0.f);
 for(int i=0;i<3;++i)Tick();xr::input.leftStick[0]=0;Tick();xr::input.leftStick[0]=-1;
 for(int i=0;i<12;++i)Tick();CHECK(boatentry::calls==3&&boatentry::seat==-1);
 // Exit must wait 1.5 seconds and requires an open door.
 for(int i=0;i<3;++i)Tick();xr::input.leftStick[0]=0;Tick();xr::input.leftStick[0]=1;
 for(int i=0;i<12;++i)Tick();CHECK(vrcam::exits==0);
 for(int i=0;i<5;++i)Tick();CHECK(vrcam::exits==1);
 for(int i=0;i<3;++i)Tick();xr::input.leftStick[0]=0;Tick();Put(cars,0x77c+3*0x18,0.f);xr::input.leftStick[0]=1;
 for(int i=0;i<20;++i)Tick();CHECK(vrcam::exits==1);
 // Rear passenger cannot teleport into the front row.
 current=nullptr;Put(cars,0x5a8,static_cast<void*>(nullptr));
 Put(rearMatrix,0x30,Vec{1,-.8f,.4f});Put(cars,0x800+9*8,static_cast<void*>(rearMatrix));
 Put(cars,0x770+5*0x18,1.f);Put(cars,0x77c+5*0x18,1.f);
 xr::input.leftStick[0]=0;Tick();xr::input.leftStick[0]=-1;
 for(int i=0;i<12;++i)Tick();CHECK(boatentry::seat==2);
 current=cars;Put(cars,0x5a8+2*8,static_cast<void*>(ped));xr::input.leftStick[0]=0;
 for(int i=0;i<3;++i)Tick();xr::input.leftStick[0]=-1;const int count=boatentry::calls;
 for(int i=0;i<20;++i)Tick();CHECK(boatentry::calls==count);
 // Entry calibration uses the SAME debug flag and owns just the edited hand.
 current=nullptr;eyeX=-1.3f;Put(doorMatrix,0x30,Vec{-1,.35f,.4f});Put(cars,0x32,std::uint16_t(400));
 driving::debug=1;xr::input.leftStick[0]=0;
 xr::handPoses[0].valid=true;xr::handPoses[0].gripPos[0]=-.5f;xr::handPoses[0].gripPos[1]=0;xr::handPoses[0].gripPos[2]=.6f;
 Tick();xr::handPoses[0].trigger=1;Tick();CHECK(ClaimedHands()==1);
 xr::handPoses[0].gripPos[0]+=.1f;Tick();CHECK(driving::calibration[0][0]>.09f);
 xr::handPoses[0].trigger=0;Tick();CHECK(ClaimedHands()==0&&driving::saves==1);
 // Debug mode still permits entry, transfer and exit when not dragging an icon.
 xr::handPoses[0].valid=false;driving::debug=1;eyeX=1.3f;Put(doorMatrix,0x30,Vec{1,.35f,.4f});
 Put(cars,0x5a8+2*8,static_cast<void*>(nullptr));Put(cars,0x77c+3*0x18,1.f);
 xr::input.leftStick[0]=0;Tick();xr::input.leftStick[0]=-1;
 const int beforeDebug=boatentry::calls;for(int i=0;i<12;++i)Tick();CHECK(boatentry::calls==beforeDebug+1&&boatentry::seat==0);
 current=cars;Put(cars,0x5a8,static_cast<void*>(ped));xr::input.leftStick[0]=0;
 for(int i=0;i<3;++i)Tick();xr::input.leftStick[0]=-1;for(int i=0;i<12;++i)Tick();CHECK(boatentry::calls==beforeDebug+2&&boatentry::seat==-1);
 xr::input.leftStick[0]=0;for(int i=0;i<3;++i)Tick();xr::input.leftStick[0]=1;
 const int beforeExit=vrcam::exits;for(int i=0;i<17;++i)Tick();CHECK(vrcam::exits==beforeExit+1);
 std::puts("vehicle entry: continuous hold, direction cancellation, occupancy, flash and release latch passed");
}

