"""Exercise the production script observer using a synthetic CPed inventory."""
from pathlib import Path
import os
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / 'build/mission-weapon-tests'
out.mkdir(parents=True, exist_ok=True)
src = (root/'native/src/MissionWeaponGrant.cpp').read_text(encoding='utf8')
start = src.index('int ProcessAndObserve(')
end = src.index('\nint On400(', start)
code = r'''
#include "MissionWeaponPolicy.h"
#include <atomic>
#include <cstring>
#include <cassert>
using namespace savr::missionweapon;
using Process = int (*)(void*,int);
std::atomic<int> pending{-1};
unsigned char player[0x1000]{};
bool mission=true, playerPresent=true;
bool IsMission(){return mission;}
bool (*onMission)()=IsMission;
void* Find(int){return playerPresent?player:nullptr;}
struct {void* (*FindPlayerPed)(int)=Find;} g;
int changedSlot=2, type=22, ammo=20;
int Original(void*,int){
 if (playerPresent && changedSlot>0) {
  WeaponSnapshot weapon{}; weapon.type=type; weapon.total=ammo;
  std::memcpy(player+0x730+changedSlot*0x20,&weapon,sizeof(weapon));
 }
 return 17;
}
''' + src[start:end] + r'''
int main(){
 assert(ProcessAndObserve(Original,nullptr,1)==17);
 assert(pending.exchange(-1)==2);
 ProcessAndObserve(Original,nullptr,1); assert(pending.exchange(-1)==-1);
 ammo=40; ProcessAndObserve(Original,nullptr,1); assert(pending.exchange(-1)==2);
 mission=false; changedSlot=3; ProcessAndObserve(Original,nullptr,1); assert(pending.exchange(-1)==-1);
 mission=true; changedSlot=11; ProcessAndObserve(Original,nullptr,1); assert(pending.exchange(-1)==-1);
 playerPresent=false; ProcessAndObserve(Original,nullptr,1); assert(pending.exchange(-1)==-1);
}
'''
source = out/'test.cpp'
source.write_text(code, encoding='utf8')
exe = out/('test.exe' if os.name == 'nt' else 'test')
if os.name == 'nt':
    vc = os.environ.get('VS_VCVARS64')
    if not vc:
        where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
        base = subprocess.check_output([str(where),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
        vc = str(Path(base)/'VC/Auxiliary/Build/vcvars64.bat')
    script = out/'run.cmd'
    script.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /I"{root / "native/src"}" "{source}" /Fe:"{exe}" /Fo:"{out / "test.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n',encoding='utf8')
    subprocess.run(['cmd','/c',str(script)],cwd=out,check=True)
else:
    subprocess.run([os.environ.get('CXX') or shutil.which('c++'),'-std=c++20','-I'+str(root/'native/src'),str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('mission script weapon observer: passed')
