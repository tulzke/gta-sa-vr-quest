"""Inspect the authored nozzle pivot without importing or changing game assets."""
from pathlib import Path
import hashlib
import json
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import build_aircraft_hlod as rw


def inspect(path):
    data = Path(path).read_bytes()
    clump_end = 12 + struct.unpack_from('<I', data, 4)[0]
    frames = rw.first_chunk(data, 12, clump_end, 0xe)
    children = list(rw.iter_chunks(data, frames.body, frames.end))
    table = next(c for c in children if c.kind == 1)
    extensions = [c for c in children if c.kind == 3]
    count = struct.unpack_from('<I', data, table.body)[0]
    nodes = []
    for i in range(count):
        transform = struct.unpack_from('<12fiI', data, table.body + 4 + i*56)
        name = ''
        for chunk in rw.iter_chunks(data, extensions[i].body, extensions[i].end):
            if chunk.kind == 0x253f2fe:
                name = data[chunk.body:chunk.end].split(b'\0')[0].decode('ascii')
        nodes.append((name, transform[:12], transform[12]))
    index = next(i for i, node in enumerate(nodes) if node[0] == 'misc_a')
    position = [0., 0., 0.]
    visited = set()
    chain = []
    while index >= 0:
        assert index not in visited, 'cyclic frame hierarchy'
        visited.add(index)
        name, transform, index = nodes[index]
        chain.append(name)
        position = [transform[9+k] + sum(position[j]*transform[j*3+k] for j in range(3)) for k in range(3)]
    return {'source_sha256': hashlib.sha256(data).hexdigest(), 'frame': 'misc_a',
            'parent_chain': chain, 'vehicle_local_pivot_m': position,
            'axes': ['right', 'forward', 'up']}


if __name__ == '__main__':
    report = inspect(sys.argv[1])
    expected = (0., 1.63911438, 1.64957905)
    assert all(abs(a-b) < 1e-6 for a, b in zip(report['vehicle_local_pivot_m'], expected)), 'shipped pivot changed: review runtime fallback'
    print(json.dumps(report, indent=2))
