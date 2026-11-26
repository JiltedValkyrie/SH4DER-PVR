---
title: Inner light [holo]
tags: [effect, holo]
updated: 2026-11-26
sources: [include/sh4der_holo.h, src/sh4der_holo.c, README.md "The holo effect", AGENTS.md "Call order: holo"]
---
# Inner light [holo]

**What you will learn:** how the holo effect makes an inner light breathe with a translucent body, and what it costs.
**For:** a programmer drawing a hologram, a ghost, or any see-through body with a glow inside.

## What it does

A light inside a translucent body follows the body's own breathing. At load, each vertex gets one weight byte. Each frame fills a 64-entry colour table, and each vertex then costs one table load. Alpha is constant (README:258-272; holo.h:8-40).

Version: v1.00, used in a released game (holo.h:31).

## Call order

The full steps are in `AGENTS.md` "Call order: holo" and README:274-289:

1. At load, call `sh4der_holo_weights` on the bind positions.
2. Each frame, feed the breath tracker. Then call `sh4der_holo_prepare` with the breath and the fade.
3. Per vertex, call `sh4der_holo_argb(frame, weight)`. Draw alpha-blended, texture × colour.

## Cost

All as-stated (README:310). None of it is measured on a console.

| Item | Cost |
|---|---|
| Frame state | 272 B (256 B of it is the table) |
| Parameters | 44 B |
| Weights | 1 B per vertex, made once with one distance test per vertex |
| Per frame | 64 multiply-adds |
| Per vertex | one load |
| VRAM | 0 |

## Recommended limits

- Weights run from 0 to 63 (64 levels) (README:263, 297).
- The glint is at full strength within 3 units, and gone from 6.5 units (README:272, 1248-1249).

## Tuning

See `README.md` "Holo tuning" (README:291-308).

## Pitfalls

- Bake the texture brighter by 1 / body, so the colour table can bring it back down (README:268).
- Bad parameters clear the table to 0. A NaN breath counts as 0.5. A NaN raw value is not remembered (README:289).
- Draw the glint 0.0075 toward the viewer, depth tested, with no depth write (README:272).

## Related

- [Shared parts](core.md), which holds the breath tracker
- [Glossary](../glossary.md)
