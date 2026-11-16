"""Export the approved saved radio to one embedded colored runtime mesh."""
import json
from pathlib import Path
import struct
import bpy

ROOT=Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'assets/service-radio/service-radio-v1.blend'))
bpy.context.scene.frame_set(1)
bpy.context.view_layer.update()
records=[];indices=[];lookup={}
for o in bpy.data.collections['ASSET | handheld service radio'].objects:
    if o.type!='MESH':continue
    ancestors=[];parent=o
    while parent:ancestors.append(parent.name);parent=parent.parent
    role=0 if 'RadioCradle' in ancestors else 3 if 'RadioPTT' in ancestors else 2 if o.name=='ServiceAccentCollar' else 1
    o.data.calc_loop_triangles()
    normal=o.matrix_world.to_3x3().inverted().transposed()
    color=o.data.materials[0].diffuse_color[:3]
    for tri in o.data.loop_triangles:
        for li in tri.loops:
            loop=o.data.loops[li];p=o.matrix_world@o.data.vertices[loop.vertex_index].co
            n=(normal@o.data.corner_normals[li].vector).normalized()
            data=struct.pack('<10f',*p,*n,*color,float(role))
            if data not in lookup:lookup[data]=len(records);records.append(data)
            indices.append(lookup[data])
assert len(records)<65536 and len(indices)<60000
out=ROOT/'assets/service-radio/runtime';out.mkdir(exist_ok=True)
(out/'service-radio.mesh').write_bytes(struct.pack('<4sIII',b'SR97',1,len(records),len(indices))+b''.join(records)+struct.pack('<'+'H'*len(indices),*indices))
print('EXPORTED radio runtime:',len(records),'vertices',len(indices)//3,'triangles; roles 0 dock / 1 body / 2 tint / 3 PTT')
