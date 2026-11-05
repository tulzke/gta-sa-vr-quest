"""Bake Blender hand Actions into sampled UXRH blocks, including mirrored hands.

Run with Blender 4.5 (the source file is never saved):
  blender --factory-startup -b assets/vrhands/BothHands.blend --python-exit-code 1 \
    --python tools/export_hand_actions.py -- --output assets/vrhands/animations

Each block retains the existing UXRH v1 layout. Its four slots are successive
animation samples, NOT the ordinary open/grip/trigger/both blend corners.
manifest.json maps source frame numbers to block/slot pairs. No game gestures
are assigned by this offline exporter.
"""

import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

import bpy
from mathutils import Matrix


ROOT = Path(__file__).resolve().parents[1]
ACTIONS = ("Grip", "Pinch", "Point", "OpenPalm", "Beckon",
           "GripPistol", "GripPistolOffhand")


def action_key(name):
    return "".join(c.lower() for c in name if c.isalnum())


def uv_key(uv):
    return tuple(round(float(v), 5) for v in uv)


def read_reference(path):
    data = path.read_bytes()
    if len(data) < 16:
        raise ValueError("Truncated reference hand")
    magic, version, vertices, indices = struct.unpack_from("<4sIII", data)
    if (magic != b"UXRH" or version != 1 or not 0 < vertices <= 65535
            or not indices or indices % 3 or len(data) != 16 + vertices * 104 + indices * 2):
        raise ValueError("Unsupported reference hand")
    uvs = [struct.unpack_from("<2f", data, 16 + i * 104 + 96) for i in range(vertices)]
    lookup = {uv_key(uv): i for i, uv in enumerate(uvs)}
    if len(lookup) != vertices:
        raise ValueError("Reference UVs must uniquely identify vertices")
    triangles = struct.unpack_from(f"<{indices}H", data, 16 + vertices * 104)
    if max(triangles) >= vertices:
        raise ValueError("Reference index outside vertex range")
    return data, uvs, lookup, triangles


def evaluate(obj, lookup, triangles, scale):
    """Keep the reference UV/index order, removing scene placement transforms.

    Coordinates are evaluated in mesh-local centimetres, as in the existing
    build_pinch_hand.py converter. This deliberately excludes layout placement
    of the hands in the Blender scene. UV seams become separate runtime verts.
    """
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()
    try:
        if mesh.uv_layers.active is None:
            raise ValueError("Hand has no UV map")
        mesh.calc_loop_triangles()
        uv = mesh.uv_layers.active.data
        samples = [None] * len(lookup)
        loop_indices = []
        normals = mesh.corner_normals
        for loop in mesh.loops:
            coord = uv[loop.index].uv
            key = uv_key((coord.x, 1.0 - coord.y))
            if key not in lookup:
                raise ValueError(f"UV {key} is absent from the reference hand")
            index = lookup[key]
            position = mesh.vertices[loop.vertex_index].co * scale
            normal = normals[loop.index].vector.normalized()
            sample = (*position, *normal)
            if not all(math.isfinite(v) for v in sample) or normal.length < .99:
                raise ValueError("Invalid evaluated position or normal")
            previous = samples[index]
            if previous is not None and max(abs(a-b) for a, b in zip(previous, sample)) > 1e-5:
                raise ValueError("One UV maps to different positions/normals")
            samples[index] = sample
            loop_indices.append(index)
        if any(sample is None for sample in samples):
            raise ValueError("Source mesh is missing reference vertices")
        faces = Counter(tuple(sorted(loop_indices[i] for i in tri.loops))
                        for tri in mesh.loop_triangles)
        reference_faces = Counter(tuple(sorted(triangles[i:i+3]))
                                  for i in range(0, len(triangles), 3))
        if faces != reference_faces:
            raise ValueError("Source triangulation differs from the reference hand")
        return samples
    finally:
        evaluated.to_mesh_clear()


def write_block(path, samples, uvs, indices, left=False):
    # Reflect local Y, as in build_pinch_hand.py's right/left coordinate map.
    # Winding must reverse along with normals to retain outward-facing faces.
    sign = -1.0 if left else 1.0
    data = bytearray(struct.pack("<4sIII", b"UXRH", 1, len(uvs), len(indices)))
    frames = list(samples)
    while len(frames) < 4:
        frames.append(frames[-1])
    for vertex, uv in enumerate(uvs):
        positions, normals = [], []
        for frame in frames:
            x, y, z, nx, ny, nz = frame[vertex]
            positions.extend((x, sign * y, z))
            normals.extend((nx, sign * ny, nz))
        data.extend(struct.pack("<26f", *positions, *normals, *uv))
    winding = list(indices)
    if left:
        for i in range(0, len(winding), 3):
            winding[i+1], winding[i+2] = winding[i+2], winding[i+1]
    data.extend(struct.pack(f"<{len(winding)}H", *winding))
    path.write_bytes(data)
    return {"path": path.name, "size": len(data),
            "sha256": hashlib.sha256(data).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mesh", default="HandRight")
    parser.add_argument("--reference", type=Path, default=ROOT / "assets/vrhands/BigHandRight.uxrh")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--units-per-metre", type=float, default=100.0)
    args = parser.parse_args(sys.argv[sys.argv.index("--")+1:])
    if not math.isfinite(args.units_per_metre) or args.units_per_metre <= 0:
        parser.error("--units-per-metre must be positive and finite")
    obj = bpy.data.objects.get(args.mesh)
    if obj is None or obj.type != "MESH":
        raise ValueError(f"Missing mesh {args.mesh}")
    rig = obj.find_armature()
    if rig is None:
        raise ValueError("Hand must have an armature")
    by_name = {}
    for action in bpy.data.actions:
        key = action_key(action.name)
        if key in by_name:
            raise ValueError(f"Ambiguous Action name: {action.name}")
        by_name[key] = action
    missing = [name for name in ACTIONS if action_key(name) not in by_name]
    if missing:
        raise ValueError(f"Missing Actions: {', '.join(missing)}")
    reference_data, uvs, lookup, indices = read_reference(args.reference)
    scene = bpy.context.scene
    rig.animation_data_create()
    animation = rig.animation_data
    old_action, old_slot = animation.action, animation.action_slot
    old_frame, old_subframe = scene.frame_current, scene.frame_subframe
    old_nla = animation.use_nla
    old_pose = {b.name: b.matrix_basis.copy() for b in rig.pose.bones}
    manifest = {"format": "SAVR hand Action samples", "version": 1,
                "source": Path(bpy.data.filepath).name, "mesh": obj.name,
                "armature": rig.name, "blender": bpy.app.version_string,
                "fps": scene.render.fps / scene.render.fps_base,
                "referenceSha256": hashlib.sha256(reference_data).hexdigest(),
                "vertices": len(uvs), "indices": len(indices),
                "coordinateSpace": "mesh-local metres; Left reflects local Y and winding",
                "clips": []}
    # Validate/evaluate every Action before publishing any output.
    clips = []
    try:
        animation.use_nla = False
        for name in ACTIONS:
            action = by_name[action_key(name)]
            # Prevent unkeyed channels from leaking from the previous Action.
            for bone in rig.pose.bones:
                bone.matrix_basis = Matrix.Identity(4)
            animation.action = action
            slots = [s for s in action.slots if s.target_id_type == "OBJECT"]
            if len(slots) != 1:
                raise ValueError(f"{action.name}: expected one object Action slot")
            animation.action_slot = slots[0]
            curves = action.layers[0].strips[0].channelbag(slots[0]).fcurves
            if any(not f.data_path.startswith('pose.bones[') for f in curves):
                raise ValueError(f"{action.name}: object animation needs explicit coordinate handling")
            keys = {float(k.co.x) for curve in curves for k in curve.keyframe_points}
            if len(keys) < 2:
                raise ValueError(f"{action.name}: expected an animated frame range")
            start, end = min(keys), max(keys)
            frames = sorted(keys | {float(f) for f in range(math.ceil(start), math.floor(end)+1)})
            samples = []
            for frame in frames:
                whole = math.floor(frame)
                scene.frame_set(whole, subframe=frame-whole)
                samples.append(evaluate(obj, lookup, indices, 1 / args.units_per_metre))
            clips.append((name, action.name, frames, samples))
            print(f"BAKED {name}: {action.name!r}, {start:g}..{end:g}, {len(frames)} samples")
    finally:
        animation.action = old_action
        if old_action is not None:
            animation.action_slot = old_slot
        animation.use_nla = old_nla
        for bone in rig.pose.bones:
            bone.matrix_basis = old_pose[bone.name]
        scene.frame_set(old_frame, subframe=old_subframe)
    args.output.mkdir(parents=True, exist_ok=True)
    for name, source_name, frames, samples in clips:
        folder = args.output / name
        folder.mkdir(exist_ok=True)
        clip = {"name": name, "sourceAction": source_name, "start": frames[0],
                "end": frames[-1], "durationSeconds": (frames[-1]-frames[0])/manifest["fps"],
                "samples": [{"frame": f, "block": i//4, "pose": i%4}
                            for i, f in enumerate(frames)], "blocks": []}
        for offset in range(0, len(samples), 4):
            block = {"count": min(4, len(samples)-offset)}
            for side in ("Right", "Left"):
                path = folder / f"BigHand{side}-{offset//4:03d}.uxrh"
                info = write_block(path, samples[offset:offset+4], uvs, indices, side == "Left")
                info["path"] = f"{name}/{info['path']}"
                block[side.lower()] = info
            clip["blocks"].append(block)
        manifest["clips"].append(clip)
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"EXPORTED {len(clips)} Actions to {args.output}")


if __name__ == "__main__":
    main()
