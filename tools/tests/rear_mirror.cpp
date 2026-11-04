#include "RearMirrorPolicy.h"
#include "VehicleDoorMotion.h"
#include "VehicleDebugDrag.h"
#include "Symbols.h"
#include "Driving.h"
#include <array>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
#define LOGI(...) ((void)0)
namespace savr {
GameSymbols g{};
namespace driving {
bool enabled=true,third=false;int debug=0;
bool RearMirrorEnabled(){return enabled;}
int RearMirrorQuality(){return 1;}
bool ThirdPersonViewActive(){return third;}
int GetActiveVehicleType(){return VEHICLE_CAR;}
int RearMirrorFov(){return 80;}
int GetVehicleDebugMode(){return debug;}
void GetRearMirrorCalibration(int,int out[12]) {
    const int values[]={0,45,110,0,0,0,0,-200,100,0,0,0};
    std::copy(std::begin(values),std::end(values),out);
}
}
namespace perf {double ThreadCpuMs(){return 0;}}
using V3=vehicledoors::Vec;
using vehicledoors::Dot;using vehicledoors::Finite;
struct CamBasis {V3 right,forward,up,pos;};
struct SkyBackdropVertex {V3 position,normal;std::uint32_t color;float u,v;};
constexpr std::uintptr_t kRetailWorldSectorsOffset=0xa1a908,
    kRetailWorldRepeatSectorsOffset=0xa52d08,kRetailScanSectorListOffset=0x4b5c6c,
    kRetailTimeModelInfoVptrOffset=0x8226f8,kRetailLodTimeModelInfoVptrOffset=0x8227a8;
template<class T> void Put(void* p,std::size_t o,T v){std::memcpy(static_cast<char*>(p)+o,&v,sizeof(v));}
CamBasis ReadCam(std::uintptr_t cam){CamBasis b;std::memcpy(&b,reinterpret_cast<void*>(cam),sizeof(b));return b;}
void WriteCam(std::uintptr_t cam,const CamBasis& b){std::memcpy(reinterpret_cast<void*>(cam),&b,sizeof(b));}
void Copy(void*,bool){}
auto g_origCopy=&Copy;
bool FormalCutsceneRunning(){return false;}
bool ScriptedCutsceneRunning(){return false;}
unsigned EyeRasterTexId(void* p){return p?1:0;}
bool ReadEntityWorldPosition(const void* e,V3* p){std::memcpy(p,static_cast<const char*>(e)+8,sizeof(*p));return Finite(*p);}
struct MobileDistanceFogSnapshot {float start=100,end=300,color[3]{.2f,.3f,.4f};bool valid=true;};
MobileDistanceFogSnapshot fogState;
auto CaptureMobileDistanceFog(){return fogState;}
bool ApplyMobileDistanceFog(const MobileDistanceFogSnapshot& f,float start,float end){fogState=f;fogState.start=start;fogState.end=end;return true;}
class ScopedScreenDimensions {
    int* p;int w,h;
public:
    explicit ScopedScreenDimensions(void* v):p(static_cast<int*>(v)+2),w(p[0]),h(p[1]){}
    void Set(int x,int y){p[0]=x;p[1]=y;}
    ~ScopedScreenDimensions(){p[0]=w;p[1]=h;}
};
// PRODUCTION_ADAPTER
}
using namespace savr;
namespace r=savr::rear_mirror_render;
struct Entity {
    alignas(16) std::array<char,0x80> e{},mi{},col{},frame{};
    std::array<char,0xb0> atomic{};
    Entity(int id,V3 pos,int type=1,float radius=4) {
        Put(e.data(),8,pos);Put(e.data(),0x32,static_cast<std::int16_t>(id));
        Put(e.data(),0x5a,static_cast<std::uint8_t>(type));Put(e.data(),0x28,std::uint32_t{128});
        Put(mi.data(),0x30,static_cast<void*>(col.data()));Put(col.data(),0x24,radius);
        Put(e.data(),0x20,static_cast<void*>(atomic.data()));
        Put(atomic.data(),0,std::uint8_t{1});Put(atomic.data(),2,std::uint8_t{4});
        Put(atomic.data(),8,static_cast<void*>(frame.data()));
        g.CModelInfo_ms_modelInfoPtrs[id]=mi.data();
    }
};
int main() {
    using namespace savr::rear_mirror;
    const float tx=TanX(80);
    CHECK(InView(0,0,20,1,tx));CHECK(!InView(0,0,-20,1,tx));
    CHECK(!InView(0,0,90,1,tx));CHECK(!InView(40,0,20,1,tx));
    CHECK(InView(20,0,20,4,tx)); // bound intersects side plane
    CHECK(!InView(0,0,std::numeric_limits<float>::quiet_NaN(),1,tx));
    CHECK(InView(0,0,72,4,tx));CHECK(SmallProp(.5f,30));
    CHECK(!SmallProp(6,30));CHECK(TanX(0)==TanX(40));CHECK(TanX(180)==TanX(120));

    std::vector<char> world(0xa56000);g.LoadBase=reinterpret_cast<std::uintptr_t>(world.data());
    std::array<void*,20000> models{};g.CModelInfo_ms_modelInfoPtrs=models.data();
    r::rear={{1,0,0},{0,-1,0},{0,0,1},{0,0,0}};r::tanX=tx;
    Entity road(1,{0,-30,0}),child(2,{0,-35,0}),parent(3,{0,-32,0},1,12),car(400,{0,-20,0},2),front(5,{0,30,0}),tiny(6,{0,-35,0},5,.5f);
    Put(child.e.data(),0x50,static_cast<void*>(parent.e.data()));
    r::Consider(road.e.data());r::Consider(road.e.data());
    r::Consider(child.e.data());r::Consider(car.e.data());r::Consider(front.e.data());r::Consider(tiny.e.data());
    CHECK(r::mapCount==2&&r::trafficCount==1);CHECK(r::map[1].entity==parent.e.data());
    // Medium always keeps detail, admits objects beyond LOW's 70 m and does
    // not prune small scenery. It never substitutes the loaded LOD parent.
    r::quality=2;r::farClip=FarFor(2);r::mapCount=0;
    r::Consider(child.e.data());r::Consider(tiny.e.data());
    Entity distant(8,{0,-95,0},1,3);r::Consider(distant.e.data());
    CHECK(r::mapCount==3&&r::map[0].entity==child.e.data());
    r::quality=1;r::farClip=Far;r::mapCount=0;
    r::Consider(distant.e.data());CHECK(r::mapCount==0);
    // At 120 degrees, an invisible LOD must not remove a visible child.
    r::tanX=TanX(120);Put(parent.e.data(),8,V3{0,60,0});
    r::Consider(child.e.data());CHECK(r::mapCount==1&&r::map[0].entity==child.e.data());
    // Camera-adjacent road coverage keeps detail even with a resident parent.
    Put(parent.e.data(),8,V3{0,-10,0});r::mapCount=0;r::Consider(child.e.data());
    CHECK(r::mapCount==1&&r::map[0].entity==child.e.data());
    // Large road bounds intersecting the camera have priority over small props.
    Entity largeRoad(9,{0,-80,0},1,82);float priority=999;
    CHECK(r::Bounds(largeRoad.e.data(),largeRoad.mi.data(),priority)&&priority==0);
    Put(parent.e.data(),8,V3{0,-32,0});r::tanX=tx;
    r::mapCount=0;r::Consider(road.e.data());r::Consider(child.e.data());
    CHECK(r::Read<std::uint32_t>(road.e.data(),0x28)==128);
    // Collision bounds are not necessarily centred on the entity origin.
    Entity offset(7,{0,20,0},1,3);Put(offset.col.data(),0x18,V3{0,-45,0});
    r::Consider(offset.e.data());CHECK(r::mapCount==3);
    // A hidden/timed parent must fall back to its visible child.
    Put(parent.mi.data(),0,g.LoadBase+kRetailTimeModelInfoVptrOffset);
    r::mapCount=0;r::Consider(child.e.data());CHECK(r::mapCount==1&&r::map[0].entity==child.e.data());
    // Request-only LOD loading has a hard frame budget; no synchronous loading.
    Put(parent.mi.data(),0,std::uintptr_t{0});Put(parent.e.data(),0x20,static_cast<void*>(nullptr));
    int requested=0;static int* requestCount=&requested;
    g.CStreaming_RequestModel=[](int,int){++*requestCount;};
    r::requests=0;for(int i=0;i<10;++i)r::Consider(child.e.data());CHECK(requested==MaxRequests);
    // Candidate cap retains closer objects without overrunning storage.
    std::array<r::Candidate,2> bounded{};int count=0;
    r::Insert(bounded,count,road.e.data(),50);r::Insert(bounded,count,child.e.data(),40);r::Insert(bounded,count,car.e.data(),10);
    CHECK(count==2);CHECK(bounded[0].entity==car.e.data());

    // Surface and camera translations stay in car axes when yaw is adjusted.
    CamBasis basis{{0,1,0},{-1,0,0},{0,0,1},{10,20,30}};
    const int position[]={100,200,300,0,90,0};const auto pose=r::Pose(basis,position);
    CHECK(std::fabs(pose.pos.x-8)<.001f&&std::fabs(pose.pos.y-21)<.001f&&std::fabs(pose.pos.z-33)<.001f);
    CHECK(std::fabs(Dot(pose.right,pose.forward))<.001f);
    for(int pitch : {-75,0,65})for(int yaw : {-80,0,70})for(int roll : {-60,0,85}) {
        const int input[]={pitch,yaw,roll};int output[3]{};
        const auto b=vehicledoors::MirrorOrientation(input);
        vehicledoors::MirrorAngles(b,output);
        CHECK(output[0]==pitch&&output[1]==yaw&&output[2]==roll);
    }

    // A far vehicle with a VLO atomic draws that atomic alone; near vehicles
    // keep high detail, and HD replacements without VLO retain their body.
    std::array<char,0xb0> high{},low{};
    char highName[]="chassis_hi",lowName[]="chassis_vlo";
    Put(high.data(),2,std::uint8_t{4});Put(low.data(),2,std::uint8_t{4});
    Put(high.data(),8,static_cast<void*>(highName));Put(low.data(),8,static_cast<void*>(lowName));
    g.GetFrameNodeName=[](void* f)->const char*{return static_cast<const char*>(f);};
    r::api.frameLtm=[](void* f)->void*{return f;};
    static std::vector<void*> drawn;
    g.AtomicDefaultRenderCallBack=[](void* a)->void*{drawn.push_back(a);return a;};
    r::AtomicContext ac{true,true,false};r::FindLow(high.data(),&ac);r::FindLow(low.data(),&ac);
    r::DrawAtomic(high.data(),&ac);r::DrawAtomic(low.data(),&ac);
    CHECK(drawn.size()==1&&drawn[0]==low.data());
    drawn.clear();ac.low=false;r::DrawAtomic(high.data(),&ac);r::DrawAtomic(low.data(),&ac);
    CHECK(drawn.size()==1&&drawn[0]==high.data());
    drawn.clear();ac.low=true;ac.hasLow=false;r::DrawAtomic(high.data(),&ac);
    CHECK(drawn.size()==1&&drawn[0]==high.data());
    Put(high.data(),2,std::uint8_t{0});r::DrawAtomic(high.data(),&ac);CHECK(drawn.size()==1);

    // Actual adapter renders into a temporary camera target and restores every
    // projection/raster/clip byte, head transform, dimensions and fog values.
    std::array<char,0xc0> camera{};CamBasis head{{-1,0,0},{0,1,0},{0,0,1},{0,0,1}};
    g.TheCamera=&head;int screen[4]={0,0,1920,1080};g.RsGlobal=screen;
    Put(camera.data(),0xa8,.05f);Put(camera.data(),0xac,500.f);
    const auto originalCamera=camera;const auto originalHead=head;const auto originalFog=fogState;
    g.RwCameraSetNearClipPlane=[](void* c,float n){Put(c,0xa8,n);};
    g.RwCameraSetFarClipPlane=[](void* c,float n){Put(c,0xac,n);};
    g.RwCameraClear=[](void*,void*,int){};g.RwCameraBeginUpdate=[](void*){};g.RwCameraEndUpdate=[](void*){};
    r::api.ambient=[](){};r::api.directionalOff=[](){};
    static std::map<int,std::uintptr_t> states;
    states={{1,123},{6,0},{8,1},{9,1},{12,1},{14,1},{20,2}};
    g.RwRenderStateSet=[](int s,void* v){states[s]=reinterpret_cast<std::uintptr_t>(v);return 1;};
    g.RwRenderStateGet=[](int s,void* out){Put(out,0,states[s]);return 1;};
    g.RwIm3DTransform=[](void* v,unsigned,void*,unsigned)->void*{return v;};
    g.RwIm3DRenderIndexedPrimitive=[](int,unsigned short*,int){return 1;};g.RwIm3DEnd=[](){return 1;};
    r::visible=true;r::raster=road.atomic.data();r::Render(camera.data(),reinterpret_cast<std::uintptr_t>(&head));
    CHECK(camera==originalCamera);CHECK(std::memcmp(&head,&originalHead,sizeof(head))==0);
    CHECK(screen[2]==1920&&screen[3]==1080);CHECK(fogState.start==originalFog.start&&fogState.end==originalFog.end);
    const auto savedStates=states;r::DrawSurface();CHECK(states==savedStates);
    // Two sectors pointing at the same road yield one render candidate. The
    // collector does not touch the entity's retail visibility scan stamp.
    std::array<void*,2> node{road.e.data(),nullptr};
    Put(world.data()+kRetailWorldSectorsOffset,(59*120+60)*0x10,node.data());
    Put(world.data()+kRetailWorldSectorsOffset,(60*120+60)*0x10,node.data());
    Put(road.e.data(),0x48,std::uint16_t{321});r::Collect();
    CHECK(r::mapCount==1&&r::map[0].entity==road.e.data());CHECK(r::Read<std::uint16_t>(road.e.data(),0x48)==321);
    // Disabled/third-person paths cannot reuse a stale visible surface.
    driving::enabled=false;CHECK(!r::Prepare(head)&&!r::visible);
    driving::enabled=true;driving::third=true;r::visible=true;CHECK(!r::Prepare(head)&&!r::visible);
    std::puts("rear mirror: frustum, LOD/fallback, request cap, bounds, pose, camera/state restoration PASS");
}
