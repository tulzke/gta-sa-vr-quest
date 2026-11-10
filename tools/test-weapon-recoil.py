"""Test production recoil dynamics, limits, frame rates, reflected poses and ownership."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build' / 'weapon-recoil-tests'
OUT.mkdir(parents=True, exist_ok=True)

def compile_run(source, name):
    exe = OUT / (name + ('.exe' if os.name == 'nt' else ''))
    if os.name == 'nt':
        vc = os.environ.get('VS_VCVARS64')
        if not vc:
            where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
            base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
            vc = str(Path(base) / 'VC/Auxiliary/Build/vcvars64.bat')
        command = OUT / (name + '.cmd')
        command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{ROOT / "native/src"}" "{source}" /Fe:"{exe}" /Fo:"{OUT / (name + ".obj")}"\nif errorlevel 1 exit /b 1\n"{exe}"\n', encoding='utf8')
        subprocess.run(['cmd', '/c', str(command)], cwd=OUT, check=True)
    else:
        compiler = os.environ.get('CXX') or shutil.which('c++')
        subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-I'+str(ROOT/'native/src'), str(source), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)

compile_run(ROOT / 'tools/tests/weapon_recoil.cpp', 'weapon-recoil')

# Exercise the actual runtime owner, with only engine/tracking and time mocked.
runtime = (ROOT/'native/src/WeaponRecoil.cpp').read_text(encoding='utf8')
runtime = '\n'.join(line for line in runtime.splitlines() if not line.startswith('#include'))
start = runtime.index('double Now(){')
end = runtime.index('\n}', start)
runtime = runtime[:start] + 'double Now(){return testTime;}' + runtime[end:]
harness = r'''
#include "WeaponRecoil.h"
#include <cassert>
#include <mutex>
#include <cstdint>
double testTime=1;
namespace savr {
namespace xr { struct HandPose {bool valid=true,aimValid=true;}; }
namespace calib {
int level=2;int PhysicalRecoilLevel(){return level;}
recoil::Tuning tuning[64][3]{};
recoil::Tuning SnapshotRecoil(int type,int skill){return tuning[type][skill];}
}
int inventory[13*8+0x730/4]{};float stats[100]{};
void* Find(int){return inventory;}
struct {void*(*FindPlayerPed)(int)=Find;float* CStats_StatTypesFloat=stats;} g;
namespace physicalweapon {
int slots[2]{2,5},support[2]{-1,-1};bool detached[2]{};
bool valid=true;std::uint64_t epoch=1;
bool GetInteractionTrackingSnapshot(float*,float*,xr::HandPose* p,std::uint64_t* e){
 p[0]={};p[1]={};*e=epoch;return valid;
}
bool IsPrimaryDetached(int h){return detached[h];}
int HeldSlot(int h){return detached[h]?-1:slots[h];}
int SupportHand(int h){return support[h];}
}
}
''' + runtime + r'''
int main(){
 using namespace savr;using namespace savr::recoil;
 inventory[0x730/4+2*8]=22;inventory[0x730/4+5*8]=30;
 for(int type=0;type<64;++type)for(int skill=0;skill<3;++skill)
     calib::tuning[type][skill]=Defaults(type,skill);
 auto step=[] {testTime+=1./90;Update(false);};
 step();step();
 OnShot(0,22,0);assert(Snapshot(0,22).pitch==0);
 step();assert(Snapshot(0,22).pitch>0);assert(Snapshot(1,30).pitch==0);
 const Sample same=Snapshot(0,22);
 for(int i=0;i<100;++i)assert(Snapshot(0,22).pitch==same.pitch); // reads don't advance
 OnShot(1,30,2);step();assert(Snapshot(1,30).pitch>0);
 Invalidate(0);assert(Snapshot(0,22).pitch==0);assert(Snapshot(1,30).pitch>0);
 Update(true);assert(Snapshot(1,30).pitch==0);
 OnShot(1,30,0);assert(Snapshot(1,30).pitch==0); // menu cannot queue impulses
 step();OnShot(1,30,0);step();assert(Snapshot(1,30).pitch>0);
 physicalweapon::detached[1]=true;step();assert(Snapshot(1,30).pitch==0);
 physicalweapon::detached[1]=false;step();OnShot(1,30,0);step();
 ++physicalweapon::epoch;step();assert(Snapshot(1,30).pitch==0);
 step();OnShot(1,30,0);step();
 inventory[0x730/4+5*8]=31;step();assert(Snapshot(1,31).pitch==0);
 OnShot(1,30,0);step();assert(Snapshot(1,31).pitch==0); // stale shot identity
 OnShot(1,31,0);step();assert(Snapshot(1,31).pitch>0);
 calib::level=0;step();assert(Snapshot(1,31).pitch==0);
 calib::level=2;step();OnShot(1,31,0);step();testTime+=1;step();
 assert(Snapshot(1,31).pitch==0);
 physicalweapon::valid=false;step();OnShot(1,31,0);
 assert(Snapshot(1,31).pitch==0);
 physicalweapon::valid=true;step();Invalidate(1);step();
 // The actual native tier selects an independent profile, even when it is
 // intentionally stronger than POOR. Never apply a hidden skill multiplier.
 calib::tuning[31][2]={0,0,0,65};
 OnShot(1,31,2);step();assert(Snapshot(1,31).pitch==0);
 OnShot(1,31,0);step();assert(Snapshot(1,31).pitch>0);
}
'''
owner = OUT/'runtime.cpp'
owner.write_text(harness, encoding='utf8')
compile_run(owner, 'recoil-runtime')

# Load/edit/save the real settings, ensuring the old vibration setting retains
# its meaning and files without the new field get the MEDIUM default.
settings = (ROOT/'native/src/Calib.cpp').read_text(encoding='utf8')
settings = settings.replace('#include "Log.h"','')
if os.name == 'nt':
    settings = settings.replace('#include <unistd.h>',
        '#include <io.h>\n#define fsync _commit\n#define fileno _fileno')
    settings = settings.replace('std::rename(kTempPath, kPath)',
        '(std::remove(kPath),std::rename(kTempPath,kPath))')
settings = settings.replace('/sdcard/Android/data/com.rockstargames.gtasa/files/vr_calib.ini',
                            'recoil-calib.ini')
settings = '#define LOGI(...) ((void)0)\n#define LOGW(...) ((void)0)\n' + settings
settings += r'''
#include <cassert>
#include <fstream>
int main(){
 using namespace savr::calib;
 {std::ofstream f("recoil-calib.ini");f<<"recoil_level 1\n";}
 Init();assert(RecoilLevel()==1 && PhysicalRecoilLevel()==2);
 SetPhysicalRecoilLevel(3);Init();assert(RecoilLevel()==1 && PhysicalRecoilLevel()==3);
 CyclePhysicalRecoil(1);assert(PhysicalRecoilLevel()==0);
 Init();assert(PhysicalRecoilLevel()==0 && RecoilLevel()==1);
 CyclePhysicalRecoil(-1);assert(PhysicalRecoilLevel()==3);
 SetPhysicalRecoilLevel(-10);assert(PhysicalRecoilLevel()==0);
 SetPhysicalRecoilLevel(10);Init();assert(PhysicalRecoilLevel()==3);
 SetRecoilLevel(0);Init();assert(RecoilLevel()==0 && PhysicalRecoilLevel()==3);
 SetRecoilTuning(22,{12.3f,2.4f,4.5f,71.f});
 SetRecoilTuning(32,{6.7f,.8f,1.9f,42.f});
 SetRecoilTuning(22,{22.1f,4.2f,6.3f,81.f},2);
 Init();
 auto p=SnapshotRecoil(22),tec=SnapshotRecoil(32),ak=SnapshotRecoil(30);
 assert(p.verticalDegrees==12.3f && p.horizontalDegrees==2.4f);
 assert(p.shoulderMm==4.5f && p.supportReductionPercent==71.f);
 assert(tec.verticalDegrees==6.7f && tec.supportReductionPercent==42.f);
 assert(ak.verticalDegrees==savr::recoil::Defaults(30).verticalDegrees);
 assert(SnapshotRecoil(22,2).verticalDegrees==22.1f);
 assert(SnapshotRecoil(22,1).verticalDegrees==savr::recoil::Defaults(22,1).verticalDegrees);
 AdjustRecoilTuning(22,0,1);AdjustRecoilTuning(22,3,-1);Init();
 p=SnapshotRecoil(22);assert(p.verticalDegrees==12.4f && p.supportReductionPercent==70.f);
 assert(SnapshotRecoil(32).verticalDegrees==6.7f);
 assert(SnapshotRecoil(22,2).verticalDegrees==22.1f);
 AdjustRecoilTuning(22,0,1,2);Init();
 assert(SnapshotRecoil(22,2).verticalDegrees==22.2f && SnapshotRecoil(22,0).verticalDegrees==12.4f);
 {std::ifstream f("recoil-calib.ini");std::string text((std::istreambuf_iterator<char>(f)),{});
  assert(text.find("recoil_weapon_skill 22 2 22.2 4.2 6.3 81.0")!=std::string::npos);}
 {std::ofstream f("recoil-calib.ini");
  f<<"recoil_weapon 22 -5 100 90 200\nrecoil_weapon 32 nan 2 3 nan\n";}
 Init();p=SnapshotRecoil(22);tec=SnapshotRecoil(32);
 assert(p.verticalDegrees==0 && p.horizontalDegrees==20 && p.shoulderMm==50 && p.supportReductionPercent==100);
 assert(std::isfinite(tec.verticalDegrees) && std::isfinite(tec.supportReductionPercent));
 {std::ofstream f("recoil-calib.ini");f<<"recoil_weapon 22 1 2\nrecoil_weapon 80 1 2 3 4\n";}
 Init();assert(SnapshotRecoil(22).verticalDegrees==savr::recoil::Defaults(22).verticalDegrees);
}
'''
setting_file = OUT/'settings.cpp'
setting_file.write_text(settings, encoding='utf8')
compile_run(setting_file, 'recoil-settings')

physical = (ROOT/'native/src/PhysicalWeapon.cpp').read_text(encoding='utf8')
def function(source, signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

feedback = r'''
#include "PhysicalWeapon.h"
#include <cassert>
#include <mutex>
namespace savr::physicalweapon {
std::mutex gMutex;
int gHeldSlot[2]{2,2},gHeldBaseSlot[2]{-1,-1},gHeldVisualSlot[2]{-1,-1};
bool gHeldBaseValid[2]{},gHeldVisualValid[2]{},gPrimaryDetached[2]{};
TrackingPose gHeldVisualPose[2]{},gHeldBasePose[2]{};
bool ValidHand(int h){return h>=0&&h<2;}
TrackingPose NormalizedTrackingPose(TrackingPose p){return p;}
'''
feedback += function(physical, 'void PublishHeldVisualPoseTracking(')
feedback += function(physical, 'void PublishHeldBasePoseTracking(')
feedback += r'''
}
int main(){
 using namespace savr::physicalweapon;
 for(int hand:{0,1}){
  TrackingPose base;base.position[0]=1;
  PublishHeldBasePoseTracking(hand,2,&base);
  TrackingPose kicked=base;kicked.position[0]=1.02f;
  for(int i=0;i<100;++i)PublishHeldVisualPoseTracking(hand,2,&kicked);
  assert(gHeldBasePose[hand].position[0]==1);
  assert(gHeldVisualPose[hand].position[0]==1.02f);
  base.position[0]=2;PublishHeldBasePoseTracking(hand,2,&base);
  PublishHeldVisualPoseTracking(hand,2,&kicked);
  assert(gHeldBasePose[hand].position[0]==2);
 }
}
'''
feedback_file = OUT/'feedback.cpp'
feedback_file.write_text(feedback, encoding='utf8')
compile_run(feedback_file, 'recoil-base-feedback')
