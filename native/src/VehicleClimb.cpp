#include "VehicleClimb.h"
#include "VehicleClimbProfiles.h"
#include "VehicleClimbGeometry.h"
#include "VehicleDebugDrag.h"
#include "Driving.h"
#include "Symbols.h"
#include "VrCamera.h"
#include "Xr.h"
#include <chrono>
#include <cstring>
#include <dlfcn.h>

namespace savr::vehicleclimb {
namespace {
using namespace vehicledoors;
Basis visualFrame;
Vec visualPipeAxis{0,0,1},handHeading[2]{{0,1,0},{0,1,0}};
template<class T>T Read(void* p,int off){T v{};std::memcpy(&v,static_cast<char*>(p)+off,sizeof(v));return v;}
template<class T>void Write(void* p,int off,T v){std::memcpy(static_cast<char*>(p)+off,&v,sizeof(v));}
void (*removeEntity)(void*){};void (*addEntity)(void*){};void* (*ltm)(void*){};
bool (*healthy)(const void*){};
MarkerFrame markers;
GripEdge grip[2],trigger[2];
unsigned claimed{};
std::uint64_t lastEpoch{};
double lastTime{};
void* activeCar{};void* activePool{};int activeSlot=-1;std::uint8_t generation{};
int activeSide{};bool held[2]{};Vec previous[2]{},targetRoot{};
Vec grabRootLocal[2]{},grabTracking[2]{},grabLocal[2]{},grabWorld[2]{};
DebugDrag edit;int control=-1;float editBase[12]{};bool dirty{};
void* suspendedPed{};bool savedCollision{},savedGravity{};

void ResumePhysics(){
    // Player identity can change at respawn. Never restore into a stale ped.
    if(suspendedPed&&g.FindPlayerPed&&g.FindPlayerPed(-1)==suspendedPed&&
       (!g.FindPlayerVehicle||!g.FindPlayerVehicle(-1,false))){
        auto flags=Read<std::uint64_t>(suspendedPed,0x28);
        Write(suspendedPed,0x28,(flags&~std::uint64_t(1))|(savedCollision?1u:0u));
        auto physical=Read<std::uint32_t>(suspendedPed,0x64);
        Write(suspendedPed,0x64,(physical&~2u)|(savedGravity?2u:0u));
    }
    suspendedPed=nullptr;
}
void SuspendPhysics(void* ped){
    if(suspendedPed!=ped){
        ResumePhysics();suspendedPed=ped;
        savedCollision=(Read<std::uint64_t>(ped,0x28)&1)!=0;
        savedGravity=(Read<std::uint32_t>(ped,0x64)&2)!=0;
    }
    // Retail ARM64 ApplyGravity @4a23d0: +64 bit 1 is gravity, +28 bit 0
    // is collision. JetPack::ProcessControlInput tests standing at +584 bit 0.
    // Move() performs our own swept clearance against the world and cab;
    // stock collision response must not eject CJ from the held position.
    Write(ped,0x28,Read<std::uint64_t>(ped,0x28)&~std::uint64_t(1));
    Write(ped,0x64,Read<std::uint32_t>(ped,0x64)&~2u);
    Write(ped,0x584,std::uint8_t(Read<std::uint8_t>(ped,0x584)&~1u));
    Write(ped,0x68,Vec{});
}
double Now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
Vec Position(void* p){void* m=Read<void*>(p,0x18);return Read<Vec>(m?m:p,m?0x30:8);}
void EndEdit(){if(dirty)driving::SaveDebugCalibration();dirty=false;edit={};control=-1;}
void Reset(){ResumePhysics();EndEdit();claimed=0;activeCar=nullptr;activePool=nullptr;activeSlot=-1;held[0]=held[1]=false;}
bool World(Vec p,Vec& out){float a[]={p.x,p.y,p.z},b[3]{};if(!vrcam::TrackingPointToWorld(a,b))return false;out={b[0],b[1],b[2]};return Finite(out);}
Vec Raw(const xr::HandPose& h){return {h.gripPos[0],h.gripPos[1],h.gripPos[2]};}
struct Geometry {Pipe pipe;};
Geometry Shape(Vec hinge,int side,const float c[12],float pipeX=0.f) {
    const float sign=side?1.f:-1.f;
    // linerun.dff: front door hinge |x|=.929, fuel tank/cab details reach
    // |x|=1.449. Keep the pipe at |x|=1.659 outside that shell.
    const float outward=pipeX>0?pipeX-std::abs(hinge.x):.73f;
    Geometry result;
    const Vec base=hinge+Vec{sign*outward,-.65f,-.65f};
    result.pipe={base+Vec{c[6],c[7],c[8]},base+Vec{c[9],c[10],1.05f+c[11]}};
    return result;
}
void Bar(Vec p,Vec axis,Vec up,bool selected=false,float length=.11f){
    if(markers.count>=markers.kCapacity)return;
    Marker m{p,axis,up,selected};m.bikeGrip=true;m.pipeLength=length;markers.markers[markers.count++]=m;
}
// Reject body obstruction as well as swept torso/head paths. The source car is
// deliberately included: pulling through its cab must never be allowed.
bool MoveDirect(void* ped,Vec from,Vec to){
    if(!Finite(to)||Length(to-from)>.36f||!g.CWorld_ProcessLineOfSight||!g.CWorld_TestSphereAgainstWorld||!g.CWorld_pIgnoreEntity)return false;
    void* old=*g.CWorld_pIgnoreEntity;*g.CWorld_pIgnoreEntity=ped;
    bool clear=true;
    for(float z:{-.25f,.45f}){
        GameSymbols::Vec3 a{from.x,from.y,from.z+z},b{to.x,to.y,to.z+z};
        alignas(8) unsigned char col[0x2c]{};void* hit{};
        if(g.CWorld_ProcessLineOfSight(&a,&b,col,&hit,true,true,true,true,true,false,false,false)||
           g.CWorld_TestSphereAgainstWorld(b,.22f,ped,true,true,true,true,true,false)){clear=false;break;}
    }
    *g.CWorld_pIgnoreEntity=old;
    if(!clear)return false;
    removeEntity(ped);void* matrix=Read<void*>(ped,0x18);Write(matrix?matrix:ped,matrix?0x30:8,to);addEntity(ped);
    Vec speed=Read<Vec>(ped,0x68);speed.z=0;Write(ped,0x68,speed);return true;
}
bool Move(void* ped,Vec from,Vec to){
    if(!Finite(to)||!Finite(from))return false;
    // Resolve short sweeps, retaining accepted travel. A small sideways lean
    // into the cab must not cancel an otherwise clear upward pull.
    Vec accepted=from;
    const auto sweep=[&](Vec end){
        const Vec start=accepted,delta=end-start;
        const int steps=std::max(1,int(std::ceil(Length(delta)/.04f)));
        for(int i=1;i<=steps;++i){const Vec next=start+delta*(float(i)/steps);
            if(!MoveDirect(ped,accepted,next))break;
            accepted=next;
        }
    };
    sweep(to);
    if(Length(to-accepted)>.0001f){
        sweep({accepted.x,accepted.y,to.z});
        sweep({to.x,to.y,accepted.z});
    }
    return Length(accepted-to)<.0001f;
}
}
void Install(void* h){
    removeEntity=reinterpret_cast<decltype(removeEntity)>(dlsym(h,"_ZN6CWorld6RemoveEP7CEntity"));
    addEntity=reinterpret_cast<decltype(addEntity)>(dlsym(h,"_ZN6CWorld3AddEP7CEntity"));
    ltm=reinterpret_cast<decltype(ltm)>(dlsym(h,"_Z13RwFrameGetLTMP7RwFrame"));
    healthy=reinterpret_cast<decltype(healthy)>(dlsym(h,"_ZN13CCarEnterExit12IsPedHealthyERK4CPed"));
}
unsigned ClaimedHands(){return claimed;}
bool Editing(){return edit.hand>=0;}
bool Attached(){return held[0]||held[1];}
void PrepareForBoarding(){Reset();for(auto& edge:grip)edge.latched=true;}
void ApplyHandVisuals(xr::HandPose hands[2]){
    for(int h=0;h<2;++h)if(held[h]&&hands[h].valid){
        const Vec shift=visualFrame.Vector({driving::ClimbHandOffsetCm(0)*.01f,driving::ClimbHandOffsetCm(1)*.01f,driving::ClimbHandOffsetCm(2)*.01f});
        const Vec shifted=grabWorld[h]+shift;
        const float world[]={shifted.x,shifted.y,shifted.z};float tracking[3]{};
        if(vrcam::WorldPointToTracking(world,tracking)){
            // Match the standard hand renderer's palm normal: it uses the
            // controller's local right axis with opposite signs per hand.
            // Project that *current* orientation onto the plane of the pipe;
            // no pose captured at grab time can bias the resulting angle.
            const float* q=hands[h].gripOri;const Vec u{q[0],q[1],q[2]},v{1,0,0};
            const float norm=Dot(u,u)+q[3]*q[3];
            if(std::isfinite(norm)&&norm>.5f){
                const Vec direction=(v+(Cross(u,v)*q[3]+Cross(u,Cross(u,v)))*(2.f/norm))*(h==0?-1.f:1.f);
                Vec start{},end{};
                if(World(Raw(hands[h]),start)&&World(Raw(hands[h])+direction,end)){
                    const Vec normal=end-start;
                    const Vec planar=normal-visualPipeAxis*Dot(normal,visualPipeAxis);
                    if(Length(planar)>.05f)handHeading[h]=Cross(planar,visualPipeAxis);
                }
            }
            const Vec heading=handHeading[h];
            const float mirror=h==0?-1.f:1.f;
            const auto frame=PipeHandFrame(visualPipeAxis,heading,mirror*float(driving::ClimbHandAngleDeg(0)),mirror*float(driving::ClimbHandAngleDeg(1)));
            const Vec axes[]={frame.right,frame.forward,frame.up};
            float* outputs[]={hands[h].climbRight,hands[h].climbForward,hands[h].climbUp};
            bool validFrame=true;
            for(int a=0;a<3;++a){const Vec point=shifted+axes[a];const float input[]={point.x,point.y,point.z};float out[3]{};
                if(!vrcam::WorldPointToTracking(input,out)){validFrame=false;break;}
                for(int c=0;c<3;++c)outputs[a][c]=out[c]-tracking[c];
            }
            std::copy_n(tracking,3,hands[h].gripPos);hands[h].grip=1.f;hands[h].trigger=0.f;hands[h].climbGrip=validFrame;
        }
    }
}
void Append(MarkerFrame& frame){if(frame.epoch==markers.epoch)for(int i=0;i<markers.count&&frame.count<frame.kCapacity;++i)frame.markers[frame.count++]=markers.markers[i];}
void Update(bool usable,const xr::HandPose hands[2],std::uint64_t epoch,unsigned occupied){
    const double now=Now();const float dt=float(now-lastTime);lastTime=now;
    markers={};markers.epoch=epoch;claimed=0;
    void* ped=g.FindPlayerPed?g.FindPlayerPed(-1):nullptr;
    usable=usable&&epoch==lastEpoch&&dt>0&&dt<.15f&&ped&&removeEntity&&addEntity&&ltm&&healthy&&healthy(ped)&&
        g.FindPlayerVehicle&&!g.FindPlayerVehicle(-1,false);
    lastEpoch=epoch;
    bool fresh[2]{},editFresh[2]{};const bool editing=driving::GetVehicleDebugMode()!=0;
    for(int h=0;h<2;++h){fresh[h]=grip[h].Step(hands[h].grip,usable&&hands[h].valid);editFresh[h]=trigger[h].Step(hands[h].trigger,usable&&editing&&hands[h].valid);}
    void* pool=g.CPools_ms_pVehiclePool?*g.CPools_ms_pVehiclePool:nullptr;
    if(!usable||!pool||Read<int>(pool,0x10)!=110){Reset();return;}
    auto objects=Read<char*>(pool,0);auto flags=Read<std::uint8_t*>(pool,8);
    if(!objects||!flags){Reset();return;}
    if(activeCar&&(pool!=activePool||activeSlot<0||activeSlot>=110||flags[activeSlot]!=generation||
        objects+activeSlot*0xc68!=activeCar)){Reset();return;}
    if(!editing)EndEdit();
    float origin[3]{},direction[3]{};if(!vrcam::GetHeadWorldRay(origin,direction)){Reset();return;}
    Vec eye{origin[0],origin[1],origin[2]},root=Position(ped);
    // Keep the retained instance while holding/editing; otherwise
    // select only the nearest supported high vehicle and its near side.
    if(!activeCar){
        float best=5.f;
        for(int i=0;i<110;++i)if(!(flags[i]&0x80)){
            void* car=objects+i*0xc68;if(!FindProfile(Read<std::uint16_t>(car,0x32)))continue;
            const float d=Length(Position(car)-eye);if(d<best){best=d;activeCar=car;activeSlot=i;generation=flags[i];activePool=pool;}
        }
        if(!activeCar)return;
        void* matrix=Read<void*>(activeCar,0x18);if(!matrix){Reset();return;}
        const Basis b{Read<Vec>(matrix,0),Read<Vec>(matrix,0x10),Read<Vec>(matrix,0x20),Read<Vec>(matrix,0x30)};
        activeSide=b.Local(eye).x>0?1:0;
    }
    void* matrix=Read<void*>(activeCar,0x18);if(!matrix){Reset();return;}
    const Basis b{Read<Vec>(matrix,0),Read<Vec>(matrix,0x10),Read<Vec>(matrix,0x20),Read<Vec>(matrix,0x30)};
    const Vec speed=Read<Vec>(activeCar,0x68),turn=Read<Vec>(activeCar,0x74);
    if(!b.Valid()||b.up.z<.98f||!Finite(speed)||!Finite(turn)||Length(root-b.position)>6.f){Reset();return;}
    void* node=Read<void*>(activeCar,0x800+(activeSide?8:10)*8);void* door=node?ltm(node):nullptr;
    if(!door){Reset();return;}const Vec hinge=b.Local(Read<Vec>(door,0x30));
    const int model=Read<std::uint16_t>(activeCar,0x32);
    const auto* profile=FindProfile(model);if(!profile){Reset();return;}
    visualFrame=b;visualFrame.right=b.right*(activeSide?1.f:-1.f);
    float c[12]{};driving::GetClimbCalibration(model,activeSide,c);
    Geometry shape=Shape(hinge,activeSide,c,profile->pipeX);
    if(!shape.pipe.Valid()){Reset();return;}
    Vec world[2]{};bool valid[2]{};for(int h=0;h<2;++h)valid[h]=hands[h].valid&&World(Raw(hands[h]),world[h]);
    if(edit.hand>=0){
        const int h=edit.hand;
        if(!valid[h]||!edit.Step(b.Local(world[h]))){Reset();return;}
        std::copy_n(editBase,12,c);const Vec d=edit.delta;
        if(control==5){for(int j:{6,9}){c[j]+=d.x;c[j+1]+=d.y;c[j+2]+=d.z;}}
        else {const int j=control==6?6:9;c[j]+=d.x;c[j+1]+=d.y;c[j+2]+=d.z;}
        const Geometry next=Shape(hinge,activeSide,c,profile->pipeX);
        if(next.pipe.Valid()){driving::SetClimbCalibration(model,activeSide,c);shape=next;dirty=true;}
        claimed=1u<<h;if(hands[h].trigger<=.35f)EndEdit();
    }
    Vec controls[8]{};
    controls[5]=(shape.pipe.bottom+shape.pipe.top)*.5f;controls[6]=shape.pipe.bottom;controls[7]=shape.pipe.top;
    if(editing){
        float best=.13f;int hbest=-1,cbest=-1;
        for(int i=5;i<8;++i){Bar(b.Point(controls[i]),b.right,b.up,control==i&&edit.hand>=0);
            for(int h=0;h<2;++h)if(valid[h]&&editFresh[h]&&!(occupied&(1u<<h))&&edit.hand<0){const float d=Length(world[h]-b.Point(controls[i]));if(d<best){best=d;hbest=h;cbest=i;}}}
        if(hbest>=0){edit.Begin(hbest,b.Local(world[hbest]));control=cbest;std::copy_n(c,12,editBase);claimed=1u<<hbest;xr::TriggerHaptic(hbest,.25f,0,30);}
    }
    Vec axis=shape.pipe.top-shape.pipe.bottom;const float length=Length(axis);axis=axis*(1.f/length);
    visualPipeAxis=b.Vector(axis);
    Vec perpendicular=Cross(axis,{1,0,0});if(Length(perpendicular)<.1f)perpendicular=Cross(axis,{0,1,0});perpendicular=perpendicular*(1.f/Length(perpendicular));
    Bar(b.Point(controls[5]),b.Vector(axis),b.Vector(perpendicular),held[0]||held[1],length);
    auto& pipeMarker=markers.markers[markers.count-1];
    pipeMarker.climbPipe=true;pipeMarker.clipPipe=!editing;
    for(int h=0;h<2;++h)if(valid[h]){
        pipeMarker.pipeHandMask|=1u<<h;
        pipeMarker.pipeHands[h]=held[h]?b.Point(grabLocal[h]):world[h];
    }
    if(edit.hand>=0){held[0]=held[1]=false;ResumePhysics();return;}
    const unsigned wasHeld=(held[0]?1u:0u)|(held[1]?2u:0u);
    Vec motion{};int count=0;
    for(int h=0;h<2;++h){
        if(!valid[h]||(occupied&(1u<<h))||hands[h].grip<=.35f)held[h]=false;
        if(valid[h]&&fresh[h]&&!(occupied&(1u<<h))&&shape.pipe.Distance(b.Local(world[h]))<.12f){
            if(!held[0]&&!held[1])targetRoot=root;
            held[h]=true;previous[h]=Raw(hands[h]);xr::TriggerHaptic(h,.25f,0,30);
            SuspendPhysics(ped);
            grabRootLocal[h]=b.Local(root);grabTracking[h]=Raw(hands[h]);
            handHeading[h]=b.forward;

            const Vec pipeAxis=shape.pipe.top-shape.pipe.bottom;
            const Vec local=b.Local(world[h]);
            grabLocal[h]=shape.pipe.bottom+pipeAxis*std::clamp(Dot(local-shape.pipe.bottom,pipeAxis)/Dot(pipeAxis,pipeAxis),0.f,1.f);
        }
        if(!held[h])continue;
        Vec delta{},startWorld{};
        if(!PullDelta(previous[h],Raw(hands[h]),dt,delta)||!World(grabTracking[h],startWorld)){held[h]=false;continue;}
        previous[h]=Raw(hands[h]);
        grabWorld[h]=b.Point(grabLocal[h]);
        motion=motion+b.Point(grabRootLocal[h])+startWorld-world[h];++count;claimed|=1u<<h;
    }
    if(count){
        SuspendPhysics(ped);
        if(count==1&&wasHeld==3){
            for(int h=0;h<2;++h)if(held[h]){grabRootLocal[h]=b.Local(root);grabTracking[h]=Raw(hands[h]);}
            motion=root;
        }
        Vec next=motion*(1.f/count);
        if(Move(ped,root,next)){root=next;targetRoot=next;}
        else {
            root=Position(ped);
            // Consume blocked displacement rather than storing an invisible
            // spring that launches the player when the obstacle clears.
            for(int h=0;h<2;++h)if(held[h]){grabRootLocal[h]=b.Local(root);grabTracking[h]=Raw(hands[h]);}
            targetRoot=root;
        }
        Write(ped,0x68,Vec{});
    }
    if(count)SuspendPhysics(ped);else ResumePhysics();
    if(!count&&edit.hand<0){activeCar=nullptr;activeSlot=-1;}
}
}
