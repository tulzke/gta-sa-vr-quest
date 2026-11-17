"""Exercise production pistol support sockets, aiming and ownership transitions."""
from pathlib import Path
import os
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/pistol-support-tests'
OUT.mkdir(parents=True, exist_ok=True)
physical = (ROOT / 'native/src/PhysicalWeapon.cpp').read_text(encoding='utf8')
calibration = (ROOT / 'native/src/Calib.cpp').read_text(encoding='utf8')


def function(source, signature):
    start = source.index(signature)
    brace = start + re.search(r'\)\s*\{', source[start:]).end() - 1
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


code = r'''
#include "PhysicalWeapon.h"
#include "Calib.h"
#include "HandAnimation.h"
#include "WeaponOptic.h"
#include "GrabArbitration.h"
#include "PairedWeapons.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
using namespace savr;
using savr::physicalweapon::TrackingPose;
constexpr int kHandCount=2,kWeaponSlotCount=13;
#define LOGI(...) ((void)0)
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
bool Near(float a,float b){return std::abs(a-b)<.00001f;}
namespace savr::calib {
'''+function(calibration, 'void ApplyEffectiveSupportDefaults(')+r'''
WeaponCalib current{};
WeaponCalib Snapshot(int,int){return current;}
}
struct V3{float x{},y{},z{};};
'''+physical[physical.index('V3 Add('):physical.index('Q NormalizeQ(')]+r'''
'''+function(physical, 'V3 RotateAroundAxis(')+function(physical, 'V3 PositionOf(')+r'''
struct Hand{bool valid=true;float grip=1,trigger=0,yaw=0;float gripPos[3]{};};
Hand gHands[2];
V3 GripPosition(const Hand& h){return {h.gripPos[0],h.gripPos[1],h.gripPos[2]};}
TrackingPose RawGripFallbackPose(const Hand& h){TrackingPose p;std::memcpy(p.position,h.gripPos,sizeof(p.position));p.right[0]=std::cos(h.yaw);p.right[2]=-std::sin(h.yaw);p.forward[0]=-std::sin(h.yaw);p.forward[2]=-std::cos(h.yaw);return p;}
int gHeldSlot[2]{-1,-1},gSupportHand[2]{-1,-1},gHeldBaseSlot[2]{-1,-1},gHeldVisualSlot[2]{-1,-1};
TrackingPose gHeldBasePose[2],gHeldVisualPose[2];
bool gHeldBaseValid[2]{},gHeldVisualValid[2]{},gGripLatched[2]{},gGripLock{};
bool gInteractionsBlocked{},gPrimaryDetached[2]{};
TrackingPose gDetachedModel[2],gDetachedController[2],gDetachedAnchor[2],gDetachedPrimary[2];
int gPreferredHand=-1,dropCount=0,dropVelocityHand=-1;
double gSupportStartedAt[2]{},now=10;
std::mutex gMutex;
double NowSeconds(){return now;}
int type=22,availableHolster=-1;
void* FindPlayerPed(int){return reinterpret_cast<void*>(1);}
struct{void*(*FindPlayerPed)(int)=::FindPlayerPed;}g;
int WeaponTypeInSlot(void*,int slot){return slot>0?type:0;}
'''+function(physical, 'bool ValidHand(')+function(physical, 'bool ValidSlot(')+function(physical, 'bool IsTwoHandedWeaponTypeInternal(')+function(physical, 'int PrimaryHandForSupportLocked(')+function(physical, 'bool IsSupportHandLocked(')+function(physical, 'void ClearSupportForHandLocked(')+function(physical, 'bool MapSupportOnlyPoseLocked(')+function(physical, 'bool BuildSupportAnchorLocked(')+function(physical, 'bool BuildTwoHandTransformLocked(')+function(physical, 'bool BeginSupportOnlyLocked(')+function(physical, 'bool SinglePointTransfer(')+function(physical, 'int AnyHeldHandLocked(')+function(physical, 'int HeldSlot(')+function(physical, 'int VisualHeldSlot(')+function(physical, 'int FiringHand(')+function(physical, 'float FireTrigger(')+function(physical, 'float SupportAnimationProgress(')+function(physical, 'int LauncherTriggerHand(')+r'''
constexpr float kGripOpen=.30f,kGripClose=.65f,kSupportGrabRadius=.18f,kHolsterReturnRadius=.24f;
namespace savr::holster {int FindPointForSlot(int){return -1;}}
void StartDropLocked(int,int,double,int velocityHand=-1){++dropCount;dropVelocityHand=velocityHand;}
int ClosestAvailableHolsterSlotLocked(void*,int,const float[][3],const bool*){return availableHolster;}
void ClearHeldLocked(int h){ClearSupportForHandLocked(h);gHeldSlot[h]=-1;}
void GiveToHandLocked(void*,int h,int slot,const char*){gHeldSlot[h]=slot;}
void Tick(){void* ped=FindPlayerPed(-1);unsigned handledHands=0;float anchors[7][3]{};bool anchorValid[7]{};
'''+physical[physical.index('        // Keep/release an established support grip first.'):physical.index('        // Explicit hand-to-hand transfer at the final rendered gun body.')]+r'''
}
void Reset(int primary,int weaponType){
 type=weaponType;availableHolster=-1;gGripLock=false;now=10;gPreferredHand=primary;dropCount=0;dropVelocityHand=-1;
 for(int h=0;h<2;++h){gHands[h]=Hand{};gHeldSlot[h]=gSupportHand[h]=gHeldBaseSlot[h]=-1;
  gPrimaryDetached[h]=false;gHeldBaseValid[h]=gHeldVisualValid[h]=gGripLatched[h]=false;gSupportStartedAt[h]=0;}
 gHeldSlot[primary]=gHeldBaseSlot[primary]=2;gHeldBaseValid[primary]=true;
 auto& model=gHeldBasePose[primary];model=TrackingPose{};
 model.right[0]=1;model.forward[1]=1;model.forward[2]=0;
 model.up[1]=0;model.up[2]=1;gHeldVisualPose[primary]=model;gHeldVisualSlot[primary]=2;gHeldVisualValid[primary]=true;
 calib::current={};calib::ApplyEffectiveSupportDefaults(calib::current,false,type);
}
int main(){
 for(int t:{35,36})for(int primary:{0,1}){
  Reset(primary,t);const int support=1-primary;
  CHECK(LauncherTriggerHand(primary)==-1);
  gSupportHand[primary]=support;CHECK(LauncherTriggerHand(primary)==support);
  gHands[support].valid=false;CHECK(LauncherTriggerHand(primary)==-1);gHands[support].valid=true;
  gHands[primary].valid=false;CHECK(LauncherTriggerHand(primary)==-1);gHands[primary].valid=true;
  gHeldSlot[support]=4;CHECK(LauncherTriggerHand(primary)==-1);gHeldSlot[support]=-1;
  gInteractionsBlocked=true;CHECK(LauncherTriggerHand(primary)==-1);gInteractionsBlocked=false;
  CHECK(LauncherTriggerHand(primary)==support);
 }
 for(int t=0;t<48;++t){
  bool supported=(t>=22&&t<=38)||t==43;
  CHECK(IsTwoHandedWeaponTypeInternal(t)==supported);
 }
 calib::WeaponCalib longGun;calib::ApplyEffectiveSupportDefaults(longGun,false,30);
 CHECK(longGun.supOffY==60&&longGun.supOffZ==-10&&longGun.supRotY==360);
 for(int primary : {0,1}) {
  Reset(primary,43);
  gSupportHand[primary]=1-primary;
  TrackingPose anchor{};
  CHECK(BuildSupportAnchorLocked(primary,&anchor));
  CHECK(anchor.position[0]>.06f&&anchor.position[1]<-.03f);
  CHECK(calib::current.supStyle==calib::SUPPORT_FROM_BELOW);
  V3 pivot,axis;float angle=-1;
  std::copy_n(anchor.position,3,gHands[1-primary].gripPos);
  CHECK(BuildTwoHandTransformLocked(primary,pivot,axis,angle)&&Near(angle,0));
  gHands[1-primary].gripPos[2]+=.05f;
  CHECK(BuildTwoHandTransformLocked(primary,pivot,axis,angle)&&angle>.1f);
  const V3 snapped=Add(pivot,RotateAroundAxis(Sub(PositionOf(anchor),pivot),axis,angle));
  CHECK(Dot(Normalized(Sub(snapped,pivot)),
            Normalized(Sub(GripPosition(gHands[1-primary]),pivot)))>.999f);
  TrackingPose currentModel=gHeldBasePose[primary];
  currentModel.position[2]+=.05f;
  CHECK(BuildTwoHandTransformLocked(primary,pivot,axis,angle,&currentModel)&&Near(angle,0));
 }
 calib::WeaponCalib edited;edited.supOffX=17;calib::ApplyEffectiveSupportDefaults(edited,true,22);
 CHECK(edited.supOffX==17&&edited.supOffY==0&&edited.supRotX==0);
 for(int t:{28,32}){
  calib::WeaponCalib legacy;legacy.supOffY=60;legacy.supOffZ=-10;legacy.supRotY=360;
  calib::ApplyEffectiveSupportDefaults(legacy,true,t);
  CHECK(legacy.supOffY==(t==32?44:2)&&legacy.supRotX==-180);
  legacy.supOffX=17;calib::ApplyEffectiveSupportDefaults(legacy,true,t);
  CHECK(legacy.supOffX==17); // custom support is retained
 }
 calib::WeaponCalib tecHd;tecHd.supRotY=360;
 calib::ApplyEffectiveSupportDefaults(tecHd,true,32);
 CHECK(tecHd.supOffY==44&&tecHd.supOffX==8&&tecHd.supRotY==-270);
 calib::WeaponCalib oldTec;calib::ApplyEffectiveSupportDefaults(oldTec,false,22);
 calib::ApplyEffectiveSupportDefaults(oldTec,true,32);CHECK(oldTec.supOffY==44);
 for(int t:{22,23,24,28,32})for(int primary:{0,1}){
  Reset(primary,t);const int support=1-primary;TrackingPose anchor{};
  CHECK(BuildSupportAnchorLocked(primary,&anchor));
  if(t==32) {
   CHECK(Near(anchor.position[2],primary==0?-.04f:.04f));
   CHECK(Near(anchor.position[0],.22f)&&Near(anchor.position[1],-.015f));
   const float diagonal=std::sqrt(.5f);
   CHECK(Near(std::abs(anchor.forward[0]),diagonal));
   CHECK(Near(std::abs(anchor.forward[2]),diagonal)); // diagonal fore-end wrist
  } else CHECK(Length(PositionOf(anchor))<.05f); // pistol / Micro Uzi grip
  CHECK(Near(Length({anchor.right[0],anchor.right[1],anchor.right[2]}),1));
  CHECK(Near(Length({anchor.up[0],anchor.up[1],anchor.up[2]}),1));
  CHECK(Near(Length({anchor.forward[0],anchor.forward[1],anchor.forward[2]}),1));
  std::memcpy(gHands[support].gripPos,anchor.position,sizeof(anchor.position));
  availableHolster=4;Tick();CHECK(gSupportHand[primary]==-1); // reaching a holster wins
  availableHolster=-1;gHands[support].gripPos[0]+=1;Tick();CHECK(gSupportHand[primary]==-1);
  std::memcpy(gHands[support].gripPos,anchor.position,sizeof(anchor.position));
  gHeldSlot[support]=4;Tick();CHECK(gSupportHand[primary]==-1); // occupied hand
  gHeldSlot[support]=-1;Tick();CHECK(gSupportHand[primary]==support);
  CHECK(gHeldSlot[primary]==2&&gHeldSlot[support]==-1); // single inventory owner
  CHECK(Near(SupportAnimationProgress(primary),0));now+=.09;
  CHECK(Near(SupportAnimationProgress(primary),.5f));now+=1;
  CHECK(Near(SupportAnimationProgress(primary),1));
  V3 pivot{},axis{};float angle=99;
  for(float distance:{0.f,.000001f,.5f,-.5f}){
   gHands[support].gripPos[0]=distance;
   CHECK(BuildTwoHandTransformLocked(primary,pivot,axis,angle));
   CHECK(Near(Length(axis),1));
   if(t==32) CHECK(angle>0&&angle<=1.570797f);
   else CHECK(angle==0); // pistol/Uzi aim remains primary-controller driven
  }
  gHands[support].grip=0;Tick();CHECK(gSupportHand[primary]==-1&&gHeldSlot[primary]==2);
  CHECK(SupportAnimationProgress(primary)==0);
  gGripLatched[support]=false;gHands[support].grip=1;
  std::memcpy(gHands[support].gripPos,anchor.position,sizeof(anchor.position));Tick();
  gHands[primary].grip=0;Tick();
  if(t==32){CHECK(gPrimaryDetached[primary]&&gHeldSlot[primary]==2&&gHeldSlot[support]==-1);}
  else {CHECK(gHeldSlot[support]==2&&gHeldSlot[primary]==-1);CHECK(gSupportHand[primary]==-1);} // support promoted on primary release
 }

 for(int primary:{0,1}) {
  Reset(primary,25);const int support=1-primary;TrackingPose anchor{};
  CHECK(BuildSupportAnchorLocked(primary,&anchor));
  std::memcpy(gHands[support].gripPos,anchor.position,sizeof(anchor.position));Tick();
  for(float offset:{0.f,-.12f,-.35f,.20f}) {
   for(int a=0;a<3;++a)gHands[support].gripPos[a]=anchor.position[a]+gHeldBasePose[primary].right[a]*offset;
   V3 pivot{},axis{};float angle{};CHECK(BuildTwoHandTransformLocked(primary,pivot,axis,angle));
   CHECK(std::abs(angle)<.0001f); // rail motion cannot steer or pitch the gun
  }
 }
 for(int t:{25,26,27,29,30,31,32,33,34,35,36,37,38})for(int primary:{0,1}){
  Reset(primary,t);const int support=1-primary;TrackingPose anchor{};
  CHECK(BuildSupportAnchorLocked(primary,&anchor));
  std::memcpy(gHands[support].gripPos,anchor.position,sizeof(anchor.position));Tick();
  CHECK(gSupportHand[primary]==support);
  const auto originalModel=gHeldVisualPose[primary];const auto originalSupport=GripPosition(gHands[support]);
  gHands[primary].trigger=1;gHands[primary].grip=0;Tick();
  CHECK(gPrimaryDetached[primary]&&gHeldSlot[primary]==2&&gHeldSlot[support]==-1);
  CHECK(HeldSlot(primary)==-1&&VisualHeldSlot(primary)==2);
  CHECK(FiringHand()==-1&&FireTrigger()==0&&LauncherTriggerHand(primary)==-1);
  TrackingPose model{},pinned{};CHECK(MapSupportOnlyPoseLocked(primary,gDetachedModel[primary],&model));
  CHECK(Near(Distance(PositionOf(model),PositionOf(originalModel)),0));
  CHECK(BuildSupportAnchorLocked(primary,&pinned));CHECK(Near(Distance(PositionOf(pinned),PositionOf(anchor)),0));
  // The released controller can move without steering the weapon or support.
  gHands[primary].gripPos[0]=3;gHands[primary].gripPos[1]=2;Tick();
  CHECK(MapSupportOnlyPoseLocked(primary,gDetachedModel[primary],&model));
  CHECK(Near(Distance(PositionOf(model),PositionOf(originalModel)),0));
  // Support motion preserves the complete relative model transform, including rotation.
  gHands[support].gripPos[0]+=.3f;gHands[support].gripPos[1]+=.1f;gHands[support].gripPos[2]-=.2f;
  gHands[support].yaw=.5f;
  CHECK(MapSupportOnlyPoseLocked(primary,gDetachedModel[primary],&model));
  const auto delta=Sub(PositionOf(originalModel),originalSupport);
  const V3 expected=Add(GripPosition(gHands[support]),{std::cos(.5f)*delta.x+std::sin(.5f)*delta.z,delta.y,-std::sin(.5f)*delta.x+std::cos(.5f)*delta.z});
  CHECK(Near(Distance(PositionOf(model),expected),0));
  CHECK(Near(model.right[0],std::cos(.5f))&&Near(model.right[2],-std::sin(.5f)));
  // Closing away from the original primary handle cannot reattach it.
  gHands[primary].grip=1;gGripLatched[primary]=false;Tick();CHECK(gPrimaryDetached[primary]);
  TrackingPose target{};CHECK(MapSupportOnlyPoseLocked(primary,gDetachedPrimary[primary],&target));
  std::memcpy(gHands[primary].gripPos,target.position,sizeof(target.position));
  gGripLatched[primary]=true;Tick();CHECK(gPrimaryDetached[primary]);
  gHands[primary].grip=0;Tick();gGripLatched[primary]=false;gHands[primary].grip=1;Tick();
  CHECK(!gPrimaryDetached[primary]&&HeldSlot(primary)==2&&gSupportHand[primary]==support);
  CHECK(FiringHand()==primary);if(t<35||t>36)CHECK(FireTrigger()==1);
  // Release again, then release support: drop from that contact, never promote.
  gHeldVisualPose[primary]=model;gHeldBasePose[primary]=model;
  gHands[primary].grip=0;Tick();CHECK(gPrimaryDetached[primary]);
  gHands[support].grip=0;Tick();CHECK(gHeldSlot[primary]==-1&&gHeldSlot[support]==-1);
  CHECK(dropCount==1&&dropVelocityHand==support);
  // Simultaneous primary release / fresh support close uses the same state.
  Reset(primary,t);CHECK(BuildSupportAnchorLocked(primary,&anchor));
  std::memcpy(gHands[support].gripPos,anchor.position,sizeof(anchor.position));
  gHands[primary].grip=0;Tick();CHECK(gPrimaryDetached[primary]&&gHeldSlot[support]==-1);
 }
 Reset(1,30);gSupportHand[1]=0;TrackingPose anchor{};CHECK(BuildSupportAnchorLocked(1,&anchor));
 gHands[0].gripPos[0]=1;gHands[0].gripPos[1]=0;gHands[0].gripPos[2]=0;
 V3 pivot,axis;float angle;CHECK(BuildTwoHandTransformLocked(1,pivot,axis,angle));
 CHECK(angle>.1f); // long-gun two-controller aiming retained
 for(int primary:{0,1}){
  Reset(primary,32);gSupportHand[primary]=1-primary;
  // Use a non-default, user-adjusted socket; aiming must not rewrite it.
  calib::current.supOffX=11.4f;calib::current.supOffY=41.2f;
  calib::current.supRotY=-263.4f;
  TrackingPose before{},after{};CHECK(BuildSupportAnchorLocked(primary,&before));
  gHands[1-primary].gripPos[0]=.4f;gHands[1-primary].gripPos[1]=.1f;
  V3 tecPivot,tecAxis;float tecAngle;
  CHECK(BuildTwoHandTransformLocked(primary,tecPivot,tecAxis,tecAngle));CHECK(tecAngle>.1f);
  CHECK(BuildSupportAnchorLocked(primary,&after));
  CHECK(std::memcmp(&before,&after,sizeof(before))==0);
  type=30; // identical calibrated socket/input -> exactly the AK transform
  CHECK(BuildTwoHandTransformLocked(primary,pivot,axis,angle));
  CHECK(Near(angle,tecAngle)&&Near(Distance(axis,tecAxis),0)&&Near(Distance(pivot,tecPivot),0));
 }
 std::puts("PASS pistol support: eligibility, sockets, defaults, ownership, release/promotion, timing, stable aim; long guns retained");
}
'''
source = OUT / 'pistol_support.cpp'
source.write_text(code, encoding='utf8')
exe = OUT / ('test.exe' if os.name == 'nt' else 'test')
if os.name == 'nt':
    where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    vc = Path(base) / 'VC/Auxiliary/Build/vcvars64.bat'
    script = OUT / 'test.cmd'
    script.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\n'
        f'cl /nologo /std:c++20 /EHsc /W4 /D_CRT_SECURE_NO_WARNINGS /I"{ROOT / "native/src"}" "{source}" '
        f'/Fe:"{exe}" /Fo:"{OUT / "test.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n', encoding='utf8')
    subprocess.run(['cmd', '/c', str(script)], cwd=OUT, check=True)
else:
    subprocess.run([shutil.which('c++'), '-std=c++20', '-Wall', '-Wextra',
        '-I'+str(ROOT/'native/src'), str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
