// The horn edge is overridden ONLY while the original HydraulicControl runs.
// Normal horn behaviour and all other vehicles/call sites keep their original ABI.
using HydraulicControlFn=void(*)(void*);
using HydraulicHornFn=bool(*)(void*);
HydraulicControlFn g_originalHydraulicControl{};
HydraulicHornFn g_originalHydraulicHorn{};
using HydraulicBeatFn=void(*)(void*,void*);
HydraulicBeatFn g_originalHydraulicBeat{};
void OnHydraulicBeat(void* hardware,void* output){
    if(!g_originalHydraulicBeat)return;g_originalHydraulicBeat(hardware,output);
    if(!hardware||!hydraulics::DanceRunning())return;
    void* internal=static_cast<char*>(hardware)+0xce8;
    hydrocontrol::SimplifyBeatWindow(internal);
    if(output!=internal)hydrocontrol::SimplifyBeatWindow(output);
}
thread_local bool g_hydraulicHornScope{};
thread_local bool g_hydraulicHornDelivered{};
thread_local void* g_hydraulicControlVehicle{};
bool OnHydraulicHorn(void* pad){
    if(g_hydraulicHornScope){
        if(g_hydraulicHornDelivered||!g_hydroToggle.load(std::memory_order_acquire))return false;
        g_hydraulicHornDelivered=true;return true;
    }
    return g_originalHydraulicHorn?g_originalHydraulicHorn(pad):false;
}
void OnHydraulicControl(void* vehicle){
    if(!g_originalHydraulicControl||!vehicle)return;
    const bool managed=g_hydroManagedVehicle.load(std::memory_order_acquire)==vehicle;
    const bool current=GetControlledVehicle()==vehicle;
    const int model=*reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(vehicle)+0x32);
    const bool physical=g_hydroHooksReady&&current&&vrcam::IsStereoActive()&&ModeForAppearance(1)==MODE_IMMERSIVE&&
        !ThirdPersonViewActive()&&HydraulicsCompatible(model)&&HydroEquipment(vehicle);
    if(physical&&(GetVehicleDebugMode()!=0||!VrGameplayInputAllowed())){g_hydroToggle.store(false);return;}
    const bool saved=g_hydraulicHornScope,savedDelivered=g_hydraulicHornDelivered;
    g_hydraulicHornScope=managed||physical;g_hydraulicHornDelivered=false;
    const auto high=[&]{return *reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(vehicle)+0xa88)>=500;};
    const bool before=high();void* savedVehicle=g_hydraulicControlVehicle;
    g_hydraulicControlVehicle=vehicle;g_originalHydraulicControl(vehicle);g_hydraulicControlVehicle=savedVehicle;
    // A pad query is not an acknowledgement. Keep the event until the
    // original hydraulic routine actually changes LOW/HIGH.
    if(g_hydraulicHornScope&&g_hydraulicHornDelivered&&high()!=before)g_hydroToggle.store(false,std::memory_order_release);
    g_hydraulicHornScope=saved;g_hydraulicHornDelivered=savedDelivered;
}
void InstallHydraulicsHooks(){
    if(!g.CAutomobile_HydraulicControl||!g.CPad_HornJustDown||!g.CAEAudioHardware_GetBeatInfo)return;
    constexpr std::uint32_t beat[4]={0xa9bd7bfd,0xf9000bf5,0xa9024ff4,0x910003fd};
    constexpr std::uint32_t horn[4]={0xa9be7bfd,0xf9000bf3,0x910003fd,0x79422008};
    constexpr std::uint32_t control[4]={0xd10403ff,0x6d063bef,0x6d0733ed,0x6d082beb};
    g_originalHydraulicHorn=reinterpret_cast<HydraulicHornFn>(InstallVerifiedTrampoline(
        reinterpret_cast<void*>(g.CPad_HornJustDown),reinterpret_cast<void*>(&OnHydraulicHorn),horn,"CPad::HornJustDown (hydraulics scope)"));
    if(!g_originalHydraulicHorn)return;
    g_originalHydraulicControl=reinterpret_cast<HydraulicControlFn>(InstallVerifiedTrampoline(
        reinterpret_cast<void*>(g.CAutomobile_HydraulicControl),reinterpret_cast<void*>(&OnHydraulicControl),control,"CAutomobile::HydraulicControl"));
    g_originalHydraulicBeat=reinterpret_cast<HydraulicBeatFn>(InstallVerifiedTrampoline(
        reinterpret_cast<void*>(g.CAEAudioHardware_GetBeatInfo),reinterpret_cast<void*>(&OnHydraulicBeat),beat,"CAEAudioHardware::GetBeatInfo (four-way LOWGAME)"));
    g_hydroHooksReady=g_originalHydraulicControl&&g_originalHydraulicBeat;
}
