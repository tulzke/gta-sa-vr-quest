"""Create a hands-only source file from a working copy, never the original.

blender -b .cache/hand-skeletal/BothHands-export.blend --python-exit-code 1
 --python tools/prepare_hand_source.py
"""
import bpy
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from hand_glb import ACTIONS, action_key

ROOT=Path(__file__).resolve().parents[1]
assert Path(bpy.data.filepath).resolve()!=ROOT/'assets/vrhands/BothHands.blend'
hand=bpy.data.objects['HandRight'];rig=hand.find_armature()
for obj in list(bpy.data.objects):
    if obj not in (hand,rig):bpy.data.objects.remove(obj,do_unlink=True)
names={action_key(n) for n in ACTIONS}
for action in list(bpy.data.actions):
    if action_key(action.name) in names:action.use_fake_user=True
    else:bpy.data.actions.remove(action,do_unlink=True)
for text in list(bpy.data.texts):bpy.data.texts.remove(text)
bpy.ops.outliner.orphans_purge(do_recursive=True)
assert len(bpy.data.objects)==2 and len(bpy.data.meshes)==1 and len(bpy.data.armatures)==1
assert {action_key(a.name) for a in bpy.data.actions}==names
out=ROOT/'assets/vrhands/source/Hands.blend'
out.parent.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(out),compress=True)
print('SAVED HANDS-ONLY SOURCE: 2 objects, 1 mesh, 1 rig, 14 Actions',out)
