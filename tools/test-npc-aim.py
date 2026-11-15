"""Run production NPC aim selection/dispatch against controlled engine callbacks."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/npc-aim-tests'
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / 'native/src/NpcAim.cpp').read_text(encoding='utf8')

def remove_function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[:start] + text[end:]

# Omit only Android installation and the return-address entry stubs. The same
# event filters are exercised below with caller addresses supplied explicitly.
for signature in ('__attribute__((noinline)) void* OnPedEvent(',
                  '__attribute__((noinline)) bool OnGroupEvent(',
                  'bool InstallHooks(', 'void Install('):
    source = remove_function(source, signature)
source = '\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
harness = (ROOT / 'tools/tests/npc_aim.cpp').read_text(encoding='utf8')
generated = OUT / 'npc_aim.cpp'
generated.write_text(harness.replace('// PRODUCTION_NPC_AIM', source), encoding='utf8')
exe = OUT / ('npc_aim.exe' if os.name == 'nt' else 'npc_aim')
if os.name == 'nt':
    vc = os.environ.get('VS_VCVARS64')
    if not vc:
        where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        vc = str(Path(base) / 'VC/Auxiliary/Build/vcvars64.bat')
    command = OUT / 'run.cmd'
    command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{ROOT / "native/src"}" "{generated}" /Fe:"{exe}" /Fo:"{OUT / "npc_aim.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n', encoding='utf8')
    subprocess.run(['cmd', '/c', str(command)], cwd=OUT, check=True)
else:
    compiler = os.environ.get('CXX') or shutil.which('c++')
    subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-I'+str(ROOT/'native/src'), str(generated), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
