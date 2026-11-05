#include "VehicleClimbGeometry.h"
#include "VehicleClimb.h"
#include "VehicleClimbProfiles.h"
#include "VehicleDebugDrag.h"
#include "Symbols.h"
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <atomic>
#define LOGI(...) ((void)0)
double testNow=1;
namespace savr {
GameSymbols g;
namespace xr {
struct HandPose {bool valid{true};float gripPos[3]{};float grip{},trigger{};float gripOri[4]{0,0,0,1};bool climbGrip{};float climbRight[3]{},climbForward[3]{},climbUp[3]{};};
void TriggerHaptic(int,float,float,float){}
}
namespace driving {
int mode{},saves{},lastCalibrationModel{};float config[2][12]{};
int GetVehicleDebugMode(){return mode;}
void SaveDebugCalibration(){++saves;}
void GetClimbCalibration(int model,int side,float out[12]){assert(vehicleclimb::FindProfile(model));lastCalibrationModel=model;std::copy_n(config[side],12,out);}
void SetClimbCalibration(int model,int side,const float in[12]){assert(vehicleclimb::FindProfile(model));lastCalibrationModel=model;std::copy_n(in,12,config[side]);}
}
namespace vrcam {
float offsetZ{};
bool TrackingPointToWorld(const float* a,float* b){std::copy_n(a,3,b);b[2]+=offsetZ;return true;}
bool WorldPointToTracking(const float* a,float* b){std::copy_n(a,3,b);b[2]-=offsetZ;return true;}
bool GetHeadWorldRay(float* o,float* d){o[0]=-1.5f;o[1]=0;o[2]=1.6f;d[1]=1;return true;}
}
}
namespace savr::locomotion { bool AutoParachuteEnabled(){return false;} }
namespace savr::physicalweapon {
std::atomic<unsigned> gHeldMaskRelaxed{};
int CurrentWeaponSlot(void* ped){return *(static_cast<unsigned char*>(ped)+0x8dc);}
int WeaponTypeInSlot(void* ped,int slot){int type{};std::memcpy(&type,static_cast<char*>(ped)+0x730+slot*0x20,4);return type;}
double NowSeconds(){return testNow;}
}
// PRODUCTION_CLIMB
// PRODUCTION_PARACHUTE
using namespace savr;
using namespace savr::vehicleclimb;
// ACTUAL_GRIP_THUMBS
template<class T>void Put(void* p,int off,T v){std::memcpy(static_cast<char*>(p)+off,&v,sizeof(v));}
char cars[110*0xc68]{},pool[32]{},matrix[64]{},door[64]{},ped[0xa00]{},pm[64]{};
std::uint8_t flags[110];void* poolPtr=pool;void* ignored{};bool wall{},carBlocks{};int moves{};
xr::HandPose hands[2];
void Tick(bool usable=true,std::uint64_t epoch=1,unsigned occupied=0){testNow+=.01;Update(usable&&!vrcam::ParachuteStateActive(),hands,epoch,occupied);physicalweapon::AutoEquipParachute();}
int main(){
    // The real left/right Grip meshes both place the thumb cluster at +Z.
    // Apply the exact renderer frame and the UXR vertex mapping: both thumbs
    // must end up above the fixed pipe anchor, with opposite mesh parity.
    for(float angle:{0.f,45.f,90.f,180.f,270.f}){
        const float radians=angle*.01745329252f;
        const Vec heading{-std::sin(radians),std::cos(radians),0};
        const auto left=PipeGripMeshFrame(PipeHandFrame({0,0,1},heading,0,0),0);
        const auto right=PipeGripMeshFrame(PipeHandFrame({0,0,1},heading*(-1.f),0,0),1);
        auto point=[](const vehicledoors::Basis& frame,Vec v){
            return frame.forward*(v.x-.055f)+frame.right*v.z-frame.up*v.y-
                GripPivotOffset(frame.forward,frame.up);
        };
        assert(point(left,thumb0).z>.01f&&point(right,thumb1).z>.01f);
        assert(vehicledoors::Dot(vehicledoors::Cross(left.right,left.forward),left.up)*
               vehicledoors::Dot(vehicledoors::Cross(right.right,right.forward),right.up)<0);
    }
    for(int calibration:{-90,0,90}){
        const auto left=PipeGripMeshFrame(PipeHandFrame({0,0,1},{0,1,0},-float(calibration),-float(calibration)),0);
        const auto right=PipeGripMeshFrame(PipeHandFrame({0,0,1},{0,-1,0},float(calibration),float(calibration)),1);
        assert(Length(left.right-right.right)<.0001f);
        assert(thumb0.z>.01f&&thumb1.z>.01f);
    }
    const auto tiltA=PipeHandFrame({0,0,1},{0,1,.8f},10,20);
    const auto tiltB=PipeHandFrame({0,0,1},{0,1,-.8f},10,20);
    const auto yaw=PipeHandFrame({0,0,1},{1,0,.8f},10,20);
    assert(tiltA.Valid()&&tiltB.Valid()&&yaw.Valid());
    assert(Length(tiltA.up-tiltB.up)<.0001f&&Length(tiltA.forward-tiltB.forward)<.0001f);
    assert(Length(tiltA.forward-yaw.forward)>.5f);
    const auto left=PipeHandFrame({0,0,1},{0,1,0},-90,0);
    const auto right=PipeHandFrame({0,0,1},{0,1,0},90,0);
    assert(left.forward.z<-.99f&&right.forward.z>.99f);
    for(float a:{0.f,45.f,90.f,180.f}){
        const float radians=a*.01745329252f;
        const auto frame=PipeHandFrame({0,0,1},{-std::sin(radians),std::cos(radians),0},0,0);
        const Vec centre=GripPivotOffset(frame.forward,frame.up);
        const Vec anchor{};const Vec meshPosition=anchor-centre;
        assert(Length(meshPosition+GripPivotOffset(frame.forward,frame.up)-anchor)<.00001f);
        assert(std::abs(Dot(frame.right,{0,0,1})-1.f)<.00001f);
    }
    // Regression: the door node lies inside the cab outline. Default pipes
    // must clear the 1.449 m lateral detail on both sides of retail linerun.dff.
    for(int side=0;side<2;++side){float zero[12]{};const float sign=side?1.f:-1.f;
        const auto initial=Shape({sign*.929f,2.21f,.664f},side,zero);
        assert(sign*initial.pipe.bottom.x>1.649f);
        assert(sign*initial.pipe.top.x>1.649f);
        zero[0]=1;zero[1]=-1;zero[2]=.5f;zero[3]=.8f;zero[4]=.7f;zero[5]=-.4f;
        const auto oldStep=Shape({sign*.929f,2.21f,.664f},side,zero);
        assert(Length(oldStep.pipe.bottom-initial.pipe.bottom)<.0001f&&Length(oldStep.pipe.top-initial.pipe.top)<.0001f);
    }
    assert(FindProfile(514)&&FindProfile(444)&&FindProfile(573));
    assert(!FindProfile(400)&&!FindProfile(406)&&!FindProfile(486)&&!FindProfile(532));
    std::fill_n(flags,110,std::uint8_t(0x80));flags[0]=0;
    Put(pool,0,static_cast<char*>(cars));Put(pool,8,flags);Put(pool,0x10,110);
    Put(cars,0x18,static_cast<void*>(matrix));Put(cars,0x32,std::uint16_t(403));
    Put(matrix,0,Vec{1,0,0});Put(matrix,0x10,Vec{0,1,0});Put(matrix,0x20,Vec{0,0,1});
    Put(door,0x30,Vec{-.929f,0,1.5f});Put(cars,0x800+10*8,static_cast<void*>(door));
    Put(ped,0x18,static_cast<void*>(pm));Put(pm,0x30,Vec{-1.3f,0,.9f});
    Put(ped,0x28,std::uint64_t(0x4001));Put(ped,0x64,std::uint32_t(0x202));Put(ped,0x584,std::uint8_t(0x81));
    g.CPools_ms_pVehiclePool=&poolPtr;g.FindPlayerPed=[](int)->void*{return ped;};g.FindPlayerVehicle=[](int,bool)->void*{return nullptr;};
    g.CPed_GetBonePosition=[](void*,GameSymbols::Vec3* out,unsigned,bool){out->x=10;out->y=0;out->z=0;};
    g.CWorld_pIgnoreEntity=&ignored;
    g.CWorld_ProcessLineOfSight=[](const GameSymbols::Vec3*,const GameSymbols::Vec3*,void*,void** hit,bool,bool,bool,bool,bool,bool,bool,bool){
        if(carBlocks&&ignored!=cars){*hit=cars;return true;}return wall;
    };
    g.CWorld_TestSphereAgainstWorld=[](GameSymbols::Vec3,float,void* exclude,bool,bool,bool,bool,bool,bool)->void*{
        return wall||(carBlocks&&exclude!=cars)?cars:nullptr;
    };
    removeEntity=[](void*){++moves;};addEntity=[](void*){};ltm=[](void* p){return p;};healthy=[](const void*){return true;};
    for(int model=400;model<=611;++model)if(FindProfile(model)){
        Put(cars,0x32,std::uint16_t(model));Tick();Tick();
        assert(driving::lastCalibrationModel==model&&markers.count==1);
    }
    Put(cars,0x32,std::uint16_t(403));
    hands[0].gripPos[0]=-1.65f;hands[0].gripPos[1]=-.65f;hands[0].gripPos[2]=1.2f;
    Tick();Tick();
    vehicledoors::MarkerFrame published;published.epoch=1;Append(published);
    assert(published.count==1&&published.markers[0].bikeGrip&&published.markers[0].pipeLength>1.f);
    assert(published.markers[0].climbPipe&&published.markers[0].clipPipe);
    assert(published.markers[0].pipeHandMask==3);
    assert(std::abs(published.markers[0].pipeHands[0].x-hands[0].gripPos[0])<.0001f);
    const Vec grounded=Position(ped);
    Put(cars,0x68,Vec{.1f,0,0});Put(cars,0x74,Vec{0,0,.1f});
    hands[0].grip=1;Tick();assert(ClaimedHands()==1);
    Put(cars,0x68,Vec{});Put(cars,0x74,Vec{});
    assert(Length(Position(ped)-grounded)<.0001f); // no delayed floor pop
    assert((Read<std::uint64_t>(ped,0x28)&1)==0&&(Read<std::uint32_t>(ped,0x64)&2)==0);
    assert(Read<std::uint8_t>(ped,0x584)==0x80); // detach at grip, not after pulling
    const float before=Position(ped).z;hands[0].gripPos[2]-=.02f;Tick();assert(std::abs(Position(ped).z-before-.02f)<.0001f);
    // Holding still must restore the anchor after native gravity, and the
    // rendered grip must stay at the same world point while the camera moves.
    const Vec locked=grabWorld[0];const Vec stable=Position(ped);
    Put(pm,0x30,stable-Vec{0,0,.04f});Tick();assert(std::abs(Position(ped).z-stable.z)<.0001f);
    xr::HandPose rendered[2]={hands[0],hands[1]};vrcam::offsetZ=.12f;
    ApplyHandVisuals(rendered);assert(std::abs(rendered[0].gripPos[2]+.12f-locked.z)<.0001f);
    Tick();assert(std::abs(markers.markers[markers.count-1].pipeHands[0].z-locked.z)<.0001f);
    assert(rendered[0].grip==1.f&&rendered[0].trigger==0.f&&rendered[0].climbGrip);
    assert(std::abs(rendered[0].climbRight[2]-1.f)<.0001f);
    const Vec beforeYaw{rendered[0].climbForward[0],rendered[0].climbForward[1],rendered[0].climbForward[2]};
    rendered[0].gripOri[2]=.70710678f;rendered[0].gripOri[3]=.70710678f;
    ApplyHandVisuals(rendered);
    const Vec afterYaw{rendered[0].climbForward[0],rendered[0].climbForward[1],rendered[0].climbForward[2]};
    assert(std::abs(Dot(beforeYaw,afterYaw))<.0001f);
    assert(std::abs(rendered[0].climbRight[2]-1.f)<.0001f);
    rendered[0].gripOri[2]=0;rendered[0].gripOri[3]=1;
    vrcam::offsetZ=0;
    const float still=Position(ped).z;vrcam::offsetZ=.03f;Tick();assert(std::abs(Position(ped).z-still)<.0001f);vrcam::offsetZ=0;
    hands[1]=hands[0];hands[1].grip=0;Tick();hands[1].grip=1;Tick();assert(ClaimedHands()==3);
    for(float deg:{0.f,45.f,90.f,180.f,270.f,360.f}){
        const float a=deg*.01745329252f;
        xr::HandPose visual[2]={hands[0],hands[1]};
        for(auto& hand:visual){hand.gripOri[2]=std::sin(a*.5f);hand.gripOri[3]=std::cos(a*.5f);}
        ApplyHandVisuals(visual);
        const Vec expected{std::cos(a),std::sin(a),0};
        const Vec leftUp{visual[0].climbUp[0],visual[0].climbUp[1],visual[0].climbUp[2]};
        const Vec rightUp{visual[1].climbUp[0],visual[1].climbUp[1],visual[1].climbUp[2]};
        assert(Length(leftUp+expected)<.0001f&&Length(rightUp-expected)<.0001f);
    }
    // Equal controller poses must produce mirrored hand frames. Repeated calls
    // use the live pose, not the
    // orientation at which the grip was initially acquired.
    {
        xr::HandPose visual[2]={hands[0],hands[1]};ApplyHandVisuals(visual);
        const Vec lf{visual[0].climbForward[0],visual[0].climbForward[1],visual[0].climbForward[2]};
        const Vec rf{visual[1].climbForward[0],visual[1].climbForward[1],visual[1].climbForward[2]};
        const Vec lu{visual[0].climbUp[0],visual[0].climbUp[1],visual[0].climbUp[2]};
        const Vec ru{visual[1].climbUp[0],visual[1].climbUp[1],visual[1].climbUp[2]};
        assert(Length(lf+rf)<.0001f&&Length(lu+ru)<.0001f);
        visual[0].gripOri[2]=.70710678f;visual[0].gripOri[3]=.70710678f;ApplyHandVisuals(visual);
        visual[0].gripOri[2]=0;visual[0].gripOri[3]=1;ApplyHandVisuals(visual);
        assert(Length(Vec{visual[0].climbForward[0],visual[0].climbForward[1],visual[0].climbForward[2]}-lf)<.0001f);
    }
    const float dual=Position(ped).z;hands[0].gripPos[2]-=.005f;hands[1].gripPos[2]-=.005f;Tick();assert(std::abs(Position(ped).z-dual-.005f)<.0001f);
    hands[1].grip=0;Tick();
    wall=true;const Vec blocked=Position(ped);hands[0].gripPos[2]-=.02f;Tick();assert(Position(ped).z==blocked.z&&ignored==nullptr);wall=false;
    hands[0].gripPos[2]-=1;Tick();assert(!ClaimedHands());
    Tick(false);assert(Read<std::uint64_t>(ped,0x28)==0x4001&&Read<std::uint32_t>(ped,0x64)==0x202);
    hands[0].gripPos[2]=1.2f;Tick();assert(!ClaimedHands());
    hands[0].grip=0;Tick();hands[0].grip=1;Tick(true,1,1);assert(!ClaimedHands());
    hands[0].grip=0;Tick();hands[0].grip=1;Tick();assert(ClaimedHands());flags[0]=2;Tick();assert(!ClaimedHands());
    // There is no support volume: releasing restores physics even if a foot
    // would have intersected the former step.
    hands[0].grip=0;Tick();assert(!Attached()&&!suspendedPed);
    assert(Read<std::uint64_t>(ped,0x28)==0x4001&&Read<std::uint32_t>(ped,0x64)==0x202);
    // Editing changes the actual shape and persists once on trigger release.
    driving::mode=1;hands[0].trigger=0;hands[0].gripPos[0]=-1.659f;hands[0].gripPos[1]=-.65f;hands[0].gripPos[2]=1.375f;Tick();
    assert(markers.count==4); // centre, both ends, full pipe; no step markers
    hands[0].trigger=1;Tick();assert(Editing());hands[0].gripPos[0]-=.1f;Tick();assert(driving::config[0][6]<-.09f&&driving::config[0][9]<-.09f);
    assert(markers.markers[markers.count-1].climbPipe&&!markers.markers[markers.count-1].clipPipe);
    hands[0].trigger=0;Tick();assert(!Editing()&&driving::saves==1);
    // A long pull is not a distance-to-pipe release. No speed cap discards
    // controller travel; every 10 cm stroke must move the body by 10 cm.
    driving::mode=0;hands[0].grip=0;hands[0].gripPos[0]=-1.65f;hands[0].gripPos[1]=-.65f;hands[0].gripPos[2]=1.2f;Tick();
    hands[0].grip=1;Tick();assert(ClaimedHands()==1);
    for(int i=0;i<9;++i){const float z=Position(ped).z;hands[0].gripPos[2]-=.1f;Tick();assert(ClaimedHands()==1);assert(std::abs(Position(ped).z-z-.1f)<.0001f);assert((Read<std::uint64_t>(ped,0x28)&1)==0&&(Read<std::uint32_t>(ped,0x64)&2)==0);}
    // A cab obstruction blocks movement while retaining suspended physics.
    wall=true;hands[0].gripPos[2]-=.02f;Tick();assert(ClaimedHands()==1&&suspendedPed==ped);wall=false;
    Put(ped,0x28,Read<std::uint64_t>(ped,0x28)|0x1000); // unrelated engine change
    Put(cars,0x68,Vec{.1f,0,0});Put(cars,0x74,Vec{0,0,.1f});Tick();assert(activeCar&&ClaimedHands()==1);
    const Vec beforeCarMove=Position(ped);
    carBlocks=true; // own cab intersects the sweep unless explicitly ignored
    const Vec oldPipe=markers.markers[markers.count-1].position;
    Put(matrix,0x30,Vec{.05f,0,0});FollowBeforeCamera();
    vehicledoors::MarkerFrame late;late.epoch=1;Append(late);
    assert(late.count==1&&std::abs(late.markers[0].position.x-oldPipe.x-.05f)<.0001f);
    xr::HandPose lateHands[2]={hands[0],hands[1]};ApplyHandVisuals(lateHands);
    assert(std::abs(lateHands[0].gripPos[0]-(grabLocal[0].x+.05f))<.0001f);
    Tick();
    assert(ClaimedHands()==1&&std::abs(Position(ped).x-beforeCarMove.x-.05f)<.0001f);
    const Vec stillOutside=Position(ped);
    assert(!Move(ped,stillOutside,stillOutside+Vec{0,0,.05f})&&Length(Position(ped)-stillOutside)<.0001f);
    const int movesBeforeFast=moves;const Vec priorFastMove=Position(ped);Put(matrix,0x30,Vec{.65f,0,0});Tick();
    assert(ClaimedHands()==1&&std::abs(Position(ped).x-priorFastMove.x-.60f)<.0001f);
    assert(moves==movesBeforeFast+1); // all sweep probes, one world remove/add
    const Vec localBefore=Position(ped)-Vec{.65f,0,0};const float angle=.02f;
    Put(matrix,0,Vec{std::cos(angle),std::sin(angle),0});
    Put(matrix,0x10,Vec{-std::sin(angle),std::cos(angle),0});Tick();
    const Vec expected{.65f+localBefore.x*std::cos(angle)-localBefore.y*std::sin(angle),localBefore.x*std::sin(angle)+localBefore.y*std::cos(angle),localBefore.z};
    assert(ClaimedHands()==1&&Length(Position(ped)-expected)<.0001f);
    // A real wall must still prevent transport, and the global ignore pointer
    // must be restored even on the blocked path.
    wall=true;const Vec blockedCarry=Position(ped);Put(matrix,0x30,Vec{.70f,0,0});FollowBeforeCamera();
    assert(Length(Position(ped)-blockedCarry)<.0001f&&ignored==nullptr);wall=false;carBlocks=false;
    hands[0].grip=0;Tick();assert(!activeCar&&!ClaimedHands());
    assert(Read<std::uint64_t>(ped,0x28)==0x5001&&Read<std::uint32_t>(ped,0x64)==0x202);
    Put(cars,0x68,Vec{});Put(cars,0x74,Vec{});Put(matrix,0,Vec{1,0,0});Put(matrix,0x10,Vec{0,1,0});Put(matrix,0x30,Vec{});driving::mode=0;
    Put(cars,0x32,std::uint16_t(400));Tick();assert(markers.count==0);
    Put(cars,0x32,std::uint16_t(403));Tick();assert(markers.count==1);
    // Exercise the actual auto-equip -> parachute-state -> input gate chain.
    Put(pm,0x30,Vec{-1.75f,-.65f,1.5f});hands[0].grip=0;hands[0].gripPos[2]=1.2f;Tick();
    Put(ped,0x730+11*0x20,46);Put(ped,0x8dc,std::uint8_t(0));
    g.CPed_SetCurrentWeaponSlot=[](void* p,int slot){Put(p,0x8dc,std::uint8_t(slot));};
    g.CPedGeometryAnalyser_IsInAir=[](const void*){return true;};
    hands[0].grip=1;Tick();assert(Attached());
    for(int i=0;i<120;++i)Tick();
    assert(ClaimedHands()==1&&physicalweapon::CurrentWeaponSlot(ped)==0);
    Put(ped,0x8dc,std::uint8_t(11)); // previously selected or external switch
    assert(!vrcam::ParachuteStateActive());Tick();assert(ClaimedHands()==1&&physicalweapon::CurrentWeaponSlot(ped)==0);
    hands[0].grip=0;Tick();assert(!Attached()&&!ClaimedHands());
    for(int i=0;i<50;++i)Tick();assert(physicalweapon::CurrentWeaponSlot(ped)==0);
    for(int i=0;i<30;++i)Tick();assert(physicalweapon::CurrentWeaponSlot(ped)==11&&vrcam::ParachuteStateActive());
    g.CPedGeometryAnalyser_IsInAir=[](const void*){return false;};
    Put(ped,0x8dc,std::uint8_t(0));hands[0].grip=0;Tick();hands[0].grip=1;Tick();assert(Attached());
    PrepareForBoarding();assert(!Attached()&&!ClaimedHands()&&!suspendedPed);
    assert((Read<std::uint64_t>(ped,0x28)&1)!=0&&(Read<std::uint32_t>(ped,0x64)&2)!=0);
    // If an external native entry already seated CJ, cancellation must not
    // restore on-foot collision flags over the native occupant flags.
    SuspendPhysics(ped);g.FindPlayerVehicle=[](int,bool)->void*{return cars;};
    PrepareForBoarding();assert((Read<std::uint64_t>(ped,0x28)&1)==0);
    assert((Read<std::uint32_t>(ped,0x64)&2)==0);
    g.FindPlayerVehicle=[](int,bool)->void*{return nullptr;};
    // A lateral collision must preserve a clear vertical component.
    Put(pm,0x30,Vec{-2,0,1});
    g.CWorld_TestSphereAgainstWorld=[](GameSymbols::Vec3 p,float,void*,bool,bool,bool,bool,bool,bool)->void*{return p.x> -1.99f?cars:nullptr;};
    assert(!Move(ped,Position(ped),{-1.8f,0,1.2f}));
    assert(std::abs(Position(ped).z-1.2f)<.0001f&&Position(ped).x<=-1.99f);
    std::puts("vehicle climb: grip, movement, obstruction, interruption, ownership, pool reuse, release and editing passed");
}
