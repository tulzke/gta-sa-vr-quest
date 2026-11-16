"""Create task 31's editable Blender asset, continuous bellows rig and review renders.

Blender 4.5 --background --factory-startup --threads 8 --python-exit-code 1
  --python tools/build_hydraulics_joystick.py [-- --overwrite]
No game integration, installation, staging or commits. Existing source is protected.
"""
import json
import math
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT, REVIEW = ROOT / 'assets/hydraulics', ROOT / 'build/hydraulics-review'
BLEND = OUT / 'hydraulics-joystick-v1.blend'
if BLEND.exists() and '--overwrite' not in sys.argv:
    raise RuntimeError('Preserve artist edits: source exists; use --overwrite explicitly')
OUT.mkdir(parents=True, exist_ok=True)
REVIEW.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.length_unit = 'MILLIMETERS'
scene.unit_settings.scale_length = 1
scene.render.fps = 30
asset = bpy.data.collections.new('HYDRAULICS | editable asset')
studio = bpy.data.collections.new('REVIEW ONLY | studio')
scene.collection.children.link(asset)
scene.collection.children.link(studio)


def empty(name, parent=None, location=(0, 0, 0)):
    o = bpy.data.objects.new(name, None)
    asset.objects.link(o)
    o.parent, o.location = parent, location
    o.empty_display_size = .008
    return o


root = empty('HydraulicsRoot')
root['task'] = 31
root['review_status'] = 'MODEL REVIEW | NOT COMMITTED'
root['axes'] = 'metres: Blender X right, Y forward, Z up'
root['tilt_right_deg'] = 0.
root['tilt_forward_deg'] = 0.
root['button_press'] = 0.
root['high_mode'] = 0.
root['mission_four_way'] = 0.
for k in ('tilt_right_deg', 'tilt_forward_deg'):
    root.id_properties_ui(k).update(min=-25., max=25., soft_min=-22., soft_max=22., description='Continuous tilt vector; nominal detent radius 22 degrees, combined limit 25 degrees')
for k in ('button_press', 'high_mode', 'mission_four_way'):
    root.id_properties_ui(k).update(min=0., max=1.)
root['bellows_note'] = '21 axial ring joints; smoothstep weights; fixed base, exact shaft collar; continuous any-angle deformation'
fixed = empty('HydraulicsFixed', root)
PX, PY, PZ, GRIP = -.032, .008, .018, .055
BOTTOM, TOP, SIDES = .019, .045, 24
pivot = empty('HydraulicsPivot', root, (PX, PY, PZ))
pivot.rotation_mode = 'QUATERNION'
grip_anchor = empty('HydraulicsGripAnchor', pivot, (0, 0, GRIP))
grip_anchor['sphere_radius_m'] = .0225
button = empty('HydraulicsModeButton', fixed, (.047, -.015, .015))
button['travel_m'] = .006
button['interaction'] = 'free-hand fingertip press; Trigger stays accelerator'


def drive(owner, path, index, expression):
    f = owner.driver_add(path, index) if index is not None else owner.driver_add(path)
    d = f.driver
    for var, prop in [('x', 'tilt_right_deg'), ('y', 'tilt_forward_deg'), ('press', 'button_press'), ('high', 'high_mode'), ('mission', 'mission_four_way')]:
        v = d.variables.new()
        v.name, v.type = var, 'SINGLE_PROP'
        v.targets[0].id = root
        v.targets[0].data_path = '["' + prop + '"]'
    d.expression = expression


# Exponential-map swing: both diagonal and cardinal tilt have the same radius.
RAD = 'sqrt(x*x+y*y)'
ANGLE = f'(min({RAD},25)*0.0174532925199433)'


def rotation_drivers(owner, weight):
    half = f'({ANGLE}*{weight:.12g}*0.5)'
    drive(owner, 'rotation_quaternion', 0, f'cos({half})')
    drive(owner, 'rotation_quaternion', 1, f'-y/{RAD}*sin({half}) if {RAD}>0.000001 else 0')
    drive(owner, 'rotation_quaternion', 2, f'x/{RAD}*sin({half}) if {RAD}>0.000001 else 0')
    drive(owner, 'rotation_quaternion', 3, '0')


rotation_drivers(pivot, 1.)
drive(button, 'location', 2, '.015-.006*press')


def material(name, color, metallic=0., roughness=.5, emission=0.):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    p = m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = (*color, 1)
    p.inputs['Metallic'].default_value = metallic
    p.inputs['Roughness'].default_value = roughness
    if emission:
        p.inputs['Emission Color'].default_value = (*color, 1)
        p.inputs['Emission Strength'].default_value = emission
    return m


graphite = material('Housing | graphite satin', (.055, .068, .078), .4, .43)
edge = material('Deck | brushed dark alloy', (.125, .15, .17), .65, .38)
steel = material('Shaft | satin steel', (.48, .53, .57), .82, .25)
dark = material('Fasteners | gunmetal', (.035, .042, .048), .65, .4)
rubber = material('Bellows | matte elastomer', (.018, .023, .027), 0, .78)
ivory = material('Ball | warm ivory polymer', (.79, .76, .66), .02, .29)
amber = material('Accents | amber polymer', (.95, .37, .035), .12, .4)
ink = material('Labels | warm white atlas', (.83, .86, .85), 0, .7)
led_low = material('LOW | amber indicator', (.95, .38, .035), .1, .3)
led_high = material('HIGH | amber indicator', (.95, .38, .035), .1, .3)
for mat, expr in [(led_low, '.08+1.1*(1-high)'), (led_high, '.08+1.1*high')]:
    p = mat.node_tree.nodes['Principled BSDF']
    p.inputs['Emission Color'].default_value = (.95, .38, .035, 1)
    drive(p.inputs['Emission Strength'], 'default_value', None, expr)


def mesh(name, vertices, faces, mat, parent=fixed, smooth=False, solid=True):
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices, [], faces)
    data.update()
    bm = bmesh.new()
    bm.from_mesh(data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(data)
    bm.free()
    obj = bpy.data.objects.new(name, data)
    asset.objects.link(obj)
    obj.parent = parent
    obj.data.materials.append(mat)
    obj['closed_solid'] = solid
    for poly in data.polygons:
        poly.use_smooth = smooth and len(poly.vertices) <= 4
    return obj


def lathe(name, profile, mat, parent=fixed, sides=24, origin=(0, 0, 0)):
    verts = [(origin[0]+r*math.cos(i*math.tau/sides), origin[1]+r*math.sin(i*math.tau/sides), origin[2]+z) for z, r in profile for i in range(sides)]
    faces = [(j*sides+i, j*sides+(i+1)%sides, (j+1)*sides+(i+1)%sides, (j+1)*sides+i) for j in range(len(profile)-1) for i in range(sides)]
    faces += [tuple(reversed(range(sides))), tuple(range((len(profile)-1)*sides, len(profile)*sides))]
    return mesh(name, verts, faces, mat, parent, True)


def annulus(name, r0, r1, z, mat, center=(PX, PY), parent=fixed):
    vs = [(center[0]+r*math.cos(i*math.tau/32), center[1]+r*math.sin(i*math.tau/32), z) for r in (r0, r1) for i in range(32)]
    fs = [(i, (i+1)%32, (i+1)%32+32, i+32) for i in range(32)]
    return mesh(name, vs, fs, mat, parent, solid=False)


def ring_shell(name, profile, mat, origin):
    n=32
    vs=[(origin[0]+r*math.cos(i*math.tau/n),origin[1]+r*math.sin(i*math.tau/n),origin[2]+z) for z,r in profile for i in range(n)]
    fs=[(j*n+i,j*n+(i+1)%n,((j+1)%len(profile))*n+(i+1)%n,((j+1)%len(profile))*n+i) for j in range(len(profile)) for i in range(n)]
    return mesh(name,vs,fs,mat,smooth=True)


# Two stacked octagonal plates, softly beveled for readable highlights.
def plate(name, width, depth, bottom, top, mat, inset=.006):
    x, y, c = width/2, depth/2, inset
    outline = [(-x+c,-y),(x-c,-y),(x,-y+c),(x,y-c),(x-c,y),(-x+c,y),(-x,y-c),(-x,-y+c)]
    vs = [(a,b,z) for z in (bottom,top) for a,b in outline]
    fs = [tuple(reversed(range(8))), tuple(range(8,16))]+[(i,(i+1)%8,(i+1)%8+8,i+8) for i in range(8)]
    o = mesh(name, vs, fs, mat)
    bevel = o.modifiers.new('Small machined edge radius', 'BEVEL')
    bevel.width, bevel.segments = .0008, 2
    bevel.affect = 'EDGES'
    bpy.context.view_layer.objects.active = o
    o.select_set(True)
    bpy.ops.object.modifier_apply(modifier=bevel.name)
    o.select_set(False)
    return o


plate('MountingHousing', .150, .110, 0, .011, graphite)
plate('TopDeck', .146, .106, .0105, .0145, edge)
for i, (x, y) in enumerate([(-.063,-.043),(.063,-.043),(-.063,.043),(.063,.043)]):
    lathe(f'MountScrew_{i+1}', [(0,.0031),(.001,.0031),(.0014,.0026)], dark, sides=12, origin=(x,y,.0145))
    mesh(f'ScrewRecess_{i+1}', [(x-.0015,y-.00035,.01595),(x+.0015,y-.00035,.01595),(x+.0015,y+.00035,.01595),(x-.0015,y+.00035,.01595)], [(0,1,2,3)], rubber, solid=False)

lathe('FixedBellowsFlange', [(0,.0275),(.0013,.028),(.0028,.0275),(.0038,.0258)], dark, origin=(PX,PY,.0145), sides=32)
annulus('Flange | fine steel edge', .0268, .0277, .018, steel)
lathe('LeverShaft', [(0,.006),(.056,.006)], steel, pivot, sides=20)
lathe('MovingNeckClamp', [(.026,.0086),(.027,.0095),(.029,.0095),(.030,.0086)], dark, pivot, sides=24)
lathe('Neck | amber accent', [(.0298,.0082),(.031,.0082)], amber, pivot, sides=24)

bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=16, radius=.0225, location=(0,0,0))
ball = bpy.context.object
ball.name = 'SphericalGrip'
for col in list(ball.users_collection): col.objects.unlink(ball)
asset.objects.link(ball)
ball.parent, ball.location = pivot, (0,0,GRIP)
ball.data.materials.append(ivory)
ball['closed_solid'] = True
for p in ball.data.polygons: p.use_smooth = True

# A true folded hollow shell. Both lips remain attached, rather than sliding
# a static rubber object under the shaft or crossfading nine unrelated poses.
PROFILE = [(.019,.0248),(.020,.0250),(.0212,.0230),(.0225,.0260),(.024,.0268),(.0255,.0255),(.027,.0208),
           (.0285,.0198),(.030,.0230),(.0315,.0236),(.033,.0220),(.0345,.0178),(.036,.0166),
           (.0375,.0190),(.039,.0192),(.0405,.0178),(.042,.0110),(.043,.0082),(.0438,.0080),(.0444,.0080),(.045,.0080)]
count = len(PROFILE)
boot_verts = []
for inner in (False, True):
    for z, r in PROFILE:
        rr = r-.001 if inner else r
        boot_verts += [(PX+rr*math.cos(i*math.tau/SIDES),PY+rr*math.sin(i*math.tau/SIDES),z) for i in range(SIDES)]
boot_faces = []
for layer in (0,1):
    offset = layer*count*SIDES
    for j in range(count-1):
        for i in range(SIDES):
            f = (offset+j*SIDES+i,offset+j*SIDES+(i+1)%SIDES,offset+(j+1)*SIDES+(i+1)%SIDES,offset+(j+1)*SIDES+i)
            boot_faces.append(f if layer==0 else tuple(reversed(f)))
for j in (0,count-1):
    for i in range(SIDES):
        a,b = j*SIDES+i,j*SIDES+(i+1)%SIDES
        boot_faces.append((a,b,b+count*SIDES,a+count*SIDES))
boot = mesh('Bellows | continuously deformed',boot_verts,boot_faces,rubber,smooth=True)
boot['ring_count'], boot['radial_segments'] = count, SIDES
boot['wall_thickness_m'] = .001

rig_data = bpy.data.armatures.new('Bellows ring skeleton')
rig = bpy.data.objects.new('BellowsRig',rig_data)
asset.objects.link(rig)
rig.parent = root
boot.parent = rig
rig.show_in_front = True
rig.display_type = 'WIRE'
bpy.context.view_layer.objects.active = rig
rig.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
for j,(z,r) in enumerate(PROFILE):
    b = rig_data.edit_bones.new(f'BellowsRing_{j:02d}')
    b.head, b.tail = (PX,PY,z),(PX,PY+.004,z)
bpy.ops.object.mode_set(mode='OBJECT')
rig.select_set(False)
for j,(z,r) in enumerate(PROFILE):
    t = (z-BOTTOM)/(TOP-BOTTOM)
    weight = t*t*(3-2*t)
    bone = rig.pose.bones[f'BellowsRing_{j:02d}']
    bone.rotation_mode = 'QUATERNION'
    rotation_drivers(bone,weight)
    length = TOP-PZ
    for axis,var in [(0,'x'),(1,'y')]:
        drive(bone,'location',axis,f'{weight*length:.12g}*sin({ANGLE})*{var}/{RAD} if {RAD}>0.000001 else 0')
    drive(bone,'location',2,f'{weight*length:.12g}*(cos({ANGLE})-1)')
    vg = boot.vertex_groups.new(name=bone.name)
    ids = [layer*count*SIDES+j*SIDES+i for layer in (0,1) for i in range(SIDES)]
    vg.add(ids,1.,'REPLACE')
mod = boot.modifiers.new('Continuous accordion | fixed foot to moving neck','ARMATURE')
mod.object = rig
mod.use_deform_preserve_volume = False

# Eight external detent indices. The actual gate sits beneath the sealed boot.
for i in range(8):
    a = i*math.tau/8
    marker = empty(f'DetentIndex_{i}',fixed,(PX+.032*math.sin(a),PY+.032*math.cos(a),.0147))
    marker['direction_angle_deg'] = i*45
    marker['diagonal'] = bool(i%2)
    if i%2:
        for axis in range(3): drive(marker,'scale',axis,'1-mission')
    length,width = (.005,.0015) if not i%2 else (.0035,.0013)
    vs = [(-width/2,-length/2,0),(width/2,-length/2,0),(width/2,length/2,0),(-width/2,length/2,0)]
    tick = mesh(f'DetentTick_{i}',vs,[(0,1,2,3)],amber,marker,solid=False)
    tick.rotation_euler.z = -a

# A distinct finger button on the right-hand wing; its centre is 82 mm from
# the joystick pivot. The cap has 6 mm travel, with its bottom concealed.
ring_shell('ModeButtonFixedBezel',[(0,.019),(.003,.019),(.004,.018),(.004,.0157),(0,.0157)],dark,(.047,-.015,.0145))
annulus('Button | steel rim',.017,.018,.0186,steel,center=(.047,-.015))
lathe('ModeButtonCap',[(0,.015),(.006,.015),(.007,.0144)],amber,button,sides=32)
button_anchor = empty('FingertipPressAnchor',button,(0,0,.007))
button_anchor['press_direction'] = 'local -Z'
for name,x,mat in [('LOW',.032,led_low),('HIGH',.059,led_high)]:
    lathe('StateLamp_'+name,[(0,.0027),(.001,.0027),(.0015,.0022)],mat,origin=(x,.020,.0145),sides=16)

# Shared packed label atlas, no loose fonts or texture paths required by source.
FONT = {
'A':['01110','10001','10001','11111','10001','10001','10001'],
'B':['11110','10001','10001','11110','10001','10001','11110'],
'C':['01111','10000','10000','10000','10000','10000','01111'],
'D':['11110','10001','10001','10001','10001','10001','11110'],
'E':['11111','10000','10000','11110','10000','10000','11111'],
'F':['11111','10000','10000','11110','10000','10000','10000'],
'G':['01111','10000','10000','10111','10001','10001','01111'],
'H':['10001','10001','10001','11111','10001','10001','10001'],
'I':['11111','00100','00100','00100','00100','00100','11111'],
'K':['10001','10010','10100','11000','10100','10010','10001'],
'L':['10000','10000','10000','10000','10000','10000','11111'],
'M':['10001','11011','10101','10101','10001','10001','10001'],
'N':['10001','11001','11001','10101','10011','10011','10001'],
'O':['01110','10001','10001','10001','10001','10001','01110'],
'R':['11110','10001','10001','11110','10100','10010','10001'],
'S':['01111','10000','10000','01110','00001','00001','11110'],
'T':['11111','00100','00100','00100','00100','00100','00100'],
'U':['10001','10001','10001','10001','10001','10001','01110'],
'W':['10001','10001','10001','10101','10101','11011','10001'],
'Y':['10001','10001','01010','00100','00100','00100','00100']}
W,H=1024,256
pixels=[0.]*(W*H*4)
rects={}
for j,label in enumerate(['HYDRAULICS','MODE','LOW','HIGH','FRONT','BACK','L','R']):
    col,row=j%4,j//4
    step=min(6,236//(6*len(label)-1))
    ww,hh=(6*len(label)-1)*step,7*step
    ox,oy=col*256+(256-ww)//2,row*128+(128-hh)//2
    for k,char in enumerate(label):
        for y,line in enumerate(FONT[char]):
            for x,bit in enumerate(line):
                if bit!='1':continue
                for dy in range(step):
                    for dx in range(step):
                        at=((oy+(6-y)*step+dy)*W+ox+k*6*step+x*step+dx)*4
                        pixels[at:at+4]=[1.,1.,1.,1.]
    rects[label]=[(ox-1)/W,(oy-1)/H,(ox+ww+1)/W,(oy+hh+1)/H]
image=bpy.data.images.new('Hydraulics | shared label atlas',width=W,height=H,alpha=True)
image.pixels.foreach_set(pixels)
image.filepath_raw=str(OUT/'hydraulics-labels.png')
image.file_format='PNG'
image.save()
image.pack()
shader=ink.node_tree.nodes['Principled BSDF']
tex=ink.node_tree.nodes.new('ShaderNodeTexImage')
tex.image=image
ink.node_tree.links.new(tex.outputs['Alpha'],shader.inputs['Alpha'])
ink.surface_render_method='DITHERED'


def label(text,x,y,width,height):
    z=.0148
    vs=[(x-width/2,y-height/2,z),(x+width/2,y-height/2,z),(x+width/2,y+height/2,z),(x-width/2,y+height/2,z)]
    o=mesh('Label_'+text,vs,[(0,1,2,3)],ink,solid=False)
    uv=o.data.uv_layers.new(name='LabelAtlas')
    u0,v0,u1,v1=rects[text]
    coords=[(u0,v0),(u1,v0),(u1,v1),(u0,v1)]
    for loop in o.data.loops:uv.data[loop.index].uv=coords[loop.vertex_index]


label('HYDRAULICS',-.027,-.044,.066,.0065)
label('MODE',.047,-.040,.021,.006)
label('LOW',.032,.029,.014,.005)
label('HIGH',.059,.029,.017,.005)
label('FRONT',PX,.050,.021,.0045)
label('BACK',PX,-.033,.017,.0045)

# An editable tour through all nine detents. Every movement passes through N;
# evaluation between keyframes also demonstrates genuinely continuous rubber.
frame=1
poses={'NEUTRAL':1}
root.keyframe_insert(data_path='["tilt_right_deg"]',frame=frame)
root.keyframe_insert(data_path='["tilt_forward_deg"]',frame=frame)
scene.timeline_markers.new('NEUTRAL',frame=1)
for name,dx,dy in [('FRONT',0,1),('FRONT RIGHT',1,1),('RIGHT',1,0),('REAR RIGHT',1,-1),('REAR',0,-1),('REAR LEFT',-1,-1),('LEFT',-1,0),('FRONT LEFT',-1,1)]:
    length=math.hypot(dx,dy)
    for delta,scale,suffix in [(8,1.,''),(6,1.,' HOLD'),(8,0.,' NEUTRAL')]:
        frame+=delta
        root['tilt_right_deg']=22*dx/length*scale
        root['tilt_forward_deg']=22*dy/length*scale
        for key in ['tilt_right_deg','tilt_forward_deg']:root.keyframe_insert(data_path='["'+key+'"]',frame=frame)
        if not suffix:
            poses[name]=frame
            scene.timeline_markers.new(name,frame=frame)
root.animation_data.action.name='Hydraulics | detent tour via neutral'
for curve in root.animation_data.action.fcurves:
    for k in curve.keyframe_points:k.interpolation='LINEAR'
scene.frame_start,scene.frame_end=1,frame
scene.frame_set(1)
bpy.context.view_layer.update()

parts=[]
for o in asset.objects:
    if o.type!='MESH':continue
    o.data.calc_loop_triangles()
    parts.append({'name':o.name,'triangles':len(o.data.loop_triangles),'parent':o.parent.name if o.parent else None})
triangles=sum(p['triangles'] for p in parts)
assert triangles<5000,triangles

# Export the real rig + baked sampled animation. The .blend keeps procedural
# drivers; the GLB carries skin joints and animation without needing Python.
bpy.ops.object.select_all(action='DESELECT')
for o in asset.objects:o.select_set(True)
bpy.context.view_layer.objects.active=root
bpy.ops.export_scene.gltf(filepath=str(OUT/'hydraulics-joystick-v1.glb'),export_format='GLB',use_selection=True,
    export_animations=True,export_extras=True,export_yup=True,export_force_sampling=True,export_bake_animation=True,
    export_animation_mode='SCENE',export_frame_range=True)


def studio_obj(o):
    for col in list(o.users_collection):col.objects.unlink(o)
    studio.objects.link(o)
    return o


def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()


bpy.ops.object.camera_add(location=(.19,-.29,.22))
cam=studio_obj(bpy.context.object)
cam.name='REVIEW | driver three-quarter'
cam.data.type='ORTHO'
cam.data.ortho_scale=.225
aim(cam,(0,0,.038))
scene.camera=cam
for name,loc,power,size in [('Key',(-.17,-.20,.30),6,.25),('Fill',(.24,-.04,.23),3,.22),('Rim',(-.05,.23,.26),5,.2)]:
    bpy.ops.object.light_add(type='AREA',location=loc)
    o=studio_obj(bpy.context.object)
    o.name='REVIEW | '+name
    o.data.energy=power
    o.data.shape='DISK'
    o.data.size=size
    aim(o,(0,0,.04))
scene.world=bpy.data.worlds.new('ReviewWorld')
scene.world.color=(.20,.20,.20)
scene.render.engine='CYCLES'
scene.cycles.device='CPU'
scene.cycles.samples=40
scene.cycles.use_denoising=True
scene.render.resolution_x=1200
scene.render.resolution_y=1000
scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.render.film_transparent=True
scene.view_settings.view_transform='AgX'
studio.hide_viewport=True
bpy.ops.object.select_all(action='DESELECT')
root.select_set(True)
bpy.context.view_layer.objects.active=root
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            s=area.spaces.active
            s.region_3d.view_location=(0,0,.038)
            s.region_3d.view_distance=.28
            s.region_3d.view_rotation=cam.rotation_euler.to_quaternion()
            s.clip_start=.0005
            s.shading.type='MATERIAL'
            s.overlay.show_extras=False
scene.frame_set(1)
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(BLEND),compress=True)
manifest={'task':31,'status':'review; not committed; no game integration','units':'metres','triangles':triangles,'parts':parts,
          'base_size_m':[.150,.110,.0145],'pivot_m':[PX,PY,PZ],'ball_diameter_m':.045,'grip_local_m':[0,0,GRIP],
          'detent_angle_deg':22,'positions':9,'mission_positions':5,'poses':poses,
          'bellows':{'method':'continuous axial ring skinning','rings':count,'sides':SIDES,'profile_z_radius':PROFILE,
                     'base_z_m':BOTTOM,'neck_z_m':TOP,'weight':'t*t*(3-2*t)','t':'(z-base_z)/(neck_z-base_z)',
                     'ring_center_delta':'weight * (swingQuaternion * (0,0,neck_z-pivot_z) - (0,0,neck_z-pivot_z))',
                     'ring_rotation':'swingQuaternion with angle multiplied by weight','fixed_base':True,'attached_neck':True,
                     'runtime_note':'evaluate ring transforms for actual visual lever pose, including transitions and slack; CPU ring deformation or skin palette; no cloth simulation'},
          'atlas':'hydraulics-labels.png','button_travel_m':.006}
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf8')
for name,pose in [('neutral','NEUTRAL'),('front-right','FRONT RIGHT'),('left','LEFT')]:
    scene.frame_set(poses[pose])
    scene.render.filepath=str(REVIEW/f'hydraulics-{name}.png')
    bpy.ops.render.render(write_still=True)
scene.frame_set(1)
print(f'CREATED {BLEND}; {triangles} triangles, {count} continuously driven bellows rings',flush=True)
