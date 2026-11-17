"""Build the approved Firetruck controller; whole-asset budget <= 4000 triangles.

Run in Blender 4.5 with --background --factory-startup --python-exit-code 1.
Pass -- --no-render to rebuild the asset without product renders.
"""
from pathlib import Path
import bpy,bmesh,math,json,sys
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'assets/firetruck-controller'
REVIEW=ROOT/'build/firetruck-controller-review'
REVIEW.mkdir(parents=True,exist_ok=True)
OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene
scene.name='Firetruck video controller | MODEL REVIEW'
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
scene.render.fps=30;scene.frame_start=1;scene.frame_end=11
asset=bpy.data.collections.new('ASSET | Firetruck controller');scene.collection.children.link(asset)
studio=bpy.data.collections.new('PREVIEW ONLY | studio');scene.collection.children.link(studio)

def color(h):
 vals=[int(h[i:i+2],16)/255 for i in (0,2,4)]
 return tuple(v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in vals)
def mat(name,h,rough=.6,metal=0):
 m=bpy.data.materials.new(name);m.use_nodes=True;m.diffuse_color=(*color(h),1)
 p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*color(h),1)
 p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
 return m
shell=mat('Housing | cool graphite polymer','3D4651',.47,.12)
rearpaint=mat('Rear shell | dark graphite','303943',.55,.1)
rubber=mat('Grip and bezel | black rubber','151B21',.8)
dark=mat('Vents and recesses | black','080C10',.9)
steel=mat('Fasteners | blackened steel','353D46',.32,.8)
trigger_mat=mat('Trigger | satin graphite','4B5662',.4,.45)
glass_edge=mat('CRT glass edge | smoke','080F16',.19)
amber=mat('Power lamp | amber glass','FFB12B',.23)
ap=amber.node_tree.nodes.get('Principled BSDF');ap.inputs['Emission Color'].default_value=(*color('FF8C16'),1);ap.inputs['Emission Strength'].default_value=2.3

def move(o,col=asset):
 for c in list(o.users_collection):c.objects.unlink(o)
 col.objects.link(o);return o
def empty(name,parent=None,loc=(0,0,0)):
 o=bpy.data.objects.new(name,None);asset.objects.link(o);o.parent=parent;o.location=loc
 o.empty_display_size=.012;return o
root=empty('FiretruckMonitorRoot')
root['task']=113;root['approval_status']='Form approved 2026-11-17; optimized to <=4000 triangles per user request'
root['scope']='Firetruck 407 only; no SWAT; no game integration'
root['axes']='metres; +X right; +Y aiming forward; -Y screen toward operator; +Z up'
device=empty('MonitorDevice',root)
mount=empty('RoofCradle',root)
device['role']='handheld assembly; fixed one-to-one yaw/pitch alignment; no grab-time recenter'
mount['role']='fixed ceiling mount; device returns here on release'
def finish(o,name,m,parent,bevel=0,segments=3):
 move(o);o.name=name;o.parent=parent;o.data.materials.append(m)
 if bevel:
  mod=o.modifiers.new('Manufactured radii','BEVEL');mod.width=min(bevel,min(o.dimensions)*.45);mod.segments=min(segments,2) if name=='MainHousing' else 1
  bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=mod.name)
 for p in o.data.polygons:p.use_smooth=True
 normal=o.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL');normal.keep_sharp=True
 bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=normal.name)
 return o
def box(name,loc,size,m,parent=device,bevel=.001,segments=3):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.dimensions=size
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 return finish(o,name,m,parent,bevel,segments)
def mesh(name,verts,faces,m,parent=device,smooth=False):
 me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);me.update()
 bm=bmesh.new();bm.from_mesh(me);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(me);bm.free()
 o=bpy.data.objects.new(name,me);asset.objects.link(o);o.parent=parent;me.materials.append(m)
 for p in me.polygons:p.use_smooth=smooth
 return o
def cyl(name,loc,r,depth,m,parent=device,axis='Y',n=20,bevel=.00025):
 n=12 if 'PowerLamp' in name else 8
 bevel=0
 bpy.ops.mesh.primitive_cylinder_add(vertices=n,radius=r,depth=depth,location=loc)
 o=bpy.context.object
 if axis=='Y':o.rotation_euler.x=math.pi/2
 elif axis=='X':o.rotation_euler.y=math.pi/2
 bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
 return finish(o,name,m,parent,bevel,2)
def rr(w,h,r,segments=3):
 pts=[]
 for cx,cz,a0 in [(w/2-r,h/2-r,0),(-w/2+r,h/2-r,90),(-w/2+r,-h/2+r,180),(w/2-r,-h/2+r,270)]:
  for i in range(segments+1):
   a=math.radians(a0+i*90/segments);pts.append((cx+r*math.cos(a),cz+r*math.sin(a)))
 return pts
def ring(name,w,h,r,iw,ih,ir,y0,y1,z,m,parent=device):
 outer=rr(w,h,r);inner=rr(iw,ih,ir);n=len(outer)
 vs=[(x,y,z+zz) for y in (y0,y1) for loop in (outer,inner) for x,zz in loop]
 faces=[]
 for i in range(n):
  j=(i+1)%n
  faces += [(i,j,n+j,n+i),(2*n+i,3*n+i,3*n+j,2*n+j),
            (i,2*n+i,2*n+j,j),(n+i,n+j,3*n+j,3*n+i)]
 return mesh(name,vs,faces,m,parent,True)
def cut(target,cutter):
 bpy.context.view_layer.objects.active=target
 mod=target.modifiers.new('Actual recessed opening','BOOLEAN');mod.operation='DIFFERENCE';mod.solver='EXACT';mod.object=cutter
 bpy.ops.object.modifier_apply(modifier=mod.name);bpy.data.objects.remove(cutter,do_unlink=True)
def profile_yz(name,pts,width,m,parent=device,bevel=.0007):
 n=len(pts);vs=[(x,y,z) for x in (-width/2,width/2) for y,z in pts]
 faces=[tuple(reversed(range(n))),tuple(range(n,n*2))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
 o=mesh(name,vs,faces,m,parent)
 mod=o.modifiers.new('Soft profile edges','BEVEL');mod.width=bevel;mod.segments=1
 bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=mod.name)
 return o

# Rounded two-piece housing. The display is recessed behind the rubber bezel.
body=box('MainHousing',(0,.005,.169),(.166,.090,.134),shell,bevel=.008,segments=4)
box('FrontHousingLip',(0,-.041,.169),(.160,.006,.128),rearpaint,bevel=.009,segments=4)
ring('DeepRubberBezel',.159,.126,.018,.127,.09525,.009,-.062,-.042,.173,rubber)
ring('ScreenInnerGasket',.130,.09825,.010,.123,.09225,.008,-.054,-.047,.173,dark)
back=box('RearServicePanel',(0,.052,.169),(.144,.009,.111),rearpaint,bevel=.008,segments=3)
for i in range(5):
 z=.145+i*.009
 cutter=box('TEMP rear vent',(0,.0575,z),(.096,.011,.0035),dark,bevel=.0014,segments=3)
 cut(back,cutter)
box('RearVentDarkBacking',(0,.0494,.163),(.102,.0008,.052),dark,bevel=.0003)
for side in (-1,1):
 for i in range(4):
  cutter=box('TEMP side vent',(side*.0825,.006,.131+i*.008),(.009,.032,.0034),dark,bevel=.0013,segments=3)
  cut(body,cutter)
 box(('Left' if side<0 else 'Right')+'VentBacking',(side*.079,.006,.143),(.0007,.034,.036),dark,bevel=.0002)
# Booleans invalidate the pre-cut weighted normals. Flat large faces retain
# the machined radii and prevent interpolation fans around the vent cutouts.
for cutmesh in (body,back):
 for poly in cutmesh.data.polygons:poly.use_smooth=False
 cutmesh.data.normals_split_custom_set([(0,0,0)]*len(cutmesh.data.loops))
for x in (-.069,.069):
 for z in (.123,.223):
  cyl('FrontFastener',(x,-.063,z),.00215,.0013,steel,n=16)
  box('FastenerSlot',(x,-.06375,z),(.0026,.00025,.0005),dark,bevel=.00015,segments=2)
for x in (-.061,.061):
 for z in (.126,.212):
  cyl('RearFastener',(x,.0573,z),.0018,.001,steel,n=16)
  box('RearFastenerSlot',(x,.0579,z),(.0022,.0002,.0004),dark,bevel=.00012,segments=2)
cyl('PowerLampMetalRim',(.069,-.063,.204),.00265,.0016,steel,n=24)
cyl('PowerLampAmberLens',(.069,-.0642,.204),.0019,.0012,amber,n=24)

# Continuous oval rubber grip with subtle molded finger scallops, not a round rod.
levels=[]
for i in range(14):
 t=i/13;z=.009+.096*t
 rx=.0155+.0028*math.sin(math.pi*t)**.65
 ry=.017+.0021*math.sin(math.pi*t)
 cy=-.009+.015*t
 if i==0:rx-=.001;ry-=.001
 levels.append((z,rx,ry,cy))
n=16;vs=[]
for z,rx,ry,cy in levels:
 groove=sum(.0015*math.exp(-((z-c)/.0032)**2) for c in (.029,.048,.067,.085))
 for i in range(n):
  a=i*math.tau/n;front=max(0,math.sin(a));vs.append(((rx-.45*groove)*math.cos(a),cy+(ry-.45*groove)*math.sin(a)-.55*groove*front**4,z))
faces=[]
for j in range(len(levels)-1):
 for i in range(n):faces.append((j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i))
faces += [tuple(reversed(range(n))),tuple(range((len(levels)-1)*n,len(levels)*n))]
grip=mesh('PistolGripRubber',vs,faces,rubber,smooth=True)
box('GripShoulder',(0,.004,.101),(.043,.046,.022),shell,bevel=.009,segments=4)
box('GripHeel',(0,-.009,.009),(.034,.037,.009),rubber,bevel=.004,segments=3)
triggerpivot=empty('TriggerPivot',device,(0,.018,.103));triggerpivot.rotation_mode='XYZ'
triggerpivot['axis']='local X';triggerpivot['travel_deg']=[0,-12]
controls=[Vector((.003,0)),Vector((.025,-.005)),Vector((.030,-.025)),Vector((.009,-.033))]
center=[];normals=[]
for i in range(9):
 t=i/8;a,b,c,d=controls
 p=a*(1-t)**3+b*(3*(1-t)**2*t)+c*(3*(1-t)*t*t)+d*t**3
 tangent=(b-a)*(3*(1-t)**2)+(c-b)*(6*(1-t)*t)+(d-c)*(3*t*t)
 normal=Vector((-tangent.y,tangent.x)).normalized()
 center.append(p);normals.append(normal)
triggerpts=[tuple(p+n*.0027) for p,n in zip(center,normals)]+[tuple(p-n*.0027) for p,n in reversed(list(zip(center,normals)))]
trigger=profile_yz('IndexFingerTrigger',triggerpts,.010,trigger_mat,parent=triggerpivot,bevel=.0009)
cyl('TriggerPivotCap_R',(.008,.018,.103),.003,.0018,steel,axis='X',n=20)
cyl('TriggerPivotCap_L',(-.008,.018,.103),.003,.0018,steel,axis='X',n=20)
for f,angle in [(1,0),(11,-12)]:
 triggerpivot.rotation_euler.x=math.radians(angle);triggerpivot.keyframe_insert('rotation_euler',frame=f)
triggerpivot.animation_data.action.name='WaterTriggerPull'
for fc in triggerpivot.animation_data.action.fcurves:
 for key in fc.keyframe_points:key.interpolation='LINEAR'
scene.frame_set(1)
handR=empty('RightHandGripAnchor',device,(0,.003,.065))
handL=empty('LeftHandGripAnchor',device,(0,.003,.065))
handR['role']='symmetric handle; right hand pose to be fitted separately'
handL['role']='symmetric handle; left hand pose to be fitted separately'
empty('IndexTriggerContact',triggerpivot,(0,.020,-.019))
aimframe=empty('AimFrame',device,(0,.052,.173));aimframe['forward']='+Y';aimframe['camera_role']='orientation reference only; actual camera is beside nozzle'
screenframe=empty('ScreenFrame',device,(0,-.055,.173));screenframe['view_normal']='-Y';screenframe['screen_aspect']=4/3

# A passive ceiling cradle with real back support and non-obstructing side clips.
box('CeilingPlate',(0,.045,.268),(.079,.059,.004),rearpaint,parent=mount,bevel=.004,segments=3)
box('CeilingStem',(0,.059,.247),(.032,.025,.040),shell,parent=mount,bevel=.004)
box('CradleBackSpine',(0,.064,.202),(.072,.012,.077),shell,parent=mount,bevel=.005)
box('CradleUpperStop',(0,.036,.239),(.090,.054,.007),rearpaint,parent=mount,bevel=.002)
for side in (-1,1):
 box(('Left' if side<0 else 'Right')+'CradleGuide',(side*.0835,.036,.207),(.008,.047,.053),rearpaint,parent=mount,bevel=.003)
 box(('Left' if side<0 else 'Right')+'CradlePad',(side*.079,.035,.207),(.0018,.029,.034),rubber,parent=mount,bevel=.0007)
for x in (-.028,.028):
 for y in (.027,.063):cyl('CeilingMountScrew',(x,y,.2658),.0021,.0011,steel,parent=mount,axis='Z',n=16)
empty('CeilingAttachment',mount,(0,.045,.270))
empty('DockDeviceTarget',mount,(0,0,0))

# Self-authored color feed: separate scene, no original game material/assets.
feed=bpy.data.scenes.new('PREVIEW ONLY | synthetic color camera feed')
bpy.context.window.scene=feed;feed.render.engine='CYCLES';feed.cycles.samples=16;feed.cycles.use_denoising=True
feed.render.resolution_x=640;feed.render.resolution_y=480;feed.render.resolution_percentage=100
feed.view_settings.view_transform='Standard';feed.world=bpy.data.worlds.new('Demo daylight');feed.world.use_nodes=True
feed.world.node_tree.nodes['Background'].inputs[0].default_value=(.45,.60,.78,1);feed.world.node_tree.nodes['Background'].inputs[1].default_value=.7
def feedbox(name,loc,size,material):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name='DEMO | '+name;o.dimensions=size
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(material);return o
asphalt=mat('DEMO | asphalt','5B6064',1);pavement=mat('DEMO | pavement','9D9C94',1);stripe=mat('DEMO | lane paint','CDC18D',1)
feedbox('road',(0,15,-.12),(11,70,.2),asphalt)
for side in (-1,1):feedbox('sidewalk',(side*6.5,15,-.04),(2,70,.25),pavement)
for y in range(-8,48,5):feedbox('center line',(0,y,.002),(.14,2,.003),stripe)
windowmat=mat('DEMO | windows','344852',.3)
for side in (-1,1):
 for i in range(7):
  y=i*8+1;h=5.5+(i%3)*2
  building=mat('DEMO | building '+str(side)+'_'+str(i),['9A8978','B4ADA0','8A9394'][i%3],.9)
  feedbox('building',(side*10,y,h/2),(6,6,h),building)
  for z in (1.5,3.2,4.9):
   if z<h:feedbox('window',(side*6.98,y,z),(.035,3.4,1),windowmat)
 carmat=mat('DEMO | parked car '+str(side),'667E86' if side>0 else '997C6D',.5,.1)
 for y in (7,25):
  feedbox('car body',(side*4.15,y,.65),(1.7,3.8,.8),carmat)
  feedbox('car cabin',(side*4.15,y,1.25),(1.55,1.9,.65),windowmat)
bpy.ops.object.light_add(type='SUN',location=(0,0,15));bpy.context.object.data.energy=2;bpy.context.object.rotation_euler=(.4,-.3,-.5)
bpy.ops.object.camera_add(location=(0,-7,3));fcam=bpy.context.object;fcam.rotation_euler=(Vector((0,23,2))-fcam.location).to_track_quat('-Z','Y').to_euler();fcam.data.lens=32;fcam.data.clip_start=.02;feed.camera=fcam
def emissive(name,h):
 m=mat(name,h);p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Emission Color'].default_value=(*color(h),1);p.inputs['Emission Strength'].default_value=1;return m
wh=emissive('DEMO | central aim','E5E4DC');am=emissive('DEMO | actual nozzle','FFB52A')
for size in ((.005,.00065,.0001),(.00065,.005,.0001)):
 o=feedbox('center crosshair',(0,0,0),size,wh);o.parent=fcam;o.location=(0,0,-.2)
cu=bpy.data.curves.new('DEMO | lagging ring','CURVE');cu.dimensions='3D';cu.bevel_depth=.00028;cu.bevel_resolution=1
sp=cu.splines.new('POLY');sp.points.add(47)
for i,p in enumerate(sp.points):a=i*math.tau/48;p.co=(.0033*math.cos(a),.0033*math.sin(a),0,1)
sp.use_cyclic_u=True;o=bpy.data.objects.new('DEMO | lagging ring',cu);feed.collection.objects.link(o);o.parent=fcam;o.location=(-.010,-.006,-.2);cu.materials.append(am)
feed.render.filepath=str(REVIEW/'preview-color-feed.png')
if '--no-render' not in sys.argv or not (REVIEW/'preview-color-feed.png').exists():bpy.ops.render.render(write_still=True)
bpy.context.window.scene=scene

# Curved, closed CRT glass with a separately addressable video material and UVs.
video=mat('CRT_VideoSurface_REPLACE_WITH_CAMERA','FFFFFF',.22)
vp=video.node_tree.nodes.get('Principled BSDF');vp.inputs['Coat Weight'].default_value=.28;vp.inputs['Coat Roughness'].default_value=.16
image=bpy.data.images.load(str(REVIEW/'preview-color-feed.png'));image.name='PREVIEW ONLY | self-authored sample feed';image.pack()
tx=video.node_tree.nodes.new('ShaderNodeTexImage');tx.name='RuntimeVideoInput';tx.label='Preview only: replace with live camera';tx.image=image
video.node_tree.links.new(tx.outputs['Color'],vp.inputs['Base Color']);video.node_tree.links.new(tx.outputs['Color'],vp.inputs['Emission Color']);vp.inputs['Emission Strength'].default_value=.55
video['role']='VIDEO_SURFACE: sample image only, replace at runtime; color CRT'
boundary=rr(.122,.0915,.008,segments=4);n=len(boundary);rings=5
vs=[(0,-.059,.173)]
for j in range(1,rings+1):
 t=j/rings
 for x,z in boundary:
  u=x/.061;v=z/.04575;y=-.059+.0033*t*t*(u*u+v*v)
  vs.append((x*t,y,.173+z*t))
back_start=len(vs)
vs += [(x,-.047,.173+z) for x,z in boundary]
faces=[]
for i in range(n):faces.append((0,1+i,1+(i+1)%n))
for j in range(rings-1):
 a=1+j*n;b=a+n
 for i in range(n):k=(i+1)%n;faces.append((a+i,b+i,b+k,a+k))
front_faces=len(faces);a=1+(rings-1)*n
for i in range(n):k=(i+1)%n;faces.append((a+i,back_start+i,back_start+k,a+k))
faces.append(tuple(reversed(range(back_start,back_start+n))))
screen=mesh('CurvedCRTScreen',vs,faces,video,smooth=True);screen.data.materials.append(glass_edge)
uv=screen.data.uv_layers.new(name='VideoUV')
for pi,p in enumerate(screen.data.polygons):
 p.material_index=0 if pi<front_faces else 1
 for li in p.loop_indices:
  x,y,z=screen.data.vertices[screen.data.loops[li].vertex_index].co;uv.data[li].uv=(x/.122+.5,(z-.173)/.0915+.5)
screen['active_size_m']=[.122,.0915];screen['aspect']=4/3;screen['role']='closed convex CRT surface, unique camera material'

# Asset-only export. No studio/cabin/game meshes are selected.
scene.frame_set(1);bpy.context.view_layer.update()
parts=[]
for o in asset.objects:
 if o.type!='MESH':continue
 o.data.calc_loop_triangles();parts.append({'name':o.name,'triangles':len(o.data.loop_triangles),'parent':o.parent.name,'materials':[m.name for m in o.data.materials]})
triangles=sum(p['triangles'] for p in parts)
assert triangles<=4000,triangles
bpy.ops.object.select_all(action='DESELECT')
for o in asset.objects:o.select_set(True)
bpy.context.view_layer.objects.active=root
bpy.ops.export_scene.gltf(filepath=str(OUT/'firetruck-controller-v1.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_animations=True,export_yup=True,export_animation_mode='ACTIONS',export_force_sampling=True)

# Actual mesh renders, not generated concept imagery.
def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
def studio_obj(o):return move(o,studio)
bpy.ops.object.camera_add(location=(.29,-.42,.28));cam=studio_obj(bpy.context.object);cam.name='REVIEW | camera';cam.data.type='ORTHO';cam.data.ortho_scale=.285;cam.data.clip_start=.001;aim(cam,(0,0,.126));scene.camera=cam
for name,loc,power,size in [('Key',(-.22,-.24,.43),1.5,.27),('Fill',(.25,-.12,.25),.7,.24),('Rim',(.04,.28,.31),2.5,.2)]:
 bpy.ops.object.light_add(type='AREA',location=loc);o=studio_obj(bpy.context.object);o.name='REVIEW | '+name;o.data.energy=power;o.data.shape='DISK';o.data.size=size;aim(o,(0,0,.15))
floor_mat=mat('PREVIEW | neutral cool gray','78828D',.95)
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,.001));o=studio_obj(bpy.context.object);o.name='REVIEW | ground';o.data.materials.append(floor_mat)
scene.world=bpy.data.worlds.new('Studio world');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.45,.48,.53,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.2
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True
scene.render.resolution_x=1100;scene.render.resolution_y=1200;scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG';scene.view_settings.view_transform='AgX'
for name,position,target,scale,hide_mount,frame in ([] if '--no-render' in sys.argv else [
 ('controller-front',(.28,-.43,.29),(0,0,.125),.285,True,1),
 ('controller-rear',(-.30,.43,.30),(0,.02,.139),.31,False,1),
 ('controller-trigger',(.30,.21,.14),(0,.013,.078),.137,True,1),
 ('controller-trigger-pulled',(.30,.21,.14),(0,.013,.078),.137,True,11),
 ('controller-docked',(.20,-.40,.27),(0,.015,.147),.315,False,1),
]):
 for o in asset.objects:
  if o.parent==mount:o.hide_render=hide_mount
 cam.location=position;aim(cam,target);cam.data.ortho_scale=scale;scene.frame_set(frame)
 scene.render.filepath=str(REVIEW/(name+'.png'));bpy.ops.render.render(write_still=True)
for o in asset.objects:o.hide_render=False
scene.frame_set(1);cam.location=(.28,-.43,.29);cam.data.ortho_scale=.31;aim(cam,(0,.01,.139))
studio.hide_viewport=True
for scr in bpy.data.screens:
 for area in scr.areas:
  if area.type=='VIEW_3D':
   s=area.spaces.active;s.region_3d.view_location=(0,.01,.139);s.region_3d.view_distance=.37;s.region_3d.view_rotation=cam.rotation_euler.to_quaternion();s.clip_start=.0005;s.shading.type='MATERIAL'
bpy.ops.object.select_all(action='DESELECT');body.select_set(True);bpy.context.view_layer.objects.active=body
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'firetruck-controller-v1.blend'),compress=True)
manifest={'task':113,'status':'approved form; optimized <=4000 triangles; no gameplay integration','scope':'Firetruck 407 only','units':'metres','axes':root['axes'],'body_size_m':[.166,.09,.134],'body_top_m':.236,'ceiling_anchor_m':[0,.045,.270],'active_screen_m':[.122,.0915],'trigger_rotation_deg':[0,-12],'trigger_animation':'WaterTriggerPull','no_rotary_knob':True,'video_material':video.name,'sample_feed':'self-authored preview, not game assets; replace at runtime','triangle_budget':4000,'triangles':triangles,'parts':parts}
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf8')
print('CREATED',triangles,'triangles',len(parts),'mesh parts; optimized source+GLB; no gameplay integration',flush=True)
