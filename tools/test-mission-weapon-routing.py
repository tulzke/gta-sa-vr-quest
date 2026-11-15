"""Run production weapon routing functions against synthetic retail memory."""

import os
from pathlib import Path
import re
import subprocess
import shutil

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "native/src/VrFire.cpp").read_text(encoding="utf-8")


def function(name):
    match = re.search(r"^(?:void|bool) " + name + r"\(", SOURCE, re.MULTILINE)
    if not match:
        raise ValueError("Missing production function: " + name)
    start = match.start()
    end = SOURCE.index("{", start) + 1
    depth = 1
    while depth:
        depth += (SOURCE[end] == "{") - (SOURCE[end] == "}")
        end += 1
    return SOURCE[start:end]


PRELUDE = r"""
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <initializer_list>
#include "ArcadeReload.h"
savr::arcadereload::Gesture g_reloadGestures[2];
struct Vec3 { float x{}, y{}, z{}; };
alignas(16) std::uint8_t ped[0xa00]{}, camera[0x1000]{}, pad[0x140]{};
void* FindPed(int) { return ped; }
void* GetPad(int) { return pad; }
bool FakeFire(void*,void*,Vec3*,Vec3*,void*,Vec3*,Vec3*);
void CorrectPhysicalReload(void*,void*,int,unsigned){}
namespace vrcam {bool FirePhotograph(void*,void*,const float*){return true;}}
struct Symbols {
    void* TheCamera=camera;
    void* (*FindPlayerPed)(int)=FindPed;
    void* (*CPad_GetPad)(int)=GetPad;
    bool (*CWeapon_Fire)(void*,void*,Vec3*,Vec3*,void*,Vec3*,Vec3*)=FakeFire;
} g;
namespace justbusiness { bool active=false; bool Active(){return active;} }
thread_local bool g_insideScriptedFire=false;
std::atomic<bool> g_directTriggerDown[2]{};
bool g_detonatorReleaseRequired[2]{true,true};
std::uint32_t g_directNextFireMs[2]{};
int g_directWeaponType[2]{-1,-1};
int checks=0;
void Check(bool condition,const char* name) {
    ++checks;
    if (!condition) { std::fprintf(stderr,"FAIL: %s\n",name); std::exit(1); }
}
template<class T> void Put(std::uint8_t* p,std::size_t offset,T value) {
    std::memcpy(p+offset,&value,sizeof(value));
}
"""

TESTS = r"""
int expectedDuringFire=0;
bool expectedScripted=false, nativeResult=true;
bool FakeFire(void*,void*,Vec3*,Vec3*,void*,Vec3*,Vec3*) {
    Check(*reinterpret_cast<std::uint16_t*>(camera+kPlayerWeaponModeOffset)==expectedDuringFire,
          "native fire sees intended mode");
    Check(IsScriptedWeaponCamera()==expectedScripted,
          "nested physical-ray query retains scripted ownership");
    return nativeResult;
}
int main() {
    Put(ped,kPedActiveSlotOffset,std::int8_t{5});
    Put(ped,kPedWeaponsOffset+5*kWeaponStride,std::int32_t{31});
    Put(camera,kPlayerWeaponModeOffset,std::uint16_t{45});
    justbusiness::active=true;
    Check(IsScriptedWeaponCamera(),"Just Business passenger needs no attachment/mode");
    Put(camera,kPlayerWeaponModeOffset,std::uint16_t{0});
    Check(IsScriptedWeaponCamera(),"passenger remains gunner in ordinary camera mode");
    expectedDuringFire=0;expectedScripted=true;
    Check(CallProductionFire(28),"ordinary passenger reaches native Micro SMG fire");
    Check(*reinterpret_cast<std::uint16_t*>(camera+kPlayerWeaponModeOffset)==0,
          "passenger camera mode restored after native shot");
    expectedScripted=false;
    justbusiness::active=false;
    Put(camera,kPlayerWeaponModeOffset,std::uint16_t{45});
    Check(!IsScriptedWeaponCamera(),"ordinary ped cannot become attached gunner");
    Put(ped,kPedAttachedToOffset,static_cast<void*>(camera));
    Check(IsScriptedWeaponCamera(),"attached M4 helicannon mode");
    Put(camera,kPlayerWeaponModeOffset,std::uint16_t{65});
    Check(IsScriptedWeaponCamera(),"attached aiming mode with gun");
    Put(ped,kPedWeaponsOffset+5*kWeaponStride,std::int32_t{7});
    Check(!IsScriptedWeaponCamera(),"attached pool cue cannot become gunner");
    Put(ped,kPedWeaponsOffset+5*kWeaponStride,std::int32_t{31});
    for (int mode : {0,4,7,34,46,53,55}) {
        Put(camera,kPlayerWeaponModeOffset,static_cast<std::uint16_t>(mode));
        Check(!IsScriptedWeaponCamera(),"ordinary rifle/sniper/photo modes excluded");
    }
    Put(camera,kPlayerWeaponModeOffset,std::uint16_t{45});
    for (int slot : {-1,0,13,127}) {
        Put(ped,kPedActiveSlotOffset,static_cast<std::int8_t>(slot));
        Check(!IsScriptedWeaponCamera(),"invalid inventory slot rejected");
    }
    Put(ped,kPedActiveSlotOffset,std::int8_t{5});
    for (bool result : {false,true}) {
        nativeResult=result; expectedDuringFire=0; expectedScripted=true;
        Check(CallProductionFire(31)==result,"native success/failure preserved");
        Check(*reinterpret_cast<std::uint16_t*>(camera+kPlayerWeaponModeOffset)==45,
              "native weapon mode restored after success/failure");
        Check(!g_insideScriptedFire,"temporary ownership flag restored");
    }
    Put(camera,kPlayerWeaponModeOffset,std::uint16_t{34});
    nativeResult=true; expectedDuringFire=34; expectedScripted=false;
    Check(CallProductionFire(31),"ordinary rifle fire mode left untouched");
    Check(PlayerFireEnabled(),"enabled pad accepts physical fire");
    for (std::uint16_t bits : {1,2,128,256,32768}) {
        Put(pad,kPadDisableControlsOffset,bits);
        Check(!PlayerFireEnabled(),"every tested native control lock blocks fire");
    }
    Put(pad,kPadDisableControlsOffset,std::uint16_t{0});
    pad[kPadDisableFireOffset]=1;
    Check(!PlayerFireEnabled(),"native fire lock blocks fire");
    pad[kPadDisableFireOffset]=0;
    Check(PlayerFireEnabled(),"native locks restored");
    for (int hand : {0,1}) {
        ResetDirectTrigger();
        Check(!DetonatorTriggerReady(40,hand,1.0f),"held trigger after block cannot detonate");
        Check(!DetonatorTriggerReady(40,hand,0.5f),"partial release cannot rearm detonator");
        Check(!DetonatorTriggerReady(40,hand,std::numeric_limits<float>::quiet_NaN()),
              "invalid trigger cannot rearm detonator");
        Check(!DetonatorTriggerReady(40,hand,0.0f),"release rearms without firing");
        Check(DetonatorTriggerReady(40,hand,1.0f),"new press after release allowed");
        ResetDirectTrigger();
        Check(!DetonatorTriggerReady(40,hand,1.0f),"next menu requires another release");
    }
    Check(DetonatorTriggerReady(31,1,1.0f),"automatic gun behavior unchanged");
    Check(DetonatorTriggerReady(43,1,1.0f),"camera behavior unchanged");
    std::printf("PASS: %d production mission weapon checks\n",checks);
}
"""


output=ROOT / "build" / "mission-weapon-tests"
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
    constants = "\n".join(re.findall(
        r"^constexpr (?:std::uintptr_t|float) (?:kPadDisableControlsOffset|"
        r"kPadDisableFireOffset|kPedWeaponsOffset|kPedActiveSlotOffset|"
        r"kWeaponStride|kPedAttachedToOffset|kPlayerWeaponModeOffset|"
        r"kTriggerRelease) = .*?;", SOURCE, re.MULTILINE))
    names = ["PlayerFireEnabled", "SupportedHitscan", "ResetDirectTrigger",
             "DetonatorTriggerReady", "IsScriptedWeaponCamera"]
    begin = SOURCE.index("    std::uint16_t* playerWeaponMode = nullptr;")
    end = SOURCE.index("    if (!fired) {", begin)
    fire = ("bool CallProductionFire(int weaponType) {\n"
            "void* weapon=nullptr; void* ped=::ped; unsigned now=0; Vec3 origin{}; Vec3* target=nullptr;\n" +
            SOURCE[begin:end] + "\nreturn fired;\n}\n")
    output = ROOT / "build/mission-weapon-tests"
    output.mkdir(parents=True, exist_ok=True)
    source = output / "mission_weapons.cpp"
    source.write_text(PRELUDE + constants + "\n" +
                      "\n".join(function(n) for n in names) + fire + TESTS,
                      encoding="utf-8")
    compile_run(source, "mission_weapons")


if __name__ == "__main__":
    main()
