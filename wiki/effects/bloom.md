---
title: Mesh bloom [bloom]
tags: [effect, bloom]
updated: 2026-11-26
sources: [include/sh4der_bloom.h, src/sh4der_bloom.c, README.md "The bloom effect"]
---
# Mesh bloom [bloom]

**What you will learn:** how to get a soft glow round a blade or a lamp without reading the frame back, and what it costs.
**For:** a programmer who wants a cheap halo, sun disc or lens-flare ghost.

> **In plain words.** A soft halo of light round one bright thing, such as a blade tip, a lamp or the sun. It is a few flat, soft-edged shapes added to the picture, so their colours add up and it looks like light. It needs no copy of the picture taken back from the screen. For a glow over the whole screen, use [screen bloom](glow.md).

## Try this first

For a soft glow round one bright point, with no read of the picture.

1. Blade ribbon: `SH4DER_BLOOM_ROWS` = 3 rows and `SH4DER_BLOOM_COLS` = 5 columns (include/sh4der_bloom.h:54-55; ADVISED, README:369-370).
2. Ribbon width: `SH4DER_BLOOM_HALF_WIDTH` = 0.0875, in the blade's units (include/sh4der_bloom.h:70; ADVISED, README:378).
3. Tip disc: `SH4DER_BLOOM_RIM` = 8 rim vertices. A round glow uses 16, under the cap `SH4DER_BLOOM_DISC_MAX` = 24 (include/sh4der_bloom.h:56-57; ADVISED, README:371-372).
4. Rings: `sh4der_bloom_rings` takes up to `SH4DER_BLOOM_RINGS_MAX` = 8 (include/sh4der_bloom.h:58). No recommended value yet for how many rings. Test: draw one lamp glow at 3 rings and at 8, then compare the picture and the PVR time on one console.
5. Blend: added (source alpha / one), depth test on, depth write off, untextured (README:342; ADVISED).

Run on a console by the owner in Lamp with a lit shade (her test; no measured numbers recorded yet). The page says: "All as-stated. None of it is measured on a console."

Header: [sh4der_bloom.h](../../include/sh4der_bloom.h). Steps in full: [Call order](#call-order).

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
