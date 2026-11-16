#include "VehicleNitro.h"
#include "VehicleNitroModel.h"
#include "VehicleNitroPcm.h"
#include "VehicleCabin.h"
#include "Driving.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace savr {GameSymbols g{};}
namespace savr::vrcam {bool IsStereoActive(){return true;}}
using namespace savr;
namespace savr::driving {
std::atomic<void*> g_nitroManagedVehicle{nullptr};std::atomic<bool> g_nitroCommand{false};bool g_nitroHookReady=true;
struct NitroSample {bool installed{},usable{},flow{};float reserve{};};
bool allowed=true,engine=true;int debugMode=0,mode=MODE_IMMERSIVE;bool third=false;
void* controlled{};
bool VrGameplayInputAllowed(){return allowed;}
int GetVehicleDebugMode(){return debugMode;}
bool EngineOn(const void*){return engine;}
bool EngineBroken(const void*){return false;}
int ModeForAppearance(int){return mode;}
bool ThirdPersonViewActive(){return third;}
void* GetControlledVehicle(){return controlled;}
int interactionResets{};
void ResetInteraction(){++interactionResets;g_nitroCommand=false;}
void* InstallVerifiedTrampoline(void*,void*,const std::uint32_t[4],const char*){return nullptr;}
#include "VehicleNitroRuntime.inl"
// COMPATIBILITY_FUNCTION
int nativeCalls{},effectStops{};bool stockButton{};
float nativeTimeStep=100.f;
void FakeNative(void* vehicle,signed char arg){
    ++nativeCalls;float& value=NativeNitro(vehicle);auto& charges=NativeNitroCharges(vehicle);
    if(arg>0){value=1;charges=arg;return;}if(arg<0){value=1;charges=0;return;}
    if(value>=1){if(stockButton&&charges>0){value=-.000001f;--charges;}}
    else if(value<0){value-=.001f*nativeTimeStep;if(value< -1.f)value=.000001f;}
    else {const float gas=*reinterpret_cast<const float*>(static_cast<const char*>(vehicle)+0x608);
        value=std::min(1.f,value+.001f*nativeTimeStep*std::max(.25f,1.f-gas));}
}
int Installed(void* v,int slot){return slot==15&&NativeNitroCharges(v)>=0?1008:-1;}
void Stop(void*){++effectStops;}
int Button(void*){return stockButton?1:0;}
alignas(16) unsigned char padMemory[512]{};
void* Pad(int){return padMemory;}
}

namespace savr::cheats {
// NITRO_CHEAT_FUNCTIONS
void* vehicle{};bool player=true,driver=true,loadSucceeds=true;
int requests{},loads{},adds{},releases{};
void* Player(int){return player?reinterpret_cast<void*>(1):nullptr;}
void* Vehicle(int,bool){return vehicle;}
bool Driver(const void*,const void*){return driver;}
void Request(int model,int flags){CHECK(model==1010&&flags==2);++requests;}
void Load(bool priorityOnly){CHECK(!priorityOnly);++loads;g.CStreaming_ms_aInfoForModel[1010*20+0x10]=loadSucceeds?1:0;}
void Release(int model){CHECK(model==1010);++releases;}
void Add(void* car,int model){
    CHECK(car==vehicle&&model==1010&&g.CStreaming_ms_aInfoForModel[model*20+0x10]==1);
    ++adds;driving::OnNitrousControl(car,10);
}
void Tests(void* car){
    vehicle=car;static const std::uint16_t bottle=1010;g_nitroBottleModel=&bottle;
    g_loadUpgradeModels=Load;g_releaseUpgradeModel=Release;
    g.FindPlayerPed=Player;g.FindPlayerVehicle=Vehicle;g.CVehicle_IsDriver=Driver;
    g.CStreaming_RequestModel=Request;static std::uint8_t streaming[1194*20]{};g.CStreaming_ms_aInfoForModel=streaming;
    static char modelInfo;g.CModelInfo_ms_modelInfoPtrs[1010]=&modelInfo;
    void* upgrade=reinterpret_cast<void*>(&Add);
    driver=false;CHECK(!InstallNitroUpgrade(upgrade));driver=true;
    player=false;CHECK(!InstallNitroUpgrade(upgrade));player=true;
    vehicle=nullptr;CHECK(!InstallNitroUpgrade(upgrade));vehicle=car;
    *reinterpret_cast<std::uint16_t*>(static_cast<char*>(car)+0x32)=522;CHECK(!InstallNitroUpgrade(upgrade));
    *reinterpret_cast<std::uint16_t*>(static_cast<char*>(car)+0x32)=400;
    CHECK(!InstallNitroUpgrade(nullptr)&&requests==0);
    loadSucceeds=false;CHECK(!InstallNitroUpgrade(upgrade)&&requests==1&&loads==1&&adds==0&&releases==1);
    loadSucceeds=true;driving::NativeNitroCharges(car)=-1;
    CHECK(InstallNitroUpgrade(upgrade)&&adds==1&&releases==2);
    CHECK(driving::NativeNitroCharges(car)==10&&driving::NativeNitro(car)==1);
    driving::NativeNitro(car)=-.6f;driving::OnNitrousControl(car,0);CHECK(driving::g_nitroBursts[car].burst.partial);
    CHECK(InstallNitroUpgrade(upgrade)&&!driving::g_nitroBursts.contains(car)&&driving::NativeNitro(car)==1);
    CHECK(driving::interactionResets==2);
    std::puts("PASS nitro cheat: driver/compatibility guards, streaming before native upgrade, load failure cleanup, install/refill clears paused burst");
}
}

nitro::Point PointAt(float u){const float a=nitro::Angle(u);return {0,-nitro::kReach*std::sin(a),nitro::kReach*std::cos(a)};}
void StateTests(){
    using namespace nitro;
    for(float release:{0.f,.5f,.79f,.8f,.9f,1.f}){
        State state;Hand hands[2]{};hands[0]={PointAt(0),0,true,true};
        state.Step(hands,.01f,true,true);hands[0].grip=1;state.Step(hands,.01f,true,true);CHECK(state.owner==0);
        hands[0].point=PointAt(release);state.Step(hands,.01f,true,true);hands[0].grip=0;state.Step(hands,0,true,true);
        CHECK(state.owner==-1);CHECK(state.on==(release>=.8f));
        for(int i=0;i<20;++i)state.Step(hands,.01f,true,true);
        CHECK(std::abs(state.position-(release>=.8f?1.f:0.f))<1e-5f);
    }
    // Held grip swept into the handle must not capture, including after reset.
    State state;Hand hands[2]{{PointAt(0),1,true,true},{PointAt(0),1,true,true}};
    state.Step(hands,.01f,true,true);CHECK(state.owner==-1);
    hands[0].grip=0;state.Step(hands,.01f,true,true);hands[0].grip=1;state.Step(hands,.01f,true,true);CHECK(state.owner==0);
    hands[0].point=PointAt(1);state.Step(hands,.01f,true,true);CHECK(state.on);
    hands[0].valid=false;state.Step(hands,.01f,true,true);CHECK(!state.on&&state.owner==-1);
    hands[0].valid=true;state.Step(hands,.01f,true,true);CHECK(state.owner==-1);
    state.Step(hands,.01f,false,true);CHECK(!state.on&&state.position==0);
    // Noise below the held threshold must not chatter, but releasing outside
    // the exact sticky zone must stop immediately, before visual return.
    hands[0]={PointAt(0),0,true,true};state.Step(hands,0,true,true);
    hands[0].grip=1;state.Step(hands,0,true,true);
    hands[0].point=PointAt(1);state.Step(hands,0,true,true);CHECK(state.on);
    hands[0].point=PointAt(.79f);state.Step(hands,0,true,true);CHECK(state.on);
    hands[0].grip=0;state.Step(hands,0,true,true);CHECK(!state.on&&state.position>.78f);
    hands[0].point=PointAt(0);for(int i=0;i<20;++i)state.Step(hands,.01f,true,true);
    hands[0].grip=1;state.Step(hands,0,true,true);hands[0].point=PointAt(1);state.Step(hands,0,true,true);
    state.Step(hands,0,true,false);CHECK(!state.on&&state.owner==-1);
    state.Step(hands,0,true,true);CHECK(!state.on&&state.owner==-1); // recharge with held grip cannot capture
    for(int fps:{30,72,90,120}){State s;s.position=.6f;s.Return(false);Hand h[2]{};for(int i=0;i<fps;++i)s.Step(h,1.f/fps,true,true);CHECK(s.position==0&&!s.on);}
    for(auto pair:std::array<std::pair<float,int>,8>{{{0.f,0},{.001f,1},{.1f,1},{.11f,2},{.6f,6},{.68f,7},{.84f,9},{1.f,10}}})CHECK(Segments(pair.first)==pair.second);
    CHECK(Distance({std::numeric_limits<float>::quiet_NaN(),0,0},0)>1e5f);
    std::puts("PASS lever: exact sticky boundary, return at varied FPS, fresh grip, one owner, tracking loss, disabled context, whole segments");
}
void RuntimeTests(){
    using namespace savr::driving;
    alignas(16) static std::array<unsigned char,110*0xc68> objects{};
    std::array<unsigned char,110> flags{};flags.fill(0x80);flags[0]=1;flags[1]=2;
    struct Pool{void* objects;void* flags;int size;int top;};Pool pool{objects.data(),flags.data(),110,0};void* poolPtr=&pool;
    std::uint32_t clock=1000;g.CPools_ms_pVehiclePool=&poolPtr;g.CTimer_m_snTimeInMilliseconds=&clock;
    g.CVehicle_GetUpgrade=Installed;g.CAutomobile_StopNitroEffect=Stop;g.CPad_GetNitroFired=Button;g.CPad_GetPad=Pad;g_originalNitrous=FakeNative;
    g.CTimer_ms_fTimeStep=&nativeTimeStep;
    static unsigned char modelInfo[256]{},modelStructure[1024]{};static void* modelTable[1194]{};
    modelTable[400]=modelInfo;void* structure=modelStructure;std::memcpy(modelInfo+0x90,&structure,sizeof(structure));
    g.CModelInfo_ms_modelInfoPtrs=modelTable;g.CVehicleModelInfo_IsUpgradeAvailable=[](void*,int slot){return slot==15;};
    void* car=objects.data();void* npc=objects.data()+0xc68;controlled=car;
    for(void* v:{car,npc}){*reinterpret_cast<std::uint16_t*>(static_cast<char*>(v)+0x32)=400;*reinterpret_cast<void**>(static_cast<char*>(v)+0x20)=reinterpret_cast<void*>(1);NativeNitro(v)=1;NativeNitroCharges(v)=5;}
    g_nitroManagedVehicle=car;stockButton=true;g_nitroCommand=false;
    OnNitrousControl(car,0);CHECK(NativeNitro(car)==1&&NativeNitroCharges(car)==5); // stock button cannot bypass OFF
    allowed=false;g_nitroManagedVehicle=nullptr;OnNitrousControl(car,0);CHECK(NativeNitro(car)==1&&NativeNitroCharges(car)==5); // menu with no publication still blocks stock start
    allowed=true;g_nitroManagedVehicle=car;
    mode=MODE_DEFAULT;debugMode=1;g_nitroManagedVehicle=nullptr;OnNitrousControl(car,0);
    CHECK(NativeNitro(car)==1&&NativeNitroCharges(car)==5); // calibration in DEFAULT never fires the shared button
    debugMode=0;mode=MODE_IMMERSIVE;g_nitroManagedVehicle=car;
    g_nitroCommand=true;*reinterpret_cast<std::uint16_t*>(padMemory+0x110)=1;OnNitrousControl(car,0);
    CHECK(NativeNitro(car)==1&&NativeNitroCharges(car)==5);
    *reinterpret_cast<std::uint16_t*>(padMemory+0x110)=0;engine=false;OnNitrousControl(car,0);
    CHECK(NativeNitro(car)==1&&NativeNitroCharges(car)==5);engine=true;g_nitroCommand=false;
    OnNitrousControl(npc,0);CHECK(NativeNitro(npc)<0&&NativeNitroCharges(npc)==4); // NPC untouched
    stockButton=false;g_nitroCommand=true;OnNitrousControl(car,0);CHECK(NativeNitro(car)<0&&NativeNitroCharges(car)==4);
    const float remaining=1+NativeNitro(car);g_nitroCommand=false;OnNitrousControl(car,0);
    CHECK(NativeNitro(car)>0);CHECK(ReadNitroSample(car).reserve>remaining);
    for(int i=0;i<30;++i)OnNitrousControl(car,0);
    CHECK(ReadNitroSample(car).reserve==1.f&&NativeNitroCharges(car)==4); // partial OFF now recovers fully
    g_nitroCommand=true;OnNitrousControl(car,0);CHECK(NativeNitro(car)<0&&NativeNitroCharges(car)==4);
    g_nitroCommand=false;OnNitrousControl(car,0);const float paused=ReadNitroSample(car).reserve;
    g_nitroManagedVehicle=nullptr;controlled=npc;NativeNitro(car)=1; // retail parked ProcessControl may reset its timer
    OnNitrousControl(car,0);CHECK(ReadNitroSample(car).reserve>=paused); // cached remainder plus native recovery
    controlled=car;mode=MODE_DEFAULT;stockButton=true;OnNitrousControl(car,0);CHECK(NativeNitro(car)<0&&NativeNitroCharges(car)==4);
    mode=MODE_IMMERSIVE;stockButton=false;g_nitroManagedVehicle=car;g_nitroCommand=true;
    for(int i=0;i<12;++i)OnNitrousControl(car,0);
    CHECK(NativeNitro(car)>=0&&g_nitroBursts[car].burst.spent);
    for(int i=0;i<20;++i)OnNitrousControl(car,0);
    CHECK(NativeNitro(car)==1&&NativeNitroCharges(car)==4); // held ON cannot restart after exhaustion
    g_nitroCommand=false;OnNitrousControl(car,0);g_nitroCommand=true;OnNitrousControl(car,0);CHECK(NativeNitroCharges(car)==3);
    allowed=false;OnNitrousControl(car,0);CHECK(NativeNitro(car)>0&&!ReadNitroSample(car).flow);allowed=true;
    const float saved=ReadNitroSample(car).reserve;
    OnNitrousControl(car,5);CHECK(!g_nitroBursts.contains(car)&&NativeNitroCharges(car)==5&&NativeNitro(car)==1); // actual installation remains retail
    CHECK(saved<1);
    OnNitrousControl(car,0);g_nitroCommand=false;OnNitrousControl(car,0);CHECK(g_nitroBursts[car].burst.partial);
    ++flags[0];NativeNitro(car)=1;NativeNitroCharges(car)=2;OnNitrousControl(car,0);CHECK(!g_nitroBursts[car].burst.partial&&ReadNitroSample(car).reserve==1);
    g_nitroCommand=true;OnNitrousControl(car,0);g_nitroCommand=false;OnNitrousControl(car,0);CHECK(g_nitroBursts[car].burst.partial);
    clock=100;NativeNitro(car)=1;NativeNitroCharges(car)=5;OnNitrousControl(car,0);
    CHECK(!g_nitroBursts[car].burst.partial&&ReadNitroSample(car).reserve==1); // loaded game uses its own state
    for(float recovered:{.099f,.1f,.11f,.25f,.68f}){
        g_nitroBursts.erase(car);NativeNitro(car)=recovered;NativeNitroCharges(car)=5;g_nitroCommand=true;
        CHECK(ReadNitroSample(car).usable==(recovered>.1f));
        OnNitrousControl(car,0);
        if(recovered<=.1f){CHECK(NativeNitro(car)>0&&NativeNitroCharges(car)==5);}
        else {
            CHECK(NativeNitro(car)<0&&NativeNitroCharges(car)==4);
            CHECK(std::abs(ReadNitroSample(car).reserve-(recovered-.1f))<1e-5f); // no free full charge
            g_nitroCommand=false;OnNitrousControl(car,0);const float reserve=ReadNitroSample(car).reserve;
            CHECK(g_nitroBursts[car].burst.partial&&reserve<=recovered+.00001f);
            CHECK(ReadNitroSample(car).usable);
            g_nitroCommand=true;OnNitrousControl(car,0);CHECK(NativeNitroCharges(car)==4);
        }
    }
    g_nitroCommand=false;g_nitroBursts.erase(car);NativeNitro(car)=.3f;NativeNitroCharges(car)=5;
    OnNitrousControl(car,0);CHECK(std::abs(NativeNitro(car)-.4f)<1e-5f&&NativeNitroCharges(car)==5);
    // Pause at 60% and exercise the production adapter against the retail
    // formula, at three throttle positions. No plateau at 60% or above.
    nativeTimeStep=1.f;stockButton=true;
    for(float gas:{0.f,.5f,1.f}){
        g_nitroBursts.erase(car);NativeNitro(car)=-.4f;NativeNitroCharges(car)=4;
        *reinterpret_cast<float*>(static_cast<char*>(car)+0x608)=gas;
        float expected=.6f;g_nitroCommand=false;const int callsBeforeRecovery=nativeCalls;
        for(int i=0;i<150;++i){
            OnNitrousControl(car,0);expected+=.001f*std::max(.25f,1.f-gas);
            CHECK(std::abs(ReadNitroSample(car).reserve-expected)<.00001f);
            CHECK(!ReadNitroSample(car).flow&&NativeNitroCharges(car)==4);
        }
        const float threeSeconds=gas==0.f?.75f:(gas==.5f?.675f:.6375f);
        CHECK(std::abs(ReadNitroSample(car).reserve-threeSeconds)<.00002f);
        CHECK(nativeCalls==callsBeforeRecovery); // no allocate/kill loop through retail exhaust FX
        for(int i=0;i<2000;++i)OnNitrousControl(car,0);
        CHECK(ReadNitroSample(car).reserve==1.f&&NativeNitro(car)==1.f&&NativeNitroCharges(car)==4);
        const int callsAtFull=nativeCalls;
        for(int i=0;i<100;++i)OnNitrousControl(car,0);
        CHECK(nativeCalls==callsAtFull&&!ReadNitroSample(car).flow); // held stock button cannot restart full OFF
        g_nitroCommand=true;OnNitrousControl(car,0);
        CHECK(NativeNitro(car)<0&&NativeNitroCharges(car)==4&&ReadNitroSample(car).reserve>.998f);
    }
    nativeTimeStep=100.f;stockButton=false;g_nitroCommand=false;
    *reinterpret_cast<float*>(static_cast<char*>(car)+0x608)=0.f;
    cheats::Tests(car);
    std::puts("PASS native adapter: partial OFF recovers 60->100% at stock throttle-dependent rates; full OFF never autostarts; resume keeps charge credit; >10% restart, NPC and pool reuse preserved");
}
void FeedbackTests(){
    for(int fps:{30,72,90,120}){
        nitro::Feel feel;nitro::State before,after;after.owner=0;
        CHECK(feel.Step(before,after,1.f/fps,true).hand==0);before=after;
        for(int i=0;i<fps;++i)CHECK(feel.Step(before,after,1.f/fps,true).hand<0); // no idle buzz
        int moving=0;
        for(int i=0;i<fps/2;++i){before=after;after.position+=.6f/fps;
            const auto pulse=feel.Step(before,after,1.f/fps,true);
            if(pulse.hand>=0){++moving;CHECK(pulse.amplitude<=.14f&&!pulse.click);}}
        CHECK(moving>5);
        before=after;after.on=true;after.position=.85f;
        const auto detent=feel.Step(before,after,1.f/fps,true);CHECK(detent.click&&detent.amplitude>=.6f&&detent.duration>=40);
        before=after;after.position=.9f;CHECK(feel.Step(before,after,.01f,true).hand<0); // weak pulse cannot replace the click
        before=after;after.on=false;after.owner=-1;after.position=.79f;
        const auto release=feel.Step(before,after,.1f,true);CHECK(release.click&&release.hand==0); // OFF on release still clicks
        CHECK(feel.Step(before,after,.01f,false).hand<0);
        nitro::SpeedEffect effect;
        for(int i=0;i<fps;++i)effect.Step(true,1.f/fps,true);
        CHECK(effect.amount>.99f&&effect.amount<=1.f);
        for(int i=0;i<fps;++i)effect.Step(false,1.f/fps,true);
        CHECK(effect.amount==0);effect.Step(true,.1f,true);CHECK(effect.Step(true,.01f,false)==0);
    }
    const auto pcm=nitroaudio::MechanicalClickPcm();double energy=0;int peak=0;
    for(auto s:pcm){energy+=static_cast<double>(s)*s;peak=std::max(peak,std::abs(static_cast<int>(s)));}
    CHECK(pcm.front()==0&&pcm.back()==0&&peak>8000&&peak<32767&&energy/pcm.size()>1000000);
    std::puts("PASS feedback: motion-only weak resistance, strong ON/OFF detents including release, no pulse overwrite, FPS-independent fade, audible bounded click PCM");
}
void CompatibilityTests(){
    alignas(16) unsigned char info[256]{},structure[1024]{};void* table[612]{};
    g.CModelInfo_ms_modelInfoPtrs=table;
    g.CModelInfo_IsCarModel=[](int model){return model==400;};
    int calls=0;static int* counter=&calls;
    g.CVehicleModelInfo_IsUpgradeAvailable=[](void* model,int slot){++*counter;CHECK(slot==15);void* p{};std::memcpy(&p,static_cast<char*>(model)+0x90,sizeof(p));return *reinterpret_cast<int*>(static_cast<char*>(p)+0x2b0)>=0;};
    CHECK(!driving::NitroCompatible(399)&&!driving::NitroCompatible(612)&&!driving::NitroCompatible(400)&&calls==0);
    table[400]=info;CHECK(!driving::NitroCompatible(400)&&calls==0);
    void* p=structure;std::memcpy(info+0x90,&p,sizeof(p));*reinterpret_cast<int*>(structure+0x2b0)=-1;
    CHECK(!driving::NitroCompatible(400)&&calls==1);*reinterpret_cast<int*>(structure+0x2b0)=0;
    CHECK(driving::NitroCompatible(400)&&calls==2); // no vehicle instance or installed kit required
    std::puts("PASS model compatibility: invalid/nonresident models rejected, slot 15 native ABI, absent kit does not hide debug calibration");
}
int main(int argc,char** argv){
    CHECK(argc==2);StateTests();FeedbackTests();CompatibilityTests();RuntimeTests();
    std::ifstream file(argv[1],std::ios::binary);std::vector<char> data((std::istreambuf_iterator<char>(file)),{});
    nitro::Model model;CHECK(model.Load(data.data(),data.size()));CHECK(model.indices.size()/3==1376);
    auto corrupt=data;corrupt[0]=0;CHECK(!model.Load(corrupt.data(),corrupt.size()));CHECK(!model.Load(data.data(),data.size()-1));
    std::puts("PASS runtime model: approved 1376 triangles; bad magic/truncated payload rejected");
}
