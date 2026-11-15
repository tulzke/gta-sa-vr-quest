"""Exercise the production two-hand firing loop with independent retail weapons."""
import os
from pathlib import Path
import re
import subprocess
import shutil
ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "native/src/VrFire.cpp").read_text(encoding="utf-8")
def function(name, source=SOURCE):
    m=re.search(r"^[\w:*<> ]+ " + name + r"\(", source, re.M)
    if not m: raise ValueError(name)
    start=m.start(); end=source.index("{",start)+1; depth=1
    while depth:
        depth+=(source[end]=="{")-(source[end]=="}"); end+=1
    return source[start:end]
PRELUDE=r"""
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iterator>
#include "PhysicalGunCycle.h"
#include "PairedWeapons.h"
namespace pairedweapon=savr::pairedweapon;
namespace arcadereload=savr::arcadereload;
arcadereload::Gesture g_reloadGestures[2];
thread_local int g_directHand=-1;
namespace vrfire {int DirectFiringHand(){return g_directHand;}}
using savr::vrfire::AdvancePhysicalGun;
using savr::vrfire::PhysicalMagazineCapacity;
#define LOGI(...) ((void)0)
#ifdef _WIN32
constexpr int CLOCK_MONOTONIC=0;
int clock_gettime(int,timespec* ts){ts->tv_sec=1;ts->tv_nsec=0;return 0;}
#endif
struct Vec3 {float x{},y{},z{};};
alignas(16) std::uint8_t player[0xa00]{},pad[0x140]{},info[64][0x70]{};
unsigned tick=100; int shots[2]{},held[2]{2,4}; float triggers[2]{};
int reloadSounds[2]{},lastReloadEvent{};
int checks=0;
void Check(bool b,const char* msg){++checks;if(!b){std::fprintf(stderr,"FAIL: %s\n",msg);std::exit(1);}}
void* FindPed(int){return player;} void* GetPad(int){return pad;}
void* NoVehicle(int,bool){return nullptr;}
int Skill(void*,int){return 2;}
void* GetInfo(int type,signed char){return info[type];}
int ReloadTime(void* ptr){auto* p=static_cast<std::uint8_t*>(ptr);return (*reinterpret_cast<unsigned*>(p+0x18)&0x800)?500:*reinterpret_cast<int*>(p+0x60);}
void ReloadSound(void* engine,int event,int type,void* parent){
    Check(engine==player && parent==player,"reload sound uses real audio engine and player entity");
    Check(type==31 && (event==146 || event==147),"reload sound is a short native weapon one-shot");
    ++reloadSounds[g_directHand];lastReloadEvent=event;
}
bool FakeFire(void* object,void*,Vec3* origin,Vec3*,void*,Vec3*,Vec3*){
    auto* p=static_cast<std::uint8_t*>(object); auto& clip=*reinterpret_cast<unsigned*>(p+8);
    auto& total=*reinterpret_cast<unsigned*>(p+12); if(!clip||!total)return false;
    int hand=g_directHand;
    Check(origin->x==float(hand+1),"shot uses its own hand ray");
    Check(player[0x8dc]==pairedweapon::Base(held[hand]),"native fire sees its own active slot");
    ++shots[hand]; --clip;--total;
    *reinterpret_cast<unsigned*>(p+4)=clip?1:2;
    *reinterpret_cast<unsigned*>(p+16)=tick+(clip?100:500);return true;
}
struct Symbols {
    void* AudioEngine=player;
    void (*CAudioEngine_ReportWeaponEvent)(void*,int,int,void*)=ReloadSound;
    void* (*FindPlayerPed)(int)=FindPed;
    void* (*FindPlayerVehicle)(int,bool)=NoVehicle;
    void* (*CPad_GetPad)(int)=GetPad;
    bool (*CWeapon_Fire)(void*,void*,Vec3*,Vec3*,void*,Vec3*,Vec3*)=FakeFire;
    unsigned* CTimer_m_snTimeInMilliseconds=&tick;
    int (*CPed_GetWeaponSkill)(void*,int)=Skill;
    void* (*CWeaponInfo_GetWeaponInfo)(int,signed char)=GetInfo;
    int (*CWeaponInfo_GetWeaponReloadTime)(void*)=ReloadTime;
    void* TheCamera=nullptr;
} g;
namespace physicalweapon {constexpr int kHandCount=2; int HeldSlot(int h){return held[h];} bool IsPrimaryDetached(int){return false;}}
namespace driving {bool VehicleWeaponsImmersive(){return false;}}
namespace scopeaim {bool IsActiveFor(int,int){return true;}}
namespace justbusiness {bool Active(){return false;}void RecordShot(int,int){}}
namespace vrcam {bool FirePhotograph(void*,void*,const float*){return true;}}
using WeaponUpdateFn=void(*)(void*,void*);
void NativeUpdate(void*,void*){}
WeaponUpdateFn g_origWeaponUpdate=NativeUpdate;
void* g_cyclePed{};int g_managedGunType[13]{};
std::atomic<bool> g_installed{true},g_directTriggerDown[2]{};
bool g_detonatorReleaseRequired[2]{true,true};
unsigned g_directNextFireMs[2]{};int g_directWeaponType[2]{-1,-1};
bool g_insideScriptedFire=false;
bool IsScriptedWeaponCamera(){return false;}
using PhysicalFireQuery=bool(*)(int,int*);
bool QueryPhysicalFireHand(int,int*);
std::atomic<PhysicalFireQuery> g_physicalFireQuery{QueryPhysicalFireHand};
namespace xr {
struct InputState {bool b=false;float triggers[2]{};};
void GetInput(InputState& in){in.triggers[0]=::triggers[0];in.triggers[1]=::triggers[1];}
struct HandPose {bool valid=true,aimValid=true;float gripPos[3]{},gripOri[4]{0,0,0,1};};
HandPose tracked[2];bool focused=true,tracking=true;std::uint64_t epoch=1;int haptics[2]{};
float hapticAmplitude[2]{},hapticDuration[2]{};
bool IsSessionFocused(){return focused;}
void TriggerHaptic(int hand,float amplitude,float,float duration){++haptics[hand];hapticAmplitude[hand]=amplitude;hapticDuration[hand]=duration;}
}
namespace physicalweapon {
struct TrackingPose {float right[3]{0,0,-1};};
bool GetInteractionTrackingSnapshot(float*,float*,xr::HandPose out[2],std::uint64_t* epoch){
    std::copy_n(xr::tracked,2,out);*epoch=xr::epoch;return xr::tracking;
}
bool GetHeldVisualPoseTracking(int hand,int,TrackingPose* out){
    const auto& q=xr::tracked[hand].gripOri;
    const auto barrel=arcadereload::Rotate({q[0],q[1],q[2],q[3]},{0,0,-1});
    out->right[0]=barrel.x;out->right[1]=barrel.y;out->right[2]=barrel.z;return true;
}
}
namespace vehicleentry {int SeatIndex(void*,void*){return -1;}}
bool GetPhysicalRay(void*,int type,Vec3& origin,Vec3& direction){
    int h=-1;if(!QueryPhysicalFireHand(type,&h))return false;origin={float(h+1),0,0};direction={0,1,0};return true;
}
"""
TESTS=r"""
unsigned& Field(int slot,int offset){return *reinterpret_cast<unsigned*>(player+0x730+slot*0x20+offset);}
void Setup(int h,int slot,int type,unsigned clip,unsigned total,unsigned capacity,int duration,unsigned flags=0){
    held[h]=slot;Field(slot,0)=type;Field(slot,4)=0;Field(slot,8)=clip;Field(slot,12)=total;
    *reinterpret_cast<short*>(info[type]+0x20)=capacity;
    *reinterpret_cast<unsigned*>(info[type]+0x18)=flags;
    *reinterpret_cast<int*>(info[type]+0x60)=duration;
    *reinterpret_cast<float*>(info[type]+0x44)=.1f;
}
""" + (ROOT / "tools/tests/combined_reload.inc").read_text(encoding="utf-8")
output=ROOT / "build" / "dual-fire-tests"
def compile_run(source, name):
    exe = output / (name + ('.exe' if os.name == 'nt' else ''))
    if os.name == 'nt':
        vc = os.environ.get('VS_VCVARS64')
        if not vc:
            where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
            base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
            vc = str(Path(base) / 'VC/Auxiliary/Build/vcvars64.bat')
        command = output / (name + '.cmd')
        command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{ROOT / "native/src"}" "{source}" /Fe:"{exe}" /Fo:"{output / (name + ".obj")}"\nif errorlevel 1 exit /b 1\n"{exe}"\n', encoding='utf8')
        subprocess.run(['cmd', '/c', str(command)], cwd=output, check=True)
    else:
        compiler = os.environ.get('CXX') or shutil.which('c++')
        subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-I'+str(ROOT/'native/src'), str(source), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


def main():
    names=["PlayerFireEnabled","SupportedHitscan","AreaEffectWeapon","SupportedDirectFire",
           "AutomaticHitscan","PhysicalFireIntervalMs","PhysicalWeaponInfo","MagazineCapacity",
           "OnWeaponUpdate","PhysicalReloadDuration","CorrectPhysicalReload","UpdateReloadGesture","ResetDirectTrigger","DetonatorTriggerReady",
           "GetPhysicalTrigger","UpdateHand","Update"]
    constants="\n".join(re.findall(r"^constexpr (?:std::uintptr_t|float) \w+ = .*?;",SOURCE,re.M))
    output=ROOT/"build/dual-fire-tests";output.mkdir(parents=True,exist_ok=True)
    source=output/"mission_weapons.cpp"
    source.write_text(PRELUDE+constants+"\n"+function("QueryPhysicalFireHand",(ROOT/"native/src/main.cpp").read_text(encoding="utf-8"))+"\n"+"\n".join(function(n) for n in names)+TESTS,encoding="utf-8")
    compile_run(source, "mission_weapons")


if __name__ == "__main__":
    main()
