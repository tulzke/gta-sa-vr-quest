"""Exercise production optic pose, collection and camera/state transactions."""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/weapon-optic-tests'
OUT.mkdir(parents=True, exist_ok=True)


def function(source, name):
    match = re.search(r'^(?:Mat34|V3|bool|float|void\*|void) ' + name + r'\(', source, re.M)
    start = match.start()
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


camera = (ROOT / 'native/src/VrCamera.cpp').read_text(encoding='utf8')
mirror = (ROOT / 'native/src/RearMirrorRender.inl').read_text(encoding='utf8')
start = mirror.index('bool Resolve() {')
end = mirror.index('\nvoid* raster{};', start)
mirror = mirror[:start] + 'bool Resolve() {return api.ready;}\n' + mirror[end:]
helpers = r'''
V3 Normalized(V3 v) {float l=std::sqrt(Dot(v,v));return l>.000001f?v*(1.f/l):V3{};}
struct RwMat {float r[3];unsigned flags;float u[3];unsigned p1;float a[3];unsigned p2;float t[3];unsigned p3;};
struct Mat34 {V3 right,fwd,up,pos;};
namespace calib {
weaponoptic::Profile opticProfile=weaponoptic::Defaults(34);
weaponoptic::Profile SnapshotOptic(int){return opticProfile;}
}
namespace scopeaim {
float WeaponZoom(int type){return type==34?2.5f:1.8f;}
HeatLockState HeatLockSnapshot(){return {};}
}
bool PhotoViewActive(){return false;}
void RenderWorldStableSkyPolys(std::uintptr_t,const MobileDistanceFogSnapshot&){}
'''
production = helpers + function(camera, 'MatMul34') + function(camera, 'SetRotate34')
production += (ROOT / 'native/src/WeaponOpticFrame.inl').read_text(encoding='utf8')
production += mirror + (ROOT / 'native/src/WeaponOpticRender.inl').read_text(encoding='utf8')
production += r'''
namespace vrcam {bool TrackingPointToWorld(const float* a,float* b){std::copy_n(a,3,b);return true;}}
namespace driving {
watermonitor::Visual monitorVisual{};
bool GetWaterMonitorVisual(watermonitor::Visual* out){*out=monitorVisual;return out->visible;}
}
'''
monitor = (ROOT / 'native/src/WaterMonitorRender.inl').read_text(encoding='utf8')
start = monitor.index('void RenderEffects(')
end = monitor.index('\nvoid Prepare()', start)
monitor = monitor[:start] + 'void RenderEffects(void*){}\n' + monitor[end:]
monitor = monitor.replace('static const bool loaded=model.Load(savr_water_monitor_mesh,static_cast<std::size_t>(savr_water_monitor_mesh_end-savr_water_monitor_mesh));',
                          'const bool loaded=!model.vertices.empty(); // Host loads the same asset below.')
production += monitor
scope = (ROOT / 'native/src/ScopeAim.cpp').read_text(encoding='utf8')
production += r'''
namespace heat_test {
using GameVec3=GameSymbols::Vec3;
using scopeaim::HeatLockState;
using rear_mirror_render::Read;
constexpr int kHeatSeekerType=36;
HeatLockState gHeatLock{};
void* gHeatPool{};
int gHeatIndex=-1,gHeatHand=-1;
unsigned char gHeatGeneration{};
std::uint64_t gHeatSince{};
struct ColPoint {alignas(4) unsigned char bytes[0x2c]{};};
float WeaponZoom(int){return 1.8f;}
float LengthSquared(V3 v){return Dot(v,v);}
namespace vrcam {
bool GetWeaponOpticRay(int,int,float* o,float* d){o[0]=o[1]=o[2]=0;d[0]=d[2]=0;d[1]=-1;return true;}
}
bool IsActiveFor(int,int){return false;}
bool BuildCentreTarget(V3&){return false;}
'''
for name in ('ClearHeat', 'ValidHeatEntity', 'UpdateHeat', 'ApplyReticleAim'):
    production += function(scope, name)
production += '\n}\n'
harness = (ROOT / 'tools/tests/rear_mirror.cpp').read_text(encoding='utf8')
harness = '#include "WeaponOptic.h"\n#include "ScopeAim.h"\n#include "StickTurn.h"\n#include "WaterMonitorModel.h"\n#include "WaterMonitorCamera.h"\n#include <fstream>\n#include <iterator>\n' + harness
harness = harness.replace('// PRODUCTION_ADAPTER', production)
tests = (ROOT / 'tools/tests/weapon_optics.inc').read_text(encoding='utf8')
tests += (ROOT / 'tools/tests/water_monitor_render.inc').read_text(encoding='utf8').replace('WATER_ASSET_PATH', (ROOT / 'assets/firetruck-controller/runtime/monitor.mesh').as_posix())
harness = harness[:harness.rfind('}')] + tests + '\n}\n'
cpp = OUT / 'weapon_optics.cpp'
cpp.write_text(harness, encoding='utf8')
exe = OUT / ('weapon_optics.exe' if os.name == 'nt' else 'weapon_optics')
if os.name == 'nt':
    vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    vc = Path(vs) / 'VC/Auxiliary/Build/vcvars64.bat'
    command = OUT / 'run.cmd'
    command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{ROOT / "native/src"}" "{cpp}" /Fe:"{exe}" /Fo:"{OUT / "weapon_optics.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n')
    subprocess.run(['cmd', '/c', str(command)], cwd=OUT, check=True)
else:
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra', '-I'+str(ROOT/'native/src'), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
