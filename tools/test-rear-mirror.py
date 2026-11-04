"""Compile the real mirror collector/render adapter against fake retail memory."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/rear-mirror-tests'
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / 'native/src/RearMirrorRender.inl').read_text(encoding='utf8')
# Only Android symbol resolution is replaced; selection, camera transactions,
# atomic LOD choice and rendering are the production implementation.
start = source.index('bool Resolve() {')
end = source.index('\nvoid* raster{};', start)
source = source[:start] + 'bool Resolve() { return api.ready; }\n' + source[end:]
harness = (ROOT / 'tools/tests/rear_mirror.cpp').read_text(encoding='utf8')
generated = OUT / 'rear_mirror.cpp'
generated.write_text(harness.replace('// PRODUCTION_ADAPTER', source), encoding='utf8')
exe = OUT / ('rear_mirror.exe' if os.name == 'nt' else 'rear_mirror')
if os.name == 'nt':
    vc = os.environ.get('VS_VCVARS64')
    if not vc:
        where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        vc = str(Path(base) / 'VC/Auxiliary/Build/vcvars64.bat')
    command = OUT / 'run.cmd'
    command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{ROOT / "native/src"}" "{generated}" /Fe:"{exe}" /Fo:"{OUT / "rear_mirror.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n', encoding='utf8')
    subprocess.run(['cmd', '/c', str(command)], cwd=OUT, check=True)
else:
    subprocess.run([os.environ.get('CXX') or shutil.which('c++'), '-std=c++20', '-Wall', '-Wextra', '-I'+str(ROOT/'native/src'), str(generated), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
