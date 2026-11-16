"""Independently inspect source rig, arbitrary-angle bellows and exported GLB."""
import json
import math
import struct
from pathlib import Path

import bmesh
import bpy
from mathutils import Quaternion, Vector
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'assets/hydraulics'
report={'source_samples':0,'glb_samples':0,'self_intersections':[]}
manifest=json.loads((OUT/'manifest.json').read_text())
bpy.ops.wm.open_mainfile(filepath=str(OUT/'hydraulics-joystick-v1.blend'))
scene=bpy.context.scene
root=bpy.data.objects['HydraulicsRoot']
boot=bpy.data.objects['Bellows | continuously deformed']
pivot=bpy.data.objects['HydraulicsPivot']
assert scene.frame_current==1
assert bpy.data.images['Hydraulics | shared label atlas'].packed_file
assert boot.parent==bpy.data.objects['BellowsRig']
assert len(bpy.data.objects['BellowsRig'].data.bones)==21
assert not boot.data.shape_keys, 'Must be continuous skinning, not nine replacement shapes'
for obj in bpy.context.scene.objects:
    if obj.type!='MESH' or not obj.get('closed_solid'):continue
    bm=bmesh.new();bm.from_mesh(obj.data)
    assert all(e.is_manifold for e in bm.edges),obj.name
    bm.free()
root.animation_data_clear()
PX,PY,PZ=manifest['pivot_m']
BOTTOM,TOP=manifest['bellows']['base_z_m'],manifest['bellows']['neck_z_m']
SIDES=manifest['bellows']['sides']
RINGS=manifest['bellows']['rings']
base_vertices=[v.co.copy() for v in boot.data.vertices]


def evaluate(x,y,check_intersections=True):
    root['tilt_right_deg'],root['tilt_forward_deg']=x,y
    root.update_tag();bpy.context.view_layer.update()
    dg=bpy.context.evaluated_depsgraph_get()
    mesh=boot.evaluated_get(dg).to_mesh()
    try:
        coords=[v.co.copy() for v in mesh.vertices]
        assert all(math.isfinite(a) for p in coords for a in p)
        for layer in (0,1):
            for i in range(SIDES):
                k=layer*RINGS*SIDES+i
                assert (coords[k]-base_vertices[k]).length<1e-6,('detached base',x,y,k)
                k=layer*RINGS*SIDES+(RINGS-1)*SIDES+i
                local=base_vertices[k]-Vector((PX,PY,PZ))
                expected=pivot.rotation_quaternion@local+Vector((PX,PY,PZ))
                assert (coords[k]-expected).length<1e-6,('detached neck',x,y,k)
        assert pivot.rotation_quaternion.angle<=math.radians(25)+1e-5
        if check_intersections:
            faces=[tuple(p.vertices) for p in mesh.polygons]
            bvh=BVHTree.FromPolygons(coords,faces,epsilon=0.)
            overlaps=[]
            face_sets=[set(f) for f in faces]
            for a,b in bvh.overlap(bvh):
                if a>=b or face_sets[a]&face_sets[b]:continue
                overlaps.append((a,b))
            assert not overlaps,('bellows self intersection',x,y,overlaps[:6])
        report['source_samples']+=1
        return coords
    finally:
        boot.evaluated_get(dg).to_mesh_clear()


for radius in (0,5,11,22,25):
    for degree in range(0,360,22):
        a=math.radians(degree)
        evaluate(radius*math.sin(a),radius*math.cos(a))
# Off-axis slack, clamped manual property extremes, and smoothly varying samples.
evaluate(25,25)
first=evaluate(3.5,5.)
second=evaluate(3.51,5.01)
distance=max((a-b).length for a,b in zip(first,second))
assert 0<distance<.0001,('continuous tiny-angle deformation',distance)
root['mission_four_way']=1.;root.update_tag();bpy.context.view_layer.update()
for i in range(8):
    scale=bpy.data.objects[f'DetentIndex_{i}'].scale.x
    assert abs(scale-(0 if i%2 else 1))<1e-6
root['button_press']=1.;root.update_tag();bpy.context.view_layer.update()
assert abs(bpy.data.objects['HydraulicsModeButton'].location.z-.009)<1e-6
report['tiny_tilt_max_displacement_m']=distance

# The standalone GLB must retain the skinned mesh and real animation, without
# requiring Blender's procedural drivers at playback time.
raw=(OUT/'hydraulics-joystick-v1.glb').read_bytes()
magic,version,length=struct.unpack_from('<III',raw)
assert magic==0x46546c67 and version==2 and length==len(raw)
size,kind=struct.unpack_from('<II',raw,12)
doc=json.loads(raw[20:20+size])
assert doc.get('skins') and len(doc['skins'][0]['joints'])==21
assert doc.get('animations') and len(doc['images'])==1
assert not doc.get('cameras') and not doc.get('extensions',{}).get('KHR_lights_punctual')
assert all('uri' not in im for im in doc['images'])
assert all('targets' not in primitive for m in doc['meshes'] for primitive in m['primitives'])
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.render.fps=30
bpy.ops.import_scene.gltf(filepath=str(OUT/'hydraulics-joystick-v1.glb'))
scene=bpy.context.scene
boot=bpy.data.objects['Bellows | continuously deformed']
arm=[m.object for m in boot.modifiers if m.type=='ARMATURE'][0]
assert len(arm.data.bones)==21
animated=[o for o in scene.objects if o.animation_data and o.animation_data.action]
assert animated,'Missing baked animation'
first_frame=min(o.animation_data.action.frame_range[0] for o in animated)
scene.render.fps=30
for name,original in manifest['poses'].items():
    frame=first_frame+original-1
    scene.frame_set(int(frame),subframe=frame-int(frame));bpy.context.view_layer.update()
    dg=bpy.context.evaluated_depsgraph_get()
    eval_boot=boot.evaluated_get(dg)
    mesh=eval_boot.to_mesh()
    pts=[eval_boot.matrix_world@v.co for v in mesh.vertices]
    eval_boot.to_mesh_clear()
    # GLB may split vertices at UV/normal seams; compare extrema and actual motion.
    assert all(math.isfinite(v) for p in pts for v in p)
    if name=='NEUTRAL':
        neutral=[p.copy() for p in pts]
    else:
        assert max((a-b).length for a,b in zip(neutral,pts))>.002,(name,'static GLB bellows')
    report['glb_samples']+=1
report['triangles']=manifest['triangles']
report['result']='PASS: closed solids; arbitrary-angle samples; fixed base; exact moving collar; no bellows self-intersections; continuous tiny motion; clamp; mission indices; button travel; standalone GLB skin animation'
(ROOT/'build/hydraulics-review/validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(report,indent=2))
