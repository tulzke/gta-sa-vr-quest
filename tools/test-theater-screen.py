"""Host regression tests for theater geometry and frontend touch transitions."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
out = root / 'build/theater-tests'
out.mkdir(parents=True, exist_ok=True)
vc = next(path for base in ('C:/Program Files', 'C:/Program Files (x86)')
          for path in Path(base).glob('Microsoft Visual Studio/*/*/VC/Auxiliary/Build/vcvars64.bat'))
script = out / 'run.cmd'
script.write_text(
    f'@echo off\ncall "{vc}" >nul\n'
    f'cl /nologo /std:c++20 /EHsc /W4 /I"{root / "native/src"}" '
    f'"{root / "tools/tests/theater_screen.cpp"}" '
    f'/Fe:"{out / "test.exe"}" /Fo:"{out / "test.obj"}"\n'
    f'if errorlevel 1 exit /b 1\n"{out / "test.exe"}"\n', encoding='utf8')
sys.exit(subprocess.run(['cmd', '/c', str(script)], cwd=out).returncode)
