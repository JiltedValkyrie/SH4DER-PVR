---
title: Rising lights [rise] (lights that lift and become stars)
tags: [effect, rise]
updated: 2026-11-26
sources: [include/sh4der_rise.h, README.md effect table, test/rise_test.c]
---
# Rising lights [rise]: lights that lift and become stars

**What you will learn:** what the rise effect draws and what it costs.
**For:** a programmer adding released lanterns, candles that become stars, or fireflies leaving a bush.

> **In plain words.** Points of light sit on their source, such as a lantern or a bush of fireflies. Then each one lifts off, one after another, drifts up, shrinks and cools, and settles in the sky as a twinkling star. Like the motes, it stores nothing for each light; it works each one out from the time. Use it for lanterns let go into the sky, fireflies leaving a bush, or sparks that turn into stars.

## Try this first

What it is for: lights that leave their source one at a time and settle as stars, with nothing stored per light.

1. Start from `sh4der_rise_init` (include/sh4der_rise.h:131, ADVISED). Its defaults are listed in its comment (include/sh4der_rise.h:128-130).
2. `stagger` = 0.4 s and `jitter` = 3 s (include/sh4der_rise.h:128, ADVISED). At these defaults the jitter is larger than the stagger, so the lift-off order shuffles (the header's rule, include/sh4der_rise.h:33: a jitter larger than the stagger shuffles the order). To keep the index order, set the jitter at or under the stagger.
3. `rise_time` = 6 s, `sky_y` = 60 world units, `sky_band` = 20 world units (include/sh4der_rise.h:128, ADVISED).
4. `SH4DER_RISE_TWINKLE_IN` = 1.0 s (include/sh4der_rise.h:69, ADVISED): how long the twinkle takes to fade in after a star arrives.
5. Keep few lights in one screen tile, a 32 × 32 pixel block. 256 sorted translucent triangles in one tile measured 6,385 us, against 82 us spread over the screen (PVR-009, MEASURED; wiki/hardware/ledger-pvr.md:43). No recommended count per tile yet. The test: time the frame with the lights packed into one tile, then spread out.

Run on a console by the owner in Match Light (her test; no measured numbers recorded yet). The header says: 'counts, not times; nothing here is measured on a console' (include/sh4der_rise.h:13).

Header: [sh4der_rise.h](../../include/sh4der_rise.h).

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
