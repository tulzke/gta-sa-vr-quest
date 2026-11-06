# GTA San Andreas VR for Quest

This is a fork of [GTA San Andreas VR for Quest](https://github.com/dubrovskiy-yevhen-stakelogic/gta-sa-vr-quest)
by **Yevhen Dubrovskyi**, the original author of the VR mod. This repository
continues that work with additional immersive interactions, controls, and tools.
It includes source code and build tools; a legally obtained copy of the game
and the required external assets must be supplied separately.

**This fork is currently WIP (Work in progress).** Steering-wheel positions,
seat positions, and other interactive elements have not yet been calibrated
for most cars.

## Features added in this fork

- **Physical car doors:** grab, pull, and push doors from outside or from your
  seat. Interior door panels provide larger grab areas, and fully opened doors
  stay open for boarding.
- **Immersive vehicle entry and exit:** use directional stick holds toward
  seat and exit markers to board, leave, or switch seats without the standard
  entry animation. Four-seat cars, supported doorless vehicles, motorcycles,
  bicycles, and quad bikes have adapted interactions. High-cab exits use the
  game's native clearance and placement checks.
- **Gesture-based vehicle theft:** follow punch, grab, and pull prompts to
  remove occupants. Selected passenger interactions use a head push toward the
  dashboard. Motorcycles and bicycles have their own jacking interactions;
  unfamiliar car drivers can flee or fight back when you enter as a passenger.
- **Climbing into high vehicles:** grab vertical pipes and pull yourself up
  toward the cab, including holding on while the vehicle moves. Pipe placement
  is configurable for supported models; there are no standing platforms.
- **Hands-on driving:** hybrid hand-lean and handlebar steering for motorcycles
  and bicycles, handlebar control for quad bikes, motorcycle lifting and
  stabilization, and per-model steering-wheel position and tilt calibration.
- **Manual ignition:** grab and turn a physical key to start or stop the
  engine in supported motor vehicles, with contextual hand poses and hints.
- **Physical radio control:** turn a modeled radio knob to change stations,
  with discrete clicks and haptic feedback in cars and on motorcycles.
- **Rear-view mirror:** an optional rear camera view with LOW and MEDIUM
  quality, adjustable field of view, and per-car mirror/camera placement.
  The simplified view omits pedestrians, water, and some scene effects.
- **Shared interaction targeting and calibration:** nearby cabin controls are
  selected and highlighted to reduce overlapping grabs. VEHICLE DEBUG lets you
  adjust supported doors, seats, steering wheels, keys, radio controls, mirrors,
  and climbing pipes, saving settings per vehicle model.
- **Two holsters with radial weapon selection:** choose weapons for the two
  holsters through a radial menu. In BODY mode, the holsters rotate to follow
  HMD turns through an estimated upright torso frame, while pausing during
  reaches. Configurable socket markers and return rings help locate holsters
  and put held weapons away.
- **Physical melee combat:** motion-based punches distinguish
  jabs, hooks, and uppercuts, with combo reactions, hit sounds, and haptics.
  A two-hand inward-facing guard blocks attacks and slows movement; red damage
  and gray block halos provide peripheral feedback.
- **Tracked hands and room-scale movement:** authored pointing, fist, pinch,
  and firearm hand poses, plus improved alignment between head movement and
  the player body, including movement near collision boundaries.
- **Flat-screen placement:** game menus and cinema-mode cutscenes open in front
  of the current view, then remain anchored in space until reopened.
- **Local development workflow:** incremental Windows build/deploy with save
  and settings backup, plus host tests for the added interaction systems.

Coverage and calibration vary by vehicle model. Some features are still being
refined; handling, reach, and comfort need testing in Quest. See
[BACKLOG/BACKLOG.md](BACKLOG/BACKLOG.md) for remaining work and
[completed task notes](BACKLOG/ARCHIVE) for implementation details and limits.

## Requirements

- A complete Google Play GTA San Andreas `2.11.311` ARM64 APK split set.
- The supported audio archive `gta-sa-ps2-style-mod-pack_1786856007_737162.7z`,
  or its extracted audio directory. Select **Original plan mod pack** on the
  [download page](https://libertycity.net/files/gta-san-andreas-ios-android/241069-gta-sa-classic-avanced-mod-pack.html).
- Windows 10/11, Linux x86_64, or macOS, with an internet connection for tools.
- Quest Developer Mode and authorized USB debugging for installation.

## Build and install

1. Export the installed Google Play game from an Android device with
   `EXPORT_PLAY_APKS.bat`. Keep `base.apk` and every exported split together.
2. Run `BUILD_AND_INSTALL.bat` on Windows or `bash BUILD_AND_INSTALL.sh` on
   Linux/macOS.
3. Select the complete APK export and the supported audio input.
4. Connect Quest, authorize USB debugging, and follow the installer prompts.

The installer leaves the game stopped. Preserve the personal signing key for
future installations. See [BUILDING.md](BUILDING.md) for command-line options.

For subsequent local Windows deployments from a prepared checkout, run:

```powershell
./tools/deploy-quest.ps1
```

This incrementally builds and updates the installed game while preserving its
game/language splits, and backs up accessible saves and settings before
installation. Use `-BuildOnly` to build without a headset. For a first install
or changes to game data, audio, hand assets, manifests, OpenXR, or HLOD, use
`-Full` or the full builder above. The deploy log is
`build/quest-deploy/deploy.log`.

## Proper Fixes

1. Supply an SA Mobile - Proper Fixes archive or extracted directory containing
   `com.rockstargames.gtasa/files`.
2. Run `INSTALL_PROPER_FIXES.bat` and select that input.
3. Start the game, open **VR MENU > MODEL SET**, enable **PROPER FIXES**, and
   fully restart the game.

The script prepares and verifies the pack under `files/optionalmods/properfixes`.
It retains the input pack and its credits. Set **PROPER FIXES** to **OFF** and
restart to use the original game assets. `DISABLE_OPTIONAL_MODS.bat` disables
optional model sets from the computer.

To prepare the pack without installing it:

```powershell
./tools/install-optional-mod.ps1 -Kind properfixes -Archive <path> -PrepareOnly
```

## HD weapons

Run `INSTALL_HD_WEAPONS.bat` with a compatible mobile weapon pack, then enable
**HD WEAPONS** in **VR MENU > MODEL SET** and fully restart the game. Weapon
packs are supplied separately.

## Vehicle setup and debugging

**VEHICLE DEBUG** is the in-world editor for positioning and calibrating vehicle
interaction elements. **VEHICLE TEST AREA** is a dedicated area for testing
and adjusting different car models.
It enables the editor and supplies vehicles in pages, so both modes can be used
together to configure steering wheels, seating positions, doors, and other
interactive elements.

See [VEHICLE_DEBUG.md](VEHICLE_DEBUG.md) for controls, editable elements,
test-area navigation, and how calibration settings are saved.

## Settings

In **VR MENU > HOLSTER LOADOUT > HOLSTER FRAME**, select **BODY** to let an
upright estimated torso follow sustained/large head turns and room movement.
Nearby hands pause its yaw while reaching. **RECENTERED** retains the previous
frame and remains the default for existing settings. BODY currently applies to
ordinary on-foot movement; vehicles and special movement retain their existing frame.

A held weapon leaves a ring with an inward chevron at its return socket.
The global marker toggle and each socket's SHOW/HIDE/OFF setting control these
hints; dropped weapons do not leave a return ring.

To tune the pause near a resting hand, edit `vr_holsters.ini` and restart the game:

```ini
body_reach_idle_ms 1200
body_reach_motion_cm 3
```

After 1200 ms without at least 3 cm of controller displacement, a nearby hand
stops holding the torso yaw. Real hand movement or a grip action re-arms reach
protection; a socket moving past a stationary hand does not. The idle timeout
accepts 0–10000 ms (0 restores unlimited protection); movement accepts 1–20 cm.
The existing 0.2-second exit grace also applies when the last protecting hand
becomes idle.

- `RESET_VR_SETTINGS.bat` or `bash RESET_VR_SETTINGS.sh` restores default VR
  settings after confirmation, preserving saves and game assets.
- `tools/export-savr-settings.ps1` exports VR settings to the computer.
- `COLLECT_RENDER_REPORT.bat` collects a render diagnostic report.

## Bike lean steering

In immersive handlebar driving, **BIKE STEERING > LEAN + BARS** is enabled by
default. At parking speed, turn the bars normally. As forward speed increases,
tilt the line between the two held controllers to steer into a curve (raise
the right grip to lean left, or the left grip to lean right). Capture a
comfortable neutral by releasing and grabbing the bars. Lean authority blends
in over 0.18 seconds after the second hand grabs; one hand and reverse retain
handlebar steering. This also applies to bicycles in HANDLEBAR + R2 mode.
Bicycle MOTION and ordinary stick driving retain their controls.

**LEAN SENSITIVITY** accepts 25–200%; at 100%, the roll dead zone is 2 degrees
and a 25-degree hand tilt requests full steering. Select **BARS ONLY** to
restore the previous handlebar solver. Both settings persist in `vr_driving.ini`:

```ini
BikeLeanSteering=1
BikeLeanSensitivityPercent=100
```

This controls the turn through GTA's existing physics; the bike angle still
depends on speed, traction and handling. **VISUAL BIKE LEAN** and **LOCAL HORIZON**
remain separate display preferences. Moving both hands sideways without tilting
them does not steer. Wheelie/forward-lean gestures and throttle are unchanged.
The default speed blend spans 0.04–0.12 in GTA velocity units; it only applies
while moving forwards. Quest handling and comfort still need a riding check.

## Developer tests

Run `python tools/test-vehicle-doors.py` for the production door solver and
engine-adapter host tests (fake engine memory, real selection/state logic).

Developers can run `python tools/test-holsters.py` for host tests of the body,
marker geometry, production grab/return branches, camera transforms and settings.
On Windows the runner discovers Visual Studio C++ tools, or uses `VS_VCVARS64`.
On Linux/macOS it uses `CXX` or `c++` with C++20 support.

Run `python tools/test-bike-steering.py` for production lean-input maths,
handover, settings and motorcycle/bicycle routing tests. Developer builds log
`[driving.lean]` speed, bar input, hand roll, blend and resulting steering.

## License

See [LICENSE](LICENSE), [NOTICE.md](NOTICE.md), and the licenses alongside
third-party files. Game APKs, game assets, audio packs, and signing keys are
provided locally and are not included in this source kit.
