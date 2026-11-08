#include "LedgeClimb.h"
#include "Symbols.h"
#include "VrCamera.h"
#include "Xr.h"
#include <chrono>
#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include "Log.h"

namespace savr::ledgeclimb {
namespace {
using namespace vehicledoors;
template<class T>T Read(const void* p,int off){T v{};std::memcpy(&v,static_cast<const char*>(p)+off,sizeof(v));return v;}
template<class T>void Write(void* p,int off,T v){std::memcpy(static_cast<char*>(p)+off,&v,sizeof(v));}
void (*removeEntity)(void*){};void (*addEntity)(void*){};
bool (*canClimb)(void*,unsigned){};void* surfaces{};
GripEdge edges[2];bool held[2]{};Vec anchor[2]{},previous[2]{},inward[2]{};
void* suspended{};bool collision{},gravity{};unsigned claimed{};
Vec collisionHeadOffset{0,0,.55f};
MarkerFrame markers;double last{};std::uint64_t lastEpoch{};
double Now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
Vec Position(void* p){void* m=Read<void*>(p,0x18);return Read<Vec>(m?m:p,m?0x30:8);}
bool World(Vec p,Vec& out){float a[]{p.x,p.y,p.z},b[3]{};if(!vrcam::TrackingPointToWorld(a,b))return false;out={b[0],b[1],b[2]};return Finite(out);}
void Resume(){
 if(suspended&&g.FindPlayerPed&&g.FindPlayerPed(-1)==suspended){
  Write(suspended,0x28,(Read<std::uint64_t>(suspended,0x28)&~std::uint64_t(1))|(collision?1u:0u));
  Write(suspended,0x64,(Read<std::uint32_t>(suspended,0x64)&~2u)|(gravity?2u:0u));
 }
 suspended=nullptr;
}
void Reset(){Resume();held[0]=held[1]=false;claimed=0;}
void Suspend(void* ped){
 if(suspended!=ped){Resume();suspended=ped;collision=(Read<std::uint64_t>(ped,0x28)&1)!=0;gravity=(Read<std::uint32_t>(ped,0x64)&2)!=0;}
 Write(ped,0x28,Read<std::uint64_t>(ped,0x28)&~std::uint64_t(1));
 Write(ped,0x64,Read<std::uint32_t>(ped,0x64)&~2u);Write(ped,0x68,Vec{});
}
bool Hook(void* target,const std::uint32_t (&expected)[4],void* hook,void*& original) {
    if (!target||std::memcmp(target,expected,16)) return false;
    const long size=sysconf(_SC_PAGESIZE);
    if (size<=0) return false;
    void* trampoline=mmap(nullptr,size,PROT_READ|PROT_WRITE|PROT_EXEC,
                          MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if (trampoline==MAP_FAILED) return false;
    auto* t=static_cast<std::uint32_t*>(trampoline);
    std::memcpy(t,target,16); t[4]=0x58000051; t[5]=0xd61f0220;
    const auto resume=reinterpret_cast<std::uintptr_t>(target)+16;
    std::memcpy(t+6,&resume,sizeof(resume));
    __builtin___clear_cache(static_cast<char*>(trampoline),static_cast<char*>(trampoline)+32);
    const auto address=reinterpret_cast<std::uintptr_t>(target);
    const auto start=address&~(static_cast<std::uintptr_t>(size)-1);
    const auto end=(address+16+size-1)&~(static_cast<std::uintptr_t>(size)-1);
    if (mprotect(reinterpret_cast<void*>(start),end-start,PROT_READ|PROT_WRITE|PROT_EXEC)) {
        munmap(trampoline,size); return false;
    }
    original=trampoline;
    auto* code=static_cast<std::uint32_t*>(target);
    std::memcpy(code+2,&hook,sizeof(hook)); code[1]=0xd61f0220;
    __atomic_store_n(code,0x58000051u,__ATOMIC_RELEASE);
    __builtin___clear_cache(static_cast<char*>(target),static_cast<char*>(target)+16);
    return true;
}

using ClimbFn=void* (*)(void*,Vec&,float&,unsigned char&,bool);
using LaunchFn=void (*)(void*,void*);
ClimbFn originalClimb{};LaunchFn originalLaunch{};
bool hooksReady{};bool installed{};double jumpUntil{},hoverUntil{};bool hovered{};
void* OnClimb(void* ped,Vec& point,float& angle,unsigned char& surface,bool launch){
 if(hooksReady&&xr::IsSessionFocused()&&g.FindPlayerPed&&ped==g.FindPlayerPed(-1)&&vrcam::GroundWalkControlsActive())return nullptr;
 return originalClimb(ped,point,angle,surface,launch);
}
void OnLaunch(void* task,void* ped){
 const bool local=hooksReady&&xr::IsSessionFocused()&&g.FindPlayerPed&&ped==g.FindPlayerPed(-1)&&vrcam::GroundWalkControlsActive();
 if(local&&claimed)return;
 const Vec before=Read<Vec>(ped,0x68);
 if(local){float eye[3]{},dir[3]{};if(vrcam::GetHeadWorldRay(eye,dir)&&std::hypot(dir[0],dir[1])>.1f){
  const float yaw=std::atan2(-dir[0],dir[1]);
  auto orient=reinterpret_cast<void (*)(void*,float,float,float)>(dlsym(RTLD_DEFAULT,"_ZN10CPlaceable14SetOrientationEfff"));
  if(orient)orient(ped,0,0,yaw);Write(ped,0x6c4,yaw);Write(ped,0x6c8,yaw);
 }}
 originalLaunch(task,ped);
 Vec speed=Read<Vec>(ped,0x68);
 if(local&&speed.z>before.z+.001f&&speed.z>0){speed.z*=1.224744871f;Write(ped,0x68,speed);jumpUntil=Now()+3.;hovered=false;hoverUntil=0;}
}

struct Hit {Vec point{},normal{};unsigned surface{};};
bool Ray(Vec a,Vec b,Hit& out,bool obstacles=false){
 alignas(8) unsigned char col[0x2c]{};void* entity{};
 GameSymbols::Vec3 from{a.x,a.y,a.z},to{b.x,b.y,b.z};
 if(!g.CWorld_ProcessLineOfSight(&from,&to,col,&entity,true,obstacles,obstacles,true,obstacles,false,false,false))return false;
 out={Read<Vec>(col,0),Read<Vec>(col,0x10),Read<unsigned char>(col,0x23)};
 return Finite(out.point)&&Finite(out.normal);
}
bool Clear(void* ped,Vec root){
 for(float z:{-.55f,0.f,.55f})if(g.CWorld_TestSphereAgainstWorld({root.x,root.y,root.z+z},.22f,ped,true,true,true,true,true,false))return false;
 return true;
}
void Place(void* ped,Vec p){removeEntity(ped);void* m=Read<void*>(ped,0x18);Write(m?m:ped,m?0x30:8,p);addEntity(ped);Write(ped,0x68,Vec{});}
// Native body collision is suspended while gripping. Only the tracked head
// keeps a small swept collision volume; animation legs cannot snag a fence.
bool Move(void* ped,Vec from,Vec to){
 if(!Finite(to)||Length(to-from)>.65f)return false;
 const int count=std::max(1,int(std::ceil(Length(to-from)/.035f)));Vec accepted=from;
 for(int i=1;i<=count;++i){Vec p=from+(to-from)*(float(i)/count);Hit hit;
  Vec head=p+collisionHeadOffset;
  if(g.CWorld_TestSphereAgainstWorld({head.x,head.y,head.z},.12f,ped,true,true,true,true,true,false)||
     Ray(accepted+collisionHeadOffset,head,hit,true))break;
  accepted=p;
 }
 Place(ped,accepted);return Length(accepted-to)<.005f;
}
// The tracked player can be crouched above the top while CJ's animation feet
// are still below it. Lift to a verified support plane before moving across.
bool Stand(void* ped,Vec root,Vec eye,float edgeZ){
 if(eye.z<edgeZ+.18f)return false;
 Hit floor;
 if(!Ray({eye.x,eye.y,edgeZ+.12f},{eye.x,eye.y,edgeZ-.18f},floor)||floor.normal.z<.7f||std::abs(floor.point.z-edgeZ)>.12f)return false;
 Vec target{eye.x,eye.y,floor.point.z+1.02f};
 if(std::hypot(target.x-root.x,target.y-root.y)>.7f||target.z-root.z>1.65f||target.z<root.z-.15f||!Clear(ped,target))return false;
 // Check the head path above the edge; the lower body is lifted vertically
 // on the near side before crossing, never swept through the wall.
 const Vec raised{root.x,root.y,target.z};Hit hit;
 if(!Clear(ped,raised))return false;
 if(Ray(eye,{target.x,target.y,std::max(eye.z,target.z+.55f)},hit,true))return false;
 for(float z:{-.55f,0.f,.55f})if(Ray(raised+Vec{0,0,z},target+Vec{0,0,z},hit,true))return false;
 Place(ped,target);Resume();Write(ped,0x584,std::uint8_t(Read<std::uint8_t>(ped,0x584)|1u));
 held[0]=held[1]=false;claimed=0;return true;
}
struct Reach {float grab{},preview{},vertical{};};
Reach ReachFor(Vec velocity,bool airborne){
 const float speed=Finite(velocity)?std::min(Length(velocity)*50.f,8.f):0.f;
 const float extra=std::min(speed*.04f,.26f)+(airborne?.16f:0.f);
 return {.18f+extra,.48f+extra,.55f+extra};
}
bool Find(Vec hand,Vec forward,Vec& point,Vec& tangent,Reach reach={.18f,.48f,.55f}){
 float best=reach.preview;bool found=false;
 // Fan out from the hand rather than requiring the ledge to face the gaze.
 // Probe higher as well: roof overhangs often sit above the jumping hand.
 for(float angle:{0.f,-.65f,.65f,-1.3f,1.3f})for(float fraction:{.15f,.5f,.9f}){
  Vec direction=Rotate(forward,angle);
  Vec probe=hand+direction*(reach.preview*fraction);Hit top,wall;
  if(!Ray(probe+Vec{0,0,reach.vertical},probe-Vec{0,0,reach.vertical},top)||top.normal.z<.65f)continue;
  // The stock material flag excludes some physically solid roofs. For VR,
  // an exposed collision edge and a solid top are the eligibility criteria.
  Vec start{hand.x-direction.x*reach.preview,hand.y-direction.y*reach.preview,top.point.z-.12f};
  if(!Ray(start,{probe.x,probe.y,top.point.z-.12f},wall)||std::abs(wall.normal.z)>.45f)continue;
  Vec edge{wall.point.x,wall.point.y,top.point.z};const float d=Length(edge-hand);
  Vec axis=Cross({0,0,1},wall.normal);const float n=Length(axis);
  if(d>=best||n<.1f)continue;
  best=d;point=edge;tangent=axis*(1.f/n);found=true;
 }
 return found;
}
}
void Install(void* h){
 if(installed)return;installed=true;
 removeEntity=reinterpret_cast<decltype(removeEntity)>(dlsym(h,"_ZN6CWorld6RemoveEP7CEntity"));
 addEntity=reinterpret_cast<decltype(addEntity)>(dlsym(h,"_ZN6CWorld3AddEP7CEntity"));
 canClimb=reinterpret_cast<decltype(canClimb)>(dlsym(h,"_ZN14SurfaceInfos_c8CanClimbEj"));surfaces=dlsym(h,"g_surfaceInfos");
 const std::uint32_t climbCode[]{0xd10243ff,0x6d032beb,0x6d0423e9,0xa9057bfd};
 const std::uint32_t launchCode[]{0xd10283ff,0x6d023bef,0x6d0333ed,0x6d042beb};
 void* climb=dlsym(h,"_ZN16CTaskSimpleClimb12TestForClimbEP4CPedR7CVectorRfRhb");
 void* launch=dlsym(h,"_ZN15CTaskSimpleJump6LaunchEP4CPed");
 if(removeEntity&&addEntity&&canClimb&&surfaces&&climb&&launch&&!std::memcmp(climb,climbCode,16)&&!std::memcmp(launch,launchCode,16)){
  void* c{};void* l{};
  const bool a=Hook(climb,climbCode,reinterpret_cast<void*>(&OnClimb),c);originalClimb=reinterpret_cast<ClimbFn>(c);
  const bool b=Hook(launch,launchCode,reinterpret_cast<void*>(&OnLaunch),l);originalLaunch=reinterpret_cast<LaunchFn>(l);
  hooksReady=a&&b;
 }
 LOGI("[ledge] hand climbing %s",hooksReady?"ready":"DISABLED: retail hooks unavailable");
}
unsigned ClaimedHands(){return claimed;}
void Append(vehicledoors::MarkerFrame& f){if(f.epoch==markers.epoch)for(int i=0;i<markers.count&&f.count<f.kCapacity;++i)f.markers[f.count++]=markers.markers[i];}
void ApplyHandVisuals(xr::HandPose hands[2]){
 for(int h=0;h<2;++h)if(held[h]){float a[]{anchor[h].x,anchor[h].y,anchor[h].z},out[3]{};if(vrcam::WorldPointToTracking(a,out)){std::copy_n(out,3,hands[h].gripPos);hands[h].grip=1.f;}}
}
void Update(bool usable,const xr::HandPose hands[2],std::uint64_t epoch,unsigned occupied){
 const double now=Now();const float dt=float(now-last);last=now;markers={};markers.epoch=epoch;
 void* ped=g.FindPlayerPed?g.FindPlayerPed(-1):nullptr;
 usable=usable&&hooksReady&&epoch==lastEpoch&&dt>0&&ped&&removeEntity&&addEntity&&canClimb&&surfaces&&g.CWorld_ProcessLineOfSight&&g.CWorld_TestSphereAgainstWorld&&g.CWorld_pIgnoreEntity&&vrcam::GroundWalkControlsActive()&&(!g.CPed_IsAlive||g.CPed_IsAlive(ped));lastEpoch=epoch;
 bool fresh[2]{};for(int h=0;h<2;++h)fresh[h]=edges[h].Step(hands[h].grip,usable&&hands[h].valid);
 if(!usable){Reset();jumpUntil=hoverUntil=0;return;}
 if(now<jumpUntil&&!held[0]&&!held[1]){
  Vec velocity=Read<Vec>(ped,0x68);
  if(!hovered&&velocity.z<=.01f&&velocity.z>=-.025f){hovered=true;hoverUntil=now+.08;}
  if(now<hoverUntil){velocity.z=0;Write(ped,0x68,velocity);}
 }
 void* saved=*g.CWorld_pIgnoreEntity;*g.CWorld_pIgnoreEntity=ped;
 float a[3]{},f[3]{};if(!vrcam::GetHeadWorldRay(a,f)){*g.CWorld_pIgnoreEntity=saved;Reset();return;}
 Vec eye{a[0],a[1],a[2]},forward{f[0],f[1],0};float n=Length(forward);if(n>.1f)forward=forward*(1.f/n);else forward={0,1,0};
 Vec root=Position(ped),motion{};int count=0;claimed=0;
 collisionHeadOffset=eye-root;
 const Vec speed=Read<Vec>(ped,0x68);
 const bool airborne=(Read<std::uint8_t>(ped,0x584)&1)==0||std::abs(speed.z)>.015f;
 const Reach reach=ReachFor(speed,airborne);
 const bool wasHeld=held[0]||held[1];
 const float releaseZ=held[0]?anchor[0].z:anchor[1].z;
 for(int h=0;h<2;++h){
  Vec raw{hands[h].gripPos[0],hands[h].gripPos[1],hands[h].gripPos[2]},world{};
  const bool valid=hands[h].valid&&World(raw,world)&&!(occupied&(1u<<h));
  if(!valid||hands[h].grip<=.35f)held[h]=false;
  Vec point{},tangent{};
  if(valid&&!held[h]&&Find(world,forward,point,tangent,reach)){
   Marker m{point,tangent,{0,0,1},false};m.bikeGrip=true;m.climbPipe=true;m.ledge=true;m.pipeLength=.24f;m.opacity=.45f;markers.markers[markers.count++]=m;
   if(fresh[h]&&Length(point-world)<reach.grab){held[h]=true;anchor[h]=point;inward[h]=forward;previous[h]=raw;xr::TriggerHaptic(h,.25f,0,30);}
  }
  if(!held[h])continue;
  if(dt>.15f)previous[h]=raw;
  Vec prevWorld{};if(!World(previous[h],prevWorld)){previous[h]=raw;claimed|=1u<<h;++count;continue;}
  if(Length(raw-previous[h])>.65f){previous[h]=raw;claimed|=1u<<h;++count;continue;}
  previous[h]=raw;motion=motion+prevWorld-world;++count;claimed|=1u<<h;
 }
 if(count){Suspend(ped);Move(ped,root,root+motion*(1.f/count));Write(ped,0x68,Vec{});}
 else {if(wasHeld){Stand(ped,root,eye,releaseZ);}Resume();}
 *g.CWorld_pIgnoreEntity=saved;
}
}
