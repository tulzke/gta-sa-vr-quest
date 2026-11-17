"""Render the real controller, hinge detail and schematic roof mounting."""
from pathlib import Path
import bpy,sys
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'assets/firetruck-controller';REVIEW=ROOT/'build/firetruck-controller-review'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'firetruck-controller-v1.blend'))
scene=bpy.context.scene;studio=bpy.data.collections['PREVIEW ONLY | studio']
asset=bpy.data.collections['ASSET | Firetruck controller'];mount=bpy.data.objects['RoofCradle']
cam=scene.camera;ground=bpy.data.objects['REVIEW | ground']
screen=bpy.data.objects['CurvedCRTScreen'];video=screen.data.materials[0]
def mat(name,c,metal=0,rough=.6):
 m=bpy.data.materials.new(name);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF')
 p.inputs['Base Color'].default_value=(*c,1);p.inputs['Metallic'].default_value=metal;p.inputs['Roughness'].default_value=rough;return m
roofmat=mat('PREVIEW | black cabin headliner',(.008,.011,.015),rough=.95)
mirrormat=mat('PREVIEW | mirror frame',(.006,.009,.013),rough=.55)
mirror=mat('PREVIEW | mirror glass',(.35,.40,.44),metal=1,rough=.17)
off=mat('PREVIEW | powered off CRT',(.002,.004,.007),rough=.18)
off.node_tree.nodes.get('Principled BSDF').inputs['Coat Weight'].default_value=.4
context=[]
def box(name,loc,size,m,bevel=.002):
 existing=bpy.data.objects.get('REVIEW | '+name)
 if existing:
  existing.hide_render=True;context.append(existing);return existing
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name='REVIEW | '+name;o.dimensions=size
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 for c in list(o.users_collection):c.objects.unlink(o)
 studio.objects.link(o);o.data.materials.append(m)
 mod=o.modifiers.new('Preview soft edges','BEVEL');mod.width=min(bevel,min(size)*.4);mod.segments=3
 o.visible_shadow=False;o.hide_render=True;context.append(o);return o
box('schematic ceiling',(-.08,.05,.279),(.72,.38,.018),roofmat,.004)
box('schematic mirror stem',(-.185,.005,.249),(.009,.012,.042),mirrormat)
box('schematic mirror frame',(-.185,-.025,.225),(.166,.009,.043),mirrormat,.004)
box('schematic mirror surface',(-.185,-.030,.225),(.153,.001,.032),mirror,.0004)
def aim(target):cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler()
scene.cycles.samples=32
for name,position,target,scale,show_mount,frame in [
 ('controller-front',(.28,-.43,.29),(0,0,.125),.285,False,1),
 ('controller-rear',(-.30,.43,.30),(0,.02,.139),.31,True,1),
 ('controller-trigger',(.30,.21,.14),(0,.013,.078),.137,False,1),
 ('controller-trigger-pulled',(.30,.21,.14),(0,.013,.078),.137,False,11),
 ('controller-docked',(.19,-.55,.245),(-.065,.025,.135),.50,True,1),
]:
 if '--dock-only' in sys.argv and name!='controller-docked':continue
 for o in asset.objects:
  if o.parent==mount:o.hide_render=not show_mount
 is_context=name=='controller-docked'
 for o in context:o.hide_render=not is_context
 ground.hide_render=is_context
 screen.data.materials[0]=off if is_context else video
 cam.location=position;aim(target);cam.data.ortho_scale=scale;scene.frame_set(frame)
 scene.render.resolution_x=1400 if is_context else 1100;scene.render.resolution_y=950 if is_context else 1200
 scene.render.filepath=str(REVIEW/(name+'.png'));bpy.ops.render.render(write_still=True)
screen.data.materials[0]=video
for o in context:o.hide_render=True
ground.hide_render=False
for o in asset.objects:o.hide_render=False
scene.frame_set(1);cam.location=(.28,-.43,.29);cam.data.ortho_scale=.31;aim((0,.01,.139))
scene.render.resolution_x=1100;scene.render.resolution_y=1200
bpy.context.preferences.filepaths.save_version=0
if '--dock-only' not in sys.argv:bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'firetruck-controller-v1.blend'),compress=True)
print('FINAL REVIEW RENDERS READY',flush=True)
