---
title: Screen overlays [lens]
tags: [effect, lens, post]
updated: 2026-11-26
sources: [include/sh4der_lens.h, src/sh4der_lens.c, README.md "The lens effect"]
---
# Screen overlays [lens]

**What you will learn:** what the lens overlays are, and the one piece of video memory they use.
**For:** a programmer adding a vignette, film grain, hit-direction wedges or a full-screen flash.

> **In plain words.** These are the overlays a camera lens and film add: a dark edge round the screen, a moving grain, a red wedge at the edge that shows which way a hit came from, and a white flash. None of them read the picture, so they need no [frame read](frame.md). Use them for a dark corner in a horror scene, grain in a cutscene, damage markers in a shooter, or a flash from lightning. They are drawn over the picture, under the HUD (the on-screen numbers and icons).

## Try this first

Lay a dark edge, grain, damage wedges and a flash over the picture, without reading it.

1. At load, call `sh4der_lens_init()` (`include/sh4der_lens.h:409`). It makes the 64 x 64 grain tile, `SH4DER_LENS_GRAIN_TILE` = 64 (8 KB of video memory, `include/sh4der_lens.h:73`, ADVISED). Call `sh4der_lens_vignette_setup(w, h)` (`include/sh4der_lens.h:412`) again whenever the screen size changes.
2. Vignette: `SH4DER_LENS_VIG_STRENGTH` = 0.45, `SH4DER_LENS_VIG_INNER` = 0.60, `SH4DER_LENS_VIG_OUTER` = 1.10 (`include/sh4der_lens.h:64-66`, ADVISED). `SH4DER_LENS_VIG_SPOKES` = 32 (`include/sh4der_lens.h:68`, ADVISED); the header's host sweep gives a worst error of 2.3 / 255 (ESTIMATE, not hardware).
3. Grain: `SH4DER_LENS_GRAIN_AMP` = 0.06 (about plus or minus 3 %), and `SH4DER_LENS_GRAIN_PX` = 1 for fine grain or 2 for coarse (`include/sh4der_lens.h:74-75`, ADVISED). Call `sh4der_lens_grain(frame, w, h)` (`include/sh4der_lens.h:418`) every frame so the grain moves.
4. Damage wedges: up to `SH4DER_LENS_HIT_MAX` = 4 at once, full alpha in `SH4DER_LENS_HIT_ATTACK_S` = 0.05 s, held for `SH4DER_LENS_HIT_HOLD_S` = 1.0 s, gone at `SH4DER_LENS_HIT_LIFE_S` = 1.5 s (`include/sh4der_lens.h:77-80`, ADVISED). Call `sh4der_lens_hit_add` and `sh4der_lens_hit_tick` (`include/sh4der_lens.h:302`, `:335`).
5. Flash: `sh4der_lens_flash_hit(&f, SH4DER_LENS_FLASH_PEAK)` with a peak of 0.35 (`include/sh4der_lens.h:89`, `:382`), attack 0.03 s and decay 0.25 s (`include/sh4der_lens.h:90-91`, ADVISED). The overlays sit at z 430, 432, 950 and 960, under a HUD at 1000 (`include/sh4der_lens.h:95-98`, ADVISED).

On a console: NOT RUN (no ledger row for the lens; the page's vignette figure is "ESTIMATE, a host sweep, not hardware").

Header: [include/sh4der_lens.h](../../include/sh4der_lens.h). Call order: [Call order](#call-order).

## What it does

Overlays that need no frame read (README:529-531; lens.h:8-45):

- a vignette mesh (spokes × rings);
- film grain that keeps the picture's mean, from a 64 × 64 ARGB4444 tile;
- hit-direction wedges at the screen's edge;
- a full-screen flash.

`src/sh4der_lens.c` is the KallistiOS side, with six polygon headers. The maths is in `sh4der_lens.h` and is host tested (lens.c:16).

Version: v0.10, passes its maths tests (lens.h:34).

## Call order

The init and draw functions are at lens.h:~407-417. `AGENTS.md` has no section of its own.

## Cost

| Item | Cost | Status |
|---|---|---|
| Grain tile | 64 × 64 × 2 B = 8 KB, the only VRAM the lens uses | as-stated (lens.h:44; lens.c:16; README:546) |
| Vignette quality | worst Gouraud error 9.1/255 at 16 spokes, 4.0 at 24, 2.3 at 32 | ESTIMATE, a host sweep, not hardware (README:541; lens.h:68) |
| Vignette vertex count | 594 vertices and 288 triangles for the whole overlay set, from spokes × rings (`src/sh4der_lens.c:17`) | |

## Recommended limits

- `SPOKES_MAX` 32, `RINGS_MAX` 16 (README:571-572; lens.h:101-102).
- `HIT_MAX` 4 wedges (README:549).
- Autosort z values 430, 432, 950 and 960, under the HUD at 1000 (README:567-570).

## Tuning

See `README.md` "Lens tuning" (README:533-572).

## Pitfalls

- With no VRAM at init, the lens draws what it can without the grain. It does not retry until the next PVR start (lens.h:~407; lens.c:29, :51).
- A PVR reset clears VRAM, so call the forget function at a PVR start (lens.c:103).
- Rebuild the rim table when the screen size changes (lens.h:~410).

## Related

- [Screen filters](visor.md), [Fake focus blur](focus.md)
- [Video ledger](../hardware/ledger-video.md)
