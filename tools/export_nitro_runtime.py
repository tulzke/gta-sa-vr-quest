"""Read the approved BLEND without saving it; bake the native static payload."""
import hashlib
import struct
from pathlib import Path
import bpy

ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'assets/nitro-lever/nitro-lever-v1.blend'
before=hashlib.sha256(source.read_bytes()).digest()
bpy.ops.wm.open_mainfile(filepath=str(source))
scene=bpy.context.scene;scene.frame_set(13);bpy.context.view_layer.update()
pivot=bpy.data.objects['NitroPivot'];asset=bpy.data.collections['NITRO | editable asset']
vertices=[];indices=[];lookup={}
for obj in asset.objects:
    if obj.type!='MESH':continue
    role=1 if obj.parent==pivot else 0
    if obj.name.startswith('ReserveSegment_'):role=10+int(obj.name.rsplit('_',1)[1])-1
    if obj.name in ['ReserveHundreds','ReserveTens','ReserveUnits']:role=20+['ReserveHundreds','ReserveTens','ReserveUnits'].index(obj.name)
    if obj.name=='FlowLampLens':role=30
    transform=pivot.matrix_world.inverted()@obj.matrix_world if role==1 else obj.matrix_world
    normal=transform.to_3x3().inverted_safe().transposed()
    color=obj.data.materials[0].diffuse_color[:3]
    layer=obj.data.uv_layers.active
    if layer:
        us=[loop.uv.x for loop in layer.data];vs=[loop.uv.y for loop in layer.data]
        u0,u1,v0,v1=min(us),max(us),min(vs),max(vs)
    obj.data.calc_loop_triangles()
    for triangle in obj.data.loop_triangles:
        for li in triangle.loops:
            loop=obj.data.loops[li];p=transform@obj.data.vertices[loop.vertex_index].co
            n=(normal@obj.data.corner_normals[li].vector).normalized()
            uv=list(layer.data[li].uv)if layer else [-1.,-1.]
            if 20<=role<=22:uv=[(uv[0]-u0)/(u1-u0),(uv[1]-v0)/(v1-v0)]
            packed=struct.pack('<12f',*p,*n,*color,*uv,float(role))
            if packed not in lookup:lookup[packed]=len(vertices);vertices.append(packed)
            indices.append(lookup[packed])
out=ROOT/'assets/nitro-lever/runtime';out.mkdir(exist_ok=True)
(out/'nitro.mesh').write_bytes(struct.pack('<4sIII',b'SVNT',1,len(vertices),len(indices))+b''.join(vertices)+struct.pack(f'<{len(indices)}H',*indices))
image=bpy.data.images['Nitro monochrome atlas | shared']
assert tuple(image.size)==(1024,512)
rgba=list(image.pixels[:]);alpha=bytes(max(0,min(255,round(rgba[i]*255)))for i in range(3,len(rgba),4))
(out/'nitro.alpha').write_bytes(alpha)
assert hashlib.sha256(source.read_bytes()).digest()==before
print(f'PASS approved source unchanged; runtime {len(vertices)} vertices, {len(indices)//3} triangles, shared 1024x512 alpha atlas')
