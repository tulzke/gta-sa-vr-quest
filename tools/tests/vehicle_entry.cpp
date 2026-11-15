#include "VehicleEntry.h"
#include "VehicleExit.h"
#include "VehicleAccess.h"
#include "Symbols.h"
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
namespace savr::ledgeclimb { unsigned ClaimedHands(){return 0;} }
namespace savr::vehicleclimb { unsigned ClaimedHands(){return 0;} }
using savr::vehicledoors::Vec;
double testNow=1;float eyeX=-1.3f,eyeY=0;int vehicleAppearance=1;bool locksOpen=true;
namespace savr {
GameSymbols g{};
namespace passengerreaction { void Update(bool){} }
namespace xr { struct HandPose {bool valid{};float trigger{},gripPos[3]{};};HandPose handPoses[2]{};void GetHandPoses(HandPose* out){out[0]=handPoses[0];out[1]=handPoses[1];} struct InputState {float leftStick[2]{},rightStick[2]{};};InputState input;void GetInput(InputState& out){out=input;}void TriggerHaptic(int,float,float,float){} }
namespace driving { int debug{},saves{};float calibration[4][3]{};int GetVehicleDebugMode(){return debug;} float exitCal[2][4][3]{};void GetExitCalibration(int,int i,bool p,float* out){std::copy_n(exitCal[p][i],3,out);}void SetExitCalibration(int,int i,bool p,const float* v){std::copy_n(v,3,exitCal[p][i]);}void SaveDebugCalibration(){++saves;} void GetEntryCalibration(int,int seat,float* out){for(int i=0;i<3;++i)out[i]=calibration[seat][i];}void SetEntryCalibration(int,int seat,const float* in){for(int i=0;i<3;++i)calibration[seat][i]=in[i];} }
namespace locomotion { void MapGameplaySticks(float x,float y,float,float,float* mx,float* my,float*,float*){*mx=x;*my=y;} }
namespace vrcam { int exits{};void RequestVehicleExit(){++exits;}bool TrackingPointToWorld(const float* in,float* out){for(int i=0;i<3;++i)out[i]=in[i];return true;} bool GetHeadWorldRay(float* o,float* f){o[0]=eyeX;o[1]=eyeY;o[2]=.6f;f[0]=0;f[1]=1;f[2]=0;return true;} }
namespace vehicleexit {
 bool PlacementPreview(void*,int,vehicledoors::Vec& out){out={1,0,1};return true;}
 bool emergencyAvailable{},lastEmergency{},nativeAllowed=true;
 Mode Check(void* car,int seat,vehicledoors::Vec){
  if(!nativeAllowed)return Mode::Unavailable;
  if(emergencyAvailable)return Mode::Emergency;
  std::uint16_t model{};std::memcpy(&model,static_cast<char*>(car)+0x32,2);
  return vehicleAppearance==2||model==471||vehicleaccess::DoorOpen(car,seat+3)?Mode::Normal:Mode::Unavailable;
 }
 bool TryExit(void*,int,vehicledoors::Vec,bool emergency){lastEmergency=emergency;++vrcam::exits;return true;}
}
namespace boatentry {int calls{},seat{};bool success=true;bool SeatPlayer(void*,int s){++calls;seat=s;return success;} }
}
// PRODUCTION_ENTRY
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
template<class T>void Put(void* p,int off,T v){std::memcpy(static_cast<char*>(p)+off,&v,sizeof(v));}
char cars[110*0xc68]{},pool[32]{},matrix[64]{},doorMatrix[64]{},rearMatrix[64]{},leftMatrix[64]{},ped[64]{},pad[0x140]{};
unsigned char flags[110];void* poolPtr=pool;void* current{};
bool blockedLeft=false,blockedRight=false; bool Ray(const savr::GameSymbols::Vec3*,const savr::GameSymbols::Vec3* end,void*,void** hit,bool,bool,bool,bool,bool,bool,bool,bool){bool blocked=end->x<0?blockedLeft:blockedRight;*hit=blocked?pad:nullptr;return blocked;}
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
 g.FindPlayerPed=[](int)->void*{return ped;};g.FindPlayerVehicle=[](int,bool)->void*{return current;};
 g.CPad_GetPad=[](int)->void*{return pad;};g.CVehicle_GetVehicleAppearance=[](const void*){return vehicleAppearance;};
 canOpen=[](const void*,const void*){return locksOpen;};healthy=[](const void*){return true;};frameLtm=[](void* p){return p;};
 Tick();Tick();xr::input.leftStick[0]=1;Tick();CHECK(started>0);Tick();Tick();CHECK(boatentry::calls==0);
 xr::input.leftStick[0]=-1;Tick();CHECK(started==0);
 xr::input.leftStick[0]=1;Tick();for(int i=0;i<7;++i)Tick();CHECK(boatentry::calls==0);
 for(int i=0;i<2;++i)Tick();CHECK(boatentry::calls==1);CHECK(boatentry::seat==-1);
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
 // Exit must wait 1 second and requires an open door.
 for(int i=0;i<3;++i)Tick();xr::input.leftStick[0]=0;Tick();xr::input.leftStick[0]=1;
 for(int i=0;i<10;++i)Tick();CHECK(vrcam::exits==0);
 for(int i=0;i<2;++i)Tick();CHECK(vrcam::exits==1);
 for(int i=0;i<3;++i)Tick();xr::input.leftStick[0]=0;Tick();Put(cars,0x77c+3*0x18,0.f);xr::input.leftStick[0]=1;
 for(int i=0;i<20;++i)Tick();CHECK(vrcam::exits==1);
 // Exit eligibility comes from the shared native adapter, not the UI rays.
 Put(leftMatrix,0x30,Vec{-1,.35f,.4f});Put(cars,0x800+10*8,static_cast<void*>(leftMatrix));
 blockedLeft=blockedRight=true;xr::input.leftStick[0]=0;Tick();
 for(int i=0;i<icons.count;++i)CHECK(icons.markers[i].seatIcon!=3);
 Put(cars,0x5a0,static_cast<void*>(nullptr));blockedLeft=blockedRight=false;
 // Debug handles remain editable when native exit is unavailable, without enabling it.
 driving::debug=1;xr::input.leftStick[0]=0;Tick();
 CHECK(started==0);bool exitHandle=false,placementHandle=false;for(int i=0;i<icons.count;++i){exitHandle|=icons.markers[i].seatIcon==3;placementHandle|=icons.markers[i].seatIcon==4;}CHECK(exitHandle&&placementHandle);
 driving::debug=0;
 // A closed, blocked doorway offers the same hold for a native emergency exit.
 vehicleexit::emergencyAvailable=true;xr::input.leftStick[0]=0;Tick();xr::input.leftStick[0]=1;
 for(int i=0;i<12;++i)Tick();CHECK(vrcam::exits==2&&vehicleexit::lastEmergency);
 vehicleexit::emergencyAvailable=false;
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
 // Motorcycle icons respect actual passenger capacity and occupied seats.
 driving::debug=0;vehicleAppearance=2;current=nullptr;eyeX=-1.3f;
 Put(cars,0x32,std::uint16_t(461));Put(cars,0x5a0,static_cast<void*>(nullptr));
 Put(cars,0x5a8,static_cast<void*>(nullptr));Put(cars,0x5ec,std::uint8_t(1));
 xr::input.leftStick[0]=0;for(int i=0;i<3;++i)Tick();
 CHECK(icons.count==2);Put(cars,0x5ec,std::uint8_t(0));Tick();CHECK(icons.count==1);
 Put(cars,0x5ec,std::uint8_t(1));Put(cars,0x5a8,static_cast<void*>(ped));Tick();CHECK(icons.count==1);
 Put(cars,0x5a8,static_cast<void*>(nullptr));xr::input.leftStick[0]=1;
 const int beforeBike=boatentry::calls;for(int i=0;i<12;++i)Tick();CHECK(boatentry::calls==beforeBike+1);
 CHECK(boatentry::seat==-1);
 for(int i=0;i<3;++i)Tick();xr::input.leftStick[0]=0;eyeY=-.55f;Tick();xr::input.leftStick[0]=1;
 for(int i=0;i<12;++i)Tick();CHECK(boatentry::calls==beforeBike+2&&boatentry::seat==0);
 // All pedal bikes and the automobile-derived quad expose rider boarding.
 for(int model:{481,509,510,471}){
  xr::input.leftStick[0]=0;for(int i=0;i<3;++i)Tick();
  Put(cars,0x32,std::uint16_t(model));Put(cars,0x5ec,std::uint8_t(0));
  vehicleAppearance=model==471?1:2;eyeY=.2f;Tick();CHECK(icons.count==1);
  xr::input.leftStick[0]=1;const int before=boatentry::calls;
  for(int i=0;i<12;++i)Tick();CHECK(boatentry::calls==before+1&&boatentry::seat==-1);
 }
 // Quad exit icons sit outside the wider chassis and do not depend on locks.
 current=cars;Put(cars,0x5a0,static_cast<void*>(ped));locksOpen=false;eyeX=eyeY=0;
 xr::input.leftStick[0]=0;for(int i=0;i<3;++i)Tick();
 CHECK(icons.count==2&&std::abs(icons.markers[0].position.x+1.4f)<.001f&&
       std::abs(icons.markers[1].position.x-1.4f)<.001f);
 const int beforeQuadExit=vrcam::exits;xr::input.leftStick[0]=1;
 for(int i=0;i<12;++i)Tick();CHECK(vrcam::exits==beforeQuadExit+1);
 // Kart has one central driver seat, approached from either side without a door node.
 current=nullptr;locksOpen=true;vehicleAppearance=1;Put(cars,0x32,std::uint16_t(571));
 Put(cars,0x5a0,static_cast<void*>(nullptr));Put(cars,0x800+10*8,static_cast<void*>(nullptr));
 Put(cars,0x800+8*8,static_cast<void*>(nullptr));eyeY=0;
 for(float side:{-1.f,1.f}){
  eyeX=side*1.0f;xr::input.leftStick[0]=0;for(int i=0;i<3;++i)Tick();
  CHECK(icons.count==1&&icons.markers[0].seatIcon==1);
  const int before=boatentry::calls;xr::input.leftStick[0]=-side;
  for(int i=0;i<12;++i)Tick();CHECK(boatentry::calls==before+1&&boatentry::seat==-1);
 }
 xr::input.leftStick[0]=0;Put(cars,0x5a0,static_cast<void*>(ped));Tick();CHECK(icons.count==0);
 // Tractor is single-seat; the two buggies expose a passenger only if available.
 for(int model:{531,424,568,457,485,486,530,572})for(float side:{-1.f,1.f}){
  Put(cars,0x32,std::uint16_t(model));Put(cars,0x5a0,static_cast<void*>(nullptr));
  Put(cars,0x5a8,static_cast<void*>(nullptr));Put(cars,0x5ec,std::uint8_t(1));
  eyeX=side*(model==486?1.45f:1.0f);xr::input.leftStick[0]=0;for(int i=0;i<3;++i)Tick();
  CHECK(icons.count==1);const int seat=vehicleaccess::Find(model)->single||side<0?-1:0;
  CHECK(icons.markers[0].seatIcon==(seat<0?1:2));
  const int before=boatentry::calls;xr::input.leftStick[0]=-side;
  for(int i=0;i<12;++i)Tick();CHECK(boatentry::calls==before+1&&boatentry::seat==seat);
 }
 // Seated open vehicles show exits; two-seat buggies also show an empty seat.
 for(int model:{531,424,568,457,485,486,530,572}){
  Put(cars,0x32,std::uint16_t(model));Put(cars,0x5a0,static_cast<void*>(ped));
  Put(cars,0x5a8,static_cast<void*>(nullptr));current=cars;eyeX=eyeY=0;
  xr::input.leftStick[0]=0;for(int i=0;i<3;++i)Tick();
  const bool single=vehicleaccess::Find(model)->single;
  CHECK(icons.count==(single?2:3));
  int exits=0,transfers=0;for(int i=0;i<icons.count;++i){exits+=icons.markers[i].seatIcon==3;transfers+=icons.markers[i].seatIcon==2;}
  CHECK(exits==2&&transfers==(single?0:1));
  if(!single){Put(cars,0x5a8,static_cast<void*>(pad));Tick();CHECK(icons.count==1&&icons.markers[0].seatIcon==3);}
 }
 // Detached doors are passable per side, without changing the other door.
 current=nullptr;Put(cars,0x32,std::uint16_t(400));Put(cars,0x5a0,static_cast<void*>(nullptr));
 Put(cars,0x800+10*8,static_cast<void*>(leftMatrix));Put(leftMatrix,0x30,Vec{-1,.35f,.4f});
 Put(cars,0x758+9+2,std::uint8_t(4));Put(cars,0x77c+2*0x18,0.f);
 eyeX=-1.3f;xr::input.leftStick[0]=0;for(int i=0;i<3;++i)Tick();
 CHECK(DoorOpen(cars,0));const int beforeDetached=boatentry::calls;xr::input.leftStick[0]=1;
 for(int i=0;i<12;++i)Tick();CHECK(boatentry::calls==beforeDetached+1&&boatentry::seat==-1);
 // Stock lock state 2 cannot lock a physically absent door; other native
 // lock states and an intact door remain blocked.
 Put(cars,0x32,std::uint16_t(531));Put(cars,0x684,std::uint32_t(2));locksOpen=false;
 xr::input.leftStick[0]=0;eyeX=-1.f;for(int i=0;i<3;++i)Tick();CHECK(icons.count==1);
 const int beforeLocked=boatentry::calls;xr::input.leftStick[0]=1;for(int i=0;i<12;++i)Tick();
 CHECK(boatentry::calls==beforeLocked+1&&boatentry::seat==-1);
 xr::input.leftStick[0]=0;Put(cars,0x684,std::uint32_t(3));for(int i=0;i<3;++i)Tick();CHECK(icons.count==0);
 Put(cars,0x32,std::uint16_t(400));Put(cars,0x684,std::uint32_t(2));Put(cars,0x758+9+2,std::uint8_t(0));Tick();CHECK(icons.count==0);
 // A high cab and an arbitrarily distant ENTRY calibration cannot hide an
 // available exit. Custom UI clearance rays cannot overrule the game either.
 locksOpen=true;current=cars;Put(cars,0x5a0,static_cast<void*>(ped));
 Put(cars,0x32,std::uint16_t(514));Put(cars,0x77c+2*0x18,1.f);
 Put(leftMatrix,0x30,Vec{-1,.35f,3.f});driving::calibration[0][2]=20;
 blockedLeft=blockedRight=true;xr::input.leftStick[0]=0;for(int i=0;i<3;++i)Tick();
 bool exitVisible=false;for(int i=0;i<icons.count;++i)exitVisible|=icons.markers[i].seatIcon==3;CHECK(exitVisible);
 vehicleexit::nativeAllowed=false;Tick();
 for(int i=0;i<icons.count;++i)CHECK(icons.markers[i].seatIcon!=3);
 vehicleexit::nativeAllowed=true;xr::input.leftStick[0]=-1;for(int i=0;i<6;++i)Tick();CHECK(started>0);
 vehicleexit::emergencyAvailable=true;Tick();const int modeSwitchExits=vrcam::exits;
 for(int i=0;i<5;++i)Tick();CHECK(vrcam::exits==modeSwitchExits);
 for(int i=0;i<6;++i)Tick();CHECK(vrcam::exits==modeSwitchExits+1&&vehicleexit::lastEmergency);
 // Exit handles edit a separate profile and consume the stick action.
 current=nullptr;Put(cars,0x5a0,static_cast<void*>(nullptr));Put(cars,0x32,std::uint16_t(400));Put(leftMatrix,0x30,Vec{-1,.35f,.4f});
 driving::debug=1;driving::calibration[0][2]=0;blockedLeft=blockedRight=false;eyeX=-1.3f;xr::input.leftStick[0]=0;
 xr::handPoses[0].valid=true;xr::handPoses[0].trigger=0;
 const Vec proxy{-1.45f,0,.75f};std::memcpy(xr::handPoses[0].gripPos,&proxy,12);for(int i=0;i<3;++i)Tick();
 xr::handPoses[0].trigger=1;Tick();CHECK(editHand==0&&editElement==1&&ClaimedHands()==1);
 xr::handPoses[0].gripPos[0]+=.1f;Tick();CHECK(std::abs(driving::exitCal[0][0][0]-.1f)<1e-4f);
 const int saved=driving::saves;xr::handPoses[0].trigger=0;Tick();CHECK(editHand<0&&driving::saves==saved+1&&started==0);
 const Vec placementProxy=proxy+Vec{0,0,.35f};std::memcpy(xr::handPoses[0].gripPos,&placementProxy,12);Tick();xr::handPoses[0].trigger=1;Tick();CHECK(editHand==0&&editElement==2);
 xr::handPoses[0].gripPos[1]+=.15f;xr::handPoses[0].gripPos[2]-=.2f;Tick();CHECK(editElement==2);CHECK(std::abs(driving::exitCal[1][0][1]-.15f)<1e-4f&&driving::exitCal[1][0][2]==0);
 flags[0]++;Tick();CHECK(editHand<0&&ClaimedHands()==0);
 // Closest handle wins even when its sibling is also within the grab radius.
 xr::handPoses[0].trigger=0;Tick();Vec overlap=proxy+Vec{0,0,.18f};std::memcpy(xr::handPoses[0].gripPos,&overlap,12);
 // Move placement back to its default, so 18cm is closer to the 35cm proxy.
 std::fill_n(driving::exitCal[1][0],3,0.f);xr::handPoses[0].trigger=1;Tick();CHECK(editHand==0&&editElement==2);
 xr::handPoses[0].trigger=0;Tick();
 // All three handle types are present in the same debug frame.
 bool entryShown=false,exitShown=false,placementShown=false;
 for(int i=0;i<icons.count;++i){entryShown|=icons.markers[i].seatIcon==1;exitShown|=icons.markers[i].seatIcon==3;placementShown|=icons.markers[i].seatIcon==4;}
 CHECK(entryShown&&exitShown&&placementShown);
 // External ownership prevents an editor grab even on the exact handle.
 xr::handPoses[0].trigger=0;Tick();std::memcpy(xr::handPoses[0].gripPos,&proxy,12);xr::handPoses[0].trigger=1;testNow+=.1;Update(true,1,1);CHECK(editHand<0);
 // Holding a weapon still permits normal boarding outside the editor.
 driving::debug=0;driving::calibration[0][2]=0;xr::handPoses[0].trigger=0;xr::input.leftStick[0]=0;Tick();Tick();
 xr::input.leftStick[0]=1;testNow+=.1;Update(true,1,1);CHECK(started>0);
 std::puts("vehicle entry: continuous hold, direction cancellation, occupancy, flash and release latch passed");
}

