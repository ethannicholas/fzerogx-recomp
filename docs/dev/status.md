# Status

## 2026-10-12: first boot

Boots through the SDK, builds and links `fze.main`, links `fze.title`, plays the AV logo
and shows the first memory card prompt ("There is no F-ZERO GX save file on the Memory
Card in Slot A. Would you like to create a save file now?") at full speed. The rendering
of both looks right. Not yet tried: answering the prompt, the title screen, menus, a race.

## Next

- Script input past the memory card prompt (`GCN_INPUT` / `--replay`) and get to the title
  screen and the menus, which load further modules.
- A race: `fze.main`'s per-frame work and the course loading through the virtual memory
  window.
- Audio: not looked at. The music is ADX (`snd/adx_bgm`), the sound effects are in
  `snd/normal_se/PACK*.bin`, and the movies are Sofdec (`.sfd`).
- Windows: `build.ps1` from prime-recomp has not been brought over.
