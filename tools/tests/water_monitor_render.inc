    namespace wm=savr::water_monitor_render;
    std::ifstream waterFile("WATER_ASSET_PATH",std::ios::binary);
    std::vector<unsigned char> waterBytes((std::istreambuf_iterator<char>(waterFile)),{});
    CHECK(wm::model.Load(waterBytes.data(),waterBytes.size()));
    opt::ResetFrame();
    auto& remote=opt::optics[2];remote.raster=road.atomic.data();remote.imageReady=true;
    auto& monitor=savr::driving::monitorVisual;monitor.visible=monitor.cameraActive=true;monitor.owner=1;
    monitor.body={{0,1,0},{1,0,0},{0,1,0},{0,0,1}};monitor.dock=monitor.body;
    monitor.actualWorld={0,1,0};monitor.cameraWorld={0,0,1};monitor.screen={1,1,1};monitor.vehicle=car.e.data();
    wm::Prepare();CHECK(wm::ready&&remote.visible&&remote.remote&&remote.excluded==car.e.data());
    CHECK(remote.width==512&&remote.height==384);
    CHECK(remote.frame.camera.right.x<-.999f&&remote.frame.camera.forward.y>.999f);
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
    opt::seen.fill(nullptr);opt::candidateCount=0;opt::Consider(car.e.data(),remote);CHECK(opt::candidateCount==0);
    const auto waterStates=states;
    g.RwIm3DTransform=[](void* p,unsigned n,void*,unsigned)->void*{
        auto* v=static_cast<SkyBackdropVertex*>(p);for(unsigned i=0;i<n;++i)CHECK(Finite(v[i].position));return p;};
    g.RwIm3DRenderIndexedPrimitive=[](int,unsigned short*,int count){waterTriangles+=count/3;return 1;};
    wm::DrawSurface();CHECK(waterTriangles==3726&&states==waterStates);
    waterTriangles=0;opt::DrawSurfaces();CHECK(waterTriangles==0&&states==waterStates); // no weapon tube for remote
    opt::ResetFrame();monitor.owner=-1;monitor.cameraActive=false;monitor.screen={.3f,.018f,.4f};
    wm::Prepare();CHECK(wm::ready&&!remote.visible);wm::DrawSurface();CHECK(waterTriangles==3726);
    opt::Render(camera.data(),reinterpret_cast<std::uintptr_t>(&head));CHECK(remotePasses==1&&remoteFx==1); // freeze after tail
    waterTriangles=0;monitor.screen={};wm::Prepare();wm::DrawSurface();CHECK(waterTriangles==3222&&states==waterStates);
    monitor.visible=false;wm::Prepare();CHECK(!wm::ready);
    std::puts("water monitor: camera axes/aspect, remote facing, owner exclusion, effects pass, frozen shutdown, mesh/CRT draw and render state restoration PASS");
