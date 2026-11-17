// Included in Driving.cpp's private namespace. State is protected by g_stateMutex.
bool BuildWaterMonitor(WheelVisualState& visual,void* vehicle){
 auto& v=visual.monitor;v.visible=false;
 if(!g_waterRetailReady||!vehicle||visual.modelId!=407||ThirdPersonViewActive()||
    (ModeForAppearance(1)!=MODE_IMMERSIVE&&GetVehicleDebugMode()==0)||
    !g.FindPlayerPed||!g.CVehicle_IsDriver||!g.CVehicle_IsDriver(vehicle,g.FindPlayerPed(-1)))return false;
 V3 r{},f{},u{},p{};if(!ReadIgnitionFrame(vehicle,false,&r,&f,&u,&p))return false;
 float pose[12]{};bool custom=false;
 {std::lock_guard<std::mutex> lock(g_modelMutex);const auto it=g_modelCalibration.find(407);
  if(it!=g_modelCalibration.end()&&it->second.monitorPoseSet){std::copy_n(it->second.monitorPose,12,pose);custom=true;}}
 V3 local{};
 if(custom){local={pose[0],pose[1],pose[2]};const V3 rr=r*pose[3]+f*pose[4]+u*pose[5],ff=r*pose[6]+f*pose[7]+u*pose[8],uu=r*pose[9]+f*pose[10]+u*pose[11];r=rr;f=ff;u=uu;}
 else{int mirror[12]{};GetRearMirrorCalibration(407,mirror);local={mirror[0]*.01f+.25f,mirror[1]*.01f,mirror[2]*.01f-.10f};}
 // Position remains car-local even when the authored orientation is rotated.
 V3 cr{},cf{},cu{},cp{};if(!ReadIgnitionFrame(vehicle,false,&cr,&cf,&cu,&cp))return false;
 const V3 c=cp+cr*local.x+cf*local.y+cu*local.z;
 V3 tc{},tr{},tf{},tu{};
 if(!WorldPointToTracking(c,&tc)||!WorldPointToTracking(c+r,&tr)||!WorldPointToTracking(c+f,&tf)||!WorldPointToTracking(c+u,&tu))return false;
 const auto vec=[](V3 a){return watermonitor::Vec{a.x,a.y,a.z};};
 v.dock={vec(tc),vec(Normalized(tr-tc)),vec(Normalized(tf-tc)),vec(Normalized(tu-tc))};
 v.visible=true;v.vehicle=vehicle;return true;
}
void PublishWaterMonitor(WheelVisualState& visual,double now){
 auto& v=visual.monitor;if(!v.visible)return;
 if(ReadWaterNozzle(v.vehicle,v.nozzleWorld))v.actualWorld=v.nozzleWorld.forward;
 else v.actualWorld=g_waterActualWorld;
 const bool preview=g_waterCameraPreview.load(std::memory_order_acquire)&&g_menuPreview.load(std::memory_order_acquire)&&GetVehicleDebugMode()==0;
 {std::lock_guard<std::mutex> lock(g_modelMutex);auto it=g_modelCalibration.find(407);
  const auto settings=it==g_modelCalibration.end()?watermonitor::DefaultCamera:it->second.monitorCamera;
  std::copy(settings.begin(),settings.end(),v.cameraSettings);}
 v.cameraWorld=v.nozzleWorld.Point({v.cameraSettings[0]*.01f,v.cameraSettings[1]*.01f,v.cameraSettings[2]*.01f});
 v.owner=g_waterMonitor.owner;v.fire=g_waterMonitor.fire;v.press=g_waterMonitor.press;v.screen=g_waterMonitor.Display(now);v.cameraActive=g_waterMonitor.CameraActive(now);
 if(v.owner>=0)v.body=g_waterMonitor.held;
 else if(g_waterMonitor.returnUntil>now)v.body=watermonitor::Blend(v.dock,g_waterMonitor.released,static_cast<float>((g_waterMonitor.returnUntil-now)/watermonitor::ReturnSeconds));
 else v.body=v.dock;
 if(preview){v.cameraActive=true;v.screen={1,1,1};}
 v.desiredTracking=v.owner>=0?v.body.forward:v.dock.forward;
 g_waterHandMask.store(v.owner>=0?1u<<v.owner:0u,std::memory_order_release);
 g_waterConsumesB.store(g_waterMonitor.ConsumesFire(),std::memory_order_release);
}
void UpdateWaterMonitor(WheelVisualState& visual,void* vehicle,const xr::InputState& input,const xr::HandPose poses[2],unsigned occupied,bool preview=false){
 const double now=MonotonicSeconds();const bool geometry=BuildWaterMonitor(visual,vehicle);
 if(const auto* tracking=xr::ScopedTrackingFrame::Current();tracking&&tracking->epoch!=g_waterTrackingEpoch){ResetWaterMonitor();g_waterTrackingEpoch=tracking->epoch;}
 watermonitor::Hand hands[2]{};const auto& dock=visual.monitor.dock;
 const auto target=(g_waterMonitor.owner>=0?g_waterMonitor.held:dock).Point({0,.003f,.065f});
 for(int h=0;h<2;++h){float q[4]{};const bool valid=poses[h].valid&&NormalizeQuaternion(poses[h].gripOri,q)&&
   (!poses[h].aimValid||NormalizeQuaternion(poses[h].aimOri,q));
  const auto frame=valid?watermonitor::HeldFrame(poses[h],h):watermonitor::Frame{};
  const watermonitor::Vec p{poses[h].gripPos[0],poses[h].gripPos[1],poses[h].gripPos[2]};
  hands[h]={frame,input.grip[h],valid&&watermonitor::finite(p),(occupied&(1u<<h))==0,geometry&&watermonitor::dot(p-target,p-target)<.0144f};}
 void* pad=g.CPad_GetPad?g.CPad_GetPad(0):nullptr;
 const bool enabled=geometry&&visual.active&&!preview&&!g_inputBlocked.load()&&GetVehicleDebugMode()==0&&pad&&
  !*reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(pad)+0x110);
 const int old=g_waterMonitor.owner;
 g_waterMonitor.Step(enabled,ServiceSeat(vehicle,407),hands,input.b,now);
 if(old<0&&g_waterMonitor.owner>=0)xr::TriggerHaptic(g_waterMonitor.owner,.24f,0,25);
 PublishWaterMonitor(visual,now);
}
