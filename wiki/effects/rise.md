---
title: Rising lights [rise] (lights that lift and become stars)
tags: [effect, rise]
updated: 2026-11-26
sources: [include/sh4der_rise.h, README.md effect table, test/rise_test.c]
---
# Rising lights [rise]: lights that lift and become stars

**What you will learn:** what the rise effect draws and what it costs.
**For:** a programmer adding released lanterns, candles that become stars, or fireflies leaving a bush.

## What it does

Points of light sit still, then lift off one after another. They drift up with a sway, shrink and cool, and come to rest as twinkling stars (README effect table). Nothing is stored per light: each value comes from the time.

Header only. Version v0.10 (`include/sh4der_rise.h:45`). The details are in the header comment.

## Cost

These are counts, not times; nothing here is measured on a console (`include/sh4der_rise.h:13`).

| Item | Cost | Status |
|---|---|---|
| RAM | 84 B parameter block, 0 a light | as-stated |
| VRAM | 0 of its own | as-stated |
| CPU | about 50 multiplies and one sine-cosine pair a light a frame | as-stated (counts) |
| PVR | at most N additive quads on the translucent list | as-stated |
| Console time | not measured | OPEN |

## Pitfalls

Additive quads that pile up in one 32 × 32 tile are expensive: 256 sorted translucent triangles in one tile took 6.4 ms (PVR-009). Spread the lights out.

## Related

- [Sparks and tracers](spark.md)
- [Drifting specks](motes.md)
- [Recommended limits](../limits/recommended-limits.md)
