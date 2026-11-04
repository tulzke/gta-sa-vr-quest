"""Test production climbing and parachute interaction with a deterministic clock."""
from pathlib import Path
import os
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/vehicle-climb-tests'
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / 'native/src/VehicleClimb.cpp').read_text(encoding='utf8')
start=source.index('void Install(')
end=source.index('unsigned ClaimedHands()',start)
source=source[:start]+source[end:]
source=re.sub(r'double Now\(\)\{[^\n]*\}', 'double Now(){return testNow;}', source)
source='\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
harness=(ROOT / 'tools/tests/vehicle_climb.cpp').read_text(encoding='utf8')
generated=OUT / 'vehicle_climb.cpp'
def function(path, signature):
    text=(ROOT/path).read_text(encoding='utf8')
    start=text.index(signature);brace=text.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
parachute=function('native/src/PhysicalWeapon.cpp','void AutoEquipParachute()')
state=function('native/src/VrCamera.cpp','bool ParachuteStateActive() {')
combined=harness.replace('// PRODUCTION_CLIMB',source).replace('// PRODUCTION_PARACHUTE',
    'namespace savr::physicalweapon {\n'+parachute+'\n}\nnamespace savr::vrcam {\n'+state+'\n}')
generated.write_text(combined,encoding='utf8')
exe = OUT / ('vehicle_climb.exe' if os.name == 'nt' else 'vehicle_climb')
if os.name == 'nt':
    vc = os.environ.get('VS_VCVARS64')
    if not vc:
        where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        vc = str(Path(base) / 'VC/Auxiliary/Build/vcvars64.bat')
    command = OUT / 'run.cmd'
    command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{ROOT / "native/src"}" "{generated}" /Fe:"{exe}" /Fo:"{OUT / "vehicle_climb.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n', encoding='utf8')
    subprocess.run(['cmd', '/c', str(command)], cwd=OUT, check=True)
else:
    compiler = os.environ.get('CXX') or shutil.which('c++')
    subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-I'+str(ROOT/'native/src'), str(generated), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
