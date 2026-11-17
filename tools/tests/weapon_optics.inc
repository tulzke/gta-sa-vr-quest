    using namespace savr::weaponoptic;
    CHECK(Supported(34)&&Supported(36)&&!Supported(33)&&!Supported(35)&&!Supported(43));
    CHECK(StepZoom(34,2.5f,0.f,.1f)==2.5f);
    CHECK(StepZoom(34,2.5f,.25f,.1f)==2.5f);
    CHECK(StepZoom(34,2.5f,1.f,.1f)>2.5f);
    CHECK(StepZoom(36,1.8f,-1.f,.1f)<1.8f);
    CHECK(StepZoom(34,8.f,1.f,.1f)==8.f&&StepZoom(34,2.f,-1.f,.1f)==2.f);
    CHECK(StepZoom(36,4.f,1.f,.1f)==4.f&&StepZoom(36,1.f,-1.f,.1f)==1.f);
    CHECK(StepZoom(34,2.5f,1.f,100.f)==StepZoom(34,2.5f,1.f,.1f));
    CHECK(StepZoom(34,2.5f,std::numeric_limits<float>::quiet_NaN(),.1f)==2.5f);
    CHECK(TanHalfFov(.03f,8.f)<TanHalfFov(.03f,2.f));
    CHECK(ClampField(6,-100)==20&&ClampField(6,999)==300&&ClampField(7,0)==10);
    RwMat gun{{0,1,0},0,{0,0,1},0,{1,0,0},0,{10,20,30},0};
    WeaponOpticFrame opticFrame{};
    calib::opticProfile=Defaults(34);
    CHECK(BuildWeaponOpticFrame(34,gun,opticFrame));
    CHECK(std::fabs(opticFrame.surface.pos.y-19.94f)<.0001f);
    CHECK(std::fabs(opticFrame.surface.pos.z-30.06f)<.0001f);
    CHECK(std::fabs(opticFrame.camera.pos.y-20.09f)<.0001f);
    CHECK(opticFrame.camera.forward.y>.999f&&opticFrame.camera.right.x<-.999f);
    calib::opticProfile.value[2]=50; // lateral position mirrors with the model
    CHECK(BuildWeaponOpticFrame(34,gun,opticFrame));CHECK(opticFrame.surface.pos.x>10.049f);
    gun.a[0]=-1;
    CHECK(BuildWeaponOpticFrame(34,gun,opticFrame));CHECK(opticFrame.surface.pos.x<9.951f);
    CHECK(opticFrame.camera.right.x<-.999f); // never mirror the scene texture
    calib::opticProfile.value[3]=450;calib::opticProfile.value[4]=325;calib::opticProfile.value[5]=-150;
    CHECK(BuildWeaponOpticFrame(34,gun,opticFrame));
    CHECK(std::fabs(Dot(opticFrame.camera.forward,opticFrame.camera.up))<.0001f);
    CHECK(std::fabs(Dot(opticFrame.camera.forward,opticFrame.camera.forward)-1)<.0001f);
    CHECK(!BuildWeaponOpticFrame(35,gun,opticFrame));
    CHECK(Calibratable(PhotoCamera) && !Supported(PhotoCamera));
    calib::opticProfile=Defaults(PhotoCamera);
    WeaponOpticFrame photoBefore{},photoAfter{};
    CHECK(BuildWeaponOpticFrame(PhotoCamera,gun,photoBefore));
    calib::opticProfile.value[0]+=30;
    calib::opticProfile.value[4]+=45;
    calib::opticProfile.value[6]+=20;
    CHECK(BuildWeaponOpticFrame(PhotoCamera,gun,photoAfter));
    CHECK(photoBefore.surface.pos.y < photoAfter.surface.pos.y);
    CHECK(photoBefore.halfSize < photoAfter.halfSize);
    CHECK(std::memcmp(&photoBefore.camera,&photoAfter.camera,sizeof(CamBasis))==0);
    namespace opt=savr::weapon_optic_render;
    opt::Optic optic{};optic.type=34;optic.tanFov=.15f;
    optic.frame.camera={{-1,0,0},{0,-1,0},{0,0,1},{0,0,0}};
    optic.frame.surface=optic.frame.camera;optic.frame.halfSize=.03f;optic.frame.length=.15f;
    opt::candidateCount=0;opt::seen.fill(nullptr);
    Entity pedestrian(10,{0,-30,0},3,1),beyondMirror(11,{0,-200,0},2,3),outside(12,{50,-30,0},3,1);
    opt::Consider(pedestrian.e.data(),optic);opt::Consider(pedestrian.e.data(),optic);
    opt::Consider(beyondMirror.e.data(),optic);opt::Consider(outside.e.data(),optic);
    CHECK(opt::candidateCount==2); // includes pedestrians and distant vehicles
    opt::optics[0]=optic;opt::optics[0].visible=true;opt::optics[0].raster=road.atomic.data();
    opt::optics[1]={};r::api.ready=true;
    bool skipBlend=true,skipOpaque=true;g.skipBlendRender=&skipBlend;g.skipNonblendRender=&skipOpaque;
    opt::Render(camera.data(),reinterpret_cast<std::uintptr_t>(&head));
    CHECK(camera==originalCamera&&std::memcmp(&head,&originalHead,sizeof(head))==0);
    CHECK(skipBlend&&skipOpaque&&screen[2]==1920&&screen[3]==1080);
    CHECK(fogState.start==originalFog.start&&fogState.end==originalFog.end);
    CHECK(opt::optics[0].imageReady);
    // Native Quest raster needs reversed U on the world-space lens. Keep the
    // surface/target axes intact and compensate only the texture coordinates.
    static std::vector<SkyBackdropVertex> opticVertices;
    g.RwIm3DTransform=[](void* vertices,unsigned n,void*,unsigned)->void* {
        auto* first=static_cast<SkyBackdropVertex*>(vertices);
        opticVertices.insert(opticVertices.end(),first,first+n);return vertices;
    };
    states[10]=5;states[11]=6;
    const auto beforeOpticStates=states;
    opt::DrawSurfaces();CHECK(states==beforeOpticStates&&!opticVertices.empty());
    for(const auto& vertex:opticVertices) {
        CHECK(Finite(vertex.position)&&std::isfinite(vertex.u)&&std::isfinite(vertex.v));
        CHECK((vertex.position.x>=0 && vertex.u<=.501f)||(vertex.position.x<=0 && vertex.u>=.499f));
    }
    const auto defaultVertices=opticVertices;
    for(bool flipH : {false,true}) for(bool flipV : {false,true}) {
        opt::optics[0].flipHorizontal=flipH;opt::optics[0].flipVertical=flipV;
        opticVertices.clear();opt::DrawSurfaces();
        CHECK(opticVertices.size()==defaultVertices.size());
        for(std::size_t i=0;i<opticVertices.size();++i) {
            const auto& v=opticVertices[i];
            CHECK(std::memcmp(&v.position,&defaultVertices[i].position,sizeof(V3))==0);
            CHECK(v.position.x==0.f || (v.u-.5f)*v.position.x*(flipH ? -1.f : 1.f)>=-.00001f);
            CHECK(v.position.z==0.f || (v.v-.5f)*v.position.z*(flipV ? 1.f : -1.f)>=-.00001f);
        }
        CHECK(states==beforeOpticStates);
    }
    CHECK(ClampField(8,-1)==0&&ClampField(8,2)==1&&ClampField(9,2)==1);
    CHECK(Defaults(34).value[8]==1&&Defaults(34).value[9]==0);
    opt::ResetFrame();CHECK(!opt::optics[0].visible);
    std::puts("weapon optics: supported types, zoom/clamps, mirrored mount, camera basis, ped/vehicle frustum, camera/fog/state restore, unreflected world-space lens PASS");
    namespace heat=savr::heat_test;
    alignas(16) char pool[0x20]{};
    std::vector<char> vehicles(2*0xc68);
    unsigned char flags[2]={7,0x80};
    Put(pool,0,vehicles.data());Put(pool,8,static_cast<const unsigned char*>(flags));Put(pool,0x10,2);
    void* poolAddress=pool;g.CPools_ms_pVehiclePool=&poolAddress;
    Put(vehicles.data(),8,V3{0,-60,0});Put(vehicles.data(),0x28,std::uint32_t{128});
    void* ignoreEntity=reinterpret_cast<void*>(0x123);g.CWorld_pIgnoreEntity=&ignoreEntity;
    static bool occluded=false;
    g.CWorld_ProcessLineOfSight=[](const GameSymbols::Vec3*,const GameSymbols::Vec3*,void*,void** hit,
        bool,bool,bool,bool,bool,bool,bool,bool){*hit=nullptr;return occluded;};
    calib::opticProfile=Defaults(36);
    heat::UpdateHeat(nullptr,1,1000);CHECK(heat::gHeatLock.candidate&&!heat::gHeatLock.locked);
    heat::UpdateHeat(nullptr,1,1400);CHECK(heat::gHeatLock.progress==.5f);
    heat::UpdateHeat(nullptr,1,1800);CHECK(heat::gHeatLock.locked&&heat::ValidHeatEntity()==vehicles.data());
    CHECK(ignoreEntity==reinterpret_cast<void*>(0x123));
    flags[0]=8;CHECK(!heat::ValidHeatEntity()); // same address, new vehicle generation
    heat::UpdateHeat(nullptr,1,1900);CHECK(!heat::gHeatLock.locked);
    flags[0]=0x80;heat::UpdateHeat(nullptr,1,2000);CHECK(!heat::gHeatLock.candidate);
    flags[0]=8;heat::UpdateHeat(nullptr,1,2100);heat::UpdateHeat(nullptr,1,3000);CHECK(heat::gHeatLock.locked);
    occluded=true;heat::UpdateHeat(nullptr,1,3100);CHECK(!heat::gHeatLock.candidate&&!heat::ValidHeatEntity());
    occluded=false;Put(vehicles.data(),8,V3{100,-60,0});heat::UpdateHeat(nullptr,1,3200);CHECK(!heat::gHeatLock.candidate);
    float muzzle[]={.2f,0,0},direction[]={1,0,0};
    CHECK(heat::ApplyReticleAim(1,34,muzzle,direction));
    CHECK(direction[1]<-.999f&&direction[0]<0); // points to weapon optic, not HMD centre
    float untouched[]={1,0,0};CHECK(heat::ApplyReticleAim(1,35,muzzle,untouched)&&untouched[0]==1);
    std::puts("heat lock: dwell, occlusion loss, pool generation/reuse, ignore restore; optic zero and RPG physical ray PASS");
    for(float y : {-1.f,-.8f,.8f,1.f}) for(float x : {-.6f,-.2f,0.f,.2f,.6f})
        CHECK(locomotion::FilterTurnStickX(x,y)==0.f);
    CHECK(locomotion::FilterTurnStickX(.19f,0.f)==0.f);
    CHECK(locomotion::FilterTurnStickX(.7f,.7f)==0.f);
    CHECK(locomotion::FilterTurnStickX(1.f,.3f)==1.f);
    CHECK(locomotion::FilterTurnStickX(-1.f,-.3f)==-1.f);
    CHECK(std::abs(locomotion::FilterTurnStickX(.6f,0.f)-.5f)<.0001f);
    CHECK(locomotion::FilterTurnStickX(std::numeric_limits<float>::quiet_NaN(),0.f)==0.f);
    CHECK(locomotion::FilterTurnStickX(1.f,std::numeric_limits<float>::infinity())==0.f);
    std::puts("turn stick: vertical zoom sectors, centre drift, diagonals suppressed; deliberate left/right and smooth scaling PASS");
    int parsedSet=-1,parsedType=-1;Profile parsed;
    CHECK(ParseProfile("optic 1 34 -12 12 3 4 5 6 12 30 0 1",parsedSet,parsedType,parsed));
    CHECK(parsedSet==1&&parsedType==34&&parsed.value[0]==-60&&parsed.value[2]==15);
    CHECK(parsed.value[6]==60&&parsed.value[7]==150&&parsed.value[8]==0&&parsed.value[9]==1);
    CHECK(ParseProfile("optic 0 36 -24 16 0 0 0 0 24 12",parsedSet,parsedType,parsed));
    CHECK(parsed.value[0]==-120&&parsed.value[8]==1&&parsed.value[9]==0);
    CHECK(ParseProfile("optic2 0 34 -61 61 1 1 0 0 60 150 1 1",parsedSet,parsedType,parsed));
    CHECK(parsed.value[0]==-61&&parsed.value[1]==61&&parsed.value[2]==1&&parsed.value[3]==1);
    CHECK(ParseProfile("optic2 1 43 -60 0 0 0 0 0 100 150 1 0",parsedSet,parsedType,parsed));
    CHECK(parsedSet==1&&parsedType==43&&parsed.value[6]==100);
    CHECK(!ParseProfile("optic2 2 34 0 0 0 0 0 0 60 150 1 0",parsedSet,parsedType,parsed));
    CHECK(!ParseProfile("optic2 0 35 0 0 0 0 0 0 60 150 1 0",parsedSet,parsedType,parsed));
    CHECK(!ParseProfile("optic2 0 34 0 0 0",parsedSet,parsedType,parsed));
    CHECK(NearEye(.2f,-.2f,.03f,.03f,false));
    CHECK(!NearEye(.4f,-.4f,0.f,.03f,false));
    CHECK(NearEye(.4f,-.4f,0.f,.03f,true));
    CHECK(!NearEye(.46f,-.46f,0.f,.03f,true));
    CHECK(!NearEye(.2f,.2f,0.f,.03f,false)); // front of objective
    CHECK(!NearEye(.2f,-.1f,.18f,.03f,false)); // eye beside tube
    opt::Optic farOptic=optic;Entity farTarget(13,{0,-900,0},1,3);
    opt::candidateCount=0;opt::seen.fill(nullptr);
    opt::Consider(farTarget.e.data(),farOptic);CHECK(opt::candidateCount==0);
    farOptic.detailed=true;farOptic.farClip=DetailedFar;opt::seen.fill(nullptr);
    opt::Consider(farTarget.e.data(),farOptic);CHECK(opt::candidateCount==1);
    calib::opticProfile=Defaults(34);
    WeaponOpticFrame nearFrame;CHECK(BuildWeaponOpticFrame(34,gun,nearFrame));
    CamBasis nearHead=nearFrame.surface;nearHead.pos=nearFrame.surface.pos-nearFrame.surface.forward*.2f;
    opt::Prepare(0,34,gun,nearHead);CHECK(opt::optics[0].detailed&&opt::optics[0].farClip==DetailedFar);
    const auto savedHeadForDetail=head;
    head=nearHead;
    static float detailedClip=0.f,detailedFog=0.f;
    g.RwCameraEndUpdate=[](void* c){detailedClip=r::Read<float>(c,0xac);detailedFog=fogState.end;};
    opt::Render(camera.data(),reinterpret_cast<std::uintptr_t>(&head));
    CHECK(detailedClip==DetailedFar&&detailedFog==DetailedFar);
    CHECK(camera==originalCamera&&fogState.start==originalFog.start&&fogState.end==originalFog.end);
    head=savedHeadForDetail;g.RwCameraEndUpdate=[](void*){};
    nearHead.pos=nearFrame.surface.pos-nearFrame.surface.forward*.4f;
    opt::Prepare(0,34,gun,nearHead);CHECK(opt::optics[0].detailed); // hysteresis
    nearHead.pos=nearFrame.surface.pos-nearFrame.surface.forward*.5f;
    opt::Prepare(0,34,gun,nearHead);CHECK(!opt::optics[0].detailed&&opt::optics[0].farClip==NormalFar);
    std::puts("optic range: eye-close 1200m, lowered 450m, hysteresis/front/side; legacy profile migration and 0.1-unit precision PASS");
