"""Run the production wanted observer, animation and shared halo priority on the host."""
from pathlib import Path
import runpy
import struct
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'native/src/WantedFeedback.cpp').read_text(encoding='utf8')
source = source.replace('#include "VrCamera.h"', '').replace('#include "Xr.h"', '')
start = source.index('double Now() {')
end = source.index('\n}', start) + 2
source = source[:start] + 'double Now() { return testNow; }' + source[end:]
test = (ROOT / 'tools/tests/wanted_feedback.cpp').read_text(encoding='utf8')
helpers = runpy.run_path(str(ROOT / 'tools/test-bike-steering.py'))
with tempfile.TemporaryDirectory(prefix='savr-wanted-') as tmp:
    output = Path(tmp)
    cpp = output / 'wanted.cpp'
    cpp.write_text(test.replace('// PRODUCTION_WANTED', source), encoding='utf8')
    helpers['compile_run'](cpp, 'wanted', output)

if len(sys.argv) > 1:
    binary = Path(sys.argv[1]).read_bytes()
    # ArrestPlayer writes state 2 at CPlayerInfo+0x118; dead/failed are 1/3.
    for address, expected in {
        0x4b0620: (0x39446008, 0x34000048, 0xd65f03c0),
        0x4b0634: (0x52800048, 0x3906401f, 0x39046008),
        0x4b05dc: (0x52800028, 0x39046008),
        0x4b066c: (0x52800068,),
        0x4b0678: (0x39046008,),
        # PlayerPed getter: player-info stride 0x1d8 and ped pointer at +0.
        0x5bc784: (0xf940ed09, 0xf940010a, 0x9107610b),
    }.items():
        actual = struct.unpack_from('<' + 'I' * len(expected), binary, address)
        if actual != expected:
            raise SystemExit(f'wanted feedback retail layout mismatch at 0x{address:x}')
    print('PASS: retail player-info state and stride fingerprints')
