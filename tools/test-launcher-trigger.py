"""Run actual launcher trigger-edge/update and native fire gates with mocked engine calls."""
from pathlib import Path
import os
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/launcher-trigger-tests'
OUT.mkdir(parents=True,exist_ok=True)
src=(ROOT/'native/src/Throwable.cpp').read_text(encoding='utf8')

def block(signature):
    start=src.index(signature)
    brace=start+re.search(r'\)\s*\{',src[start:]).end()-1
    end,depth=brace+1,1
    while depth:
        depth+=(src[end]=='{')-(src[end]=='}')
        end+=1
    return src[start:end]+'\n'

code=r'''
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define LOGI(...) ((void)0)
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace savr::xr {
struct InputState{float triggers[2]{};};InputState input;
void GetInput(InputState& out){out=input;}
}
namespace savr::physicalweapon {
int primary=1;bool attached=false,tracked=true,blocked=false;
int LauncherTriggerHand(int h){return h==primary&&attached&&tracked&&!blocked?1-h:-1;}
}
namespace savr::throwable {
struct Vec3{float x{},y{},z{};};
int player{},npc{},heldType=35,calls{},throws{},lastOwner=-1;
bool vehicle=false;
std::uint32_t weapon[8]{};
void* Player(int){return &player;}void* Vehicle(int,bool){return vehicle?&npc:nullptr;}
bool NativeFire(void*,void*,Vec3*,Vec3*,void*,void*,void*){++calls;return true;}
struct{void*(*FindPlayerPed)(int)=Player;void*(*FindPlayerVehicle)(int,bool)=Vehicle;
bool(*CWeapon_Fire)(void*,void*,Vec3*,Vec3*,void*,void*,void*)=NativeFire;}g;
constexpr int kRpgType=35,kHeatSeekerType=36;
constexpr float kTriggerPress=.62f,kTriggerRelease=.32f;
std::atomic<bool> g_installed{true};int g_directLauncherHand=-1;
'''+src[src.index('struct HandThrowState {'):src.index('bool IsThrowable(')]+block('bool IsThrowable(')+block('bool IsLauncher(')+r'''
int WeaponTypeInHeldSlot(void*,int h){return h==physicalweapon::primary?heldType:-1;}
void* WeaponInHeldSlot(void*,int h,int& slot,int& type){
 if(h!=physicalweapon::primary){slot=type=-1;return nullptr;}
 slot=7;type=heldType;return weapon;
}
void ClearPreview(int){}
bool BuildLauncherAim(void*,int,Vec3&,Vec3&,int& out,int h){out=h;lastOwner=h;return true;}
bool BuildLaunch(void*,int type,Vec3&,Vec3&,int& out,int h){out=h;return IsThrowable(type);}
void PublishTrajectory(void*,int,int,Vec3,Vec3){}
void FireTrackedRelease(void*,void*,int,int,int,Vec3,Vec3){++throws;}
'''+block('bool FireTrackedLauncher(')+block('void Update(bool interactionsBlocked)')+r'''
bool NativeAllowed(void* shooter,int type){
'''+block('if (g.FindPlayerPed && shooter == g.FindPlayerPed(-1))')+r'''
 return true;
}
void Reset(int type,int owner){
 heldType=type;physicalweapon::primary=owner;physicalweapon::attached=false;
 physicalweapon::tracked=true;physicalweapon::blocked=false;vehicle=false;
 calls=throws=0;weapon[0]=type;weapon[1]=0;lastOwner=-1;
 xr::input={};for(auto& state:g_handState)state={};
}
void Test(){
 for(int type:{35,36})for(int owner:{0,1}){
  Reset(type,owner);const int support=1-owner;Update(false);
  xr::input.triggers[owner]=1;Update(false);CHECK(calls==0);
  CHECK(!FireTrackedLauncher(weapon,&player,owner,type));CHECK(calls==0);
  CHECK(!NativeAllowed(&player,type));CHECK(NativeAllowed(&npc,type));
  physicalweapon::attached=true;xr::input.triggers[support]=1;Update(false);
  CHECK(calls==0); // held-before-grab trigger needs a new edge
  CHECK(NativeAllowed(&player,type));
  xr::input.triggers[support]=0;Update(false);CHECK(calls==0);
  xr::input.triggers[support]=1;Update(false);CHECK(calls==1&&lastOwner==owner);
  for(int i=0;i<5;++i)Update(false);CHECK(calls==1); // one native call per edge
  physicalweapon::attached=false;Update(false);CHECK(calls==1);
  CHECK(!NativeAllowed(&player,type));
  physicalweapon::attached=true;Update(false);CHECK(calls==1);
  xr::input.triggers[support]=0;Update(false);xr::input.triggers[support]=1;Update(false);
  CHECK(calls==2); // released/re-grabbed, then fresh support press
  Update(true);Update(false);CHECK(calls==2); // menu close must not fire a held trigger
  physicalweapon::tracked=false;Update(false);CHECK(!NativeAllowed(&player,type));
  physicalweapon::tracked=true;Update(false);CHECK(calls==2);
  xr::input.triggers[support]=0;Update(false);weapon[1]=1;
  xr::input.triggers[support]=1;Update(false);CHECK(calls==2); // native cooldown retained
  xr::input.triggers[support]=0;Update(false);weapon[1]=0;
  xr::input.triggers[support]=1;Update(false);CHECK(calls==3);
 }
 for(int owner:{0,1}){
  Reset(16,owner);Update(false);xr::input.triggers[owner]=1;Update(false);
  CHECK(throws==0);xr::input.triggers[owner]=0;Update(false);CHECK(throws==1);
 }
 std::puts("PASS launchers: support trigger only, two-hand/native gates, both types/hands, re-grab/menu/tracking rearm, cadence and grenade release retained");
}
}
int main(){savr::throwable::Test();}
'''
(OUT/'test.cpp').write_text(code,encoding='utf8')
exe=OUT/('test.exe' if os.name=='nt' else 'test')
if os.name=='nt':
    where=Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
    base=subprocess.check_output([str(where),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
    vc=Path(base)/'VC/Auxiliary/Build/vcvars64.bat'
    script=OUT/'test.cmd'
    script.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 "{OUT / "test.cpp"}" /Fe:"{exe}" /Fo:"{OUT / "test.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n',encoding='utf8')
    subprocess.run(['cmd','/c',str(script)],cwd=OUT,check=True)
else:
    subprocess.run(['c++','-std=c++20',str(OUT/'test.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
