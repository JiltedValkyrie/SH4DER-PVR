---
title: Dented ground [deform] (ground that keeps a dent)
tags: [effect, deform]
updated: 2026-11-26
sources: [include/sh4der_deform.h, README.md "The deform effect (ground that keeps a dent)", test/deform_test.c]
---
# Dented ground [deform]: ground that keeps a dent

**What you will learn:** how feet and wheels press lasting dents into a ground mesh you already draw, and what it costs.
**For:** a programmer adding footprints or tracks in snow, mud, sand or ash.

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
