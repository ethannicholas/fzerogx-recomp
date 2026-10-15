# Status

## 2026-10-12: a race

From a blank memory card, scripted input (`docs/dev/routes.md`) gets through the whole
first-run flow and into a Grand Prix race:

- AV logo; "no save file" prompt; name entry; the save is written to the emulated card.
- Title screen with its real-time attract sequence; Select Mode; Grand Prix → Novice → Ruby
  Cup; Select Machine (Blue Falcon); the course preview; Machine Settings.
- Mute City / Twist Road: the flyover intro, the countdown, the race with 29 CPU machines,
  the HUD and minimap, boost start, speed. With accelerate held and no steering the
  machine scrapes the walls until it retires, and the Retry / Settings / Quit menu comes up.

Modules seen linked and unlinked: `fze.sample`, `fze.main`, `fze.title`, `fze.sel`.

## Known problems

- In some attract-mode shots on the title screen, a flat grey-lilac area covers part of
  the screen. Seen before the FPRF fix (below) and not checked since.

## Windows

This note used to say `build.ps1` had not been brought over from prime-recomp. It has been.
On 2026-10-12 it built from a clean checkout on Windows 11 (clang 21, VS Build Tools 2022),
reading the disc straight from a `.ciso`. The blank-card route in `routes.md` reached a live
race. With `--telemetry`, the packets arrived (236 bytes each, SimRacingStudio API v102) and
went from rest to active. `--vr` reached the Meta OpenXR runtime.

## Played through

This note used to say audio had not been looked at and that nothing had been tried in a
headset or on a motion rig. Since then the game has been played end to end in VR with the
motion rig, with sound and music, and tried on the Quest 3.

## Fixed on the way (all in gcn-recomp)

- The game's GPU watchdog reset the console: the CP write pointer was overwritten at a
  CP_CR write, and PE control read back its write-only interrupt bits (`boot.md`).
- Starting a race crashed in a worker thread: the game re-creates a detached thread on the
  same OSThread once the last one has exited, and the runtime resumed the dead one from the
  save the SDK makes as a thread exits. Bindings are now stamped with the save's SRR0 and
  r1 and dropped when the context is re-initialised.
- Every machine, and the camera, was NaN (the game prints `NaN cam_pos` on screen): the
  math library classifies a value with `frsp` + `mcrfs` before `frsqrte`, and FPSCR[FPRF]
  was never set, so it took the reciprocal square root of zero. FPRF is now kept for every
  op that sets it. This also brought back the machine model and balance chart on Machine
  Settings.

## Next

- Steering: a route that finishes a race, then the rest of the cup.
- The other modes, which load the remaining modules (story, customize, replay, ...).
- The grey area in the attract sequence.
- Audio.
