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
#include "PhysicalWeapon.h"
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
int disarmCalls[2]{};
bool GetHeldVisualPoseTracking(int,int,TrackingPose* pose){*pose={};return true;}
bool GetDroppedPoseTracking(int,TrackingPose* pose){*pose={};return true;}
bool DisarmAfterShot(int h,int slot,int type,const recoil::Sample& kick){
 assert(slots[h]==slot && inventory[0x730/4+slot*8]==type);
 assert(kick.pitch>=0 && std::isfinite(kick.yaw));++disarmCalls[h];slots[h]=-1;recoil::Invalidate(h);return true;
}
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
 inventory[0x730/4+2*8]=25;step();
 OnShot(0,25,0);assert(physicalweapon::disarmCalls[0]==0);
 FinishShots();assert(physicalweapon::disarmCalls[0]==1); // deferred until native fire returned
 FinishShots();assert(physicalweapon::disarmCalls[0]==1); // one request, not one per pellet
 step();assert(ReleasedHandOffset(0).x<0); // clearing ownership preserves wrist shoulder kick
 for(int i=0;i<120;++i)step();assert(ReleasedHandOffset(0).x==0);
 physicalweapon::slots[0]=2;step();physicalweapon::support[0]=1;step();OnShot(0,25,0);FinishShots();
 assert(physicalweapon::disarmCalls[0]==1);
 physicalweapon::support[0]=-1;step();calib::tuning[25][0].disarmOneHanded=false;
 OnShot(0,25,0);FinishShots();assert(physicalweapon::disarmCalls[0]==1);
 calib::tuning[25][0].disarmOneHanded=true;OnShot(0,25,0);Invalidate(0);FinishShots();
 assert(physicalweapon::disarmCalls[0]==1);
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
  f<<"recoil_weapon 22 -5 100 900 200\nrecoil_weapon 32 nan 2 3 nan\n";}
 Init();p=SnapshotRecoil(22);tec=SnapshotRecoil(32);
 assert(p.verticalDegrees==0 && p.horizontalDegrees==20 && p.shoulderMm==200 && p.supportReductionPercent==100);
 assert(std::isfinite(tec.verticalDegrees) && std::isfinite(tec.supportReductionPercent));
 {std::ofstream f("recoil-calib.ini");f<<"recoil_weapon 22 1 2\nrecoil_weapon 80 1 2 3 4\n";}
 Init();assert(SnapshotRecoil(22).verticalDegrees==savr::recoil::Defaults(22).verticalDegrees);
 // Old four-component rows retain the shotgun's default disarm. Explicit
 // per-skill ON/OFF survives save/load and is independent of row order.
 {std::ofstream f("recoil-calib.ini");
  f<<"recoil_one_hand_disarm 25 1 0\nrecoil_weapon_skill 25 1 38 12 6 95\n";}
 Init();assert(SnapshotRecoil(25,0).disarmOneHanded);
 assert(!SnapshotRecoil(25,1).disarmOneHanded);
 assert(!SnapshotRecoil(22,0).disarmOneHanded);
 auto shotgun=SnapshotRecoil(25,2);shotgun.disarmOneHanded=false;
 SetRecoilTuning(25,shotgun,2);Init();
 assert(!SnapshotRecoil(25,2).disarmOneHanded && SnapshotRecoil(25,0).disarmOneHanded);
 shotgun=SnapshotRecoil(25,0);shotgun.shoulderMm=200;SetRecoilTuning(25,shotgun,0);Init();
 assert(SnapshotRecoil(25,0).shoulderMm==200);
 AdjustRecoilTuning(25,2,10,0);assert(SnapshotRecoil(25,0).shoulderMm==200);
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

# Compile the production ejection and catch paths together. Engine access and
# tracked poses are mocked, but the release trajectory, ownership guards and
# deliberate re-grip behavior below are the actual gameplay implementation.
disarm = r'''
#include "PhysicalWeapon.h"
#include "RecoilModel.h"
#include <atomic>
#include <cassert>
#include <mutex>
#define LOGI(...) ((void)0)
namespace disarm_test {
using savr::physicalweapon::TrackingPose;
namespace recoil=savr::recoil;
constexpr int kHandCount=2;
constexpr float kMaximumThrowSpeed=6,kReleaseUpwardImpulse=1,kGripOpen=.2f,kGripCatch=.7f,kCatchRadius=.2f;
struct V3{float x{},y{},z{};};
V3 Add(V3 a,V3 b){return{a.x+b.x,a.y+b.y,a.z+b.z};}
V3 Scale(V3 a,float s){return{a.x*s,a.y*s,a.z*s};}
float Length(V3 a){return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
float Distance(V3 a,V3 b){return Length(Add(a,Scale(b,-1)));}
struct Hand{bool valid=true;float grip=1;float position[3]{};}gHands[2];
struct DroppedWeapon{int slot=-1;TrackingPose startPose{};V3 velocity{},angularAxis{};float angularSpeed{};double startedAt{};}gDropped[2];
std::mutex gMutex;bool gInteractionsBlocked{},gDualHoldEnabled=true,gGripLock=true;
int gHeldSlot[2]{-1,-1},gSupportHand[2]{-1,-1},gHeldVisualSlot[2]{3,3};
bool gHeldVisualValid[2]{true,true},gDisarmCatchBlocked[2]{},gGripLatched[2]{};
TrackingPose gHeldVisualPose[2]{};V3 gHandVelocity[2]{},gHandAngularVelocity[2]{};
std::atomic<unsigned>gHeldMaskRelaxed{};
int slotType[13]{},selectedSlot=-1,caught{};
void* Find(int){return slotType;}struct{void*(*FindPlayerPed)(int)=Find;}g;
bool ValidHand(int h){return h>=0&&h<2;}
bool ValidSlot(int s){return s>0&&s<13;}
bool IsSupportHandLocked(int h){return gSupportHand[1-h]==h;}
int AnyHeldHandLocked(){return ValidSlot(gHeldSlot[0])?0:ValidSlot(gHeldSlot[1])?1:-1;}
int WeaponTypeInSlot(void*,int slot){return slotType[slot];}
double NowSeconds(){return 1;}
V3 PositionOf(const TrackingPose& p){return{p.position[0],p.position[1],p.position[2]};}
V3 GripPosition(const Hand& h){return{h.position[0],h.position[1],h.position[2]};}
TrackingPose RawGripFallbackPose(const Hand&){return {};}
void ClearHeldLocked(int h){gHeldSlot[h]=-1;}
void SelectCurrentSlot(void*,int s){selectedSlot=s;}
void GiveToHandLocked(void*,int h,int slot,const char*){gHeldSlot[h]=slot;gDisarmCatchBlocked[h]=false;++caught;}
bool DroppedPoseAtLocked(int h,double,TrackingPose* out){
 if(!ValidSlot(gDropped[h].slot))return false;*out=gDropped[h].startPose;return true;
}
'''
disarm += function(physical, 'void StartDropLocked(')
disarm += function(physical, 'bool DisarmAfterShot(')
catch_start = physical.index('// Recoil-ejected weapons cannot snap back')
catch_end = physical.index('// Release returns at the matching socket', catch_start)
disarm += 'void CatchTick(){void* ped=Find(-1);double now=1;unsigned handledHands=0;\n'
disarm += physical[catch_start:catch_end] + '\n}\n}\n'
disarm += r'''
int main(){
 using namespace disarm_test;
 slotType[3]=25;
 for(int hand:{0,1}){
  for(int h:{0,1}){gHands[h]={};gHands[h].valid=h==hand;gHeldSlot[h]=-1;
    gDropped[h]={};gSupportHand[h]=-1;gDisarmCatchBlocked[h]=false;}
  gHeldSlot[hand]=3;gHeldMaskRelaxed=1u<<hand;
  assert(DisarmAfterShot(hand,3,25,{.8f,.1f,0}));
  assert(gGripLock && gHeldSlot[hand]==-1 && gDropped[hand].slot==3);
  assert(gDisarmCatchBlocked[hand] && gGripLatched[hand]);
  assert(gHeldMaskRelaxed==0 && selectedSlot==0);
  assert(gDropped[hand].velocity.y>1 && gDropped[hand].velocity.x<0);
  assert(gDropped[hand].angularSpeed>0);
  const int old=caught;
  CatchTick();assert(caught==old); // closed grip cannot immediately recatch
  gHands[hand].grip=0;CatchTick();assert(!gDisarmCatchBlocked[hand] && caught==old);
  gHands[hand].grip=1;CatchTick();assert(caught==old+1 && gHeldSlot[hand]==3);
 }
 gHeldSlot[0]=3;gHands[0].valid=true;gSupportHand[0]=1;
 assert(!DisarmAfterShot(0,3,25,{.8f,0,0}));assert(gHeldSlot[0]==3);
 gSupportHand[0]=-1;gInteractionsBlocked=true;
 assert(!DisarmAfterShot(0,3,25,{.8f,0,0}));gInteractionsBlocked=false;
 assert(!DisarmAfterShot(0,3,26,{.8f,0,0}));
 assert(!DisarmAfterShot(0,2,25,{.8f,0,0}));
 gHands[0].valid=false;assert(!DisarmAfterShot(0,3,25,{.8f,0,0}));
 gHands[0].valid=true;gHandVelocity[0]={.2f,.3f,-.1f};
 StartDropLocked(0,3,1);assert(std::abs(gDropped[0].velocity.y-1.3f)<.00001f);
 assert(DisarmAfterShot(0,3,25,{0,0,.003f}));
 assert(std::abs(gDropped[0].velocity.y-.3f)<.00001f); // no hard-coded upward impulse
 assert(gDropped[0].angularSpeed==0);
}
'''
disarm_file=OUT/'disarm.cpp'
disarm_file.write_text(disarm,encoding='utf8')
compile_run(disarm_file,'recoil-disarm')

# Exercise the exact compositor block that moves the visible wrists, including
# translation-only recoil. A model-only test cannot catch a pinned primary hand.
xr=(ROOT/'native/src/Xr.cpp').read_text(encoding='utf8')
hand_start=xr.index('        const auto& kick=twoHand.weaponRecoil[hand];')
hand_end=xr.index('        const bool debugPinch=',hand_start)
hand_test=r'''
#include "RecoilModel.h"
#include "PhysicalWeapon.h"
#include <cassert>
namespace recoil=savr::recoil;
struct XrVector3f{float x{},y{},z{};};
struct Captured{recoil::Transform weaponRecoil[2];};
void Apply(int hand,const Captured& twoHand,XrVector3f& pos,XrVector3f& right,
           XrVector3f& forward,XrVector3f& up){
''' + xr[hand_start:hand_end] + r'''
}
int main(){
 for(bool support:{false,true}){
  recoil::State state;
  state.Shot(recoil::ForWeapon(25,support,3,{0,0,50,92,false}));
  state.Advance(.05f);
  assert(state.sample.rear>0);
  const float barrel[]{0,0,-1},top[]{0,1,0},pivot[]{0,1,-.4f};
  const auto transform=recoil::Build(state.sample,barrel,top,pivot);
  Captured captured;captured.weaponRecoil[0]=captured.weaponRecoil[1]=transform;
  for(int hand:{0,1}){
   XrVector3f pos{.1f*hand,1,-.4f},right{1,0,0},forward{0,0,-1},up{0,1,0};
   const float oldZ=pos.z;
   Apply(hand,captured,pos,right,forward,up);
   assert(std::abs((pos.z-oldZ)-state.sample.rear)<.000001f);
   assert(pos.z>oldZ); // both wrists follow the gun toward the shoulder
   savr::physicalweapon::TrackingPose model;model.position[2]=oldZ;
   transform.Apply(model);
   assert(std::abs(model.position[2]-pos.z)<.000001f);
  }
 }
}
'''
hands_file=OUT/'shoulder-hands.cpp'
hands_file.write_text(hand_test,encoding='utf8')
compile_run(hands_file,'recoil-shoulder-hands')
