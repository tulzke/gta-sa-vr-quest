# UltimateXR hand assets

The hand geometry and source mask textures used to build these runtime assets
come from [VRMADA/ultimatexr-unity](https://github.com/VRMADA/ultimatexr-unity),
commit `985f20c`.

Used source assets:

- `Runtime/Art/Avatars/BigHands/BigHandLeftGeo.fbx`
- `Runtime/Art/Avatars/BigHands/BigHandRightGeo.fbx`
- `Runtime/Art/Avatars/BigHands/HandPoses/Default.asset`
- `Runtime/Art/Avatars/BigHands/HandPoses/Grab.asset`
- `Runtime/Art/Avatars/BigHands/HandPoses/Wave.asset`
- `Runtime/Art/Avatars/BigHands/Textures/BigHandsTex_Mask1.png`
- `Runtime/Art/Avatars/BigHands/Textures/BigHandsTex_Mask2.png`
- `Runtime/Art/Avatars/BigHands/Textures/BigHandsTex_Mask3.png`

The hands-only Blender source is `source/Hands.blend`. It contains HandRight,
its deform rig and all fourteen authored Actions. The original local mixed
scene is not published; helper weapon meshes, cameras and unused data have
been removed from the published copy. The original scene is never overwritten.

`tools/export_hand_glb.py` exports one skinned mesh, all weights, inverse bind
matrices and named bone-animation clips to `skeletal/hands.glb`. Constraints
are baked to bone TRS if present. No per-frame geometry is used at runtime.
The old UXRH hand assets and conversion tools have been removed.

`BigHandsAlbedo.png` is an offline Type 3 skin-colour approximation made from
the original masks; it does not include Unity or Shader Graph code. It is
embedded in the GLB material. The original MIT license is retained in
`ULTIMATEXR_LICENSE.txt`.

The Blender source, GLB and PNG are versioned using Git LFS. Run `git lfs pull`
after cloning. See `ANIMATIONS.md` and `../../docs/hand-skeleton.md` for gesture
mapping, export and validation.
