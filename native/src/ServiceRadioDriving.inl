// Included inside Driving.cpp's state namespace.
bool ServiceRetailReady(){return serviceradio::RetailReady(g.CTheScripts_ScriptSpace,g.CTheScripts_pActiveScripts);}
serviceradio::Mission ServiceMission(){return serviceradio::ReadMission(g.CTheScripts_ScriptSpace,g.CTheScripts_pActiveScripts);}
tracking::SeatKey ServiceSeat(void* vehicle,int model){
    return vehiclecabin::SeatKey(g,vehicle,g.FindPlayerPed?g.FindPlayerPed(-1):nullptr,model,-1);
}
bool BuildServiceTracking(WheelVisualState& visual,void* vehicle){
    visual.serviceVisible=false;
    const int model=visual.modelId;
    if(!vehicle||serviceradio::ForModel(model)==serviceradio::Service::None||!ServiceRetailReady()||
       *reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(vehicle)+0x32)!=model||
       !g.FindPlayerPed||!g.CVehicle_IsDriver||!g.CVehicle_IsDriver(vehicle,g.FindPlayerPed(-1)))return false;
    V3 vr{},vf{},vu{},vp{};
    if(!ReadIgnitionFrame(vehicle,visual.bike,&vr,&vf,&vu,&vp))return false;
    V3 local{},right=vr,forward=vf,up=vu;float pose[12]{};bool custom=false;
    {std::lock_guard<std::mutex> lock(g_modelMutex);const auto it=g_modelCalibration.find(model);
        if(it!=g_modelCalibration.end()&&it->second.servicePoseSet){std::copy_n(it->second.servicePose,12,pose);custom=true;}}
    if(custom){local={pose[0],pose[1],pose[2]};right=Normalized(vr*pose[3]+vf*pose[4]+vu*pose[5]);
        forward=Normalized(vr*pose[6]+vf*pose[7]+vu*pose[8]);up=Normalized(vr*pose[9]+vf*pose[10]+vu*pose[11]);
        if(!std::isfinite(Length(local))||Length(local)>10||Dot(Cross(right,forward),up)<.95f)return false;
    }else{
        if(!g.CModelInfo_ms_modelInfoPtrs)return false;
        const auto* info=static_cast<const char*>(g.CModelInfo_ms_modelInfoPtrs[model]);
        void* structure{};if(!info)return false;std::memcpy(&structure,info+0x90,sizeof(structure));if(!structure)return false;
        V3 seat{};std::memcpy(&seat,static_cast<const char*>(structure)+0x30,sizeof(seat));
        if(!std::isfinite(Length(seat))||Length(seat)>10)return false;
        local=visual.bike?V3{.18f,seat.y+.48f,seat.z+.24f}:V3{-seat.x+.26f,seat.y+.38f,seat.z+.08f};
    }
    const V3 center=vp+vr*local.x+vf*local.y+vu*local.z;
    V3 c{},r{},f{},u{};
    if(!WorldPointToTracking(center,&c)||!WorldPointToTracking(center+right,&r)||
       !WorldPointToTracking(center+forward,&f)||!WorldPointToTracking(center+up,&u))return false;
    const V3 vectors[]{c,Normalized(r-c),Normalized(f-c),Normalized(u-c)};
    float* fields[]{visual.serviceCenter,visual.serviceRight,visual.serviceForward,visual.serviceUp};
    for(int i=0;i<4;++i){fields[i][0]=vectors[i].x;fields[i][1]=vectors[i].y;fields[i][2]=vectors[i].z;}
    visual.serviceVisible=true;visual.serviceKind=static_cast<int>(serviceradio::ForModel(model));
    visual.serviceVehicle=vehicle;
    return true;
}
void UpdateServiceRadio(WheelVisualState& visual,void* vehicle,const xr::InputState& input,
                        const xr::HandPose poses[2],unsigned occupied){
    if(const auto* tracking=xr::ScopedTrackingFrame::Current();tracking&&tracking->epoch!=g_serviceTrackingEpoch){
        ResetServiceRadio();g_serviceTrackingEpoch=tracking->epoch;
    }
    const auto mission=ServiceMission();
    const bool geometry=BuildServiceTracking(visual,vehicle);
    void* pad=g.CPad_GetPad?g.CPad_GetPad(0):nullptr;
    const bool enabled=geometry&&visual.active&&!g_inputBlocked.load()&&GetVehicleDebugMode()==0&&
        pad&&*reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(pad)+0x110)==0;
    const auto key=ServiceSeat(vehicle,visual.modelId);
    serviceradio::Hand hands[2]{};
    const V3 target{visual.serviceCenter[0]+visual.serviceUp[0]*.064f,
                    visual.serviceCenter[1]+visual.serviceUp[1]*.064f,
                    visual.serviceCenter[2]+visual.serviceUp[2]*.064f};
    for(int h=0;h<2;++h){const V3 p=CockpitFingerPoint(poses[h],h);float q[4]{};
        const bool valid=poses[h].valid&&NormalizeQuaternion(poses[h].gripOri,q)&&
            (!poses[h].aimValid||NormalizeQuaternion(poses[h].aimOri,q))&&
            std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);
        hands[h]={valid,(occupied&(1u<<h))==0,geometry&&valid&&Length(p-target)<.085f,input.grip[h],input.triggers[h]};}
    const int old=g_serviceRadio.owner;
    g_serviceRadio.Step(enabled,key,hands,mission,MonotonicSeconds());
    if(old>=0&&g_serviceRadio.owner<0){
        const bool released=enabled&&hands[old].valid&&hands[old].free&&input.grip[old]<=.30f;
        g_serviceReturnHand=released?old:-1;g_serviceReturnUntil=released?MonotonicSeconds()+.18:0;
    }
    if(g_serviceRadio.grabbed){g_serviceReturnHand=-1;xr::TriggerHaptic(g_serviceRadio.owner,.20f,0,25);}
    if(g_serviceRadio.pressed){xr::TriggerHaptic(g_serviceRadio.owner,.32f,0,32);cockpitclick::Click(true);}
    g_serviceTriggerMask.store(g_serviceRadio.TriggerMask(),std::memory_order_release);
    g_serviceHandMask.store(g_serviceRadio.owner>=0?1u<<g_serviceRadio.owner:0u,std::memory_order_release);
    visual.serviceOwner=g_serviceRadio.owner;visual.servicePress=g_serviceRadio.press;
    visual.serviceReturnHand=g_serviceReturnHand;
    visual.serviceReturnWeight=static_cast<float>(std::clamp((g_serviceReturnUntil-MonotonicSeconds())/.18,0.,1.));
}
