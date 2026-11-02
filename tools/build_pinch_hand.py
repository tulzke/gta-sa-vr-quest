#!/usr/bin/env python3
"""Convert an evaluated hand glTF binary into a matching UXRH pose.

Export only the posed hand mesh from Blender to GLB, applying its armature
and preserving UVs. Pass the game hand as reference; vertex order may differ.
"""
import argparse
import json
import math
from pathlib import Path
import struct


def read_glb(path):
    data = path.read_bytes()
    magic, version, size = struct.unpack_from('<4sII', data)
    if (magic, version, size) != (b'glTF', 2, len(data)):
        raise ValueError('Invalid GLB header')
    length, kind = struct.unpack_from('<I4s', data, 12)
    if kind != b'JSON':
        raise ValueError('Missing GLB JSON')
    doc = json.loads(data[20:20 + length])
    binary = 20 + length
    binary_length, kind = struct.unpack_from('<I4s', data, binary)
    if kind != b'BIN\x00' or binary + 8 + binary_length > len(data):
        raise ValueError('Missing GLB binary buffer')
    binary += 8

    def accessor(index):
        entry = doc['accessors'][index]
        view = doc['bufferViews'][entry['bufferView']]
        count = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3}[entry['type']]
        code = {5126: 'f', 5123: 'H', 5125: 'I'}[entry['componentType']]
        fmt = '<' + code * count
        stride = view.get('byteStride', struct.calcsize(fmt))
        start = binary + view.get('byteOffset', 0) + entry.get('byteOffset', 0)
        return [struct.unpack_from(fmt, data, start + i * stride)
                for i in range(entry['count'])]

    primitive = doc['meshes'][0]['primitives'][0]
    attrs = primitive['attributes']
    return (accessor(attrs['POSITION']), accessor(attrs['NORMAL']),
            accessor(attrs['TEXCOORD_0']), accessor(primitive['indices']))


def key(uv):
    return tuple(round(v, 5) for v in uv)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('glb', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--side', choices=('left', 'right'), default='right')
    args = parser.parse_args()
    output = bytearray(args.reference.read_bytes())
    magic, version, vertices, indices = struct.unpack_from('<4sIII', output)
    if magic != b'UXRH' or version != 1 or len(output) != 16 + vertices * 104 + indices * 2:
        raise ValueError('Unsupported reference hand')
    pos, norm, uv, triangles = read_glb(args.glb)
    lookup = {key(value): i for i, value in enumerate(uv)}
    if len(lookup) != vertices or len(uv) != vertices:
        raise ValueError('UVs are not unique or vertex counts differ')
    mapping = [lookup[key(struct.unpack_from('<2f', output, 16 + i * 104 + 96))]
               for i in range(vertices)]
    old_indices = struct.unpack_from('<' + str(indices) + 'H', output, 16 + vertices * 104)
    reference_faces = sorted(tuple(sorted(mapping[i] for i in old_indices[j:j+3]))
                             for j in range(0, indices, 3))
    glb_indices = [entry[0] for entry in triangles]
    glb_faces = sorted(tuple(sorted(glb_indices[j:j+3]))
                       for j in range(0, len(glb_indices), 3))
    if reference_faces != glb_faces:
        raise ValueError('Mesh topology differs from the reference')
    for i, source in enumerate(mapping):
        x, y, z = pos[source]
        nx, ny, nz = norm[source]
        # glTF object-space coordinates use centimetres. Left and right game
        # meshes use opposite handedness in their local Y coordinate.
        side = 1 if args.side == 'left' else -1
        xyz = (x * .01, side * z * .01, y * .01)
        normal = (nx, side * nz, ny)
        if not all(math.isfinite(v) for v in xyz + normal):
            raise ValueError('Non-finite vertex')
        for pose in range(4):
            struct.pack_into('<3f', output, 16 + i * 104 + pose * 12, *xyz)
            struct.pack_into('<3f', output, 16 + i * 104 + 48 + pose * 12, *normal)
    args.output.write_bytes(output)
    print(f'Wrote {args.output}: {vertices} vertices, {indices // 3} triangles')


if __name__ == '__main__':
    main()
