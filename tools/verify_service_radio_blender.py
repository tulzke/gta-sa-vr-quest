"""Read-only source, exchange GLB and HandleRadio verification in Blender 4.5."""
import hashlib
import json
import math
from pathlib import Path

import bmesh
import bpy

ROOT = Path(__file__).resolve().parents[1]
REVIEW = ROOT / 'build/service-radio'
CACHE = ROOT / '.cache/service-radio'
baseline = json.loads((CACHE/'hands-baseline.json').read_text(encoding='utf8'))


def normalized(value):
    return json.loads(json.dumps(value))


def signature(a):
    return [(c.data_path, c.array_index, [(list(k.co), list(k.handle_left), list(k.handle_right),
            k.interpolation) for k in c.keyframe_points]) for c in a.fcurves]


working = CACHE/'Hands-radio-working.blend'
bpy.ops.wm.open_mainfile(filepath=str(working))
hand = bpy.data.objects['HandRight']
rig = hand.find_armature()
assert len(bpy.data.objects) == 2
assert len(bpy.data.actions) == 15 and 'HandleRadio' in bpy.data.actions
assert normalized({a.name: signature(a) for a in bpy.data.actions if a.name != 'HandleRadio'}) == baseline['actions']
assert normalized([(tuple(v.co), [(g.group, g.weight) for g in v.groups]) for v in hand.data.vertices]) == baseline['mesh']
assert normalized([(b.name, [list(r) for r in b.matrix_local]) for b in rig.data.bones]) == baseline['bones']
action = bpy.data.actions['HandleRadio']
assert action.use_fake_user and action.asset_data
rig.animation_data.action = action
rig.animation_data.action_slot = action.slots[0]
snapshots = []
for frame in (1, 6, 11):
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()
    snapshots.append({p.name: p.head.copy() for p in rig.pose.bones})
    assert all(math.isfinite(v) for p in rig.pose.bones for row in p.matrix for v in row)
    thumb = rig.pose.bones['Thumb_Tip_Right'].head-rig.pose.bones['Thumb_1_Right'].head
    assert thumb.normalized().z > .95
    assert rig.pose.bones['Thumb_Tip_Right'].head.x < 8.1
    assert rig.pose.bones['Index_Tip_Right'].head.x > 14.5
travel_mm = (snapshots[-1]['Index_Tip_Right']-snapshots[0]['Index_Tip_Right']).length * 10
assert .2 < travel_mm < 2, travel_mm
for name in snapshots[0]:
    if not name.startswith('Index'):
        assert (snapshots[0][name]-snapshots[-1][name]).length < 1e-4, name
print(f'PASS hands: 15 Actions; previous 14 curves, mesh/weights and rest rig identical; index travel {travel_mm:.3f} mm')

model = ROOT/'assets/service-radio/service-radio-v1.blend'
bpy.ops.wm.open_mainfile(filepath=str(model))
scene = bpy.context.scene
asset = bpy.data.collections['ASSET | handheld service radio']
meshes = [o for o in asset.objects if o.type == 'MESH']
triangles = 0
for o in meshes:
    assert not o.modifiers, o.name
    assert all(math.isfinite(v) for p in o.data.vertices for v in p.co)
    bm = bmesh.new()
    bm.from_mesh(o.data)
    assert all(e.is_manifold for e in bm.edges), o.name
    assert all(f.calc_area() > 1e-12 for f in bm.faces), o.name
    bm.free()
    o.data.calc_loop_triangles()
    triangles += len(o.data.loop_triangles)
assert triangles < 18000
accent_parts = [o.name for o in meshes if any(m.name == 'RadioServiceAccent' for m in o.data.materials)]
assert accent_parts == ['ServiceAccentCollar'], accent_parts
for name in ('HandWristAnchor_Right', 'HandWristAnchor_Left', 'IndexPressAnchor', 'DockAnchor'):
    assert name in bpy.data.objects
scene.frame_set(1)
x1 = bpy.data.objects['RadioPTT'].location.x
scene.frame_set(11)
x2 = bpy.data.objects['RadioPTT'].location.x
assert abs((x2-x1)-.0015) < 1e-7
print(f'PASS model: {len(meshes)} closed solid meshes, {triangles} triangles; isolated accent and 1.5 mm PTT')
expected_bounds = {}
for o in meshes:
    points = [o.matrix_world @ p.co for p in o.data.vertices]
    expected_bounds[o.name] = [(min(p[i] for p in points), max(p[i] for p in points)) for i in range(3)]
scene.frame_set(1)
expected_materials = {o.name: o.data.materials[0].name for o in meshes}

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(ROOT/'assets/service-radio/service-radio-v1.glb'))
scene = bpy.context.scene
scene.frame_set(11)
max_error = 0.
for name, bounds in expected_bounds.items():
    o = bpy.data.objects.get(name)
    assert o is not None and o.type == 'MESH', name
    assert o.data.materials[0].name == expected_materials[name]
    points = [o.matrix_world @ p.co for p in o.data.vertices]
    actual = [(min(p[i] for p in points), max(p[i] for p in points)) for i in range(3)]
    max_error = max(max_error, max(abs(a-b) for aa, bb in zip(actual, bounds) for a, b in zip(aa, bb)))
assert max_error < .00001, max_error
print(f'PASS GLB roundtrip: all {len(expected_bounds)} parts/materials retained; bounds error {max_error*1000:.6f} mm')
report = {'status': 'passed', 'model_closed_meshes': len(expected_bounds), 'triangles': triangles,
          'accent_parts': accent_parts, 'ptt_travel_mm': 1.5, 'hand_action': 'HandleRadio',
          'existing_14_actions_mesh_weights_rig': 'identical', 'index_travel_mm': travel_mm,
          'glb_bounds_error_mm': max_error*1000, 'source_before_sha256': baseline['source_sha256'],
          'working_sha256': hashlib.sha256(working.read_bytes()).hexdigest(),
          'note': 'offline asset review only; no runtime GLB export, game build, install or commit'}
(REVIEW/'verification.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
