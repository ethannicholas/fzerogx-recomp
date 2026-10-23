# VR and motion telemetry

The headset frontends (PC VR with `--vr` on Windows, the Quest app) and the motion telemetry
are gcn-recomp's (`openxr/`, `android/`, `runtime/telemetry.*`). What is this game's is in
`src/`:

- `race.cpp` reads the race out of the game's memory;
- `vr_fzerogx.cpp` answers the frontend's hooks (`vr::GameHooks`) and does the per-frame
  work on the guest thread (`gx::set_guest_frame_hook`);
- `motion.cpp` turns the player's machine into telemetry.

## What the game's memory says

Everything is in `fze.main` (module 1), whose sections move from run to run (the loader pads
by a random amount), so it is read relative to `rels_section_address(1, section)`. Offsets
below are into its `.bss` (section 6) unless said otherwise. They were found by diffing RAM
dumps (`GCN_RAM_DUMP_AT`) over the scripted race in `routes.md`, with a pause, a quit and
the title's attract demo.

| `.bss` offset | what | values |
|---|---|---|
| 0x3C28 | race phase | 38 machine settings, 39 course flyover, 40 grid and countdown, 41 racing, 43 retired. **Not reset** when a race is quit: still 41 in the menus after; also 41 in the title's demo race |
| 0x25E88 | a player's race | 1 from the grid to the end of a race the player drives; 0 in the menus (after a quit too), the flyover and the title's demo |
| 0x25C44 | paused | 1 on the pause menu and the retired screen; 0 racing and in the menus. (0x26B50 behaves the same; 0x50F8 latches at the first pause and stays) |
| 0x6D838 | the player's machine | a pointer into the heap, the first of the race's machines; set from machine settings on, in every mode seen (Grand Prix, Practice), and in the title's demo too |
| 0x2C48C | the player's machine, in a Grand Prix | the same pointer, but 0 throughout a Practice race; **stale** after the race, 0 in the title's demo |
| 0x83C24 | race camera, top byte | 0 first person, 1 the default chase camera, 2 and 3 further back |
| 0x6D840 | racers | 0x1E1E with 30 racing; the low byte dropped to 0x1D when the player retired |

Not what they looked like: `.bss`+0x7F02C read 9 while racing and 0 paused and in the menus,
but it is the rumble motors' state (a per-controller array driving a pad call), and 9 is a
rumble level.

The machine (from the pointer above):

| offset | what |
|---|---|
| 0x00 | flags; 0x80800400 racing in a Grand Prix, 0x80000401 in Practice, 0x88000480 retired |
| 0x7C | position; the previous frame's at 0x88 |
| 0x14C | world matrix, a GX 3x4 by rows: the columns are its right, up and *back* axes (the nose is -Z), the fourth its position |
| 0x1BC | the normal of the track under it (0,1,0 on the flat), copied at 0x270 |

A game unit is a metre by the speedometer: at 748 km/h on the HUD the machine moved 3.485
units a frame, 209 units a second, and 748 km/h is 208 m/s.

## When to be in stereo

Stereo while `race_live`: a player's race (0x25E88), in phase 40 or 41, not paused, with a
machine. So: theater through the menus, the course flyover, the pause menu and the end of
a race; stereo from the grid, through the countdown, to the finish or retirement.

A quit from the pause menu unpauses the race for about 30 frames before the race flag
clears, which would blink into stereo on the way out, so after a pause stereo comes back
only once the race has been live for 45 frames; continuing from the pause menu waits the
same.

## First person

The game's own: its race camera has a cockpit view, one D-Up from the default chase
camera, one D-Down back (further D-Downs pull the chase camera further back). The right
thumbstick click asks the hook for one or the other (`set_first_person`); the guest frame
hook compares that with the camera byte and, while racing, taps the D-pad -- four frames
held, twenty apart -- until they agree, through `map_pad`, which the headset frontends call
with each controller state. It never taps outside a live race, where the D-pad moves menu
cursors, and gives up after three taps that change nothing.

`FZGX_VRLOG=1` logs the taps and the stereo answer; `FZGX_FP=1` asks for first person from the
start, as the click would. The desktop window does not call `map_pad`, so there the taps are
only logged.

## Practice (2026-10-22)

A Practice race on the Quest never went into stereo. Replaying its input log with
`FZGX_VRLOG=1`, which also prints each change of the race fields, showed the phase and the
race flag as in a Grand Prix (40 on the grid, 41 racing, the flag 1), but `.bss`+0x2C48C,
which had been taken for the player's machine, 0 throughout. The belief was that it is the
player's machine in any race; it is set in a Grand Prix only. In a RAM dump of the race the
one machine moving (its matrix's position equal to its position at +0x7C) was pointed to
from `.bss`+0x6D838, which holds the same machine as 0x2C48C in a Grand Prix: that is the
pointer now. The replay goes into stereo on the grid with it.

## Shadows in stereo (2026-10-22)

Every machine's shadow turned into a translucent grey square once the view went to stereo.
The harness's eyes showed them right, because it renders at one internal scale throughout;
the app renders theater at scale 3 and stereo at 1, and with that switch reproduced
(`--scale=3` and `GCN_RESCALE=<the frame stereo starts>:1`) the squares appeared. The game
renders each machine's shadow once, as the race loads, and samples that copy for the rest of
the race; the change of scale dropped every copy texture. Fixed in gcn-recomp (copies are
resampled to the new scale).

## Distant terrain in stereo (2026-10-22)

On Cylinder Knot, from the moment the view went to stereo, part of the distant terrain was
black, and the black swung about with the head. The eyes' far plane was the frontend's
`far_m`, 2 km; the course's scenery stands farther out, and the plane turned with the head.
An eye now takes each draw's far plane from the game's projection (gcn-recomp, "The eye's
far plane" in `graphics.md`); on the Quest the terrain is whole.

## Grips (2026-10-23)

Boost (Y) is on the right grip, the spin attack (Z) on the left (`GameHooks::grip_right`,
`grip_left`). A button on the left grip replaces the frontend's D-pad mode, which this game
does not need from the player: the cockpit camera's D-pad taps are the hook's own.

## Shadows' depth in stereo (2026-10-23)

Once the shadows drew right in stereo, the player's floated at about the height of the
middle of the machine. A draw log of a stereo race frame (frame 3000 of the 23:06 session,
replayed on the Quest with `fzerogx_egl --eye`) showed why: each machine's shadow is a
six-vertex quad sampling that machine's shadow copy, with an identity position matrix -- the
game computes its corners in view space -- and it is pulled along the lines of sight
towards the camera. The player's machine was at z -4.7 to -9.0 and its shadow at -2.45 to
-5.9; against the machines, the pull came to 1.5 to 3 units at every distance, a distance
rather than a factor. The flat picture cannot show it; two eyes can.

The eye filter (`eye_filter` in `src/vr_fzerogx.cpp`) answers `EyePushOut` for a perspective
draw whose only texture is a copy of part of the frame, which in that frame was the 17
shadows and nothing else, and `shadow_push` (vr.txt, 2 units by default, chosen by eye and
right on the first try) is how far they go back. The constant in the game's code was not
looked for.

## Telemetry

See the comment at the top of `motion.cpp`. In short: velocity and acceleration differenced
from the machine's position (1/60 s a frame) and eased, put in the machine's frame (surge
along the nose, sway to the right, heave along the track normal); pitch and roll against the
track rather than the world, since gravity points into the track; the accelerations scaled
by `accel_scale` (0.1) because a boost start is about 20 g; inactive outside `race_live`.

Retuned after a first ride of the same shared module with another game, on which yawing
with every turn was jarring, the platform sat dead still on a straight, and the shaker had
no engine:

- Yaw only in a slide or a spin (`Motion::yaw_heading` false). The ride recorded on this
  machine (`saves/inputs/20261012-180637`, replayed headless with `FZGX_MOTIONLOG=1`) had the
  nose 1 degree off the velocity at the median and 7 at the 90th percentile, 20-45 through
  slide turns (about 4% of the time): the slide's yaw starts past 10 degrees. Steered, the
  machine turned at 190 degrees a second at most; through a spin attack at 1350-1520, for
  about a second: a spin is over 400, and lasts until under 200.
- A texture from the shared module's made-up bumps, on at 0.3, off in the air -- while the
  eased acceleration into the track is below -0.6 g; on the ride a jump's fall read about -9
  m/s^2 and nothing else came near. The machine's track normal (machine+0x1BC) stays valid
  in the air, so it cannot tell.
- The engine is the speed (1500-9000 rpm to 1400 km/h; the ride's median speed was 990, its
  boosts 1400-1780).

## The Vision Pro (2026-10-22)

A standalone Vision Pro build, on gcn-recomp's `visionos/` frontend (how it works, and what
the first games on a device overturned, is in `gcn-recomp/docs/visionos.md`).
`VISIONOS_BUNDLE_ID` in `CMakeLists.txt` turns it on, and `build-visionos.sh` builds,
installs and pushes the disc image. Like the Android build, it takes `fze.main` from the
desktop build's `build/rels-runtime/`, so the desktop build comes first. The platform's own
defaults apply over this game's: `eye_scale` 1, since the recommendation is the foveated
texture's size, and no GPU budget, since ANGLE's timer overstates (gcn-recomp
`docs/visionos.md`, "Resolution").

Nothing new was needed from this game's hooks: `map_pad` only adds the camera button it
taps, so a PS VR2 Sense controller, mapped onto the Touch profile, plays as a Touch
controller on a Quest, and a gamepad gets the desktop's layout; the right stick click is the
cockpit view on either.

Moving to gcn-recomp 5294025 from 79c1f4f took one change: `Motion::yaw_heading` is gone
(gcn-recomp fda4ad1, yaw is only ever relative), and `src/motion.cpp` set it to false over a
yaw that was already the slide and spin, so the line went.

Played on the device, and works.

### Shader keys from the Vision Pro (2026-10-22)

`src/shader_keys.bin` (`SHADER_KEYS`) is the list gcn-recomp compiles on a fresh install
before the game starts. It is the Vision Pro's cache from the first session there, which
reached name registration: 26 keys, the menus' only. To extend it with a later cache, put it
first so its order holds: `shader_keys src/shader_keys.bin src/shader_keys.bin <cache>`.

On 2026-10-23 it was extended so from the Quest's cache after sessions through Grand Prix,
Practice and Story Mode: 217 keys, 191 of them new (the cache held all 26 already).

