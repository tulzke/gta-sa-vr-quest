"""Build the task 56 rigid lever source, GLB and review render in Blender 4.5.

blender --background --factory-startup --python-exit-code 1
  --python tools/build_vehicle_lift_lever.py
"""
import json
import math
import struct
from collections import Counter
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/vehicle-lift-lever'
PREVIEW = ROOT / 'build/vehicle-lift-lever'
OUT.mkdir(parents=True, exist_ok=True)
PREVIEW.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1
scene.render.fps = 25
scene.frame_start, scene.frame_end = 1, 51


def material(name, color, metallic=0, roughness=.5):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1)
    bsdf.inputs['Metallic'].default_value = metallic
    bsdf.inputs['Roughness'].default_value = roughness
    return mat


paint = material('Paint | graphite steel', (.20, .235, .25), .55, .42)
cheek_paint = material('Paint | guide cheeks', (.285, .32, .33), .55, .4)
steel = material('Metal | satin shaft', (.48, .53, .57), .85, .3)
fastener = material('Metal | dark fasteners', (.09, .105, .115), .8, .34)
rubber = material('Rubber | grip and seal', (.023, .028, .03), 0, .82)
red_grip = material('Rubber | red grip', (.55, .012, .008), 0, .82)
index_paint = material('Paint | neutral index', (.78, .64, .25), .2, .48)
asset = bpy.data.collections.new('ASSET | lift lever')
scene.collection.children.link(asset)
studio = bpy.data.collections.new('PREVIEW ONLY | cameras lights ground')
scene.collection.children.link(studio)


def empty(name, parent=None, location=(0, 0, 0)):
    obj = bpy.data.objects.new(name, None)
    asset.objects.link(obj)
    obj.empty_display_type = 'PLAIN_AXES'
    obj.empty_display_size = .025
    obj.parent, obj.location = parent, location
    return obj


root = empty('LiftRoot')
root['units'] = 'metres; Blender X right, Y forward, Z up'
root['task'] = 56
root['version'] = 'horizontal T grip / semicircular single-axis guides'
fixed = empty('LiftFixed', root)
pivot = empty('LiftPivot', root, (0, 0, .05))
pivot.rotation_mode = 'XYZ'
pivot.lock_rotation = (False, True, True)
pivot['axis'] = 'local +X; positive angle pulls grip toward driver (-Y)'
pivot['min_angle_deg'], pivot['max_angle_deg'] = -25., 25.
pivot['neutral_angle_deg'] = 0.
grip_anchor = empty('GripAnchor', pivot, (0, 0, .18))
grip_anchor['capsule_axis'] = 'local X'
grip_anchor['capsule_length_m'] = .11
grip_anchor['capsule_diameter_m'] = .032


def mesh(name, verts, faces, mat, parent=fixed, smooth=False):
    data = bpy.data.meshes.new(name)
    data.from_pydata(verts, [], faces)
    data.update()
    bm = bmesh.new()
    bm.from_mesh(data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(data)
    bm.free()
    obj = bpy.data.objects.new(name, data)
    asset.objects.link(obj)
    obj.parent = parent
    data.materials.append(mat)
    for poly in data.polygons:
        poly.use_smooth = smooth and len(poly.vertices) == 4
    return obj


def lathe(name, levels, mat, parent=fixed, sides=12, transform=lambda p: p):
    # Each level: axial coordinate, radius X, radius Y; default axis is Z.
    verts = [transform((rx * math.cos(i * math.tau / sides),
                        ry * math.sin(i * math.tau / sides), z))
             for z, rx, ry in levels for i in range(sides)]
    faces = []
    for j in range(len(levels) - 1):
        for i in range(sides):
            faces.append((j*sides+i, j*sides+(i+1)%sides,
                          (j+1)*sides+(i+1)%sides, (j+1)*sides+i))
    faces.extend((tuple(reversed(range(sides))),
                  tuple(range((len(levels)-1)*sides, len(levels)*sides))))
    return mesh(name, verts, faces, mat, parent, True)


# Three perimeter loops make an actual chamfer without runtime modifiers.
outline = [(-.045, -.08), (.045, -.08), (.055, -.07), (.055, .07),
           (.045, .08), (-.045, .08), (-.055, .07), (-.055, -.07)]
verts = [(x*sx, y*sy, z) for z,sx,sy in [(0,1,1),(.027,1,1),(.032,.94,.96)]
         for x,y in outline]
faces = [tuple(reversed(range(8))), tuple(range(16,24))]
faces += [(j*8+i,j*8+(i+1)%8,(j+1)*8+(i+1)%8,(j+1)*8+i)
          for j in range(2) for i in range(8)]
mesh('MountingPlate', verts, faces, paint)
for i,(x,y) in enumerate(((-.039,-.063),(.039,-.063),(-.039,.063),(.039,.063))):
    lathe(f'MountScrew_{i+1}',[(.0318,.0045,.0045),(.0338,.0045,.0045)],
          fastener,sides=6,transform=lambda p,x=x,y=y:(p[0]+x,p[1]+y,p[2]))


def guide_cheek(name, center_x):
    """Extrude a tiled planar section, omitting the curved through-slot cells.

    No booleans, overlapping internal faces, floating decals or slot textures.
    """
    angles = [-90,-75,-60,-45,-30,-25,-20,-15,-10,-5,0,5,10,15,20,25,30,45,60,75,90]
    def polar(r, a):
        t = math.radians(a)
        return (r*math.sin(t), .05+r*math.cos(t))
    cells = []
    for a,b in zip(angles,angles[1:]):
        cells.append([(0,.05),polar(.053,a),polar(.053,b)])
        if not (-25 <= a and b <= 25):
            cells.append([polar(.053,a),polar(.059,a),polar(.059,b),polar(.053,b)])
        cells.append([polar(.059,a),polar(.064,a),polar(.064,b),polar(.059,b)])
    # Match every half-disc edge on its straight bottom to the skirt.
    cuts = [-.064,-.059,-.053,0,.053,.059,.064]
    for a,b in zip(cuts,cuts[1:]):
        cells.append([(a,.032),(b,.032),(b,.05),(a,.05)])
    points, lookup = [], {}
    def index(p):
        key = tuple(round(v,9) for v in p)
        if key not in lookup:
            lookup[key] = len(points)
            points.append(key)
        return lookup[key]
    indexed = [[index(p) for p in cell] for cell in cells]
    edges = Counter(tuple(sorted((a,b))) for cell in indexed
                    for a,b in zip(cell,cell[1:]+cell[:1]))
    n = len(points)
    vs = [(x,y,z) for x in (center_x-.003,center_x+.003) for y,z in points]
    fs = []
    for cell in indexed:
        fs.extend((tuple(reversed(cell)),tuple(i+n for i in cell)))
    for (a,b), count in edges.items():
        if count == 1:
            fs.append((a,b,b+n,a+n))
        else:
            assert count == 2
    obj = mesh(name, vs, fs, cheek_paint)
    obj['slot_radius_m'] = .056
    obj['slot_width_m'] = .006
    obj['travel_deg'] = 50.
    return obj


guide_cheek('GuideCheek_L',-.021)
guide_cheek('GuideCheek_R', .021)
lathe('RecessedRubberSeal',[(.032,.014,.026),(.037,.012,.023),
      (.042,.014,.025),(.047,.011,.020),(.052,.010,.018)],rubber,sides=8)
lathe('FixedPivotAxle',[(-.027,.006,.006),(.027,.006,.006)],
      steel,transform=lambda p:(p[2],p[1],.05+p[0]))
for side,label in [(-1,'L'),(1,'R')]:
    lathe(f'PivotCap_{label}',[(.024*side,.009,.009),(.028*side,.008,.008)],
          fastener,sides=10,transform=lambda p:(p[2],p[1],.05+p[0]))
lathe('LeverShaft',[(0,.006,.006),(.18,.006,.006)],steel,parent=pivot)
lathe('HorizontalGrip',[(-.055,.001,.001),(-.052,.010,.010),(-.044,.016,.016),
      (.044,.016,.016),(.052,.010,.010),(.055,.001,.001)],red_grip,parent=pivot,
      transform=lambda p:(p[2],p[1],.18+p[0]))
lathe('GuideFollowerPin',[(-.026,.0023,.0023),(.026,.0023,.0023)],
      steel,parent=pivot,sides=8,transform=lambda p:(p[2],p[1],.056+p[0]))
for side,label in [(-1,'L'),(1,'R')]:
    lathe(f'FollowerCap_{label}',[(.0245*side,.0044,.0044),(.027*side,.0044,.0044)],
          fastener,parent=pivot,sides=8,transform=lambda p:(p[2],p[1],.056+p[0]))

# A small neutral mark on each outer cheek above the through-slot.
for side,label in [(-1,'L'),(1,'R')]:
    x = side*.02405
    mesh(f'NeutralIndex_{label}',[(x,-.0008,.110),(x,.0008,.110),
         (x,.0008,.1135),(x,-.0008,.1135)],[(0,1,2,3)],index_paint)
index_paint.use_backface_culling = False

# One rigid animation channel, with neutral as the saved pose.
for frame, angle in [(1,-25),(26,0),(51,25)]:
    pivot.rotation_euler.x = math.radians(angle)
    pivot.keyframe_insert(data_path='rotation_euler',index=0,frame=frame,group='Single axis')
action = pivot.animation_data.action
action.name = 'LiftTravel'
for curve in action.fcurves:
    for key in curve.keyframe_points:
        key.interpolation = 'LINEAR'
limit = pivot.constraints.new('LIMIT_ROTATION')
limit.name = 'Single axis | -25 to +25 degrees'
limit.owner_space = 'LOCAL'
limit.use_limit_x = limit.use_limit_y = limit.use_limit_z = True
limit.min_x,limit.max_x = math.radians(-25),math.radians(25)
limit.min_y = limit.max_y = limit.min_z = limit.max_z = 0
scene.frame_set(26)
bpy.context.view_layer.update()

# Validate source geometry and the moving hierarchy before writing outputs.
triangles = 0
parts = []
for obj in asset.objects:
    if obj.type != 'MESH':
        continue
    obj.data.calc_loop_triangles()
    count = len(obj.data.loop_triangles)
    triangles += count
    parts.append({'name':obj.name,'triangles':count,'parent':obj.parent.name})
    if not obj.name.startswith('NeutralIndex'):
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        assert all(e.is_manifold for e in bm.edges), f'Nonmanifold: {obj.name}'
        bm.free()
assert triangles <= 1500, triangles
for frame, angle in [(1,-25),(26,0),(51,25)]:
    scene.frame_set(frame)
    bpy.context.view_layer.update()
    actual = grip_anchor.matrix_world.translation
    expected = Vector((0,-.18*math.sin(math.radians(angle)),.05+.18*math.cos(math.radians(angle))))
    assert (actual-expected).length < 1e-6,(frame,actual,expected)
scene.frame_set(26)
bpy.ops.object.select_all(action='DESELECT')
for obj in asset.objects:
    obj.select_set(True)
bpy.context.view_layer.objects.active = pivot
bpy.ops.export_scene.gltf(filepath=str(OUT/'lift-lever.glb'),export_format='GLB',
    use_selection=True,export_animations=True,export_extras=True,
    export_materials='EXPORT',export_yup=True,export_force_sampling=True)

# Compact runtime mesh, same approved source and colors. Moving vertices are
# pivot-local; fixed vertices are plate-local. No Blender/glTF parsing at runtime.
runtime_vertices, runtime_indices, runtime_lookup = [], [], {}
for obj in asset.objects:
    if obj.type != 'MESH':
        continue
    moving = obj.parent == pivot
    normal_matrix = obj.matrix_world.to_3x3().inverted_safe().transposed()
    color = obj.data.materials[0].diffuse_color[:3]
    obj.data.calc_loop_triangles()
    for tri in obj.data.loop_triangles:
        for loop_id in tri.loops:
            loop = obj.data.loops[loop_id]
            p = obj.matrix_world @ obj.data.vertices[loop.vertex_index].co
            if moving:
                p -= pivot.matrix_world.translation
            normal = (normal_matrix @ obj.data.corner_normals[loop_id].vector).normalized()
            packed = struct.pack('<10f',*p,*normal,*color,float(moving))
            if packed not in runtime_lookup:
                runtime_lookup[packed] = len(runtime_vertices)
                runtime_vertices.append(packed)
            runtime_indices.append(runtime_lookup[packed])
assert len(runtime_vertices)<65536 and len(runtime_indices)==triangles*3
(OUT/'lift-lever.mesh').write_bytes(struct.pack('<4sIII',b'SAVL',1,len(runtime_vertices),len(runtime_indices))+
    b''.join(runtime_vertices)+struct.pack(f'<{len(runtime_indices)}H',*runtime_indices))

# Studio belongs only to the editable .blend and is excluded from the GLB.
def studio_object(obj):
    for collection in list(obj.users_collection):
        collection.objects.unlink(obj)
    studio.objects.link(obj)
    return obj


ground_mat = material('PREVIEW | ground',(.075,.085,.095),0,.85)
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.001))
ground = studio_object(bpy.context.object)
ground.name='PREVIEW | ground'
ground.data.materials.append(ground_mat)


def aim(obj, target):
    obj.rotation_euler = (Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()


bpy.ops.object.camera_add(location=(-.38,.52,.33))
camera = studio_object(bpy.context.object)
camera.name='PREVIEW | camera'
camera.data.type='ORTHO'
camera.data.ortho_scale=.38
camera.data.lens=55
aim(camera,(0,0,.12))
scene.camera=camera
for name,loc,power,size in [('Key',(-.35,.12,.6),3,.4),
                           ('Fill',(.4,.15,.3),1.4,.35),
                           ('Rim',(.1,-.4,.45),2.5,.3)]:
    bpy.ops.object.light_add(type='AREA',location=loc)
    light=studio_object(bpy.context.object)
    light.name='PREVIEW | '+name
    light.data.energy=power
    light.data.shape='DISK'
    light.data.size=size
    aim(light,(0,0,.12))
scene.world.color=(.15,.15,.15)
scene.render.engine='CYCLES'
scene.cycles.device='CPU'
scene.cycles.samples=32
scene.cycles.use_denoising=True
scene.render.resolution_x=1000
scene.render.resolution_y=900
scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.view_settings.view_transform='AgX'
scene.render.filepath=str(PREVIEW/'lift-lever-neutral.png')
studio.hide_viewport=True
bpy.ops.object.select_all(action='DESELECT')
pivot.select_set(True)
bpy.context.view_layer.objects.active=pivot
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            region=area.spaces.active.region_3d
            region.view_location=(0,0,.12)
            region.view_distance=.45
            region.view_rotation=camera.rotation_euler.to_quaternion()
            area.spaces.active.clip_start=.001
            area.spaces.active.clip_end=50
            area.spaces.active.shading.color_type='MATERIAL'
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'lift-lever.blend'),compress=True)
report={'triangles':triangles,'parts':parts,'units':'metres',
        'blender_axes':{'right':'+X','forward':'+Y','up':'+Z'},
        'pivot_blender_m':[0,0,.05],'grip_anchor_from_pivot_m':[0,0,.18],
        'rotation_axis_blender':'+X','rotation_axis_gltf':'+X',
        'travel_deg':[-25,25],'neutral_frame':26,'animation':'LiftTravel',
        'verified':'manifold solids; grip anchor at -25/0/+25 degrees'}
(OUT/'manifest.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(f'PASS source: {triangles} triangles, rigid pivot and all solid meshes manifold',flush=True)
bpy.ops.render.render(write_still=True)
print(f'OUTPUT {OUT}',flush=True)
