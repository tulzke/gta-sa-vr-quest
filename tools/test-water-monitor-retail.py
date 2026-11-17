"""Check the specific Android 2.11.311 water ABI; pass an extracted retail ELF."""
from pathlib import Path
import struct
import sys

data = Path(sys.argv[1]).read_bytes()
assert data[:5] == b'\x7fELF\x02'
checks = {
    # FireTruckControl trampoline: 16 copied bytes, no PC-relative instructions.
    0x5fa414: (0xd10343ff, 0x6d072beb, 0x6d0823e9, 0xa9097bfd),
    # Native frame origin and native yaw/pitch reads.
    0x5fa7d4: (0xf9445260, 0xb40003c0, 0x9407ef11),
    0x5fa87c: (0xbd4b8260,),
    0x5fa894: (0xbd4b8660,),
    # Native source velocity and UpdateOne(uint32 vehicle id, &pos, &velocity).
    0x5fa9bc: (0x2a1303e0,),
    0x5fa9e0: (0xfd403662,),
    0x5fa9f4: (0x94084a5f,),
    # Model 407 PreRender consumes these same angles: reset X, then append Z.
    0x6af294: (0xf9445261, 0xbd4b8660, 0xaa1303e0, 0x2a1f03e2, 0x52800023, 0x94058442),
    0x6af2ac: (0xbd4b8260, 0x2a1f03e3, 0x91228268, 0x52800042),
    # FxManager Render starts by clearing one byte, restored by remote pass.
    0x4dfb08: (0x3904401f,),
}
for address, expected in checks.items():
    actual = struct.unpack_from('<' + 'I'*len(expected), data, address)
    assert actual == expected, f'water ABI mismatch at 0x{address:x}'

# ELF64 section and dynamic symbol tables, without external dependencies.
shoff = struct.unpack_from('<Q', data, 0x28)[0]
shsize, shcount = struct.unpack_from('<HH', data, 0x3a)
sections = [struct.unpack_from('<IIQQQQIIQQ', data, shoff+i*shsize) for i in range(shcount)]
symbols = {}
for section in sections:
    if section[1] != 11:
        continue
    strings = sections[section[6]]
    for at in range(section[4], section[4]+section[5], section[9]):
        name, info, other, index, value, size = struct.unpack_from('<IBBHQQ', data, at)
        start = strings[4]+name
        symbols[data[start:data.index(b'\0', start)].decode()] = (value,size)
for name, expected in {
    '_ZN11CAutomobile16FireTruckControlEP5CFire': (0x5fa414, 0x618),
    '_ZN13CWaterCannons9UpdateOneEjP7CVectorS1_': (0x5fa1d8, 352),
    '_ZN9CPhysical8GetSpeedE7CVector': (0x4a1408, 236),
    '_Z13RwFrameGetLTMP7RwFrame': (0x768858, 48),
    '_ZN11FxManager_c6RenderEP8RwCamerah': (0x4dfae4, 196),
    'g_fxMan': (0xa5bcf8, 280),
}.items():
    assert symbols[name] == expected, name
print('Retail water control/emission ABI, native nozzle PreRender and remote effects fingerprints PASS')
