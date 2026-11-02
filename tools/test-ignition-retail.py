"""Verify the retail ARM64 instructions used by the ignition research.

Pass an extracted libGame.so; the game binary is not part of the repository.
This checks the specific 2.11.311 evidence, not behavior on a headset.
"""
from pathlib import Path
import struct
import sys


def main():
    if len(sys.argv) != 2:
        raise SystemExit('usage: test-ignition-retail.py path/to/libGame.so')
    data = Path(sys.argv[1]).read_bytes()
    # In this retail ELF the .text file offset equals its virtual address.
    checks = {
        # CTaskSimpleCarSetPedInAsDriver::ProcessPed: preserve other flags,
        # set/clear engine bit depending on the existing bit 41.
        0x646DC0: (0xF942B509, 0xAA1303E0, 0x52800641, 0x9256F92A,
                   0x927BF92B, 0xF257013F, 0xB27C014A, 0x9A8B0149,
                   0xF902B509),
        # Automobile control reads byte +0x568 and branches on bit 4.
        0x6A88B8: (0x3955A26A, 0x362001EA),
    }
    for address, expected in checks.items():
        actual = struct.unpack_from('<' + 'I' * len(expected), data, address)
        if actual != expected:
            raise SystemExit(f'instruction mismatch at 0x{address:x}')
    print('Retail ignition instruction checks passed')


if __name__ == '__main__':
    main()
