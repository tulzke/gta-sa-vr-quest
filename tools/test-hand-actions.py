"""Validate locally exported hand Action samples (no Blender required).

First run tools/export_hand_actions.py in Blender; source and generated assets
are intentionally local and are not required for unrelated project tests.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[1]


def read_block(root, entry):
    path = (root / entry['path']).resolve()
    assert path.is_relative_to(root.resolve()), 'Block outside export directory'
    data = path.read_bytes()
    assert len(data) == entry['size']
    assert hashlib.sha256(data).hexdigest() == entry['sha256']
    magic, version, vc, ic = struct.unpack_from('<4sIII', data)
    assert magic == b'UXRH' and version == 1
    assert len(data) == 16 + vc * 104 + ic * 2
    vertices = [struct.unpack_from('<26f', data, 16+i*104) for i in range(vc)]
    indices = struct.unpack_from(f'<{ic}H', data, 16+vc*104)
    assert max(indices) < vc and ic % 3 == 0
    for vertex in vertices:
        assert all(math.isfinite(x) for x in vertex)
        for offset in (12, 15, 18, 21):
            assert abs(sum(v*v for v in vertex[offset:offset+3])-1) < 1e-5
    return vertices, indices


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=ROOT / 'assets/vrhands/animations')
    args = parser.parse_args()
    root = args.directory
    manifest_path = root / 'manifest.json'
    if not manifest_path.is_file():
        parser.error('No exported Actions; run tools/export_hand_actions.py in Blender first')
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    assert manifest['version'] == 1 and manifest['fps'] > 0
    reference = (ROOT / 'assets/vrhands/BigHandRight.uxrh').read_bytes()
    assert hashlib.sha256(reference).hexdigest() == manifest['referenceSha256']
    _, _, vc, ic = struct.unpack_from('<4sIII', reference)
    assert (vc, ic) == (manifest['vertices'], manifest['indices'])
    reference_uvs = [struct.unpack_from('<2f', reference, 16+i*104+96) for i in range(vc)]
    reference_indices = struct.unpack_from(f'<{ic}H', reference, 16+vc*104)
    expected_names = {'Grip', 'Pinch', 'Point', 'OpenPalm', 'Beckon',
                      'GripPistol', 'GripPistolOffhand', 'Fist', 'GripPistolShoot'}
    assert {c['name'] for c in manifest['clips']} == expected_names
    assert len(manifest['clips']) == len(expected_names)
    total_samples, total_blocks = 0, 0
    starts, endpoints = [], []
    for clip in manifest['clips']:
        samples = clip['samples']
        frames = [sample['frame'] for sample in samples]
        timing = (root / clip['name'] / 'timing.txt').read_text(encoding='ascii').split()
        assert timing[:2] == ['UXRA', '1'] and int(timing[2]) == len(samples)
        assert abs(float(timing[3])-manifest['fps']) < 1e-5
        assert [float(v) for v in timing[4:]] == frames
        assert frames == sorted(set(frames))
        assert frames[0] == clip['start'] and frames[-1] == clip['end']
        assert all(b-a <= 1 for a, b in zip(frames, frames[1:]))
        assert abs(clip['durationSeconds']-(frames[-1]-frames[0])/manifest['fps']) < 1e-6
        assert len(clip['blocks']) == (len(samples)+3)//4
        total_samples += len(samples)
        right_blocks = []
        for number, block in enumerate(clip['blocks']):
            right, ri = read_block(root, block['right'])
            left, li = read_block(root, block['left'])
            assert len(right) == len(left) == vc and len(ri) == len(li) == ic
            assert ri == reference_indices
            assert [v[24:26] for v in right] == reference_uvs
            assert 1 <= block['count'] <= 4
            assert block['count'] == min(4, len(samples)-number*4)
            for offset in range(0, ic, 3):
                assert li[offset:offset+3] == (ri[offset], ri[offset+2], ri[offset+1])
            for r, l in zip(right, left):
                assert l[24:26] == r[24:26]
                for offset in range(0, 24, 3):
                    assert l[offset:offset+3] == (r[offset], -r[offset+1], r[offset+2])
                for slot in range(block['count'], 4):
                    valid = block['count']-1
                    assert r[slot*3:slot*3+3] == r[valid*3:valid*3+3]
                    assert r[12+slot*3:15+slot*3] == r[12+valid*3:15+valid*3]
            right_blocks.append(right)
            total_blocks += 2
        poses = []
        for i, sample in enumerate(samples):
            assert (sample['block'], sample['pose']) == (i//4, i%4)
            offset = sample['pose']*3
            poses.append([v[offset:offset+3] for v in right_blocks[sample['block']]])
        assert max(math.dist(a, b) for a, b in zip(poses[0], poses[-1])) > .001, clip['name']
        # Authored dimensions are in metres, not source centimetres.
        assert max(abs(x) for pose in poses for v in pose for x in v) < .5
        if clip['name'] != 'GripPistolShoot':
            starts.append(poses[0])
        endpoints.append(poses[-1])
        print(f"PASS {clip['name']}: {len(samples)} samples, frames {frames[0]:g}..{frames[-1]:g}")
    # Ordinary transitions share an open start. The shooting Action may start
    # from the held pistol pose instead; it must retain its own authored start.
    assert max(math.dist(a, b) for pose in starts for a, b in zip(starts[0], pose)) < 1e-5
    # Distinct named gestures must not accidentally export the same current Action.
    for i, pose in enumerate(endpoints):
        for other in endpoints[i+1:]:
            assert max(math.dist(a, b) for a, b in zip(pose, other)) > .001
    print(f'PASS: {total_samples} source samples, {total_blocks} mirrored UXRH blocks')


if __name__ == '__main__':
    main()
