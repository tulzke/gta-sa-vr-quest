"""Host-test the production door solver and engine adapter with fake engine memory."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/vehicle-door-tests'
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / 'native/src/VehicleDoors.cpp').read_text(encoding='utf8')

def remove_function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    end = brace + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[:start] + text[end:]

# Only OS-specific symbol resolution/patch installation is omitted. All live
# selection, pool validation, latching, writes and PreRender dispatch are real.
source = remove_function(source, 'bool Hook(void* target)')
source = remove_function(source, 'bool HookAutomaticDriverClose(void* status,void* drive)')
source = remove_function(source, 'unsigned DoorStatusForDrive(const void* damage,int door)')
source = remove_function(source, 'void Install(void* handle)')
source = '\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
harness = (ROOT / 'tools/tests/vehicle_doors.cpp').read_text(encoding='utf8')
generated = OUT / 'vehicle_doors.cpp'
generated.write_text(harness.replace('// PRODUCTION_ADAPTER', source), encoding='utf8')
exe = OUT / ('vehicle_doors.exe' if os.name == 'nt' else 'vehicle_doors')
if os.name == 'nt':
    vc = os.environ.get('VS_VCVARS64')
    if not vc:
        where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        vc = str(Path(base) / 'VC/Auxiliary/Build/vcvars64.bat')
    command = OUT / 'run.cmd'
    command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{ROOT / "native/src"}" "{generated}" /Fe:"{exe}" /Fo:"{OUT / "vehicle_doors.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n', encoding='utf8')
    subprocess.run(['cmd', '/c', str(command)], cwd=OUT, check=True)
else:
    compiler = os.environ.get('CXX') or shutil.which('c++')
    subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-I'+str(ROOT/'native/src'), str(generated), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
