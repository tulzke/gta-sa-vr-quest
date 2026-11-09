"""Exercise production ledge queries, collision sweeps and crouched landing."""
from pathlib import Path
import os, subprocess, shutil, re
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/ledge-climb-tests'
OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'native/src/LedgeClimb.cpp').read_text()
source='\n'.join(x for x in source.splitlines() if not x.startswith('#include'))
a=source.index('bool Hook(');b=source.index('using ClimbFn',a)
source=source[:a]+source[b:]
a=source.index('void Install(');b=source.index('bool JumpInProgress()',a)
source=source[:a]+source[b:]
source=source.replace('dlsym(RTLD_DEFAULT,"_ZN10CPlaceable14SetOrientationEfff")','nullptr')
source=re.sub(r'double Now\(\)\{[^\n]*\}', 'double Now(){return testNow;}',source)
camera=(ROOT/'native/src/VrCamera.cpp').read_text()
a=camera.index('bool ParachuteStateActive() {');b=camera.index('\n}',a)+2
source+='\nnamespace savr::vehicleclimb {bool Attached(){return false;}}\nnamespace savr::vrcam {\n'+camera[a:b]+'\n}\n'
xr=(ROOT/'native/src/Xr.cpp').read_text()
a=xr.index('bool BuildHandPanelAnchor(');b=xr.index('// Dashboard map offset',a)
source+='\nnamespace savr::xr {\n'+(ROOT/'tools/tests/wrist_anchor.cpp').read_text().replace('// ANCHOR',xr[a:b])+'\n}\n'
generated=OUT/'ledge_climb.cpp'
generated.write_text((ROOT/'tools/tests/ledge_climb.cpp').read_text().replace('// PRODUCTION',source))
exe=OUT/('ledge_climb.exe' if os.name=='nt' else 'ledge_climb')
if os.name == 'nt':
    vc = os.environ.get('VS_VCVARS64')
    if not vc:
        where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        vc = str(Path(base) / 'VC/Auxiliary/Build/vcvars64.bat')
    command = OUT / 'run.cmd'
    command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{ROOT / "native/src"}" "{generated}" /Fe:"{exe}" /Fo:"{OUT / "ledge_climb.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n', encoding='utf8')
    subprocess.run(['cmd', '/c', str(command)], cwd=OUT, check=True)
else:
    compiler = os.environ.get('CXX') or shutil.which('c++')
    subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-I'+str(ROOT/'native/src'), str(generated), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
