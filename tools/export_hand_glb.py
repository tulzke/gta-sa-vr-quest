"""Export a working COPY of source/Hands.blend to a skeletal glTF 2.0 GLB.

Blender 4.5: blender -b .cache/hand-skeletal/Hands-export.blend
  --python-exit-code 1 --python tools/export_hand_glb.py
No scene is saved. Geometry is emitted once; evaluated pose bones bake any IK
or constraints into local TRS channels. Eight influences preserve source weights.
"""
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

import bpy
from mathutils import Matrix
sys.path.insert(0, str(Path(__file__).resolve().parent))
from hand_glb import ACTIONS, action_key, uv_key

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/vrhands/skeletal'
CACHE = ROOT / '.cache/hand-skeletal'


def topology(mesh):
    mesh.calc_loop_triangles()
    uvs=[];lookup={};corners=[]
    for loop in mesh.loops:
        uv=mesh.uv_layers.active.data[loop.index].uv
        value=(float(uv.x),1-float(uv.y));key=uv_key(value)
        if key not in lookup:
            lookup[key]=len(uvs);uvs.append(value)
        corners.append(lookup[key])
    indices=[corners[i] for triangle in mesh.loop_triangles for i in triangle.loops]
    return uvs,lookup,indices


def evaluate(obj,lookup,indices,scale):
    evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh=evaluated.to_mesh()
    try:
        samples=[None]*len(lookup)
        for loop in mesh.loops:
            uv=mesh.uv_layers.active.data[loop.index].uv
            index=lookup[uv_key((uv.x,1-uv.y))]
            sample=(*(mesh.vertices[loop.vertex_index].co*scale),*mesh.corner_normals[loop.index].vector.normalized())
            if not all(math.isfinite(v) for v in sample):raise ValueError('Nonfinite evaluated geometry')
            if samples[index] is not None and max(abs(a-b) for a,b in zip(samples[index],sample))>1e-5:
                raise ValueError('A UV maps to different positions/normals')
            samples[index]=sample
        if any(v is None for v in samples):raise ValueError('Missing UV vertex')
        return samples
    finally:
        evaluated.to_mesh_clear()


def flat(m):
    return [m[r][c] for c in range(4) for r in range(4)]


def trs(m):
    t, q, s = m.decompose()
    rebuilt = Matrix.LocRotScale(t, q, s)
    if max(abs(m[r][c]-rebuilt[r][c]) for r in range(4) for c in range(4)) > 2e-5:
        raise ValueError('Bone transform contains shear; cannot represent as glTF TRS')
    return list(t), [q.x, q.y, q.z, q.w], list(s)


def main():
    print("Starting skeletal hand export", flush=True)
    source = ROOT / 'assets/vrhands/source/Hands.blend'
    if Path(bpy.data.filepath).resolve() in (source.resolve(),(ROOT/'assets/vrhands/BothHands.blend').resolve()):
        raise ValueError('Open a separate COPY of source/Hands.blend before exporting')
    OUT.mkdir(parents=True, exist_ok=True)
    CACHE.mkdir(parents=True, exist_ok=True)
    source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
    obj = bpy.data.objects['HandRight']
    rig = obj.find_armature()
    if [m.type for m in obj.modifiers] != ['ARMATURE']:
        raise ValueError('Only a single Armature modifier is supported')
    if obj.modifiers[0].use_deform_preserve_volume:
        raise ValueError('Dual quaternion skinning must be converted explicitly')
    bones = [b for b in rig.data.bones if b.use_deform]
    if len(bones) > 64:
        raise ValueError('Too many deform joints')
    joint = {b.name: i for i, b in enumerate(bones)}
    parents = []
    for b in bones:
        p = b.parent
        while p and p.name not in joint:
            p = p.parent
        parents.append(joint[p.name] if p else -1)
    # Asset-only basis: Blender mesh-local centimetres -> glTF Y-up metres.
    # In the renderer its inverse rotation yields the old UXR mesh-local metres.
    C = Matrix(((.01, 0, 0, 0), (0, 0, .01, 0), (0, -.01, 0, 0), (0, 0, 0, 1)))
    Ci = C.inverted()
    R = C.to_3x3() * 100
    arm = obj.matrix_world.inverted() @ rig.matrix_world
    rest = [C @ arm @ b.matrix_local @ Ci for b in bones]
    invbind = [m.inverted() for m in rest]
    uvs, lookup, indices = topology(obj.data)
    mesh = obj.data
    mesh.calc_loop_triangles()
    records = [None] * len(uvs)
    for loop in mesh.loops:
        uv = mesh.uv_layers.active.data[loop.index].uv
        index = lookup[uv_key((uv.x, 1-uv.y))]
        v = mesh.vertices[loop.vertex_index]
        influences = [(joint[obj.vertex_groups[g.group].name], g.weight) for g in v.groups
                      if obj.vertex_groups[g.group].name in joint and g.weight > 0]
        if not 1 <= len(influences) <= 8:
            raise ValueError('Expected one to eight deform weights per vertex')
        total = sum(w for _, w in influences)
        influences = [(j, w/total) for j, w in influences]
        influences += [(0, 0)] * (8-len(influences))
        records[index] = (list(C @ v.co), list((R @ mesh.corner_normals[loop.index].vector).normalized()),
                          [j for j, _ in influences], [w for _, w in influences])
    if any(v is None for v in records):
        raise ValueError('Missing UV seam vertex')
    doc = {'asset': {'version': '2.0', 'generator': 'SAVR skeletal hands'},
           'scene': 0, 'scenes': [{'nodes': [0]+[i+1 for i,p in enumerate(parents) if p < 0]}],
           'nodes': [{'name': 'HandRight', 'mesh': 0, 'skin': 0}],
           'bufferViews': [], 'accessors': [], 'animations': []}
    binary = bytearray()

    def blob(data):
        while len(binary) % 4:
            binary.append(0)
        view = len(doc['bufferViews'])
        doc['bufferViews'].append({'buffer': 0, 'byteOffset': len(binary), 'byteLength': len(data)})
        binary.extend(data)
        return view

    def accessor(rows, kind, component=5126, bounds=False):
        width = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}[kind]
        values = [x for row in rows for x in row]
        if not all(math.isfinite(x) for x in values):
            raise ValueError('Nonfinite accessor')
        a = {'bufferView': blob(struct.pack('<'+str(len(values))+('f' if component == 5126 else 'H'), *values)),
             'componentType': component, 'count': len(rows), 'type': kind}
        if bounds:
            a['min'] = [min(row[i] for row in rows) for i in range(width)]
            a['max'] = [max(row[i] for row in rows) for i in range(width)]
        doc['accessors'].append(a)
        return len(doc['accessors'])-1

    attributes = {'POSITION': accessor([v[0] for v in records], 'VEC3', bounds=True),
                  'NORMAL': accessor([v[1] for v in records], 'VEC3'),
                  'TEXCOORD_0': accessor(uvs, 'VEC2')}
    for n in range(2):
        attributes[f'JOINTS_{n}'] = accessor([v[2][n*4:n*4+4] for v in records], 'VEC4', 5123)
        attributes[f'WEIGHTS_{n}'] = accessor([v[3][n*4:n*4+4] for v in records], 'VEC4')
    doc['meshes'] = [{'name': 'HandRight', 'primitives': [{'attributes': attributes,
                      'indices': accessor([[i] for i in indices], 'SCALAR', 5123), 'material': 0}]}]
    image = (ROOT / 'assets/vrhands/BigHandsAlbedo.png').read_bytes()
    doc['images'] = [{'bufferView': blob(image), 'mimeType': 'image/png'}]
    doc['textures'] = [{'source': 0}]
    doc['materials'] = [{'name': 'Hands', 'pbrMetallicRoughness': {
        'baseColorTexture': {'index': 0}, 'metallicFactor': 0, 'roughnessFactor': 1}, 'doubleSided': True}]
    for i, b in enumerate(bones):
        local = rest[parents[i]].inverted() @ rest[i] if parents[i] >= 0 else rest[i]
        t, q, s = trs(local)
        node = {'name': b.name, 'translation': t, 'rotation': q, 'scale': s}
        children = [j+1 for j,p in enumerate(parents) if p == i]
        if children:
            node['children'] = children
        doc['nodes'].append(node)
    doc['skins'] = [{'joints': list(range(1, len(bones)+1)),
                     'inverseBindMatrices': accessor([flat(m) for m in invbind], 'MAT4')}]
    by_name = {}
    for action in bpy.data.actions:
        key = action_key(action.name)
        if key in by_name:
            raise ValueError(f'Ambiguous Action name: {action.name}')
        by_name[key] = action
    rig.animation_data_create()
    anim = rig.animation_data
    anim.use_nla = False
    reference = bytearray(struct.pack('<III', len(ACTIONS), len(records), len(indices)))
    report = {'sourceSha256': source_hash, 'joints': len(bones), 'vertices': len(records),
              'indices': len(indices), 'maxInfluences': max(len(v.groups) for v in mesh.vertices),
              'clips': [], 'coordinateSpace': 'glTF Y-up metres; asset inverse maps to UXR mesh-local metres'}
    for name in ACTIONS:
        for pb in rig.pose.bones:
            pb.matrix_basis = Matrix.Identity(4)
        action = by_name[action_key(name)]
        anim.action = action
        slots = [s for s in action.slots if s.target_id_type == 'OBJECT']
        if len(slots) != 1:
            raise ValueError('Expected one Action slot')
        anim.action_slot = slots[0]
        curves = action.layers[0].strips[0].channelbag(slots[0]).fcurves
        if any(not f.data_path.startswith('pose.bones[') for f in curves):
            raise ValueError('Object animation requires explicit handling')
        keys = {float(k.co.x) for f in curves for k in f.keyframe_points}
        start, end = min(keys), max(keys)
        frames = sorted(keys | {f/4 for f in range(math.ceil(start*4), math.floor(end*4)+1)})
        times = [(f-start)/(bpy.context.scene.render.fps/bpy.context.scene.render.fps_base) for f in frames]
        # Include midpoint source comparisons in the external QA fixture.
        checks = sorted(set(frames) | {(a+b)/2 for a,b in zip(frames, frames[1:])})
        reference.extend(struct.pack('<I', len(checks)))
        poses = [[] for _ in bones]
        for f in checks:
            bpy.context.scene.frame_set(math.floor(f), subframe=f-math.floor(f))
            deps = bpy.context.evaluated_depsgraph_get()
            evaluated_rig = rig.evaluated_get(deps)
            world = [C @ arm @ evaluated_rig.pose.bones[b.name].matrix @ Ci for b in bones]
            if f in frames:
                for i in range(len(bones)):
                    local = world[parents[i]].inverted() @ world[i] if parents[i] >= 0 else world[i]
                    poses[i].append(trs(local))
            reference.extend(struct.pack('<f', (f-start)/(end-start)))
            samples = evaluate(obj, lookup, indices, .01)
            reference.extend(struct.pack('<'+str(len(samples)*6)+'f', *(x for v in samples for x in v)))
        clip = {'name': name, 'samplers': [], 'channels': []}
        inp = accessor([[t] for t in times], 'SCALAR', bounds=True)
        endpoints = accessor([[times[0]], [times[-1]]], 'SCALAR', bounds=True)
        for i in range(len(bones)):
            for axis, path in enumerate(('translation', 'rotation', 'scale')):
                rows = [p[axis] for p in poses[i]]
                if axis == 1:
                    for j in range(1, len(rows)):
                        if sum(a*b for a,b in zip(rows[j-1], rows[j])) < 0:
                            rows[j] = [-v for v in rows[j]]
                constant = max(abs(a-b) for row in rows for a,b in zip(rows[0], row)) < 1e-6
                clip['channels'].append({'sampler': len(clip['samplers']), 'target': {'node': i+1, 'path': path}})
                clip['samplers'].append({'input': endpoints if constant else inp,
                                         'output': accessor([rows[0],rows[0]] if constant else rows, 'VEC4' if axis == 1 else 'VEC3'),
                                         'interpolation': 'LINEAR'})
        doc['animations'].append(clip)
        report['clips'].append({'name': name, 'frames': frames, 'duration': times[-1]})
        print(f'BAKED BONES {name}: {len(frames)} keys, {len(bones)} joints', flush=True)
    doc['buffers'] = [{'byteLength': len(binary)}]
    encoded = json.dumps(doc, separators=(',', ':')).encode()
    encoded += b' ' * (-len(encoded) % 4)
    binary += b'\0' * (-len(binary) % 4)
    glb = struct.pack('<III', 0x46546c67, 2, 12+8+len(encoded)+8+len(binary))
    glb += struct.pack('<II', len(encoded), 0x4e4f534a)+encoded
    glb += struct.pack('<II', len(binary), 0x004e4942)+binary
    (OUT / 'hands.glb').write_bytes(glb)
    (CACHE / 'reference.bin').write_bytes(reference)
    report['glbBytes'] = len(glb)
    (OUT / 'manifest.json').write_text(json.dumps(report, indent=2))
    assert hashlib.sha256(source.read_bytes()).hexdigest() == source_hash
    print(f'EXPORTED skeletal GLB: {len(glb)} bytes; source unchanged')


if __name__ == '__main__':
    main()
