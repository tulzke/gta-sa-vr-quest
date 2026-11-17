// Game-thread only. Only cannon slots submitted by the player's Firetruck are
// registered; other native cannons retain their original update path.
using WaterCannonUpdate=void(*)(void*,std::int16_t);
WaterCannonUpdate g_origWaterCannonUpdate{};
unsigned char* g_waterCannonPool{};
watermonitor::Sampling g_waterSampling[3]{};
int WaterCannonSlot(const void* cannon){
 const auto p=reinterpret_cast<std::uintptr_t>(cannon),base=reinterpret_cast<std::uintptr_t>(g_waterCannonPool);
 if(!base||p<base||p-base>=watermonitor::CannonStride*3||(p-base)%watermonitor::CannonStride)return -1;
 return static_cast<int>((p-base)/watermonitor::CannonStride);
}
void SubmitWaterInput(std::uint32_t id,GameSymbols::Vec3* p,GameSymbols::Vec3* v){
 g_waterUpdateOne(id,p,v);
 if(!g_origWaterCannonUpdate||!g_waterCannonPool)return;
 for(int i=0;i<3;++i){auto* slot=g_waterCannonPool+i*watermonitor::CannonStride;std::uint32_t identity{},born{};
  std::memcpy(&identity,slot,4);std::memcpy(&born,slot+8,4);if(identity!=id)continue;
  g_waterSampling[i].Input(id,born,*g.CTimer_m_FrameCounter,{p->x,p->y,p->z},{v->x,v->y,v->z});break;
 }
}
bool WaterInputStillHeld(const watermonitor::Sampling& state){
 void* vehicle=GetControlledVehicle();
 if(!vehicle||ControlledVehicleModel()!=407||static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(vehicle))!=state.id||
    !VrGameplayInputAllowed()||GetVehicleDebugMode()!=0||!g.FindPlayerPed||!g.CVehicle_IsDriver||!g.CVehicle_IsDriver(vehicle,g.FindPlayerPed(-1)))return false;
 void* pad=g.CPad_GetPad?g.CPad_GetPad(0):nullptr;
 if(!pad||*reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(pad)+0x110)||*(static_cast<const unsigned char*>(pad)+0x12f))return false;
 xr::InputState input{};xr::GetInput(input);if(!input.b)return false;
 if(ModeForAppearance(1)!=MODE_IMMERSIVE)return true;
 std::lock_guard<std::mutex> lock(g_stateMutex);
 return CabinVisualContext()&&g_waterMonitor.owner>=0&&g_waterMonitor.fire&&g_waterMonitor.seat==ServiceSeat(vehicle,407);
}
void OnWaterCannonUpdate(void* cannon,std::int16_t index){
 const int slot=WaterCannonSlot(cannon);
 if(slot<0||!g_waterSampling[slot].Matches(cannon)){g_origWaterCannonUpdate(cannon,index);return;}
 auto& sample=g_waterSampling[slot];const auto frame=*g.CTimer_m_FrameCounter;
 if(sample.updated&&sample.updatedFrame==frame)return;
 sample.updated=true;sample.updatedFrame=frame;
 const auto now=*g.CTimer_m_snTimeInMilliseconds;
 // Preserve retail's initial 150ms startup. Afterwards only ring advancement
 // is time-based; native movement, extinguishing and PushPeds run once/frame.
 if(static_cast<std::uint32_t>(now-sample.born)<=150){g_origWaterCannonUpdate(cannon,index);return;}
 sample.Prepare(cannon,*g.CTimer_ms_fTimeStep,frame,WaterInputStillHeld(sample));
 auto* birth=static_cast<unsigned char*>(cannon)+8;std::uint32_t saved{};std::memcpy(&saved,birth,4);
 // Suppress the old one-slot-per-frame branch without altering global timers.
 std::memcpy(birth,&now,4);g_origWaterCannonUpdate(cannon,index);std::memcpy(birth,&saved,4);
}
