"""Production detents/button/native hook tests and Blender-to-C++ deformation comparison."""
from pathlib import Path
import os
import shutil
import struct
import subprocess
import zipfile

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/hydraulics-tests';OUT.mkdir(exist_ok=True)
settings=(ROOT/'native/src/VehicleHydraulicsSettings.inl').read_text()
compatible=settings[:settings.index('\nvoid SetHydraulicsPose')]
driving=(ROOT/'native/src/VehicleHydraulicsDriving.inl').read_text()
equipment=driving[driving.index('bool HydroEquipment'):driving.index('\nbool BuildHydraulicsTracking')]
source=OUT/'vehicle_hydraulics.cpp'
source.write_text((ROOT/'tools/tests/vehicle_hydraulics.cpp').read_text().replace('// COMPATIBILITY_FUNCTION',compatible).replace('// EQUIPMENT_FUNCTION',equipment))
exe=OUT/('hydraulics.exe' if os.name=='nt' else 'hydraulics')
mesh=ROOT/'assets/hydraulics/runtime/hydraulics.mesh';fixture=OUT/'blender-reference.bin'
hands=ROOT/'assets/vrhands/skeletal/hands.glb';skeleton=ROOT/'native/src/HandSkeleton.cpp'
if os.name=='nt':
    where=Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
    base=subprocess.check_output([str(where),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
    vc=Path(base)/'VC/Auxiliary/Build/vcvars64.bat';command=OUT/'run.cmd'
    command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /D_CRT_SECURE_NO_WARNINGS /I"{ROOT/"native/src"}" "{source}" "{skeleton}" /Fe:"{exe}"\nif errorlevel 1 exit /b 1\n"{exe}" "{mesh}" "{fixture}" "{hands}"\n')
    subprocess.run(['cmd','/c',str(command)],cwd=OUT,check=True)
else:
    subprocess.run([shutil.which('c++'),'-std=c++20','-I'+str(ROOT/'native/src'),str(source),str(skeleton),'-o',str(exe)],check=True)
    subprocess.run([str(exe),str(mesh),str(fixture),str(hands)],check=True)
with zipfile.ZipFile(ROOT/'build/quest-deploy/template/split_config.arm64_v8a.apk') as z:elf=z.read('lib/arm64-v8a/libGame.so')
for offset,words in [(0x6a1f8c,(0xd10403ff,0x6d063bef,0x6d0733ed,0x6d082beb)),(0x49d76c,(0xa9be7bfd,0xf9000bf3,0x910003fd,0x79422008)),(0x378d5c,(0xa9bd7bfd,0xf9000bf5,0xa9024ff4,0x910003fd))]:
    assert struct.unpack_from('<4I',elf,offset)==words,hex(offset)
print('PASS exact target ARM64 hook prologues for hydraulic control, horn edge and beat output')
