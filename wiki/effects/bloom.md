---
title: Mesh bloom [bloom]
tags: [effect, bloom]
updated: 2026-11-26
sources: [include/sh4der_bloom.h, src/sh4der_bloom.c, README.md "The bloom effect"]
---
# Mesh bloom [bloom]

**What you will learn:** how to get a soft glow round a blade or a lamp without reading the frame back, and what it costs.
**For:** a programmer who wants a cheap halo, sun disc or lens-flare ghost.

## What it does

A few feathered additive polygons make a glow (README:312-322; bloom.h:8-45):

- a ribbon facing the camera along a blade (3 × 5 vertices);
- a disc at the blade's tip (8 rim vertices);
- `sh4der_bloom_rings`, a round glow with its own level per ring: a sun disc, a lamp, a lens-flare ghost.

There is no frame read and no texture. This is not the screen bloom; that one is [Screen bloom](glow.md).

Version: v1.00, used in a released game (bloom.h:31).

## Call order

See README:324-343 "Calls". `AGENTS.md` has no call-order section for bloom.

Blend: additive (source alpha / one), depth test on, no depth write (README:322).

## Cost

All as-stated. None of it is measured on a console.

| Item | Cost | Source |
|---|---|---|
| A blade's glow | 24 vertices, 24 triangles (16 ribbon + 8 disc) | README:384, 354 |
| CPU | one tube evaluation per ribbon vertex, one normalise for the disc | README:384 |
| Largest ring glow | 193 vertices, 360 triangles (the indices fit in a byte) | bloom.h:30-31 |
| VRAM | 0 | |

## Recommended limits

- The disc rim has 3 to `SH4DER_BLOOM_DISC_MAX` (24) vertices; the rim count is clamped to that range (README:352; bloom.h:135).
- Ring glows have up to `SH4DER_BLOOM_RINGS_MAX` rings (bloom.h:30-31).

## Tuning

See `README.md` "Bloom tuning" (README:345-368) and the MAX preset (README:370-382).

## Pitfalls

- Bad parameters return 0 and leave the levels at 0 (README:343).
- `sh4der_bloom_disc_n` with a rim below 3 writes nothing (bloom.c:124).
- The colour writer needs a stride that is a multiple of 4 (README:1316).

## Open

- **Is `src/sh4der_bloom.c` PVR drawing or maths only?** `AGENTS.md` says "The .c is KOS PVR drawing, Dreamcast only". The file's own comments say "Maths only: no PVR calls" and "the caller picks the list and blend" (bloom.c:35, 125, 212, 370). Answered 2026-11-25 (Scholar, Parity): maths only. The file includes only `sh4der_bloom.h` (bloom.c:16), no KallistiOS or PVR header; the `AGENTS.md` line is out of date (reported to lead).

## Related

- [Glowing tubes](blade.md)
- [Screen bloom](glow.md)
- [Sparks and tracers](spark.md), for a lamp's halo quad
- [Send checks](guard.md)
