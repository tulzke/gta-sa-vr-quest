#include "VehicleClimbGeometry.h"
#include "VehicleClimb.h"
#include "VehicleDebugDrag.h"
#include "Symbols.h"
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>
double testNow=1;
namespace savr {
GameSymbols g;
namespace xr {
struct HandPose {bool valid{true};float gripPos[3]{};float grip{},trigger{};};
void TriggerHaptic(int,float,float,float){}
}
namespace driving {
int mode{},saves{};float config[2][12]{};
int GetVehicleDebugMode(){return mode;}
void SaveDebugCalibration(){++saves;}
void GetClimbCalibration(int model,int side,float out[12]){assert(model==403);std::copy_n(config[side],12,out);}
void SetClimbCalibration(int model,int side,const float in[12]){assert(model==403);std::copy_n(in,12,config[side]);}
}
namespace vrcam {
float offsetZ{};
bool TrackingPointToWorld(const float* a,float* b){std::copy_n(a,3,b);b[2]+=offsetZ;return true;}
bool WorldPointToTracking(const float* a,float* b){std::copy_n(a,3,b);b[2]-=offsetZ;return true;}
bool GetHeadWorldRay(float* o,float* d){o[0]=-1.5f;o[1]=0;o[2]=1.6f;d[1]=1;return true;}
}
}
// PRODUCTION_CLIMB
using namespace savr;
using namespace savr::vehicleclimb;
template<class T>void Put(void* p,int off,T v){std::memcpy(static_cast<char*>(p)+off,&v,sizeof(v));}
char cars[110*0xc68]{},pool[32]{},matrix[64]{},door[64]{},ped[0x600]{},pm[64]{};
std::uint8_t flags[110];void* poolPtr=pool;void* ignored{};bool wall{};int moves{};
xr::HandPose hands[2];
void Tick(bool usable=true,std::uint64_t epoch=1,unsigned occupied=0){testNow+=.01;Update(usable,hands,epoch,occupied);}
int main(){
    // Regression: the door node lies inside the cab outline. Default pipes
    // must clear the 1.449 m lateral detail on both sides of retail linerun.dff.
    for(int side=0;side<2;++side){float zero[12]{};const float sign=side?1.f:-1.f;
        const auto initial=Shape({sign*.929f,2.21f,.664f},side,zero);
        assert(sign*initial.pipe.bottom.x>1.649f);
        assert(sign*initial.pipe.top.x>1.649f);
        assert(initial.step.Contains({sign*1.759f,1.56f,-.15f}));
    }
    std::fill_n(flags,110,std::uint8_t(0x80));flags[0]=0;
    Put(pool,0,static_cast<char*>(cars));Put(pool,8,flags);Put(pool,0x10,110);
    Put(cars,0x18,static_cast<void*>(matrix));Put(cars,0x32,std::uint16_t(403));
    Put(matrix,0,Vec{1,0,0});Put(matrix,0x10,Vec{0,1,0});Put(matrix,0x20,Vec{0,0,1});
    Put(door,0x30,Vec{-.929f,0,1.5f});Put(cars,0x800+10*8,static_cast<void*>(door));
    Put(ped,0x18,static_cast<void*>(pm));Put(pm,0x30,Vec{-1.3f,0,.9f});
    g.CPools_ms_pVehiclePool=&poolPtr;g.FindPlayerPed=[](int)->void*{return ped;};g.FindPlayerVehicle=[](int,bool)->void*{return nullptr;};
    g.CPed_GetBonePosition=[](void*,GameSymbols::Vec3* out,unsigned,bool){out->x=10;out->y=0;out->z=0;};
    g.CWorld_pIgnoreEntity=&ignored;
    g.CWorld_ProcessLineOfSight=[](const GameSymbols::Vec3*,const GameSymbols::Vec3*,void*,void**,bool,bool,bool,bool,bool,bool,bool,bool){return wall;};
    g.CWorld_TestSphereAgainstWorld=[](GameSymbols::Vec3,float,void*,bool,bool,bool,bool,bool,bool)->void*{return wall?cars:nullptr;};
    removeEntity=[](void*){++moves;};addEntity=[](void*){};ltm=[](void* p){return p;};healthy=[](const void*){return true;};
    hands[0].gripPos[0]=-1.65f;hands[0].gripPos[1]=-.65f;hands[0].gripPos[2]=1.2f;
    Tick();Tick();
    vehicledoors::MarkerFrame published;published.epoch=1;Append(published);
    assert(published.count==1&&published.markers[0].bikeGrip&&published.markers[0].pipeLength>1.f);
    hands[0].grip=1;Tick();assert(ClaimedHands()==1);
    const float before=Position(ped).z;hands[0].gripPos[2]-=.02f;Tick();assert(std::abs(Position(ped).z-before-.02f)<.0001f);
    // Holding still must restore the anchor after native gravity, and the
    // rendered grip must stay at the same world point while the camera moves.
    const Vec locked=grabWorld[0];const Vec stable=Position(ped);
    Put(pm,0x30,stable-Vec{0,0,.04f});Tick();assert(std::abs(Position(ped).z-stable.z)<.0001f);
    xr::HandPose rendered[2]={hands[0],hands[1]};vrcam::offsetZ=.12f;
    ApplyHandVisuals(rendered);assert(std::abs(rendered[0].gripPos[2]+.12f-locked.z)<.0001f);
    assert(rendered[0].grip==1.f);vrcam::offsetZ=0;
    const float still=Position(ped).z;vrcam::offsetZ=.03f;Tick();assert(std::abs(Position(ped).z-still)<.0001f);vrcam::offsetZ=0;
    hands[1]=hands[0];hands[1].grip=0;Tick();hands[1].grip=1;Tick();assert(ClaimedHands()==3);
    const float dual=Position(ped).z;hands[0].gripPos[2]-=.005f;hands[1].gripPos[2]-=.005f;Tick();assert(std::abs(Position(ped).z-dual-.005f)<.0001f);
    hands[1].grip=0;Tick();
    wall=true;const Vec blocked=Position(ped);hands[0].gripPos[2]-=.02f;Tick();assert(Position(ped).z==blocked.z&&ignored==nullptr);wall=false;
    hands[0].gripPos[2]-=1;Tick();assert(!ClaimedHands());
    Tick(false);hands[0].gripPos[2]=1.2f;Tick();assert(!ClaimedHands());
    hands[0].grip=0;Tick();hands[0].grip=1;Tick(true,1,1);assert(!ClaimedHands());
    hands[0].grip=0;Tick();hands[0].grip=1;Tick();assert(ClaimedHands());flags[0]=2;Tick();assert(!ClaimedHands());
    // Place a native foot inside the step and verify persistent support after
    // the animated foot moves away, followed by clean release on walking off.
    hands[0].grip=0;
    g.CPed_GetBonePosition=[](void*,GameSymbols::Vec3* out,unsigned bone,bool){assert(bone==44||bone==54);out->x=-1.75f;out->y=-.65f;out->z=.65f;};
    Put(pm,0x30,Vec{-1.75f,-.65f,1.5f});Tick();Tick();assert(supported);const float supportedZ=Position(ped).z;
    g.CPed_GetBonePosition=[](void*,GameSymbols::Vec3* out,unsigned,bool){out->x=10;out->z=0;};
    Put(pm,0x30,Vec{-1.75f,-.65f,supportedZ-.02f});Tick();assert(std::abs(Position(ped).z-supportedZ)<.0001f);
    Put(pm,0x30,Vec{-3,-.45f,supportedZ});Tick();assert(!supported);
    // Editing changes the actual shape and persists once on trigger release.
    driving::mode=1;hands[0].trigger=0;hands[0].gripPos[0]=-1.97f;hands[0].gripPos[1]=-1.07f;hands[0].gripPos[2]=.75f;Tick();
    hands[0].trigger=1;Tick();assert(Editing());hands[0].gripPos[0]-=.1f;Tick();assert(driving::config[0][0]<-.09f);
    hands[0].trigger=0;Tick();assert(!Editing()&&driving::saves==1);
    // A long pull is not a distance-to-pipe release. No speed cap discards
    // controller travel; every 10 cm stroke must move the body by 10 cm.
    driving::mode=0;hands[0].grip=0;hands[0].gripPos[0]=-1.65f;hands[0].gripPos[1]=-.65f;hands[0].gripPos[2]=1.2f;Tick();
    hands[0].grip=1;Tick();assert(ClaimedHands()==1);
    for(int i=0;i<9;++i){const float z=Position(ped).z;hands[0].gripPos[2]-=.1f;Tick();assert(ClaimedHands()==1);assert(std::abs(Position(ped).z-z-.1f)<.0001f);}
    Put(cars,0x74,Vec{.1f,0,0});Tick();assert(!activeCar&&!supported&&!ClaimedHands());
    Put(cars,0x74,Vec{});driving::mode=0;
    Put(cars,0x32,std::uint16_t(514));Tick();assert(markers.count==0);
    Put(cars,0x32,std::uint16_t(403));Tick();assert(markers.count==1);
    std::puts("vehicle climb: grip, movement, obstruction, interruption, ownership, pool reuse, support and editing passed");
}
