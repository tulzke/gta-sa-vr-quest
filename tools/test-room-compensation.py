"""Host checks for the shipped per-axis room/native collision attribution."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / 'build/room-compensation-tests'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'native/src/VrCamera.cpp').read_text(encoding='utf8')
start = source.index('const auto accepted = []')
end = source.index('\n    };', start) + len('\n    };')
code = '#include <algorithm>\n#include <cmath>\n#include <cassert>\nint main(){\n'
code += source[start:end]
turn_start = source.index('const auto followYaw = []')
turn_end = source.index('\n        };',turn_start)+len('\n        };')
code += '\n'+source[turn_start:turn_end]
code += r'''
const float pi=3.14159265358979323846f;
// Releasing sideways movement must turn gradually without a wrap-around spin.
float yaw=pi/2;
for(int i=0;i<120;++i){
    const float next=followYaw(yaw,0,1.f/60);
    assert(next>=0 && next<=yaw && yaw-next<=4.001f*pi/180);
    yaw=next;
}
assert(yaw<.001f);
assert(followYaw(179*pi/180,-179*pi/180,1.f/60)>179*pi/180);
// Time-based smoothing should converge similarly across frame rates.
float a=.5f,b=.5f;
for(int i=0;i<60;++i)a=followYaw(a,0,1.f/60);
for(int i=0;i<90;++i)b=followYaw(b,0,1.f/90);
assert(std::abs(a-b)<1e-5f);
assert(followYaw(.5f,0,0)==.5f);
assert(std::abs(accepted(.1f,.1f,.04f)-.04f)<1e-6f); // free motion: keep native .06
assert(accepted(0,.1f,.04f)==0); // wall: do not eat the physical lean
assert(std::abs(accepted(.05f,.1f,.04f)-.02f)<1e-6f); // partial collision response
assert(std::abs(accepted(-.1f,-.1f,-.04f)+.04f)<1e-6f); // backwards
assert(accepted(-.1f,.1f,.04f)==0); // external push opposite request
assert(std::abs(accepted(.3f,.1f,.04f)-.04f)<1e-6f); // never absorb excess native motion
assert(accepted(.1f,.1f,0)==0); // stick alone never changes tracking origin
assert(std::abs(accepted(0,0,.04f)-.04f)<1e-6f); // equal/opposite room and native commands
assert(accepted(0,.1f,.04f)==0 && accepted(.1f,.1f,.04f)>.039f); // slide along wall
}
'''
(out/'test.cpp').write_text(code, encoding='utf8')
where = Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
base = subprocess.check_output([str(where),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
vc = Path(base)/'VC/Auxiliary/Build/vcvars64.bat'
script = out/'run.cmd'
script.write_text(f'@echo off\ncall "{vc}" >nul\ncl /nologo /std:c++20 /EHsc test.cpp /Fe:test.exe\nif errorlevel 1 exit /b 1\ntest.exe\n',encoding='utf8')
subprocess.run(['cmd','/c',str(script)],cwd=out,check=True)
print('PASS: free, blocked, partial, reverse, native-only and opposing movement')
