"""Exercise sticky horn geometry, input state and the actual OpenPalm asset."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='vehicle-horn-', dir=ROOT / '.cache', ignore_cleanup_errors=True) as temporary:
    output = Path(temporary)
    exe = output / ('test.exe' if os.name == 'nt' else 'test')
    sources = [ROOT/'tools/tests/vehicle_horn.cpp', ROOT/'native/src/HandSkeleton.cpp']
    driving = (ROOT/'native/src/Driving.cpp').read_text(encoding='utf-8')
    start = driving.index('    for (int hand=0;hand<2;++hand) {\n        positions[hand]=')
    end = driving.index('    const bool left=g_grabbed[0]', start)
    # Execute the production wheel-grab and horn publication block together.
    (output/'horn_integration.inc').write_text(r'''
namespace integration {
using namespace savr::driving;
using V3=vehiclehorn::Vec;
vehiclehorn::State g_hornState;
std::atomic<bool> g_hornPressed;
double g_hornLastTime{},testTime{};
double MonotonicSeconds(){return testTime;}
bool g_grabbed[2]{},g_angleValid[2]{},g_gripDown[2]{};
float g_rimGripAngle[2]{};
int haptics[2]{};
namespace xr {void TriggerHaptic(int hand,float,float,float){++haptics[hand];}}
struct Input {float grip[2]{};};
void Tick(WheelVisualState& visual,const savr::xr::HandPose poses[2],const Input& input={},unsigned weaponHands=0,int activeType=VEHICLE_CAR){
    testTime+=.01;
    const bool circular=!visual.bike&&!visual.yoke;
    const V3 center=vehiclehorn::Read(visual.center);
    V3 positions[2]{};bool poseValid[2]{},justGrabbed[2]{};
''' + driving[start:end] + '\n}\n}\n', encoding='utf-8')
    if os.name == 'nt':
        where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires',
            'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        vc = Path(base) / 'VC/Auxiliary/Build/vcvars64.bat'
        script = output / 'test.cmd'
        script.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\n'
            f'cl /nologo /std:c++20 /EHsc /W4 /D_CRT_SECURE_NO_WARNINGS /I"{ROOT / "native/src"}" /I"{output}" '
            + ' '.join(f'"{s}"' for s in sources)+f' /Fe:"{exe}"\n'
            'if errorlevel 1 exit /b 1\n'
            f'"{exe}" "{ROOT / "assets/vrhands/skeletal/hands.glb"}"\n', encoding='utf-8')
        subprocess.run(['cmd', '/c', str(script)], cwd=output, check=True)
    else:
        subprocess.run([os.environ.get('CXX') or shutil.which('c++'), '-std=c++20', '-Wall', '-Wextra',
                        '-I'+str(ROOT/'native/src'), '-I'+str(output), *(str(s) for s in sources), '-o', str(exe)], check=True)
        subprocess.run([str(exe), str(ROOT/'assets/vrhands/skeletal/hands.glb')], check=True)
