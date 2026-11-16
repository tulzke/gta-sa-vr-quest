// Included after cockpit coordinate helpers in Driving.cpp.
bool HydroEquipment(void* vehicle){
    if(!vehicle)return false;const auto flags=*reinterpret_cast<const std::uint32_t*>(static_cast<const char*>(vehicle)+0x4b8);
    return (flags&0x30000u)!=0&&(flags&0x40000u)==0;
}
void HydroModeClick(int hand){
    xr::TriggerHaptic(hand,.65f,180.f,18.f);
    if(g.AudioEngine&&g.CAudioEngine_ReportFrontendAudioEvent)
        g.CAudioEngine_ReportFrontendAudioEvent(g.AudioEngine,1,-3.f,1.f);
}
bool BuildHydraulicsTracking(WheelVisualState& v,void* vehicle,bool preview=false){
    v.hydroVisible=false;const int model=v.modelId;
    if(!vehicle||!HydraulicsCompatible(model)||ThirdPersonViewActive()||!g.FindPlayerPed||!g.CVehicle_IsDriver||
       !g.CVehicle_IsDriver(vehicle,g.FindPlayerPed(-1)))return false;
    const bool editor=preview||GetVehicleDebugMode()!=0,installed=HydroEquipment(vehicle);
    if(!editor&&(!installed||!g_hydroHooksReady||ModeForAppearance(1)!=MODE_IMMERSIVE))return false;
    V3 vr{},vf{},vu{},vp{};if(!ReadEntityMatrix(vehicle,&vr,&vf,&vu,&vp))return false;
    V3 local{},right=vr,forward=vf,up=vu;float pose[12]{};bool custom=false;v.hydroScale=hydrocontrol::kModelScale;
    {std::lock_guard<std::mutex> lock(g_modelMutex);const auto it=g_modelCalibration.find(model);
        if(it!=g_modelCalibration.end()){v.hydroScale=hydrocontrol::kModelScale*it->second.hydroScale;custom=it->second.hydroPoseSet;if(custom)std::copy_n(it->second.hydroPose,12,pose);}}
    if(custom){local={pose[0],pose[1],pose[2]};right=Normalized(vr*pose[3]+vf*pose[4]+vu*pose[5]);
        forward=Normalized(vr*pose[6]+vf*pose[7]+vu*pose[8]);up=Normalized(vr*pose[9]+vf*pose[10]+vu*pose[11]);
        if(!std::isfinite(Length(local))||Length(local)>10||Dot(Cross(right,forward),up)<.95f)return false;
    }else{
        // Separate from the central-tunnel nitro lever: 30 cm right of wheel,
        // 8 cm forward and 12 cm below, measured at the spherical grip centre.
        local=V3{v.wheelVehicleCenter[0],v.wheelVehicleCenter[1],v.wheelVehicleCenter[2]}+
            V3{.30f+.032f*v.hydroScale,.08f-.008f*v.hydroScale,-.12f-.073f*v.hydroScale};
    }
    const V3 center=vp+vr*local.x+vf*local.y+vu*local.z;V3 c{},r{},f{},u{};
    if(!WorldPointToTracking(center,&c)||!WorldPointToTracking(center+right,&r)||!WorldPointToTracking(center+forward,&f)||!WorldPointToTracking(center+up,&u))return false;
    const V3 vectors[]{c,Normalized(r-c),Normalized(f-c),Normalized(u-c)};
    float* fields[]{v.hydroCenter,v.hydroRight,v.hydroForward,v.hydroUp};
    for(int i=0;i<4;++i){fields[i][0]=vectors[i].x;fields[i][1]=vectors[i].y;fields[i][2]=vectors[i].z;}
    v.hydroVisible=true;v.hydroGhost=!installed;v.hydroFour=hydraulics::DanceRunning();
    v.hydroHigh=editor?g_hydroPreviewHigh:*reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(vehicle)+0xa88)>=500;
    return true;
}
hydrocontrol::Point HydroLocalPoint(const WheelVisualState& v,V3 point){
    const V3 c{v.hydroCenter[0],v.hydroCenter[1],v.hydroCenter[2]};const V3 p=(point-c)*(1.f/std::max(.4f,v.hydroScale));
    return {Dot(p,{v.hydroRight[0],v.hydroRight[1],v.hydroRight[2]}),Dot(p,{v.hydroForward[0],v.hydroForward[1],v.hydroForward[2]}),Dot(p,{v.hydroUp[0],v.hydroUp[1],v.hydroUp[2]})};
}
hydrocontrol::Point HydroHandPoint(const WheelVisualState& v,const xr::HandPose& h){
    return HydroLocalPoint(v,{h.gripPos[0],h.gripPos[1],h.gripPos[2]})-hydrocontrol::Point{-.032f,.008f,.018f};
}
hydrocontrol::InputFrame HydroInputFrame(const WheelVisualState& v){
    const hydrocontrol::Point r{v.hydroRight[0],v.hydroRight[1],v.hydroRight[2]},
        f{v.hydroForward[0],v.hydroForward[1],v.hydroForward[2]},u{v.hydroUp[0],v.hydroUp[1],v.hydroUp[2]},
        c{v.hydroCenter[0],v.hydroCenter[1],v.hydroCenter[2]};
    return {c+(r*-.032f+f*.008f+u*.018f)*v.hydroScale,r,f,u,v.hydroScale};
}
hydrocontrol::Point HydroButtonPoint(const WheelVisualState& v,const xr::HandPose& h,int hand){
    // Full Point pose from the actual hand GLB, independent of the current
    // gesture blend: approaching the cap cannot press it by straightening.
    float tip[3]{};if(!xr::GetPointingFingertip(h,hand,tip))return {1e6f,1e6f,1e6f};
    return HydroLocalPoint(v,{tip[0],tip[1],tip[2]})-hydrocontrol::Point{.047f,-.015f,.022f};
}
float HydroButtonDistance(const WheelVisualState& v,const xr::HandPose& h,int hand){
    const auto p=HydroButtonPoint(v,h,hand);
    return g_hydroLever.button[hand].WithinHover(p)?0.f:1e6f;
}
void UpdateHydraulicsJoystick(WheelVisualState& v,void* vehicle,const xr::InputState& input,const xr::HandPose poses[2],unsigned occupied,bool preview=false){
    if(!BuildHydraulicsTracking(v,vehicle,preview)){g_hydroLever.Reset();g_hydroManagedVehicle.store(nullptr);g_hydroToggle.store(false);return;}
    const bool editor=preview||GetVehicleDebugMode()!=0;
    const auto key=vehiclecabin::SeatKey(g,vehicle,nullptr,v.modelId,-1);
    if(g_hydroVehicle!=vehicle||!(key==g_hydroSeatKey)||editor!=g_hydroWasPreview){
        g_hydroVehicle=vehicle;g_hydroSeatKey=key;g_hydroLever.Reset();g_hydroLastTime=0;g_hydroToggle.store(false);g_hydroPreviewHigh=false;g_hydroWasPreview=editor;}
    const double now=MonotonicSeconds();const float dt=g_hydroLastTime?static_cast<float>(now-g_hydroLastTime):0.f;g_hydroLastTime=now;
    void* pad=g.CPad_GetPad?g.CPad_GetPad(0):nullptr;
    const bool controls=pad&&*reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(pad)+0x110)==0;
    const bool enabled=editor||(v.active&&!v.hydroGhost&&ModeForAppearance(1)==MODE_IMMERSIVE&&controls);
    hydrocontrol::Hand h[2];
    for(int i=0;i<2;++i)h[i]={{poses[i].gripPos[0],poses[i].gripPos[1],poses[i].gripPos[2]},HydroButtonPoint(v,poses[i],i),input.grip[i],poses[i].valid,
        !(occupied&(1u<<i))&&(preview||v.interactionTarget[i]==7)};
    const int previous=g_hydroLever.owner;
    if(g_hydroLever.four!=v.hydroFour)g_hydroToggle.store(false);
    g_hydroLever.StepTracked(h,HydroInputFrame(v),dt,enabled,v.hydroFour,xr::CaptureTrackingSnapshot().epoch);
    if(g_hydroLever.owner>=0&&g_hydroLever.owner!=previous){++g_hydroGrabSequence;xr::TriggerHaptic(g_hydroLever.owner,.18f,0,15);}
    if(g_hydroLever.changed&&g_hydroLever.owner>=0){
        xr::TriggerHaptic(g_hydroLever.owner,.32f,0,22);
        if(g.AudioEngine&&g.CAudioEngine_ReportFrontendAudioEvent)g.CAudioEngine_ReportFrontendAudioEvent(g.AudioEngine,1,-14.f,1.f);}
    v.hydroPress=0;
    for(int i=0;i<2;++i){
        const bool free=enabled&&g_hydroLever.owner<0&&g_hydroLever.slot==0&&!(occupied&(1u<<i))&&
            (preview||v.interactionTarget[i]==8);
        auto& b=g_hydroLever.button[i];b.Step(h[i].finger,poses[i].valid,free,dt);
        if(b.clicked){if(editor)g_hydroPreviewHigh=!g_hydroPreviewHigh;else g_hydroToggle.store(true,std::memory_order_release);
            HydroModeClick(i);}
        v.hydroPoint[i]=b.hover;v.hydroPress=std::max(v.hydroPress,b.depth);
        v.hydroGrabbed[i]=g_hydroLever.owner==i;v.hydroGrabId[i]=v.hydroGrabbed[i]?g_hydroGrabSequence:0;
    }
    v.hydroX=g_hydroLever.visualX;v.hydroY=g_hydroLever.visualY;v.hydroSlot=g_hydroLever.slot;
    if(editor)v.hydroHigh=g_hydroPreviewHigh;
    const auto command=g_hydroLever.Command();g_hydroCommandX.store(command.x);g_hydroCommandY.store(command.y);
    g_hydroManagedVehicle.store(!editor&&enabled?vehicle:nullptr,std::memory_order_release);
}
