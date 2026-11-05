"""Compile the production sampler and test against the local Blender exports."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='hand-animation-', dir=ROOT / '.cache') as temporary:
    output = Path(temporary)
    source = ROOT / 'tools/tests/hand_animation.cpp'
    exports = ROOT / 'assets/vrhands/animations'
    malformed = output / 'invalid'
    malformed.mkdir()
    exe = output / ('test.exe' if os.name == 'nt' else 'test')
    if os.name == 'nt':
        where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires',
            'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        vc = Path(base) / 'VC/Auxiliary/Build/vcvars64.bat'
        script = output / 'test.cmd'
        script.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\n'
            f'cl /nologo /std:c++20 /EHsc /W4 /D_CRT_SECURE_NO_WARNINGS /I"{ROOT / "native/src"}" '
            f'"{source}" /Fe:"{exe}" /Fo:"{output / "test.obj"}"\n'
            'if errorlevel 1 exit /b 1\n'
            f'"{exe}" "{exports}" "{malformed}"\n', encoding='utf-8')
        subprocess.run(['cmd', '/c', str(script)], cwd=output, check=True)
    else:
        compiler = os.environ.get('CXX') or shutil.which('c++')
        subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-I'+str(ROOT/'native/src'),
                        str(source), '-o', str(exe)], check=True)
        subprocess.run([str(exe), str(exports), str(malformed)], check=True)
