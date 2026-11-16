"""Render current saved radio or hand fit, without regenerating source assets.

blender -b --factory-startup --threads 8 --python-exit-code 1
  --python tools/render_service_radio_review.py -- model|hand
"""
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/service-radio'
mode = sys.argv[sys.argv.index('--')+1]
assert mode in ('model', 'hand')
path = ROOT/'assets/service-radio/service-radio-v1.blend' if mode == 'model' else OUT/'radio-hand-review.blend'
bpy.ops.wm.open_mainfile(filepath=str(path))
scene = bpy.context.scene
scene.frame_set(1)
scene.cycles.samples = 32
if mode == 'model':
    mat = bpy.data.materials['RadioServiceAccent']
    for service, color in [('police', (.018, .12, .64)), ('medic', (.78, .80, .79)), ('fire', (.65, .022, .018))]:
        mat.diffuse_color = (*color, 1)
        mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (*color, 1)
        scene.render.filepath = str(OUT/f'radio-{service}.png')
        bpy.ops.render.render(write_still=True)
else:
    rig = bpy.data.objects['Hand']
    hand = bpy.data.objects['HandRight']
    radio = bpy.data.objects['ServiceRadioRoot']
    space = rig.matrix_world @ Matrix.Scale(100, 4)
    target = space @ Vector((.10, .02, .035))
    cam = scene.camera

    def view(label, location, frame=1):
        scene.frame_set(frame)
        cam.location = location
        cam.rotation_euler = (target-cam.location).to_track_quat('-Z', 'Y').to_euler()
        scene.render.filepath = str(OUT/f'hand-{label}.png')
        bpy.ops.render.render(write_still=True)

    view('index', (.44, .38, .23))
    view('thumb', (-.23, .32, .22))
    view('palm', (-.08, -.38, .23))
    view('index-pressed', (.44, .38, .23), 11)
    reflection = Matrix.Diagonal((-1, 1, 1, 1))
    rig.matrix_world = reflection @ rig.matrix_world
    hand.matrix_world = reflection @ hand.matrix_world
    radio.matrix_world = space @ Matrix.Translation((-.112, .025, -.060))
    target = space @ Vector((-.10, .02, .035))
    view('left', (-.44, -.36, .23))
print('Rendered current saved '+mode+' review', flush=True)
