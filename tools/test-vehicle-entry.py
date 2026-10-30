"""Host-test production vehicle entry using fake game memory and a deterministic clock."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/vehicle-entry-tests'
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / 'native/src/VehicleEntry.cpp').read_text(encoding='utf8')
start=source.index('void Install(')
end=source.index('bool ConsumesMove()',start)
source=source[:start]+source[end:]
start=source.index('double Now()')
end=source.index('\n}',start)
source=source[:start]+'double Now(){return testNow;}'+source[end:]
source='\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
harness=(ROOT / 'tools/tests/vehicle_entry.cpp').read_text(encoding='utf8')
generated=OUT / 'vehicle_entry.cpp'
generated.write_text(harness.replace('// PRODUCTION_ENTRY',source),encoding='utf8')
exe = OUT / ('vehicle_entry.exe' if os.name == 'nt' else 'vehicle_entry')
if os.name == 'nt':
    vc = os.environ.get('VS_VCVARS64')
    if not vc:
        where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        vc = str(Path(base) / 'VC/Auxiliary/Build/vcvars64.bat')
    command = OUT / 'run.cmd'
    command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{ROOT / "native/src"}" "{generated}" /Fe:"{exe}" /Fo:"{OUT / "vehicle_entry.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n', encoding='utf8')
    subprocess.run(['cmd', '/c', str(command)], cwd=OUT, check=True)
else:
    compiler = os.environ.get('CXX') or shutil.which('c++')
    subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-I'+str(ROOT/'native/src'), str(generated), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
