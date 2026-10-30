#include "VehicleTestArea.h"
#include "VehicleTestAreaLogic.h"
#include "VehicleTestAreaLog.h"
#include "HolsterBody.h"
#include "Symbols.h"
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
#define LOGI(...) ((void)0)
#define LOGE(...) ((void)0)
#define SAVR_TESTAREA_LOG_PATH "vehicle_test_area.log"
namespace savr {
GameSymbols g;
namespace xr {
struct HandPose {bool valid{true};float gripPos[3]{};float trigger{};};
bool focused=true,menu{};
vehicledoors::MarkerFrame markers;
bool IsSessionFocused(){return focused;}
bool IsMenuVisible(){return menu;}
void SetVehicleTestMarkers(const vehicledoors::MarkerFrame& f){markers=f;}
}
namespace physicalweapon {
unsigned held{};
unsigned HeldHandMaskRelaxed(){return held;}
bool IsSupportHand(int){return false;}
}
namespace basketball {unsigned HeldHandMask(){return 0;}}
namespace driving {
int editor{},saves{};
int GetVehicleDebugMode(){return editor;}
void CycleVehicleDebugMode(int){editor=!editor;}
void ResetInteraction(){}
void SaveDebugCalibration(){++saves;}
}
namespace vehicledoors {
bool editing{};
bool DebugEditActive(){return editing;}
unsigned ClaimedHands(){return 0;}
void Reset(){editing=false;}
}
namespace vrcam {
bool control=true,tracking=true;
int recenters{};
bool GroundWalkControlsActive(){return control;}
void RequestRecenter(){++recenters;}
bool TrackingPointToWorld(const float in[3],float out[3]) {
    out[0]=in[0];out[1]=-2800-in[2];out[2]=80+in[1];return tracking;
}
bool WorldPointToTracking(const float in[3],float out[3]) {
    out[0]=in[0];out[1]=in[2]-80;out[2]=-2800-in[1];return tracking;
}
}
}
// PRODUCTION_ADAPTER
using namespace savr;
using namespace savr::testarea;
namespace {
struct FakePool {
    alignas(8) char header[0x20]{};
    std::vector<char> bytes;
    std::vector<std::uint8_t> flags;
    int stride;
    void* pointer=header;
    FakePool(int count,int size):bytes(count*size),flags(count,0x80),stride(size) {
        Write(header,0,bytes.data());Write(header,8,flags.data());Write(header,0x10,count);
    }
    void Reset(){std::fill(bytes.begin(),bytes.end(),char{});std::fill(flags.begin(),flags.end(),std::uint8_t{0x80});}
    int Live() const {int n=0;for(auto f:flags)n+=!(f&0x80);return n;}
    void* Allocate() {
        for(int i=0;i<int(flags.size());++i)if(flags[i]&0x80) {
            flags[i]=static_cast<std::uint8_t>((flags[i]+1)&0x7f);
            void* p=bytes.data()+i*stride;std::memset(p,0,stride);return p;
        }
        return nullptr;
    }
    void Free(void* p){const auto slot=(static_cast<char*>(p)-bytes.data())/stride;CHECK(slot>=0&&slot<int(flags.size()));CHECK(!(flags[slot]&0x80));flags[slot]|=0x80;}
} objects(180,0x220),vehicles(110,0xc68),peds(1,0x9c8);
std::array<std::array<char,0x80>,4000> info;
std::array<void*,4000> models;
std::array<int,4000> refs;
std::array<char,0x38> collision;
std::vector<std::uint8_t> streaming(26000*0x14);
void* seated{};void* actor{};
bool alive=true,mission{},loads=true,floorHit=true,returnHit=true,failVehicle{};
bool incrementalLoads{};
bool deferAmbulance{};
int currentArea{},modelBalance{},collisionBalance{},deletedCars{},deletedTiles{},saveCalls{},restarts{},teleports{};
void DestroyCar(void* p){CHECK(p!=seated);++deletedCars;vehicles.Free(p);}
void* carVtable[2]{nullptr,reinterpret_cast<void*>(&DestroyCar)};
void* NewCar(int id,unsigned char createdBy) {
    CHECK(Loaded(id));
    if(failVehicle)return nullptr;
    void* p=vehicles.Allocate();CHECK(p);Write(p,0,carVtable);Write<std::uint16_t>(p,0x32,static_cast<std::uint16_t>(id));Write(p,0x610,createdBy);return p;
}
void* NewObject(int id,bool temporary) {
    CHECK(id==kFloorModel&&!temporary);void* p=objects.Allocate();CHECK(p);
    Write<std::uint32_t>(p,0x64,0x80000c); // real stock a51_jetdoor flags, before our old bad override
    return p;
}
void DestroyObject(void* p){++deletedTiles;objects.Free(p);}
void Move(void* p,float x,float y,float z,unsigned char){Write(p,8,Vec{x,y,z});}
void MoveActor(void* p,float x,float y,float z,unsigned char c){CHECK(p==actor);++teleports;Move(p,x,y,z,c);}
void Rotate(void* p,float,float,float z){Write(p,0x14,z);}
bool Ray(const GameSymbols::Vec3* start,const GameSymbols::Vec3*,void* point,void** hit,
         bool buildings,bool carsFlag,bool pedsFlag,bool objectFlag,bool,bool,bool,bool) {
    CHECK(buildings&&!carsFlag&&!pedsFlag&&objectFlag);
    if(std::abs(start->x-100)<5&&std::abs(start->y-200)<5) {
        if(!returnHit)return false;
        Write(point,8,14.f);Write(point,0x18,1.f);*hit=reinterpret_cast<void*>(1);return true;
    }
    if(!floorHit)return false;
    for(int i=0;i<int(objects.flags.size());++i)if(!(objects.flags[i]&0x80)) {
        void* p=objects.bytes.data()+i*objects.stride;const auto pos=Position(p);
        if(std::abs(start->x-pos.x)<4.5f&&std::abs(start->y-pos.y)<4.5f) {
            Write(point,8,kFloorZ);*hit=p;return true;
        }
    }
    return false;
}
void Setup() {
    state=State::Off;publishedState=State::Off;notice=Notice::None;toggleRequested=false;pageRequested=0;
    objects.Reset();vehicles.Reset();peds.Reset();actor=peds.Allocate();
    Move(actor,100,200,15,0);Write(actor,0x14,.7f);seated=nullptr;
    alive=loads=floorHit=returnHit=true;mission=failVehicle=false;currentArea=0;
    incrementalLoads=deferAmbulance=false;refs={};pendingRefCount=requestedPinCount=0;
    deletedCars=deletedTiles=modelBalance=collisionBalance=teleports=0;
    testarea::floor={};cars={};refCount=0;colSlot=-1;teleported=died=pageResourcesReady=false;player={};catalogue={};
    ClearMarkers();navEpoch=0;driving::editor=0;vrcam::tracking=vrcam::control=xr::focused=true;xr::menu=false;
    physicalweapon::held=0;vehicledoors::editing=false;
    std::fill(streaming.begin(),streaming.end(),std::uint8_t{});
    std::fill(models.begin(),models.end(),nullptr);
    for(int id=400;id<=422;++id) {
        info[id]={};models[id]=info[id].data();Write(models[id],0x6c,id==417?3:0);
        Write(models[id],0x30,collision.data());
    }
    models[kFloorModel]=info[kFloorModel].data();
    collision={};Write(collision.data(),0,Vec{-4.4926f,-4.4944f,.0034f});
    Write(collision.data(),12,Vec{4.5074f,4.5056f,.5554f});Write<std::uint8_t>(collision.data(),0x28,5);
    Write(collision.data(),0x30,collision.data());Write(models[kFloorModel],0x30,collision.data());
    g={};g.CModelInfo_ms_modelInfoPtrs=models.data();g.CStreaming_ms_aInfoForModel=streaming.data();
    g.CStreaming_RequestModel=[](int id,int flags){
        streaming[id*0x14+6]|=static_cast<std::uint8_t>(flags);
        if(loads)streaming[id*0x14+0x10]=(incrementalLoads||(id==416&&deferAmbulance))?2:1;
    };
    g.FindPlayerPed=[](int)->void*{return actor;};g.FindPlayerVehicle=[](int,bool)->void*{return seated;};
    g.PlayerIsEnteringCar=[](){return false;};g.CPed_IsAlive=[](void*){return alive;};
    g.CPools_ms_pVehiclePool=&vehicles.pointer;g.CWorld_ProcessLineOfSight=Ray;
    g.CStreaming_LoadScene=[](const GameSymbols::Vec3*){};g.CObject_SetIsStatic=[](void*,bool){};
    api={};api.ready=true;available=true;api.object=NewObject;api.vehicle=NewCar;
    api.remove=[](void*){};api.deleteObject=DestroyObject;api.objectTeleport=api.carTeleport=Move;
    api.pedTeleport=MoveActor;api.orientation=Rotate;api.createRw=[](void*){};
    api.addModelRef=[](void* mi){++modelBalance;++refs[(static_cast<char*>(mi)-info[0].data())/0x80];};
    api.releaseModel=[](void* mi){--modelBalance;CHECK(modelBalance>=0);--refs[(static_cast<char*>(mi)-info[0].data())/0x80];};
    api.colAddRef=[](int){++collisionBalance;};api.colRemoveRef=[](int){--collisionBalance;CHECK(collisionBalance>=0);};
    api.area=&currentArea;api.onMission=[](){return mission;};api.canStart=[](void*){return true;};
    api.loadCollision=[](const GameSymbols::Vec3*){};
    api.releaseMission=[](int id){streaming[id*0x14+6]&=~std::uint8_t{4};};
    api.objectPool=&objects.pointer;pedPool=&peds.pointer;
    originalSave=[](int){++saveCalls;return true;};
    originalRestart=[](){++restarts;objects.Reset();vehicles.Reset();};
}
void Enter(){RequestToggle();for(int i=0;i<100&&state!=State::Active;++i)Tick(true,false);CHECK(state==State::Active);}
void SetPage(int step){RequestPage(step);Tick(true,false);for(int i=0;i<20&&state==State::Changing;++i)Tick(true,false);}
void Exit(){RequestToggle();Tick(true,false);CHECK(state==State::Off);}
}
int main() {
    std::remove(SAVR_TESTAREA_LOG_PATH);
    const char* rotation="vehicle_test_area_rotation.log";
    std::remove(rotation);std::remove("vehicle_test_area_rotation.log.previous");
    CHECK(diagnostics::Append(rotation,"first entry",1));
    CHECK(diagnostics::Append(rotation,"second entry",1));
    FILE* rotated=std::fopen("vehicle_test_area_rotation.log.previous","r");CHECK(rotated);std::fclose(rotated);
    Catalogue c;for(int id=400;id<=611;++id)c.Add(id,id==417?3:0);
    CHECK(c.count==211&&c.Pages()==22&&c.Size(21)==1&&c.Model(1,7)==418);
    CHECK(c.Next(0,-1)==21&&c.Next(21,1)==0&&c.Model(-1,0)==-1);
    Recovery guard;
    CHECK(guard.Step(kArrival,0,false)==Recovery::None);
    const Vec fallen{kCentreX,kCentreY,kFloorZ-5};
    CHECK(guard.Step(fallen,1,false)==Recovery::None);
    CHECK(guard.Step(kArrival,1.1,false)==Recovery::None); // transient boundary sample
    CHECK(guard.Step(fallen,2,false)==Recovery::None);
    CHECK(guard.Step(fallen,2.8,false)==Recovery::Rescue);
    CHECK(guard.Step(kArrival,3,false)==Recovery::None);
    CHECK(guard.Step(fallen,4,false)==Recovery::None);
    CHECK(guard.Step(fallen,4.8,false)==Recovery::Leave);
    CHECK(guard.Step(fallen,5,true)==Recovery::None);
    Setup();RequestToggle();Tick(true,true);CHECK(state==State::Off&&teleports==0);
    for(int i=0;i<100&&state!=State::Active;++i)Tick(true,false);
    CHECK(state==State::Active&&objects.Live()==kTileCount&&vehicles.Live()==10);
    for(const auto& tile:testarea::floor) {
        void* entity=tile.Pointer(Objects());CHECK(entity);
        CHECK(Read<std::uint32_t>(entity,0x64)==0x80000c); // no disable-Z/special-collision bit
    }
    CHECK(modelBalance==11&&collisionBalance==1&&driving::editor==1);
    CHECK(teleports==1&&std::abs(Position(actor).z-kArrival.z)<.001f);
    CHECK(!OnSave(0)&&notice==Notice::ExitToSave);
    // Bounds/collision are actually probed at the arrival and each parking bay.
    CHECK(CheckFloor());
    const auto first=cars[0];void* parked=first.Pointer(Vehicles());
    seated=parked;SetPage(1);CHECK(page==0&&deletedCars==0&&notice==Notice::LeaveCar);seated=nullptr;
    vehicledoors::editing=true;SetPage(1);CHECK(page==0&&notice==Notice::EditorBusy);vehicledoors::editing=false;
    loads=false;RequestPage(1);Tick(true,false);CHECK(state==State::Changing&&deletedCars==0);
    loads=true;SetPage(1);CHECK(page==1&&deletedCars==10&&vehicles.Live()==10&&objects.Live()==kTileCount);
    CHECK(modelBalance==11&&!first.Pointer(Vehicles()));
    SetPage(1);CHECK(page==2&&vehicles.Live()==2&&catalogue.Size(page)==2&&modelBalance==3);
    SetPage(1);CHECK(page==0&&vehicles.Live()==10);
    SetPage(-1);CHECK(page==2&&vehicles.Live()==2);
    SetPage(-1);CHECK(page==1&&vehicles.Live()==10);
    // A replacement slot is external ownership even if it retains our address.
    const auto retained=cars[0];vehicles.flags[retained.slot]++;
    const int deletedBefore=deletedCars;SetPage(-1);
    CHECK(deletedCars-deletedBefore==9&&vehicles.Live()==11);
    CHECK(!retained.Pointer(Vehicles()));
    Exit();CHECK(objects.Live()==0&&vehicles.Live()==1&&modelBalance==0&&collisionBalance==0&&driving::editor==0);
    CHECK(teleports==2&&std::abs(Position(actor).x-100)<.001f&&std::abs(Read<float>(actor,0x14)-.7f)<.001f);
    CHECK(OnSave(0));
    // Reproduce real streaming pressure: at most one new model per tick, with
    // every unreferenced resident evicted before the following load. Waiting
    // until all ten are loaded to AddRef never completes this test.
    Setup();RequestPage(1);CHECK(pageRequested==0);Enter();
    incrementalLoads=true;RequestPage(1);
    CHECK(std::string(PageLabel())=="CLOSE MENU TO LOAD PAGE");
    Tick(true,true);CHECK(state==State::Active&&page==0);
    Tick(true,false);CHECK(state==State::Changing);
    CHECK(std::string(PageLabel()).find("LOADING 2/")!=std::string::npos);
    for(int frame=0;frame<40&&state==State::Changing;++frame) {
        for(int id=400;id<=422;++id)if(Loaded(id)&&refs[id]==0)streaming[id*0x14+0x10]=0;
        for(int id=400;id<=422;++id)if(streaming[id*0x14+0x10]==2) {
            streaming[id*0x14+0x10]=1;break;
        }
        Tick(true,false);
    }
    CHECK(state==State::Active&&page==1&&vehicles.Live()==10&&pendingRefCount==0&&modelBalance==11);
    Exit();CHECK(modelBalance==0);
    // A delayed 416 leaves exactly its original parking slot empty. Later
    // entries retain their catalogue slots; no native factory sees an unloaded model.
    Setup();Enter();deferAmbulance=true;RequestPage(1);Tick(true,false);
    CHECK(state==State::Changing&&!Loaded(416)&&(streaming[416*0x14+6]&4));
    modelDeadline=Now()-1;Tick(true,false);
    for(int i=0;i<20&&state==State::Changing;++i)Tick(true,false);
    CHECK(page==1&&state==State::Active&&vehicles.Live()==9&&publishedSkipped==1);
    CHECK(!cars[6].Pointer(Vehicles())&&skipped[6]);
    CHECK(Read<std::uint16_t>(cars[7].Pointer(Vehicles()),0x32)==418);
    CHECK(requestedPinCount==0&&!(streaming[416*0x14+6]&4));Exit();
    // Do not release a mission pin belonging to another native owner.
    Setup();Enter();streaming[416*0x14+6]|=4;SetPage(1);Exit();
    CHECK((streaming[416*0x14+6]&4)!=0);
    // A mostly failed page still opens with the one ready car; skipped requests
    // and temporary refs are released instead of leaving the switch stuck.
    Setup();Enter();loads=false;RequestPage(1);Tick(true,false);
    streaming[410*0x14+0x10]=1;Tick(true,false);CHECK(pendingRefCount>0&&refs[410]==1);
    modelDeadline=Now()-1;Tick(true,false);
    for(int i=0;i<20&&state==State::Changing;++i)Tick(true,false);
    CHECK(state==State::Active&&page==1&&vehicles.Live()==1&&publishedSkipped==9&&pendingRefCount==0&&refs[410]==1&&modelBalance==2);Exit();
    CHECK(requestedPinCount==0&&!(streaming[416*0x14+6]&4));
    Setup();Enter();const int entryTeleports=teleports;
    for(int i=0;i<50;++i)Tick(true,false);
    CHECK(teleports==entryTeleports); // standing in the area never recentres
    Move(actor,kCentreX,kCentreY,kFloorZ-5,0);recovery.outsideSince=Now()-1;Tick(true,false);
    CHECK(teleports==entryTeleports+1&&state==State::Active);
    Move(actor,kCentreX,kCentreY,kFloorZ-5,0);recovery.outsideSince=Now()-1;Tick(true,false);
    CHECK(state==State::Returning&&teleports==entryTeleports+1);
    Tick(true,false);CHECK(state==State::Off&&teleports==entryTeleports+2);

    Setup();Enter();loads=false;RequestPage(1);Tick(true,false);seated=cars[0].Pointer(Vehicles());
    loads=true;Tick(true,false);CHECK(page==0&&state==State::Active&&deletedCars==0);seated=nullptr;Exit();
    Setup();Write<std::uint8_t>(collision.data(),0x28,0);Enter();CHECK(collisionBalance==0);Exit();
    Setup();Enter();returnHit=false;RequestToggle();Tick(true,false);
    CHECK(state==State::Active&&notice==Notice::ReturnFailed&&objects.Live()==kTileCount&&teleports==1);
    returnHit=true;Exit();CHECK(objects.Live()==0&&modelBalance==0);

    Setup();floorHit=false;RequestToggle();for(int i=0;i<100;++i)Tick(true,false);
    CHECK(state==State::Off&&notice==Notice::FloorFailed&&teleports==0&&objects.Live()==0&&modelBalance==0);
    Setup();loads=false;RequestToggle();Tick(true,false);deadline=Now()-1;Tick(true,false);
    CHECK(state==State::Off&&notice==Notice::LoadFailed&&collisionBalance==0);
    Setup();Enter();failVehicle=true;RequestPage(1);Tick(true,false);Tick(true,false);
    for(int i=0;i<20&&state==State::Changing;++i)Tick(true,false);
    CHECK(state==State::Active&&page==1&&objects.Live()==kTileCount&&vehicles.Live()==0&&publishedSkipped==10&&teleports==1);
    Exit();CHECK(modelBalance==0);
    Setup();Enter();alive=false;Tick(true,false);CHECK(died&&objects.Live()==kTileCount);
    alive=true;Move(actor,200,200,15,0);Tick(true,false);CHECK(state==State::Off&&teleports==1&&objects.Live()==0);
    Setup();Enter();OnRestart();CHECK(state==State::Off&&objects.Live()==0&&modelBalance==0&&collisionBalance==0);
    Setup();mission=true;RequestToggle();Tick(true,false);CHECK(state==State::Off&&notice==Notice::FreeRoam);
    Setup();vehicles.flags.assign(110,1);RequestToggle();Tick(true,false);CHECK(state==State::Off&&notice==Notice::PoolFull);

    // Real navigation code: both hands, tracking epoch, single press, held item,
    // editor exclusion and invalidation of immutable marker batches.
    for(int hand=0;hand<2;++hand) {
        Setup();Enter();xr::HandPose hands[2]{};float head[]{0,1.7f,0},q[]{0,0,0,1};
        UpdateNavigation(false,hands,head,q,true,1);UpdateNavigation(false,hands,head,q,true,1);
        CHECK(xr::markers.count==2&&pageRequested==0);
        const auto& m=xr::markers.markers[1];const auto target=m.position+m.direction*.09f;
        hands[hand].gripPos[0]=target.x;hands[hand].gripPos[1]=target.y;hands[hand].gripPos[2]=target.z;
        hands[hand].trigger=1;
        CHECK(UpdateNavigation(false,hands,head,q,true,1)&&pageRequested==1);
        pageRequested=0;UpdateNavigation(false,hands,head,q,true,1);CHECK(pageRequested==0);
        hands[hand].trigger=0;UpdateNavigation(false,hands,head,q,true,1);
        physicalweapon::held=1u<<hand;hands[hand].trigger=1;
        UpdateNavigation(false,hands,head,q,true,1);CHECK(pageRequested==0);
        UpdateNavigation(false,hands,head,q,false,2);CHECK(xr::markers.count==0);
        physicalweapon::held=0;UpdateNavigation(false,hands,head,q,true,2);
        CHECK(pageRequested==0);Exit();
    }
    std::puts("vehicle test area: catalogue, session, collision, ownership, failures, save/restart and both-hand navigation passed");
    FILE* log=std::fopen(SAVR_TESTAREA_LOG_PATH,"r");CHECK(log);
    std::string contents;char chunk[1024];while(std::fgets(chunk,sizeof(chunk),log))contents+=chunk;std::fclose(log);
    CHECK(contents.find("resource-timeout after 30s")!=std::string::npos);
    CHECK(contents.find("id=3095 loaded=")!=std::string::npos);
    CHECK(contents.find("id=25005 loaded=")!=std::string::npos);
    CHECK(contents.find("id=400 loaded=")!=std::string::npos);
    CHECK(contents.find("notice=model-load-failed")!=std::string::npos);
    CHECK(contents.find("skipped model id=416 slot=7 reason=load-timeout-5s")!=std::string::npos);
    std::puts("vehicle test area: persistent release diagnostics, resource IDs, failure notices and log rotation passed");
}
