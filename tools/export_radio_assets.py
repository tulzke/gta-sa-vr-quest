"""Export the two static radio meshes and their packed textures from Blender.

blender -b assets/radio/Radio.blend --python tools/export_radio_assets.py -- assets/radio/runtime
Both meshes use the knob origin as a common pivot in Blender scene coordinates.
UXRH's four hand-pose slots are identical; these meshes are not hand poses.
"""
import json
import math
from pathlib import Path
import struct
import sys

import bpy
from mathutils import Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
from radio_png import rgba_png


def export_mesh(obj, pivot, destination):
    evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh = evaluated.to_mesh()
    try:
        if not mesh.uv_layers.active:
            raise ValueError(f'{obj.name}: missing UV map')
        material_ids = {poly.material_index for poly in mesh.polygons}
        if len(material_ids) != 1:
            raise ValueError(f'{obj.name}: expected one active material')
        mesh.calc_loop_triangles()
        matrix = obj.matrix_world
        normal_matrix = matrix.to_3x3().inverted_safe().transposed()
        uv_data = mesh.uv_layers.active.data
        vertices, indices, lookup = [], [], {}
        for tri in mesh.loop_triangles:
            for loop_id in tri.loops:
                loop = mesh.loops[loop_id]
                position = matrix @ mesh.vertices[loop.vertex_index].co - pivot
                normal = (normal_matrix @ mesh.corner_normals[loop_id].vector).normalized()
                uv = uv_data[loop_id].uv
                values = tuple(position) + tuple(normal) + tuple(uv)
                if not all(math.isfinite(v) for v in values):
                    raise ValueError(f'{obj.name}: non-finite vertex')
                # UXRH has per-vertex UV and normals; split seams and hard edges.
                key = struct.pack('<8f', *values)
                if key not in lookup:
                    lookup[key] = len(vertices)
                    vertices.append(values)
                indices.append(lookup[key])
        if not indices or len(vertices) > 65535:
            raise ValueError(f'{obj.name}: empty mesh or too many vertices for UXRH')
        with destination.open('wb') as out:
            out.write(struct.pack('<4sIII', b'UXRH', 1, len(vertices), len(indices)))
            for v in vertices:
                out.write(struct.pack('<26f', *(v[:3]*4 + v[3:6]*4 + v[6:8])))
            out.write(struct.pack(f'<{len(indices)}H', *indices))
        return {'vertices':len(vertices), 'triangles':len(indices)//3}
    finally:
        evaluated.to_mesh_clear()


def main():
    if '--' not in sys.argv:
        raise SystemExit('Expected output directory after --')
    out = Path(sys.argv[sys.argv.index('--')+1])
    out.mkdir(parents=True, exist_ok=True)
    base = bpy.data.objects.get('Подложка')
    knob = bpy.data.objects.get('Крутилка')
    if not base or not knob or base.type != 'MESH' or knob.type != 'MESH':
        raise ValueError('Expected mesh objects Подложка and Крутилка')
    pivot = knob.matrix_world.translation.copy()
    manifest = {'coordinates':'Blender scene XYZ, origin at knob object origin, +Z towards knob face',
                'units':'Blender scene units; scale for game at integration',
                'pivot_in_blender_scene':list(pivot), 'parts':{}}
    for obj, name, image_name in ((base, 'base', 'Подложка 2'), (knob, 'knob', 'Radio.png')):
        image = bpy.data.images.get(image_name)
        if not image or not image.packed_file:
            raise ValueError(f'{obj.name}: image {image_name} is not packed')
        used = {poly.material_index for poly in obj.data.polygons}
        if len(used) != 1:
            raise ValueError(f'{obj.name}: expected one assigned material')
        material = obj.material_slots[next(iter(used))].material
        if not material or not material.use_nodes or not any(
            link.from_node.type == 'TEX_IMAGE' and link.from_node.image == image and
            link.to_node.type == 'BSDF_PRINCIPLED' and link.to_socket.name == 'Base Color'
            for link in material.node_tree.links
        ):
            raise ValueError(f'{obj.name}: expected packed image connected to base color')
        texture = out / f'{name}.png'
        texture.write_bytes(bytes(image.packed_file.data))
        if not texture.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'):
            raise ValueError(f'{obj.name}: packed image is not PNG')
        width, height, rgba = rgba_png(texture.read_bytes())
        if [width, height] != list(image.size):
            raise ValueError(f'{obj.name}: PNG dimensions differ from Blender image')
        (out / f'{name}.rgba').write_bytes(rgba)
        info = export_mesh(obj, pivot, out / f'{name}.uxrh')
        info.update(texture=texture.name, texture_size=list(image.size), source_object=obj.name)
        manifest['parts'][name] = info
    (out / 'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(manifest,ensure_ascii=False,indent=2))


if __name__ == '__main__':
    main()
