# Status

## Complete (2026-10-23)

The game is fully playable from start to finish, on the desktop and in VR (PC VR and the
Quest 3), with sound and music, and has no known issues. Story Mode's movies were the last
thing found broken. The shader keys shipped in `src/shader_keys.bin` now include everything
the Quest compiled over those sessions (`vr.md`). The sections below are the history.

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

None. This section used to list a flat grey-lilac area over part of the screen in some of
the title's attract-mode shots, seen before the FPRF fix (below); it has not been seen in
play since.

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

## Dash plates (2026-10-22)

Dash plates had never worked reliably: some fired, most did not, with no pattern anyone
could see. A Practice session on the Quest that ran over two without a boost replayed the
same on the desktop, and a later session in which one fired showed the path. Each frame
fn_1_28660 tests the machine's move (its position at +0x7C and last frame's at +0x88)
against the course's special surfaces with fn_1_163BC, and a result with 0x08 sets 0x400000
in the machine's flags, from which fn_1_26F34 starts the boost (a countdown at +0x18A).
fn_1_163BC finds the collision grid cell under the machine by `fctiw` with FPSCR[RN] set to
round toward minus infinity. The runtime's `fctiw` always rounded to nearest, so wherever
the machine was past the middle of a cell the next cell was searched, and a plate's
triangles in the right one were missed. gcn-recomp d6d72af makes `fctiw` honour RN. With it
the Practice replay follows the recorded path exactly up to the first plate and then
takes the boost there; after that the run differs from what was played, so the second
plate could not be checked from the recording.

## Story Mode's movies (2026-10-23)

Story Mode's movies (`fze.movie`, `fze.movie_module`) played as a black screen for their
length. A replay of a Quest session reached the prologue's movie; its decoded frames were
right (the movie draws each as a 640x336 I8 luma texture and a 320x168 IA8 chroma texture,
both dumped with `GCN_TEXDUMP`), but the quad that draws them wrote nothing even with every
texture white (`GCN_TEX_WHITE`), and a stop at its first TEV stage was black too. The quad
is orthographic at view z 1e-4 with a near plane at 0, a clip z of -1.0000001: a hair past
GX's near plane, which a console still draws, and the renderer clipped it exactly. The
prologue and the first chapter's movie play since gcn-recomp's near test lets that through
(`graphics.md`, "Depth past the viewport's range").

The first fix gave both planes 1e-5 of w, on the belief that a console clips with some
slack. It is known and measured: Dolphin issue 6914 (where "F-Zero GX cutscenes not being
shown" was merged as a duplicate) found on a Wii that a clip z + w of -2^-23 draws and
-2^-22 does not. The movie's quad comes to exactly -2^-23. The test is now Dolphin's (z
scaled by 1 - 1e-7), which draws the one and clips the other; the replay's movies still play.

## Next

Nothing planned. This used to list a route that finishes a race, the other modes, the grey
area in the attract sequence and audio; the game has since been played through, with sound
and music.
