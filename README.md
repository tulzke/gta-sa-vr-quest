# GTA San Andreas VR for Quest

Version `0.3.1 alpha`. Source code and build tools for the Quest VR mod.

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
