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

Early. The game boots to its first memory card prompt; see `docs/dev/status.md` for how
far it gets and what is known not to work.

## Building

### Requirements

macOS on Apple Silicon, with Xcode (or the Command Line Tools), CMake 3.20+, Ninja,
Python 3 and SDL2:

```sh
brew install cmake ninja sdl2 python
```

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
./build.sh
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

Run it from the repository root: the memory card is created in `saves/` relative to the
current directory. Every run also records the controller input to `saves/inputs/<timestamp>/`
so that a route can be played back with `--replay=<that directory>`; `--no-input-log` turns
that off.

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
- `tools/`: `analyze.sh`, which regenerates `analysis/`; `dis` and `disraw`, which print
  disassembly from it and from your extracted executable.
- `docs/dev/`: working notes.
