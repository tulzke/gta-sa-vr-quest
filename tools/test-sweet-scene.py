"""Test SWEET5 camera timing and diagnostics against the user's retail SCM."""
from pathlib import Path
import argparse
import os
import re
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
args = argparse.ArgumentParser()
args.add_argument('--scm', type=Path, required=True)
args.add_argument('--elf', type=Path, help='Optional retail arm64 libGame.so prologue verification')
options = args.parse_args()
out = root / 'build/sweet-scene-tests'
out.mkdir(parents=True, exist_ok=True)
data = options.scm.read_bytes()
header = struct.unpack_from('<I', data, 3)[0]
header = struct.unpack_from('<I', data, header + 3)[0]
offsets = struct.unpack_from('<' + 'I' * struct.unpack_from('<H', data, header + 16)[0], data, header + 24)
fixture = data[offsets[18]:offsets[19]]
assert len(fixture) == 15739 and fixture[3:9] == b'SWEET5'
(out / 'sweet5.scm').write_bytes(fixture)
source = (root / 'native/src/MissionScenes.cpp').read_text(encoding='utf8')
if options.elf:
    elf = options.elf.read_bytes()
    address = 0x413020
    words = re.search(r'kSweetCommandPrologue\[\]\{([^}]+)', source).group(1)
    expected = struct.pack('<4I', *(int(word.strip(), 16) for word in words.split(',')))
    phoff = struct.unpack_from('<Q', elf, 32)[0]
    entry_size, count = struct.unpack_from('<HH', elf, 54)
    for i in range(count):
        kind, _, offset, virtual, _, size, _, _ = struct.unpack_from('<IIQQQQQQ', elf, phoff + i * entry_size)
        if kind == 1 and virtual <= address and address + 16 <= virtual + size:
            assert elf[offset + address - virtual:offset + address - virtual + 16] == expected
            print('PASS: retail ARM64 command-handler prologue', flush=True)
            break
    else:
        raise AssertionError('Command handler is absent from ELF load segments')
source = source.replace('#include "Log.h"', '#define LOGI(...) ((void)0)')
source = source.replace('#include <dlfcn.h>', 'void* dlsym(void*,const char*){return nullptr;}\ndouble TestNow();')
source = source.replace('/sdcard/Android/data/com.rockstargames.gtasa/files/', '')
source = re.sub(r'return std::chrono::duration<double>\(.*?\.count\(\);', 'return TestNow();', source)
(out / 'test.cpp').write_text(source + '\n' + (root / 'tools/tests/sweet_scene.cpp').read_text(encoding='utf8'), encoding='utf8')
vc = Path(os.environ.get('VS_VCVARS64', r'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'))
java = Path(os.environ['JAVA_HOME'])
(out / 'run.cmd').write_text(
    f'@echo off\ncall "{vc}" >nul\n'
    f'cl /nologo /std:c++20 /EHsc /W3 /D_CRT_SECURE_NO_WARNINGS /I"{root / "native/src"}" '
    f'/I"{java / "include"}" /I"{java / "include/win32"}" test.cpp /Fe:test.exe /Fo:test.obj\n'
    'if errorlevel 1 exit /b 1\ntest.exe\n', encoding='utf8')
result = subprocess.run(['cmd', '/c', str(out / 'run.cmd')], cwd=out)
raise SystemExit(result.returncode)
