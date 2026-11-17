# Firetruck water-cannon video controller

Form and asset commit approved by the user on 17 November 2026. Optimized on
request to **3222 triangles including the screen and roof cradle**, with a hard
4000-triangle limit. `.blend` and `.glb` use Git LFS. Gameplay integration has
not started.

- Editable source: `firetruck-controller-v1.blend`.
- Interchange model: `firetruck-controller-v1.glb`.
- Geometry/material inventory: `manifest.json`.
- Scope: Firetruck 407 only. No S.W.A.T. model or gameplay integration.

The approved form uses a cool graphite housing, near-black rubber bezel and
symmetric pistol grip, an index-finger trigger and a separate passive roof
cradle. There is no rotary knob. Housing: 166 × 90 × 134 mm; overall device
height about 232 mm. Convex active CRT area: 122 × 91.5 mm, aspect 4:3.

The original review mesh had 12286 triangles. Reduced bevel segments, small
fastener sides, grip rings and CRT tessellation cut the complete asset by 73.8%.
The silhouette, object dimensions, hinge motion, screen UVs and mounting split
are preserved. No blind whole-model decimation was used.

Units are metres. Blender axes: X right, Y aiming forward, Z up; the screen
faces the operator along -Y. `MonitorDevice` and `RoofCradle` are separate
assemblies. `TriggerPivot` rotates about local X, 0 to -12 degrees in the
`WaterTriggerPull` animation. `ScreenFrame`, `AimFrame`, `DockDeviceTarget`,
`CeilingAttachment` and hand anchors are explicit empties for later integration.
Hand anchors do not imply a completed hand animation or validated Quest grip.

`CurvedCRTScreen` has its own `VideoUV` and material
`CRT_VideoSurface_REPLACE_WITH_CAMERA`. Its sample image is a self-authored
Blender scene, packed into the source and GLB. It is only a preview of a color
camera feed with two reticles; replace it with the live camera at runtime.
No original game models, textures or footage are included.

The source separates the asset, studio and synthetic camera scene. The GLB
contains only the asset and trigger animation; studio cameras/lights and the
synthetic city are excluded. Ceiling/mirror shapes used in a review render
are schematic preview geometry, not the game's Firetruck interior.

Build with `tools/build_firetruck_controller.py` in Blender 4.5 (add
`-- --no-render` to skip product renders), verify with
`tools/verify_firetruck_controller_blender.py`, and render additional angles
with `tools/render_firetruck_controller_review.py`. Renders and verification
output stay in `build/firetruck-controller-review/`. Game code, APKs and hand
libraries have not changed.
