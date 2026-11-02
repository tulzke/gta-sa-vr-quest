#!/usr/bin/env python3
"""Run in Blender with the source .blend open to bake a joint-aware pinch path.

blender -b hand.blend --python tools/build_pinch_transition.py -- OPEN.uxrh PINCH.uxrh OUTPUT.uxrh
PINCH may already contain four frames; frame 3 is the final pose.
"""
import sys
import struct
from pathlib import Path
import bpy
import numpy as np
from mathutils import Matrix, Quaternion


def read(path):
    data = bytearray(Path(path).read_bytes())
    magic, version, vertices, indices = struct.unpack_from('<4sIII', data)
    if magic != b'UXRH' or version != 1 or len(data) != 16 + vertices * 104 + indices * 2:
        raise ValueError('Invalid UXRH')
    values = np.array([struct.unpack_from('<26f', data, 16 + i * 104) for i in range(vertices)])
    return data, values


def uv_key(uv):
    return tuple(round(float(x), 5) for x in uv)


def weights(mesh, uvs):
    group_names = {group.index: group.name for group in mesh.vertex_groups}
    uv_layer = mesh.data.uv_layers.active.data
    by_uv = {}
    for loop in mesh.data.loops:
        coord = uv_layer[loop.index].uv
        vertex = mesh.data.vertices[loop.vertex_index]
        by_uv[uv_key((coord.x, 1.0 - coord.y))] = {
            group_names[item.group]: item.weight for item in vertex.groups}
    rows = [by_uv[uv_key(uv)] for uv in uvs]
    groups = sorted({name for row in rows for name in row})
    w = np.array([[row.get(name, 0) for name in groups] for row in rows])
    w /= w.sum(axis=1)[:, None]
    return groups, w


def fit(source, target, weight):
    weight = weight / weight.sum()
    a = (source * weight[:, None]).sum(axis=0)
    b = (target * weight[:, None]).sum(axis=0)
    u, _, vt = np.linalg.svd(((source - a) * weight[:, None]).T @ (target - b))
    correction = np.eye(3)
    correction[2, 2] = np.linalg.det(u @ vt)
    r = u @ correction @ vt
    return r, b - a @ r


def main():
    args = sys.argv[sys.argv.index('--') + 1:]
    if len(args) != 3:
        raise ValueError('Expected OPEN.uxrh PINCH.uxrh OUTPUT.uxrh')
    original, start = read(args[0])
    target_data, end = read(args[1])
    if len(original) != len(target_data) or not np.array_equal(start[:, 24:26], end[:, 24:26]):
        raise ValueError('Hand geometry does not match')
    groups, w = weights(bpy.data.objects['HandRight'], start[:, 24:26])
    x, final = start[:, :3], end[:, 9:12]
    normals, final_normals = start[:, 12:15], end[:, 21:24]
    transforms = []
    for i, name in enumerate(groups):
        weight = w[:, i]
        mask = weight > .95
        if mask.sum() < 8: mask = weight > .55
        if mask.sum() < 8: mask = weight > .1
        if mask.sum() < 3: raise ValueError(f'Insufficient vertices for {name}')
        rotation, translation = fit(x[mask], final[mask], weight[mask] ** 4)
        # x @ rotation + translation: decompose into rotation around a point
        # and translation along its screw axis. This traces an arc, not a chord.
        column = rotation.T
        pivot = np.linalg.lstsq(np.eye(3) - column, translation, rcond=1e-8)[0]
        axial = translation - (np.eye(3) - column) @ pivot
        q = Matrix(column.tolist()).to_quaternion().normalized()
        transforms.append((q, pivot, axial))

    def pose(t):
        result = np.zeros_like(x)
        result_n = np.zeros_like(normals)
        for i, (q, pivot, axial) in enumerate(transforms):
            rotation = np.array(Quaternion((1, 0, 0, 0)).slerp(q, t).to_matrix())
            moved = (x - pivot) @ rotation.T + pivot + t * axial
            result += w[:, i, None] * moved
            result_n += w[:, i, None] * (normals @ rotation.T)
        return result, result_n

    estimate, estimate_n = pose(1.0)
    error = np.linalg.norm(estimate - final, axis=1)
    if np.quantile(error, .95) > .003 or error.max() > .01:
        raise ValueError(f'Bone fit is inaccurate: p95={np.quantile(error, .95):.6f} max={error.max():.6f}')
    for frame in range(4):
        t = (frame + 1) / 4
        positions, ns = pose(t)
        # Correct residual skinning differences while preserving exact endpoints.
        positions += t * (final - estimate)
        ns += t * (final_normals - estimate_n)
        ns /= np.linalg.norm(ns, axis=1)[:, None]
        if frame == 3:
            positions = final
            ns = final_normals
        for i in range(len(start)):
            struct.pack_into('<3f', original, 16 + i * 104 + frame * 12, *positions[i])
            struct.pack_into('<3f', original, 16 + i * 104 + 48 + frame * 12, *ns[i])
    Path(args[2]).write_bytes(original)
    print(f'Baked {len(start)} vertices; bone fit p95 {np.quantile(error, .95) * 1000:.2f} mm')


if __name__ == '__main__':
    main()
