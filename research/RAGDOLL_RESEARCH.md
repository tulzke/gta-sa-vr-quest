# NPC ragdoll feasibility and licensing

Checked: 2026-10-30. Research only; no third-party mod installed or bundled.

## Conclusion

No ready-made Android ragdoll was found with both confirmed compatibility
with retail GTA SA 2.11.311 ARM64 and verified permission to adapt and ship it.
This is a search result, not proof that none exists.

A real NPC ragdoll is technically plausible through an original native adapter
using Jolt Physics (MIT). This has not been prototyped, built, benchmarked or
tested on Quest. Animation replacements are not an equivalent implementation.

## Candidates

- **Realistic Ragdoll Physics, LibertyCity file 243394.** Inspected the exact
  user-supplied `realistic-ragdoll-physics_1790101026_579181.zip` without
  executing or installing anything. SHA-256:
  `46f20b2112b59c0726b04244a93831066a7baab6e58a19ef6f13a1f3e698caf5`.
  The archive contains exactly one file: `ped.ifp`, 2,422,244 bytes, with an
  `ANP3` animation-bank header. Embedded animation names include `FALL_back`,
  `FALL_front`, `BIKE_fall_off`, and `KO_shot_front`, as well as ordinary
  locomotion/idle names. No native plugin, script, source, README, credits
  document, or license is included. This download supplies animations, not
  a runtime ragdoll solver. Which individual animations differ from stock
  has not been checked. Compatibility and visual quality on Quest remain
  untested. No permission to adapt and redistribute was established by the
  archive or listing; do not bundle on the basis of attribution alone.
  https://libertycity.ru/files/gta-san-andreas-ios-android/243394-realistichnaja-rjegdoll-fizika.html
- **Ragdoll Style Animations - Android, YB Ultimate.** The author's listing
  instructs users to replace `files/anim/ped.ifp`. This is an animation pack,
  not evidence of a runtime rigid-body solver. No explicit adaptation and
  redistribution license was found on the listing; archive terms have not been
  inspected. Compatibility with our exact game build is untested.
  https://www.gtainside.com/sanandreas/mods/77315-ragdoll-style-animations-android/
- **Ragdollv2 Anim for Android, arshi678.** The author's listing replaces
  `ped.ifp` and `muscular.ifp`, and mentions an optional `free_fall.csa` in a
  comment. No verified native physics implementation or explicit redistribution
  grant was established. Not a confirmed solution for our target.
  https://www.gtainside.com/sanandreas/mods/67523-ragdollv2-anim-for-android/
- **Ragdoll Bullet Physics + Fix, madleg and contributors.** The release page
  maintained by fix contributor Junior_Djjr describes real NPC physics, PC
  Compact EXE compatibility, and remaining floor/vehicle penetration problems.
  This is a PC implementation requiring a port. No explicit license for the
  whole mod or complete licensed source was found in the consulted sources.
  The Bullet library's license does not license the mod's adapter or fixes.
  https://www.mixmods.com.br/2021/09/mod-ragdoll-bullet-physics-fix-fisica-realista/
- **GTA Trilogy - Jolt Physics, MrRealistic.** The author describes reactive
  physics, a standalone ASI version, and plans to open-source the mod when
  finished. Those statements do not grant present rights to adapt its code.
  No Android 2.11.311 ARM64 release or explicit reuse license was verified.
  https://www.patreon.com/MrRealistic/posts/159598382
  https://www.patreon.com/MrRealistic/posts/sa-gta-trilogy-163517399

Do not treat licenses shown by unrelated reuploaders as permission from the
original author. Free downloads and attribution alone do not establish a
right to modify and redistribute. Obtain explicit permission from all relevant
rightsholders before incorporating a mod with unresolved terms.

## Licensed engine options

- **Jolt Physics:** upstream explicitly supports Android ARM64 and supplies
  ragdoll pose and constraint APIs. MIT permits modification and distribution,
  including commercial use, with its copyright and permission notice retained.
  This fits this project's MIT original code. It does not grant rights to
  MrRealistic's mod, game assets, or any separately sourced rig/animation data.
  https://github.com/jrouwe/JoltPhysics
  https://github.com/jrouwe/JoltPhysics/blob/master/LICENSE
  https://github.com/jrouwe/JoltPhysics/blob/master/Jolt/Physics/Ragdoll/Ragdoll.h
- **Bullet:** core library is zlib licensed; altered sources must be identified,
  origin must not be misrepresented, and the source notice must remain.
  Upstream excludes `Extras` and `examples/ThirdPartyLibs` from this blanket
  license. Audit any imported files separately. Also a possible engine for an
  original adapter, without reusing madleg's unverified mod code.
  https://github.com/bulletphysics/bullet3/blob/master/LICENSE.txt

## Project integration work

The project already builds a native C++20 library for Android ARM64 and resolves
`RpHAnimIDGetIndex`, `RpHAnimHierarchyGetMatrixArray`, and
`CWorld::ProcessLineOfSight` in `native/src/Symbols.cpp`. These are useful
starting points, not a complete physics bridge. A line query alone is not
sufficient for stable body collisions.

Suggested first milestone: opt-in death ragdolls for ordinary nearby NPCs,
with a small configurable active count. Initially exclude the player,
cutscenes, mission-critical actors, and actors seated in vehicles.

Required work includes bone mapping and pose transforms; original capsule and
joint definitions; collision geometry extraction and streaming; moving vehicle
proxies; impulses and timestep handling; synchronization with skin updates;
safe cleanup when actors/models unload; and preservation of death/mission
semantics. Existing animation and collision code must not fight the solver.

Validate stairs, slopes, walls, impacts, explosions, vehicles, interior changes,
save/load, NPC deletion, and mission progression on Quest. Measure CPU and
frame-time cost before increasing the active count. Pin an audited upstream
revision and add its license to third-party notices when actually integrating.
