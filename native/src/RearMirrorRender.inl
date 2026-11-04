// Included inside VrCamera.cpp's private namespace. All work runs on the game
// producer; the mirror is baked into both eye rasters before publication. Its
// texture never crosses to the XR context, so ordinary RenderQueue ordering
// protects read-after-write and next-frame reuse without a new fence/wait.
namespace rear_mirror_render {
namespace policy = savr::rear_mirror;
template<class T> T Read(const void* p,std::size_t offset) {
    T value{}; if(p)std::memcpy(&value,static_cast<const char*>(p)+offset,sizeof(value));
    return value;
}
struct Api {
    void* (*frameLtm)(void*){};
    void (*createObject)(void*){};
    void (*setupVehicle)(void*){};
    void (*resetVehicle)(void*){};
    bool (*setupLighting)(void*){};
    void (*removeLighting)(void*,bool){};
    void (*ambient)(){};
    void (*directionalOff)(){};
    void (*markRendered)(void*){};
    bool attempted{},ready{};
} api;
bool Resolve() {
    if(api.attempted)return api.ready;
    api.attempted=true;
    void* h=dlopen("libGame.so",RTLD_NOW|RTLD_NOLOAD);
    if(!h||!g.LoadBase){if(h)dlclose(h);return false;}
    const auto get=[h](const char* name,std::uintptr_t offset)->void* {
        void* p=dlsym(h,name);
        return reinterpret_cast<std::uintptr_t>(p)==g.LoadBase+offset?p:nullptr;
    };
    api.frameLtm=reinterpret_cast<decltype(api.frameLtm)>(get("_Z13RwFrameGetLTMP7RwFrame",0x768858));
    api.createObject=reinterpret_cast<decltype(api.createObject)>(get("_ZN7CEntity14CreateRwObjectEv",0x48b6f0));
    api.setupVehicle=reinterpret_cast<decltype(api.setupVehicle)>(get("_ZN8CVehicle11SetupRenderEv",0x6d98a4));
    api.resetVehicle=reinterpret_cast<decltype(api.resetVehicle)>(get("_ZN8CVehicle16ResetAfterRenderEv",0x6d9974));
    api.setupLighting=reinterpret_cast<decltype(api.setupLighting)>(get("_ZN8CVehicle13SetupLightingEv",0x4b59c0));
    api.removeLighting=reinterpret_cast<decltype(api.removeLighting)>(get("_ZN8CVehicle14RemoveLightingEb",0x4b5acc));
    api.ambient=reinterpret_cast<decltype(api.ambient)>(get("_Z17SetAmbientColoursv",0x600e94));
    api.directionalOff=reinterpret_cast<decltype(api.directionalOff)>(get("_Z21DeActivateDirectionalv",0x600dc8));
    api.markRendered=reinterpret_cast<decltype(api.markRendered)>(get("_ZN10CStreaming12RenderEntityEP5CLinkIP7CEntityE",0x3b5040));
    api.ready=api.frameLtm&&api.createObject&&api.setupVehicle&&api.resetVehicle&&
        api.setupLighting&&api.removeLighting&&api.ambient&&api.directionalOff&&api.markRendered&&g.AtomicDefaultRenderCallBack&&
        g.RpClumpForAllAtomics&&g.RwRenderStateGet&&g.RwRenderStateSet&&
        g.RwIm3DTransform&&g.RwIm3DRenderIndexedPrimitive&&g.RwIm3DEnd&&
        g.RwCameraSetNearClipPlane&&g.RwCameraSetFarClipPlane&&g.RwCameraClear&&
        g.CModelInfo_ms_modelInfoPtrs&&g.GetFrameNodeName&&
        reinterpret_cast<std::uintptr_t>(g.CRenderer_ScanSectorList)==g.LoadBase+kRetailScanSectorListOffset;
    dlclose(h);
    LOGI("[mirror] renderer ready=%d 512x160 quality=LOW/MEDIUM shadows=off",api.ready);
    return api.ready;
}
void* raster{};
bool visible{};
CamBasis rear{},surface{};
void* owner{};
float tanX{};
int quality=1;
float farClip=policy::Far;
int atoms{},lods{},requests{},creates{};
struct Candidate { void* entity{}; float distance{}; };
std::array<Candidate,policy::MediumMap> map{};
std::array<Candidate,policy::MediumTraffic> traffic{};
int mapCount{},trafficCount{};

// Position axes stay vehicle-local even when orientation is adjusted.
CamBasis Pose(const CamBasis& car,const int* values) {
    CamBasis p{car.right,car.forward*-1.f,car.up,
        car.pos+car.right*(values[0]*.01f)+car.forward*(values[1]*.01f)+car.up*(values[2]*.01f)};
    const auto rotatePair=[](V3& a,V3& b,float deg) {
        const float r=deg*.01745329252f,c=std::cos(r),s=std::sin(r);
        const V3 previous=a;a=previous*c+b*s;b=b*c-previous*s;
    };
    rotatePair(p.forward,p.up,static_cast<float>(values[3]));
    rotatePair(p.forward,p.right,static_cast<float>(values[4]));
    rotatePair(p.right,p.up,static_cast<float>(values[5]));
    return p;
}
bool Prepare(const CamBasis& head) {
    visible=false;owner=nullptr;
    if(!driving::RearMirrorEnabled()||driving::ThirdPersonViewActive()||
        driving::GetActiveVehicleType()!=driving::VEHICLE_CAR||!g.FindPlayerVehicle||
        FormalCutsceneRunning()||ScriptedCutsceneRunning())return false;
    owner=g.FindPlayerVehicle(-1,false);
    if(!owner||!g.CVehicle_GetVehicleAppearance||g.CVehicle_GetVehicleAppearance(owner)!=1)return false;
    void* m=Read<void*>(owner,0x18);if(!m)return false;
    CamBasis car{Read<V3>(m,0),Read<V3>(m,0x10),Read<V3>(m,0x20),Read<V3>(m,0x30)};
    if(!Finite(car.pos)||!Finite(car.right)||!Finite(car.forward)||!Finite(car.up)||
        Dot(car.right,car.right)<.5f||Dot(car.forward,car.forward)<.5f||Dot(car.up,car.up)<.5f)return false;
    int values[12];driving::GetRearMirrorCalibration(Read<std::int16_t>(owner,0x32),values);
    rear=Pose(car,values+6);surface=Pose(car,values);tanX=policy::TanX(driving::RearMirrorFov());
    const V3 delta=surface.pos-head.pos;
    // Conservative HMD cone with a margin: no eye-tracking dependency. Keep
    // updating during debug so an off-screen rear camera can be calibrated.
    const float depth=Dot(delta,head.forward);
    if(!driving::GetVehicleDebugMode() &&
        (depth<-.2f || std::fabs(Dot(delta,head.right))>depth*1.8f+.2f ||
         std::fabs(Dot(delta,head.up))>depth*1.8f+.2f ||
         Dot(head.pos-surface.pos,surface.forward)<=0.f))return false;
    if(!Resolve())return false;
    quality=driving::RearMirrorQuality();farClip=policy::FarFor(quality);
    if(!raster) {raster=g.RwRasterCreate(policy::Width,policy::Height,32,5);return false;}
    if(!EyeRasterTexId(raster))return false;
    visible=true;return true;
}

void* Model(void* entity) {
    const int id=Read<std::int16_t>(entity,0x32);
    return id>=0&&id<20000?g.CModelInfo_ms_modelInfoPtrs[id]:nullptr;
}
bool StaticAllowed(void* entity,void* mi) {
    if(!entity||!mi)return false;
    const int type=Read<std::uint8_t>(entity,0x5a)&7;
    const auto flags=Read<std::uint32_t>(entity,0x28);
    const auto vptr=Read<std::uintptr_t>(mi,0);
    return (type==1||type==5||type==4) && (flags&(1u<<7)) &&
        !(flags&(1u<<14)) &&
        vptr!=g.LoadBase+kRetailTimeModelInfoVptrOffset &&
        vptr!=g.LoadBase+kRetailLodTimeModelInfoVptrOffset;
}
bool Bounds(void* entity,void* mi,float& distance) {
    V3 pos{};if(!ReadEntityWorldPosition(entity,&pos))return false;
    const void* collision=Read<void*>(mi,0x30);
    const V3 center=Read<V3>(collision,0x18);
    void* matrix=Read<void*>(entity,0x18);
    if(matrix)pos=pos+Read<V3>(matrix,0)*center.x+Read<V3>(matrix,0x10)*center.y+Read<V3>(matrix,0x20)*center.z;
    else {
        const float heading=Read<float>(entity,0x14),c=std::cos(heading),s=std::sin(heading);
        pos=pos+V3{center.x*c-center.y*s,center.x*s+center.y*c,center.z};
    }
    const V3 d=pos-rear.pos;
    const float radius=Read<float>(collision,0x24);
    const float centerDistance=std::sqrt(Dot(d,d));
    // Rank by the closest possible surface, not the origin of a large mesh.
    // A road underneath the camera must not lose the candidate budget to
    // smaller objects whose centres happen to be closer.
    distance=std::max(0.f,centerDistance-radius);
    return policy::InView(Dot(d,rear.right),Dot(d,rear.up),Dot(d,rear.forward),radius,tanX,farClip)&&
        (quality>=2||!policy::SmallProp(radius,centerDistance));
}
template<std::size_t N> void Insert(std::array<Candidate,N>& entries,int& count,void* entity,float distance,int limit=static_cast<int>(N)) {
    for(int i=0;i<count;++i)if(entries[i].entity==entity)return;
    if(count<limit){entries[count++]={entity,distance};return;}
    auto farthest=std::max_element(entries.begin(),entries.begin()+count,[](const auto& a,const auto& b){return a.distance<b.distance;});
    if(distance<farthest->distance)*farthest={entity,distance};
}
// Only the render object is instantiated; no simulation/PreRender/ScanWorld
// calls, scan-code writes or mutations of the two eyes' visibility lists.
bool Resident(void* entity,void* mi,bool allowRequest) {
    if(Read<void*>(entity,0x20))return true;
    if(Read<void*>(mi,0x40)) {
        if(creates>=(quality>=2?4:policy::MaxCreates))return false;
        ++creates;
        api.createObject(entity);
        return Read<void*>(entity,0x20)!=nullptr;
    }
    if(allowRequest && requests<(quality>=2?4:policy::MaxRequests) && g.CStreaming_RequestModel) {
        g.CStreaming_RequestModel(Read<std::int16_t>(entity,0x32),0);
        ++requests;
    }
    return false;
}
void Consider(void* entity) {
    if(!entity||entity==owner)return;
    void* mi=Model(entity);if(!mi)return;
    const int type=Read<std::uint8_t>(entity,0x5a)&7;
    float distance{};
    if(type==2) {
        if((Read<std::uint32_t>(entity,0x28)&(1u<<7))&&Read<void*>(entity,0x20)&&Bounds(entity,mi,distance))
            Insert(traffic,trafficCount,entity,distance,quality>=2?policy::MediumTraffic:policy::MaxTraffic);
        return;
    }
    if(!StaticAllowed(entity,mi)||!Bounds(entity,mi,distance))return;
    void* parent=Read<void*>(entity,0x50);
    void* parentModel=parent?Model(parent):nullptr;
    float parentDistance{};
    // Check the replacement before selecting it. A loaded but off-frustum
    // parent previously discarded its visible detail child entirely. Also
    // keep detail for parents whose bounds approach/contain the camera: early
    // LOD meshes can lack the near road surface at wide FOV.
    if(quality==1&&StaticAllowed(parent,parentModel)&&
       Bounds(parent,parentModel,parentDistance)&&parentDistance>15.f&&Resident(parent,parentModel,true)) {
        Insert(map,mapCount,parent,parentDistance,policy::MaxMap);
    } else if((quality<2||!(Read<std::uint32_t>(entity,0x28)&0x100u))&&Resident(entity,mi,true))
        Insert(map,mapCount,entity,distance,quality>=2?policy::MediumMap:policy::MaxMap);
}
void Collect() {
    mapCount=trafficCount=requests=creates=lods=atoms=0;
    const auto index=[](float p){return std::clamp(static_cast<int>(std::floor((p+3000.f)/50.f)),0,119);};
    const int minX=index(rear.pos.x-farClip),maxX=index(rear.pos.x+farClip);
    const int minY=index(rear.pos.y-farClip),maxY=index(rear.pos.y+farClip);
    int visited=0;
    for(int y=minY;y<=maxY;++y)for(int x=minX;x<=maxX;++x) {
        const auto* normal=reinterpret_cast<const char*>(g.LoadBase+kRetailWorldSectorsOffset)+(y*120+x)*0x10;
        const auto* repeat=reinterpret_cast<const char*>(g.LoadBase+kRetailWorldRepeatSectorsOffset)+((x&15)|((y&15)<<4))*0x18;
        // Buildings, dummies, vehicles and physical objects. Pedestrians and
        // particles are deliberately omitted from this inexpensive prototype.
        const void* heads[]={Read<void*>(normal,0),Read<void*>(normal,8),Read<void*>(repeat,0),Read<void*>(repeat,0x10)};
        for(const void* head:heads)for(const void* node=head;node;) {
            if(++visited>8192)return;
            Consider(Read<void*>(node,0));
            const void* next=Read<void*>(node,8);if(next==node)break;node=next;
        }
    }
}
struct AtomicContext { bool vehicle{},low{},hasLow{}; };
bool LowAtomic(void* atomic) {
    void* frame=Read<void*>(atomic,8);
    const char* name=frame?g.GetFrameNodeName(frame):nullptr;
    return name&&(std::strstr(name,"_vlo")||std::strstr(name,"_lo"));
}
void* FindLow(void* atomic,void* data) {
    if((Read<std::uint8_t>(atomic,2)&4)&&LowAtomic(atomic))static_cast<AtomicContext*>(data)->hasLow=true;
    return atomic;
}
void* DrawAtomic(void* atomic,void* data) {
    const auto& c=*static_cast<AtomicContext*>(data);
    if(!atomic||!(Read<std::uint8_t>(atomic,2)&4))return atomic;
    if(c.vehicle&&LowAtomic(atomic)!=(c.low&&c.hasLow))return atomic;
    void* frame=Read<void*>(atomic,8);if(!frame||!api.frameLtm(frame))return atomic;
    // Bypass visibility callbacks that enqueue into the main alpha lists or
    // advance wheels/animation. The native material pipeline remains intact.
    g.AtomicDefaultRenderCallBack(atomic);++atoms;
    return atomic;
}
void DrawEntity(const Candidate& c,bool vehicle) {
    void* object=Read<void*>(c.entity,0x20);if(!object)return;
    AtomicContext context{vehicle,quality==1&&c.distance>20.f,false};
    bool lit=false;
    if(vehicle){api.setupVehicle(c.entity);lit=api.setupLighting(c.entity);}
    if(Read<std::uint8_t>(object,0)==1)DrawAtomic(object,&context);
    else if(Read<std::uint8_t>(object,0)==2) {
        if(vehicle)g.RpClumpForAllAtomics(object,&FindLow,&context);
        g.RpClumpForAllAtomics(object,&DrawAtomic,&context);
    }
    if(vehicle){api.removeLighting(c.entity,lit);api.resetVehicle(c.entity);}
    // Same streaming-LRU notification as CEntity::Render, without running
    // its attached effects. Prevent a visible rear-only model being evicted.
    if(void* link=Read<void*>(c.entity,0x40))api.markRendered(link);
    if((Read<std::uint32_t>(c.entity,0x28)&0x100)||(context.hasLow&&context.low))++lods;
}

void Render(void* sc,std::uintptr_t cam) {
    if(!visible)return;
    const double start=perf::ThreadCpuMs();
    Collect();
    char* camera=static_cast<char*>(sc);
    const CamBasis saved=ReadCam(cam);
    // Snapshot raster, projection (including view offset) and clip/fog planes.
    std::array<std::uint8_t,0x34> fields{};
    std::memcpy(fields.data(),camera+0x80,fields.size());
    const auto fog=CaptureMobileDistanceFog();
    const bool savedSkipBlend=g.skipBlendRender&&*g.skipBlendRender;
    const bool savedSkipOpaque=g.skipNonblendRender&&*g.skipNonblendRender;
    if(g.skipBlendRender)*g.skipBlendRender=false;
    if(g.skipNonblendRender)*g.skipNonblendRender=false;
    ScopedScreenDimensions dimensions(g.RsGlobal);dimensions.Set(policy::Width,policy::Height);
    WriteCam(cam,rear);
    if(g.CCamera_CalculateDerivedValues)g.CCamera_CalculateDerivedValues(g.TheCamera,false,true);
    g_origCopy(g.TheCamera,false);
    *reinterpret_cast<void**>(camera+0x80)=raster;
    *reinterpret_cast<void**>(camera+0x88)=nullptr;
    *reinterpret_cast<float*>(camera+0x90)=tanX;
    *reinterpret_cast<float*>(camera+0x94)=tanX*policy::Height/policy::Width;
    *reinterpret_cast<float*>(camera+0x98)=1.f/tanX;
    *reinterpret_cast<float*>(camera+0x9c)=policy::Width/(tanX*policy::Height);
    *reinterpret_cast<float*>(camera+0xa0)=0.f;
    *reinterpret_cast<float*>(camera+0xa4)=0.f;
    g.RwCameraSetNearClipPlane(sc,policy::Near);g.RwCameraSetFarClipPlane(sc,farClip);
    const float fogStart=farClip*.64f;
    *reinterpret_cast<float*>(camera+0xb0)=fogStart;
    unsigned char background[4]={110,125,140,255};
    if(fog.valid)for(int i=0;i<3;++i)background[i]=static_cast<unsigned char>(std::clamp(fog.color[i]<=1.5f?fog.color[i]*255.f:fog.color[i],0.f,255.f));
    g.RwCameraClear(sc,background,1|2|4);g.RwCameraBeginUpdate(sc);
    if(g.DefinedState)g.DefinedState();
    ApplyMobileDistanceFog(fog,fogStart,farClip);
    g.RwRenderStateSet(14,reinterpret_cast<void*>(1));
    api.directionalOff();api.ambient();
    for(int i=0;i<mapCount;++i)DrawEntity(map[i],false);
    for(int i=0;i<trafficCount;++i)DrawEntity(traffic[i],true);
    g.RwCameraEndUpdate(sc);
    WriteCam(cam,saved);
    if(g.CCamera_CalculateDerivedValues)g.CCamera_CalculateDerivedValues(g.TheCamera,false,true);
    g_origCopy(g.TheCamera,false);
    std::memcpy(camera+0x80,fields.data(),fields.size());
    g.RwCameraSetNearClipPlane(sc,Read<float>(fields.data(),0x28));
    g.RwCameraSetFarClipPlane(sc,Read<float>(fields.data(),0x2c));
    ApplyMobileDistanceFog(fog,fog.start,fog.end);
    if(g.skipBlendRender)*g.skipBlendRender=savedSkipBlend;
    if(g.skipNonblendRender)*g.skipNonblendRender=savedSkipOpaque;
    if(g.DefinedState)g.DefinedState();
    static unsigned samples=0;
    if(++samples%180==1)LOGI("[mirror] quality=%d far=%.0f map=%d traffic=%d lod=%d atoms=%d requests=%d creates=%d cpu=%.3fms fov=%d",
        quality,farClip,mapCount,trafficCount,lods,atoms,requests,creates,perf::ThreadCpuMs()-start,driving::RearMirrorFov());
}
void DrawSurface() {
    if(!visible)return;
    constexpr int states[]={1,6,8,9,12,14,20};
    std::uintptr_t saved[std::size(states)]{};
    for(std::size_t i=0;i<std::size(states);++i)g.RwRenderStateGet(states[i],&saved[i]);
    g.RwRenderStateSet(6,reinterpret_cast<void*>(1));g.RwRenderStateSet(8,reinterpret_cast<void*>(1));
    g.RwRenderStateSet(9,reinterpret_cast<void*>(2)); // linear filtering
    g.RwRenderStateSet(12,nullptr);g.RwRenderStateSet(14,nullptr);
    g.RwRenderStateSet(20,reinterpret_cast<void*>(1));
    const auto quad=[](float w,float h,float offset,void* texture,std::uint32_t color) {
        const V3 center=surface.pos+surface.forward*offset;
        SkyBackdropVertex vertices[4];
        for(int i=0;i<4;++i) {
            const float x=(i==0||i==3)?-1.f:1.f,y=i<2?1.f:-1.f;
            // Back-facing camera's image is horizontally reflected for mirror
            // behaviour. Camera-texture V follows RenderWare's native raster.
            vertices[i]={center+surface.right*(x*w)+surface.up*(y*h),{},color,
                x<0?1.f:0.f,y>0?0.f:1.f};
        }
        unsigned short indices[]={0,1,2,0,2,3};
        g.RwRenderStateSet(1,texture);
        if(g.RwIm3DTransform(vertices,4,nullptr,1u|8u|16u)) {
            g.RwIm3DRenderIndexedPrimitive(3,indices,6);g.RwIm3DEnd();
        }
    };
    quad(policy::HalfWidth+.008f,policy::HalfHeight+.008f,0,nullptr,0xff161616u);
    quad(policy::HalfWidth,policy::HalfHeight,.001f,raster,0xffffffffu);
    for(std::size_t i=0;i<std::size(states);++i)g.RwRenderStateSet(states[i],reinterpret_cast<void*>(saved[i]));
}
} // namespace rear_mirror_render
