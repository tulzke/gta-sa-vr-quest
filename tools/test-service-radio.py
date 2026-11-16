"""Compile the production service radio state, hand frames and mesh loader in a host executable."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/service-radio/tests'
OUT.mkdir(parents=True,exist_ok=True)
source=ROOT/'tools/tests/service_radio.cpp'
asset=ROOT/'assets/service-radio/runtime/service-radio.mesh'
exe=OUT/('radio.exe' if os.name=='nt' else 'radio')
if os.name=='nt':
    where=Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
    base=subprocess.check_output([str(where),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
    vc=Path(base)/'VC/Auxiliary/Build/vcvars64.bat'
    command=OUT/'run.cmd'
    command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{ROOT/"native/src"}" "{source}" /Fe:"{exe}" /Fo:"{OUT/"radio.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}" "{asset}"\n',encoding='utf8')
    subprocess.run(['cmd','/c',str(command)],cwd=OUT,check=True)
else:
    subprocess.run([os.environ.get('CXX') or shutil.which('c++'),'-std=c++20','-Wall','-Wextra','-I'+str(ROOT/'native/src'),str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe),str(asset)],check=True)
