    namespace wm=savr::water_monitor_render;
    std::ifstream waterFile("WATER_ASSET_PATH",std::ios::binary);
    std::vector<unsigned char> waterBytes((std::istreambuf_iterator<char>(waterFile)),{});
    CHECK(wm::model.Load(waterBytes.data(),waterBytes.size()));
    opt::ResetFrame();
    auto& remote=opt::optics[2];remote.raster=road.atomic.data();remote.imageReady=true;
    auto& monitor=savr::driving::monitorVisual;monitor.visible=monitor.cameraActive=true;monitor.owner=1;
    monitor.body={{0,1,0},{1,0,0},{0,1,0},{0,0,1}};monitor.dock=monitor.body;
    monitor.nozzleWorld={{0,0,1},{1,0,0},{0,1,0},{0,0,1}};
    monitor.actualWorld={0,1,0};monitor.cameraWorld={0,0,1};monitor.screen={1,1,1};monitor.vehicle=car.e.data();
    wm::Prepare();CHECK(wm::ready&&remote.visible&&remote.remote&&!remote.excluded);
    CHECK(remote.width==512&&remote.height==384);
    CHECK(remote.frame.camera.right.x<-.999f&&remote.frame.camera.forward.y>.999f);
    CHECK(std::abs(remote.frame.camera.pos.x-.35f)<.0001f&&std::abs(remote.frame.camera.pos.z-1.3f)<.0001f);
    static int remotePasses{},remoteFx{},waterTriangles{};
    static int* remoteScreen=screen;
    g.RwCameraBeginUpdate=[](void* sc){++remotePasses;
        CHECK(std::abs(r::Read<float>(sc,0x94)/r::Read<float>(sc,0x90)-.75f)<.0001f);
        CHECK(remoteScreen[2]==512&&remoteScreen[3]==384);};
    opt::remoteEffects=[](void*){++remoteFx;};
    // Facing away from the headset must not disable a handheld remote camera.
    remote.frame.surface.pos=head.pos;remote.frame.surface.pos.y-=10;remote.frame.surface.forward={0,1,0};
    opt::Render(camera.data(),reinterpret_cast<std::uintptr_t>(&head));
    CHECK(remotePasses==1&&remoteFx==1&&remote.imageReady);
    CHECK(camera==originalCamera&&std::memcmp(&head,&originalHead,sizeof(head))==0);
    CHECK(screen[2]==1920&&screen[3]==1080&&skipBlend&&skipOpaque);
    CHECK(fogState.start==originalFog.start&&fogState.end==originalFog.end);
    // The remote camera now includes its own truck, including the nozzle.
    Put(car.e.data(),8,V3{0,1,0});
    opt::seen.fill(nullptr);opt::candidateCount=0;opt::Consider(car.e.data(),remote);
    CHECK(opt::candidateCount==1&&opt::candidates[0].entity==car.e.data());
    static void* waterVehicleAtomic=car.atomic.data();static bool waterVehicleDrawn{};
    r::api.setupVehicle=[](void*){};r::api.resetVehicle=[](void*){};
    r::api.setupLighting=[](void*){return true;};r::api.removeLighting=[](void*,bool){};
    g.AtomicDefaultRenderCallBack=[](void* atomic)->void*{if(atomic==waterVehicleAtomic)waterVehicleDrawn=true;return atomic;};
    opt::DrawEntities();CHECK(waterVehicleDrawn);
    const auto waterStates=states;
    static bool checkedWaterUv{};static float waterLight=1.f;
    g.RwIm3DTransform=[](void* p,unsigned n,void*,unsigned)->void*{
        auto* v=static_cast<SkyBackdropVertex*>(p);for(unsigned i=0;i<n;++i)CHECK(Finite(v[i].position));
        if(n==wm::model.vertices.size())for(unsigned i=0;i<n;++i){const auto& source=wm::model.vertices[i];if(source.role==3){
            // The shipped screen UV runs left-to-right with physical model X.
            // Do not inherit the old mirror compensation from weapon optics.
            CHECK(std::abs(v[i].u-source.uv[0])<.00001f);if(source.position[0]>.04f)CHECK(v[i].u>.8f);checkedWaterUv=true;
            if(states[1]){const float nx=source.position[0]/.061f,nz=(source.position[2]-.173f)/.04575f;
                const float level=waterLight*(1-.14f*std::clamp(nx*nx+nz*nz,0.f,1.f));CHECK(v[i].color==wm::Color(level,level,level));
                if(waterLight==1.f){const auto world=wm::Point(wm::body,{source.position[0],source.position[1],source.position[2]});
                    CHECK(std::abs(v[i].position.y-world.y)<.000001f);}}}}
        return p;};
    g.RwIm3DRenderIndexedPrimitive=[](int,unsigned short*,int count){waterTriangles+=count/3;return 1;};
    wm::DrawSurface();CHECK(checkedWaterUv&&waterTriangles==3546&&states==waterStates);
    waterTriangles=0;opt::DrawSurfaces();CHECK(waterTriangles==0&&states==waterStates); // no weapon tube for remote
    opt::ResetFrame();monitor.owner=-1;monitor.cameraActive=false;monitor.screen={.3f,.018f,.4f};
    waterLight=.4f;wm::Prepare();CHECK(wm::ready&&!remote.visible);wm::DrawSurface();CHECK(waterTriangles==3726);
    opt::Render(camera.data(),reinterpret_cast<std::uintptr_t>(&head));CHECK(remotePasses==1&&remoteFx==1); // freeze after tail
    waterTriangles=0;monitor.screen={};wm::Prepare();wm::DrawSurface();CHECK(waterTriangles==3222&&states==waterStates);
    monitor.visible=false;wm::Prepare();CHECK(!wm::ready);
    monitor.visible=true;monitor.cameraActive=true;monitor.cameraSettings[0]=55;monitor.cameraSettings[1]=10;monitor.cameraSettings[2]=45;
    monitor.cameraSettings[3]=15;monitor.cameraSettings[4]=25;monitor.cameraSettings[5]=30;
    const auto oldNozzle=monitor.nozzleWorld;wm::Prepare();
    CHECK(std::abs(remote.frame.camera.pos.x-.55f)<.0001f&&std::abs(remote.frame.camera.pos.y-.1f)<.0001f&&std::abs(remote.frame.camera.pos.z-1.45f)<.0001f);
    CHECK(std::abs(remote.frame.camera.forward.x)>.1f&&std::abs(remote.frame.camera.forward.z)>.1f);
    CHECK(monitor.actualWorld.y==1&&monitor.nozzleWorld.center.z==oldNozzle.center.z&&monitor.nozzleWorld.forward.y==1);
    const auto unrolledCamera=remote.frame.camera;
    monitor.body.right={0,0,-1};monitor.body.up={1,0,0};wm::Prepare();
    CHECK(std::memcmp(&unrolledCamera,&remote.frame.camera,sizeof(unrolledCamera))==0);
    std::array<unsigned char,watermonitor::CannonStride*3> cannonData{};
    Put(cannonData.data(),0,std::uint32_t{42});Put(cannonData.data(),4,std::int16_t{1});
    Put(cannonData.data(),0xc,watermonitor::Vec{0,1.5f,1});Put(cannonData.data(),0x18,watermonitor::Vec{0,1,1});
    cannonData[0x30c]=cannonData[0x30d]=1;
    const auto originalWater=cannonData;wm::streamApi.cannons=cannonData.data();static int waterQueries{};
    g.CWorld_ProcessLineOfSight=[](const GameSymbols::Vec3* a,const GameSymbols::Vec3* b,void*,void**,bool buildings,bool vehicles,bool peds,bool objects,bool dummies,bool seeThrough,bool someObjects,bool shootThrough){
        CHECK(a->y==1&&b->y==1.5f&&buildings&&vehicles&&!peds&&!objects&&!dummies&&!seeThrough&&!someObjects&&!shootThrough);
        ++waterQueries;return true;};
    waterTriangles=0;wm::DrawWaterStream();CHECK(waterTriangles==6&&waterQueries==1&&states==waterStates&&cannonData==originalWater);
    wm::DrawWaterStream();CHECK(waterTriangles==12&&waterQueries==2&&cannonData==originalWater);
    cannonData[0x30c]=0;wm::DrawWaterStream();CHECK(waterTriangles==12); // no fabricated stream after native state disappears
    std::puts("water monitor: camera axes/aspect, own truck render, CRT filter, independent device roll, frozen shutdown, UV and render state restoration PASS");
    std::puts("water stream: real native samples, collision-only query, no simulation/audio/particle calls, repeatable geometry and state restoration PASS");
