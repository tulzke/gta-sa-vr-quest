"""Exercise production seat transitions, recenter and HMD transforms on the host."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/seat-centering-tests'
OUT.mkdir(parents=True, exist_ok=True)
camera = (ROOT / 'native/src/VrCamera.cpp').read_text(encoding='utf8')


def function(signature):
    start = camera.index(signature)
    end = camera.index('{', start) + 1
    depth = 1
    while depth:
        depth += (camera[end] == '{') - (camera[end] == '}')
        end += 1
    return camera[start:end] + '\n'


cache_start = camera.index('                static void* cachedSeatVehicle')
cache_end = camera.index('                const bool rearPassenger=', cache_start)
center = function('if(seatTransition.Center(')
math_start = camera.index('struct Q  {')
math_end = camera.index('// --- reading / writing the game camera matrix', math_start)
code = r'''
#include "VehicleCabin.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#e);std::exit(3);}}while(0)
#define LOGI(...) ((void)0)
using namespace savr;
struct V3 {float x{},y{},z{};};
''' + camera[math_start:math_end] + r'''
struct CamBasis {V3 right,forward,up,pos;};
struct Origin {bool valid{};V3 pos;Q ori;}g_origin;
namespace savr {GameSymbols g{};}
CamBasis g_vrBase{};
bool g_vrBaseValid{},g_duckLatched{};float g_crouchBlend{};
int g_staticCullCone{};
std::uint64_t g_cameraResetSerial{};
struct{void Invalidate(){}}g_holsterBody;
namespace savr::xr {void SetHolsterMarkers(const void*,int){}}
namespace savr::physicalweapon {void ResetTracking(){}}
namespace savr::npcgrab {void Reset(){}}
namespace savr::driving {void ResetTracking(){}}
namespace savr::melee {void Reset(){}}
void TraceTracking(const char*,V3){}
double nowMs{};double NowMs(){return nowMs;}
bool Near(V3 a,V3 b){return std::abs(a.x-b.x)<1e-4f&&std::abs(a.y-b.y)<1e-4f&&std::abs(a.z-b.z)<1e-4f;}
int seatIndex=-1;
namespace savr::vehicleentry {int SeatIndex(void*,void*){return seatIndex;}}
''' + function('V3 TrackingToGame(') + function('void EnsureOrigin(') + function('void RelativePose(') + function('CamBasis BuildVrCamera(') + function('void RequestRecenter()') + r'''
struct Result {CamBasis view;V3 seat;std::uint64_t serial;};
Result Frame(void* vehicle,void* ped,V3 headPos,Q headOri,bool enteringVehicle=false,
             bool thirdPersonView=false,bool ready=true){
 bool vehicleBasisValid=ready;
''' + camera[cache_start:cache_end] + r'''
 if(vehicle&&!enteringVehicle&&!thirdPersonView&&ready&&!cachedSeatLocalValid){
  GameSymbols::Vec3 eye{};
  if(vehiclecabin::ReadSeatEye(g,seatModel,1,passengerSeat,&eye)){
   cachedSeatLocal={eye.x,eye.y,eye.z};cachedSeatLocalValid=true;
  }
 }
''' + center + r'''
 CamBasis base{{-1,0,0},{0,1,0},{0,0,1},cachedSeatLocal};
 g_vrBase=base;g_vrBaseValid=true;
 EnsureOrigin(headPos,headOri);
 V3 relPos{};Q relOri{};RelativePose(headPos,headOri,relPos,relOri);
 return {BuildVrCamera(base,relPos,relOri),cachedSeatLocal,seatTransition.serial};
}
int main(){
 unsigned char car[0xc68]{},ped[16]{},otherPed[16]{},pool[0x30]{},modelInfo[0x100]{},structure[0x100]{};
 void* models[612]{};models[411]=modelInfo;g.CModelInfo_ms_modelInfoPtrs=models;
 void* structurePtr=structure;std::memcpy(modelInfo+0x90,&structurePtr,8);
 GameSymbols::Vec3 front{.42f,.2f,.3f},rear{.39f,-.8f,.35f};
 std::memcpy(structure+0x30,&front,sizeof(front));std::memcpy(structure+0x3c,&rear,sizeof(rear));
 std::uint16_t model=411;std::memcpy(car+0x32,&model,2);
 std::uint8_t generations[110]{};generations[0]=7;
 void* objects=car;auto flags=generations;int count=110;
 std::memcpy(pool,&objects,8);std::memcpy(pool+8,&flags,8);std::memcpy(pool+0x10,&count,4);
 void* poolPtr=pool;g.CPools_ms_pVehiclePool=&poolPtr;
 const V3 h0{.2f,1.65f,-.4f};const Q q0{0,.38268343f,0,.92387953f};
 Frame(nullptr,ped,h0,q0);Frame(nullptr,ped,h0+V3{.3f,-.2f,.1f},q0);
 const auto before=g_cameraResetSerial;
 Frame(car,ped,h0,q0,true);CHECK(g_cameraResetSerial==before);
 Frame(car,ped,h0,q0,false,false,false);CHECK(g_cameraResetSerial==before);
 auto seated=Frame(car,ped,h0,q0);CHECK(g_cameraResetSerial==before+1);
 CHECK(Near(seated.seat,{-.42f,.2f,.9f}) && Near(seated.view.pos,seated.seat));
 for(int f=0;f<200;++f){nowMs=f*15.;auto stable=Frame(car,ped,h0,q0);CHECK(g_cameraResetSerial==before+1 && stable.serial==seated.serial);}
 // Real HMD translation still produces exactly the normal tracking transform.
 const V3 motion{.17f,-.13f,.09f};
 auto leaning=Frame(car,ped,h0+motion,q0);V3 rp{};Q rq{};
 RelativePose(h0+motion,q0,rp,rq);
 CHECK(Near(leaning.view.pos,seated.seat+TrackingToGame(g_vrBase,rp)));
 CHECK(!Near(leaning.view.pos,seated.seat));
 RequestRecenter();auto manual=Frame(car,ped,h0+motion,q0);
 CHECK(manual.serial==seated.serial && Near(manual.seat,seated.seat) && Near(manual.view.pos,manual.seat));
 CHECK(g_cameraResetSerial==before+2);
 // Same car, new seat: exactly one centering; repeated reads cannot replay it.
 seatIndex=0;auto passenger=Frame(car,ped,h0,q0);
 CHECK(g_cameraResetSerial==before+3 && Near(passenger.view.pos,{.42f,.2f,.9f}));
 Frame(car,ped,h0,q0);CHECK(g_cameraResetSerial==before+3);
 // Reused address/model with a new pool generation is a new seating context.
 generations[0]=8;Frame(car,ped,h0,q0);CHECK(g_cameraResetSerial==before+4);
 Frame(car,otherPed,h0,q0);CHECK(g_cameraResetSerial==before+5);
 Frame(nullptr,otherPed,h0,q0);
 Frame(car,otherPed,h0,q0,false,true);CHECK(g_cameraResetSerial==before+5);
 auto firstPerson=Frame(car,otherPed,h0,q0);CHECK(g_cameraResetSerial==before+6 && Near(firstPerson.view.pos,firstPerson.seat));
 // Key readers reject out-of-pool pointers and freed slots without indexing flags.
 const auto key=vehiclecabin::SeatKey(g,car,ped,411,-1);
 CHECK(key.pool==pool && key.generation==8);
 const auto misaligned=vehiclecabin::SeatKey(g,car+1,ped,411,-1);CHECK(!misaligned.pool);
 generations[0]=0x80;const auto free=vehiclecabin::SeatKey(g,car,ped,411,-1);CHECK(!free.pool);
 std::puts("PASS production seat transitions: deferred boarding/anchor, one rebase, free HMD motion, manual recenter preserves geometry, seat/ped/pool generation, chase-to-first-person, pool bounds");
}
'''
(OUT / 'test.cpp').write_text(code, encoding='utf8')
vc = os.environ.get('VS_VCVARS64')
if not vc:
    where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires',
                                   'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    vc = str(Path(base) / 'VC/Auxiliary/Build/vcvars64.bat')
java = Path(os.environ['JAVA_HOME'])
cmd = OUT / 'run.cmd'
cmd.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\n'
               f'cl /nologo /std:c++20 /EHsc /W3 /I"{ROOT / "native/src"}" '
               f'/I"{java / "include"}" /I"{java / "include/win32"}" "{OUT / "test.cpp"}" '
               f'/Fe:"{OUT / "test.exe"}" /Fo:"{OUT / "test.obj"}"\n'
               'if errorlevel 1 exit /b 1\n' + f'"{OUT / "test.exe"}"\n', encoding='utf8')
subprocess.run(['cmd', '/c', str(cmd)], cwd=OUT, check=True)
