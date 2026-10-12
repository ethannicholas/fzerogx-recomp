# Working notes for this repository

This is a static recompilation of *F-Zero GX* (GameCube). The recompiler, runtime, renderer
and frontends are the `gcn-recomp` submodule (canonical checkout at `~/Source/gcn-recomp`),
shared with `~/Source/bluestorm-recomp` and `~/Source/prime-recomp`. Fix shared things
there: commit in the submodule, push to the canonical repo, then commit the new pointer
here. Only what is specific to this game -- `analysis/`, the `recomp/*.txt` tables, `src/` (the
game's hooks, once there are any), `docs/dev/` -- lives
in this repository. Anything game-specific a shared mechanism needs goes in a setting or
hook supplied from here, never in gcn-recomp; `gcn-recomp/tools/check_generic.py` enforces
the names part of that after every edit (`.claude/settings.json`) and at commit in the
submodule.

**The repository must contain no game code or data.** Only original source and metadata
about code layout (`analysis/`). The disc image lives in `rom/`, and everything derived
from it -- `build/main.dol`, `build/rels/`, `build/rels-runtime/` and the generated C in
`build/gen/` -- is ignored by git. Never commit, quote at length, or paste disassembly of
the game into tracked files; a function name and address is fine, its instructions are not.

**Never add anything that decodes game data.** The game assembles its main module,
`fze.main`, in memory at boot; the build takes it from the running game (`REL_RUNTIME`,
see `docs/dev/boot.md`). Do not write, commit or document code, keys, constants, file
names or algorithms that reproduce how the game does that, or any other protection or
obfuscation in the game; if something seems to need it, find a way to let the game's own
code do it, and ask first.

`README.md` is the public landing page: only what someone needs to build and play. Record
anything non-obvious you learn in `docs/dev/`:

- `docs/dev/status.md` -- how far the game gets, what is broken, the next milestones.
- `docs/dev/boot.md` -- how the game loads its code and maps its memory.
- `docs/dev/routes.md` -- scripted input routes through the game.

When a belief is overturned, say what the belief was and what disproved it.

Most of the game is in relocatable modules, not the DOL; see `gcn-recomp/docs/rels.md`.
Module code is compiled at canonical addresses from 0x81800000: an address there in the
generated C or a backtrace is `fn_<module>_<offset>` in `analysis/rels/<module>.txt`
(module 1, `fze.main`, has its text at 0x81837BE0 in the current layout; `build/main_rels.dol.rels`
lists every section). `tools/dis fn_1_95C` disassembles a module function.

Commit on `main` and push as work lands; no feature branches or PRs.
