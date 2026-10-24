# How the game loads its code

## The modules

The DOL holds the SDK, SEGA's math library and the boot code: about 2,260 functions.
Everything else is in relocatable modules (`.rel`, linked with the SDK's `OSLink`), about
5,100 functions:

- `fze.main` (module 1): the game itself, 1.4 MB of code. Not on the disc as a file; see
  below.
- One module per mode or screen, loaded and unloaded as the game moves between them:
  `fze.title` (8), `fze.sel` (10), `fze.customize` (3), `fze.option` (4), `fze.movie` (5),
  `fze.story` (7), `fze.car_colchg` (9), `fze.movie_module` (12), `fze.replay` (13),
  `fze.pilotpoint` (14), `fze.winning` (15), `fze.profile` (16), `fze.interview` (17).
  They import from `fze.main` heavily, and `fze.main` imports one symbol from `fze.winning`.
- `fze.sample` (18): assembles `fze.main` at boot; see below.

`fze.str` is the module name table (`c:\fze\sources\rel\fze.<name>.plf`); it also names
`fze.test`, `fze.window` and `fze.testmode`, which the retail disc does not have.

gcn-recomp's REL support (`gcn-recomp/docs/rels.md`) was written for this game. The
alternative, forcing every module to a fixed address, was ruled out by the loader itself:
it pads each allocation by a random `((OSGetTick() & 31) + 1) * 32` bytes, so the address
changes from run to run.

## The main module

`fze.main` is not on the disc as a file. `main` (0x800055E0) calls `fn_8000591C`, which
reads `fze.sample.rel`, links it, calls its prolog, unlinks it and clears its memory, and
then calls what the prolog returned. The prolog assembles `fze.main` in memory and links
it with `OSLinkFixed`.

The build does not reproduce any of that. `REL_RUNTIME fze.main` in `CMakeLists.txt` makes
the first configure build the game without `fze.main` (`build/bootstrap`, in which every
module that imports from it is left out too), run it headless until the game hands
`fze.main` to `OSLinkFixed`, and keep the module exactly as the game produced it, in
`build/rels-runtime/fze.main.rel` (gcn-recomp `docs/rels.md`). That takes a few seconds,
once per build directory. Keep it that way: nothing in this repository may decode, unpack
or otherwise reproduce how the game produces the module.

dtk cannot read back its own analysis of `fze.main` (on reload it re-derives a jump table
in `.data` that runs into a string, and fails), so `tools/analyze.sh` empties that module's
symbol files before every run. Hand-named functions in `fze.main` go in `recomp/names.txt`.

## The logo delay

The AV logo stayed up for about 13 s against about 4 on a GameCube. It was not waiting on
work: the boot had linked `fze.title` by 1.4 s of game time. The logo screens' only timer is
`fn_80005E0C`, a fixed-length busy loop of about 400k loop back-edges that nothing reads a
result from. The boot screen (`fn_80005A08`) calls it 20 times, then twice in each of five
frames, then 20 more times. The title module's per-frame function (`fn_81A2BA04`) calls it
twice a frame through the logo, whose length is counted in frames. The virtual clock
charges every back-edge the same 2 ticks (24 CPU cycles), about three times what this loop's
tiny iterations cost a real Gekko. A logo frame therefore came to 2.46 fields, and since the
game waits for the retrace, the screen ran at 20 fps instead of 60. `recomp/patches.txt`
returns from the function at once; the logo screen then runs at 60 fps and lasts about 4.5 s
(measured on the Quest with frame dumps).

The game still waits for the retrace every frame, so nothing runs faster than 60 fps without
the delay: a logo frame is about 13k back-edges.

On Windows, a headless run (`--headless`, which never initialises SDL and so never raises the
timer resolution) sleeps in 16 ms steps. Catch-up turns each overshoot into a jump of game
time, and boot there takes 3-7 s longer than it should. The headset does not have this; the
windowed game should not, since `SDL_Init` sets a 1 ms timer, but that is not measured.

## Virtual memory

`fn_800071B8`, called at boot, takes a page table from the top of the arena, points SDR1
at it and gives all sixteen segment registers VSIDs; `fn_8000740C` maps pages. `fze.main`
then uses addresses such as 0x23801CE0. gcn-recomp's MMU support used to translate only
0x40000000-0x7FFFFFFF and treated everything below as RAM, which put that access 570 MB
into the host reservation; its window is now 0x02000000-0x7FFFFFFF.

## The GPU watchdog

The game keeps two GX FIFOs: the CPU fills one while the GP reads the other, swapped every
frame by `fn_8006FD1C` (which also clears a flag and issues a draw-done). `fn_8006FDEC`
then waits for the draw-done callback (`fn_800700F4`) to set the flag, polling the time
base for up to one second. On a timeout it prints `GP WAIT Timeout... (sending object n/m)`,
and the main loop prints `Ord ID / 2D ID / Last ID` and soft-resets
(`fn_1_D3474`); `OSResetSystem` returning is then the `reset.c:118: NANDEYANEN!?` panic.

Two gcn-recomp bugs produced that timeout, both fixed in the runtime:

- The CP's write pointer was copied from the PI's at every CP_CR write while linked.
  `GXSetGPFifo` writes the new FIFO's pointer with the link still on, then unlinks; the copy
  replaced it with the CPU's pointer in the other FIFO, and the GP stopped. It now follows
  the PI only when a burst lands.
- PE control read back the pending interrupt bits. The SDK's token handler acknowledges
  with a read-modify-write, so a token interrupt taken while a finish was pending
  acknowledged the finish too, and its callback never ran. The bits now read as 0.
