"""Exercise native rhythm snapshots, suspension/scoring isolation and the actual strip mesh."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/rhythm-tests';OUT.mkdir(exist_ok=True)
native=(ROOT/'native/src/Hydraulics.cpp').read_text(encoding='utf8')
native=native.replace('#include "Log.h"','').replace('#include <dlfcn.h>','')
source=OUT/'rhythm.cpp'
source.write_text('#define LOGI(...) ((void)0)\nvoid* dlsym(void*,const char*){return nullptr;}\n'+native+'\n'+(ROOT/'tools/tests/rhythm_hud.cpp').read_text(encoding='utf8'),encoding='utf8')
exe=OUT/('rhythm.exe' if os.name=='nt' else 'rhythm')
if os.name=='nt':
    where=Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
    base=subprocess.check_output([str(where),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
    vc=Path(base)/'VC/Auxiliary/Build/vcvars64.bat'
    command=OUT/'run.cmd'
    command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{ROOT/"native/src"}" "{source}" /Fe:"{exe}"\nif errorlevel 1 exit /b 1\n"{exe}" "{OUT}"\n')
    subprocess.run(['cmd','/c',str(command)],cwd=OUT,check=True)
else:
    subprocess.run([shutil.which('c++'),'-std=c++20','-I'+str(ROOT/'native/src'),str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe),str(OUT)],check=True)
