#include "WaterMonitorSampling.h"
#include <mutex>
#include <cstdio>
#include <cstdlib>
using namespace savr;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(3);}}while(0)
namespace adapter {
struct GameSymbols {struct Vec3 {float x,y,z;};};
namespace xr {struct InputState {bool b;};bool held=true;void GetInput(InputState& out){out.b=held;}}
std::uint32_t frame=1,now=1000;float step=50.f/72;
unsigned char vehicle[0xc68]{},pad[512]{};
void* GetControlledVehicle(){return vehicle;}
int ControlledVehicleModel(){return 407;}
bool gameplay=true;bool VrGameplayInputAllowed(){return gameplay;}
int GetVehicleDebugMode(){return 0;}
int drivingMode=0;constexpr int MODE_IMMERSIVE=1;int ModeForAppearance(int){return drivingMode;}
bool CabinVisualContext(){return true;}
std::mutex g_stateMutex;
watermonitor::State g_waterMonitor;
tracking::SeatKey ServiceSeat(void*,int){tracking::SeatKey key{};key.vehicle=vehicle;key.model=407;return key;}
struct Symbols {
 std::uint32_t* CTimer_m_FrameCounter=&frame;std::uint32_t* CTimer_m_snTimeInMilliseconds=&now;float* CTimer_ms_fTimeStep=&step;
 void*(*FindPlayerPed)(int)=[](int)->void*{return pad;};
 bool(*CVehicle_IsDriver)(void*,void*)=[](void*,void*){return true;};
 void*(*CPad_GetPad)(int)=[](int)->void*{return pad;};
} g;
std::array<unsigned char,watermonitor::CannonStride*3> pool{};
int inputCalls{},updateCalls{},nativeAdvances{};bool sawSuppression{};
void NativeInput(std::uint32_t id,GameSymbols::Vec3* p,GameSymbols::Vec3* v){
 ++inputCalls;auto* bytes=pool.data();std::memcpy(bytes,&id,4);std::int16_t current{};std::memcpy(&current,bytes+4,2);
 std::memcpy(bytes+0xc+current*12,p,12);std::memcpy(bytes+0x18c+current*12,v,12);bytes[0x30c+current]=1;
}
auto g_waterUpdateOne=&NativeInput;
#include "WaterMonitorSampling.inl"
void NativeUpdate(void* memory,std::int16_t){
 ++updateCalls;auto* bytes=static_cast<unsigned char*>(memory);std::uint32_t birth{};std::memcpy(&birth,bytes+8,4);
 if(birth==now)sawSuppression=true;
 if(now>birth+150){++nativeAdvances;std::int16_t current{};std::memcpy(&current,bytes+4,2);current=(current+1)&31;std::memcpy(bytes+4,&current,2);bytes[0x30c+current]=0;}
 for(int i=0;i<32;++i)if(bytes[0x30c+i]){watermonitor::Vec p{},v{};std::memcpy(&p,bytes+0xc+i*12,12);std::memcpy(&v,bytes+0x18c+i*12,12);
  v.z-=.004f*step;p=p+v*step;std::memcpy(bytes+0xc+i*12,&p,12);std::memcpy(bytes+0x18c+i*12,&v,12);}
}
}
int main(){using namespace adapter;
 g_waterCannonPool=pool.data();g_origWaterCannonUpdate=&NativeUpdate;
 const auto id=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(vehicle));
 GameSymbols::Vec3 p{0,0,2},v{0,.5f,.05f};
 SubmitWaterInput(id,&p,&v);CHECK(inputCalls==1);
 const auto savedStep=step;const auto savedNow=now;
 OnWaterCannonUpdate(pool.data(),0);CHECK(sawSuppression&&step==savedStep&&now==savedNow);
 std::uint32_t birth{};std::memcpy(&birth,pool.data()+8,4);CHECK(birth==0&&nativeAdvances==0);
 OnWaterCannonUpdate(pool.data(),0);CHECK(updateCalls==1); // duplicate frame is not integrated twice
 drivingMode=MODE_IMMERSIVE;g_waterMonitor.owner=1;g_waterMonitor.fire=true;g_waterMonitor.seat=ServiceSeat(vehicle,407);
 CHECK(WaterInputStillHeld(g_waterSampling[0]));g_waterMonitor.fire=false;CHECK(!WaterInputStillHeld(g_waterSampling[0]));
 g_waterMonitor.fire=true;xr::held=false;CHECK(!WaterInputStillHeld(g_waterSampling[0]));xr::held=true;
 gameplay=false;CHECK(!WaterInputStillHeld(g_waterSampling[0]));gameplay=true;
 const std::uint32_t npc=77;std::memcpy(pool.data()+watermonitor::CannonStride,&npc,4);
 OnWaterCannonUpdate(pool.data()+watermonitor::CannonStride,1);CHECK(nativeAdvances==1); // unrelated cannon unchanged
 const auto before=updateCalls;const std::uint32_t replacedBirth=now;std::memcpy(pool.data()+8,&replacedBirth,4);
 ++frame;OnWaterCannonUpdate(pool.data(),0);CHECK(updateCalls==before+1); // pool reuse/birth mismatch is native
 SubmitWaterInput(id,&p,&v);CHECK(g_waterSampling[0].born==replacedBirth&&g_waterSampling[0].phase==0);
 std::puts("water sampling adapter: native per-frame motion, scoped timestamp restore, unchanged globals, duplicate guard, fire gates and unrelated pool slots PASS");
}
