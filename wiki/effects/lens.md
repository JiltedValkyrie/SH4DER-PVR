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

1. At load, call `sh4der_lens_init()` (`include/sh4der_lens.h:596`). It makes the 64 x 64 grain tile, `SH4DER_LENS_GRAIN_TILE` = 64 (8 KB of video memory, `include/sh4der_lens.h:111`, ADVISED). Call `sh4der_lens_vignette_setup(w, h)` (`include/sh4der_lens.h:604`) again whenever the screen size changes.
2. Vignette: `SH4DER_LENS_VIG_STRENGTH` = 0.45, `SH4DER_LENS_VIG_INNER` = 0.60, `SH4DER_LENS_VIG_OUTER` = 1.10 (`include/sh4der_lens.h:102-104`, ADVISED). `SH4DER_LENS_VIG_SPOKES` = 32 (`include/sh4der_lens.h:106`, ADVISED); the header's host sweep gives a worst error of 2.3 / 255 (ESTIMATE, not hardware).
3. Grain: `SH4DER_LENS_GRAIN_AMP` = 0.06 (about plus or minus 3 %), and `SH4DER_LENS_GRAIN_PX` = 1 for fine grain or 2 for coarse (`include/sh4der_lens.h:112-113`, ADVISED). Call `sh4der_lens_grain(frame, w, h)` (`include/sh4der_lens.h:610`) every frame so the grain moves.
4. Damage wedges: up to `SH4DER_LENS_HIT_MAX` = 4 at once, full alpha in `SH4DER_LENS_HIT_ATTACK_S` = 0.05 s, held for `SH4DER_LENS_HIT_HOLD_S` = 1.0 s, gone at `SH4DER_LENS_HIT_LIFE_S` = 1.5 s (`include/sh4der_lens.h:115-118`, ADVISED). Call `sh4der_lens_hit_add` and `sh4der_lens_hit_tick` (`include/sh4der_lens.h:488`, `:521`).
5. Flash: `sh4der_lens_flash_hit(&f, SH4DER_LENS_FLASH_PEAK)` with a peak of 0.35 (`include/sh4der_lens.h:127`, `:568`), attack 0.03 s and decay 0.25 s (`include/sh4der_lens.h:128-129`, ADVISED). The overlays sit at z 430, 432, 950 and 960, under a HUD at 1000 (`include/sh4der_lens.h:133-136`, ADVISED).

**When the first forms are not enough** (all ADVISED, host tested, NOT RUN on a console):

- The grain over part of the screen, weaker, or one layer cheaper: make a grain of your own, `sh4der_lens_grain_make(&g, 64, 1, seed, SH4DER_LENS_GRAIN_MONO)`, and draw it with `sh4der_lens_grain_draw(&g, frame, x0, y0, x1, y1, z, amp, neutral)`. `neutral` 0 leaves the boost layer out: the picture is darker by about half the amplitude, and one translucent layer is saved.
- A different grain: a tile of 16, 32, 64 or 128 a side; `SH4DER_LENS_GRAIN_CHROMA` for colour grain, `SH4DER_LENS_GRAIN_SOFT` for a gentler one; your own texels with `sh4der_lens_grain_load`; a texture you already have with `sh4der_lens_grain_use`.
- A different movement: `sh4der_lens_grain_move(&g, SH4DER_LENS_MOVE_HOLD, SH4DER_LENS_GRAIN_HOLD)` holds each offset 2 frames (film at 30 a second on a 60 Hz screen); `_DRIFT` slides, `_STILL` stays.
- A different vignette: start from `sh4der_lens_vig_default`, change the fields, `sh4der_lens_vig_round`, `sh4der_lens_vig_centre` and `curve`, then `sh4der_lens_vignette_shape(&v)`; `sh4der_lens_vignette_draw_at(ox, oy, z)` for a picture band; `sh4der_lens_vignette_mesh_draw` for a mesh you built once.

On a console: NOT RUN (no ledger row for the lens; the page's vignette figure is "ESTIMATE, a host sweep, not hardware").

Header: [include/sh4der_lens.h](../../include/sh4der_lens.h). Call order: [Call order](#call-order).

## What it does

Overlays that need no frame read (README, 'The lens effect'; the header's top comment):

- a vignette mesh (spokes × rings);
- film grain that keeps the picture's mean, from a 64 × 64 ARGB4444 tile;
- hit-direction wedges at the screen's edge;
- a full-screen flash.

The four first forms draw the whole screen at the default tuning. A client that needs something else has the variations: a grain of its own (`sh4der_lens_grain_t`: its tile's size, kind and texels, where it is drawn, how strong, with or without the boost layer, how it moves) and a vignette of its own (every field of the shape, the centre off the middle, the falloff's curve, an offset and a depth key, or a mesh built once). The first forms' output did not change: `test/lens_test.c` pins the tile, the default mesh, the frame offsets, the boost and the grain's colours.

`src/sh4der_lens.c` is the KallistiOS side, with five untextured polygon headers and one textured header a grain. Every draw checks its header was compiled and sends nothing before its setup. The maths is in `sh4der_lens.h` and is host tested (`test/lens_test.c`).

Version: v0.10, passes its maths tests (`test/lens_test.c`).

## Call order

The init, vignette and grain functions are at `include/sh4der_lens.h:596`, `:604` and `:610`. `AGENTS.md` has no section of its own.

## Cost

| Item | Cost | Status |
|---|---|---|
| Grain tile | 64 × 64 × 2 B = 8 KB, the only VRAM the lens uses | as-stated (`include/sh4der_lens.h:111`; README, 'The lens effect') |
| Vignette quality | worst Gouraud error 9.1/255 at 16 spokes, 4.0 at 24, 2.3 at 32 | ESTIMATE, a host sweep, not hardware (README, 'The lens effect'; `include/sh4der_lens.h:106`) |
| A caller's grain tile | 2 B a texel: a side of 16: 512 B; 32: 2 KB; 64: 8 KB; 128: 32 KB; a texture of the caller's own: 0 more | exact (`include/sh4der_lens.h`) |
| Grain, TA a frame | 4 vertices and 1 header; 8 and 2 with neutral on | exact (`src/sh4der_lens.c`) |
| Grain, PVR | one translucent layer over its rectangle, two with neutral on: about 3.1 ms a full-screen layer at 640 x 480 | ESTIMATE (PVR-002 is a plain layer, MEASURED; not this layer) |
| Vignette vertex count | 594 vertices and 288 triangles for the whole overlay set, from spokes × rings (`src/sh4der_lens.c:19`) | |

## Recommended limits

- `SPOKES_MAX` 32, `RINGS_MAX` 16 (README, 'The lens effect'; `include/sh4der_lens.h:144-145`).
- `HIT_MAX` 4 wedges (README, 'The lens effect').
- A caller's grain tile: a power of two from `SH4DER_LENS_GRAIN_TILE_MIN` 16 to `SH4DER_LENS_GRAIN_TILE_MAX` 128; the amplitude at most `SH4DER_LENS_GRAIN_AMP_MAX` 0.5. ADVISED.
- Autosort z values 430, 432, 950 and 960, under the HUD at 1000 (README, 'The lens effect').

## Tuning

See `README.md` "Lens tuning" (under 'The lens effect').

## Pitfalls

- With no video memory for the grain tile, `sh4der_lens_init` still returns 1 and the lens draws without the grain. `sh4der_lens_ready()` says whether the tile is there (`src/sh4der_lens.c:213`, `:224`). It does not retry until the next PVR start.
- A PVR reset clears VRAM, so call the forget function at a PVR start (`sh4der_lens_forget`, `src/sh4der_lens.c:227`).
- Rebuild the rim table when the screen size changes (`sh4der_lens_vignette_setup`, `include/sh4der_lens.h:604`).
- `sh4der_lens_init` returns 1 even when there was no video memory for the tile. Ask `sh4der_lens_ready()`.
- A draw before its setup sends nothing (it does not hang): if an overlay is missing, check that `sh4der_lens_init`, `sh4der_lens_init_headers` or the grain's own make ran.
- Do not fill a `sh4der_lens_vig_t` by hand: start from `sh4der_lens_vig_default`, or the centre and the curve are not set.
- With neutral off the picture is darker by the amplitude times (1 - the tile's mean). That is the price of the saved layer.
- A grain from `sh4der_lens_grain_use` does not own its texture: `sh4der_lens_grain_free` leaves it, and the caller frees it.

## Related

- [Screen filters](visor.md), [Fake focus blur](focus.md)
- [Video ledger](../hardware/ledger-video.md)
