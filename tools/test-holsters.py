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
markers = physical[physical.index('    holstermarker::Marker markers['):physical.index('        // Current reference Quest build exposes the model-bound foregrip')]
prefix = r'''
#include "HolsterMarker.h"
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace savr;
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
struct {bool grabbed[2]{};} seatWheel;
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
    holstermarker::Marker m[8];CHECK(Publish(m)==7);
    // Both hands grab different sockets using the real nearest-available filter.
    for(int h=0;h<2;++h){gHands[h].grip=1;gHands[h].pos={float(h),0,0};}
    Transition();CHECK(gHeldSlot[0]==1&&gHeldSlot[1]==2);
    CHECK(Publish(m)==7);CHECK(m[0].kind==holstermarker::Kind::Return&&m[1].kind==holstermarker::Kind::Return);
    // Hand-to-hand ownership state retains the same return point and count.
    gHeldSlot[1]=-1;gHeldSlot[0]=2;
    CHECK(Publish(m)==7);CHECK(m[0].kind==holstermarker::Kind::Grab&&m[1].kind==holstermarker::Kind::Return);
    // Release into its own socket returns; a different socket is a drop.
    gHands[0].pos={1,0,0};gHands[0].grip=0;Transition();CHECK(gHeldSlot[0]==-1&&gDropped[0].slot==-1);
    CHECK(Publish(m)==7&&m[1].kind==holstermarker::Kind::Grab);
    gHeldSlot[0]=2;gHands[0].pos={0,0,0};Transition();CHECK(gDropped[0].slot==2);
    CHECK(Publish(m)==6);for(int i=0;i<6;++i)CHECK(m[i].kind!=holstermarker::Kind::Return);
    gDropped[0].slot=-1;CHECK(Publish(m)==7); // expired/caught drop is distinct from held
    // Grip lock retains a gun away from the socket, but permits return nearby.
    gHeldSlot[0]=2;gGripLock=true;Transition();CHECK(gHeldSlot[0]==2);
    gHands[0].pos={1,0,0};visibility[1]=2;Transition();CHECK(gHeldSlot[0]==-1);
    CHECK(Publish(m)==6); // OFF hides the marker but retains the existing return contract
    visibility[0]=1;gHeldSlot[0]=1;CHECK(Publish(m)==5); // HIDE also hides return hints
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
    CHECK(Publish(m)==6);
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
'''+globals_text
settings_production='\n'.join(function(sig,holsters) for sig in (
    'bool ValidPoint(', 'bool SelectableSlot(', 'void LoadSettings(',
    'void SaveSettingsLocked(', 'void Init()', 'bool BodyFrameEnabled()', 'void SetBodyFrameEnabled(',
    'float BodyReachIdleSeconds()', 'float BodyReachMotionMetres()'))
settings_tests=r'''
void Write(const char* text){FILE* f=std::fopen(kSettingsPath,"w");CHECK(f);std::fputs(text,f);std::fclose(f);}
}
int main(){
    using namespace savr::holster;
    std::remove(kSettingsPath);Init();CHECK(!BodyFrameEnabled());
    CHECK(std::abs(BodyReachIdleSeconds()-1.2f)<0.001f);CHECK(std::abs(BodyReachMotionMetres()-0.03f)<0.001f);
    Write("body_reach_idle_ms 3400\nbody_reach_motion_cm 5\n");LoadSettings();SaveSettingsLocked();
    gBodyReachIdleMs=0;gBodyReachMotionCm=1;LoadSettings();
    CHECK(gBodyReachIdleMs==3400&&gBodyReachMotionCm==5);
    Write("body_reach_idle_ms 99999\nbody_reach_motion_cm 0\n");LoadSettings();
    CHECK(gBodyReachIdleMs==10000&&gBodyReachMotionCm==1);
    Write("body_reach_idle_ms -1\nbody_reach_motion_cm 99\n");LoadSettings();
    CHECK(gBodyReachIdleMs==0&&gBodyReachMotionCm==20);
    Write("grip_markers 0\ngrab_radius_cm 23\ngrip_lock 1\nvisible 0 0\np 0 4\n");
    LoadSettings();CHECK(!BodyFrameEnabled());CHECK(!gGripMarkersEnabled);CHECK(gGrabRadiusCm==23);CHECK(gGripLockEnabled);CHECK(gPointState[0]==1);
    CHECK(gBodyReachIdleMs==1200&&gBodyReachMotionCm==3);
    SetBodyFrameEnabled(true);CHECK(BodyFrameEnabled());
    gBodyFrameEnabled=false;LoadSettings();CHECK(BodyFrameEnabled());CHECK(gPointState[0]==1&&gGrabRadiusCm==23);
    SetBodyFrameEnabled(false);gBodyFrameEnabled=true;LoadSettings();CHECK(!BodyFrameEnabled());
    Write("body_frame 99\nstate 0 2\n");LoadSettings();CHECK(!BodyFrameEnabled());CHECK(gPointState[0]==2);
    Write("body_frame 1\nbody_frame 0\n");LoadSettings();CHECK(!BodyFrameEnabled());
    std::puts("PASS: production settings old configs, BODY round trip, legacy visibility, invalid values and reset");
}
'''
generated=OUT/'settings.cpp'
generated.write_text(settings_prefix+settings_production+settings_tests,encoding='utf8')
compile_run(generated,'holster-settings')
