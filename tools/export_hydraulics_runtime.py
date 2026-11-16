"""Export the approved neutral BLEND as a compact ring-deformable native payload."""
import hashlib
import math
import struct
from pathlib import Path
import bpy

ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'assets/hydraulics/hydraulics-joystick-v1.blend'
before=hashlib.sha256(source.read_bytes()).digest()
bpy.ops.wm.open_mainfile(filepath=str(source))
bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
asset=bpy.data.collections['HYDRAULICS | editable asset']
pivot=bpy.data.objects['HydraulicsPivot']
vertices=[];indices=[];lookup={}
boot_references={}
for obj in asset.objects:
    if obj.type!='MESH':continue
    role=1 if obj.parent==pivot else 0
    if obj.name=='Bellows | continuously deformed':role=2
    if obj.parent and obj.parent.name=='HydraulicsModeButton':role=3
    if obj.name=='StateLamp_LOW':role=4
    if obj.name=='StateLamp_HIGH':role=5
    if obj.name.startswith('DetentTick_'):role=10+int(obj.name.rsplit('_',1)[1])
    matrix=pivot.matrix_world.inverted()@obj.matrix_world if role==1 else obj.matrix_world
    normal=matrix.to_3x3().inverted_safe().transposed()
    evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh=evaluated.to_mesh()
    mesh.calc_loop_triangles()
    # Primitive meshes also have UVs. Only the authored label layer samples
    # the alpha atlas; applying it to the ball cuts holes and draws letters.
    layer=mesh.uv_layers.get('LabelAtlas')
    color=obj.data.materials[0].diffuse_color[:3]
    for tri in mesh.loop_triangles:
        for li in tri.loops:
            loop=mesh.loops[li];p=matrix@mesh.vertices[loop.vertex_index].co
            n=(normal@mesh.corner_normals[li].vector).normalized()
            uv=list(layer.data[li].uv) if layer else [-1.,-1.]
            t=min(1.,max(0.,(p.z-.019)/(.045-.019))) if role==2 else 0.
            packed=struct.pack('<14f',*p,*n,*color,*uv,float(role),t*t*(3-2*t),p.z)
            if packed not in lookup:
                lookup[packed]=len(vertices);vertices.append(packed)
                if role==2:boot_references[lookup[packed]]=loop.vertex_index
            indices.append(lookup[packed])
    evaluated.to_mesh_clear()
assert len(vertices)<65536
out=ROOT/'assets/hydraulics/runtime';out.mkdir(exist_ok=True)
(out/'hydraulics.mesh').write_bytes(struct.pack('<4sIII',b'SVHY',1,len(vertices),len(indices))+b''.join(vertices)+struct.pack(f'<{len(indices)}H',*indices))
image=bpy.data.images['Hydraulics | shared label atlas']
assert tuple(image.size)==(1024,256)
rgba=list(image.pixels[:])
(out/'hydraulics.alpha').write_bytes(bytes(max(0,min(255,round(rgba[i]*255)))for i in range(3,len(rgba),4)))
assert hashlib.sha256(source.read_bytes()).digest()==before
root=bpy.data.objects['HydraulicsRoot'];root.animation_data_clear()
fixture=bytearray(struct.pack('<4sII',b'HYRF',5,len(boot_references)))
for x,y in [(0,0),(22,0),(0,22),(-22/math.sqrt(2),22/math.sqrt(2)),(5.3,-9.7)]:
    root['tilt_right_deg'],root['tilt_forward_deg']=x,y;root.update_tag();bpy.context.view_layer.update()
    obj=bpy.data.objects['Bellows | continuously deformed'];evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());mesh=evaluated.to_mesh()
    fixture+=struct.pack('<2f',math.radians(x),math.radians(y))
    for native,index in boot_references.items():
        p=evaluated.matrix_world@mesh.vertices[index].co
        fixture+=struct.pack('<I3f',native,*p)
    evaluated.to_mesh_clear()
testdir=ROOT/'build/hydraulics-tests';testdir.mkdir(exist_ok=True)
(testdir/'blender-reference.bin').write_bytes(fixture)
print(f'PASS approved source unchanged; {len(vertices)} native vertices, {len(indices)//3} triangles, continuous ring data')
