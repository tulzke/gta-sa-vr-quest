// GameThread-only render-to-texture camera. Like the rear mirror, texture writes
// and both world-space lens reads stay on the same RenderQueue. No XR-context
// texture sharing, recursive RenderScene, PreRender, animation or gameplay tick.
namespace weapon_optic_render {
using rear_mirror_render::Read;
constexpr int Resolution=768;
struct Optic {
    bool visible{}, imageReady{};
    bool flipHorizontal{true},flipVertical{};
    bool detailed{},remote{};
    int width{Resolution},height{Resolution};
    void* excluded{};
    float farClip=weaponoptic::NormalFar;
    int type{};
    void* raster{};
    WeaponOpticFrame frame{};
    float tanFov{};
    scopeaim::HeatLockState lock{};
};
void (*remoteEffects)(void*){};
Optic optics[3]; // The third slot is the Firetruck remote camera.
void ResetFrame() { for(auto& optic:optics) optic.visible=false; }
bool HasRaster(int hand) { return optics[hand].raster!=nullptr; }
void Prepare(int hand,int type,const RwMat& weapon,const CamBasis& head) {
    if(!weaponoptic::Supported(type) || PhotoViewActive() ||
        FormalCutsceneRunning() || ScriptedCutsceneRunning() || !rear_mirror_render::Resolve()) return;
    auto& optic=optics[hand];
    if(!BuildWeaponOpticFrame(type,weapon,optic.frame)) return;
    const auto& surface=optic.frame.surface;
    const V3 eye=head.pos-surface.pos;
    const float axial=Dot(eye,surface.forward);
    const float lateral=std::hypot(Dot(eye,surface.right),Dot(eye,surface.up));
    optic.detailed=weaponoptic::NearEye(std::sqrt(Dot(eye,eye)),axial,lateral,
        optic.frame.halfSize,optic.detailed);
    optic.farClip=optic.detailed ? weaponoptic::DetailedFar : weaponoptic::NormalFar;
    optic.type=type;
    const auto profile=calib::SnapshotOptic(type);
    optic.flipHorizontal=profile.value[8]!=0;
    optic.flipVertical=profile.value[9]!=0;
    optic.tanFov=weaponoptic::TanHalfFov(optic.frame.halfSize,scopeaim::WeaponZoom(type));
    optic.lock=type==36 ? scopeaim::HeatLockSnapshot() : scopeaim::HeatLockState{};
    optic.visible=true;
    if(!optic.raster) optic.raster=g.RwRasterCreate(Resolution,Resolution,32,5);
}
struct Candidate { void* entity{}; float distance{}; int type{}; };
std::array<Candidate,2048> candidates{};
std::array<void*,8192> seen{};
int candidateCount{};
int requests{},creates{};
bool First(void* entity) {
    auto hash=(reinterpret_cast<std::uintptr_t>(entity)>>4)%seen.size();
    for(std::size_t n=0;n<seen.size();++n) {
        auto& entry=seen[(hash+n)%seen.size()];
        if(entry==entity) return false;
        if(!entry) {entry=entity;return true;}
    }
    return false;
}
void Consider(void* entity,const Optic& optic) {
    if(!entity || entity==optic.excluded || !First(entity) || (g.FindPlayerPed && entity==g.FindPlayerPed(-1))) return;
    const int type=Read<unsigned char>(entity,0x5a)&7;
    if(type<1 || type>5 || !(Read<std::uint32_t>(entity,0x28)&(1u<<7))) return;
    void* model=rear_mirror_render::Model(entity);
    if(!model) return;
    V3 position{};if(!ReadEntityWorldPosition(entity,&position)) return;
    const void* collision=Read<void*>(model,0x30);
    const V3 centre=Read<V3>(collision,0x18);
    const void* matrix=Read<void*>(entity,0x18);
    if(matrix) position=position+Read<V3>(matrix,0)*centre.x+Read<V3>(matrix,0x10)*centre.y+Read<V3>(matrix,0x20)*centre.z;
    else {
        const float heading=Read<float>(entity,0x14),c=std::cos(heading),s=std::sin(heading);
        position=position+V3{centre.x*c-centre.y*s,centre.x*s+centre.y*c,centre.z};
    }
    const auto& camera=optic.frame.camera;
    const V3 delta=position-camera.pos;
    const float radius=std::max(Read<float>(collision,0x24),1.f);
    const float depth=Dot(delta,camera.forward);
    const float sideLimit=depth*optic.tanFov+radius*std::sqrt(1.f+optic.tanFov*optic.tanFov);
    if(!Finite(position) || !std::isfinite(radius) || depth+radius<.1f || depth-radius>optic.farClip ||
        std::abs(Dot(delta,camera.right))>sideLimit || std::abs(Dot(delta,camera.up))>sideLimit) return;
    if(!Read<void*>(entity,0x20)) {
        // Asynchronous, bounded supplementation only in the eye-close frustum.
        // The engine owns loading/unloading; never run LoadAllRequestedModels,
        // simulation or PreRender again for the optic.
        if(!optic.detailed || type==2 || type==3) return;
        if(Read<void*>(model,0x40) && creates<4) {
            ++creates;rear_mirror_render::api.createObject(entity);
        } else if(requests<4 && g.CStreaming_RequestModel) {
            ++requests;g.CStreaming_RequestModel(Read<std::int16_t>(entity,0x32),0);
        }
        if(!Read<void*>(entity,0x20)) return;
    }
    const float distance=std::max(0.f,std::sqrt(Dot(delta,delta))-radius);
    // Prefer authored detail when both its detail and linked LOD are resident.
    // A parent is skipped below only when a selected child really replaces it.
    if(candidateCount<static_cast<int>(candidates.size())) candidates[candidateCount++]={entity,distance,type};
}
void Collect(const Optic& optic) {
    candidateCount=requests=creates=0;seen.fill(nullptr);
    const auto index=[](float p){return std::clamp(static_cast<int>(std::floor((p+3000.f)/50.f)),0,119);};
    const V3 pos=optic.frame.camera.pos;
    int visited=0;
    for(int y=index(pos.y-optic.farClip);y<=index(pos.y+optic.farClip);++y)
    for(int x=index(pos.x-optic.farClip);x<=index(pos.x+optic.farClip);++x) {
        const V3 forward=optic.frame.camera.forward;
        const float planar=std::hypot(forward.x,forward.y);
        if(planar>.85f && optic.tanFov<.35f) {
            const float dx=x*50.f-2975.f-pos.x,dy=y*50.f-2975.f-pos.y;
            const float along=(dx*forward.x+dy*forward.y)/planar;
            const float across=std::abs(dx*forward.y-dy*forward.x)/planar;
            if(along < -50.f || across>std::max(0.f,along)*optic.tanFov*1.5f+50.f) continue;
        }
        const auto* normal=reinterpret_cast<const char*>(g.LoadBase+kRetailWorldSectorsOffset)+(y*120+x)*0x10;
        const auto* repeat=reinterpret_cast<const char*>(g.LoadBase+kRetailWorldRepeatSectorsOffset)+((x&15)|((y&15)<<4))*0x18;
        const void* lists[]={Read<void*>(normal,0),Read<void*>(normal,8),Read<void*>(repeat,0),Read<void*>(repeat,8),Read<void*>(repeat,0x10)};
        for(const void* list:lists) for(const void* node=list;node;) {
            if(++visited>65536) return;
            Consider(Read<void*>(node,0),optic);
            const void* next=Read<void*>(node,8);if(next==node)break;node=next;
        }
    }
}
void* DrawPedAtomic(void* atomic,void*) {
    if(!atomic || !(Read<unsigned char>(atomic,2)&4)) return atomic;
    void* frame=Read<void*>(atomic,8);
    if(frame) rear_mirror_render::api.frameLtm(frame);
    using Callback=void* (*)(void*);
    if(auto callback=Read<Callback>(atomic,0x70)) callback(atomic);
    return atomic;
}
void DrawEntities() {
    // Near objects first to keep the additional camera's overdraw bounded.
    std::sort(candidates.begin(),candidates.begin()+candidateCount,
        [](const Candidate& a,const Candidate& b){return a.distance<b.distance;});
    std::array<std::uintptr_t,2048> replacedParents{};
    int parents=0;
    for(int i=0;i<candidateCount;++i)
        if(void* parent=Read<void*>(candidates[i].entity,0x50))
            replacedParents[parents++]=reinterpret_cast<std::uintptr_t>(parent);
    std::sort(replacedParents.begin(),replacedParents.begin()+parents);
    for(int i=0;i<candidateCount;++i) {
        const auto& c=candidates[i];
        if(std::binary_search(replacedParents.begin(),replacedParents.begin()+parents,
            reinterpret_cast<std::uintptr_t>(c.entity))) continue;
        if(c.type==3) {
            void* clump=Read<void*>(c.entity,0x20);
            if(!clump || Read<unsigned char>(clump,0)!=2) continue;
            const bool lit=g.CPed_SetupLighting && g.CPed_SetupLighting(c.entity);
            g.RpClumpForAllAtomics(clump,&DrawPedAtomic,nullptr);
            if(g.CPed_RemoveLighting) g.CPed_RemoveLighting(c.entity,lit);
        } else {
            // Use full-detail vehicle atomics at all optic distances.
            const int savedQuality=rear_mirror_render::quality;
            rear_mirror_render::quality=2;
            rear_mirror_render::DrawEntity({c.entity,c.distance},c.type==2);
            rear_mirror_render::quality=savedQuality;
        }
    }
}
void Render(void* sc,std::uintptr_t cam) {
    for(auto& optic:optics) {
        if(!optic.visible || !optic.raster || !EyeRasterTexId(optic.raster)) continue;
        const CamBasis saved=ReadCam(cam);
        // Skip only images facing away from the headset. The physical body
        // remains drawn. There is no eye-proximity activation or screen mode.
        if(!optic.remote&&Dot(saved.pos-optic.frame.surface.pos,optic.frame.surface.forward)>.03f) continue;
        Collect(optic);
        auto* camera=static_cast<char*>(sc);
        std::array<std::uint8_t,0x34> fields{};
        std::memcpy(fields.data(),camera+0x80,fields.size());
        const auto fog=CaptureMobileDistanceFog();
        const bool skipBlend=g.skipBlendRender && *g.skipBlendRender;
        const bool skipOpaque=g.skipNonblendRender && *g.skipNonblendRender;
        if(g.skipBlendRender)*g.skipBlendRender=false;
        if(g.skipNonblendRender)*g.skipNonblendRender=false;
        ScopedScreenDimensions dimensions(g.RsGlobal);dimensions.Set(optic.width,optic.height);
        WriteCam(cam,optic.frame.camera);
        if(g.CCamera_CalculateDerivedValues)g.CCamera_CalculateDerivedValues(g.TheCamera,false,true);
        g_origCopy(g.TheCamera,false);
        *reinterpret_cast<void**>(camera+0x80)=optic.raster;
        *reinterpret_cast<void**>(camera+0x88)=nullptr;
        *reinterpret_cast<float*>(camera+0x90)=optic.tanFov;
        *reinterpret_cast<float*>(camera+0x94)=optic.tanFov*optic.height/optic.width;
        *reinterpret_cast<float*>(camera+0x98)=1.f/optic.tanFov;
        *reinterpret_cast<float*>(camera+0x9c)=optic.width/(optic.tanFov*optic.height);
        *reinterpret_cast<float*>(camera+0xa0)=0.f;
        *reinterpret_cast<float*>(camera+0xa4)=0.f;
        g.RwCameraSetNearClipPlane(sc,.1f);g.RwCameraSetFarClipPlane(sc,optic.farClip);
        // Keep the weather's relative fog ramp, extending its range with the
        // detailed camera so the old fog end does not hide the additional world.
        const float fogScale=optic.detailed && fog.valid && fog.end>1.f
            ? std::max(1.f,optic.farClip/fog.end) : 1.f;
        const float fogStart=fog.valid ? fog.start*fogScale : optic.farClip*.65f;
        const float fogEnd=fog.valid ? std::min(fog.end*fogScale,optic.farClip) : optic.farClip;
        *reinterpret_cast<float*>(camera+0xb0)=fogStart;
        unsigned char background[4]={110,125,140,255};
        g.RwCameraClear(sc,background,1|2|4);
        g.RwCameraBeginUpdate(sc);
        {
            if(g.DefinedState)g.DefinedState();
            RenderWorldStableSkyPolys(cam,fog);
            if(g.DefinedState)g.DefinedState();
            ApplyMobileDistanceFog(fog,fogStart,fogEnd);
            rear_mirror_render::api.directionalOff();rear_mirror_render::api.ambient();
            DrawEntities();
            // FxManager::Render only submits existing fire/smoke. Never call
            // CWaterCannons::Render here: it also advances native effects.
            if(optic.remote&&remoteEffects)remoteEffects(sc);
            g.RwCameraEndUpdate(sc);
            optic.imageReady=true;
        }
        WriteCam(cam,saved);
        if(g.CCamera_CalculateDerivedValues)g.CCamera_CalculateDerivedValues(g.TheCamera,false,true);
        g_origCopy(g.TheCamera,false);
        std::memcpy(camera+0x80,fields.data(),fields.size());
        g.RwCameraSetNearClipPlane(sc,Read<float>(fields.data(),0x28));
        g.RwCameraSetFarClipPlane(sc,Read<float>(fields.data(),0x2c));
        ApplyMobileDistanceFog(fog,fog.start,fog.end);
        if(g.skipBlendRender)*g.skipBlendRender=skipBlend;
        if(g.skipNonblendRender)*g.skipNonblendRender=skipOpaque;
        if(g.DefinedState)g.DefinedState();
    }
}
// All geometry is in world space with normal depth testing: walls, the other
// eye and the weapon can occlude the lens. Quest's camera raster has reversed
// horizontal U relative to the world-space surface; compensate in sampling,
// keeping the physical camera and target-marker axes unchanged.
void DrawSurfaces() {
    constexpr int states[]={1,6,8,9,10,11,12,14,20};
    std::uintptr_t saved[std::size(states)]{};
    for(std::size_t i=0;i<std::size(states);++i)g.RwRenderStateGet(states[i],&saved[i]);
    g.RwRenderStateSet(6,reinterpret_cast<void*>(1));g.RwRenderStateSet(8,reinterpret_cast<void*>(1));
    g.RwRenderStateSet(9,reinterpret_cast<void*>(2));g.RwRenderStateSet(12,nullptr);
    g.RwRenderStateSet(14,nullptr);g.RwRenderStateSet(20,reinterpret_cast<void*>(1));
    for(const auto& optic:optics) {
        if(!optic.visible||optic.remote) continue;
        const auto& surface=optic.frame.surface;
        const float size=optic.frame.halfSize,length=optic.frame.length;
        const auto vertex=[&](float x,float y,float z,std::uint32_t color) {
            return SkyBackdropVertex{surface.pos-surface.right*x+surface.up*y+surface.forward*z,{},color,
                .5f+(optic.flipHorizontal ? -x : x)/(2.f*size),
                .5f+(optic.flipVertical ? y : -y)/(2.f*size)};
        };
        const auto draw=[&](SkyBackdropVertex* vertices,int count,unsigned short* indices,int indexCount,void* texture) {
            g.RwRenderStateSet(1,texture);
            if(g.RwIm3DTransform(vertices,count,nullptr,1u|8u|16u)) {
                g.RwIm3DRenderIndexedPrimitive(3,indices,indexCount);g.RwIm3DEnd();
            }
        };
        const auto rect=[&](float x0,float y0,float x1,float y1,float z,std::uint32_t color) {
            SkyBackdropVertex vertices[]={vertex(x0,y0,z,color),vertex(x1,y0,z,color),vertex(x1,y1,z,color),vertex(x0,y1,z,color)};
            unsigned short indices[]={0,1,2,0,2,3};draw(vertices,4,indices,6,nullptr);
        };
        const int segments=optic.type==34 ? 40 : 4;
        const float angleOffset=optic.type==34 ? 0.f : .7853981634f;
        const float radius=optic.type==34 ? size : size*1.414213562f;
        for(int i=0;i<segments;++i) {
            const float a=angleOffset+i*6.283185307f/segments,b=angleOffset+(i+1)*6.283185307f/segments;
            const float x0=std::cos(a)*radius,y0=std::sin(a)*radius,x1=std::cos(b)*radius,y1=std::sin(b)*radius;
            SkyBackdropVertex body[]={vertex(x0,y0,0,0xff222222u),vertex(x1,y1,0,0xff222222u),
                vertex(x1,y1,length,0xff111111u),vertex(x0,y0,length,0xff111111u)};
            unsigned short quad[]={0,1,2,0,2,3};draw(body,4,quad,6,nullptr);
            SkyBackdropVertex rim[]={vertex(x0,y0,0,0xff080808u),vertex(x1,y1,0,0xff080808u),
                vertex(x1*.88f,y1*.88f,0,0xff080808u),vertex(x0*.88f,y0*.88f,0,0xff080808u)};
            draw(rim,4,quad,6,nullptr);
            const auto glassColor=optic.imageReady ? 0xffffffffu : 0xff080808u;
            SkyBackdropVertex glass[]={vertex(0,0,.002f,glassColor),vertex(x0*.88f,y0*.88f,.002f,glassColor),
                vertex(x1*.88f,y1*.88f,.002f,glassColor)};
            unsigned short triangle[]={0,1,2};
            draw(glass,3,triangle,3,optic.imageReady ? optic.raster : nullptr);
            SkyBackdropVertex front[]={vertex(0,0,length,0xff161b16u),
                vertex(x0,y0,length,0xff161b16u),vertex(x1,y1,length,0xff161b16u)};
            draw(front,3,triangle,3,nullptr);
        }
        // Put markings just behind the lens, closer to the viewing eye.
        const float thin=std::max(.00018f,size*.008f),gap=size*.09f;
        const std::uint32_t color=optic.type==34 ? 0xff101010u : 0xff40ff40u;
        rect(-size*.72f,-thin,-gap,thin,.001f,color);rect(gap,-thin,size*.72f,thin,.001f,color);
        rect(-thin,-size*.72f,thin,-gap,.001f,color);rect(-thin,gap,thin,size*.72f,.001f,color);
        if(optic.type==36 && optic.lock.candidate) {
            const auto& camera=optic.frame.camera;
            const V3 delta=V3{optic.lock.target[0],optic.lock.target[1],optic.lock.target[2]}-camera.pos;
            const float depth=Dot(delta,camera.forward);
            const float x=-Dot(delta,camera.right)/std::max(.1f,depth)/optic.tanFov*size;
            const float y=Dot(delta,camera.up)/std::max(.1f,depth)/optic.tanFov*size;
            const float box=size*.18f;
            const auto lockColor=optic.lock.locked ? 0xff40ff40u : 0xff40c0ffu;
            if(std::abs(x)+box<size*.86f && std::abs(y)+box<size*.86f) {
                rect(x-box,y-box,x+box,y-box+thin*2,.0008f,lockColor);
                rect(x-box,y+box-thin*2,x+box,y+box,.0008f,lockColor);
                rect(x-box,y-box,x-box+thin*2,y+box,.0008f,lockColor);
                rect(x+box-thin*2,y-box,x+box,y+box,.0008f,lockColor);
            }
            rect(-size*.65f,-size*.78f,-size*.65f+size*1.3f*optic.lock.progress,-size*.73f,.0008f,lockColor);
        }
    }
    for(std::size_t i=0;i<std::size(states);++i)g.RwRenderStateSet(states[i],reinterpret_cast<void*>(saved[i]));
}
} // namespace weapon_optic_render
