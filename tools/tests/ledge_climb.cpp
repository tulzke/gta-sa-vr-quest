#include "LedgeClimb.h"
#include "Symbols.h"
#include <cstring>
#include <chrono>
#include <cassert>
#include <cstdio>
double testNow=1;
namespace savr {
GameSymbols g;
namespace xr {
struct HandPose {bool valid=true;float gripPos[3]{};float grip{};};
bool IsSessionFocused(){return true;}
void TriggerHaptic(int,float,float,float){}
}
namespace vrcam {
float eyeY=-.2f;
bool GroundWalkControlsActive(){return true;}
bool TrackingPointToWorld(const float* a,float* b){std::copy_n(a,3,b);return true;}
bool WorldPointToTracking(const float* a,float* b){std::copy_n(a,3,b);return true;}
bool GetHeadWorldRay(float* a,float* b){a[0]=0;a[1]=eyeY;a[2]=2.3f;b[0]=0;b[1]=1;b[2]=0;return true;}
}
}
// PRODUCTION
using namespace savr;
using namespace savr::ledgeclimb;
char ped[0xa00]{},matrix[64]{};void* ignored{};
bool ceiling{},noFloor{},material=true;int moves{};
template<class T>void Put(void* p,int off,T v){std::memcpy(static_cast<char*>(p)+off,&v,sizeof(v));}
bool Query(const GameSymbols::Vec3* a,const GameSymbols::Vec3* b,void* col,void**,bool,bool,bool,bool,bool,bool,bool,bool){
 Vec p{},normal{};bool hit=false;
 if(!noFloor&&a->z>1.f&&b->z<1.f&&a->y>=0){float t=(a->z-1.f)/(a->z-b->z);p={a->x+(b->x-a->x)*t,a->y+(b->y-a->y)*t,1};normal={0,0,1};hit=true;}
 if(a->y<0&&b->y>=0&&a->z<1.f){float t=-a->y/(b->y-a->y);p={a->x+(b->x-a->x)*t,0,a->z+(b->z-a->z)*t};normal={0,-1,0};hit=true;}
 if(hit){Put(col,0,p);Put(col,0x10,normal);Put(col,0x23,std::uint8_t(1));}return hit;
}
int main(){
 Put(ped,0x18,static_cast<void*>(matrix));Put(matrix,0x30,Vec{0,-.3f,1.2f});
 Put(ped,0x28,std::uint64_t(0x4001));Put(ped,0x64,std::uint32_t(0x202));
 g.FindPlayerPed=[](int)->void*{return ped;};g.CWorld_pIgnoreEntity=&ignored;g.CWorld_ProcessLineOfSight=Query;
 g.CWorld_TestSphereAgainstWorld=[](GameSymbols::Vec3,float,void*,bool,bool,bool,bool,bool,bool)->void*{return ceiling?ped:nullptr;};
 removeEntity=[](void*){++moves;};addEntity=[](void*){};
 canClimb=[](void*,unsigned){return material;};surfaces=ped;hooksReady=true;
 Vec point{},tangent{};assert(Find({0,-.08f,1.05f},{0,1,0},point,tangent));assert(std::abs(point.z-1)<.001f);
 material=false;assert(Find({0,-.08f,1.05f},{0,1,0},point,tangent));material=true;
 const Reach idle=ReachFor({},false),fast=ReachFor({0,.12f,0},false),jump=ReachFor({0,.12f,.1f},true);
 assert(fast.grab>idle.grab&&jump.grab>fast.grab&&jump.preview>fast.preview);
 assert(!Find({0,-.08f,.48f},{0,1,0},point,tangent,idle));
 assert(Find({0,-.08f,.48f},{0,1,0},point,tangent,jump));
 // Crouched landing: CJ's feet are below the top, tracked head is above it.
 Suspend(ped);assert(Stand(ped,{0,-.3f,1.2f},{0,.12f,1.5f},1));
 assert(std::abs(Position(ped).z-2.02f)<.001f);assert(!suspended);
 assert((Read<std::uint64_t>(ped,0x28)&1)&&(Read<std::uint32_t>(ped,0x64)&2));assert(Read<std::uint8_t>(ped,0x584)&1);
 ceiling=true;assert(!Stand(ped,{0,-.3f,1.2f},{0,.12f,1.5f},1));ceiling=false;
 noFloor=true;assert(!Stand(ped,{0,-.3f,1.2f},{0,.12f,1.5f},1));noFloor=false;
 assert(!Stand(ped,{0,-.3f,1.2f},{0,.12f,.9f},1));
 // Animation legs pass the fence while the tracked head remains protected.
 Put(matrix,0x30,Vec{0,-.3f,.8f});assert(Move(ped,Position(ped),{0,.03f,.8f}));assert(Position(ped).y>0);
 ceiling=true;assert(!Move(ped,Position(ped),{0,.1f,.8f}));ceiling=false;
 const int old=moves;assert(!Move(ped,Position(ped),{10,0,0}));assert(moves==old);
 // A closed grip on entry cannot latch; a release and fresh squeeze can.
 xr::HandPose hands[2];hands[1].valid=false;hands[0].gripPos[1]=-.08f;hands[0].gripPos[2]=1.05f;hands[0].grip=1;
 for(int i=0;i<3;++i){testNow+=.01;Update(true,hands,1,0);}assert(!ClaimedHands());
 hands[0].grip=0;testNow+=.01;Update(true,hands,1,0);
 hands[0].grip=1;testNow+=.01;Update(true,hands,1,0);assert(ClaimedHands()==1);
 // Crossing the top must not auto-release, even if the original support
 // probe temporarily misses. Physics resumes only after the last grip opens.
 vrcam::eyeY=.12f;noFloor=true;
 testNow+=.01;Update(true,hands,1,0);assert(ClaimedHands()==1&&suspended);
 noFloor=false;testNow+=.3;Update(true,hands,1,0);assert(ClaimedHands()==1&&suspended);
 hands[0].grip=0;testNow+=.01;Update(true,hands,1,0);assert(!ClaimedHands()&&!suspended);
 assert(std::abs(Position(ped).z-2.02f)<.001f);vrcam::eyeY=-.2f;
 assert((Read<std::uint64_t>(ped,0x28)&1)&&(Read<std::uint32_t>(ped,0x64)&2));
 // Retail callbacks preserve NPCs and add height only on an actual launch.
 originalClimb=[](void*,Vec&,float&,unsigned char&,bool)->void*{return matrix;};
 float angle{};unsigned char surface{};assert(!OnClimb(ped,point,angle,surface,true));
 assert(OnClimb(matrix,point,angle,surface,true)==matrix);
 originalLaunch=[](void*,void* p){Put(p,0x68,Vec{0,.1f,.2f});};
 Put(ped,0x68,Vec{});OnLaunch(nullptr,ped);
 assert(std::abs(Read<Vec>(ped,0x68).z-.244948974f)<.00001f);
 assert(jumpUntil>testNow&&!hovered);
 // A long normal jump with a parachute in the active slot must never hand
 // the stable VR view to the live ped heading at the apex or on landing.
 Put(ped,0x8dc,std::int8_t(11));Put(ped,0x730+11*0x20,46);
 g.CPedGeometryAnalyser_IsInAir=[](const void*){return true;};
 testNow+=.8;assert(JumpInProgress());assert(!vrcam::ParachuteStateActive());
 testNow=jumpUntil+.01;assert(!JumpInProgress());assert(vrcam::ParachuteStateActive());
 g.CPedGeometryAnalyser_IsInAir=[](const void*){return false;};assert(!vrcam::ParachuteStateActive());
 puts("Ledge climbing production checks passed");
}
