"""Verify aim-event ABI, emitter coverage and hook fingerprints in retail arm64 ELF."""
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
data = Path(sys.argv[1]).read_bytes()
assert data[:5] == b'\x7fELF\x02' and struct.unpack_from('<H', data, 18)[0] == 183
shoff = struct.unpack_from('<Q', data, 40)[0]
shsize, shnum = struct.unpack_from('<HH', data, 58)
sections = [struct.unpack_from('<IIQQQQIIQQ', data, shoff+i*shsize) for i in range(shnum)]
symbols = {}
for section in sections:
    if section[1] != 11:
        continue
    names = sections[section[6]]
    strings = data[names[4]:names[4]+names[5]]
    for offset in range(section[4], section[4]+section[5], section[9]):
        name, _, _, _, address, size = struct.unpack_from('<IBBHQQ', data, offset)
        symbol = strings[name:strings.find(b'\0', name)].decode()
        symbols[symbol] = (address, size)

def words(address, expected):
    # Map virtual addresses through ELF sections instead of assuming VA=file offset.
    section = next(s for s in sections if s[3] <= address < s[3]+s[5] and s[1] != 8)
    offset = section[4]+address-section[3]
    actual = struct.unpack_from('<'+'I'*len(expected), data, offset)
    assert actual == tuple(expected), f'ABI mismatch at {address:#x}: {actual}'

source = (ROOT/'native/src/NpcAim.cpp').read_text(encoding='utf8')
for name in re.findall(r'(?:LOAD\(\w+,\s*|dlsym\(handle,\s*)"([^"]+)"', source):
    assert name in symbols, f'Missing exported symbol: {name}'
emitter_names = re.findall(r'emitters\[\d\] = \{dlsym\(handle, "([^"]+)"\), (0x[0-9a-f]+)\}', source)
assert len(emitter_names) == 4
fingerprints = re.search(r'expected\[4\]\[4\]\{(.*?)\};', source, re.S).group(1)
fingerprints = [int(x,16) for x in re.findall(r'0x[0-9a-f]+', fingerprints)]
for i, (name, size) in enumerate(emitter_names):
    address, native_size = symbols[name]
    assert native_size == int(size,16), f'Emitter bounds changed: {name}'
    words(address, fingerprints[i*4:i*4+4])

checks = {
    # Trampoline prologues: stack/register operations only.
    0x4e2404: (0xa9bb7bfd, 0xf9000bf9, 0xa9025ff8, 0xa90357f6),
    0x5a0090: (0xa9bd7bfd, 0xf9000bf5, 0xa9024ff4, 0x910003fd),
    # Event source and its reference lifecycle; native wrapper owns nested event.
    0x4eb538: (0xf8018d01,),  # source +0x18
    0x4eb54c: (0x940c2ab9,),  # RegisterReference
    0x4eb59c: (0x940c2a65,),  # CleanUpOldReference
    0x4e849c: (0xf9000c02,),  # wrapper nested +0x18
    0x4e84a8: (0xf8010d01,),  # wrapper target +0x10
    0x4e84f8: (0xf9400008, 0xf9400501),  # nested deleting destructor vtable +8
    0x4ef2fc: (0xf9401529,),  # deep clone of nested event, vtable +0x28
    # Helper excludes active ReactToGunAimedAt (601), before speech or event allocation.
    0x6890f4: (0x7109641f, 0x540005e0),
    0x69d120: (0x52804b20,),
    # Native dispatch group/intelligence offsets and valid=false.
    0x689174: (0x910182a0,),  # group +0x60
    0x68919c: (0x91032100,),  # intelligence +0xc8
    0x6891a4: (0x2a1f03e2,),
    # Native target range at +4, skill flags at +0x18; not ammo checks.
    0x5bbb90: (0xbd400400,),
    0x5bbd58: (0x39406008,),
}
for address, expected in checks.items():
    words(address, expected)
for array, symbol in [('gunCode','_ZN16CEventGunAimedAtC1EP4CPed'),
                      ('groupCode','_ZN16CEventGroupEventC1EP4CPedP6CEvent')]:
    # Use the install-time layout check (last declaration; hook function also has groupCode).
    values = re.findall(rf'const std::uint32_t {array}\[\]\{{([^}}]+)\}};', source)[-1]
    words(symbols[symbol][0], [int(x,16) for x in re.findall(r'0x[0-9a-f]+',values)])
print('Retail NPC aim: exported symbols, four emitter bounds, event ownership/layout and verified ARM64 hooks passed')
