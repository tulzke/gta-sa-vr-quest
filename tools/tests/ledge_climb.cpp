#include "LedgeClimb.h"
#include "Symbols.h"
#include "HandAnimation.h"
#include <cstring>
#include <chrono>
#include <cassert>
#include <cstdio>
double testNow=1;
namespace savr {
GameSymbols g;
namespace xr {
struct HandPose {bool valid=true;float gripPos[3]{};float grip{},trigger{};bool climbGrip{},ledgeGrip{};float climbRight[3]{},climbForward[3]{},climbUp[3]{};};
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
bool ceiling{},noFloor{},material=true,thinRoof{},infiniteTop{};float fenceWidth=1000.f;float floorZ=1.f,slopeX{},slopeY{},edgeSlope{};int moves{};
template<class T>void Put(void* p,int off,T v){std::memcpy(static_cast<char*>(p)+off,&v,sizeof(v));}
bool Query(const GameSymbols::Vec3* a,const GameSymbols::Vec3* b,void* col,void**,bool,bool,bool,bool,bool,bool,bool,bool){
 Vec p{},normal{};bool hit=false;
 const float da=a->z-floorZ-slopeX*a->x-slopeY*a->y;
 const float db=b->z-floorZ-slopeX*b->x-slopeY*b->y;
 if(!noFloor&&da>0&&db<0){
  const float t=da/(da-db);Vec candidate{a->x+(b->x-a->x)*t,a->y+(b->y-a->y)*t,a->z+(b->z-a->z)*t};
  if((candidate.y>=edgeSlope*candidate.x&&candidate.y<=edgeSlope*candidate.x+fenceWidth)||infiniteTop){p=candidate;normal={-slopeX,-slopeY,1};normal=normal*(1.f/Length(normal));hit=true;}
 }
 if(a->y<0&&b->y>=0&&a->z<floorZ&&(!thinRoof||a->z>floorZ-.03f)){float t=-a->y/(b->y-a->y);p={a->x+(b->x-a->x)*t,0,a->z+(b->z-a->z)*t};normal={0,-1,0};hit=true;}
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
 // Thin roofs have no wall at the old hard-coded 12 cm depth.
 thinRoof=true;fenceWidth=.02f;
 assert(Find({0,-.01f,1.05f},{0,1,0},point,tangent));fenceWidth=1000.f;
 assert(Find({0,-.08f,1.05f},{0,1,0},point,tangent));assert(std::abs(point.y)<.025f);
 floorZ=50;assert(Find({0,-.08f,50.05f},{0,1,0},point,tangent));assert(std::abs(point.z-50)<.001f);
 floorZ=1;infiniteTop=true;
 assert(!Find({0,-.08f,1.05f},{0,1,0},point,tangent));
 infiniteTop=false;
 // The displayed segment follows both an oblique boundary and the slope,
 // using collision samples rather than the gaze direction or a level bar.
 slopeX=.45f;slopeY=.2f;edgeSlope=.35f;Vec lineA{},lineB{},lineNormal{};
 assert(Find({0,-.08f,1.05f},{0,1,0},point,tangent,idle,&lineA,&lineB,&lineNormal));
 assert(Length(lineB-lineA)>.1f&&std::abs(tangent.z)>.1f);
 for(Vec end:{lineA,lineB}){assert(std::abs(end.y-edgeSlope*end.x)<.02f);assert(std::abs(end.z-(floorZ+slopeX*end.x+slopeY*end.y))<.005f);}
 assert(std::abs(Dot(tangent,lineNormal))<.005f);
 slopeX=slopeY=edgeSlope=0;thinRoof=false;
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
 // Pull down quickly and release: actual accepted body velocity becomes
 // an upward native impulse. A held second hand must prevent this launch.
 Reset();Put(matrix,0x30,Vec{0,-.3f,.8f});noFloor=true;
 hands[0].grip=1;hands[0].gripPos[2]=1.05f;previous[0]={0,-.08f,1.05f};held[0]=true;anchor[0]={0,0,1};
 for(int i=0;i<4;++i){hands[0].gripPos[2]-=.03f;testNow+=.01;Update(true,hands,1,0);}
 assert(ClaimedHands()==1&&Read<Vec>(ped,0x68).z==0);
 hands[0].grip=0;hands[0].gripPos[2]-=.03f;testNow+=.01;Update(true,hands,1,0);
 assert(!ClaimedHands()&&!suspended);assert(Read<Vec>(ped,0x68).z>.04f&&Read<Vec>(ped,0x68).z<=.16f);
 assert(!(Read<std::uint8_t>(ped,0x584)&1));
 // Losing tracking cancels the gesture without launching the player.
 held[0]=true;Suspend(ped);pullVelocity={0,0,4};velocityTime=testNow;
 testNow+=.01;Update(false,hands,1,0);assert(Length(Read<Vec>(ped,0x68))==0);
 // A stationary release has no minimum/artificial boost; stale samples and
 // discontinuous frames cannot carry old momentum into a later release.
 SampleVelocity({},.2f,testNow);assert(Length(ReleaseVelocity(testNow))==0);
 pullVelocity={0,0,4};velocityTime=testNow-.2;assert(Length(ReleaseVelocity(testNow))==0);
 Reset();hands[1].valid=true;hands[1].grip=1;hands[1].gripPos[2]=1;
 held[0]=held[1]=true;previous[0]={hands[0].gripPos[0],hands[0].gripPos[1],hands[0].gripPos[2]};previous[1]={0,0,1};
 hands[0].grip=0;testNow+=.01;Update(true,hands,1,0);assert(ClaimedHands()==2&&Length(Read<Vec>(ped,0x68))==0);
 Reset();noFloor=false;hands[1].valid=false;
 // Fall damage is based on the last support/release, not the old jump.
 char calculator[32]{};float computed{};
 originalDamage=[](void* c,void*,void* out,bool){*static_cast<float*>(out)=Read<float>(c,8);};
 Put(calculator,8,100.f);Put(calculator,0x10,54);
 Put(matrix,0x30,Vec{0,0,20});BeginFall(ped);
 Put(matrix,0x30,Vec{0,0,15});OnDamage(calculator,ped,&computed,false);assert(computed==0&&Read<float>(calculator,8)==100.f);
 BeginFall(ped);Put(matrix,0x30,Vec{0,0,8});OnDamage(calculator,ped,&computed,false);assert(computed==10.f);
 Put(calculator,0x10,22);OnDamage(calculator,ped,&computed,false);assert(computed==100.f);
 Put(calculator,0x10,54);OnDamage(calculator,matrix,&computed,false);assert(computed==100.f);
 assert(handanimation::PalmLockWeight(0)==0&&handanimation::PalmLockWeight(.1f)==1);
 assert(handanimation::PalmLockWeight(.04f)>.4f&&handanimation::PalmLockWeight(.04f)<.6f);
 // A fixed palm frame is published separately; controller rotation remains
 // available to the renderer for the wrist bone.
 held[0]=true;anchor[0]={0,0,1};gripAxis[0]={1,0,0};gripNormal[0]={0,0,1};
 xr::HandPose visual[2];ApplyHandVisuals(visual);assert(visual[0].ledgeGrip&&visual[0].climbGrip);
 assert(visual[0].climbRight[0]>.99f&&visual[0].climbUp[2]>.99f);Reset();
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
