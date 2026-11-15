"""Exercise the production CBmx wrapper, scoped samples and hook failure paths.

Optional: pass the extracted retail libGame.so to verify its ARM64 signatures.
The generated trampoline is also compared with an independently assembled .S.
"""
from pathlib import Path
import re
import runpy
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/bicycle-sway-tests'
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / 'native/src/Driving.cpp').read_text(encoding='utf8')


def function(signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


prefix = r'''
#define _CRT_SECURE_NO_WARNINGS
#include "BicycleSway.h"
#include "BicyclePedal.h"
#include <array>
#include <atomic>
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>
using namespace savr;
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#e);std::exit(3);}}while(0)
#define LOGI(...) ((void)0)
#define LOGE(...) ((void)0)
#define __builtin___clear_cache(a,b) ((void)0)
alignas(16) unsigned char bike[0xb00]{},npc[0xb00]{};
void* vehicle=bike;int pedObject{};bool isDriver=true,stereo=true;
void* FindVehicle(int,bool){return vehicle;}
void* FindPed(int){return &pedObject;}
bool IsDriver(void* v,void* p){return v==vehicle&&p==&pedObject&&isDriver;}
struct {std::uintptr_t LoadBase{};void*(*FindPlayerVehicle)(int,bool)=FindVehicle;
void*(*FindPlayerPed)(int)=FindPed;bool(*CVehicle_IsDriver)(void*,void*)=IsDriver;}g;
namespace vrcam {bool IsStereoActive(){return stereo;}bool IsCrouched(){return false;}}
namespace xr {struct InputState{float triggers[2]{};}input;void GetInput(InputState& out){out=input;}}
std::atomic<bool> g_inputBlocked{};
bool VrGameplayInputAllowed(){return stereo&&!g_inputBlocked;}
double clockTime=1.;double MonotonicSeconds(){return clockTime;}
thread_local bicyclepedal::State g_bicyclePedal;
namespace combat {bool IsBlocking(){return false;}}
namespace locomotion {bool RunEnabled(){return false;}}
bool HasPlayerVehicle(){return vehicle!=nullptr;}
int GetVehicleDebugMode(){return 0;}
void* GetControlledVehicle(){return vehicle;}
void* GetRemoteControlledVehicle(){return nullptr;}
int GetActiveVehicleAppearance(){return 2;}
constexpr int MODE_IMMERSIVE=1,BICYCLE_MOTION=0,BIKE_ACCEL_HOLD_TRIGGER=0;
int ModeForAppearance(int){return MODE_IMMERSIVE;}
bool IsBicycleModel(int id){return id==481||id==509||id==510;}
bool EngineOn(void*){return true;}
std::mutex g_stateMutex;
std::atomic<int> g_activeVehicleModel{481},g_bicycleMode{1},g_activeBikeAccelerator{0};
float g_bicyclePedalPulse{},g_bikeThrottle{};
int OriginalAccelerate(void*){return 7;}auto g_origAccelerate=OriginalAccelerate;
bool OriginalSprint(void*,int){return true;}auto g_origSprint=OriginalSprint;
bool OriginalSprintJustDown(void*){return true;}auto g_origSprintJustDown=OriginalSprintJustDown;
using BmxControlFn=void(*)(void*);
BmxControlFn g_origBmxControl{};
constexpr int PROT_READ=1,PROT_WRITE=2,PROT_EXEC=4,MAP_PRIVATE=2,MAP_ANONYMOUS=32,_SC_PAGESIZE=30;
void* const MAP_FAILED=reinterpret_cast<void*>(~std::uintptr_t{});
alignas(16) unsigned char allocation[4096]{};
bool allocationFails{},patchFails{};int allocations{},frees{},patches{};
long sysconf(int){return 4096;}
void* mmap(void*,std::size_t,int,int,int,int){++allocations;return allocationFails?MAP_FAILED:allocation;}
int munmap(void*,std::size_t){++frees;return 0;}
bool PatchAbsoluteJump(void* target,void*){
 ++patches;if(patchFails)return false;std::memset(target,0x11,16);return true;
}
float Read(void* p,std::size_t offset){float x;std::memcpy(&x,static_cast<char*>(p)+offset,4);return x;}
void Write(void* p,std::size_t offset,float x){std::memcpy(static_cast<char*>(p)+offset,&x,4);}
void Model(int id){std::uint16_t m=static_cast<std::uint16_t>(id);std::memcpy(bike+0x32,&m,2);}
'''

code = prefix + '\n'.join(function(sig) for sig in (
    'int OnGetAccelerate(', 'bool OnGetSprint(', 'bool OnSprintJustDown(',
    'void CaptureBicycleBaseLean(', 'void OnBmxProcessControl(',
    'void InstallBicycleSwayHook(')) + r'''
float physical=.37f,phase{},weight{};int branch{},baseCalls{},tailCalls{};
bool sample=true,nested{};
bool checkPedal{},heldResult{},tapResult{};int driveResult{};
void Retail(void* p){
 if(checkPedal&&p==bike){
  driveResult=OnGetAccelerate(nullptr);heldResult=OnGetSprint(nullptr,1);tapResult=OnSprintJustDown(nullptr);
  CHECK(OnSprintJustDown(nullptr)==tapResult); // polling never consumes an edge
  CHECK(!OnGetSprint(nullptr,0)); // bicycle sprint type only
 }
 ++baseCalls;
 Write(p,bicyclesway::kLeanOffset,physical);
 Write(p,0x83c,physical); // native smoothing state must keep full lean
 Write(p,0x600,.21f); // base steering survives the wrapper
 if(sample)CaptureBicycleBaseLean(p); // actual trampoline callback position
 if(nested&&p==bike){OnBmxProcessControl(npc);CHECK(bicyclesway::current->bike==bike);}
 ++tailCalls;
 if(branch==208)Write(p,bicyclesway::kFrameSwayOffset,weight*std::sin(phase));
 else if(branch==207){
  Write(p,bicyclesway::kLeanOffset,physical+weight*std::sin(phase));
  Write(p,bicyclesway::kFrameSwayOffset,Read(p,bicyclesway::kFrameSwayOffset)*.95f);
 }
 // A tail write unrelated to sway must not be rolled back.
 Write(p,0x940,1.23f);
}
void FillRetail(unsigned char* entry){
 std::memset(entry,0,0x1b8);
 for(const auto& i:bicyclesway::kExpected)std::memcpy(entry+i.offset,&i.word,4);
}
int main(){
 using namespace bicyclesway;
 for(int model:{481,509,510})for(int anim:{0,207,208})for(int w=0;w<=4;++w)for(int phaseIndex=0;phaseIndex<32;++phaseIndex){
  std::memset(bike,0x35,sizeof(bike));Model(model);g_origBmxControl=Retail;
  phase=phaseIndex*.31f;weight=w*.15f;branch=anim;
  Write(bike,kFrameSwayOffset,.6f);
  OnBmxProcessControl(bike);
  CHECK(Read(bike,kLeanOffset)==ComfortableLean(physical)&&Read(bike,kFrameSwayOffset)==0);
  CHECK(Read(bike,0x600)==.21f&&Read(bike,0x940)==1.23f);
  CHECK(Read(bike,0x83c)==physical);
  CHECK(current==nullptr);
 }
 CHECK(baseCalls==tailCalls);
 for(int exclusion=0;exclusion<8;++exclusion){
  Model(481);vehicle=bike;isDriver=true;stereo=true;g.FindPlayerPed=FindPed;g.CVehicle_IsDriver=IsDriver;
  if(exclusion==0)vehicle=npc;
  if(exclusion==1)isDriver=false;
  if(exclusion==2)stereo=false;
  if(exclusion==3)Model(522);
  if(exclusion==4)Model(471);
  if(exclusion==5)g.FindPlayerPed=nullptr;
  if(exclusion==6)g.CVehicle_IsDriver=nullptr;
  if(exclusion==7)Model(400);
  branch=207;phase=1;weight=.2f;Write(bike,kFrameSwayOffset,.6f);
  OnBmxProcessControl(bike);
  CHECK(Read(bike,kLeanOffset)==physical+weight*std::sin(phase));
  CHECK(Read(bike,kFrameSwayOffset)==.6f*.95f&&current==nullptr);
 }
 vehicle=bike;isDriver=true;stereo=true;Model(481);g.FindPlayerPed=FindPed;g.CVehicle_IsDriver=IsDriver;
 sample=false;OnBmxProcessControl(bike);CHECK(Read(bike,kFrameSwayOffset)!=0);sample=true;
 physical=std::numeric_limits<float>::quiet_NaN();
 OnBmxProcessControl(bike);CHECK(std::isnan(Read(bike,kLeanOffset)));physical=.37f;
 nested=true;Write(npc,kFrameSwayOffset,.6f);OnBmxProcessControl(bike);nested=false;
 CHECK(Read(bike,kLeanOffset)==ComfortableLean(physical)&&Read(bike,kFrameSwayOffset)==0);
 CHECK(Read(npc,kLeanOffset)==physical+weight*std::sin(phase));
 CHECK(Read(npc,kFrameSwayOffset)==.6f*.95f&&current==nullptr);
 {Scope outer(bike);CaptureBaseLean(bike);
  {Scope inner(npc);CaptureBaseLean(npc);inner.Finish();}
  CHECK(current->bike==bike);
  std::thread thread([]{CHECK(current==nullptr);Scope scope(npc);CHECK(current->bike==npc);});thread.join();
  CHECK(current->bike==bike);outer.Finish();}
 CHECK(current==nullptr);
 constexpr float radians=3.14159265358979323846f/180.f;
 for(float degrees:{-180.f,-60.f,-30.f,-5.f,0.f,5.f,30.f,60.f,180.f}){
  physical=degrees*radians;Model(481);OnBmxProcessControl(bike);
  const float lean=Read(bike,kLeanOffset);
  CHECK(std::abs(lean)<=10.f*radians+1e-6f);
  CHECK(std::abs(lean-std::clamp(physical*.25f,-10.f*radians,10.f*radians))<1e-6f);
 }
 physical=.37f;
 // Actual production sprint/accelerate hooks inside the production wrapper.
 checkPedal=true;g_bicyclePedal.Reset();xr::input.triggers[1]=0;clockTime=10;
 OnBmxProcessControl(bike);CHECK(!heldResult&&!tapResult&&driveResult==0);
 xr::input.triggers[1]=1;clockTime+=.01;OnBmxProcessControl(bike);
 CHECK(heldResult&&tapResult&&driveResult==255);
 clockTime+=.01;OnBmxProcessControl(bike);CHECK(heldResult&&!tapResult&&driveResult==255);
 xr::input.triggers[1]=0;clockTime+=.01;OnBmxProcessControl(bike);
 CHECK(!heldResult&&!tapResult&&driveResult==255);
 for(int i=0;i<22;++i){clockTime+=.01;OnBmxProcessControl(bike);}
 CHECK(driveResult==0); // release eventually coasts
 xr::input.triggers[1]=1;clockTime+=.01;OnBmxProcessControl(bike);CHECK(tapResult);
 clockTime+=1;OnBmxProcessControl(bike);CHECK(!tapResult&&heldResult); // tracking/pause stall
 CHECK(!OnSprintJustDown(nullptr)&&!OnGetSprint(nullptr,1)); // scope does not leak
 // Four taps/s retain ordinary drive through release and add one native edge
 // per tap at different headset update rates. Holding adds only one edge.
 for(int fps:{72,90,120}){
  g_bicyclePedal.Reset();xr::input.triggers[1]=0;clockTime+=1;OnBmxProcessControl(bike);
  int edges=0;
  for(int frame=0;frame<fps*4;++frame){
   xr::input.triggers[1]=(frame%(fps/4))<(fps/8)?1.f:0.f;
   clockTime+=1./fps;OnBmxProcessControl(bike);edges+=tapResult;
   CHECK(driveResult==255);
  }
  CHECK(edges>=16&&edges<=17);
 }
 // Blocked controls, NPCs and non-bicycles cannot produce R2 sprint taps.
 g_inputBlocked=true;clockTime+=.01;OnBmxProcessControl(bike);CHECK(!heldResult&&!tapResult);g_inputBlocked=false;
 Model(522);clockTime+=.01;OnBmxProcessControl(bike);CHECK(!heldResult&&!tapResult);Model(481);
 isDriver=false;clockTime+=.01;OnBmxProcessControl(bike);CHECK(!heldResult&&!tapResult);isDriver=true;
 g_bicycleMode=0;g_bicyclePedalPulse=.8f;clockTime+=.01;OnBmxProcessControl(bike);
 CHECK(!heldResult&&!tapResult&&driveResult==204);g_bicycleMode=1;
 checkPedal=false;
 // Fail-closed signatures and installer: no partially installed experiment.
 alignas(16) unsigned char retail[0x1b8];FillRetail(retail);
 CHECK(MatchesRetail(retail));
 for(const auto& i:kExpected){retail[i.offset]^=1;CHECK(!MatchesRetail(retail));retail[i.offset]^=1;}
 g.LoadBase=reinterpret_cast<std::uintptr_t>(retail)-kControlAddress;g_origBmxControl=nullptr;
 retail[0]^=1;InstallBicycleSwayHook();CHECK(!g_origBmxControl&&allocations==0&&patches==0);retail[0]^=1;
 allocationFails=true;InstallBicycleSwayHook();CHECK(!g_origBmxControl&&patches==0&&MatchesRetail(retail));allocationFails=false;
 patchFails=true;InstallBicycleSwayHook();CHECK(!g_origBmxControl&&frees==1&&MatchesRetail(retail));patchFails=false;
 InstallBicycleSwayHook();CHECK(g_origBmxControl&&patches==2&&!MatchesRetail(retail));
 int installedAllocations=allocations;InstallBicycleSwayHook();CHECK(allocations==installedAllocations);
 std::uint64_t ptr{};std::memcpy(&ptr,allocation+48,8);CHECK(ptr==g.LoadBase+kBaseCallAddress);
 std::memcpy(&ptr,allocation+56,8);CHECK(ptr==reinterpret_cast<std::uintptr_t>(&CaptureBicycleBaseLean));
 std::memcpy(&ptr,allocation+64,8);CHECK(ptr==reinterpret_cast<std::uintptr_t>(retail)+kResumeOffset);
 // Stable pointer literals allow byte comparison with assembler output.
 BuildTrampoline(allocation,0x12340000,0x56780000,0x9abc0000);
 FILE* file=std::fopen("trampoline.bin","wb");CHECK(file);
 CHECK(std::fwrite(allocation,1,kTrampolineBytes,file)==kTrampolineBytes);std::fclose(file);
 std::puts("PASS: 1440 animation samples, lean limit/smoothing, R2 sprint/drive at 72/90/120 Hz, scope and installer failures");
}
'''
test = OUT / 'bicycle-sway.cpp'
test.write_text(code, encoding='utf8')
compile_run = runpy.run_path(str(ROOT / 'tools/test-bike-steering.py'))['compile_run']
compile_run(test, 'bicycle-sway', OUT)

cfg = __import__('json').loads(Path(r'D:/development/SAVRBuild/tools.json').read_text(encoding='utf-8-sig'))
ndk = Path(cfg['AndroidSdk']) / 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin'
asm = OUT / 'trampoline.S'
asm.write_text('''.text
str d8,[sp,#-32]!
stp x29,x30,[sp,#8]
str x19,[sp,#24]
add x29,sp,#8
mov x19,x0
ldr x17,base_call
blr x17
mov x0,x19
ldr x17,capture
blr x17
ldr x17,resume
br x17
base_call: .quad 0x12340000
capture: .quad 0x56780000
resume: .quad 0x9abc0000
''', encoding='ascii')
obj = OUT / 'trampoline.o'
expected = OUT / 'trampoline-assembled.bin'
subprocess.run([str(ndk / 'clang.exe'), '--target=aarch64-linux-android29', '-c', str(asm), '-o', str(obj)], check=True)
subprocess.run([str(ndk / 'llvm-objcopy.exe'), '-O', 'binary', '--only-section=.text', str(obj), str(expected)], check=True)
assert expected.read_bytes() == (OUT / 'trampoline.bin').read_bytes(), 'ARM64 trampoline differs from assembler'
print('PASS: generated ARM64 trampoline matches independent assembler, including literals')

if len(sys.argv) > 1:
    data = Path(sys.argv[1]).read_bytes()
    assert data[:6] == b'\x7fELF\x02\x01', 'expected little-endian ELF64'
    phoff = struct.unpack_from('<Q', data, 32)[0]
    phsize, phnum = struct.unpack_from('<HH', data, 54)

    def at(address):
        for index in range(phnum):
            kind, _, offset, va, _, filesz, _, _ = struct.unpack_from('<IIQQQQQQ', data, phoff + index * phsize)
            if kind == 1 and va <= address < va + filesz:
                return struct.unpack_from('<I', data, offset + address - va)[0]
        raise AssertionError(f'address not mapped: {address:x}')

    header = (ROOT / 'native/src/BicycleSway.h').read_text()
    signatures = re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+)u\}', header)
    for offset, word in signatures:
        address = 0x6be2a8 + int(offset, 16)
        assert at(address) == int(word, 16), f'retail mismatch: {address:x}'
    assert at(0x6be2bc) == 0x9405489d
    assert 0x6be2bc + ((at(0x6be2bc) & 0x3ffffff) << 2) == 0x810530
    print(f'PASS: {len(signatures)} retail instructions and original base-control BL target')
    # CBmx asks for sprint type 1; native energy receives taps and held state.
    assert at(0x6beba0) == 0x52800021  # MOV W1,#1
    for address, target in ((0x6beba4, 0x804210), (0x5bd624, 0x803d40), (0x5bd664, 0x803d30)):
        word = at(address)
        assert word >> 26 == 0b100101
        displacement = word & 0x3ffffff
        if displacement & (1 << 25):
            displacement -= 1 << 26
        assert address + displacement * 4 == target
    print('PASS: retail CBmx sprint type and native SprintJustDown/GetSprint call sites')
