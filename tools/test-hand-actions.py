"""Validate the committed skeletal hand asset without Blender or UXRH."""
from pathlib import Path
import hashlib
import json
import math
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from hand_glb import ACTIONS, HandGlb
from radio_png import rgba_png

ROOT=Path(__file__).resolve().parents[1]
root=ROOT/'assets/vrhands'
glb=HandGlb(root/'skeletal/hands.glb');doc=glb.doc
assert len(doc['meshes'])==len(doc['skins'])==1
assert len(doc['animations'])==len(ACTIONS) and {c['name'] for c in doc['animations']}==set(ACTIONS)
assert len(doc['skins'][0]['joints'])==25
primitive=doc['meshes'][0]['primitives'][0];attr=primitive['attributes']
positions=glb.accessor(attr['POSITION']);uv=glb.accessor(attr['TEXCOORD_0'])
assert len(positions)==len(uv)==2437
indices=glb.accessor(primitive['indices'])
assert len(indices)==13092 and max(i[0] for i in indices)<len(positions)
for key,index in attr.items():
    rows=glb.accessor(index)
    assert len(rows)==len(positions) and all(math.isfinite(v) for row in rows for v in row),key
for clip in doc['animations']:
    for sampler in clip['samplers']:
        assert sampler['interpolation']=='LINEAR'
        times=[v[0] for v in glb.accessor(sampler['input'])]
        assert times[0]==0 and times[-1]>0 and times==sorted(set(times))
    print('PASS GLB Action:',clip['name'])
view=doc['bufferViews'][doc['images'][0]['bufferView']]
png=glb.binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
assert png==(root/'BigHandsAlbedo.png').read_bytes()
assert rgba_png(png)[:2]==(1024,1024)
manifest=json.loads((root/'skeletal/manifest.json').read_text())
assert manifest['sourceSha256']==hashlib.sha256((root/'source/Hands.blend').read_bytes()).hexdigest()
assert not list(root.glob('*.uxrh')) and not (root/'animations').exists()
print('PASS: 14 GLB clips, one skinned mesh, embedded albedo; no legacy hand models')
