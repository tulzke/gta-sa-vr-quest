"""Export only the approved 3222-triangle asset; no demo image/game assets."""
from pathlib import Path
import bpy,struct
ROOT=Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'assets/firetruck-controller/firetruck-controller-v1.blend'))
bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
records=[];indices=[];lookup={}
for o in bpy.data.collections['ASSET | Firetruck controller'].objects:
 if o.type!='MESH':continue
 ancestors=[];p=o
 while p:ancestors.append(p.name);p=p.parent
 o.data.calc_loop_triangles();nm=o.matrix_world.to_3x3().inverted().transposed()
 uv=o.data.uv_layers.active
 for tri in o.data.loop_triangles:
  material=o.data.materials[tri.material_index]
  role=0 if 'RoofCradle' in ancestors else 2 if 'TriggerPivot' in ancestors else 3 if material.name.startswith('CRT_VideoSurface') else 4 if 'PowerLampAmberLens' in o.name else 1
  color=material.diffuse_color[:3]
  for li in tri.loops:
   loop=o.data.loops[li];p=o.matrix_world@o.data.vertices[loop.vertex_index].co;n=(nm@o.data.corner_normals[li].vector).normalized()
   tex=tuple(uv.data[li].uv) if uv else (0.,0.)
   data=struct.pack('<12f',*p,*n,*color,*tex,float(role))
   if data not in lookup:lookup[data]=len(records);records.append(data)
   indices.append(lookup[data])
assert len(indices)//3<=4000 and len(records)<65536
out=ROOT/'assets/firetruck-controller/runtime';out.mkdir(exist_ok=True)
(out/'monitor.mesh').write_bytes(struct.pack('<4sIII',b'WM13',1,len(records),len(indices))+b''.join(records)+struct.pack('<'+'H'*len(indices),*indices))
print('EXPORTED',len(records),'vertices',len(indices)//3,'triangles; no textures',flush=True)
