"""Exercise the production test-area adapter with a fake retail engine."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/vehicle-test-area-tests'
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / 'native/src/VehicleTestArea.cpp').read_text(encoding='utf8')

def remove_function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[:start] + text[end:]

# Omit only Android symbol/patch installation; exercise the actual state machine,
# geometry checks, resources, ownership, save/restart callbacks and navigation.
source = remove_function(source, 'void Install(void* handle)')
start = source.index('// Only exact, verified retail prologues')
end = source.index('} // namespace', start)
source = source[:start] + source[end:]
source = '\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
harness = (ROOT / 'tools/tests/vehicle_test_area.cpp').read_text(encoding='utf8')
generated = OUT / 'vehicle_test_area.cpp'
generated.write_text(harness.replace('// PRODUCTION_ADAPTER', source), encoding='utf8')
exe = OUT / ('vehicle_test_area.exe' if os.name == 'nt' else 'vehicle_test_area')
if os.name == 'nt':
    vc = os.environ.get('VS_VCVARS64')
    if not vc:
        where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        vc = str(Path(base) / 'VC/Auxiliary/Build/vcvars64.bat')
    command = OUT / 'run.cmd'
    command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{ROOT / "native/src"}" "{generated}" /Fe:"{exe}" /Fo:"{OUT / "vehicle_test_area.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n', encoding='utf8')
    subprocess.run(['cmd', '/c', str(command)], cwd=OUT, check=True)
else:
    compiler = os.environ.get('CXX') or shutil.which('c++')
    subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-I'+str(ROOT/'native/src'), str(generated), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
