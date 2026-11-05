"""Host-test production melee sweeps and damage hooks with fake engine memory."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/combat-tests'
OUT.mkdir(parents=True, exist_ok=True)

def replace_function(text, signature, replacement=''):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[:start] + replacement + text[end:]

combat = (ROOT / 'native/src/Combat.cpp').read_text(encoding='utf8')
combat = replace_function(combat, 'bool Hook(void* target)')
combat = replace_function(combat, 'void Install(void* handle)')
combat = replace_function(combat, 'double Now()', 'double Now() { return testNow; }')
melee = (ROOT / 'native/src/Melee.cpp').read_text(encoding='utf8')
melee = replace_function(melee, 'void Install(void* handle)')
melee = replace_function(melee, 'double NowSeconds()', 'double NowSeconds() { return testNow; }')
combat = '\n'.join(line for line in combat.splitlines() if not line.startswith('#include'))
melee = '\n'.join(line for line in melee.splitlines() if not line.startswith('#include'))
harness = (ROOT / 'tools/tests/combat.cpp').read_text(encoding='utf8')
generated = OUT / 'combat.cpp'
generated.write_text(harness.replace('// PRODUCTION_COMBAT', combat).replace('// PRODUCTION_MELEE', melee), encoding='utf8')
exe = OUT / ('combat.exe' if os.name == 'nt' else 'combat')
if os.name == 'nt':
    vc = os.environ.get('VS_VCVARS64')
    if not vc:
        where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        vc = str(Path(base) / 'VC/Auxiliary/Build/vcvars64.bat')
    command = OUT / 'run.cmd'
    command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{ROOT / "native/src"}" "{generated}" /Fe:"{exe}" /Fo:"{OUT / "combat.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n', encoding='utf8')
    subprocess.run(['cmd', '/c', str(command)], cwd=OUT, check=True)
else:
    subprocess.run([os.environ.get('CXX') or shutil.which('c++'), '-std=c++20', '-Wall', '-Wextra', '-I'+str(ROOT/'native/src'), str(generated), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
