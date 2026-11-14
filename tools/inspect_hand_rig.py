"""Offline source inventory; never saves the source blend."""
import bpy
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
(root / '.cache').mkdir(exist_ok=True)
obj = bpy.data.objects['HandRight']
rig = obj.find_armature()
report = {
    'blender': bpy.app.version_string,
    'objects': [{'name': o.name, 'type': o.type, 'world': [list(r) for r in o.matrix_world]} for o in bpy.data.objects],
    'modifiers': [(m.name, m.type) for m in obj.modifiers],
    'vertices': len(obj.data.vertices),
    'maxInfluences': max(len(v.groups) for v in obj.data.vertices),
    'posePosition': rig.data.pose_position,
    'drivers': len(rig.animation_data.drivers) if rig.animation_data else 0,
    'bones': [{'name': b.name, 'parent': b.parent.name if b.parent else None,
               'deform': b.use_deform, 'restMatrix': [list(r) for r in b.matrix_local],
               'constraints': [(c.name, c.type) for c in rig.pose.bones[b.name].constraints]}
              for b in rig.data.bones],
    'materials': [{'name': m.name, 'nodes': [(n.name, n.type) for n in m.node_tree.nodes] if m.use_nodes else []} for m in obj.data.materials],
    'actions': [{'name': a.name, 'range': list(a.frame_range)} for a in bpy.data.actions],
}
(root / '.cache/hand-rig.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
