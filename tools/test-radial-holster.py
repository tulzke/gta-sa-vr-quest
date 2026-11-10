"""Compile and exercise the production radial holster gesture state."""
from pathlib import Path
import os
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / 'build/radial-holster-tests'
out.mkdir(parents=True, exist_ok=True)
source = root / 'tools/tests/radial_holster_menu.cpp'
exe = out / ('radial_holster_menu.exe' if os.name == 'nt' else 'radial_holster_menu')
if os.name == 'nt':
    vc = os.environ.get('VS_VCVARS64')
    if not vc:
        where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        vc = str(Path(base) / 'VC/Auxiliary/Build/vcvars64.bat')
    script = out / 'run.cmd'
    script.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{root / "native/src"}" "{source}" /Fe:"{exe}" /Fo:"{out / "radial.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n', encoding='utf8')
    subprocess.run(['cmd', '/c', str(script)], cwd=out, check=True)
else:
    subprocess.run([os.environ.get('CXX') or shutil.which('c++'), '-std=c++20', '-Wall', '-Wextra', '-I'+str(root/'native/src'), str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('radial holster gesture: passed')


# Execute the actual two-hand owner/publication block, not a second state loop.
physical=(root/'native/src/PhysicalWeapon.cpp').read_text(encoding='utf-8')
begin=physical.index('    unsigned int radialHandledHands = 0;')
end=physical.index('\n    if (!interactionsBlocked) {',begin)
block=physical[begin:end]
prefix=r"""
#include "RadialHolsterMenu.h"
#include "PairedWeapons.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdint>
using namespace savr;
struct V3 {float x,y,z;}; struct Q {float x,y,z,w;};
Q NormalizeQ(Q q){return q;} V3 Rotate(Q,V3 v){return v;}
struct Hand {bool valid=true;float grip=0,trigger=.8f;float gripPos[3]{};};
Hand gHands[2];int gHeldSlot[2]{-1,-1};
radialholster::Menu gRadialMenus[2];
radialholster::Frames published;
constexpr int kHandCount=2,kWeaponSlotCount=26;
constexpr float kGripClose=.65f,kGripOpen=.3f;
alignas(16) unsigned char memory[0xa00]{};void* ped=memory;
float headOrientation[4]{0,0,0,1};
float anchors[2][3]{{-.3f,0,0},{.3f,0,0}};
bool anchorValid[2]{true,true},triggerEdge[2]{true,true},radialUsable=true,inVehicle=false;
unsigned doorHands=0;std::uint64_t trackingEpoch=1;int skill=2,assignments[2]{2,4};
namespace savr::basketball {unsigned HeldHandMask(){return 0;}}
namespace savr::xr {
void SetRadialHolsterMenu(const radialholster::Frames& frames){published=frames;}
void TriggerHaptic(int,float,int,int){}
}
namespace savr::holster {
int PointCount(){return 2;}int PointSlot(int p){return assignments[p];}
bool PointGrabbable(int){return true;}float GrabRadiusMetres(){return .18f;}
int FindPointForSlot(int slot){for(int p=0;p<2;++p)if(assignments[p]==slot)return p;return -1;}
void SetPointSlot(int p,int slot){assignments[p]=slot;}
}
bool ValidHand(int h){return h>=0&&h<2;}
bool ValidSlot(int slot){return pairedweapon::Valid(slot);}
bool IsSupportHandLocked(int){return false;}
unsigned OccupiedSlotMaskLocked(){return 0;}
int WeaponTypeInSlot(void* ped,int slot){return pairedweapon::Type(ped,slot);}
bool RadialUnavailable(void*,int slot,bool){return pairedweapon::Mirror(slot)&&skill<2;}
V3 GripPosition(const Hand& h){return {h.gripPos[0],h.gripPos[1],h.gripPos[2]};}
float Distance(V3 a,V3 b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));}
void Tick(){
"""
tests=r"""
}
void Reset(){
 for(int h=0;h<2;++h){gRadialMenus[h]={};gHands[h]={};gHands[h].gripPos[0]=anchors[h][0];triggerEdge[h]=true;}
 assignments[0]=2;assignments[1]=4;radialUsable=true;
}
void Aim(int h,bool east=false){gHands[h].gripPos[east?0:1]+=.1f;}
int main(){
 *reinterpret_cast<int*>(memory+0x730+2*0x20)=22;
 *reinterpret_cast<int*>(memory+0x730+3*0x20)=26;
 *reinterpret_cast<int*>(memory+0x730+4*0x20)=28;
 Reset();Tick();
 assert(gRadialMenus[0].open&&gRadialMenus[1].open);
 assert(published.hands[0].visible&&published.hands[1].visible);
 assert(published.hands[0].hand==0&&published.hands[1].hand==1);
 assert(published.hands[0].origin[0]<0&&published.hands[1].origin[0]>0);
 triggerEdge[0]=triggerEdge[1]=false;Aim(1);gHands[1].trigger=0;Tick();
 assert(gRadialMenus[0].open&&!gRadialMenus[1].open);
 assert(published.hands[0].visible&&!published.hands[1].visible);
 assert(assignments[0]==2&&assignments[1]==15);
 Reset();Tick();triggerEdge[0]=triggerEdge[1]=false;gHands[0].grip=.8f;Tick();
 assert(!gRadialMenus[0].open&&gRadialMenus[1].open);
 assert(!published.hands[0].visible&&published.hands[1].visible);
 // Simultaneous choice of a previously unassigned gun resolves the second as a pair.
 Reset();Tick();for(int h=0;h<2;++h){triggerEdge[h]=false;Aim(h,true);gHands[h].trigger=0;}Tick();
 assert(assignments[0]==3&&assignments[1]==16);
 assert(!published.Visible());
 skill=1;Reset();Tick();for(int h=0;h<2;++h){triggerEdge[h]=false;Aim(h,true);gHands[h].trigger=0;}Tick();
 assert(assignments[0]==3&&assignments[1]==4); // cannot bypass skill via simultaneous release
 Reset();Tick();triggerEdge[0]=triggerEdge[1]=false;Aim(0,true);gHands[0].trigger=0;Tick();
 assert(gRadialMenus[1].open&&published.hands[1].slots[1]==16&&published.hands[1].unavailable[1]);
 Aim(1,true);gHands[1].trigger=0;Tick();assert(assignments[1]==4);
 Reset();Tick();triggerEdge[0]=triggerEdge[1]=false;radialUsable=false;Tick();assert(!published.Visible());
 Reset();gHands[1].gripPos[0]=anchors[0][0];Tick();
 assert(gRadialMenus[0].open&&!gRadialMenus[1].open); // one editor per socket
}
"""
source=out/'two_hands.cpp';source.write_text(prefix+block+tests,encoding='utf-8')
if os.name=='nt':
    script=out/'two-hands.cmd'
    script.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /I"{root / "native/src"}" "{source}" /Fe:"{out / "two_hands.exe"}" /Fo:"{out / "two_hands.obj"}"\nif errorlevel 1 exit /b 1\n"{out / "two_hands.exe"}"\n',encoding='utf-8')
    subprocess.run(['cmd','/c',str(script)],cwd=out,check=True)
else:
    subprocess.run([os.environ.get('CXX') or shutil.which('c++'),'-std=c++20','-I'+str(root/'native/src'),str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('two-hand production radial ownership/publication: passed')
