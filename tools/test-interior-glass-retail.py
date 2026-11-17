"""Verify that alpha zero skips an entire mesh in both retail GL pipelines."""
from pathlib import Path
import struct
import sys
data=Path(sys.argv[1]).read_bytes()
for address,words in {
    0x743cf8:(0x39402d08,), # default pipeline: RpMaterial color alpha at +0xb
    0x743d18:(0x34fffe28,), # CBZ alpha -> next mesh, no colour or depth draw
    0x783cf0:(0x39402d08,), # MatFX/env-map pipeline
    0x783d08:(0x34fffe88,),
}.items():
    assert struct.unpack_from('<'+'I'*len(words),data,address)==words,hex(address)
print('Retail glass: alpha=0 skips mesh in default and environment-map pipelines PASS')
