"""Run the production lever and native nitro adapter with fake retail memory."""
from pathlib import Path
import os
import subprocess
import shutil
import struct
import zipfile

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/nitro-tests';OUT.mkdir(exist_ok=True)
production=(ROOT/'native/src/Driving.cpp').read_text(encoding='utf8')
start=production.index('bool NitroCompatible(int model){')
end=production.index('\nvoid SetNitroPose',start)
source=OUT/'vehicle_nitro.cpp'
cheats=(ROOT/'native/src/Cheats.cpp').read_text(encoding='utf8')
cheat_start=cheats.index('void (*g_loadUpgradeModels)')
cheat_end=cheats.index('// Dev-only: while ON',cheat_start)
source.write_text((ROOT/'tools/tests/vehicle_nitro.cpp').read_text(encoding='utf8')
    .replace('// COMPATIBILITY_FUNCTION',production[start:end])
    .replace('// NITRO_CHEAT_FUNCTIONS',cheats[cheat_start:cheat_end]),encoding='utf8')
exe=OUT/('nitro.exe'if os.name=='nt'else'nitro')
if os.name=='nt':
    where=Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
    base=subprocess.check_output([str(where),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
    vc=Path(base)/'VC/Auxiliary/Build/vcvars64.bat'
    command=OUT/'run.cmd'
    command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{ROOT/"native/src"}" "{source}" /Fe:"{exe}" /Fo:"{OUT/"nitro.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}" "{ROOT/"assets/nitro-lever/runtime/nitro.mesh"}"\n',encoding='utf8')
    subprocess.run(['cmd','/c',str(command)],cwd=OUT,check=True)
else:
    subprocess.run([shutil.which('c++'),'-std=c++20','-I'+str(ROOT/'native/src'),str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe),str(ROOT/'assets/nitro-lever/runtime/nitro.mesh')],check=True)
with zipfile.ZipFile(ROOT/'build/quest-deploy/template/split_config.arm64_v8a.apk')as z:elf=z.read('lib/arm64-v8a/libGame.so')
assert struct.unpack_from('<4I',elf,0x6aa3e0)==(0xa9be7bfd,0xa9014ff4,0x910003fd,0x39416808)
assert struct.unpack_from('<7I',elf,0x5354c4)==(0xf9404808,0x2a0103e9,0x8b091508,0xb940d108,0x2a2803e8,0x531f7d00,0xd65f03c0)
assert struct.unpack_from('<15I',elf,0x6aa558)==(0x1e2e1002,0xbd460a63,0x1e2a1004,0x52824de8,0xaa1303e0,0x72a75068,0x1e233843,0x1e270105,0x1e250821,0x1e242060,0x1e234c83,0x1f030020,0x1e222000,0x1e20cc41,0x1e213840)
print('PASS retail ABI: exact NitroControl prologue, throttle-dependent recovery formula and read-only compatibility predicate')
