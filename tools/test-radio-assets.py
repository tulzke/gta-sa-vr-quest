"""Check static radio UXRH structure and texture metadata without Blender."""
import json
import math
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[1] / 'assets/radio/runtime'
manifest = json.loads((root / 'manifest.json').read_text(encoding='utf8'))
for name, info in manifest['parts'].items():
    data = (root / f'{name}.uxrh').read_bytes()
    magic, version, vertices, indices = struct.unpack_from('<4sIII', data)
    assert magic == b'UXRH' and version == 1
    assert vertices == info['vertices'] and indices == info['triangles'] * 3
    assert vertices and indices and len(data) == 16 + vertices * 104 + indices * 2
    for vertex in range(vertices):
        values = struct.unpack_from('<26f', data, 16 + vertex * 104)
        assert all(math.isfinite(value) for value in values)
        assert values[:3] == values[3:6] == values[6:9] == values[9:12]
        assert values[12:15] == values[15:18] == values[18:21] == values[21:24]
        assert .99 < sum(value * value for value in values[12:15]) < 1.01
    faces = struct.unpack_from(f'<{indices}H', data, 16 + vertices * 104)
    assert max(faces) < vertices
    png = (root / info['texture']).read_bytes()
    assert png[:8] == b'\x89PNG\r\n\x1a\n'
    assert list(struct.unpack_from('>II', png, 16)) == info['texture_size']
    print(f'{name}: {vertices} vertices, {indices // 3} triangles, PNG {info["texture_size"]}')
