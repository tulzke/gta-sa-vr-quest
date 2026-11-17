"""Verify closed solids, 4000-triangle budget, UVs and GLB trigger motion."""
from pathlib import Path
import bpy,bmesh,json,struct,math
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'assets/firetruck-controller'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'firetruck-controller-v1.blend'))
scene=bpy.context.scene;scene.frame_set(1)
asset=bpy.data.collections['ASSET | Firetruck controller']
meshes=[o for o in asset.objects if o.type=='MESH']
manifest=json.loads((OUT/'manifest.json').read_text())
issues=[];triangles=0
for o in meshes:
 bm=bmesh.new();bm.from_mesh(o.data)
 bad=[e for e in bm.edges if not e.is_manifold]
 zero=[f for f in bm.faces if f.calc_area()<1e-12]
 if bad or zero:issues.append((o.name,len(bad),len(zero)))
 assert bm.calc_volume(signed=True)>0,(o.name,'inverted or empty volume')
 bm.free();o.data.calc_loop_triangles();triangles+=len(o.data.loop_triangles)
 assert all(abs(s-1)<1e-6 for s in o.scale),(o.name,'unapplied scale')
assert not issues,issues
assert triangles==manifest['triangles'] and triangles<=4000,triangles
assert not [o.name for o in asset.objects if any(s in o.name.lower() for s in ('knob','dial','brightness'))]
assert bpy.data.objects['IndexFingerTrigger'].parent.name=='TriggerPivot'
assert bpy.data.objects['TriggerPivot'].parent.name=='MonitorDevice'
assert bpy.data.objects['CeilingPlate'].parent.name=='RoofCradle'
screen=bpy.data.objects['CurvedCRTScreen']
assert len(screen.data.uv_layers)==1
assert all(-1e-6<=v<=1+1e-6 for uv in screen.data.uv_layers[0].data for v in uv.uv)
assert abs(screen['aspect']-4/3)<1e-6
assert abs(screen['active_size_m'][0]/screen['active_size_m'][1]-4/3)<1e-6
source_positions=[]
for f,expected in [(1,0),(6,-6),(11,-12)]:
 scene.frame_set(f);bpy.context.view_layer.update()
 pivot=bpy.data.objects['TriggerPivot']
 assert abs(math.degrees(pivot.rotation_euler.x)-expected)<1e-4,(f,pivot.rotation_euler[:])
 source_positions.append(bpy.data.objects['IndexTriggerContact'].matrix_world.translation.copy())
assert source_positions[-1].y<source_positions[0].y-.003
assert .004<(source_positions[-1]-source_positions[0]).length<.009
assert all(image.packed_file for image in bpy.data.images if image.source=='FILE'),[(i.name,i.filepath) for i in bpy.data.images if i.source=='FILE' and not i.packed_file]

data=(OUT/'firetruck-controller-v1.glb').read_bytes()
magic,version,length=struct.unpack_from('<3I',data)
assert magic==0x46546c67 and version==2 and length==len(data)
size,kind=struct.unpack_from('<2I',data,12);assert kind==0x4e4f534a
doc=json.loads(data[20:20+size])
assert not doc.get('cameras')
assert 'KHR_lights_punctual' not in doc.get('extensions',{})
assert not [n.get('name') for n in doc['nodes'] if any(k in n.get('name','') for k in ('REVIEW','DEMO','PREVIEW'))]
assert [a['name'] for a in doc['animations']]==['WaterTriggerPull']
assert len(doc.get('images',[]))==1
assert all('bufferView' in i and not i.get('uri') for i in doc['images'])
exported_triangles=sum(doc['accessors'][p['indices']]['count']//3 for m in doc['meshes'] for p in m['primitives'])
assert exported_triangles==triangles,(triangles,exported_triangles)

# Independent GLB import must retain hinge motion, mounting split and UVs.
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.render.fps=30
bpy.ops.import_scene.gltf(filepath=str(OUT/'firetruck-controller-v1.glb'))
pivot=bpy.data.objects['TriggerPivot'];action=pivot.animation_data.action
start,end=action.frame_range
for frame,expected in [(start,source_positions[0]),(end,source_positions[-1])]:
 scene.frame_set(round(frame));bpy.context.view_layer.update()
 actual=bpy.data.objects['IndexTriggerContact'].matrix_world.translation
 assert (actual-expected).length<1e-5,(frame,actual[:],expected[:])
assert bpy.data.objects['CeilingPlate'].parent.name=='RoofCradle'
assert len(bpy.data.objects['CurvedCRTScreen'].data.uv_layers)==1
result={'status':'PASS','mesh_parts':len(meshes),'triangles':triangles,'manifold_solids':True,'uv_4_3':True,'no_knob':True,'self_contained_source_and_glb':True,'trigger_contact_travel_mm':(source_positions[-1]-source_positions[0]).length*1000,'trigger_glb_roundtrip':True,'cameras_lights_excluded':True}
(ROOT/'build/firetruck-controller-review/verification.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2),flush=True)
