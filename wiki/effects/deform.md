---
title: Dented ground [deform] (ground that keeps a dent)
tags: [effect, deform]
updated: 2026-11-26
sources: [include/sh4der_deform.h, README.md "The deform effect (ground that keeps a dent)", test/deform_test.c]
---
# Dented ground [deform]: ground that keeps a dent

**What you will learn:** how feet and wheels press lasting dents into a ground mesh you already draw, and what it costs.
**For:** a programmer adding footprints or tracks in snow, mud, sand or ash.

> **In plain words.** Snow, mud or sand keeps the marks pressed into it. A grid stores how deep each small square of ground is pressed, and the ground mesh (the grid of points it is drawn from) reads that depth. Snow keeps a dent, mud slowly closes it, and a small raised lip stands round each one. Use it for footprints and tyre tracks that must stay put.

## Try this first

What it is for: footprints and tracks that stay pressed into snow, mud or sand, at one byte a grid cell.

1. Keep the default window: `SH4DER_DEFORM_GRID_W` = 160 and `SH4DER_DEFORM_GRID_H` = 160 cells, `SH4DER_DEFORM_CELL` = 0.25 metres: 40 m across and 25,600 B (include/sh4der_deform.h:82-84, ADVISED; the 25,600 B is in the header's cost note, include/sh4der_deform.h:13).
2. Snow: `refill` = 0, so dents stay (include/sh4der_deform.h:114; set at init, include/sh4der_deform.h:180, ADVISED). Mud: no recommended refill rate yet. README 'The deform effect' gives 'a few' steps a second, with no number. The test: walk a path through mud on a console and time how long a dent takes to close.
3. Rim: `SH4DER_DEFORM_RIM` = 0.25 of the dent's depth (include/sh4der_deform.h:86, ADVISED), reaching out `SH4DER_DEFORM_RIM_REACH` = 1.6 stamp radii (include/sh4der_deform.h:85, ADVISED). Set `SH4DER_DEFORM_RIM` to 0 for no rim.
4. Prints: `SH4DER_DEFORM_PRINTS` = 32 (include/sh4der_deform.h:87, ADVISED). The decal list keeps the last 32 prints; use it on a coarse mesh for crisp footprints.

Run on a console by the owner in Match Light (her test; no measured numbers recorded yet; released in the public Match Light film). README 'The deform effect' says: 'Not measured: the cost on a console'; this page's Console time row says: 'not measured | OPEN'.

Header: [sh4der_deform.h](../../include/sh4der_deform.h). Call order: [Call order](#call-order), below.

## What it does

A grid of one byte a cell under your ground mesh answers "how high is the ground here?" for every vertex (README "The deform effect"):

- **Stamp:** feet, capsules and wheels press a bowl with a raised rim. A lifted foot leaves its dent.
- **Keep or close:** snow keeps dents (refill 0); mud closes them over time.
- **Window:** the grid follows the camera in whole cells and wraps, with no copy.
- **Read:** height, depth (to darken a vertex) and slope (for the normal).
- **Prints:** a list of the last 32 footprints as decal quads, for crisp prints on a coarse mesh.

Header only; it draws nothing itself. Version v0.10 (`include/sh4der_deform.h:57`).

## Call order

See README "The deform effect":

1. Stamp each presser once a frame.
2. Recentre on the camera.
3. Read the height per ground vertex.
4. Draw print quads as blended decals just above the ground, each tested with `sh4der_tri_area_ok` ([Send checks](guard.md)).

## Cost

| Item | Cost | Status |
|---|---|---|
| RAM | 25,600 B for a 160 × 160 window at 0.25 m (40 m); 644 B for 32 prints | as-stated (README effect table) |
| CPU | four byte loads a ground vertex | as-stated (counts) |
| VRAM | 0 | |
| Console time | not measured | OPEN |

## Pitfalls

- A dent is no sharper than the mesh that shows it. Use the print list on a coarse mesh.
- A dent that leaves the window is gone.

## Related

- [Recommended limits](../limits/recommended-limits.md)
- [Open questions](../open-questions.md)
