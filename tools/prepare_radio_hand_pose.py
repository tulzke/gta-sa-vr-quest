"""Author HandleRadio in a working copy, then render fit against task 97 radio.

Never writes the hands source. Review/verification precedes publishing the
working copy to assets/vrhands/source/Hands.blend. Existing Actions are retained.
"""
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / '.cache/service-radio'
REVIEW = ROOT / 'build/service-radio'
CACHE.mkdir(parents=True, exist_ok=True)
REVIEW.mkdir(parents=True, exist_ok=True)
source = ROOT / 'assets/vrhands/source/Hands.blend'
assert Path(bpy.data.filepath).resolve() == source.resolve()
scene = bpy.context.scene
hand = bpy.data.objects['HandRight']
rig = hand.find_armature()


def action_signature(a):
    return [(c.data_path, c.array_index, [(list(k.co), list(k.handle_left), list(k.handle_right),
             k.interpolation) for k in c.keyframe_points]) for c in a.fcurves]


before = {a.name: action_signature(a) for a in bpy.data.actions if a.name != 'HandleRadio'}
mesh_before = [(tuple(v.co), [(g.group, g.weight) for g in v.groups]) for v in hand.data.vertices]
bone_before = [(b.name, [list(row) for row in b.matrix_local]) for b in rig.data.bones]
rig.animation_data_create()
rig.animation_data.use_nla = False


def sample(name, frame):
    a = bpy.data.actions[name]
    rig.animation_data.action = a
    if a.slots:
        rig.animation_data.action_slot = a.slots[0]
    scene.frame_set(frame)
    bpy.context.view_layer.update()
    return {p.name: p.matrix_basis.copy() for p in rig.pose.bones}


base = sample('Grip', 21)
open_palm = sample('OpenPalm', 21)
rig.animation_data.action = None
for p in rig.pose.bones:
    p.matrix_basis = base[p.name]
for p in rig.pose.bones:
    if p.name.startswith('Thumb'):
        p.matrix_basis = open_palm[p.name]
bpy.context.view_layer.update()


def aim_child(parent, child, target):
    """Rotate the existing FK joint, retaining all rig lengths and hierarchy."""
    p, c = rig.pose.bones[parent], rig.pose.bones[child]
    current, wanted = c.head-p.head, Vector(target)-p.head
    q = current.rotation_difference(wanted)
    p.matrix = Matrix.Translation(p.head) @ q.to_matrix().to_4x4() @ Matrix.Translation(-p.head) @ p.matrix
    bpy.context.view_layer.update()


# Targets use the existing canonical hand centimetres, not its rest-scene offset.
# Index travels around the PTT side; the other three fingers curl around the body.
targets = {
    'Index': [(14.6, -.8, 3.0), (15.8, 1.2, 3.2), (15.1, 3.0, 3.5)],
    'Middle': [(15.1, .3, .4), (15.6, 3.4, .4), (13.6, 5.0, .4)],
    'Ring': [(14.9, .3, -1.9), (15.3, 3.3, -1.9), (13.3, 4.9, -1.9)],
    'Little': [(13.6, .2, -4.0), (14.9, 2.3, -4.0), (13.4, 4.5, -4.0)],
}
for finger, points in targets.items():
    for i, target in enumerate(points):
        child = f'{finger}_{i+1}_Right' if i < 2 else f'{finger}_Tip_Right'
        aim_child(f'{finger}_{i}_Right', child, target)

# Thumb up along the side opposite the PTT: open web, extended distal phalanges.
for parent, child, target in [
    ('Thumb_Palm_Right', 'Thumb_0_Right', (6.5, 2.1, 4.1)),
    ('Thumb_0_Right', 'Thumb_1_Right', (7.2, 2.7, 7.2)),
    ('Thumb_1_Right', 'Thumb_Tip_Right', (7.4, 3.0, 10.0)),
]:
    aim_child(parent, child, target)

old = bpy.data.actions.get('HandleRadio')
if old:
    bpy.data.actions.remove(old)
held = {p.name: p.matrix_basis.copy() for p in rig.pose.bones}
index = rig.pose.bones['Index_Tip_Right'].head.copy()
aim_child('Index_2_Right', 'Index_Tip_Right', index + Vector((-.13, 0, 0)))
pressed = {p.name: p.matrix_basis.copy() for p in rig.pose.bones}
action = bpy.data.actions.new('HandleRadio')
action.use_fake_user = True
action.asset_mark()
action.asset_data.description = 'Service radio: index on PTT, thumb up opposite side, other fingers wrapped'
action['task'] = 97
action['meaning'] = 'index on side PTT; thumb up opposite edge; three fingers wrap shell'
action['frames'] = '1: held / PTT released; 11: held / PTT pressed; only index changes'
rig.animation_data.action = action
for frame, pose in [(1, held), (11, pressed)]:
    scene.frame_set(frame)
    for p in rig.pose.bones:
        p.rotation_mode = 'QUATERNION'
        p.matrix_basis = pose[p.name]
        for channel in ('location', 'rotation_quaternion', 'scale'):
            p.keyframe_insert(data_path=channel, frame=frame, group=p.name)
for c in action.fcurves:
    for k in c.keyframe_points:
        k.interpolation = 'LINEAR'
scene.frame_start, scene.frame_end = 1, 11
scene.frame_set(1)
bpy.context.view_layer.update()

assert before == {a.name: action_signature(a) for a in bpy.data.actions if a.name != 'HandleRadio'}
assert mesh_before == [(tuple(v.co), [(g.group, g.weight) for g in v.groups]) for v in hand.data.vertices]
assert bone_before == [(b.name, [list(row) for row in b.matrix_local]) for b in rig.data.bones]
assert len(bpy.data.objects) == 2
(CACHE/'hands-baseline.json').write_text(json.dumps({'actions': before, 'bones': bone_before,
    'mesh': mesh_before, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest()}), encoding='utf8')
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(CACHE/'Hands-radio-working.blend'), compress=True)

# Build a separate review scene; no radio geometry is added to the hands source.
if bpy.context.object and bpy.context.object.mode != 'OBJECT':
    bpy.ops.object.mode_set(mode='OBJECT')
with bpy.data.libraries.load(str(ROOT/'assets/service-radio/service-radio-v1.blend'), link=False) as (src, dst):
    dst.collections = ['ASSET | handheld service radio']
radio_collection = dst.collections[0]
scene.collection.children.link(radio_collection)
root = bpy.data.objects['ServiceRadioRoot']
hand_space = rig.matrix_world @ Matrix.Scale(100, 4)
radio_fit = Matrix.Translation((.112, .025, -.060)) @ Matrix.Rotation(math.pi, 4, 'Z')
root.matrix_world = hand_space @ radio_fit
cradle = bpy.data.objects['RadioCradle']
for o in [cradle, *cradle.children_recursive]:
    o.hide_render = True
    o.hide_set(True)
scene.frame_set(1)
bpy.context.view_layer.update()
report = {'source': str(source.relative_to(ROOT)), 'source_before_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
          'action': action.name, 'frames': [1, 11], 'existing_actions_unchanged': list(before),
          'mesh_and_rig_unchanged': True, 'radio_from_canonical_wrist_m': [list(r) for r in radio_fit],
          'pose_bones_cm': {p.name: {'head': list(p.head), 'tail': list(p.tail)} for p in rig.pose.bones}}
(REVIEW/'hand-fit.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')

studio = bpy.data.collections.new('REVIEW ONLY | cameras lights')
scene.collection.children.link(studio)


def studio_object(o):
    for c in list(o.users_collection):
        c.objects.unlink(o)
    studio.objects.link(o)
    return o


def aim(o, target):
    o.rotation_euler = (Vector(target)-o.location).to_track_quat('-Z', 'Y').to_euler()


target = hand_space @ Vector((.10, .02, .035))
bpy.ops.object.camera_add()
cam = studio_object(bpy.context.object)
cam.data.type, cam.data.ortho_scale = 'ORTHO', .265
scene.camera = cam
for name, loc, power, size in [('Key', (-.08, .3, .35), 7, .3),
                               ('Fill', (.38, .1, .20), 4, .28),
                               ('Rim', (.03, -.24, .25), 5, .24)]:
    bpy.ops.object.light_add(type='AREA', location=loc)
    o = studio_object(bpy.context.object)
    o.name, o.data.energy, o.data.size = 'REVIEW | '+name, power, size
    aim(o, target)
scene.world.color = (.25, .25, .25)
scene.render.engine = 'CYCLES'
scene.cycles.samples, scene.cycles.use_denoising = 32, True
scene.render.resolution_x, scene.render.resolution_y = 1100, 1100
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.film_transparent = False
scene.view_settings.view_transform = 'AgX'
for label, position in [('index', (.44, .38, .23)), ('thumb', (-.23, .32, .22)), ('palm', (-.08, -.38, .23))]:
    cam.location = position
    aim(cam, target)
    scene.render.filepath = str(REVIEW/f'hand-{label}.png')
    if '--no-render' not in sys.argv:
        bpy.ops.render.render(write_still=True)
scene.frame_set(11)
cam.location = (.44, .38, .23)
aim(cam, target)
scene.render.filepath = str(REVIEW/'hand-index-pressed.png')
if '--no-render' not in sys.argv:
    bpy.ops.render.render(write_still=True)
scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(REVIEW/'radio-hand-review.blend'), compress=True)

# Same source pose reflected for the left hand. Rotate the real radio, rather
# than mirroring its geometry/antenna/PTT side, so the index still reaches PTT.
reflection = Matrix.Diagonal((-1, 1, 1, 1))
rig.matrix_world = reflection @ rig.matrix_world
hand.matrix_world = reflection @ hand.matrix_world
root.matrix_world = hand_space @ Matrix.Translation((-.112, .025, -.060))
target = hand_space @ Vector((-.10, .02, .035))
cam.location = (-.44, -.36, .23)
aim(cam, target)
scene.render.filepath = str(REVIEW/'hand-left.png')
if '--no-render' not in sys.argv:
    bpy.ops.render.render(write_still=True)
print('PASS: HandleRadio created; 14 existing Actions, mesh and rig unchanged; working copy only', flush=True)
