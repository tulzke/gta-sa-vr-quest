"""Build task 97's selected handheld radio; Blender 4.5, metres, no game install.

blender --background --factory-startup --threads 8 --python-exit-code 1
  --python tools/build_service_radio.py
"""
import json
import math
from pathlib import Path
import sys

import bpy
import bmesh
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/service-radio'
REVIEW = ROOT / 'build/service-radio'
OUT.mkdir(parents=True, exist_ok=True)
REVIEW.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1
scene.render.fps = 30
scene.frame_start, scene.frame_end = 1, 11
asset = bpy.data.collections.new('ASSET | handheld service radio')
studio = bpy.data.collections.new('PREVIEW ONLY | studio')
scene.collection.children.link(asset)
scene.collection.children.link(studio)


def material(name, color, roughness=.55, metallic=0):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1)
    bsdf.inputs['Roughness'].default_value = roughness
    bsdf.inputs['Metallic'].default_value = metallic
    return m


shell = material('Radio | graphite ABS', (.033, .039, .046), .58)
rubber = material('Radio | grip and antenna rubber', (.016, .019, .023), .78)
speaker = material('Radio | speaker inset', (.009, .012, .015), .82)
button_mat = material('Radio | light neutral PTT', (.47, .49, .51), .5)
metal = material('Radio | dark steel fasteners', (.09, .11, .13), .34, .8)
accent = material('RadioServiceAccent', (.018, .12, .64), .43)
accent['purpose'] = 'Single service tint: blue police / white medic / red fire'
COLORS = {'police': (.018, .12, .64), 'medic': (.78, .80, .79), 'fire': (.65, .022, .018)}


def move_collection(o, collection=asset):
    for c in list(o.users_collection):
        c.objects.unlink(o)
    collection.objects.link(o)
    return o


def empty(name, parent=None, location=(0, 0, 0)):
    o = bpy.data.objects.new(name, None)
    asset.objects.link(o)
    o.parent, o.location = parent, location
    o.empty_display_size = .012
    return o


root = empty('ServiceRadioRoot')
root['task'] = 97
root['status'] = 'model and hand pose review; do not commit until user approves'
root['axes'] = 'metres; +X right, -Y speaker/front, +Z antenna/up'
root['service_colors'] = json.dumps(COLORS)
body = empty('RadioBody', root)
cradle = empty('RadioCradle', root)


def finish(o, name, mat, parent, bevel=0, segments=3):
    move_collection(o)
    o.name = name
    o.parent = parent
    o.data.materials.append(mat)
    if bevel:
        mod = o.modifiers.new('Manufactured edge radii', 'BEVEL')
        mod.width, mod.segments = min(bevel, min(o.dimensions)*.45), segments
        mod.affect = 'EDGES'
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.modifier_apply(modifier=mod.name)
    for p in o.data.polygons:
        p.use_smooth = True
    normal = o.modifiers.new('Weighted hard-surface normals', 'WEIGHTED_NORMAL')
    normal.keep_sharp = True
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.modifier_apply(modifier=normal.name)
    return o


def box(name, loc, size, mat, parent=body, bevel=.001, segments=3):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o = bpy.context.object
    o.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish(o, name, mat, parent, bevel, segments)


def profile(name, outline, y0, y1, mat, parent=body, bevel=.0008):
    n = len(outline)
    verts = [(x, y, z) for y in (y0, y1) for x, z in outline]
    faces = [tuple(reversed(range(n))), tuple(range(n, n * 2))]
    faces += [(i, (i+1) % n, (i+1) % n+n, i+n) for i in range(n)]
    data = bpy.data.meshes.new(name)
    data.from_pydata(verts, [], faces)
    data.update()
    bm = bmesh.new()
    bm.from_mesh(data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(data)
    bm.free()
    o = bpy.data.objects.new(name, data)
    asset.objects.link(o)
    return finish(o, name, mat, parent, bevel)


def cylinder(name, loc, radius, depth, mat, parent=body, vertices=24, bevel=.0005):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=loc)
    return finish(bpy.context.object, name, mat, parent, bevel, 2)


# Narrow grip, broad shoulders: the selected first concept, not a plain box.
outline = [(-.0235, .003), (.0235, .003), (.024, .017), (.024, .072),
           (.028, .091), (.028, .130), (.022, .139), (-.022, .139),
           (-.028, .130), (-.028, .091), (-.024, .072), (-.024, .017)]
housing = profile('RadioShell', outline, -.016, .016, shell, bevel=.0018)
battery = box('BatteryBase', (0, 0, .009), (.048, .033, .018), rubber, bevel=.002)
box('BatteryLatch', (0, -.017, .019), (.014, .002, .003), shell, bevel=.0007)

# Continuous single-piece upper collar: color alone varies between services.
collar = [(-.030, .100), (-.030, .131), (-.023, .142), (-.016, .142),
          (-.016, .134), (.016, .134), (.016, .142), (.023, .142),
          (.030, .131), (.030, .100), (.0235, .100), (.0235, .123),
          (-.0235, .123), (-.0235, .100)]
guard = profile('ServiceAccentCollar', collar, -.018, .018, accent, bevel=.001)
guard['material_role'] = 'SERVICE_ACCENT; runtime tint applies to this component only'

# A solid dark speaker baffle with six genuine recessed slots and visible backing.
baffle = box('SpeakerBaffle', (0, -.0176, .091), (.048, .004, .058), rubber, bevel=.003)
for i in range(6):
    z = .069 + i * .0084
    cut = box('TEMP | vent cutter', (0, -.019, z), (.038, .010, .0034), speaker, bevel=.0015)
    bpy.context.view_layer.objects.active = baffle
    mod = baffle.modifiers.new('Recessed speaker vent', 'BOOLEAN')
    mod.operation, mod.solver, mod.object = 'DIFFERENCE', 'EXACT', cut
    bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.data.objects.remove(cut, do_unlink=True)
bm = bmesh.new()
bm.from_mesh(baffle.data)
bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=1e-7)
bmesh.ops.dissolve_degenerate(bm, edges=list(bm.edges), dist=1e-7)
bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
bm.to_mesh(baffle.data)
bm.free()
baffle.data.update()
normals = baffle.modifiers.new('Speaker normals after slot cuts', 'WEIGHTED_NORMAL')
normals.keep_sharp = True
bpy.context.view_layer.objects.active = baffle
bpy.ops.object.modifier_apply(modifier=normals.name)
box('SpeakerBacking', (0, -.0162, .091), (.043, .001, .053), speaker, bevel=.001)

# Shallow grip ribs are geometry; no new raster textures are needed.
for side in (-1, 1):
    for i in range(4):
        box(f'SideGrip_{side}_{i}', (side * .024, .003, .031+i*.010),
            (.0022, .021, .0025), rubber, bevel=.0008, segments=2)
for i in range(6):
    box(f'FrontGripFlute_{i}', ((i-2.5)*.006, -.0168, .041),
        (.0017, .001, .029), rubber, bevel=.0007, segments=2)
box('RearGripPad', (0, .0165, .062), (.039, .0018, .069), rubber, bevel=.004)

# The index finger presses local +X, into the left side of the radio.
button = empty('RadioPTT', body, (-.0308, .001, .094))
box('PTTBezel', (-.0295, .001, .094), (.004, .026, .041), rubber, bevel=.003)
box('PTTCap', (0, 0, 0), (.0042, .021, .035), button_mat, button, bevel=.0018)
for i in range(4):
    box(f'PTTTactileRib_{i}', (-.0022, 0, -.009+i*.005),
        (.0007, .013, .0012), button_mat, button, bevel=.0004, segments=2)
press = empty('IndexPressAnchor', button, (-.0024, .0015, .006))
press['press_direction'] = 'local +X'
press['travel_m'] = .0015
button.location.x = -.0308
button.keyframe_insert(data_path='location', frame=1)
button.location.x += .0015
button.keyframe_insert(data_path='location', frame=11)
button.animation_data.action.name = 'RadioPTTPress'
for curve in button.animation_data.action.fcurves:
    for k in curve.keyframe_points:
        k.interpolation = 'LINEAR'

cylinder('AntennaSocket', (.015, 0, .141), .007, .006, rubber)
cylinder('AntennaLowerRing', (.015, 0, .146), .0065, .004, rubber)
cylinder('RigidAntenna', (.015, 0, .179), .0055, .065, rubber, vertices=20, bevel=.002)
cylinder('TopKnobBase', (-.015, 0, .140), .006, .003, rubber)
cylinder('TopKnob', (-.015, 0, .146), .006, .010, rubber, vertices=16, bevel=.001)
for x, z in [(-.024, .130), (.024, .130), (-.021, .024), (.021, .024)]:
    o = cylinder('CaseFastener', (x, -.019, z), .0023, .001, metal, vertices=12, bevel=.0002)
    o.rotation_euler.x = math.pi / 2
    box('FastenerSlot', (x, -.0197, z), (.0026, .0004, .00055), speaker, bevel=.0001, segments=1)

# Passive holder: supporting cup + back plate, no cables or contact pins.
box('CradleFloor', (0, .001, -.003), (.066, .048, .009), rubber, cradle, bevel=.003)
box('CradleBack', (0, .022, .026), (.067, .006, .067), shell, cradle, bevel=.004)
for side in (-1, 1):
    box(f'CradleWing_{side}', (side*.031, 0, .014), (.007, .040, .035), shell, cradle, bevel=.003)
box('CradleFrontLip', (0, -.020, .008), (.058, .005, .023), shell, cradle, bevel=.003)
for x in (-.023, .023):
    o = cylinder('CradleMountScrew', (x, .018, .049), .0024, .001, metal, cradle, vertices=12)
    o.rotation_euler.x = math.pi/2

empty('DockAnchor', cradle, (0, 0, 0))
empty('RadioGrabAnchor', body, (0, 0, .064))
fit_right = Matrix.Translation((.112, .025, -.060)) @ Matrix.Rotation(math.pi, 4, 'Z')
fit_left = Matrix.Translation((-.112, .025, -.060))
for name, fit in [('Right', fit_right), ('Left', fit_left)]:
    anchor = empty('HandWristAnchor_'+name, body)
    anchor.matrix_basis = fit.inverted()
    anchor['pose'] = 'HandleRadio in source/Hands.blend; left uses reflected hand geometry'
    anchor['space'] = 'canonical wrist metres; excludes source scene placement'
scene.frame_set(1)
bpy.context.view_layer.update()
parts = []
for o in asset.objects:
    if o.type == 'MESH':
        o.data.calc_loop_triangles()
        parts.append({'name': o.name, 'triangles': len(o.data.loop_triangles), 'parent': o.parent.name,
                      'material': o.data.materials[0].name})
triangles = sum(p['triangles'] for p in parts)
assert triangles < 18000, triangles

# Interchange export excludes the studio and all hand review geometry.
bpy.ops.object.select_all(action='DESELECT')
for o in asset.objects:
    o.select_set(True)
bpy.context.view_layer.objects.active = root
bpy.ops.export_scene.gltf(filepath=str(OUT/'service-radio-v1.glb'), export_format='GLB',
    use_selection=True, export_extras=True, export_animations=True, export_yup=True,
    export_animation_mode='ACTIONS', export_force_sampling=True)


def aim(o, target):
    o.rotation_euler = (Vector(target)-o.location).to_track_quat('-Z', 'Y').to_euler()


bpy.ops.object.camera_add(location=(-.32, -.48, .28))
cam = move_collection(bpy.context.object, studio)
cam.name = 'REVIEW | radio three-quarter'
cam.data.type, cam.data.ortho_scale = 'ORTHO', .29
aim(cam, (0, 0, .10))
scene.camera = cam
for name, loc, power, size in [('Key', (-.25, -.22, .45), 6, .28),
                               ('Fill', (.28, -.15, .27), 3, .24),
                               ('Rim', (.06, .24, .34), 5, .22)]:
    bpy.ops.object.light_add(type='AREA', location=loc)
    o = move_collection(bpy.context.object, studio)
    o.name, o.data.energy, o.data.size = 'REVIEW | '+name, power, size
    aim(o, (0, 0, .09))
floor_mat = material('PREVIEW | neutral ground', (.12, .14, .17), .85)
bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, -.008))
floor = move_collection(bpy.context.object, studio)
floor.name = 'REVIEW | floor'
floor.data.materials.append(floor_mat)
scene.world.color = (.22, .22, .22)
scene.render.engine = 'CYCLES'
scene.cycles.samples, scene.cycles.use_denoising = 40, True
scene.render.resolution_x, scene.render.resolution_y = 1000, 1100
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.view_settings.view_transform = 'AgX'
studio.hide_viewport = True
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type == 'VIEW_3D':
            s = area.spaces.active
            s.region_3d.view_location = (0, 0, .10)
            s.region_3d.view_distance = .34
            s.region_3d.view_rotation = cam.rotation_euler.to_quaternion()
            s.clip_start = .0005
            s.shading.type = 'MATERIAL'
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'service-radio-v1.blend'), compress=True)
manifest = {'task': 97, 'status': 'review; no commit; no game integration', 'units': 'metres',
            'variant': '1 | separate upper collar', 'body_size_m': [.056, .034, .142],
            'antenna_height_m': .070, 'accent_material': accent.name, 'colors_linear': COLORS,
            'ptt_travel_m': .0015, 'ptt_frames': [1, 11], 'triangles': triangles, 'parts': parts,
            'radio_from_right_wrist_m': [list(r) for r in fit_right],
            'radio_from_left_wrist_m': [list(r) for r in fit_left]}
(OUT/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf8')
for service, color in (COLORS.items() if '--no-render' not in sys.argv else []):
    accent.diffuse_color = (*color, 1)
    accent.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (*color, 1)
    scene.render.filepath = str(REVIEW/f'radio-{service}.png')
    bpy.ops.render.render(write_still=True)
print(f'CREATED service radio: {triangles} triangles, single tint collar, PTT animation', flush=True)
