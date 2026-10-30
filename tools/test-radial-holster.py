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
