# fzerogx-recomp

A static recompilation of *F-Zero GX* (Nintendo GameCube, 2003) to native code.

The game's PowerPC code -- the executable and the relocatable modules it loads at run
time -- is translated ahead of time into C, then compiled and linked against a runtime that
stands in for the GameCube hardware (graphics, audio, DVD, controllers, memory card). The
result is a native program that runs the original game logic without an emulator's CPU
core. The recompiler and runtime are [gcn-recomp](https://github.com/ethannicholas/gcn-recomp),
shared with [bluestorm-recomp](https://github.com/ethannicholas/bluestorm-recomp) and
[prime-recomp](https://github.com/ethannicholas/prime-recomp).

> **This repository contains no game code or data.** You must supply your own disc image,
> dumped from a copy of the game that you own. The build process reads the code out of
> *your* image and recompiles it locally. No game code or assets are distributed here; what
> this repository holds about the game is a table of its function addresses and names.

## License

The code in this repository and in `gcn-recomp` is released under the [MIT License](LICENSE).

## Legal

- *F-Zero GX*, GameCube and all related names and marks are property of Nintendo.
  This project is not affiliated with or endorsed by Nintendo or SEGA.
- This repository contains only original source code plus metadata describing code layout
  (function addresses and names in `analysis/`, produced with
  [decomp-toolkit](https://github.com/encounter/decomp-toolkit)). It contains no
  copyrighted game code, assets, BIOS/IPL files or DSP microcode.
- You need a legally obtained copy of the game. Dump it from your own disc using your own
  GameCube/Wii (e.g. with [CleanRip](https://wiibrew.org/wiki/CleanRip)). Do not download
  game images, and do not share the image, anything extracted from it (`build/main.dol`,
  `build/rels/`), the generated C code (`build/gen/`), built binaries, or memory card
  files, since all of these contain or are derived from the game.

## Supported version

Only the North American release is supported:

| Title | Disc ID | `main.dol` SHA-1 |
|---|---|---|
| F-Zero GX (USA) | `GFZE01` | `421c88106697d3275a3fc26fb7a01bf6d816b271` |

The build verifies this hash and stops with an error for any other version.

## Status

Early. The game runs from a blank memory card through the menus into a Grand Prix race;
there is no audio yet. VR and motion-rig support are written but not yet tried in a
headset or on a rig. See `docs/dev/status.md` for how far it gets and what is known not
to work.

## Building

### Requirements

**macOS (Apple Silicon)**

- Xcode (or the Command Line Tools), CMake 3.20+, Ninja, Python 3
- SDL2

```sh
brew install cmake ninja sdl2 python
```

**Windows (x64 or ARM64)**

- Clang, CMake 3.20+, Ninja, Python 3
- The Windows SDK and MSVC headers/libraries (the "Desktop development with C++" workload of
  Visual Studio or the standalone Build Tools): clang targets the MSVC ABI and uses them

```powershell
winget install Kitware.CMake Ninja-build.Ninja Python.Python.3.13 LLVM.LLVM Microsoft.VisualStudio.2022.BuildTools
```

**Quest 3**: see [Running on a Quest 3](#running-on-a-quest-3).

### 1. Provide your game image

Dump your disc, then place the image in `rom/` as an uncompressed **`.iso`** or a **`.ciso`**:

```
rom/game.iso
```

If your dump is in another format (`.rvz`, `.gcz`), convert it first, for example with
Dolphin (right-click the game → *Convert File…* → *Uncompressed Disc Image* or *CISO*),
`dolphin-tool convert`, or [`nodtool`](https://github.com/encounter/nod).

The build uses the first `.iso` or `.ciso` it finds in `rom/`. To use an image elsewhere,
pass `-DGAME_ISO=/path/to/game.iso` when configuring.

### 2. Build

```sh
./build.sh          # macOS
```

```powershell
.\build.ps1         # Windows
```

The first build fetches the `gcn-recomp` submodule if a plain `git clone` left it empty
(`git clone --recursive` avoids that). It also builds and briefly runs the game once
without its main module, which the game assembles in memory at boot, to take that module
from the running game; this happens once per build directory.

### 3. Run

```sh
./build/fzerogx            # uses the configured image / rom/*
./build/fzerogx path/to/game.iso
```

On Windows the executable is `build\fzerogx.exe`.

Run it from the repository root: the memory card is created in `saves/` relative to the
current directory. Every run also records the controller input to `saves/inputs/<timestamp>/`
so that a route can be played back with `--replay=<that directory>`; `--no-input-log` turns
that off.

## Playing in VR on a PC

On Windows, `--vr` plays the game on a PC VR headset through OpenXR, using whichever runtime
is set as active: SteamVR, Virtual Desktop, Meta Quest Link, or any other that supports
OpenGL.

```powershell
.\build\fzerogx.exe --vr
```

Connect the headset and start its PC software first. Menus and the course flyovers are on
a flat screen in front of you; a race is all around you, in stereo 3D, from the grid to the
finish, and goes back to the flat screen while paused. Clicking the right thumbstick in a
race switches between the chase camera and the game's own cockpit view (it presses the
D-pad for you: up for the cockpit, down to leave it). The window on the desktop mirrors the
headset. A gamepad and the keyboard work alongside the headset's controllers.

Settings are read from a `vr.txt` in the directory you run the game from; see
[Tuning the VR view](#tuning-the-vr-view). To use a runtime other than the active one, point
`XR_RUNTIME_JSON` at its manifest.

## Motion platforms

`--telemetry` sends your machine's motion to a motion simulator, in the format of
[SimRacingStudio](https://www.simracingstudio.com/)'s custom game API: UDP packets to
`127.0.0.1:33001`, where SimRacingStudio listens by default. `--telemetry=host:port` sends
them elsewhere. SimRacingStudio shows the game as *F-Zero GX*.

The platform moves only during a race: it eases to rest in the menus, while paused and at
the end of a race, and eases back in when one starts or you resume. Pitch and roll are your
machine's against the track under it, since that is where gravity points in this game, even
on a wall or round a pipe; surge, sway and heave are its accelerations. F-Zero machines
accelerate at tens of g, so those are scaled down before they are sent.

The platform does not yaw with every turn. It yaws into a slide, by how far the machine's
nose is off the way it is going, and thrashes from side to side through a spin attack. A
texture of bumps runs under it on the track, and stops in the air; the engine is sent for a
shaker's engine effect, rising with your speed.

Every channel can be scaled, inverted or limited with a `telemetry.txt` in the directory you
run the game from (all optional):

```
accel_scale 0.1     # what the machine's accelerations are multiplied by before anything else
slide_deadzone 10   # how far, in degrees, the nose may be off the way it goes before it yaws
slide_yaw_gain 0.4  #   then yaw by this much of the rest
slide_yaw_max 12    #   at most this, in degrees
spin_rate 400       # turning faster than this, degrees a second, is a spin
spin_yaw 12         #   which swings the platform this far each way, in degrees
track_texture 0.3   # how rough the track feels, 0 to 1
texture_gain 1      # all of the texture
texture_bumps 4     #   bumps a second
texture_hz 6        #   how fast each one rings
texture_grain 0.25  #   the finer grain under them, against the bumps
texture_heave 1.5   #   its size at roughness 1: m/s^2,
texture_pitch 0.3   #   degrees
texture_roll 0.5    #   and degrees
idle_rpm 1500       # the engine at a standstill
max_rpm 9000        #   and flat out
top_kmh 1400        #   at this speed
pitch_gain 1        # multiply each channel; a negative value inverts it
roll_gain 1
yaw_gain 1
surge_gain 1        # forward acceleration
sway_gain 1         # sideways acceleration
heave_gain 1        # acceleration along the track's normal
fade_s 1            # seconds to ease in and out of rest
max_angle 25        # the most pitch, roll or yaw sent, in degrees
angle_rate 120      #   and how fast pitch and roll may change, in degrees a second
yaw_rate 360        #   and yaw
max_accel 15        # the most acceleration sent, in m/s^2 (about 1.5 g)
accel_rate 300      #   and how fast it may change, in m/s^2 a second
accel_s 0.15        # seconds over which accelerations are smoothed; higher is gentler
velocity_s 0.05     # the same for the velocity they are taken from
log 0               # 1 prints what is sent, twice a second
```

## Running on a Quest 3

Early: the app builds from the same scripts as the other gcn-recomp games, but has not yet
been played in the headset. The scripts are PowerShell, so these steps are for a Windows PC.

### Requirements

On the PC, besides CMake, Ninja and Python as for the desktop build:

- [Android NDK](https://developer.android.com/ndk/downloads), unpacked to
  `%LOCALAPPDATA%\Android\Sdk\ndk\<version>\`
- [platform-tools](https://developer.android.com/tools/releases/platform-tools) (for `adb`),
  unpacked to `%LOCALAPPDATA%\Android\Sdk\platform-tools\`
- The Android SDK's `build-tools;34.0.0` and `platforms;android-34`, installed with
  `sdkmanager` from the
  [command-line tools](https://developer.android.com/studio#command-line-tools-only)
  (run `sdkmanager --licenses` first)
- A JDK, for signing the APK: `winget install Microsoft.OpenJDK.21`

On the headset, enable
[developer mode](https://developer.oculus.com/documentation/native/android/mobile-device-setup/),
connect it over USB, and accept the *Allow USB debugging* prompt.

### Build and install

Build for Windows first (`.\build.ps1`): the game assembles part of itself in memory at
boot, and the Android build takes that part from the desktop build. Then, with your disc
image in `rom/`:

```powershell
.\build-android.ps1          # cross-compiles libfzerogx.so for arm64
.\package-apk.ps1 -Install   # packages the APK, installs it, and pushes the disc image
```

The disc image is copied to `/sdcard/Android/data/com.ethannicholas.fzerogx/files/`, which
takes a few minutes the first time. The memory card and the shader cache live there too.

### Playing

The app appears under **Library → Unknown Sources → GX Recomp**, and behaves as
[on a PC](#playing-in-vr-on-a-pc).

| GameCube | Touch controller |
|---|---|
| Control stick | Left thumbstick |
| C stick | Right thumbstick |
| A / B | A / B (right) |
| X / Y | X / Y (left) |
| Z | Right grip |
| L / R | Left / right trigger |
| Start | Menu (left) |
| D-pad | Left thumbstick while holding the left grip |
| *(cockpit view on/off, in a race)* | Right thumbstick click |

`adb logcat -s fzerogx` shows the app's log.

### Tuning the VR view

Settings are read at startup from `vr.txt` (beside the game on a PC; on the Quest,
`/sdcard/Android/data/com.ethannicholas.fzerogx/files/vr.txt`, which `adb push` updates
between runs). One `key value` per line, `#` starts a comment; all optional:

```
units_per_metre 1        # game units per real metre: sets the apparent size of the world
near_m 0.1               # near and far planes in metres
far_m 2000
hud_scale 0.5            # how much of the field of view the HUD's frame occupies
hud_distance_m 4         # how far in front of the camera that frame stands
hud_height_m 0           # how far above the forward axis its centre sits
world_pitch_deg 0        # degrees of the chase camera's downward pitch taken out of the world
eye_scale 1.4            # eye resolution, times what the headset recommends
msaa 4                   # antialiasing samples per pixel in stereo
transition_s 1           # seconds the switch between flat and stereo takes
```

## Controls

| GameCube | Keyboard | Controller |
|---|---|---|
| Control stick | Arrow keys | Left stick |
| C stick | – | Right stick |
| A / B / X / Y | X / Z / C / S | A / X / B / Y |
| L / R | Q / W | Triggers |
| Z | D | Right shoulder |
| D-pad | I / J / K / L | D-pad |
| Start | Return | Start |

Escape quits.

## Repository layout

- `gcn-recomp/`: the shared recompiler, GameCube runtime, renderer and frontends, as a
  submodule. Nothing in it is specific to this game.
- `analysis/`: function and data layout of `main.dol` and of each module
  (`analysis/rels/`), in decomp-toolkit's format.
- `recomp/`: the tables that steer the recompiler for this game.
- `src/`: the game's half of the headset and the motion rig: when to be in stereo, the
  cockpit camera, and the machine's motion.
- `tools/`: `analyze.sh`, which regenerates `analysis/`; `dis` and `disraw`, which print
  disassembly from it and from your extracted executable.
- `docs/dev/`: working notes.
