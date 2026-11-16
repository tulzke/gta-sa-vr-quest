// Included in Driving.cpp after the verified trampoline installer.
using NitroControlFn=void(*)(void*,signed char);
NitroControlFn g_originalNitrous{};
struct NitroEntry {tracking::SeatKey key{};nitro::Burst burst;};
std::map<void*,NitroEntry> g_nitroBursts;
std::uint32_t g_nitroClock{};
float& NativeNitro(void* vehicle){return *reinterpret_cast<float*>(static_cast<char*>(vehicle)+0xac0);}
signed char& NativeNitroCharges(void* vehicle){return *reinterpret_cast<signed char*>(static_cast<char*>(vehicle)+0x5ee);}
NitroEntry* NitroEntryFor(void* vehicle,bool create=false){
    const auto now=g.CTimer_m_snTimeInMilliseconds?*g.CTimer_m_snTimeInMilliseconds:g_nitroClock;
    if(now<g_nitroClock)g_nitroBursts.clear();g_nitroClock=now;
    const int model=*reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(vehicle)+0x32);
    const auto key=vehiclecabin::SeatKey(g,vehicle,nullptr,model,-1);
    if(!key.pool)return nullptr;
    auto it=g_nitroBursts.find(vehicle);
    if(it==g_nitroBursts.end()){
        if(!create)return nullptr;
        if(g_nitroBursts.size()>=110)g_nitroBursts.clear();
        it=g_nitroBursts.emplace(vehicle,NitroEntry{key,{}}).first;
    }
    if(!(it->second.key==key))it->second={key,{}};
    return &it->second;
}
NitroSample ReadNitroSample(void* vehicle){
    NitroSample out{};
    if(!vehicle||!g.CVehicle_GetUpgrade||!*reinterpret_cast<void**>(static_cast<char*>(vehicle)+0x20))return out;
    out.installed=g.CVehicle_GetUpgrade(vehicle,15)!=-1;
    if(!out.installed)return out;
    const float value=NativeNitro(vehicle);if(!std::isfinite(value)||value< -1.01f||value>1.01f)return out;
    auto* entry=NitroEntryFor(vehicle);const nitro::Burst empty{};
    const auto& burst=entry?entry->burst:empty;
    out.reserve=burst.Reserve(value);
    out.usable=g_nitroHookReady&&burst.Usable(value,NativeNitroCharges(vehicle));
    out.flow=value<0.f&&!burst.partial;
    return out;
}
void OnNitrousControl(void* vehicle,signed char argument){
    if(!g_originalNitrous||!vehicle)return;
    if(argument!=0){g_nitroBursts.erase(vehicle);g_originalNitrous(vehicle,argument);return;}
    const bool current=GetControlledVehicle()==vehicle;
    const bool published=g_nitroManagedVehicle.load(std::memory_order_acquire)==vehicle;
    const int model=*reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(vehicle)+0x32);
    // Menus/cutscenes clear interaction publication. Still block the retail
    // shared button in the physical cockpit instead of letting it start nitro.
    const bool physical=current&&vrcam::IsStereoActive()&&ModeForAppearance(1)==MODE_IMMERSIVE&&!ThirdPersonViewActive()&&NitroCompatible(model);
    const bool managed=published||physical||(current&&(GetVehicleDebugMode()!=0||!VrGameplayInputAllowed()));
    auto* entry=NitroEntryFor(vehicle,managed);
    if(!entry){g_originalNitrous(vehicle,argument);return;}
    float& value=NativeNitro(vehicle);auto& burst=entry->burst;
    if(!std::isfinite(value)||value< -1.01f||value>1.01f){burst.Clear();g_originalNitrous(vehicle,argument);return;}
    void* pad=g.CPad_GetPad?g.CPad_GetPad(0):nullptr;
    const bool control=VrGameplayInputAllowed()&&GetVehicleDebugMode()==0&&EngineOn(vehicle)&&!EngineBroken(vehicle)&&
        pad&&*reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(pad)+0x110)==0;
    bool requested=published&&current&&control&&g_nitroCommand.load(std::memory_order_acquire);
    // Resume a parked partial charge through the normal button in DEFAULT.
    if(!managed&&burst.partial&&control&&GetControlledVehicle()==vehicle&&
       (ModeForAppearance(1)!=MODE_IMMERSIVE||ThirdPersonViewActive())){
        requested=pad&&g.CPad_GetNitroFired&&g.CPad_GetNitroFired(pad)!=0;
    }
    if(!requested)burst.spent=false;
    if(requested&&burst.Usable(value,NativeNitroCharges(vehicle))){
        if(burst.partial)burst.Restore(value);
        else if(value>=0.f){
            // Start with only the reserve actually recovered (>10%). Keep the
            // retail charge cost from 6AA49C..6AA4B8, including unlimited kits.
            // Do not call NitrousControl with a positive argument: it refills.
            auto& charges=NativeNitroCharges(vehicle);
            if(charges>0&&charges<=100)--charges;
            value=std::min(-.000001f,value-1.f);
        }
        const float before=value;
        g_originalNitrous(vehicle,0);burst.After(before,value);return;
    }
    if(value<0.f)burst.Pause(value);
    if(burst.partial){
        value=std::max(.000001f,1.f+burst.progress);
        // Exact retail recovery formula at 6AA558..6AA59C, without its
        // DoNitroEffect tail: calling then killing FX every OFF tick would
        // continually allocate exhaust emitters. TimeStep is in 1/50s units.
        const float step=g.CTimer_ms_fTimeStep?*g.CTimer_ms_fTimeStep:0.f;
        const float gas=*reinterpret_cast<const float*>(static_cast<const char*>(vehicle)+0x608);
        if(value<1.f&&std::isfinite(step)&&step>0&&std::isfinite(gas))
            value=std::min(1.f,std::fma(step*.001f,std::max(.25f,1.f-gas),value));
        burst.AfterRecovery(value);
        if(g.CAutomobile_StopNitroEffect)g.CAutomobile_StopNitroEffect(vehicle);
        return;
    }
    if(!managed){g_originalNitrous(vehicle,0);return;}
    // Cooldown uses the original rate/throttle dependence; ready OFF skips
    // only the vanilla button-start branch. No other pad functions are hooked.
    if(value<1.f)g_originalNitrous(vehicle,0);
    else if(g.CAutomobile_StopNitroEffect)g.CAutomobile_StopNitroEffect(vehicle);
}
void InstallNitroHook(){
    if(!g.CAutomobile_StopNitroEffect||!g.CVehicle_GetUpgrade||!g.CVehicleModelInfo_IsUpgradeAvailable)return;
    constexpr std::uint32_t expected[4]={0xa9be7bfdu,0xa9014ff4u,0x910003fdu,0x39416808u};
    g_originalNitrous=reinterpret_cast<NitroControlFn>(InstallVerifiedTrampoline(
        reinterpret_cast<void*>(g.CAutomobile_NitrousControl),reinterpret_cast<void*>(&OnNitrousControl),expected,"CAutomobile::NitrousControl"));
    g_nitroHookReady=g_originalNitrous!=nullptr;
}
