---
title: Corner darkening [ao] (small-scale occlusion)
tags: [effect, ao]
updated: 2026-11-26
sources: [include/sh4der_ao.h, README.md "The ao effect"]
---
# Corner darkening [ao]: small-scale occlusion

**What you will learn:** the two kinds of darkening the ao header gives, and when each is computed.
**For:** a programmer who wants crevice shading baked into a model, or a cheap cavity cue on a finished frame.

> **In plain words.** Ambient occlusion is the soft darkening in corners, creases and where an object meets the floor, because less of the room's light reaches those spots. This header makes two kinds: a crease darkening baked into the vertex colours at build time (no cost at run time), and a cheap cavity cue on the finished screen picture, which lags one frame behind. Use it when shapes look flat and you want contact darkening without casting rays.

## Try this first

What it is for: darken creases at build time, and add a cavity cue on the screen, so shapes read as solid.

1. Crease: `SH4DER_AO_CREASE_K` = 0.5 (`include/sh4der_ao.h:54`; `README.md:718`, ADVISED): a vertex at a full concave crease keeps half its colour.
2. `SH4DER_AO_CREASE_FULL` = 2.0 (`include/sh4der_ao.h:55`; `README.md:719`, ADVISED): the sum of concave edge amounts that counts as full, two right-angle edges. Add each edge with `sh4der_ao_edge_concave` (`include/sh4der_ao.h:69`), then multiply the vertex colour by `sh4der_ao_crease_factor` (`:79`). No value to set in those calls.
3. Screen grid: `SH4DER_AO_SCR_GRID_X` = 80 and `SH4DER_AO_SCR_GRID_Y` = 60 (`include/sh4der_ao.h:56-57`; `README.md:720-721`, ADVISED), the grid of the look read; `sh4der_ao_grid` then `sh4der_ao_texel565` (`:96`, `:110`).
4. Strength: `SH4DER_AO_SCR_K` = 0.5 (`include/sh4der_ao.h:58`; `README.md:722`, ADVISED): a multiply at full cavity is 1 - 0.5.
5. Floor: `SH4DER_AO_SCR_FLOOR` = 4.0 (`include/sh4der_ao.h:61`; `README.md:725`, ADVISED): black needs it, or black areas read as deep cavities (the page's Pitfalls).

The page's status: "As-stated. There are no numbers."

Header: [include/sh4der_ao.h](../../include/sh4der_ao.h). Call order: [below](#call-order).

## What it does

Header only (README "The ao effect", line 710 on; ao.h:10, 18):

- **Crease darkening** from the angle between faces. It is for a bake: the result goes into the vertex colour, so it costs nothing at run time.
- **A screen cavity cue** from the small look grid (80 × 60 by default). It is one frame behind.

Version: v0.10 (ao.h:29).

## Call order

See README "The ao effect" (line 710 on). `AGENTS.md` has no section of its own.

## Cost

As-stated. There are no numbers.

| Item | Cost |
|---|---|
| Crease bake | nothing at run time (a baked vertex colour) |
| Screen cue | "no runtime cost" in the header's summary line; it reuses the [Frame read [frame]](frame.md) read |

## Recommended limits

- The grid is 80 × 60 by default, to match the [Exposure and grade](look.md) read.

## Tuning

See `README.md` "Ao tuning" (line 714 on).

## Pitfalls

- The screen term is one frame behind (ao.h:10).
- A black read needs a floor (`SCR_FLOOR`), or black areas read as deep cavities.

## Related

- [Frame read](frame.md), [Exposure and grade](look.md), [Light through thin parts](sss.md)
