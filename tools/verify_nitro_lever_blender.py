"""Verify saved source, debug material and independent GLB motion for task 32."""
import json
import math
import struct
from pathlib import Path

import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'assets/nitro-lever'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'nitro-lever-v1.blend'))
scene=bpy.context.scene
root=bpy.data.objects['NitroRoot']
pivot=bpy.data.objects['NitroPivot']
assert scene.frame_current==1 and root['preview_uninstalled']==0
assert bpy.data.images['Nitro monochrome atlas | shared'].packed_file
assert len([o for o in bpy.data.objects if o.get('runtime_role')=='reserve_segment'])==10
assert all(not o.get('partial_fill',False) for o in bpy.data.objects if o.get('runtime_role')=='reserve_segment')
for frame,lit in [(1,False),(25,True),(49,False)]:
    scene.frame_set(frame);bpy.context.view_layer.update()
    mat=bpy.data.materials['Flow lamp | driven review state']
    strength=mat.node_tree.nodes['Principled BSDF'].inputs['Emission Strength'].default_value
    assert (strength>.1)==lit,(frame,strength)
root['preview_uninstalled']=1.;root.update_tag();bpy.context.view_layer.update()
paint=bpy.data.materials['Housing | graphite'].node_tree.nodes['Principled BSDF']
assert abs(paint.inputs['Alpha'].default_value-.45)<1e-6
for actual,base,yellow in zip(paint.inputs['Base Color'].default_value[:3],[.075,.090,.105],[1.,.85,.12]):
    assert abs(actual-(base*.8+yellow*.2))<1e-6,(actual,base)
root['preview_uninstalled']=0.;root.update_tag();bpy.context.view_layer.update()
assert abs(paint.inputs['Alpha'].default_value-1)<1e-6
print('PASS saved source: packed atlas, ten whole segments, lamp OFF/ON/OFF, reversible 20% yellow and 45% opacity')

raw=(OUT/'nitro-lever-v1.glb').read_bytes()
magic,version,length=struct.unpack_from('<III',raw)
assert magic==0x46546c67 and version==2 and length==len(raw)
size,kind=struct.unpack_from('<II',raw,12)
doc=json.loads(raw[20:20+size])
assert len(doc['images'])==1 and 'bufferView' in doc['images'][0]
assert not doc.get('cameras') and not doc.get('extensions',{}).get('KHR_lights_punctual')
assert len(doc['animations'])==1 and doc['animations'][0]['name']=='NitroTravel'
triangles=sum(doc['accessors'][p['indices']]['count']//3 for m in doc['meshes'] for p in m['primitives'])
assert triangles==json.loads((OUT/'manifest.json').read_text())['triangles']
assert all('uri' not in image for image in doc['images'])
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(OUT/'nitro-lever-v1.glb'))
pivot=bpy.data.objects['NitroPivot'];anchor=bpy.data.objects['GripAnchor']
assert anchor.parent==pivot
for name in ['LeverShaft','HorizontalGrip','GripBlueCap_L','GripBlueCap_R','GuideFollowerPin','GripLabel_N2O']:
    assert bpy.data.objects[name].parent==pivot,name
assert not [o for o in bpy.context.scene.objects if o.type in ('CAMERA','LIGHT')]
assert all((o.scale-Vector((1,1,1))).length<1e-6 for o in bpy.context.scene.objects)
action=pivot.animation_data.action
first,last=action.frame_range
for fraction,angle in [(0,22),(.25,0),(.5,-22),(1,22)]:
    frame=first+(last-first)*fraction
    bpy.context.scene.frame_set(math.floor(frame),subframe=frame-math.floor(frame))
    bpy.context.view_layer.update()
    a=math.radians(angle)
    # Blender's importer converts standard glTF axes back to Blender Z-up.
    expected=Vector((0,.018-.088*math.sin(a),.048+.088*math.cos(a)))
    actual=anchor.matrix_world.translation
    assert (actual-expected).length<1e-5,(angle,actual,expected)
print(f'PASS standalone GLB: {triangles} triangles, one packed atlas, rigid hierarchy, unit scale, correct OFF/ON motion and no studio objects')
