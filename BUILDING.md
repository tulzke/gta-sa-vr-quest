# Building

## Windows

For routine updates of the already installed personal build, run:

```powershell
./tools/deploy-quest.ps1
```

This incrementally builds native code and the Java loader, repackages/signs only
base + ARM64 APKs and uses an Android partial update to retain installed data,
locale and density splits. It copies top-level saves/settings (`*.b`, `*.ini`,
`*.set`) without hash comparisons, leaves the game stopped and does no extra
post-install audit. Output and the single log live in `build/quest-deploy`.
Use `-BuildOnly` to package without touching the Quest; `-Serial` selects a
device. `-NoBackup` is an explicit opt-out from the automatic backup.

The first fast run seeds APK templates and the HLOD pack from the existing
`build/vehicle-test-area` build; use `-SeedBuild <prepared-build-directory>` to
specify another compatible personal build before initializing the cache.
The existing key is reused from `%USERPROFILE%\AppData\Local\GTASAVRBuilder\signing`;
`-Keystore` overrides its path. No key is generated and no uninstall is attempted.

Use `./tools/deploy-quest.ps1 -Full` for initial/full installation or changes to
game/audio/hand assets, APK manifests, OpenXR or the embedded HLOD pack. This
explicit mode delegates to the full builder below and runs its validations.
`-GamePackage` and `-AudioSource` override full-mode inputs; defaults use the
prepared seed's `out` directory and `.cache/package-source/audio`.
`-Developer` applies to fast builds; for full developer builds use `tools/build.ps1`.

Run `BUILD_AND_INSTALL.bat`, or prepare a build without a Quest:

```powershell
./tools/build-and-install.ps1 `
  -GamePackage "D:\Backups\GTA-SA-Play-export" `
  -AudioSource "D:\Mods\gta-sa-ps2-style-mod-pack_1786856007_737162.7z" `
  -BuildOnly
```

The builder downloads pinned tools, validates the game and audio inputs, builds
ARM64 native code and the Java loader, and signs the assembled APK split set.
Outputs are stored under `C:\SAVRBuild`, or the directory passed with `-WorkDir`.

## Linux and macOS

Install Bash, Python 3.10 or newer, curl, tar with xz support, and unzip. Run:

```bash
bash BUILD_AND_INSTALL.sh \
  --game-package "$HOME/Backups/GTA-SA-Play-export" \
  --audio-source "$HOME/Downloads/gta-sa-ps2-style-mod-pack_1786856007_737162.7z" \
  --build-only
```

Omit `--build-only` for installation. Output runs are stored under
`~/SAVRBuild/runs`, or the directory passed with `--work-dir`.

## Inputs and signing

The game input must contain the official Google Play `com.rockstargames.gtasa`
version `2.11.311` (`4234641`), its ARM64 library, the `data_main` split, and any
installed locale/density splits. Selecting `base.apk` includes sibling splits.
The builder verifies the official signer and supported `libGame.so` hash.

The audio input must match `tools/audio-reference.json`. Only validated audio
files are copied. Aircraft HLOD is generated locally from the supplied game
assets during the build.

Keep the signing key at `%LOCALAPPDATA%\GTASAVRBuilder\signing` on Windows,
or `$XDG_DATA_HOME/gtasavr-builder/signing` (`~/.local/share/gtasavr-builder/signing`
by default) on Linux/macOS. Updates require the same key as the installed build.

## Developer build

Public builds use `SAVR_DEV=OFF`. Pass `-Developer` to `tools/build.ps1`,
`--developer` to `tools/build.sh`, or `-DSAVR_DEV=ON` directly to CMake for the
profiler and developer capture support. Use **VR MENU > PROFILER** to toggle it.

## Reset settings

`RESET_VR_SETTINGS.bat` and `RESET_VR_SETTINGS.sh` remove only:

```text
vr_appearance.ini
vr_basketball.ini
vr_calib.ini
vr_calib.ini.tmp
vr_driving.ini
vr_graphics.ini
vr_holsters.ini
vr_hud.ini
vr_jetpack.ini
vr_locomotion.ini
vr_modelsets.ini
```

Both scripts require confirmation. Saves, game assets, and installed APKs are
preserved. Build-only commands perform no headset installation or game launch.


## Vehicle calibration editor

Open **VR MENU > VEHICLE SETTINGS > VEHICLE DEBUG** and switch it **ON**.
Close the menu: door, wheel and seat markers are available together. Bring an
empty hand to a marker and squeeze its trigger to drag; release to save. With
the wheel marker held, squeeze the other trigger on the opposite side of the
wheel and move the hands together or apart to change its radius. Release the
second trigger to return to moving the wheel with one hand. Select **OFF** to
leave the editor. The switch resets to OFF at process startup and is available
in public and developer builds.

DOORS edits the existing supported car-door arrows. Outside handles are stored
per model and door; the driver's inside handle has a separate correction.
Corrections follow the moving door panel and also affect the normal grip hit test.
WHEEL edits the centre and radius of a car wheel, boat helm or plane yoke from
the driver's seat in first person. SEAT marks the neutral camera position where
the player's head appears on entry and edits the model's camera offset for the
current driving mode. If the marker coincides with the headset, lean away from
the neutral head position to see and grab it. Seat dragging previews
the target and applies the offset on release; opening a menu or losing tracking
cancels that preview. Wheel/door changes apply live and save on release or
interruption. Wheel radius and orientation remain adjustable in CONTROL CALIBRATION.

The editor suppresses weapon/holster actions and accelerator/brake input; normal
door grip, wheel steering and other controls remain available. Cockpit
editing stops if the vehicle moves, the driver changes, tracking/focus is lost
or a menu opens. It does not freeze the world. Finish parking before editing.
Bikes, helicopters, special doors and third-person cockpit editing are not
supported by this editor.

All model corrections live in `vr_driving.ini`. Existing seat and wheel keys are
retained; additive `DoorCalib.<model>.<door 0..3>.<inside 0..1>.<axis 0..2>`
keys store panel-local centimetres (X/Y/Z). Missing door keys mean zero offset.
Save uses a temporary file and replacement; a failed replacement preserves the
previous config.

Host checks: `tools/test-vehicle-doors.py`,
`tools/test-driving-wheel-pose.py`, `tools/test-driving-mission-inputs.py`.
Quest validation is required for marker visibility in both eyes, reach, seat-camera transition
comfort, restart persistence and normal interactions after leaving the editor.

## Vehicle test area (prototype)

Diagnostics are enabled in normal builds: `vr_vehicle_test_area.log` in the
game's external `files` directory, also mirrored to Android logcat under SAVR.
The log records state/page changes, resource requests/readiness/timeouts,
model IDs, streaming states/flags/archive sectors, collision availability,
pool capacity and floor/spawn failures. Slow loads are sampled after 5 seconds
and every 10 seconds thereafter. At 512 KiB it rotates to `.log.previous`.
Entries are closed immediately so an error survives a game restart.

Select **VR MENU > VEHICLE SETTINGS > VEHICLE TYPE: CAR > VEHICLE TEST AREA**,
then close the menu. Entry is on foot, outdoors in free roam, outside missions
and cutscenes. Preparation is asynchronous; the menu reports resource or floor
validation failures. The area is available in normal and developer builds and
starts OFF each process. VEHICLE DEBUG remains an independent editor switch.

The prototype creates a roughly 108 x 72 metre platform over the ocean at
(0, -2800, 80), inside the engine's world sectors. It uses collision-bearing
copies of stock model 3095 from the user's game. The native collision collection
is retained for the session; floor rays must hit the owned tiles before entry.
No game assets or map files are modified. Actual floor physics and rendering
still require Quest validation.

Ten automobile/monster models are parked in two rows of five, ordered by model
ID. The last page may contain fewer models. Bikes, quads, boats, aircraft,
trains and trailers are excluded. This catalogue includes special and RC cars;
their presence does not add editor support for special doors or cockpit types.
Each row runs from lower to higher ID from west to east; the north row contains
the first five models. **TEST PAGE** shows the page and ID bounds and also
accepts left/right menu input.

In the area, green arrows follow a body-style horizontal frame, about 50 cm to
either side, 25 cm forward and 12 cm above the head. Bring either empty hand to
an arrow and squeeze its trigger once. Release before another page request.
Both arrows are available: left on the first page wraps to the last, and right
on the last wraps to the first. Arrows are
hidden outside the area, in menus, in a vehicle, during door/editor grabs and
on tracking loss. Navigation does not use the weapon holster marker channel.

The TEST PAGE menu row accepts left/right or confirm (next page). Close the menu
to start loading; the row shows this prompt and then the page being loaded.
Page changes retain each newly loaded model immediately, preventing streaming
from evicting it while the other models load. Vehicles have a five-second load
window: missing models leave their original parking slots empty, without shifting
later cars or rejecting the whole page. The log records each skipped ID, slot
and reason; TEST PAGE shows the skipped count. Changes
require all area cars to be empty and no editor/door grab; occupancy is checked
again after loading. Old owned instances are then removed, calibration is
saved and the new page is created. Invalid vehicle collision, factory failure
or pool exhaustion also leaves that slot empty. Object ownership includes pool identity and slot
generation, so a reused slot is never treated as an owned car.

Pending models use native mission-required asynchronous streaming requests.
There is no forced synchronous load. Temporary request flags
are released after acquiring model references, or on cancellation/timeout;
pre-existing mission flags are preserved. The platform and its collision remain
mandatory: failure to load/validate the floor still cancels entry.

Select **VEHICLE TEST AREA** again and close the menu to return. Leave the car
first. The original scene is streamed and its ground is checked before the
platform is removed. If that check fails, the platform remains and the menu
asks you to retry. Staying outside the platform on foot for 0.75 seconds allows
one rescue to its centre; a second escape requests a return to the game, avoiding
a teleport loop. Position/vertical velocity and rescue decisions are logged.
Normal death/respawn and game restart end the session without moving the new
player to an old session's coordinates. Entry enables the editor; exit restores
its previous state. Wheel/door/seat corrections continue to use `vr_driving.ini`.

**Exit the area before saving the game.** The native save entry is guarded
while the temporary session is active to prevent saving the player off-map or
serializing its temporary vehicles. This does not block calibration saves.
Unsupported save/restart entry prologues disable the area instead of installing
an unchecked hook.

Host coverage: `tools/test-vehicle-test-area.py`, plus the door, wheel-pose,
driving-input and holster checks above. Before using it to calibrate the full
catalogue, validate platform stability, model streaming, door clearance, both-eye
arrow visibility, reach, performance, return and restart behavior in Quest.
