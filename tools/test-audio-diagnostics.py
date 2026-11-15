"""Exercise production audio sampling with fake AL state; never touch an AL error flag."""
from pathlib import Path
import os
import shutil
import subprocess
import re
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/audio-diagnostics-tests'
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / 'native/src/AudioDiagnostics.cpp').read_text(encoding='utf8')

# Check the guard against the actual retail engine used for local deployment.
# This catches version/offset errors independently of the host AL mocks below.
apk = ROOT / 'build/quest-deploy/template/split_config.arm64_v8a.apk'
if apk.exists():
    with zipfile.ZipFile(apk) as z:
        elf = z.read('lib/arm64-v8a/libGame.so')
    assert elf[:6] == b'\x7fELF\x02\x01', 'Expected little-endian ELF64'
    shoff = struct.unpack_from('<Q', elf, 40)[0]
    shsize, shnum = struct.unpack_from('<HH', elf, 58)
    sections = [struct.unpack_from('<IIQQQQIIQQ', elf, shoff+i*shsize) for i in range(shnum)]
    symbol = b'_ZN16CAEAudioHardware18InitOpenALListenerEv'
    function = None
    for section in sections:
        if section[1] != 11:  # SHT_DYNSYM
            continue
        names_section = sections[section[6]]
        names = elf[names_section[4]:names_section[4]+names_section[5]]
        for off in range(section[4], section[4]+section[5], section[9]):
            name, _, _, _, value, _ = struct.unpack_from('<IBBHQQ', elf, off)
            if names[name:].split(b'\0', 1)[0] == symbol:
                function = value
    assert function is not None, 'Retail audio initializer missing'
    phoff = struct.unpack_from('<Q', elf, 32)[0]
    phsize, phnum = struct.unpack_from('<HH', elf, 54)
    address = function + 0x24
    offset = None
    for i in range(phnum):
        kind, _, start, vaddr, _, size, _, _ = struct.unpack_from('<IIQQQQQQ', elf, phoff+i*phsize)
        if kind == 1 and vaddr <= address and address+16 <= vaddr+size:
            offset = start + address-vaddr
    assert offset is not None
    header = (ROOT/'native/src/AudioOutputRate.h').read_text(encoding='utf8')
    guard = re.search(r'kRetailAttributes\{([^}]+)', header).group(1)
    expected = tuple(int(x, 16) for x in re.findall(r'0x([0-9a-f]+)u', guard))
    assert struct.unpack_from('<4I', elf, offset) == expected, 'Retail context attribute fingerprint mismatch'
    assert (expected[1] >> 5) & 0xffff == 22050
    print('Retail ELF: symbol, context attribute fingerprint and original 22050 Hz verified')

def replace_function(text, signature, replacement):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[:start] + replacement + text[end:]

source = '\n'.join(line for line in source.splitlines() if line not in (
    '#include <dlfcn.h>', '#include <unistd.h>', '#include "Log.h"'))
source = replace_function(source, 'double Now()', 'double Now() { return fakeNow; }')
source = replace_function(source, 'FILE* Open()', 'FILE* Open() { return std::tmpfile(); }')
source = replace_function(source, 'bool Resolve()', 'bool Resolve() { return api.ready; }')
source = replace_function(source, 'void Configure()', 'void Configure() {}')
prefix = r'''
#include <cstdio>
#include <cstdlib>
#include <string>
#include <limits>
double fakeNow = 10.;
std::string output;
int CaptureClose(FILE* file) {
    std::fflush(file); std::rewind(file);
    char line[2048]; while (std::fgets(line, sizeof(line), file)) output += line;
    return std::fclose(file);
}
namespace std { int CaptureClose(FILE* f) { return ::CaptureClose(f); } }
#define fclose CaptureClose
#define LOGE(...) ((void)0)
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); std::exit(1); } } while (false)
'''
tests = r'''
#undef fclose
using namespace savr::audiodiag;
bool contextPresent = true;
int queries = 0;
void* Context() { return contextPresent ? reinterpret_cast<void*>(1) : nullptr; }
void* Device(void*) { return reinterpret_cast<void*>(2); }
void DeviceInt(void*, int param, int, int* out) { *out = param == 0x1007 ? 22050 : 0; }
const char* String(int) { return "fake-openal"; }
void Listener(int, float* out) { *out = .5f; }
char IsSource(unsigned id) { ++queries; return id == 1 || id == 1024; }
void SourceInt(unsigned id, int param, int* out) {
    *out = 0;
    if (param == 0x1010) *out = id == 1 ? 0x1014 : 0x1012;
    if (param == 0x1027) *out = id == 1 ? 0x1029 : 0x1028;
    if (param == 0x1015 || param == 0x1016) *out = id == 1 ? 3 : 1;
    if (param == 0x1009) *out = 2;
}
void SourceFloat(unsigned id, int param, float* out) {
    *out = (id == 1024 && param == 0x1003) ? std::numeric_limits<float>::quiet_NaN() : 1.f;
}
void BufferInt(unsigned, int param, int* out) { *out = param == 0x2001 ? 44100 : 16; }
int main() {
    using namespace savr::audiodiag::outputrate;
    CHECK(!Matches(nullptr));
    auto words = kRetailAttributes;
    CHECK(Matches(words.data()));
    for (unsigned i = 0; i < words.size(); ++i) {
        words = kRetailAttributes; words[i] ^= 1;
        CHECK(!Matches(words.data()));
    }
    CHECK(((k48kInstruction >> 5) & 0xffff) == 48000);
    CHECK((k48kInstruction & ~0x1fffe0u) == (kRetailAttributes[1] & ~0x1fffe0u));
    PrepareGameAudio(nullptr);
    CHECK(output.find("OUTPUT_RATE requested=22050 patch=0 rx_restored=1 status=stock") != std::string::npos);
    output.clear();
    api = {Context, Device, DeviceInt, String, Listener, IsSource, SourceInt, SourceFloat, BufferInt, true};
    Tick(true, true, false, false);
    CHECK(queries == 1024);
    CHECK(output.find("frequency=22050") != std::string::npos);
    CHECK(output.find("requested_hz=22050 rate_patch=0 rx_restored=1 rate_match=1") != std::string::npos);
    CHECK(output.find("sources=2 playing=1 stopped=1 streaming=1 drained=1 nonfinite=1") != std::string::npos);
    CHECK(output.find("queued=3 processed=3") != std::string::npos);
    CHECK(output.find("hz=44100") != std::string::npos);
    output.clear(); fakeNow = 11.; Tick(true, true, false, false);
    CHECK(output.empty() && queries == 1024);
    fakeNow = 15.; Tick(false, true, true, true);
    CHECK(queries == 2048);
    CHECK(output.find("focus=0 stereo=1 menu=1 pause=1") != std::string::npos);
    CHECK(output.find("max_frame_gap_ms=4000.00 gaps_gt100ms=2") != std::string::npos);
    output.clear(); contextPresent = false; fakeNow = 20.;
    Tick(true, true, false, false);
    CHECK(output.find("api=1 context=") != std::string::npos && queries == 2048);
    output.clear(); api.ready = false; fakeNow = 25.;
    Tick(true, false, false, false);
    CHECK(output.find("api=0 context=") != std::string::npos && queries == 2048);
    std::puts("Audio diagnostics: throttling, stream drain, nonfinite pitch, frame gaps and unavailable context passed");
}
'''
generated = OUT / 'audio.cpp'
generated.write_text(prefix + source + tests, encoding='utf8')
exe = OUT / ('audio.exe' if os.name == 'nt' else 'audio')
if os.name == 'nt':
    where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    vc = str(Path(base) / 'VC/Auxiliary/Build/vcvars64.bat')
    command = OUT / 'run.cmd'
    command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /I"{ROOT / "native/src"}" "{generated}" /Fe:"{exe}" /Fo:"{OUT / "audio.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n', encoding='utf8')
    subprocess.run(['cmd', '/c', str(command)], cwd=OUT, check=True)
else:
    subprocess.run([shutil.which('c++'), '-std=c++20', '-I'+str(ROOT/'native/src'), str(generated), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
