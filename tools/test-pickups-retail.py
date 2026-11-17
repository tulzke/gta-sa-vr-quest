"""Verify the contact bridge, pool and delayed-weapon ABI against the user's ELF."""
from pathlib import Path
import struct
import sys
import re

data = Path(sys.argv[1]).read_bytes()
assert data[:5] == b'\x7fELF\x02'
for address, expected in {
    0x403674: (0xd10283ff, 0xfd0013ea, 0x6d0323e9, 0xa9047bfd),
    # Common contact result w0 is ANDed with the object's enabled bit.
    # The bridge replays these four instructions and returns before AND w25.
    0x403cd0: (0xf9400288, 0xb941a508, 0x7207011f, 0x1a9f17e8, 0x0a000119),
    # Retail generation at +0x1e, pool stride 0x24; removal sets type=0.
    0x40491c: (0x5280048a, 0x9b2a2128, 0x79403d08),
    0x404954: (0x39408668, 0x3900827f, 0x2a1603e0, 0x32000108, 0x39008668),
    # One pending type/ammo pair at ped+8d4/8d8, empty sentinel 55.
    0x58db84: (0xb948d688, 0x7100dd1f, 0x54000101, 0xb908d695),
    0x58db98: (0xb908da93,),
    # Selection side effect at CPlayerData+2c must be restored on ammo-only collection.
    0x4049fc: (0xf942a2a8, 0x3900b100),
    # Pickups need not allocate CMatrix: +18 null selects CSimpleTransform at +8.
    0x417950: (0xf9401009, 0xb40000e9, 0xf9400c08, 0xf9400529, 0xb40000a8),
    0x417974: (0x91002000, 0x91008121),
    0x48bfe0: (0xf9401008, 0xb4000068, 0xf9400500),
    0x4ac9c4: (0xbd400c00,), # simple heading at entity+8+c
}.items():
    actual = struct.unpack_from('<'+'I'*len(expected), data, address)
    assert actual == expected, f'pickup retail ABI mismatch at {address:#x}: {actual}'

shoff = struct.unpack_from('<Q', data, 0x28)[0]
shsize, shcount = struct.unpack_from('<HH', data, 0x3a)
sections = [struct.unpack_from('<IIQQQQIIQQ', data, shoff+i*shsize) for i in range(shcount)]
symbols = {}
for section in sections:
    if section[1] != 11:
        continue
    strings = sections[section[6]]
    for at in range(section[4], section[4]+section[5], section[9]):
        name, _, _, _, value, size = struct.unpack_from('<IBBHQQ', data, at)
        start = strings[4]+name
        symbols[data[start:data.index(b'\0', start)].decode()] = (value, size)

# Exercise the real import names from the installer, not hand-written copies.
# Runtime fixtures replace Install(), so they cannot catch a typo that disables
# the entire module before any rise, candidate or icon code can execute.
runtime = (Path(__file__).resolve().parents[1]/'native/src/Pickups.cpp').read_text(encoding='utf8')
installer = runtime[runtime.index('void Install(void* handle)'):runtime.index('void ProbeGrabs(')]
imports = re.findall(r'\bdlsym\s*\(\s*handle\s*,\s*"([^"]+)"\s*\)', installer)
assert imports, 'No production pickup imports found'
missing = [name for name in imports if name not in symbols]
assert not missing, f'Pickup installer imports missing from retail ELF: {missing}'
for name, expected in {
    '_ZN7CPickup6UpdateEP10CPlayerPedP8CVehiclei': (0x403674, 0x1638),
    '_ZN8CPickups8aPickUpsE': (0x983b00, 620*0x24),
    'CollectPickupBuffer': (0x989524, 4),
    '_ZN4CPed17GiveDelayedWeaponE11eWeaponTypej': (0x58db2c, 0x90),
    '_ZN7CEntity14UpdateRwMatrixEv': (0x417950, 0x30),
    '_ZN7CEntity13UpdateRwFrameEv': (0x48bfe0, 0x14),
    '_ZN6CTimer22m_snTimeInMillisecondsE': (0xa1a33c, 4),
}.items():
    assert symbols[name] == expected, name
print('Retail pickup installer imports, contact gate, pool identity and delayed grant ABI: passed')
