// After InstallVerifiedTrampoline. Preserve the native NPC and S.W.A.T. paths.
using WaterFireControl=void(*)(void*,void*);
using WaterUpdateOne=void(*)(std::uint32_t,GameSymbols::Vec3*,GameSymbols::Vec3*);
using WaterPointSpeed=GameSymbols::Vec3(*)(void*,GameSymbols::Vec3);
WaterFireControl g_origWaterFireControl{};
WaterUpdateOne g_waterUpdateOne{};WaterPointSpeed g_waterPointSpeed{};
void* (*g_waterFrameLtm)(void*){};
watermonitor::Servo g_waterServo{};tracking::SeatKey g_waterPhysicsSeat{};
std::uint32_t g_waterPhysicsTick=~std::uint32_t{};
bool WaterTrackingToWorld(watermonitor::Vec p,watermonitor::Vec& out){
 float a[3]{p.x,p.y,p.z},b[3]{};if(!vrcam::TrackingPointToWorld(a,b))return false;out={b[0],b[1],b[2]};return watermonitor::finite(out);
}
bool ReadWaterNozzle(void* vehicle,watermonitor::Frame& out){
 if(!vehicle||!g_waterFrameLtm)return false;
 const auto* bytes=static_cast<const char*>(vehicle);void* matrix{};std::memcpy(&matrix,bytes+0x18,8);if(!matrix)return false;
 const auto read=[&](std::size_t at){GameSymbols::Vec3 v{};std::memcpy(&v,static_cast<const char*>(matrix)+at,12);return watermonitor::Vec{v.x,v.y,v.z};};
 const watermonitor::Frame car{read(0x30),read(0),read(0x10),read(0x20)};
 auto pivot=car.Point(watermonitor::NozzlePivot);void* frame{};std::memcpy(&frame,bytes+0x8a0,8);
 if(frame)if(void* ltm=g_waterFrameLtm(frame)){GameSymbols::Vec3 p{};std::memcpy(&p,static_cast<char*>(ltm)+0x30,12);pivot={p.x,p.y,p.z};}
 float yaw{},pitch{};std::memcpy(&yaw,bytes+0xb80,4);std::memcpy(&pitch,bytes+0xb84,4);
 if(!watermonitor::finite(pivot)||!std::isfinite(yaw)||!std::isfinite(pitch))return false;
 out=watermonitor::NozzleFrame(car,pivot,yaw,pitch);return true;
}
void OnWaterFireControl(void* vehicle,void* fire){
 if(!vehicle||vehicle!=GetControlledVehicle()||ControlledVehicleModel()!=407||!g.FindPlayerPed||!g.CVehicle_IsDriver||
    !g.CVehicle_IsDriver(vehicle,g.FindPlayerPed(-1))){if(g_origWaterFireControl)g_origWaterFireControl(vehicle,fire);return;}
 const auto* bytes=static_cast<const char*>(vehicle);void* matrix{};std::memcpy(&matrix,bytes+0x18,sizeof(matrix));if(!matrix)return;
 const auto read=[&](std::size_t at){GameSymbols::Vec3 v{};std::memcpy(&v,static_cast<const char*>(matrix)+at,sizeof(v));return watermonitor::Vec{v.x,v.y,v.z};};
 const auto r=read(0),f=read(0x10),u=read(0x20),pos=read(0x30);
 if(!watermonitor::finite(r)||!watermonitor::finite(f)||!watermonitor::finite(u)||!watermonitor::finite(pos))return;
 xr::InputState input{};xr::GetInput(input);const bool immersive=ModeForAppearance(1)==MODE_IMMERSIVE;
 void* pad=g.CPad_GetPad?g.CPad_GetPad(0):nullptr;
 const bool allowed=VrGameplayInputAllowed()&&(!immersive||CabinVisualContext())&&GetVehicleDebugMode()==0&&pad&&
   !*reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(pad)+0x110)&&
   !*(static_cast<const unsigned char*>(pad)+0x12f);
 const float nativeDt=g.CTimer_ms_fTimeStep?*g.CTimer_ms_fTimeStep:0;
 if(!std::isfinite(nativeDt)||nativeDt<0)return;
 const float dt=std::clamp(nativeDt/50.f,0.f,.1f);
 bool shoot=false;watermonitor::Vec direction=f;
 {std::lock_guard<std::mutex> lock(g_stateMutex);
  const auto key=ServiceSeat(vehicle,407);
  if(!(key==g_waterPhysicsSeat)){
   g_waterPhysicsSeat=key;g_waterPhysicsTick=~std::uint32_t{};
   std::memcpy(&g_waterServo.yaw,bytes+0xb80,4);std::memcpy(&g_waterServo.pitch,bytes+0xb84,4);
   if(!std::isfinite(g_waterServo.yaw)||!std::isfinite(g_waterServo.pitch))g_waterServo={};
   g_waterServo.yawSpeed=g_waterServo.pitchSpeed=0;
  }
  float targetYaw=0,targetPitch=0;
  if(immersive&&allowed&&g_waterMonitor.owner>=0&&g_waterMonitor.seat==key){
   watermonitor::Vec a{},b{};
   if(WaterTrackingToWorld(g_waterMonitor.held.center,a)&&WaterTrackingToWorld(g_waterMonitor.held.center+g_waterMonitor.held.forward,b)){
    direction=watermonitor::unit(b-a);targetYaw=std::atan2(-watermonitor::dot(direction,r),watermonitor::dot(direction,f));
    targetPitch=std::atan2(watermonitor::dot(direction,u),std::hypot(watermonitor::dot(direction,r),watermonitor::dot(direction,f)));
    shoot=g_waterMonitor.fire;
   }
  }else if(!immersive&&allowed){
   float mx{},my{},tx{},ty{};ReadMappedGameplaySticks(input,mx,my,tx,ty);
   targetYaw=g_waterServo.yaw-(std::abs(tx)>.18f?tx*2.5f*dt:0);
   targetPitch=g_waterServo.pitch+(std::abs(ty)>.18f?ty*dt:0);shoot=input.b;
  }
  const auto tick=g.CTimer_m_FrameCounter?*g.CTimer_m_FrameCounter:0;
  if(!g.CTimer_m_FrameCounter||tick!=g_waterPhysicsTick){g_waterPhysicsTick=tick;g_waterServo.Step(targetYaw,targetPitch,dt);}
  std::memcpy(static_cast<char*>(vehicle)+0xb80,&g_waterServo.yaw,4);std::memcpy(static_cast<char*>(vehicle)+0xb84,&g_waterServo.pitch,4);
  const auto d=g_waterServo.Direction();const auto actual=r*d.x+f*d.y+u*d.z;
  g_waterActualWorld=actual;g_visual.monitor.actualWorld=actual;
  if(!immersive||g_waterMonitor.owner<0)direction=f;
  watermonitor::Vec origin=pos+f*1.5f+u*1.8f;
  void* frame{};std::memcpy(&frame,bytes+0x8a0,sizeof(frame));
  if(frame)if(void* ltm=g_waterFrameLtm(frame)){GameSymbols::Vec3 v{};std::memcpy(&v,static_cast<char*>(ltm)+0x30,sizeof(v));origin={v.x,v.y,v.z};}
  // Camera calibration is computed separately from this native emission path.
  // Camera follows desired yaw/pitch immediately; the gun and circle lag.
  if(!shoot||!allowed)return;
  auto start=origin;
  if(frame){start=start+actual*1.2f;const auto relative=start-pos;
   const auto speed=g_waterPointSpeed(vehicle,{relative.x,relative.y,relative.z});start=start+watermonitor::Vec{speed.x,speed.y,speed.z}*nativeDt;}
  GameSymbols::Vec3 carSpeed{};std::memcpy(&carSpeed,bytes+0x68,sizeof(carSpeed));
  const float noise=static_cast<float>(std::rand()&15)*.001f;
  const auto velocity=(actual+watermonitor::Vec{0,0,noise})*.5f+watermonitor::Vec{carSpeed.x,carSpeed.y,carSpeed.z*.3f};
  GameSymbols::Vec3 p{start.x,start.y,start.z},v{velocity.x,velocity.y,velocity.z};
  g_waterUpdateOne(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(vehicle)),&p,&v);
 }
}
void InstallWaterMonitorHook(){
 void* h=dlopen("libGame.so",RTLD_NOW|RTLD_NOLOAD);if(!h)return;
 const auto resolve=[&](const char* name,std::uintptr_t at){void* p=dlsym(h,name);return reinterpret_cast<std::uintptr_t>(p)==g.LoadBase+at?p:nullptr;};
 void* target=resolve("_ZN11CAutomobile16FireTruckControlEP5CFire",0x5fa414);
 g_waterUpdateOne=reinterpret_cast<WaterUpdateOne>(resolve("_ZN13CWaterCannons9UpdateOneEjP7CVectorS1_",0x5fa1d8));
 g_waterPointSpeed=reinterpret_cast<WaterPointSpeed>(resolve("_ZN9CPhysical8GetSpeedE7CVector",0x4a1408));
 g_waterFrameLtm=reinterpret_cast<decltype(g_waterFrameLtm)>(resolve("_Z13RwFrameGetLTMP7RwFrame",0x768858));
 constexpr std::uint32_t words[4]{0xd10343ff,0x6d072beb,0x6d0823e9,0xa9097bfd};
 if(g_waterUpdateOne&&g_waterPointSpeed&&g_waterFrameLtm&&g.CTimer_ms_fTimeStep)
  g_origWaterFireControl=reinterpret_cast<WaterFireControl>(InstallVerifiedTrampoline(target,reinterpret_cast<void*>(&OnWaterFireControl),words,"Firetruck water control"));
 g_waterRetailReady=g_origWaterFireControl!=nullptr;dlclose(h);
 LOGI("[water.monitor] Firetruck ABI ready=%d",g_waterRetailReady);
}
