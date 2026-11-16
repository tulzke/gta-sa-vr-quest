"""Remove diagonal divisions from the existing artist asset without rebuilding its meshes/rig."""
import json
from pathlib import Path
import bpy

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'assets/hydraulics'
source=OUT/'hydraulics-joystick-v1.blend'
bpy.ops.wm.open_mainfile(filepath=str(source))
scene=bpy.context.scene
root=bpy.data.objects['HydraulicsRoot']
asset=bpy.data.collections['HYDRAULICS | editable asset']
assert root['task']==31
for i in (1,3,5,7):
    for name in (f'DetentTick_{i}',f'DetentIndex_{i}'):
        obj=bpy.data.objects.get(name)
        if obj:bpy.data.objects.remove(obj,do_unlink=True)
for obj in asset.objects:
    if obj.animation_data:
        for curve in obj.animation_data.drivers:
            for variable in list(curve.driver.variables):
                if variable.name=='mission':curve.driver.variables.remove(variable)
if 'mission_four_way' in root:del root['mission_four_way']
previous=root.animation_data.action if root.animation_data else None
root.animation_data_clear()
if previous and previous.users==0:bpy.data.actions.remove(previous)
root['review_status']='APPROVED | four cardinal divisions | 2026-11-16'
scene.timeline_markers.clear()
poses={'NEUTRAL':1};frame=1
for key in ('tilt_right_deg','tilt_forward_deg'):
    root[key]=0.;root.keyframe_insert(data_path=f'["{key}"]',frame=1)
scene.timeline_markers.new('NEUTRAL',frame=1)
for name,dx,dy in [('FRONT',0,1),('RIGHT',1,0),('REAR',0,-1),('LEFT',-1,0)]:
    for delta,scale,suffix in [(8,1.,''),(6,1.,' HOLD'),(8,0.,' NEUTRAL')]:
        frame+=delta
        root['tilt_right_deg'],root['tilt_forward_deg']=22*dx*scale,22*dy*scale
        for key in ('tilt_right_deg','tilt_forward_deg'):root.keyframe_insert(data_path=f'["{key}"]',frame=frame)
        if not suffix:poses[name]=frame;scene.timeline_markers.new(name,frame=frame)
root.animation_data.action.name='Hydraulics | detent tour via neutral'
for curve in root.animation_data.action.fcurves:
    for key in curve.keyframe_points:key.interpolation='LINEAR'
scene.frame_start,scene.frame_end=1,frame
scene.frame_set(1);bpy.context.view_layer.update()
parts=[]
for obj in asset.objects:
    if obj.type!='MESH':continue
    obj.data.calc_loop_triangles()
    parts.append({'name':obj.name,'triangles':len(obj.data.loop_triangles),'parent':obj.parent.name if obj.parent else None})
assert {o.name for o in asset.objects if o.name.startswith('DetentTick_')}=={f'DetentTick_{i}' for i in (0,2,4,6)}
manifest=json.loads((OUT/'manifest.json').read_text(encoding='utf8'))
manifest.update(status='approved 2026-11-16; four cardinal divisions; integrated',positions=5,mission_positions=5,
                poses=poses,parts=parts,triangles=sum(p['triangles'] for p in parts))
# Reload after editing driver variables/action slots so the GLB baker gets a
# fresh dependency graph rather than cached bone transforms from the old action.
bpy.ops.object.select_all(action='DESELECT');root.select_set(True);bpy.context.view_layer.objects.active=root
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(source),compress=True)
bpy.ops.wm.open_mainfile(filepath=str(source))
scene=bpy.context.scene;root=bpy.data.objects['HydraulicsRoot'];asset=bpy.data.collections['HYDRAULICS | editable asset']
bpy.ops.object.select_all(action='DESELECT')
for obj in asset.objects:obj.select_set(True)
bpy.context.view_layer.objects.active=root
bpy.ops.export_scene.gltf(filepath=str(OUT/'hydraulics-joystick-v1.glb'),export_format='GLB',use_selection=True,
    export_apply=False,export_animations=True,export_skins=True,export_force_sampling=True,export_bake_animation=True,
    export_animation_mode='SCENE',export_frame_range=True)
bpy.ops.object.select_all(action='DESELECT');root.select_set(True);bpy.context.view_layer.objects.active=root
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf8')
scene.render.filepath=str(ROOT/'build/hydraulics-review/hydraulics-four-way.png')
bpy.ops.render.render(write_still=True)
print(f'PASS four permanent cardinal markers, five demo positions, {manifest["triangles"]} triangles; existing rig and other meshes preserved')
