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

The FBX meshes are converted offline to the `UXRH` runtime format. The open,
grip, trigger and combined poses are baked from UltimateXR's authored finger
poses. `BigHandsAlbedo.png` is an offline Type 3 skin-colour approximation made
from the original masks; it does not include Unity or Shader Graph code.

The user's nine Blender Actions, including `GripPistol`, `GripPistolOffhand`,
`Fist` and `GripPistolShoot`, can be sampled with `tools/export_hand_actions.py`.
See [ANIMATIONS.md](ANIMATIONS.md) for the local source, mirrored exports,
validation and the distinction from the existing runtime blend corners.

`BigHandLeftPalm.uxrh` and `BigHandRightPalm.uxrh` are basketball-only meshes
with UltimateXR's authored `Wave` pose baked as the open-palm pose. They do not
replace the ordinary controller hand meshes. Rebuild them with
`tools/build_ultimatexr_palm_hands.py` after converting the official BigHands
FBX files to embedded glTF.

`BigHandLeftPinch.uxrh` and `BigHandRightPinch.uxrh` are custom pinch poses made
by the project user from the UltimateXR hands. See [PINCH.md](PINCH.md) for
the contextual animation API. Export the posed mesh from Blender to
GLB with the armature modifier applied, then run
`python tools/build_pinch_hand.py <posed.glb> assets/vrhands/BigHandRight.uxrh <endpoint.uxrh>`.
The converter checks UV correspondence and topology before writing the pose.
Bake the four joint-aware transition frames using the original `.blend`:
`blender -b <hand.blend> --python tools/build_pinch_transition.py -- assets/vrhands/BigHandRight.uxrh <endpoint.uxrh> assets/vrhands/BigHandRightPinch.uxrh`.
The transition builder estimates joint rotations using the source mesh weights;
the fourth frame retains the exact authored pinch endpoint.
For the left hand, export `HandLeft`, pass `--side left` to the converter and
pass `left` after the transition builder's three UXRH paths.

UltimateXR is distributed under the MIT License. See
`ULTIMATEXR_LICENSE.txt` in this directory.

The runtime texture is the raw RGBA8 byte stream of `BigHandsAlbedo.png`:

```python
from PIL import Image
Image.open("BigHandsAlbedo.png").convert("RGBA").tobytes()
```

Expected `BigHandsAlbedo.rgba` SHA-256:
`46BA8B2F08CE1CA420537E060F64BE139DD39A22357D4EA5B9B7B69DA2BA2AD4`.
