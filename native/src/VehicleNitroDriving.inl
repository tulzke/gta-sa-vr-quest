bool BuildNitroTracking(WheelVisualState& visual,void* vehicle,bool preview=false){
    visual.nitroVisible=false;
    const int model=visual.modelId;visual.nitroScale=nitro::kModelScale;
    if(!vehicle||!NitroCompatible(model)||ThirdPersonViewActive()||
       *reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(vehicle)+0x32)!=model||
       !g.FindPlayerPed||!g.CVehicle_IsDriver||!g.CVehicle_IsDriver(vehicle,g.FindPlayerPed(-1)))return false;
    const auto sample=ReadNitroSample(vehicle);
    const bool editor=preview||GetVehicleDebugMode()!=0;
    if(!sample.installed&&!editor)return false;
    V3 vr{},vf{},vu{},vp{};if(!ReadEntityMatrix(vehicle,&vr,&vf,&vu,&vp))return false;
    V3 local{},right=vr,forward=vf,up=vu;float pose[12]{};bool custom=false;
    {std::lock_guard<std::mutex> lock(g_modelMutex);const auto it=g_modelCalibration.find(model);
        if(it!=g_modelCalibration.end()){
            visual.nitroScale=nitro::kModelScale*it->second.nitroScale;custom=it->second.nitroPoseSet;
            if(custom)std::copy_n(it->second.nitroPose,12,pose);}}
    if(custom){
        local={pose[0],pose[1],pose[2]};right=Normalized(vr*pose[3]+vf*pose[4]+vu*pose[5]);
        forward=Normalized(vr*pose[6]+vf*pose[7]+vu*pose[8]);up=Normalized(vr*pose[9]+vf*pose[10]+vu*pose[11]);
        if(!std::isfinite(Length(local))||Length(local)>10||Dot(Cross(right,forward),up)<.95f)return false;
    }else{
        // Do not invert the previous frame's tracking-space wheel with the
        // current camera transform: that makes the default mount slide.
        const V3 wheelLocal{visual.wheelVehicleCenter[0],visual.wheelVehicleCenter[1],visual.wheelVehicleCenter[2]};
        local=wheelLocal+V3{std::max(.24f,visual.radius+.10f),-.035f,-.16f};
    }
    const V3 center=vp+vr*local.x+vf*local.y+vu*local.z;V3 c{},r{},f{},u{};
    if(!WorldPointToTracking(center,&c)||!WorldPointToTracking(center+right,&r)||!WorldPointToTracking(center+forward,&f)||!WorldPointToTracking(center+up,&u))return false;
    const V3 vectors[]{c,Normalized(r-c),Normalized(f-c),Normalized(u-c)};
    float* fields[]{visual.nitroCenter,visual.nitroRight,visual.nitroForward,visual.nitroUp};
    for(int i=0;i<4;++i){fields[i][0]=vectors[i].x;fields[i][1]=vectors[i].y;fields[i][2]=vectors[i].z;}
    visual.nitroVisible=true;visual.nitroGhost=!sample.installed;
    visual.nitroReserve=visual.nitroGhost?.6f:sample.reserve;visual.nitroFlow=sample.flow&&!editor;
    return true;
}
nitro::Point NitroHandPoint(const WheelVisualState& v,const xr::HandPose& hand){
    const V3 c{v.nitroCenter[0],v.nitroCenter[1],v.nitroCenter[2]};
    const V3 r{v.nitroRight[0],v.nitroRight[1],v.nitroRight[2]},f{v.nitroForward[0],v.nitroForward[1],v.nitroForward[2]},u{v.nitroUp[0],v.nitroUp[1],v.nitroUp[2]};
    const float scale=std::max(.4f,v.nitroScale);
    const V3 p=(V3{hand.gripPos[0],hand.gripPos[1],hand.gripPos[2]}-c)*(1.f/scale)-f*.018f-u*.048f;
    return {Dot(p,r),Dot(p,f),Dot(p,u)};
}
void UpdateNitroLever(WheelVisualState& visual,void* vehicle,const xr::InputState& input,
                      const xr::HandPose poses[2],unsigned occupied,bool preview=false){
    if(!BuildNitroTracking(visual,vehicle,preview)){
        g_nitroLever.Reset();g_nitroFeel.Reset();g_nitroSpeed.Reset();g_nitroCommand.store(false);g_nitroManagedVehicle.store(nullptr);return;}
    const auto key=vehiclecabin::SeatKey(g,vehicle,nullptr,visual.modelId,-1);
    if(g_nitroVehicle!=vehicle||!(key==g_nitroSeatKey)){g_nitroVehicle=vehicle;g_nitroSeatKey=key;g_nitroLever.Reset();g_nitroFeel.Reset();g_nitroSpeed.Reset();g_nitroLastTime=0;}
    const bool editor=preview||GetVehicleDebugMode()!=0;
    if(editor!=g_nitroWasPreview){g_nitroLever.Reset();g_nitroFeel.Reset();g_nitroSpeed.Reset();g_nitroLastTime=0;g_nitroWasPreview=editor;}
    const double now=MonotonicSeconds();const float dt=g_nitroLastTime?static_cast<float>(now-g_nitroLastTime):0.f;g_nitroLastTime=now;
    nitro::Hand hands[2];for(int h=0;h<2;++h)hands[h]={NitroHandPoint(visual,poses[h]),input.grip[h],poses[h].valid,(occupied&(1u<<h))==0};
    const auto sample=ReadNitroSample(vehicle);
    const void* pad=g.CPad_GetPad?g.CPad_GetPad(0):nullptr;
    const bool controls=pad&&*reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(pad)+0x110)==0;
    const bool enabled=editor||(visual.active&&!visual.nitroGhost&&ModeForAppearance(1)==MODE_IMMERSIVE&&EngineOn(vehicle)&&!EngineBroken(vehicle)&&controls);
    const auto before=g_nitroLever;const int previous=before.owner;
    g_nitroLever.Step(hands,dt,enabled,editor||sample.usable);
    const int feedbackHand=g_nitroLever.owner>=0?g_nitroLever.owner:previous;
    const bool feedbackValid=enabled&&feedbackHand>=0&&hands[feedbackHand].valid&&hands[feedbackHand].available;
    const auto feel=g_nitroFeel.Step(before,g_nitroLever,dt,feedbackValid);
    if(feel.hand>=0)xr::TriggerHaptic(feel.hand,feel.amplitude,0,feel.duration);
    if(feel.click)nitroaudio::Click(g_nitroLever.on);
    if(feel.hand<0&&previous>=0&&g_nitroLever.owner<0&&!editor&&!sample.usable)xr::TriggerHaptic(previous,.12f,0,40);
    visual.nitroAngle=nitro::Angle(g_nitroLever.position);visual.nitroGripX=g_nitroLever.gripX;
    for(int h=0;h<2;++h)visual.nitroGrabbed[h]=g_nitroLever.owner==h;
    if(editor)visual.nitroReserve=.6f;
    visual.nitroEffect=g_nitroSpeed.Step(sample.flow&&g_nitroLever.on,dt,enabled&&!editor);
    g_nitroCommand.store(!editor&&enabled&&g_nitroLever.on,std::memory_order_release);
    // Keep ready OFF managed too, otherwise the vanilla shared button bypasses it.
    g_nitroManagedVehicle.store(!editor&&visual.active&&!visual.nitroGhost&&ModeForAppearance(1)==MODE_IMMERSIVE?vehicle:nullptr,std::memory_order_release);
}
