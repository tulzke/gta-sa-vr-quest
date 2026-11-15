"""Check the task 56 source and round-trip the exported rigid animated GLB.

Run with Blender --background --factory-startup --python-exit-code 1 --python.
"""
import json
import math
from pathlib import Path
import struct

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'assets/vehicle-lift-lever'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'lift-lever.blend'))
for name in ['GuideCheek_L','GuideCheek_R']:
    cheek = bpy.data.objects[name]
    for angle in [-24,-12,0,12,24]:
        t = math.radians(angle)
        point = Vector((-.1,.056*math.sin(t),.05+.056*math.cos(t)))
        assert not cheek.ray_cast(point,Vector((1,0,0)))[0],(name,angle,'slot obstructed')
    for angle in [-35,35]:
        t = math.radians(angle)
        point = Vector((-.1,.056*math.sin(t),.05+.056*math.cos(t)))
        assert cheek.ray_cast(point,Vector((1,0,0)))[0],(name,angle,'missing end wall')
print('PASS source: through-slots clear; solid walls beyond travel',flush=True)

data = (OUT/'lift-lever.glb').read_bytes()
magic,version,length = struct.unpack_from('<III',data)
assert magic == 0x46546C67 and version == 2 and length == len(data)
json_length,json_kind = struct.unpack_from('<II',data,12)
assert json_kind == 0x4E4F534A
doc = json.loads(data[20:20+json_length])
assert not doc.get('images') and not doc.get('textures')
assert not doc.get('cameras') and not doc.get('extensions',{}).get('KHR_lights_punctual')
assert len(doc['animations']) == 1 and doc['animations'][0]['name']=='LiftTravel'
triangles = sum(doc['accessors'][p['indices']]['count']//3
                for m in doc['meshes'] for p in m['primitives'])
assert triangles == json.loads((OUT/'manifest.json').read_text())['triangles']

# Import into a fresh scene, independent from source drivers/constraints.
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(OUT/'lift-lever.glb'))
scene = bpy.context.scene
pivot = bpy.data.objects['LiftPivot']
anchor = bpy.data.objects['GripAnchor']
assert anchor.parent == pivot
assert bpy.data.objects['LeverShaft'].parent == pivot
assert bpy.data.objects['HorizontalGrip'].parent == pivot
assert bpy.data.objects['GuideCheek_L'].parent.name == 'LiftFixed'
assert len([o for o in scene.objects if o.type=='MESH']) == len(doc['meshes'])
assert not [o for o in scene.objects if o.type in ('CAMERA','LIGHT')]
assert all((o.scale-Vector((1,1,1))).length<1e-6 for o in scene.objects)
action = pivot.animation_data.action
f0,f1 = action.frame_range
for progress,angle in [(0,-25),(.5,0),(1,25)]:
    frame = f0+(f1-f0)*progress
    scene.frame_set(math.floor(frame),subframe=frame-math.floor(frame))
    bpy.context.view_layer.update()
    actual = anchor.matrix_world.translation
    expected = Vector((0,-.18*math.sin(math.radians(angle)),.05+.18*math.cos(math.radians(angle))))
    assert (actual-expected).length<1e-5,(angle,actual,expected)
    r = pivot.rotation_euler
    assert abs(r.y)<1e-5 and abs(r.z)<1e-5,(angle,r)
    print(f'PASS GLB angle {angle:+d}: grip anchor error {(actual-expected).length*1000:.6f} mm',flush=True)
print(f'PASS GLB: {triangles} triangles; clean hierarchy; no studio/textures; unit scale',flush=True)
