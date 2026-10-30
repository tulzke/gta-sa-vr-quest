#include "VehicleTestArea.h"
#include "VehicleTestAreaLogic.h"
#include "VehicleTestAreaLog.h"
#include "Basketball.h"
#include "Driving.h"
#include "HolsterBody.h"
#include "Log.h"
#include "PhysicalWeapon.h"
#include "Symbols.h"
#include "VehicleDoors.h"
#include "VrCamera.h"
#include "Xr.h"
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

namespace savr::testarea {
namespace {
using V=GameSymbols::Vec3;
enum class State { Off, Preparing, Active, Changing, Returning };
enum class Notice { None, Unavailable, LeaveCar, FreeRoam, PoolFull, LoadFailed,
                    FloorFailed, SpawnFailed, ExitToSave, Outside, EditorBusy, ReturnFailed };
struct Api {
    void* (*object)(int,bool){};
    void* (*vehicle)(int,unsigned char){};
    void (*remove)(void*){},(*deleteObject)(void*){};
    void (*objectTeleport)(void*,float,float,float,unsigned char){};
    void (*carTeleport)(void*,float,float,float,unsigned char){};
    void (*pedTeleport)(void*,float,float,float,unsigned char){};
    void (*orientation)(void*,float,float,float){};
    void (*createRw)(void*){};
    void (*releaseModel)(void*){};
    void (*addModelRef)(void*){};
    void (*colAddRef)(int){},(*colRemoveRef)(int){};
    bool (*onMission)(){};
    bool (*canStart)(void*){};
    void (*loadCollision)(const V*){};
    void (*releaseMission)(int){};
    void** objectPool{};
    int* area{};
    bool ready{};
} api;
Catalogue catalogue;
State state=State::Off;
std::atomic<State> publishedState{State::Off};
std::atomic<Notice> notice{Notice::None};
std::atomic<bool> toggleRequested{};
std::atomic<int> pageRequested{}, publishedPage{}, publishedPages{}, firstId{},lastId{};
std::atomic<int> publishedTarget{};
std::atomic<int> failedModel{-1};
std::atomic<int> publishedSkipped{};
std::atomic<bool> available{};
int page{},targetPage{},spawnIndex{},tileIndex{},colSlot{-1};
std::array<OwnedEntity,kTileCount> floor;
std::array<OwnedEntity,kPageSize> cars;
std::array<int,kPageSize+1> modelRefs{};
int refCount{};
std::array<int,kPageSize+1> pendingRefs{};
int pendingRefCount{};
std::array<int,kPageSize+1> requestedPins{};
int requestedPinCount{};
std::array<bool,kPageSize> skipped{};
double modelDeadline{};
// Model infos survive restart; native references are released before shutdown.
OwnedEntity player;
void** pedPool{};
V returnPosition{};
float returnHeading{};
int previousEditor{};
bool teleported{},died{},pageResourcesReady{};
Recovery recovery;
double nextPositionLog{};
int positionSamples{};
double deadline{},nextResourceLog{};
vehicledoors::MarkerFrame worldMarkers;
holsterbody::Body body;
vehicledoors::GripEdge edges[2];
std::uint64_t navEpoch{};
bool navClaim{};
bool (*originalSave)(int){};
void (*originalRestart)(){};

#ifndef SAVR_TESTAREA_LOG_PATH
#define SAVR_TESTAREA_LOG_PATH "/sdcard/Android/data/com.rockstargames.gtasa/files/vr_vehicle_test_area.log"
#endif
const char* StateName(State value) {
    switch(value) {
        case State::Off:return "off";case State::Preparing:return "preparing";
        case State::Active:return "active";case State::Changing:return "changing";
        case State::Returning:return "returning";
    }
    return "unknown";
}
const char* NoticeName(Notice value) {
    switch(value) {
        case Notice::None:return "none";case Notice::Unavailable:return "unavailable";
        case Notice::LeaveCar:return "leave-vehicle";case Notice::FreeRoam:return "free-roam-only";
        case Notice::PoolFull:return "pool-full";case Notice::LoadFailed:return "model-load-failed";
        case Notice::FloorFailed:return "floor-check-failed";case Notice::SpawnFailed:return "vehicle-spawn-failed";
        case Notice::ExitToSave:return "exit-before-saving";case Notice::Outside:return "outside-area";
        case Notice::EditorBusy:return "editor-busy";case Notice::ReturnFailed:return "return-point-failed";
    }
    return "unknown";
}
void Trace(bool error,const char* format,...) {
    char detail[640]{},line[768]{};
    va_list args;va_start(args,format);std::vsnprintf(detail,sizeof(detail),format,args);va_end(args);
    std::snprintf(line,sizeof(line),"%s state=%s page=%d target=%d %s",error?"ERROR":"INFO",
        StateName(state),page+1,targetPage+1,detail);
    const bool written=diagnostics::Append(SAVR_TESTAREA_LOG_PATH,line);
#ifdef __ANDROID__
    // Deliberately independent of LOGI/SAVR_DEV: field diagnostics must survive
    // a release build, a game restart and logcat ring-buffer turnover.
    __android_log_print(error?ANDROID_LOG_ERROR:ANDROID_LOG_INFO,SAVR_TAG,"[testarea] %s",line);
    if(!written)__android_log_print(ANDROID_LOG_ERROR,SAVR_TAG,"[testarea] cannot write %s",SAVR_TESTAREA_LOG_PATH);
#else
    (void)written;
#endif
}

double Now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
template<class T> T Read(const void* p,int offset) {
    T value{};if(p)std::memcpy(&value,static_cast<const char*>(p)+offset,sizeof(value));return value;
}
template<class T> void Write(void* p,int offset,T value) {
    std::memcpy(static_cast<char*>(p)+offset,&value,sizeof(value));
}
PoolView Pool(void** source,int stride) {
    void* p=source?*source:nullptr;
    if(!p)return {};
    const int count=Read<int>(p,0x10);
    if(count<=0||count>20000)return {};
    return {p,Read<char*>(p,0),Read<const std::uint8_t*>(p,8),count,stride};
}
PoolView Objects() {return Pool(api.objectPool,0x220);}
PoolView Vehicles() {return Pool(g.CPools_ms_pVehiclePool,0xc68);}
int Free(PoolView p) {
    if(!p.identity||!p.objects||!p.flags)return 0;
    int n=0;for(int i=0;i<p.count;++i)n+=(p.flags[i]&0x80)!=0;return n;
}
Vec Position(void* p) {
    void* matrix=Read<void*>(p,0x18);
    return matrix?Read<Vec>(matrix,0x30):Read<Vec>(p,8);
}
void ChangeState(State value) {
    if(state!=value)Trace(false,"transition %s -> %s",StateName(state),StateName(value));
    publishedTarget=targetPage;state=value;publishedState.store(value);
}
void Inform(Notice n) {
    if(notice.exchange(n)!=n)Trace(n!=Notice::None,"notice=%s",NoticeName(n));
}
bool Seated() {return g.FindPlayerVehicle&&g.FindPlayerVehicle(-1,false);}
void ClearMarkers() {
    worldMarkers={};body.Invalidate();navClaim=false;
    for(auto& edge:edges)edge.latched=true;
    xr::SetVehicleTestMarkers({});
}
void ResetEditors() {vehicledoors::Reset();driving::ResetInteraction();driving::SaveDebugCalibration();}
void RestoreEditor() {
    if(driving::GetVehicleDebugMode()!=previousEditor)driving::CycleVehicleDebugMode(1);
}
void ReleaseModels() {
    for(int i=0;i<refCount;++i) {
        void* mi=g.CModelInfo_ms_modelInfoPtrs[modelRefs[i]];
        if(mi)api.releaseModel(mi);
    }
    refCount=0;
}
void ReleasePendingModels() {
    for(int i=0;i<pendingRefCount;++i) {
        if(void* mi=g.CModelInfo_ms_modelInfoPtrs[pendingRefs[i]])api.releaseModel(mi);
    }
    pendingRefCount=0;
}
void ReleaseRequestPins() {
    // Preserve mission ownership that existed before our request. Do not clear
    // KEEP_IN_MEMORY/game flags or edit the streaming list links ourselves.
    for(int i=0;i<requestedPinCount;++i)api.releaseMission(requestedPins[i]);
    requestedPinCount=0;
}
void ReleaseCollision() {
    if(colSlot>0)api.colRemoveRef(colSlot);
    colSlot=-1;
}
void Delete(OwnedEntity& entity,bool vehicle) {
    if(void* p=entity.Pointer(vehicle?Vehicles():Objects())) {
        api.remove(p);
        // Only the supported automobile/monster factory classes are admitted.
        // Virtual deleting destructor is required for their derived allocation.
        if(vehicle) {
            auto** vtable=Read<void**>(p,0);
            reinterpret_cast<void(*)(void*)>(vtable[1])(p);
        } else api.deleteObject(p);
    }
    entity={};
}
void DeleteCars() {for(auto& car:cars)Delete(car,true);}
void DeleteFloor() {for(auto& tile:floor)Delete(tile,false);}
void Finish() {
    ClearMarkers();ResetEditors();DeleteCars();DeleteFloor();ReleasePendingModels();ReleaseModels();ReleaseRequestPins();ReleaseCollision();
    RestoreEditor();player={};teleported=false;died=false;pageResourcesReady=false;
    toggleRequested=false;pageRequested=0;ChangeState(State::Off);
}
void Failed(Notice reason) {
    Inform(reason);
    if(teleported)ChangeState(State::Returning);
    else Finish();
}
bool Loaded(int id) {
    return g.CStreaming_ms_aInfoForModel[id*0x14+0x10]==1;
}
void TraceResources(const char* reason) {
    Trace(false,"resources=%s col_slot=%d free_objects=%d free_vehicles=%d tiles=%d spawn_index=%d",
        reason,colSlot,Free(Objects()),Free(Vehicles()),tileIndex,spawnIndex);
    const auto model=[&](int id) {
        if(id<0||!g.CStreaming_ms_aInfoForModel)return;
        const auto* entry=g.CStreaming_ms_aInfoForModel+id*0x14;
        void* mi=id<20000&&g.CModelInfo_ms_modelInfoPtrs?g.CModelInfo_ms_modelInfoPtrs[id]:nullptr;
        void* col=Read<void*>(mi,0x30);
        Trace(false,"resource id=%d loaded=%u flags=0x%02x image=%u sectors=%u model_info=%d rw=%d col=%d col_data=%d",
            id,entry[0x10],entry[6],entry[7],Read<std::uint32_t>(entry,0xc),mi!=nullptr,
            Read<void*>(mi,0x40)!=nullptr,col!=nullptr,Read<void*>(col,0x30)!=nullptr);
    };
    model(kFloorModel);if(colSlot>0)model(25000+colSlot);
    for(int i=0;i<catalogue.Size(targetPage);++i)model(catalogue.Model(targetPage,i));
}
void RequestResources() {
    const auto request=[&](int id) {
        if(!Loaded(id)) {
            bool owned=false;
            for(int i=0;i<requestedPinCount;++i)owned|=requestedPins[i]==id;
            if(!owned&&!(g.CStreaming_ms_aInfoForModel[id*0x14+6]&4))requestedPins[requestedPinCount++]=id;
            // Match native scripted vehicle loading. Ordinary traffic requests
            // can be dropped before they ever reach LOADED (field case: 416).
            g.CStreaming_RequestModel(id,4);return;
        }
        // The next page has no vehicle instances yet. Retain each model as soon
        // as it becomes resident, rather than waiting for all ten to coincide:
        // ordinary streaming otherwise evicts the earlier ones while loading
        // later ones (seen alternating REQUESTED/READING/LOADED until timeout).
        for(int i=0;i<pendingRefCount;++i)if(pendingRefs[i]==id)return;
        if(void* mi=g.CModelInfo_ms_modelInfoPtrs[id]) {
            api.addModelRef(mi);pendingRefs[pendingRefCount++]=id;
            Trace(false,"retained-loading id=%d pending=%d",id,pendingRefCount);
        }
    };
    request(kFloorModel);
    if(colSlot>0&&!Loaded(25000+colSlot))g.CStreaming_RequestModel(25000+colSlot,0);
    for(int i=0;i<catalogue.Size(targetPage);++i) {
        const int id=catalogue.Model(targetPage,i);
        if(!skipped[i])request(id);
    }
}
bool ResourcesReady() {
    if(!Loaded(kFloorModel)||colSlot<0||(colSlot>0&&!Loaded(25000+colSlot)))return false;
    for(int i=0;i<catalogue.Size(targetPage);++i)if(!skipped[i]&&!Loaded(catalogue.Model(targetPage,i)))return false;
    return true;
}
int MissingModel() {
    if(!Loaded(kFloorModel))return kFloorModel;
    if(colSlot>0&&!Loaded(25000+colSlot))return 25000+colSlot;
    for(int i=0;i<catalogue.Size(targetPage);++i) {
        const int id=catalogue.Model(targetPage,i);if(!skipped[i]&&!Loaded(id))return id;
    }
    return -1;
}
void Retain(int model) {
    api.addModelRef(g.CModelInfo_ms_modelInfoPtrs[model]);modelRefs[refCount++]=model;
}
bool FloorBounds(Vec& min,Vec& max) {
    void* col=Read<void*>(g.CModelInfo_ms_modelInfoPtrs[kFloorModel],0x30);
    if(!col||!Read<void*>(col,0x30))return false;
    min=Read<Vec>(col,0);max=Read<Vec>(col,12);
    return vehicledoors::Finite(min)&&vehicledoors::Finite(max)&&
        max.x-min.x>8.99f&&max.x-min.x<9.01f&&max.y-min.y>8.99f&&max.y-min.y<9.01f&&
        max.z-min.z>.5f&&max.z-min.z<.6f;
}
bool BuildTiles() {
    Vec min,max;if(!FloorBounds(min,max)){
        Trace(true,"floor bounds rejected id=%d min=(%.3f %.3f %.3f) max=(%.3f %.3f %.3f)",
            kFloorModel,min.x,min.y,min.z,max.x,max.y,max.z);
        Failed(Notice::FloorFailed);return false;
    }
    // Four tiles/tick keeps scene creation off the rendering thread and bounded.
    for(int n=0;n<4&&tileIndex<kTileCount;++n,++tileIndex) {
        if(Free(Objects())<16){Failed(Notice::PoolFull);return false;}
        void* tile=api.object(kFloorModel,false);
        if(!floor[tileIndex].Capture(tile,Objects())){Failed(Notice::FloorFailed);return false;}
        Write<std::uint8_t>(tile,0x1a0,6); // temporary, explicitly owned, not saved
        Write<std::uint64_t>(tile,0x28,Read<std::uint64_t>(tile,0x28)|1u); // uses collision
        Write<std::uint8_t>(g.CModelInfo_ms_modelInfoPtrs[kFloorModel],0x26,255);
        g.CObject_SetIsStatic(tile,true);
        // Preserve the stock 99999 kg tile's physical flags. Retail bit 7 is
        // NOT infinite mass: ApplyMoveForce suppresses its Z response and
        // CObject::SpecialEntityPreCollisionStuff takes a special collision
        // path (2.11.311 @4a117c / @53b930). Static stock physics is sufficient.
        api.orientation(tile,0,0,0);
        const float x=kCentreX+(tileIndex%kTilesX-(kTilesX-1)*.5f)*kTilePitch-(min.x+max.x)*.5f;
        const float y=kCentreY+(tileIndex/kTilesX-(kTilesY-1)*.5f)*kTilePitch-(min.y+max.y)*.5f;
        if(!Read<void*>(tile,0x20))api.createRw(tile);
        // CObject::Teleport performs sector remove/add and updates the RW frame.
        api.objectTeleport(tile,x,y,kFloorZ-max.z,0);
        if(tileIndex==0)Trace(false,"floor tile entity_flags=0x%llx physical_flags=0x%x",
            static_cast<unsigned long long>(Read<std::uint64_t>(tile,0x28)),Read<std::uint32_t>(tile,0x64));
    }
    return tileIndex==kTileCount;
}
bool IsFloorEntity(void* p) {
    const auto pool=Objects();
    for(const auto& tile:floor)if(tile.Pointer(pool)==p&&p)return true;
    return false;
}
bool Ground(Vec p,float& z) {
    // FindGroundZFor3DCoord only scans buildings in retail. Our floor is made
    // of static CObjects, so explicitly include objects in this vertical ray.
    alignas(8) std::uint8_t point[0x2c]{};void* hit=nullptr;
    const V start{p.x,p.y,kFloorZ+2.f},end{p.x,p.y,kFloorZ-2.f};
    const bool found=g.CWorld_ProcessLineOfSight(&start,&end,point,&hit,
        true,false,false,true,false,false,false,false);
    z=Read<float>(point,8);
    const bool valid=found&&IsFloorEntity(hit)&&std::isfinite(z)&&std::abs(z-kFloorZ)<.03f;
    if(!valid)Trace(true,"floor ray failed x=%.3f y=%.3f hit=%d owned=%d z=%.3f expected=%.3f",
        p.x,p.y,found,IsFloorEntity(hit),z,kFloorZ);
    return valid;
}
bool CheckFloor() {
    float z{};if(!Ground(kArrival,z))return false;
    for(int i=0;i<kPageSize;++i) {
        const Vec p=Parking(i);
        for(float dx:{-5.f,0.f,5.f})for(float dy:{-9.f,0.f,9.f})
            if(!Ground(p+Vec{dx,dy,0},z))return false;
    }
    return true;
}
bool Occupied() {
    const auto pool=Vehicles();
    for(const auto& car:cars)if(void* p=car.Pointer(pool))
        for(int off=0x5a0;off<=0x5e0;off+=8)if(Read<void*>(p,off))return true;
    return false;
}
void SkipSlot(int index,const char* reason) {
    if(skipped[index])return;
    skipped[index]=true;
    const int model=catalogue.Model(targetPage,index);
    Trace(true,"skipped model id=%d slot=%d reason=%s streaming_state=%u; parking place left empty",
        model,index+1,reason,g.CStreaming_ms_aInfoForModel[model*0x14+0x10]);
}
bool SpawnOne() {
    if(spawnIndex>=catalogue.Size(targetPage))return true;
    const auto skip=[&](const char* reason) {
        SkipSlot(spawnIndex,reason);++spawnIndex;return spawnIndex==catalogue.Size(targetPage);
    };
    if(skipped[spawnIndex]){++spawnIndex;return spawnIndex==catalogue.Size(targetPage);}
    if(!Loaded(catalogue.Model(targetPage,spawnIndex)))return skip("model-not-resident");
    if(Free(Vehicles())<5)return skip("vehicle-pool-full");
    const int model=catalogue.Model(targetPage,spawnIndex);
    void* col=Read<void*>(g.CModelInfo_ms_modelInfoPtrs[model],0x30);
    const Vec min=Read<Vec>(col,0),max=Read<Vec>(col,12);
    if(!col||!vehicledoors::Finite(min)||!vehicledoors::Finite(max)||
       min.z>max.z||std::abs(min.z)>10||max.x-min.x>13||max.y-min.y>22) {
        Trace(true,"spawn bounds rejected id=%d col=%d min=(%.3f %.3f %.3f) max=(%.3f %.3f %.3f)",
            model,col!=nullptr,min.x,min.y,min.z,max.x,max.y,max.z);
        return skip("invalid-collision-bounds");
    }
    void* car=api.vehicle(model,2); // mission-created: native traffic cleanup leaves it alone
    if(!cars[spawnIndex].Capture(car,Vehicles())){
        Trace(true,"spawn factory/pool capture failed id=%d returned=%d free_vehicles=%d",model,car!=nullptr,Free(Vehicles()));
        return skip("factory-or-pool-capture-failed");
    }
    // Constructor owns model/RW references; don't call the nearby-vehicle cheat.
    const Vec spot=Parking(spawnIndex);
    api.orientation(car,0,0,spawnIndex<5?3.14159265358979323846f:0.f);
    api.carTeleport(car,spot.x,spot.y,kFloorZ-min.z+.25f,0);
    // STATUS_ABANDONED, ordinary physics, unlocked doors. No NPC driver.
    Write<std::uint8_t>(car,0x5a,(Read<std::uint8_t>(car,0x5a)&7)|(4<<3));
    Write<int>(car,0x684,1); // unlocked: verified CanPedOpenLocks reads +0x684
    ++spawnIndex;return spawnIndex==catalogue.Size(targetPage);
}
void PublishPage() {
    page=targetPage;publishedPage=page;publishedPages=catalogue.Pages();
    firstId=catalogue.Model(page,0);lastId=catalogue.Model(page,catalogue.Size(page)-1);
    int omitted=0;for(int i=0;i<catalogue.Size(page);++i)omitted+=skipped[i];
    publishedSkipped=omitted;
    Trace(false,"page-ready=%d/%d models=%d skipped=%d first=%d last=%d",page+1,catalogue.Pages(),
         catalogue.Size(page)-omitted,omitted,firstId.load(),lastId.load());
}
void MovePlayer(void* ped,V pos,float heading) {
    const Vec from=Position(ped);
    ResetEditors();api.pedTeleport(ped,pos.x,pos.y,pos.z,1);
    api.orientation(ped,0,0,heading);
    // Same verified retail current/target ped yaw used by Swimming.cpp.
    Write(ped,0x6c4,heading);Write(ped,0x6c8,heading);
    const Vec actual=Position(ped);
    Trace(false,"teleport from=(%.3f %.3f %.3f) target=(%.3f %.3f %.3f) actual=(%.3f %.3f %.3f)",
        from.x,from.y,from.z,pos.x,pos.y,pos.z,actual.x,actual.y,actual.z);
    // One discontinuity reset per teleport, never continuously recenter.
    vrcam::RequestRecenter();ClearMarkers();
}
bool ReturnPoint(V& destination) {
    // Re-stream before probing. If a prop disappeared, try a few nearby points
    // at the original elevation; never substitute an arbitrary city location.
    for(const Vec offset:{Vec{},Vec{2,0,0},Vec{-2,0,0},Vec{0,2,0},Vec{0,-2,0}}) {
        const V start{returnPosition.x+offset.x,returnPosition.y+offset.y,returnPosition.z+1};
        const V end{start.x,start.y,returnPosition.z-25};
        alignas(8) std::uint8_t point[0x2c]{};void* hit=nullptr;
        if(!g.CWorld_ProcessLineOfSight(&start,&end,point,&hit,true,false,false,true,false,false,false,false)) {
            Trace(true,"return ray missed at=(%.3f %.3f %.3f)",start.x,start.y,start.z);continue;
        }
        const float z=Read<float>(point,8);
        if(!std::isfinite(z)||Read<float>(point,0x18)<.5f) {
            Trace(true,"return ray rejected z=%.3f normal_z=%.3f",z,Read<float>(point,0x18));continue;
        }
        destination={start.x,start.y,z+1.f};
        if(offset.x==0&&offset.y==0&&returnPosition.z>z+.5f&&returnPosition.z<z+2.f)destination=returnPosition;
        return true;
    }
    return false;
}
bool Begin(void* ped) {
    if(!api.ready){Inform(Notice::Unavailable);return false;}
    if(Seated()||(g.PlayerIsEnteringCar&&g.PlayerIsEnteringCar())){Inform(Notice::LeaveCar);return false;}
    if(*api.area!=0||api.onMission()||!api.canStart(ped)||!vrcam::GroundWalkControlsActive()) {
        Inform(Notice::FreeRoam);return false;
    }
    catalogue={};
    for(int id=kFirstModel;id<=kLastModel;++id)
        if(void* mi=g.CModelInfo_ms_modelInfoPtrs[id])catalogue.Add(id,Read<int>(mi,0x6c));
    if(!catalogue.count){Inform(Notice::Unavailable);return false;}
    if(Free(Objects())<kTileCount+16||Free(Vehicles())<kPageSize+5){Inform(Notice::PoolFull);return false;}
    if(!player.Capture(ped,Pool(pedPool,0x9c8))){Inform(Notice::Unavailable);return false;}
    void* col=Read<void*>(g.CModelInfo_ms_modelInfoPtrs[kFloorModel],0x30);
    colSlot=Read<std::uint8_t>(col,0x28);
    if(!col||colSlot>=255){colSlot=-1;Inform(Notice::FloorFailed);return false;}
    // Slot zero is the permanently loaded generic collision collection.
    if(colSlot>0)api.colAddRef(colSlot);
    const Vec p=Position(ped);returnPosition={p.x,p.y,p.z};
    void* matrix=Read<void*>(ped,0x18);
    returnHeading=matrix?std::atan2(-Read<float>(matrix,0x10),Read<float>(matrix,0x14)):Read<float>(ped,0x14);
    previousEditor=driving::GetVehicleDebugMode();
    tileIndex=spawnIndex=targetPage=page=0;teleported=died=pageResourcesReady=false;
    recovery={};positionSamples=0;
    refCount=0;deadline=Now()+30;nextResourceLog=Now()+5;notice=Notice::None;failedModel=-1;
    skipped={};modelDeadline=Now()+5;
    Trace(false,"preparing from=(%.2f %.2f %.2f) models=%d col_slot=%d",
         returnPosition.x,returnPosition.y,returnPosition.z,catalogue.count,colSlot);
    ChangeState(State::Preparing);RequestResources();TraceResources("requested");return true;
}
bool SamePlayer(void* ped) {return ped&&player.Pointer(Pool(pedPool,0x9c8))==ped;}
void OnRestart() {
    if(state!=State::Off) {
        // Called BEFORE native teardown. The engine owns destruction on this path.
        ClearMarkers();ResetEditors();RestoreEditor();ReleasePendingModels();ReleaseModels();ReleaseRequestPins();ReleaseCollision();
        floor={};cars={};player={};teleported=false;
        toggleRequested=false;pageRequested=0;ChangeState(State::Off);
    }
    originalRestart();
}
bool OnSave(int slot) {
    if(publishedState.load()!=State::Off) {
        Inform(Notice::ExitToSave);return false; // never write an off-map player/temporary garage
    }
    return originalSave(slot);
}

// Only exact, verified retail prologues are admitted. The restart entry's fourth
// instruction is BL: expand it to an absolute call instead of copying its PC offset.
template<class Fn> bool Hook(void* target,const std::array<std::uint32_t,4>& expected,Fn hook,Fn& original) {
    if(!target||std::memcmp(target,expected.data(),16))return false;
    const long pageSize=sysconf(_SC_PAGESIZE);if(pageSize<=0)return false;
    auto* t=static_cast<std::uint32_t*>(mmap(nullptr,pageSize,PROT_READ|PROT_WRITE|PROT_EXEC,
                                          MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    if(t==MAP_FAILED)return false;
    const auto address=reinterpret_cast<std::uintptr_t>(target);
    if((expected[3]&0xfc000000u)==0x94000000u) {
        std::memcpy(t,expected.data(),12);
        const auto delta=static_cast<std::int32_t>(expected[3]<<6)>>4;
        const auto callee=address+12+delta;
        t[3]=0x580000b1;t[4]=0xd63f0220;t[5]=0x580000b1;t[6]=0xd61f0220;t[7]=0xd503201f;
        std::memcpy(t+8,&callee,8);const auto resume=address+16;std::memcpy(t+10,&resume,8);
    } else {
        std::memcpy(t,expected.data(),16);t[4]=0x58000051;t[5]=0xd61f0220;
        const auto resume=address+16;std::memcpy(t+6,&resume,8);
    }
    __builtin___clear_cache(reinterpret_cast<char*>(t),reinterpret_cast<char*>(t)+48);
    const auto begin=address&~(static_cast<std::uintptr_t>(pageSize)-1);
    const auto end=(address+16+pageSize-1)&~(static_cast<std::uintptr_t>(pageSize)-1);
    if(mprotect(reinterpret_cast<void*>(begin),end-begin,PROT_READ|PROT_WRITE|PROT_EXEC)) {
        munmap(t,pageSize);return false;
    }
    original=reinterpret_cast<Fn>(t);
    auto* code=static_cast<std::uint32_t*>(target);const auto replacement=reinterpret_cast<std::uintptr_t>(hook);
    std::memcpy(code+2,&replacement,8);code[1]=0xd61f0220;__atomic_store_n(code,0x58000051,__ATOMIC_RELEASE);
    __builtin___clear_cache(reinterpret_cast<char*>(code),reinterpret_cast<char*>(code)+16);return true;
}
} // namespace

void Install(void* handle) {
    if(!handle||api.ready)return;
#define RESOLVE(field,name) api.field=reinterpret_cast<decltype(api.field)>(dlsym(handle,name))
    RESOLVE(object,"_ZN7CObject6CreateEib");
    RESOLVE(vehicle,"_ZN8CCarCtrl32GetNewVehicleDependingOnCarModelEih");
    RESOLVE(remove,"_ZN6CWorld6RemoveEP7CEntity");
    RESOLVE(deleteObject,"_ZN7CObjectD0Ev");
    RESOLVE(objectTeleport,"_ZN7CObject8TeleportE7CVectorh");
    RESOLVE(carTeleport,"_ZN11CAutomobile8TeleportE7CVectorh");
    RESOLVE(pedTeleport,"_ZN4CPed8TeleportE7CVectorh");
    RESOLVE(orientation,"_ZN10CPlaceable14SetOrientationEfff");
    RESOLVE(createRw,"_ZN7CObject14CreateRwObjectEv");
    RESOLVE(addModelRef,"_ZN14CBaseModelInfo6AddRefEv");
    RESOLVE(releaseModel,"_ZN14CBaseModelInfo9RemoveRefEv");
    RESOLVE(colAddRef,"_ZN9CColStore6AddRefEi");RESOLVE(colRemoveRef,"_ZN9CColStore9RemoveRefEi");
    RESOLVE(onMission,"_ZN11CTheScripts18IsPlayerOnAMissionEv");
    RESOLVE(canStart,"_ZN10CPlayerPed21CanPlayerStartMissionEv");
    RESOLVE(loadCollision,"_ZN10CStreaming18LoadSceneCollisionERK7CVector");
    RESOLVE(releaseMission,"_ZN10CStreaming28SetMissionDoesntRequireModelEi");
    RESOLVE(objectPool,"_ZN6CPools14ms_pObjectPoolE");RESOLVE(area,"_ZN5CGame8currAreaE");
#undef RESOLVE
    pedPool=reinterpret_cast<void**>(dlsym(handle,"_ZN6CPools11ms_pPedPoolE"));
    const bool symbols=api.object&&api.vehicle&&api.remove&&api.deleteObject&&api.objectTeleport&&
        api.carTeleport&&api.pedTeleport&&api.orientation&&api.createRw&&api.addModelRef&&api.releaseModel&&
        api.colAddRef&&api.colRemoveRef&&api.onMission&&api.canStart&&api.loadCollision&&
        api.releaseMission&&api.objectPool&&api.area&&pedPool&&
        g.CPools_ms_pVehiclePool&&g.CModelInfo_ms_modelInfoPtrs&&g.CStreaming_RequestModel&&
        g.CStreaming_ms_aInfoForModel&&g.CObject_SetIsStatic&&g.FindPlayerPed&&g.FindPlayerVehicle&&
        g.CStreaming_LoadScene&&g.CPed_IsAlive&&g.CWorld_ProcessLineOfSight;
    if(!symbols){Trace(true,"install: required engine symbols missing");return;}
    const bool restart=Hook(dlsym(handle,"_ZN5CGame18ShutDownForRestartEv"),
        {0xa9be7bfd,0xf9000bf3,0x910003fd,0x940da1b1},&OnRestart,originalRestart);
    const bool save=Hook(dlsym(handle,"_ZN19CGenericGameStorage11GenericSaveEi"),
        {0xa9ba7bfd,0xa9016ffc,0xa90267fa,0xa9035ff8},&OnSave,originalSave);
    api.ready=restart&&save;available=api.ready;
    Trace(!api.ready,"install ready=%d restart_guard=%d save_guard=%d",api.ready,restart,save);
}
void RequestToggle() {toggleRequested.store(!toggleRequested.load());}
void RequestPage(int direction) {
    if(direction&&publishedState.load()==State::Active)pageRequested.store(direction<0?-1:1);
}
const char* MenuLabel() {
    if(!available)return "UNAVAILABLE";
    if(toggleRequested)return "CLOSE MENU";
    switch(notice.load()) {
        case Notice::LeaveCar:return "LEAVE VEHICLE FIRST";
        case Notice::FreeRoam:return "OUTDOOR FREE ROAM ONLY";
        case Notice::PoolFull:return "ENTITY POOL FULL";
        case Notice::LoadFailed: {
            static thread_local char label[48];
            std::snprintf(label,sizeof(label),"MODEL LOAD FAILED - ID %d",failedModel.load());return label;
        }
        case Notice::FloorFailed:return "FLOOR CHECK FAILED";
        case Notice::SpawnFailed:return "VEHICLE SPAWN FAILED";
        case Notice::ExitToSave:return "EXIT AREA BEFORE SAVING";
        case Notice::Outside:return "RETURN TO AREA FIRST";
        case Notice::EditorBusy:return "RELEASE EDITOR FIRST";
        case Notice::ReturnFailed:return "RETURN POINT UNAVAILABLE - RETRY";
        default:break;
    }
    switch(publishedState.load()) {
        case State::Off:return "OFF";case State::Preparing:return "PREPARING";
        case State::Changing:return "LOADING PAGE";case State::Returning:return "RETURNING";
        default:return "ON";
    }
}
const char* PageLabel() {
    static thread_local char label[80];
    if(publishedState.load()==State::Off)return "ENTER TEST AREA";
    if(publishedState.load()==State::Preparing)return "PREPARING FIRST PAGE";
    if(pageRequested.load()!=0)return "CLOSE MENU TO LOAD PAGE";
    if(publishedState.load()==State::Changing) {
        std::snprintf(label,sizeof(label),"LOADING %d/%d",publishedTarget.load()+1,publishedPages.load());
        return label;
    }
    std::snprintf(label,sizeof(label),"%d/%d - IDs %d..%d - SKIP %d",publishedPage.load()+1,
                  publishedPages.load(),firstId.load(),lastId.load(),publishedSkipped.load());return label;
}
void Tick(bool gameplay,bool menuOpen) {
    if(!api.ready)return;
    void* ped=g.FindPlayerPed(-1);
    if(state!=State::Off&&!SamePlayer(ped)) {
        // The restart hook is the primary boundary; this is a second guard for
        // replaced player/pools. Never return a new actor to the previous save.
        Finish();return;
    }
    if(state!=State::Off&&!g.CPed_IsAlive(ped)){died=true;ClearMarkers();return;}
    if(died&&ped&&g.CPed_IsAlive(ped)) {
        died=false;
        if(!InBounds(Position(ped))) {Finish();return;}
        ChangeState(State::Returning);
    }
    if(!gameplay||menuOpen||!xr::IsSessionFocused()) {ClearMarkers();return;}
    if(state!=State::Off&&state!=State::Preparing&&(*api.area!=0||api.onMission())) {
        // A mission selector or scripted transition can take ownership while
        // the area is open. Do not pull that actor back into our sandbox.
        ClearMarkers();if(!InBounds(Position(ped))&&!Seated())Finish();return;
    }
    if(toggleRequested.exchange(false)) {
        if(state==State::Off){if(!ped||!g.CPed_IsAlive(ped)||!Begin(ped))return;}
        else if(Seated()||(g.PlayerIsEnteringCar&&g.PlayerIsEnteringCar()))Inform(Notice::LeaveCar);
        else {notice=Notice::None;ChangeState(State::Returning);}
    }
    if(state==State::Off)return;
    if(state==State::Returning) {
        if(Seated()) {Inform(Notice::LeaveCar);return;}
        g.CStreaming_LoadScene(&returnPosition);
        api.loadCollision(&returnPosition);
        V destination{};
        if(!ReturnPoint(destination)){ChangeState(State::Active);Inform(Notice::ReturnFailed);return;}
        Trace(false,"return=(%.2f %.2f %.2f)",destination.x,destination.y,destination.z);
        MovePlayer(ped,destination,returnHeading);Finish();return;
    }
    if(state==State::Active) {
        const Vec position=Position(ped);
        if(positionSamples<3&&Now()>=nextPositionLog) {
            Trace(false,"player position=(%.3f %.3f %.3f) velocity_z=%.3f inside=%d",
                position.x,position.y,position.z,Read<float>(ped,0x70),InBounds(position));
            ++positionSamples;nextPositionLog=Now()+1;
        }
        const auto action=recovery.Step(position,Now(),Seated());
        if(action!=Recovery::None) {
            float ground{};const bool floorBelow=Ground(kArrival,ground);
            Trace(true,"outside area position=(%.3f %.3f %.3f) velocity_z=%.3f floor_valid=%d action=%s",
                position.x,position.y,position.z,Read<float>(ped,0x70),floorBelow,action==Recovery::Rescue?"rescue":"leave");
            if(action==Recovery::Leave||!floorBelow) {
                ChangeState(State::Returning);return;
            }
            MovePlayer(ped,{kArrival.x,kArrival.y,kArrival.z},0);notice=Notice::None;
        }
        const int step=pageRequested.exchange(0);
        if(!step)return;
        if(Seated()||Occupied()||(g.PlayerIsEnteringCar&&g.PlayerIsEnteringCar())){Inform(Notice::LeaveCar);return;}
        if(vehicledoors::DebugEditActive()||vehicledoors::ClaimedHands()){Inform(Notice::EditorBusy);return;}
        if(!InBounds(Position(ped))){Inform(Notice::Outside);return;}
        if(!catalogue.CanStep(page,step))return;
        ClearMarkers();ResetEditors();targetPage=catalogue.Next(page,step);spawnIndex=0;
        pageResourcesReady=false;deadline=Now()+30;nextResourceLog=Now()+5;notice=Notice::None;ChangeState(State::Changing);
        failedModel=-1;skipped={};modelDeadline=Now()+5;
        TraceResources("page-requested");
    }
    if(!pageResourcesReady) {
        RequestResources();
        if(Now()>=modelDeadline||Now()>deadline) {
            for(int i=0;i<catalogue.Size(targetPage);++i)
                if(!Loaded(catalogue.Model(targetPage,i)))SkipSlot(i,"load-timeout-5s");
        }
        if(Now()>deadline&&!ResourcesReady()) {
            failedModel=MissingModel();
            Trace(true,"resource-timeout after 30s");TraceResources("timeout");
            if(state==State::Changing) {
                ReleasePendingModels();ReleaseRequestPins();targetPage=page;ChangeState(State::Active);Inform(Notice::LoadFailed);
            }
            else Failed(Notice::LoadFailed);
            return;
        }
        if(!ResourcesReady()) {
            if(Now()>=nextResourceLog){TraceResources("waiting");nextResourceLog=Now()+10;}
            return;
        }
        TraceResources("ready");
        // Loading takes multiple ticks: the player may have entered an old car
        // since requesting a page. Recheck immediately before any deletion.
        if(state==State::Changing) {
            if(Seated()||Occupied()||vehicledoors::DebugEditActive()||vehicledoors::ClaimedHands()||
               (g.PlayerIsEnteringCar&&g.PlayerIsEnteringCar())) {
                ReleasePendingModels();ReleaseRequestPins();targetPage=page;ChangeState(State::Active);Inform(Notice::LeaveCar);return;
            }
            DeleteCars();
        }
        ReleaseModels();Retain(kFloorModel);
        for(int i=0;i<catalogue.Size(targetPage);++i)if(!skipped[i])Retain(catalogue.Model(targetPage,i));
        ReleasePendingModels();
        ReleaseRequestPins();
        pageResourcesReady=true;
    }
    if(state==State::Preparing&&tileIndex<kTileCount&&!BuildTiles())return;
    if(state==State::Preparing&&!teleported) {
        if(Seated()||(g.PlayerIsEnteringCar&&g.PlayerIsEnteringCar())){Failed(Notice::LeaveCar);return;}
        if(*api.area!=0||api.onMission()||!api.canStart(ped)||!vrcam::GroundWalkControlsActive()) {
            Failed(Notice::FreeRoam);return;
        }
        if(!CheckFloor()){Failed(Notice::FloorFailed);return;}
        Trace(false,"floor validated tiles=%d",tileIndex);
        if(!driving::GetVehicleDebugMode())driving::CycleVehicleDebugMode(1);
        MovePlayer(ped,{kArrival.x,kArrival.y,kArrival.z},0);teleported=true;
        nextPositionLog=Now()+1;
    }
    if(SpawnOne()) {PublishPage();ChangeState(State::Active);notice=Notice::None;}
}

bool UpdateNavigation(bool blocked,const xr::HandPose hands[2],const float head[3],
                      const float q[4],bool valid,std::uint64_t epoch) {
    const bool usable=!blocked&&state==State::Active&&valid&&epoch==navEpoch&&
        xr::IsSessionFocused()&&!xr::IsMenuVisible()&&!Seated()&&
        !vehicledoors::DebugEditActive()&&!vehicledoors::ClaimedHands()&&vrcam::GroundWalkControlsActive()&&
        InBounds(Position(g.FindPlayerPed(-1)));
    navEpoch=epoch;
    const bool previousClaim=navClaim;navClaim=false;
    bool fresh[2]{};
    for(int h=0;h<2;++h)fresh[h]=edges[h].Step(hands[h].trigger,usable&&hands[h].valid);
    if(!usable){ClearMarkers();return previousClaim;}
    unsigned occupied=physicalweapon::HeldHandMaskRelaxed()|basketball::HeldHandMask();
    for(int h=0;h<2;++h)if(physicalweapon::IsSupportHand(h))occupied|=1u<<h;
    bool near=false;
    for(int i=0;i<worldMarkers.count;++i) {
        const auto& m=worldMarkers.markers[i];
        float in[3]{m.position.x,m.position.y,m.position.z},out[3]{};
        if(vrcam::WorldPointToTracking(in,out))for(int h=0;h<2;++h)if(hands[h].valid)
            near|=vehicledoors::Length(Vec{out[0],out[1],out[2]}-
                Vec{hands[h].gripPos[0],hands[h].gripPos[1],hands[h].gripPos[2]})<.26f;
    }
    const Vec forward{-2*(q[0]*q[2]+q[3]*q[1]),-2*(q[1]*q[2]-q[3]*q[0]),
                      -(1-2*(q[0]*q[0]+q[1]*q[1]))};
    const bool reliable=vehicledoors::Finite(forward)&&forward.x*forward.x+forward.z*forward.z>.01f;
    const float yaw=reliable?std::atan2(-forward.x,-forward.z):body.yaw;
    body.Update({head[0],head[1],head[2]},yaw,true,reliable,true,epoch,0,near,near,Now());
    if(!body.valid){ClearMarkers();return previousClaim;}
    worldMarkers={};worldMarkers.epoch=epoch;
    int selected=0;
    for(int direction:{-1,1}) {
        if(!catalogue.CanStep(page,direction))continue;
        const auto anchor=holsterbody::Socket(body.position,body.yaw,direction*.50f,.12f,.25f);
        const Vec right{std::cos(body.yaw),0,-std::sin(body.yaw)};
        const Vec centre{anchor.x,anchor.y,anchor.z},d=right*static_cast<float>(direction);
        bool hover=false;
        for(int h=0;h<2;++h)if(hands[h].valid&&!(occupied&(1u<<h))) {
            const bool hit=vehicledoors::Length(centre-Vec{hands[h].gripPos[0],hands[h].gripPos[1],hands[h].gripPos[2]})<.14f;
            hover|=hit;
            if(hit&&(fresh[h]||hands[h].trigger>.35f))navClaim=true;
            if(hit&&fresh[h]&&!selected)selected=direction;
        }
        const Vec root=centre-d*.09f;
        const Vec values[3]{root,root+d,root+Vec{0,1,0}};
        Vec world[3]{};bool ok=true;
        for(int i=0;i<3;++i) {
            float in[3]{values[i].x,values[i].y,values[i].z},out[3]{};
            if(!vrcam::TrackingPointToWorld(in,out)){ok=false;break;}
            world[i]={out[0],out[1],out[2]};
        }
        if(ok)worldMarkers.markers[worldMarkers.count++]={world[0],world[1]-world[0],world[2]-world[0],hover};
    }
    if(selected){Trace(false,"navigation trigger direction=%d",selected);RequestPage(selected);}
    PublishMarkers();return navClaim||previousClaim;
}
void PublishMarkers() {
    vehicledoors::MarkerFrame frame;frame.epoch=worldMarkers.epoch;
    for(int i=0;i<worldMarkers.count;++i) {
        const auto& m=worldMarkers.markers[i];Vec local[3]{};
        const Vec world[3]{m.position,m.position+m.direction,m.position+m.up};bool ok=true;
        for(int j=0;j<3;++j) {
            float in[3]{world[j].x,world[j].y,world[j].z},out[3]{};
            if(!vrcam::WorldPointToTracking(in,out)){ok=false;break;}
            local[j]={out[0],out[1],out[2]};
        }
        if(ok)frame.markers[frame.count++]={local[0],local[1]-local[0],local[2]-local[0],m.held};
    }
    xr::SetVehicleTestMarkers(frame);
}
} // namespace savr::testarea
