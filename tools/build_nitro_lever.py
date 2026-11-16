"""Build the editable task 32 model; no game integration or installation.

Blender 4.5 --background --factory-startup --python-exit-code 1
  --python tools/build_nitro_lever.py [-- --overwrite]
Existing artist edits are protected unless --overwrite is explicitly passed.
"""
import json
import math
import sys
from collections import Counter
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/nitro-lever'
REVIEW = ROOT / 'build/nitro-lever-review'
BLEND = OUT / 'nitro-lever-v1.blend'
if BLEND.exists() and '--overwrite' not in sys.argv:
    raise RuntimeError('Source exists; preserve manual edits or explicitly pass --overwrite')
OUT.mkdir(parents=True, exist_ok=True)
REVIEW.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.length_unit = 'MILLIMETERS'
scene.unit_settings.scale_length = 1
scene.render.fps = 24
scene.frame_start, scene.frame_end = 1, 49
asset = bpy.data.collections.new('NITRO | editable asset')
studio = bpy.data.collections.new('PREVIEW ONLY | studio')
scene.collection.children.link(asset)
scene.collection.children.link(studio)


def empty(name, parent=None, location=(0, 0, 0)):
    obj = bpy.data.objects.new(name, None)
    asset.objects.link(obj)
    obj.parent, obj.location = parent, location
    obj.empty_display_size = .012
    return obj


root = empty('NitroRoot')
root['task'] = 32
root['approval_status'] = 'REVIEW REQUIRED | do not commit until user authorizes'
root['axes'] = 'Blender: X right, Y forward, Z up; metres'
root['preview_uninstalled'] = 0.
root['preview_opacity'] = .45
root['preview_yellow_mix'] = .20
root.id_properties_ui('preview_uninstalled').update(min=0., max=1., description='0: installed appearance; 1: uninstalled yellow/translucent debug preview')
root.id_properties_ui('preview_opacity').update(min=.05, max=1., description='Debug preview opacity only; proposed initial value')
root.id_properties_ui('preview_yellow_mix').update(min=0., max=1., description='Yellow tint fraction over original material, approved 0.20')
root['reserve_percent'] = 60
root['gauge_note'] = 'Review pose: 60%, six whole lit segments. Runtime changes whole-segment tint and digit UVs.'
fixed = empty('NitroFixed', root)
pivot = empty('NitroPivot', root, (0, .018, .048))
pivot.rotation_mode = 'XYZ'
pivot.lock_rotation = (False, True, True)
pivot['axis'] = 'X; negative rotation pushes forward (+Y) = ON'
pivot['off_angle_deg'], pivot['on_angle_deg'] = 22., -22.
grip_anchor = empty('GripAnchor', pivot, (0, 0, .088))
grip_anchor['capsule_length_m'] = .110
grip_anchor['capsule_diameter_m'] = .028
grip_anchor['capsule_axis'] = 'local X'
gauge = empty('NitroGauge', fixed)
gauge['segment_count'] = 10
gauge['quantization'] = 'ceil(percent/10); zero percent = zero lit; never partial fill'


# One self-contained monochrome RGBA atlas. Letters/digits remain editable UV decals.
FONT = {
    'N':['10001','11001','11001','10101','10011','10011','10001'],
    'I':['11111','00100','00100','00100','00100','00100','11111'],
    'T':['11111','00100','00100','00100','00100','00100','00100'],
    'R':['11110','10001','10001','11110','10100','10010','10001'],
    'O':['01110','10001','10001','10001','10001','10001','01110'],
    'F':['11111','10000','10000','11110','10000','10000','10000'],
    '0':['01110','10001','10011','10101','11001','10001','01110'],
    '1':['00100','01100','00100','00100','00100','00100','01110'],
    '2':['01110','10001','00001','00010','00100','01000','11111'],
    '3':['11110','00001','00001','01110','00001','00001','11110'],
    '4':['00010','00110','01010','10010','11111','00010','00010'],
    '5':['11111','10000','10000','11110','00001','00001','11110'],
    '6':['01110','10000','10000','11110','10001','10001','01110'],
    '7':['11111','00001','00010','00100','01000','01000','01000'],
    '8':['01110','10001','10001','01110','10001','10001','01110'],
    '9':['01110','10001','10001','01111','00001','00001','01110'],
    '%':['11001','11010','00100','01000','10110','00110','00000'],
    '^':['00100','01110','11111','00100','00100','00100','00100'],
}
W, H, CELL = 1024, 512, 128
pixels = [0.] * (W*H*4)
atlas_rects = {}


def fill(x0, y0, x1, y1):
    for y in range(y0, y1):
        for x in range(x0, x1):
            n = (y*W+x)*4
            pixels[n:n+4] = [1., 1., 1., 1.]


labels = ['N2O', 'NITRO', 'ON', 'OFF', *list('0123456789'), '%', '^', 'SEGMENT', 'BLANK']
for index, label in enumerate(labels):
    x, y = (index % 8)*CELL, (index//8)*CELL
    if label == 'SEGMENT':
        fill(x+8, y+8, x+120, y+120)
        atlas_rects[label] = ((x+16)/W, (y+16)/H, (x+112)/W, (y+112)/H)
    elif label == 'BLANK':
        atlas_rects[label] = ((x+16)/W, (y+16)/H, (x+112)/W, (y+112)/H)
    else:
        step = min(12, 112//(6*len(label)-1))
        width, height = (6*len(label)-1)*step, 7*step
        ox, oy = x+(CELL-width)//2, y+(CELL-height)//2
        for letter, char in enumerate(label):
            for row, pattern in enumerate(FONT[char]):
                for col, ink in enumerate(pattern):
                    if ink == '1':
                        px, py = ox+(letter*6+col)*step, oy+(6-row)*step
                        fill(px, py, px+step, py+step)
        atlas_rects[label] = ((ox-2)/W, (oy-2)/H, (ox+width+2)/W, (oy+height+2)/H)
image = bpy.data.images.new('Nitro monochrome atlas | shared', width=W, height=H, alpha=True)
image.pixels.foreach_set(pixels)
image.filepath_raw = str(OUT/'nitro-atlas.png')
image.file_format = 'PNG'
image.save()
image.pack()


def driver(socket, component, expression):
    curve = socket.driver_add('default_value', component) if component is not None else socket.driver_add('default_value')
    d = curve.driver
    for name, key in [('ghost', 'preview_uninstalled'), ('opacity', 'preview_opacity'), ('yellow', 'preview_yellow_mix')]:
        v = d.variables.new()
        v.name, v.type = name, 'SINGLE_PROP'
        v.targets[0].id = root
        v.targets[0].data_path = '["'+key+'"]'
    d.expression = expression


def material(name, color, metallic=0., roughness=.5, textured=False, emission=0.):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1.)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Metallic'].default_value = metallic
    bsdf.inputs['Roughness'].default_value = roughness
    for i in range(3):
        driver(bsdf.inputs['Base Color'], i, f'{color[i]}*(1-yellow*ghost)+{(1.,.85,.12)[i]}*yellow*ghost')
        curve = mat.driver_add('diffuse_color', i)
        d = curve.driver
        for name,key in [('ghost','preview_uninstalled'),('yellow','preview_yellow_mix')]:
            v=d.variables.new();v.name=name;v.type='SINGLE_PROP';v.targets[0].id=root;v.targets[0].data_path='["'+key+'"]'
        d.expression=f'{color[i]}*(1-yellow*ghost)+{(1.,.85,.12)[i]}*yellow*ghost'
    if textured:
        tex = mat.node_tree.nodes.new('ShaderNodeTexImage')
        tex.image, tex.interpolation = image, 'Linear'
        opacity = mat.node_tree.nodes.new('ShaderNodeMath')
        opacity.operation = 'MULTIPLY'
        mat.node_tree.links.new(tex.outputs['Alpha'], opacity.inputs[0])
        driver(opacity.inputs[1], None, '1-ghost+ghost*opacity')
        mat.node_tree.links.new(opacity.outputs[0], bsdf.inputs['Alpha'])
    else:
        driver(bsdf.inputs['Alpha'], None, '1-ghost+ghost*opacity')
    mat.surface_render_method = 'DITHERED'
    if emission:
        bsdf.inputs['Emission Color'].default_value=(*color,1.)
        bsdf.inputs['Emission Strength'].default_value=emission
    return mat


paint = material('Housing | graphite', (.075,.090,.105), .35, .5)
cheek_paint = material('Guides | satin graphite', (.20,.235,.26), .55, .4)
metal = material('Shaft and axle | brushed alloy', (.48,.54,.59), .8, .3)
fastener = material('Fasteners | dark steel', (.10,.12,.14), .7, .4)
rubber = material('Grip | black rubber', (.021,.026,.031), 0., .8)
blue = material('Grip | blue caps', (.018,.19,.52), .05, .5)
white = material('Labels | shared monochrome atlas', (.83,.88,.93), textured=True)
segment_on = material('Gauge ON | shared atlas + blue tint', (.015,.36,.95), textured=True, emission=.35)
segment_off = material('Gauge OFF | shared atlas + dark tint', (.045,.060,.075), textured=True)
amber = material('Flow lamp | amber', (1.,.42,.025), emission=.5)
lamp_off = material('Flow lamp | OFF', (.045,.038,.026))


def mesh(name, verts, faces, mat, parent=fixed, smooth=False, solid=True):
    data=bpy.data.meshes.new(name)
    data.from_pydata(verts, [], faces); data.update()
    bm=bmesh.new();bm.from_mesh(data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(data);bm.free()
    obj=bpy.data.objects.new(name,data);asset.objects.link(obj);obj.parent=parent
    data.materials.append(mat)
    obj['closed_solid']=solid
    for poly in data.polygons: poly.use_smooth=smooth and len(poly.vertices)==4
    return obj


def lathe(name, levels, mat, parent=fixed, sides=12, transform=lambda p:p):
    verts=[transform((r*math.cos(i*math.tau/sides),r*math.sin(i*math.tau/sides),z)) for z,r in levels for i in range(sides)]
    faces=[(j*sides+i,j*sides+(i+1)%sides,(j+1)*sides+(i+1)%sides,(j+1)*sides+i) for j in range(len(levels)-1) for i in range(sides)]
    faces.extend((tuple(reversed(range(sides))),tuple(range((len(levels)-1)*sides,len(levels)*sides))))
    return mesh(name,verts,faces,mat,parent,True)


def decal(name,label,points,mat=white,parent=fixed):
    obj=mesh(name,points,[(0,1,2,3)],mat,parent,solid=False)
    uv=obj.data.uv_layers.new(name='AtlasUV')
    u0,v0,u1,v1=atlas_rects[label]
    coords=[(u0,v0),(u1,v0),(u1,v1),(u0,v1)]
    for loop in obj.data.loops: uv.data[loop.index].uv=coords[loop.vertex_index]
    obj['atlas_label']=label
    return obj


# Closed wedge housing, with two planar top faces and clipped corners.
outline=[(-.054,-.090),(.054,-.090),(.060,-.084),(.060,-.040),(.060,.084),(.054,.090),(-.054,.090),(-.060,.084),(-.060,-.040),(-.060,-.084)]
panel_z=lambda y: .016+(y+.09)*.32 if y<-.04 else .032
n=len(outline)
verts=[(x,y,0.) for x,y in outline]+[(x,y,panel_z(y)) for x,y in outline]
faces=[tuple(reversed(range(n))),tuple(n+i for i in [0,1,2,3,8,9]),tuple(n+i for i in [3,4,5,6,7,8])]
faces.extend((i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n))
mesh('MountingHousing',verts,faces,paint)
for i,(x,y) in enumerate([(-.049,-.031),(.049,-.031),(-.049,.074),(.049,.074)]):
    lathe(f'MountScrew_{i+1}',[(.0319,.0034),(.0331,.0034),(.0334,.0027)],fastener,sides=8,transform=lambda p,x=x,y=y:(x+p[0],y+p[1],p[2]))
    mesh(f'ScrewSlot_{i+1}',[(x-.0018,y-.00035,.03345),(x+.0018,y-.00035,.03345),(x+.0018,y+.00035,.03345),(x-.0018,y+.00035,.03345)],[(0,1,2,3)],rubber,solid=False)


def cheek(name,center_x):
    angles=[-33,-27,-22,-15,-8,0,8,15,22,27,33]
    def polar(r,a):
        t=math.radians(a)
        return (.018+r*math.sin(t),.048+r*math.cos(t))
    cells=[]
    for a,b in zip(angles,angles[1:]):
        cells.append([(.018,.048),polar(.028,a),polar(.028,b)])
        if not (-27<=a and b<=27): cells.append([polar(.028,a),polar(.035,a),polar(.035,b),polar(.028,b)])
        cells.append([polar(.035,a),polar(.044,a),polar(.044,b),polar(.035,b)])
    # Compact skirt follows the approved arched cheeks, not a full half-disc.
    cells.append([polar(.028,-33),(.018,.048),polar(.028,33),(.045,.032),(-.009,.032)])
    pts=[];lookup={}
    def idx(p):
        key=tuple(round(v,9) for v in p)
        if key not in lookup:lookup[key]=len(pts);pts.append(key)
        return lookup[key]
    indexed=[[idx(p) for p in cell]for cell in cells]
    edges=Counter(tuple(sorted((a,b)))for cell in indexed for a,b in zip(cell,cell[1:]+cell[:1]))
    vs=[(x,y,z)for x in (center_x-.003,center_x+.003)for y,z in pts]
    count=len(pts);fs=[]
    for cell in indexed:fs.extend((tuple(reversed(cell)),tuple(i+count for i in cell)))
    for (a,b),multiplicity in edges.items():
        if multiplicity==1:fs.append((a,b,b+count,a+count))
        else:assert multiplicity==2
    obj=mesh(name,vs,fs,cheek_paint)
    obj['slot_radius_m']=.0315;obj['slot_width_m']=.007;obj['stop_angle_deg']=27.
    return obj


cheek('GuideCheek_L',-.019)
cheek('GuideCheek_R',.019)
lathe('FixedPivotAxle',[(-.026,.0065),(.026,.0065)],metal,transform=lambda p:(p[2],.018+p[1],.048+p[0]))
for side,label in [(-1,'L'),(1,'R')]:
    lathe('PivotCap_'+label,[(side*.024,.0075),(side*.027,.0065)],fastener,transform=lambda p:(p[2],.018+p[1],.048+p[0]))
lathe('LeverShaft',[(0.,.005),(.088,.005)],metal,pivot,sides=16)
lathe('GuideFollowerPin',[(-.025,.0024),(.025,.0024)],metal,pivot,transform=lambda p:(p[2],p[1],.0315+p[0]))
for side,label in [(-1,'L'),(1,'R')]:
    lathe('FollowerCap_'+label,[(side*.023,.0035),(side*.026,.0035)],fastener,pivot,transform=lambda p:(p[2],p[1],.0315+p[0]))
grip_levels=[(-.043,.014),(-.024,.0128),(0,.014),(.024,.0128),(.043,.014)]
def grip_transform(p):
    x=p[2]
    return (x,p[1],.088+.0015*(1-(x/.055)**2)+p[0])
lathe('HorizontalGrip',grip_levels,rubber,pivot,sides=16,transform=grip_transform)
for side,label in [(-1,'L'),(1,'R')]:
    levels=[(side*x,r)for x,r in [(.043,.014),(.048,.014),(.053,.012),(.055,.0075)]]
    lathe('GripBlueCap_'+label,levels,blue,pivot,sides=16,transform=grip_transform)
# Conform the thin label to the grip instead of floating a flat card in front.
label_vertices=[]
for row in range(5):
    z=.082+.013*row/4
    for col in range(3):
        x=-.020+.040*col/2
        r=.014-(.014-.0128)*abs(x)/.024
        center=.088+.0015*(1-(x/.055)**2)
        y=-math.sqrt(max(0.,r*r-(z-center)**2))-.00015
        label_vertices.append((x,y,z))
label_faces=[(row*3+col,row*3+col+1,(row+1)*3+col+1,(row+1)*3+col)for row in range(4)for col in range(2)]
label=mesh('GripLabel_N2O',label_vertices,label_faces,white,pivot,solid=False)
uv=label.data.uv_layers.new(name='AtlasUV')
u0,v0,u1,v1=atlas_rects['N2O']
for loop in label.data.loops:
    row,col=divmod(loop.vertex_index,3)
    uv.data[loop.index].uv=(u0+(u1-u0)*col/2,v0+(v1-v0)*row/4)
label['atlas_label']='N2O'


def panel_decal(name,label,x,y,width,height,mat=white,parent=fixed):
    y0,y1=y-height/2,y+height/2
    return decal(name,label,[(x-width/2,y0,panel_z(y0)+.00035),(x+width/2,y0,panel_z(y0)+.00035),(x+width/2,y1,panel_z(y1)+.00035),(x-width/2,y1,panel_z(y1)+.00035)],mat,parent)


panel_decal('BaseLabel_NITRO','NITRO',-.018,-.049,.044,.010)
for i,(digit,x)in enumerate([('BLANK',.019),('6',.026),('0',.033),('%',.041)]):
    obj=panel_decal(['ReserveHundreds','ReserveTens','ReserveUnits','ReservePercent'][i],digit,x,-.049,.006,.010,parent=gauge)
    obj['runtime_role']='reserve_digit';obj['digit_position']=i
for i in range(10):
    x=-.048+i*.0096+.00405
    y=-.074+.012*(x/.048)**2
    obj=panel_decal(f'ReserveSegment_{i+1:02d}','SEGMENT',x,y,.0081,.008,segment_on if i<6 else segment_off,parent=gauge)
    obj['runtime_role']='reserve_segment';obj['segment_index']=i;obj['partial_fill']=False
for name,label,y,width in [('StateLabel_OFF','OFF',-.002,.019),('StateLabel_ON','ON',.055,.014)]:
    decal(name,label,[(-.035-width/2,y-.004,.0324),(-.035+width/2,y-.004,.0324),(-.035+width/2,y+.004,.0324),(-.035-width/2,y+.004,.0324)])
decal('ForwardArrow','^',[(-.006,.065,.0324),(.006,.065,.0324),(.006,.077,.0324),(-.006,.077,.0324)])
lathe('FlowLampBezel',[(.0319,.005),(.034,.005)],fastener,transform=lambda p:(.040+p[0],.035+p[1],p[2]))
lamp_material=material('Flow lamp | driven review state',(.045,.038,.026))
lamp=lathe('FlowLampLens',[(.034,.0038),(.035,.0038)],lamp_material,transform=lambda p:(.040+p[0],.035+p[1],p[2]))
lamp['runtime_role']='flow_indicator';lamp['meaning']='actual flow, not merely remaining reserve'
lamp_bsdf=lamp_material.node_tree.nodes.get('Principled BSDF')
for i in range(3):
    for owner,path in [(lamp_bsdf.inputs['Base Color'],'default_value'),(lamp_material,'diffuse_color')]:
        curve=owner.driver_add(path,i);d=curve.driver
        v=d.variables.new();v.name='angle';v.type='SINGLE_PROP';v.targets[0].id=pivot;v.targets[0].data_path='rotation_euler[0]'
        d.expression=f'({(1.,.42,.025)[i]} if angle < -0.244346 else {(.045,.038,.026)[i]})*(1-yellow*ghost)+{(1.,.85,.12)[i]}*yellow*ghost'
lamp_bsdf.inputs['Emission Color'].default_value=(1.,.42,.025,1.)
curve=lamp_bsdf.inputs['Emission Strength'].driver_add('default_value')
v=curve.driver.variables.new();v.name='angle';v.type='SINGLE_PROP';v.targets[0].id=pivot;v.targets[0].data_path='rotation_euler[0]'
curve.driver.expression='.5 if angle < -0.244346 else 0.'


# A reversible demonstration, saved OFF. No stable neutral state.
for frame,angle in [(1,22),(25,-22),(49,22)]:
    pivot.rotation_euler.x=math.radians(angle)
    pivot.keyframe_insert(data_path='rotation_euler',index=0,frame=frame,group='Forward ON / backward OFF')
pivot.animation_data.action.name='NitroTravel'
for curve in pivot.animation_data.action.fcurves:
    for key in curve.keyframe_points:key.interpolation='LINEAR'
for name,frame in [('OFF | toward driver',1),('ON | forward',25),('OFF | return',49)]:scene.timeline_markers.new(name,frame=frame)
limit=pivot.constraints.new('LIMIT_ROTATION');limit.name='Single axis | OFF +22 / ON -22'
limit.owner_space='LOCAL';limit.use_limit_x=limit.use_limit_y=limit.use_limit_z=True
limit.min_x,limit.max_x=math.radians(-22),math.radians(22)
limit.min_y=limit.max_y=limit.min_z=limit.max_z=0.
scene.frame_set(1);bpy.context.view_layer.update()

# Validate actual source solids and motion, including physical slot clearance.
parts=[];triangles=0
for obj in asset.objects:
    if obj.type!='MESH':continue
    obj.data.calc_loop_triangles();count=len(obj.data.loop_triangles);triangles+=count
    parts.append({'name':obj.name,'parent':obj.parent.name,'triangles':count,'solid':obj['closed_solid']})
    if obj['closed_solid']:
        bm=bmesh.new();bm.from_mesh(obj.data)
        assert all(e.is_manifold for e in bm.edges),'Nonmanifold '+obj.name
        bm.free()
assert triangles<3000,triangles
for frame,angle in [(1,22),(13,0),(25,-22),(49,22)]:
    scene.frame_set(frame);bpy.context.view_layer.update()
    expected=Vector((0,.018-.088*math.sin(math.radians(angle)),.048+.088*math.cos(math.radians(angle))))
    assert (grip_anchor.matrix_world.translation-expected).length<1e-6
for name in ['GuideCheek_L','GuideCheek_R']:
    obj=bpy.data.objects[name]
    for a in [-22,-12,0,12,22]:
        t=math.radians(a);origin=Vector((-.1,.018+.0315*math.sin(t),.048+.0315*math.cos(t)))
        assert not obj.ray_cast(origin,Vector((1,0,0)))[0],(name,a,'slot blocked')
    for a in [-32,32]:
        t=math.radians(a);origin=Vector((-.1,.018+.0315*math.sin(t),.048+.0315*math.cos(t)))
        assert obj.ray_cast(origin,Vector((1,0,0)))[0],(name,a,'missing end stop')
scene.frame_set(1);bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT')
for obj in asset.objects:obj.select_set(True)
bpy.context.view_layer.objects.active=pivot
bpy.ops.export_scene.gltf(filepath=str(OUT/'nitro-lever-v1.glb'),export_format='GLB',use_selection=True,export_animations=True,export_extras=True,export_yup=True,export_force_sampling=True)


def studio_object(obj):
    for c in list(obj.users_collection):c.objects.unlink(obj)
    studio.objects.link(obj)
    return obj


def aim(obj,target):obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()


bpy.ops.object.camera_add(location=(.23,-.34,.24))
camera=studio_object(bpy.context.object);camera.name='REVIEW | driver three-quarter'
camera.data.type='ORTHO';camera.data.ortho_scale=.285;aim(camera,(0,-.005,.067));scene.camera=camera
for name,loc,power,size in [('Key',(-.25,-.20,.45),7,.35),('Fill',(.35,-.10,.3),3,.3),('Rim',(.1,.35,.35),5,.25)]:
    bpy.ops.object.light_add(type='AREA',location=loc)
    obj=studio_object(bpy.context.object);obj.name='REVIEW | '+name;obj.data.energy=power;obj.data.shape='DISK';obj.data.size=size;aim(obj,(0,0,.06))
scene.world.color=(.18,.18,.18)
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=24;scene.cycles.use_denoising=True
scene.render.resolution_x=1100;scene.render.resolution_y=950;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.film_transparent=True
scene.view_settings.view_transform='AgX'
studio.hide_viewport=True
bpy.ops.object.select_all(action='DESELECT');pivot.select_set(True);bpy.context.view_layer.objects.active=pivot
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            space=area.spaces.active;region=space.region_3d
            region.view_location=(0,-.005,.067);region.view_distance=.34;region.view_rotation=camera.rotation_euler.to_quaternion()
            space.clip_start=.001;space.clip_end=20;space.shading.type='MATERIAL';space.overlay.show_extras=False
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(BLEND),compress=True)
(OUT/'manifest.json').write_text(json.dumps({'task':32,'approval':'pending','triangles':triangles,'parts':parts,'blender_axes':['right +X','forward +Y','up +Z'],'pivot_m':[0,.018,.048],'grip_local_m':[0,0,.088],'off_angle_deg':22,'on_angle_deg':-22,'frames':{'OFF':1,'ON':25,'OFF_RETURN':49},'atlas':'nitro-atlas.png','preview_uninstalled':{'yellow_mix':.2,'opacity':.45},'validation':'manifold solids; correct rigid pivot; through slots clear; stops present; no game integration'},indent=2)+'\n',encoding='utf-8')
for name,frame in [('off',1),('on',25)]:
    scene.frame_set(frame)
    scene.render.filepath=str(REVIEW/f'nitro-lever-{name}.png')
    bpy.ops.render.render(write_still=True)
scene.frame_set(1)
root['preview_uninstalled']=1.;bpy.context.view_layer.update()
scene.render.filepath=str(REVIEW/'nitro-lever-uninstalled.png');bpy.ops.render.render(write_still=True)
print(f'PASS: {triangles} triangles; {len(parts)} mesh parts; source, GLB, packed atlas and review renders',flush=True)
print(f'BLEND: {BLEND}',flush=True)
