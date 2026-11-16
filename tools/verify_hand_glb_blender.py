"""Re-import skeletal export in a clean Blender scene and compare all endpoints.

Run after export_hand_glb.py: blender --factory-startup -b --python-exit-code 1
 --python tools/verify_hand_glb_blender.py
"""
import bpy
import json
from pathlib import Path
import struct
from mathutils import Vector
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from hand_glb import HandGlb

ROOT = Path(__file__).resolve().parents[1]
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(ROOT / 'assets/vrhands/skeletal/hands.glb'))
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH' and o.find_armature()]
assert len(meshes) == 1
obj = meshes[0]
rig = obj.find_armature()
assert rig and len(rig.data.bones) == 25
assert len(obj.data.materials) == 1 and obj.data.materials[0].use_nodes
print('IMPORTED ACTIONS', [(a.name, list(a.frame_range)) for a in bpy.data.actions])
report = json.loads((ROOT / 'assets/vrhands/skeletal/manifest.json').read_text())
glb=HandGlb(ROOT/'assets/vrhands/skeletal/hands.glb')
attr=glb.doc['meshes'][0]['primitives'][0]['attributes']
lookup={tuple(round(v,5) for v in uv):i for i,uv in enumerate(glb.accessor(attr['TEXCOORD_0']))}
ref = memoryview((ROOT / '.cache/hand-skeletal/reference.bin').read_bytes())
_, vc, _ = struct.unpack_from('<III', ref)
offset = 12
maximum = 0
for clip in report['clips']:
    count, = struct.unpack_from('<I', ref, offset)
    offset += 4
    snapshots = []
    for i in range(count):
        progress, = struct.unpack_from('<f', ref, offset)
        offset += 4
        if i in (0, count//2, count-1):
            expected = [struct.unpack_from('<3f', ref, offset+v*24) for v in range(vc)]
            snapshots.append((progress, expected))
        offset += vc*24
    actions = [a for a in bpy.data.actions if a.name == clip['name'] or a.name.startswith(clip['name']+'_')]
    assert len(actions) == 1, (clip['name'], [a.name for a in actions])
    action = actions[0]
    rig.animation_data_create()
    rig.animation_data.use_nla = False
    rig.animation_data.action = action
    rig.animation_data.action_slot = next(s for s in action.slots if s.target_id_type == 'OBJECT')
    for progress, expected in snapshots:
        frame = (action.frame_range[1]-action.frame_range[0])*progress+action.frame_range[0]
        bpy.context.scene.frame_set(int(frame), subframe=frame-int(frame))
        ev = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
        mesh = ev.to_mesh()
        try:
            # Importer may weld/reorder vertices. Match by the preserved unique
            # reference UVs rather than assuming Blender vertex order.
            error = 0
            for loop in mesh.loops:
                uv = mesh.uv_layers.active.data[loop.index].uv
                index = lookup[tuple(round(v,5) for v in (uv.x,1-uv.y))]
                # Blender importer converts glTF Y-up back to Z-up, so world
                # coordinates are already canonical mesh-local metres.
                actual = ev.matrix_world @ mesh.vertices[loop.vertex_index].co
                error = max(error, (actual-Vector(expected[index])).length)
            maximum = max(maximum,error)
            assert error < .00008, (clip['name'], progress, error)
        finally:
            ev.to_mesh_clear()
    print('PASS Blender roundtrip', clip['name'])
print(f'PASS one mesh, 25 bones, 15 clips, albedo material; roundtrip max {maximum*1000:.6f} mm')
