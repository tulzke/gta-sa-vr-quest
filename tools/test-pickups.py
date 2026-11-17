"""Exercise production pickup gestures, shared selection and retail transactions."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build' / 'pickup-tests'
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
        subprocess.run([os.environ.get('CXX') or shutil.which('c++'), '-std=c++20', '-Wall', '-Wextra', '-I'+str(ROOT/'native/src'), str(source), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)

compile_run(ROOT / 'tools/tests/pickup_gesture.cpp', 'pickup-gesture')

# Compile the actual runtime with only the platform hook installer removed.
# The retail engine, tracking and inventory below are controlled fixtures.
runtime = (ROOT/'native/src/Pickups.cpp').read_text(encoding='utf8')
runtime = '\n'.join(line for line in runtime.splitlines() if not line.startswith('#include'))
runtime = runtime.replace('__attribute__((visibility("hidden")))', '')
start = runtime.index('bool InstallContactGate(')
end = runtime.index('} // namespace', start)
runtime = runtime[:start] + runtime[end:]
start = runtime.index('void Install(void* handle)')
end = runtime.index('void ProbeGrabs(', start)
runtime = runtime[:start] + runtime[end:]
start = runtime.index('double Now(){')
end = runtime.index('\n', start)
runtime = runtime[:start] + 'double Now(){return testTime;}' + runtime[end:]
harness = r'''
#include "Pickups.h"
#include "PickupGesture.h"
#include "GrabArbitration.h"
#include "PhysicalWeapon.h"
#include "VehicleDoorMotion.h"
#include "TrackingFrame.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#define LOGI(...) ((void)0)
#define LOGE(...) ((void)0)
double testTime=1;
namespace savr {
std::uint8_t player[0x1000]{},data[0x100]{},object[2][0x200]{},pool[620*0x24]{};
float playerMatrix[16]{},matrices[2][16]{},renderedPoses[2][16]{};
int matrixSyncs=0,frameSyncs=0;
xr::HandPose poses[2]{};bool usable=true,focused=true,accept=true,dropped=false;
std::uint64_t trackingEpoch=1;unsigned owners=0,weaponMask=0;
int holdCalls=0,grantCalls=0,holdType=0;
void* Find(int){return player;}void* Vehicle(int,bool){return nullptr;}
struct {void*(*FindPlayerPed)(int)=Find;void*(*FindPlayerVehicle)(int,bool)=Vehicle;} g;
namespace xr {
float pickupAmplitudes[2]{};
bool IsSessionFocused(){return focused;}void TriggerHaptic(int,float,float,float){}
void PublishPickupHaptic(int h,float amplitude,std::uint64_t){pickupAmplitudes[h]=amplitude;}
}
namespace vehicledoors {unsigned ClaimedHands(){return owners;}}
namespace npcgrab {unsigned ClaimedHands(){return 0;}}
namespace basketball {unsigned HeldHandMask(){return 0;}}
namespace driving {unsigned ServiceRadioClaimedHands(){return 0;}unsigned WaterMonitorClaimedHands(){return 0;}}
namespace physicalweapon {
bool GetHandPosesSnapshot(xr::HandPose out[2]){std::copy_n(poses,2,out);return true;}
bool GetInteractionTrackingSnapshot(float*,float*,xr::HandPose out[2],std::uint64_t* epoch){
 std::copy_n(poses,2,out);*epoch=trackingEpoch;return usable;
}
unsigned HeldHandMaskRelaxed(){return weaponMask;}
unsigned RadialMenuHandMask(){return 0;}
bool IsSlotDropped(int){return dropped;}
bool HoldCollectedWeapon(int h,int,int type){++holdCalls;holdType=type;weaponMask|=1u<<h;return true;}
}
namespace vrcam {
bool TrackingPointToWorld(const float p[3],float w[3]){std::copy_n(p,3,w);return true;}
bool WeaponPickupGripWorld(int,const void* m,float p[3]){std::memcpy(p,static_cast<const float*>(m)+12,12);return true;}
}
}
''' + runtime + r'''
using namespace savr;
using namespace savr::pickups;
std::uint32_t timer=100;
std::int32_t collect=0;
int ModelType(int model){return model==346?22:model==347?23:model==352?28:model==372?32:0;}
void SyncMatrix(void* obj){
 std::array<float,16> pose{};assert(EntityPose(obj,pose));++matrixSyncs;
 for(int i=0;i<2;++i)if(obj==object[i])std::copy_n(pose.data(),16,renderedPoses[i]);
}
void SyncFrame(void*){++frameSyncs;}
bool Retail(void* self,void* ped,void*,int){
 // Retail resets the object and publishes its RenderWare frame BEFORE contact.
 void* obj=Read<void*>(self,4);
 if(obj){void* matrix=MatrixOf(obj);
  for(int i=0;i<3;++i)Write(matrix?matrix:obj,(matrix?0x30:8)+i*4,ReadPos16(self,0x14+i*2));
  SyncMatrix(obj);SyncFrame(obj);
 }
 if(!SavrPickupContactAllowed(self,ped)||!accept)return false;
 const int type=WeaponType(self);++grantCalls;
 Write(ped,0x8d4,type); // engine will materialise the inventory later
 Write(ped,0x8dc,std::uint8_t(2));Write(data,0x2c,std::uint8_t(2));
 Write(self,4,static_cast<void*>(nullptr));Write(self,0x20,std::uint8_t(0));return true;
}
void Setup(){
 Reset();std::memset(player,0,sizeof(player));std::memset(pool,0,sizeof(pool));
 std::memset(object,0,sizeof(object));std::memset(matrices,0,sizeof(matrices));
 playerMatrix[0]=playerMatrix[5]=playerMatrix[10]=1;
 Write(player,0x18,playerMatrix);Write(player,0x540,data);Write(player,0x8d4,55);
 g_pool=pool;g_weaponForModel=ModelType;g_collectBuffer=&collect;g_timeMs=&timer;g_origUpdate=Retail;
 g_updateRwMatrix=SyncMatrix;g_updateRwFrame=SyncFrame;matrixSyncs=frameSyncs=0;
 poses[0]={};poses[0].valid=true;poses[1]={};
 usable=focused=accept=true;owners=weaponMask=0;dropped=false;holdCalls=grantCalls=holdType=0;
 testTime=1;trackingEpoch=1;collect=0;
}
void ItemAt(int index,int model,float x){
 void* self=pool+index*0x24;Write(self,4,static_cast<void*>(object[index]));
 Write(object[index],0x18,matrices[index]);matrices[index][0]=matrices[index][5]=matrices[index][10]=1;
 matrices[index][12]=x;
 Write(self,0x14,std::int16_t(x*8));Write(self,0x1c,std::int16_t(model));
 Write(self,0x1e,std::uint16_t(7));Write(self,0x20,std::uint8_t(3));
 Item item{};assert(LoadItem(self,player,item));std::copy_n(item.point,3,poses[0].gripPos);
}
void Step(float grip){
 poses[0].grip=grip;testTime+=.05;grabarbitration::current.Begin(true);
 ProbeGrabs(poses);FinishProbes();Tick();
}
void Hold(){Step(0);Step(0);for(int i=0;i<24;++i)Step(1);}
int main(){
 // Real pickups can have only CSimpleTransform. These same-slot replacements
 // reproduce Colt versus Deagle and Uzi versus Tec without allocating CMatrix.
 for(int incoming:{22,28}){
  Setup();ItemAt(0,incoming==22?346:352,0);
  const int slot=incoming==22?2:4,previous=incoming==22?24:32;
  Write(player,0x730+slot*0x20,previous);
  Write(object[0],0x18,static_cast<void*>(nullptr));Write(object[0],0x14,1.57079632679f);
  std::array<float,16> simple{};assert(EntityPose(object[0],simple));
  assert(std::abs(simple[0])<.0001f&&std::abs(simple[1]-1)<.0001f&&std::abs(simple[4]+1)<.0001f);
  assert(!OnPickupUpdate(pool,player,nullptr,0)); // cannot collect by walking
  Item item{};assert(LoadItem(pool,player,item));
  assert(item.type==incoming&&item.point[2]>.5f);
  for(int i=0;i<3;++i)assert(std::abs(item.point[i]-renderedPoses[0][12+i])<.0001f);
  std::copy_n(item.point,3,poses[0].gripPos);Step(0);Step(0);
  vehicledoors::MarkerFrame frame{};Append(frame);
  assert(frame.count==1&&frame.epoch==trackingEpoch); // independent of door hint epoch
  const auto p=frame.markers[0].position;const float markerPoint[]{p.x,p.y,p.z};
  for(int i=0;i<3;++i)assert(std::abs(markerPoint[i]-renderedPoses[0][12+i])<.0001f);
  Hold();assert(grantCalls==1&&g_pending.type==incoming&&holdCalls==0);
  Write(player,0x730+slot*0x20,incoming);Write(player,0x8d4,55);Step(1);
  assert(holdCalls==1&&holdType==incoming&&matrixSyncs==frameSyncs);
 }
 // Full CMatrix pickups also need a second RW publication after the VR lift.
 Setup();ItemAt(0,346,0);assert(!OnPickupUpdate(pool,player,nullptr,0));
 assert(matrices[0][14]>.5f&&matrixSyncs==2&&frameSyncs==2);
 assert(std::abs(renderedPoses[0][14]-matrices[0][14])<.0001f);
 // Candidate and icon follow visible pose, not compressed authoring coordinates.
 matrices[0][12]=2.f;Item visible{};assert(LoadItem(pool,player,visible)&&visible.point[0]==2.f);
 Setup();ItemAt(0,346,0);matrices[0][12]=1.5f;matrices[0][14]=.7f;
 Item moved{};assert(LoadItem(pool,player,moved));std::copy_n(moved.point,3,poses[0].gripPos);
 accept=false;Hold();assert(grantCalls==0&&matrices[0][12]==1.5f&&matrices[0][14]==.7f);
 // The actual producer follows progress, stops on every cancellation and only drives the grabbing hand.
 for(int mode=0;mode<6;++mode){
  Setup();ItemAt(0,346,0);Step(0);Step(0);Step(1);
  for(int i=0;i<10;++i)Step(1);
  assert(xr::pickupAmplitudes[0]>.17f&&xr::pickupAmplitudes[0]<.18f&&xr::pickupAmplitudes[1]==0);
  if(mode==0)poses[0].gripPos[0]+=1;
  if(mode==1)usable=false;if(mode==2)owners=1;if(mode==3)++trackingEpoch;if(mode==4)weaponMask=2;
  Step(mode==5?0.f:1.f);assert(xr::pickupAmplitudes[0]==0&&xr::pickupAmplitudes[1]==0);
 }
 Setup();ItemAt(0,346,0);poses[1]=poses[0];poses[0].valid=false;
 Step(0);Step(0);poses[1].grip=1;for(int i=0;i<12;++i)Step(0);
 assert(xr::pickupAmplitudes[0]==0&&xr::pickupAmplitudes[1]>0);
 Reset();assert(xr::pickupAmplitudes[0]==0&&xr::pickupAmplitudes[1]==0);
 Setup();ItemAt(0,346,0);
 assert(!SavrPickupContactAllowed(pool,player)); // new weapon cannot be walked over
 Write(player,0x730+2*0x20,22);assert(SavrPickupContactAllowed(pool,player));
 Write(pool,0x1c,std::int16_t(347));assert(!SavrPickupContactAllowed(pool,player));
 Write(player,0x730+2*0x20,0);Write(pool,0x1c,std::int16_t(346));
 for(int i=0;i<40;++i)Step(1);assert(grantCalls==0);
 Hold();assert(grantCalls==1&&g_pending.hand==0&&holdCalls==0&&collect==0);
 assert(xr::pickupAmplitudes[0]==0&&xr::pickupAmplitudes[1]==0);
 Write(player,0x730+2*0x20,22);Write(player,0x8d4,55);Step(1);
 assert(holdCalls==1&&holdType==22&&g_pending.hand==-1);
 for(int i=0;i<40;++i)Step(1);assert(grantCalls==1&&holdCalls==1);

 // A replacement must wait for its expected type, not grab the old same-slot gun.
 Setup();ItemAt(0,347,0);Write(player,0x730+2*0x20,22);Hold();
 assert(grantCalls==1&&holdCalls==0);
 for(int i=0;i<5;++i)Step(1);assert(holdCalls==0);
 Write(player,0x730+2*0x20,23);Write(player,0x8d4,55);Step(1);
 assert(holdCalls==1&&holdType==23);

 // Blocked, occupied, rejected and released gestures never replace a hand.
 for(int mode=0;mode<6;++mode){
  Setup();ItemAt(0,346,0);
  if(mode==0)weaponMask=2;if(mode==1)owners=1;if(mode==2)usable=false;
  if(mode==3)accept=false;if(mode==4)dropped=true;if(mode==5)Write(player,0x8d4,30);
  Hold();for(int i=0;i<30;++i)Step(1);
  assert(grantCalls==0&&holdCalls==0);
 }
 Setup();ItemAt(0,346,0);Hold();Step(0);Write(player,0x730+2*0x20,22);Write(player,0x8d4,55);
 Step(1);assert(holdCalls==0); // release during delayed delivery cancels hand placement

 // Stable pool identity and nearest-hand arbitration prevent duplicate claims.
 Setup();ItemAt(0,346,0);poses[1]=poses[0];poses[1].gripPos[0]+=.05f;
 grabarbitration::current.Begin(true);ProbeGrabs(poses);FinishProbes();
 assert(g_targets[0].id&&g_targets[1].id==0);
 const auto stale=g_targets[0];Write(pool,0x1e,std::uint16_t(8));assert(!Live(stale));
 Setup();ItemAt(0,346,0);poses[1]=poses[0];poses[1].gripPos[0]+=.05f;
 grabarbitration::current.Begin(true);ProbeGrabs(poses);
 grabarbitration::current.Offer(0,grabarbitration::Kind::Holster,0,.26f);FinishProbes();
 assert(grabarbitration::Allows(0,grabarbitration::Kind::Holster));
 assert(grabarbitration::Allows(1,grabarbitration::Kind::Pickup)&&g_targets[1].id);
 // A competing hand cannot steal an already started hold merely by hovering closer.
 Setup();ItemAt(0,346,0);poses[1]=poses[0];poses[1].gripPos[0]+=.05f;
 grabarbitration::current.Begin(true);ProbeGrabs(poses);
 g_gestures[1].active=true;g_gestures[1].target=g_targets[1].id;FinishProbes();
 assert(!g_targets[0].id&&g_targets[1].id);
 Setup();ItemAt(0,346,0);ItemAt(1,347,.125f);
 grabarbitration::current.Begin(true);ProbeGrabs(poses);assert(g_targets[0].pickup==pool+0x24);

 // Walking over the same type restores both active/desired slots and never ForceHolds.
 Setup();ItemAt(0,346,0);Write(player,0x730+2*0x20,22);
 Write(player,0x8dc,std::uint8_t(5));Write(data,0x2c,std::uint8_t(5));
 assert(OnPickupUpdate(pool,player,nullptr,0));
 assert(Read<std::uint8_t>(player,0x8dc)==5&&Read<std::uint8_t>(data,0x2c)==5&&holdCalls==0);
}
'''
source = OUT/'pickup-runtime.cpp'
source.write_text(harness, encoding='utf8')
compile_run(source, 'pickup-runtime')

# Exercise the production safe hand-off itself, rather than trusting the
# mocked hand-off above to reject occupied hands and stale slot types.
physical = (ROOT/'native/src/PhysicalWeapon.cpp').read_text(encoding='utf8')
start = physical.index('bool HoldCollectedWeapon(')
end = physical.index('\nbool ForceHold(', start)
handoff = r'''
#include "PhysicalWeapon.h"
#include "TrackingFrame.h"
#include <atomic>
#include <cassert>
#include <cstring>
#include <mutex>
namespace savr {
unsigned char ped[0x1000]{};
void* Find(int){return ped;}
struct {void*(*FindPlayerPed)(int)=Find;} g;
namespace physicalweapon {
std::mutex gMutex;
xr::HandPose gHands[2]{};
bool gInteractionsBlocked=false,gGripLatched[2]{},supported=false,dropped=false;
struct Menu{bool open{};} gRadialMenus[2];
std::atomic<unsigned> gHeldMaskRelaxed{0};
int owner=-1,calls=0;
constexpr float kGripOpen=.3f;
bool ValidHand(int h){return h>=0&&h<2;}
bool ValidSlot(int slot){return slot>0&&slot<13;}
int AnyHeldHandLocked(){return owner;}
bool IsSupportHandLocked(int){return supported;}
int DroppedSourceForSlotLocked(int){return dropped?0:-1;}
int WeaponTypeInSlot(void* p,int slot){int value;std::memcpy(&value,static_cast<unsigned char*>(p)+0x730+slot*0x20,4);return value;}
void GiveToHandLocked(void*,int hand,int,const char*){owner=hand;++calls;}
''' + physical[start:end] + r'''
}}
int main(){
 using namespace savr;using namespace savr::physicalweapon;
 int type=22;std::memcpy(ped+0x730+2*0x20,&type,4);
 gHands[0].valid=true;gHands[0].grip=1;
 assert(!HoldCollectedWeapon(0,2,23)); // old weapon still occupies the replacement slot
 owner=1;assert(!HoldCollectedWeapon(0,2,22));owner=0;assert(!HoldCollectedWeapon(0,2,22));owner=-1;
 supported=true;assert(!HoldCollectedWeapon(0,2,22));supported=false;
 dropped=true;assert(!HoldCollectedWeapon(0,2,22));dropped=false;
 gInteractionsBlocked=true;assert(!HoldCollectedWeapon(0,2,22));gInteractionsBlocked=false;
 gRadialMenus[0].open=true;assert(!HoldCollectedWeapon(0,2,22));gRadialMenus[0].open=false;
 gHands[0].valid=false;assert(!HoldCollectedWeapon(0,2,22));gHands[0].valid=true;
 gHands[0].grip=.3f;assert(!HoldCollectedWeapon(0,2,22));gHands[0].grip=1;
 assert(!HoldCollectedWeapon(-1,2,22)&&!HoldCollectedWeapon(2,2,22));
 assert(calls==0&&gHeldMaskRelaxed.load()==0);
 assert(HoldCollectedWeapon(0,2,22));assert(calls==1&&owner==0&&gGripLatched[0]&&gHeldMaskRelaxed.load()==1);
 assert(WeaponTypeInSlot(ped,2)==22);
}
'''
source = OUT/'pickup-handoff.cpp'
source.write_text(handoff, encoding='utf8')
compile_run(source, 'pickup-handoff')
print('Pickup gesture, arbitration and runtime transactions: passed')
