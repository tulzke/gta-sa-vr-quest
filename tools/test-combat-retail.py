"""Check retail ARM64 hook fingerprints and the independent sprint-tap path."""
from pathlib import Path
import struct
import sys

data = Path(sys.argv[1]).read_bytes()
checks = {
    0x49f580: (0x79422008, 0x34000068, 0x2a1f03e0, 0xd65f03c0),
    # ControlButtonSprint -> SprintJustDown, before GetSprint.
    0x5bd624: (0x940919c7,),
    # SprintJustDown asks IsReleased(168), then IsReleased(31).
    0x49f610: (0x52801500,),
    0x49f62c: (0x528003e0,),
    0x380088: (0xd10203ff, 0x6d012beb, 0x6d0223e9, 0xa9037bfd),
    0x38179c: (0xd101c3ff, 0xa9017bfd, 0xa9026ffc, 0xa90367fa),
    # IsGlobalContextPain: unsigned short context in [0x154,0x167).
    0x381cb0: (0x12003c28, 0x51055108, 0x71004d1f, 0x1a9f27e0),
}
for address, expected in checks.items():
    actual = struct.unpack_from('<' + 'I' * len(expected), data, address)
    if actual != expected:
        raise SystemExit(f'retail combat instruction mismatch at 0x{address:x}')
print('Retail combat: independent sprint-tap path and audio hook fingerprints passed')
