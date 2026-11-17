"""Run production holster maths and extracted gameplay branches with synthetic poses."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build' / 'holster-tests'
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

compile_run(ROOT / 'tools/tests/holsters.cpp', 'holster-maths')

# Compile the actual ownership/marker branches, mocking only engine and pose
# access. This keeps a test from silently implementing a second copy of policy.
physical = (ROOT / 'native/src/PhysicalWeapon.cpp').read_text(encoding='utf8')
def function(signature, source=physical):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

release = physical.split('// Release returns at the matching socket, otherwise creates a flying copy.')[1].split('// Fresh close near the closest available configured socket grabs that slot.')[0]
grab = physical.split('// Fresh close near the closest available configured socket grabs that slot.')[1].split('\n    for (int hand = 0; hand < kHandCount; ++hand) {')[0]
# The final brace belongs to Update's !interactionsBlocked guard, outside the
# extracted test function. Keep both transition loops and strip only that brace.
transitions = release + grab
transitions = transitions.rstrip()
assert transitions.endswith('}')
transitions = transitions[:-1]
markers = physical[physical.index('    holstermarker::Marker markers['):physical.index('        // Only calibration exposes the model-bound foregrip cube.')]
prefix = r'''
#include "HolsterMarker.h"
#include "GrabArbitration.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace savr;
std::uint64_t trackingEpoch=1;
namespace savr::xr {void SetAmmoHolsters(const holstermarker::Frame&) {}}
#define LOGI(...) ((void)0)
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
constexpr int kHandCount=2;
struct Vec {float x{},y{},z{};};
struct Hand {bool valid{true};float grip{};Vec pos{};};
struct Drop {int slot{-1};};
Hand gHands[2]; Drop gDropped[2]; int gHeldSlot[2]={-1,-1};
bool gGripLatched[2]{},gAwaitFirstGrip[2]{},gGripLock{},gDualHoldEnabled=true;
bool enabled=true,interactionsBlocked=false,showHolsterMarkersWhileBlocked=false,showSupportMarkerWhileBlocked=false;
bool vehicleImmersive=false,scriptedGunner=false;
struct {bool grabbed[2]{}, ignitionGrabbed[2]{}, radioGrabbed[2]{};} seatWheel;
float anchors[7][3]{};bool anchorValid[7]={true,true,true,true,true,true,true};
int slots[7]={1,2,3,4,5,6,7}, visibility[7]{};
int types[13]={0,1,2,3,4,5,6,7,8,9,10,11,12};
namespace savr::holster {
constexpr int POINT_COUNT=7;
int PointCount(){return 7;} int PointSlot(int p){return slots[p];}
bool PointVisible(int p){return visibility[p]==0;}
bool PointGrabbable(int p){return visibility[p]!=2;}
bool GripMarkersEnabled(){return enabled;}
float GrabRadiusMetres(){return 0.18f;}
int FindPointForSlot(int slot){for(int p=0;p<7;++p)if(slots[p]==slot)return p;return -1;}
const char* PointName(int){return "test";}
}
bool ValidSlot(int s){return s>0&&s<13;}
bool ValidHand(int h){return h>=0&&h<2;}
int WeaponTypeInSlot(void*,int s){return ValidSlot(s)?types[s]:0;}
bool VehicleSidearmWeaponType(int t){return t==2;}
bool IsSupportHandLocked(int){return false;}
Vec GripPosition(Hand h){return h.pos;}
float Distance(Vec a,Vec b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));}
void* Vehicle(int,bool){return vehicleImmersive?reinterpret_cast<void*>(1):nullptr;}
struct {void*(*FindPlayerVehicle)(int,bool)=Vehicle;} g;
namespace savr::vrfire {bool IsScriptedWeaponCamera(){return scriptedGunner;}}
void StartDropLocked(int h,int s,double){gDropped[h].slot=s;}
void ClearHeldLocked(int h){gHeldSlot[h]=-1;}
void GiveToHandLocked(void*,int h,int s,const char*){gHeldSlot[h]=s;}
'''
constants = '\n'.join(line for line in physical.splitlines() if line.startswith('constexpr float ') and any(name in line for name in ('kGripClose ', 'kGripOpen ', 'kHolsterReturnRadius ')))
production = '\n'.join(function(sig) for sig in ('bool OwnedConfiguredSlot(', 'int HeldHandForSlotLocked(', 'std::uint32_t OccupiedSlotMaskLocked(', 'int AnyHeldHandLocked(', 'int ClosestAvailableHolsterSlotLocked('))
# AnyHeldHand reads only a preferred hand in addition to the two owners.
prefix += '\nint gPreferredHand=-1;\n'
transition_fn = 'void Transition(){void* ped=nullptr;double now=1;std::uint32_t handledHands=0;\n' + transitions + '\n}\n'
marker_fn = 'int Publish(holstermarker::Marker out[8]) {void* ped=nullptr;\n' + markers + '\n    }\nstd::memcpy(out,markers,sizeof(markers));return markerCount;\n}\n'
tests = r'''
int main(){
    for(int p=0;p<7;++p)anchors[p][0]=float(p);
    holstermarker::Marker m[8];CHECK(Publish(m)==0);
    slots[6]=-1; gHands[0].pos={6,0,0}; CHECK(Publish(m)==1);
    CHECK(m[0].kind==holstermarker::Kind::Empty && m[0].opacity>.34f);
    gHands[0].pos={0,0,0}; CHECK(Publish(m)==1);
    CHECK(m[0].opacity==.15f); slots[6]=7;
    // Both hands grab different sockets using the real nearest-available filter.
    for(int h=0;h<2;++h){gHands[h].grip=1;gHands[h].pos={float(h),0,0};}
    Transition();CHECK(gHeldSlot[0]==1&&gHeldSlot[1]==2);
    CHECK(Publish(m)==2);
    CHECK(m[0].kind==holstermarker::Kind::Return && m[1].kind==holstermarker::Kind::Return);
    // Hand-to-hand ownership state retains the same return point and count.
    gHeldSlot[1]=-1;gHeldSlot[0]=2;
    CHECK(Publish(m)==1 && m[0].kind==holstermarker::Kind::Return);
    // Release into its own socket returns; a different socket is a drop.
    gHands[0].pos={1,0,0};gHands[0].grip=0;Transition();CHECK(gHeldSlot[0]==-1&&gDropped[0].slot==-1);
    CHECK(Publish(m)==0);
    gHeldSlot[0]=2;gHands[0].pos={0,0,0};Transition();CHECK(gDropped[0].slot==2);
    CHECK(Publish(m)==0);
    gDropped[0].slot=-1;CHECK(Publish(m)==0); // expired/caught drop is distinct from held
    // Grip lock retains a gun away from the socket, but permits return nearby.
    gHeldSlot[0]=2;gGripLock=true;Transition();CHECK(gHeldSlot[0]==2);
    gHands[0].pos={1,0,0};visibility[1]=2;Transition();CHECK(gHeldSlot[0]==-1);
    CHECK(Publish(m)==0); // OFF hides the marker but retains the existing return contract
    visibility[0]=1;gHeldSlot[0]=1;CHECK(Publish(m)==0); // HIDE also hides return hints
    enabled=false;CHECK(Publish(m)==0);
    interactionsBlocked=true;showHolsterMarkersWhileBlocked=true;enabled=true;
    slots[6]=-1;types[4]=0;CHECK(Publish(m)==7); // preview includes empty/hidden/occupied sockets
    enabled=false;CHECK(Publish(m)==0);
    // No duplicate grab of a slot already in either hand or in flight.
    enabled=true;interactionsBlocked=false;showHolsterMarkersWhileBlocked=false;
    slots[6]=7;types[4]=4;visibility[0]=visibility[1]=0;
    gHeldSlot[0]=1;gHeldSlot[1]=-1;gHands[1].pos={0,0,0};gHands[1].grip=1;gGripLatched[1]=false;
    gHands[0].grip=1;Transition();CHECK(gHeldSlot[1]==-1);
    gHeldSlot[0]=-1;gDropped[0].slot=1;gGripLatched[1]=false;Transition();CHECK(gHeldSlot[1]==-1);
    // Invalid socket cannot be grabbed and cannot produce a marker.
    anchorValid[0]=false;gDropped[0].slot=-1;gGripLatched[1]=false;Transition();CHECK(gHeldSlot[1]==-1);
    CHECK(Publish(m)==0);
    std::puts("PASS: production grab/release and marker branches, both owners, return vs drop, lock, visibility and calibration");
}
'''
generated = OUT / 'gameplay.cpp'
generated.write_text(prefix + constants + '\n' + production + '\n' + transition_fn + marker_fn + tests, encoding='utf8')
compile_run(generated, 'holster-gameplay')

camera = (ROOT / 'native/src/VrCamera.cpp').read_text(encoding='utf8')
holsters = (ROOT / 'native/src/Holster.cpp').read_text(encoding='utf8')
metadata = holsters[holsters.index('const PointMetadata kMetadata['):holsters.index('std::atomic<int> gPointSlots')]
maths = camera[camera.index('struct Q  {'):camera.index('// --- reading / writing the game camera matrix')]
rw = camera[camera.index('struct RwMat {'):camera.index('// The yaw-only BODY camera basis')]
camera_prefix = r'''
#include "HolsterBody.h"
#include "Holster.h"
#include "Calib.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace savr;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct V3 {float x{},y{},z{};};
bool bodyEnabled=true;
calib::HolsterCalib calibration{};
namespace savr::calib {HolsterCalib SnapshotHolster(int){return calibration;}}
namespace savr::holster {
'''+metadata+r'''
const PointMetadata* Metadata(int i){return i>=0&&i<POINT_COUNT?&kMetadata[i]:nullptr;}
bool BodyFrameEnabled(){return bodyEnabled;}
}
'''+maths+r'''
struct CamBasis {V3 right,forward,up,pos;};
CamBasis g_vrBase;bool g_vrBaseValid=true;
struct Origin {bool valid{true};V3 pos;Q ori;}; Origin g_origin;
holsterbody::Body g_holsterBody;
std::uint64_t g_cameraTrackingEpoch=1,g_cameraResetSerial=0;
'''+rw
camera_production='\n'.join(function(sig,camera) for sig in (
    'V3 TrackingToGame(', 'bool Finite(V3', 'V3 GameVectorToTracking(',
    'V3 TrackingPointToGame(V3 localPoint) {', 'V3 TrackingVectorToGame(V3 localVector) {',
    'void SetMatBasis(', 'bool HolsterWeaponMatrix(', 'void UpdateHolsterBodyTracking(',
    'bool GetHolsterAnchorTracking('))
camera_tests = r'''
void Close(float a,float b){CHECK(std::fabs(a-b)<0.001f);}
int main(){
    constexpr float pi=holsterbody::kPi;
    g_origin.pos={-2,1.7f,3};g_origin.ori={0,std::sin(pi/8),0,std::cos(pi/8)};
    const float head[3]={-1,1.4f,4};
    const Q yaw{0,std::sin(pi/3),0,std::cos(pi/3)};
    const Q pitch{std::sin(pi/8),0,0,std::cos(pi/8)};
    const Q roll{0,0,std::sin(pi/10),std::cos(pi/10)};
    const Q q=Multiply(yaw,Multiply(pitch,roll));
    const float ori[4]={q.x,q.y,q.z,q.w};
    for(int step=0;step<4;++step){
        const float a=step*pi/2;
        g_vrBase={{-std::cos(a),-std::sin(a),0},{-std::sin(a),std::cos(a),0},{0,0,1},{100,200,10}};
        UpdateHolsterBodyTracking(head,ori,true,1,true,false,false,step/90.0);
        Close(g_holsterBody.yaw,2*pi/3); // pitch and roll do not tip the belt
        for(int p=0;p<holster::POINT_COUNT;++p){
            float anchor[3];RwMat model;
            CHECK(GetHolsterAnchorTracking(p,anchor));CHECK(HolsterWeaponMatrix(p,22,model));
            const V3 tracking=g_origin.pos+GameVectorToTracking(V3{model.t[0],model.t[1],model.t[2]}-g_vrBase.pos);
            Close(tracking.x,anchor[0]);Close(tracking.y,anchor[1]);Close(tracking.z,anchor[2]);
        }
    }
    float before[3],after[3];RwMat offset;
    CHECK(GetHolsterAnchorTracking(0,before));
    calibration.offX=20;calibration.rotY=30;
    CHECK(HolsterWeaponMatrix(0,22,offset));CHECK(GetHolsterAnchorTracking(0,after));
    for(int axis=0;axis<3;++axis)Close(before[axis],after[axis]); // model calibration never moves reach
    const V3 calibrated=g_origin.pos+GameVectorToTracking(V3{offset.t[0],offset.t[1],offset.t[2]}-g_vrBase.pos);
    CHECK(Dot(calibrated-V3{before[0],before[1],before[2]},calibrated-V3{before[0],before[1],before[2]})>0.009f);
    calibration={};bodyEnabled=false;
    UpdateHolsterBodyTracking(head,ori,true,1,true,false,false,0.1);
    CHECK(!g_holsterBody.bodyMode);
    const auto* meta=holster::Metadata(0);
    const V3 legacy=g_origin.pos+Rotate(g_origin.ori,{meta->lateral,meta->vertical,-meta->depth});
    CHECK(GetHolsterAnchorTracking(0,after));Close(after[0],legacy.x);Close(after[1],legacy.y);Close(after[2],legacy.z);
    // Wrong epoch / unavailable camera must suppress both render and interaction.
    UpdateHolsterBodyTracking(head,ori,true,2,true,false,false,0.11);
    CHECK(!GetHolsterAnchorTracking(0,after));CHECK(!HolsterWeaponMatrix(0,22,offset));
    std::puts("PASS: production camera transforms, seven sockets, physical/stick yaw, pitch/roll, calibration, legacy and epoch gating");
}
'''
generated=OUT/'camera.cpp'
generated.write_text(camera_prefix+camera_production+camera_tests,encoding='utf8')
compile_run(generated,'holster-camera')

globals_text=holsters[holsters.index('constexpr int kDefaultSlots['):holsters.index('std::atomic<bool> gCalibrationPreviewActive')]
globals_text=globals_text.replace('/sdcard/Android/data/com.rockstargames.gtasa/files/vr_holsters.ini',str(OUT/'settings.ini').replace('\\','/'))
settings_prefix=r'''
#include "Holster.h"
#include "PairedWeapons.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define LOGI(...) ((void)0)
#define LOGW(...) ((void)0)
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace savr::xr {void SetHolsterMarkers(const float (*)[3],int){}}
namespace savr::holster {
alignas(16) unsigned char pairPed[0xa00]{},pairInfo[0x70]{};
void SaveSettingsLocked();
int testSkill=1;
void* PairPed(int){return pairPed;}
int PairSkill(void*,int){return testSkill;}
void* PairInfo(int,signed char){return pairInfo;}
struct {void*(*FindPlayerPed)(int)=PairPed;int(*CPed_GetWeaponSkill)(void*,int)=PairSkill;
void*(*CWeaponInfo_GetWeaponInfo)(int,signed char)=PairInfo;} g;
const char* PointName(int){return "test";}
bool VehicleSidearmWeaponType(int type){return type==22;}
'''+globals_text
settings_production='\n'.join(function(sig,holsters) for sig in (
    'bool ValidPoint(', 'bool SelectableSlot(', 'void AssignPointSlotLocked(', 'int PointCount()', 'int PointSlot(', 'int FindPointForSlot(', 'const char* SlotName(', 'bool SetPointSlot(', 'void LoadSettings(',
    'void SaveSettingsLocked(', 'void Init()', 'bool BodyFrameEnabled()', 'void SetBodyFrameEnabled('))
settings_production+='\n'+function('bool PairAllowed(')+'\n'+function('bool RadialUnavailable(')
settings_tests=r'''
void Write(const char* text){FILE* f=std::fopen(kSettingsPath,"w");CHECK(f);std::fputs(text,f);std::fclose(f);}
}
int main(){
    using namespace savr::holster;
    std::remove(kSettingsPath);Init();CHECK(!BodyFrameEnabled());
    // Retired movement/idle settings cannot restore unlimited pauses or alter
    // the one-second rule, and disappear on the next settings save.
    Write("body_frame 1\nbody_reach_idle_ms 0\nbody_reach_motion_cm 1\n");LoadSettings();
    CHECK(BodyFrameEnabled());SaveSettingsLocked();
    {char line[128];FILE* f=std::fopen(kSettingsPath,"r");CHECK(f);
     while(std::fgets(line,sizeof(line),f)) CHECK(std::strstr(line,"body_reach_")==nullptr);
     std::fclose(f);}
    Write("grip_markers 0\ngrab_radius_cm 23\ngrip_lock 1\nvisible 0 0\np 0 4\n");
    LoadSettings();CHECK(!BodyFrameEnabled());CHECK(!gGripMarkersEnabled);CHECK(gGrabRadiusCm==23);CHECK(gGripLockEnabled);CHECK(gPointState[0]==1);
    SetBodyFrameEnabled(true);CHECK(BodyFrameEnabled());
    gBodyFrameEnabled=false;LoadSettings();CHECK(BodyFrameEnabled());CHECK(gPointState[0]==1&&gGrabRadiusCm==23);
    SetBodyFrameEnabled(false);gBodyFrameEnabled=true;LoadSettings();CHECK(!BodyFrameEnabled());
    Write("body_frame 99\nstate 0 2\n");LoadSettings();CHECK(!BodyFrameEnabled());CHECK(gPointState[0]==2);
    Write("body_frame 1\nbody_frame 0\n");LoadSettings();CHECK(!BodyFrameEnabled());
    Write("left_handed 1\np 0 12\np 1 8\np 2 3\np 3 1\n");LoadSettings();
    CHECK(gLeftHanded);CHECK(gPointSlots[0]==12&&gPointSlots[1]==8);
    CHECK(gPointSlots[2]==3&&gPointSlots[3]==1);
    SaveSettingsLocked();gLeftHanded=false;LoadSettings();
    CHECK(gLeftHanded&&gPointSlots[0]==12&&gPointSlots[1]==8&&gPointSlots[2]==3);
    Write("p 0 12\np 1 12\n");LoadSettings();CHECK(gPointSlots[0]==12&&gPointSlots[1]==-1);
    Write("p 0 2\np 1 4\n");LoadSettings();
    *reinterpret_cast<int*>(pairPed+0x730+2*0x20)=22;
    *reinterpret_cast<unsigned*>(pairInfo+0x18)=0x800;
    CHECK(RadialUnavailable(pairPed,15,false));
    CHECK(!SetPointSlot(1,15));CHECK(PointSlot(0)==2 && PointSlot(1)==4);
    testSkill=2;CHECK(!RadialUnavailable(pairPed,15,false));
    CHECK(SetPointSlot(1,15));CHECK(PointSlot(0)==2 && PointSlot(1)==15);
    SaveSettingsLocked();LoadSettings();CHECK(PointSlot(0)==2 && PointSlot(1)==15);
    CHECK(FindPointForSlot(2)==0 && FindPointForSlot(15)==1);
    CHECK(!RadialUnavailable(pairPed,15,true));
    testSkill=1;CHECK(RadialUnavailable(pairPed,15,false));
    CHECK(!SetPointSlot(1,15));CHECK(PointSlot(0)==2 && PointSlot(1)==15);
    testSkill=2;*reinterpret_cast<int*>(pairPed+0x730+2*0x20)=23;
    CHECK(RadialUnavailable(pairPed,15,false));CHECK(!SetPointSlot(1,15));
    *reinterpret_cast<int*>(pairPed+0x730+2*0x20)=22;
    CHECK(SetPointSlot(0,-1));CHECK(PointSlot(0)==-1 && PointSlot(1)==2);
    Write("p 0 15\np 1 4\n");LoadSettings();CHECK(PointSlot(0)==2 && PointSlot(1)==4);
    std::puts("PASS: production settings, paired wheel skill/vehicle gates, rejection without changes, paired persistence and removing primary");
}
'''
generated=OUT/'settings.cpp'
generated.write_text(settings_prefix+settings_production+settings_tests,encoding='utf8')
compile_run(generated,'holster-settings')

# Exercise the production proximity/timer wiring and both production button
# latch loops. Rendering/inventory/hand poses are mocked; timer and torso are real.
reach_begin = physical.index('    // Test reach against last tick\'s socket')
reach_end = physical.index('    float anchors[holster::POINT_COUNT][3]{};', reach_begin)
reach = physical[reach_begin:reach_end]
trigger_begin = physical.index('    const bool radialUsable =', reach_end)
trigger_end = physical.index('    unsigned int radialHandledHands = 0;', trigger_begin)
trigger_latches = physical[trigger_begin:trigger_end]
grip_latches = function('for (int hand = 0; hand < kHandCount; ++hand) {\n        if (interactionsBlocked)')
reach_prefix = r'''
#include "HolsterBody.h"
#include "RadialHolsterMenu.h"
#include <cstdio>
#include <cstdlib>
using namespace savr;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct V3 {float x{},y{},z{};};
struct Hand {bool valid{true};V3 pos;float grip{},trigger{};} gHands[2];
V3 GripPosition(const Hand& h){return h.pos;}
float Distance(V3 a,V3 b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));}
constexpr int kHandCount=2;
'''+constants+'\n'+next(line for line in physical.splitlines() if line.startswith('constexpr float kGripBlockedLatch '))+r'''
holsterbody::ReachIntent gReachIntent[2];holsterbody::Body body;
radialholster::Menu gRadialMenus[2];
int gHeldSlot[2]{-1,-1},slots[2]{2,4};
bool gGripLatched[2]{},gRadialTriggerLatched[2]{};
bool interactionsBlocked{},inVehicle{},vehicleImmersive{},headValid=true,bodyEnabled=true,ground=true,focused=true;
bool grabbable[2]{true,true},support[2]{};
unsigned doorHands{},basketballHands{};
struct {bool grabbed[2]{},ignitionGrabbed[2]{},radioGrabbed[2]{};}seatWheel;
float headPosition[3]{},headOrientation[4]{0,0,0,1};std::uint64_t trackingEpoch=1;
bool ValidSlot(int s){return s>=0&&s<26;}
bool IsSupportHandLocked(int h){return support[h];}
namespace savr::holster {
int PointCount(){return 2;}int PointSlot(int p){return slots[p];}
bool PointGrabbable(int p){return grabbable[p];}bool BodyFrameEnabled(){return bodyEnabled;}
float GrabRadiusMetres(){return .18f;}
}
namespace savr::basketball {unsigned HeldHandMask(){return basketballHands;}}
namespace savr::xr {bool IsSessionFocused(){return focused;}}
namespace savr::vrcam {
bool GroundWalkControlsActive(){return ground;}
bool GetHolsterAnchorTracking(int p,float out[3]){
    if(!body.valid)return false;
    const auto socket=holsterbody::Socket(body.position,body.yaw,p==0?-.27f:.27f,-.58f,.07f);
    out[0]=socket.x;out[1]=socket.y;out[2]=socket.z;return true;
}
void UpdateHolsterBodyTracking(const float*,const float*,bool valid,std::uint64_t epoch,
                               bool allowed,bool inner,bool outer,double now){
    body.Update({},holsterbody::Radians(90),valid,true,allowed&&bodyEnabled,epoch,0,inner,outer,now);
}
}
int frame;
void Tick(int tick){const double now=tick/100.0;
'''
reach_tests = r'''
}
void Through(int tick){while(frame<tick)Tick(++frame);}
void Reset(){
 body={};body.Update({},0,true,true,true,1,0,false,false,0);frame=-1;
 interactionsBlocked=inVehicle=vehicleImmersive=false;headValid=bodyEnabled=ground=focused=true;
 doorHands=basketballHands=0;slots[0]=2;slots[1]=4;seatWheel={};
 for(int h=0;h<2;++h){
  gReachIntent[h]={};gRadialMenus[h]={};gHeldSlot[h]=-1;
  gGripLatched[h]=gRadialTriggerLatched[h]=false;grabbable[h]=true;support[h]=false;
  gHands[h]={};gHands[h].pos={10,0,0};
 }
 gHands[0].pos={-.27f,-.58f,-.07f};
}
int main(){
 // Filled and empty sockets both start a one-second pause.
 for(int slot : {2,-1}){
  Reset();slots[0]=slot;Through(99);CHECK(body.frozen);
  Through(100);CHECK(!body.frozen&&body.yaw>0&&body.yaw<holsterbody::Radians(1));
 }
 // Each press/release edge refreshes; a held button never refreshes each tick.
 for(int action=0;action<4;++action){
  Reset();const bool trigger=action>=2,releasing=action%2;
  (trigger?gHands[0].trigger:gHands[0].grip)=releasing?.9f:0.f;
  (trigger?gRadialTriggerLatched[0]:gGripLatched[0])=releasing;
  Through(49);(trigger?gHands[0].trigger:gHands[0].grip)=releasing?0.f:.9f;
  Through(149);CHECK(body.frozen);Through(150);CHECK(!body.frozen);
 }
 // Edges remain usable after expiry, and later edges replace the deadline.
 Reset();Through(110);CHECK(!body.frozen);gHands[0].grip=.9f;Through(111);CHECK(body.frozen);
 Through(149);gHands[0].trigger=.9f;Through(150);
 Through(249);CHECK(body.frozen);Through(250);CHECK(!body.frozen);
 // Releasing the trigger in an open wheel away from the belt is interaction.
 Reset();Through(24);gRadialMenus[0].open=true;gHands[0].trigger=.9f;Through(25);
 gHands[0].pos={10,0,0};Through(74);gHands[0].trigger=0;Through(75);
 Through(174);CHECK(body.frozen);Through(175);CHECK(!body.frozen);
 // Moving away keeps the original deadline; unrelated buttons do not extend it.
 Reset();Through(25);gHands[0].pos={10,0,0};Through(74);gHands[0].trigger=.9f;
 Through(100);CHECK(!body.frozen);
 // A second hand has its own deadline; expiring the first cannot release it.
 Reset();Through(49);gHands[1].pos={.27f,-.58f,-.07f};Through(50);
 Through(149);CHECK(body.frozen);Through(150);CHECK(!body.frozen);
 // Ineligible hands/points cannot pause the belt.
 for(int reason=0;reason<8;++reason){
  Reset();
  if(reason==0)grabbable[0]=false;
  if(reason==1)support[0]=true;
  if(reason==2)doorHands=1;
  if(reason==3)basketballHands=1;
  if(reason==4)gHands[0].valid=false;
  if(reason==5)bodyEnabled=false;
  if(reason==6)inVehicle=true;
  if(reason==7)focused=false;
  Through(0);CHECK(!body.frozen);
 }
 // Tracking/recenter invalidation discards a deadline, as does leaving BODY.
 Reset();Through(75);gReachIntent[0].Invalidate();gHands[0].pos={10,0,0};
 Through(76);CHECK(!body.frozen);
 Reset();Through(75);bodyEnabled=false;Through(76);CHECK(!body.frozen);
 std::puts("PASS: production reach/button wiring, one-second deadline, all button edges, hold, empty sockets, wheel, two hands, unrelated actions and gates");
}
'''
generated=OUT/'reach.cpp'
generated.write_text(reach_prefix+reach+trigger_latches+grip_latches+reach_tests,encoding='utf8')
compile_run(generated,'holster-reach')
