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
| 0x2C48C | the player's machine | a pointer into the heap; set from machine settings on, **stale** after the race, 0 in the title's demo |
| 0x83C24 | race camera, top byte | 0 first person, 1 the default chase camera, 2 and 3 further back |
| 0x6D840 | racers | 0x1E1E with 30 racing; the low byte dropped to 0x1D when the player retired |

Not what they looked like: `.bss`+0x7F02C read 9 while racing and 0 paused and in the menus,
but it is the rumble motors' state (a per-controller array driving a pad call), and 9 is a
rumble level.

The machine (from the pointer above):

| offset | what |
|---|---|
| 0x00 | flags; 0x80800400 racing, 0x88000480 retired |
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

## Not yet known

- World scale, HUD placement and any world pitch in the headset: `units_per_metre` is 1 from
  the speedometer, everything else the defaults. Needs wearing.
- Whether the countdown accepts a camera tap (taps wait for phase 41).
- Phase 42 (a finish) and the other modes (time attack, practice, story, VS).
- Telemetry on a rig: the accelerations' scale (wall and machine contacts hold surge, sway
  and heave at the 15 m/s^2 limit for a moment), whether the track-relative angles feel
  right in a pipe or on a cylinder, the slide and spin yaw, the texture.
