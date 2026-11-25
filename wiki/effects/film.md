---
title: film (old projector wear)
tags: [effect, film]
updated: 2026-11-25
sources: [include/sh4der_film.h, README.md effect table, test/film_test.c]
---
# film: the wear of old film

**What you will learn:** what the film effect draws and what it costs.
**For:** a programmer making an old newsreel, a flashback or a found-footage look.

## What it does

It draws the wear of old film in a projector (README effect table):

- a brightness that swings a few per cent; it is never a strobe (at most 8 % peak to peak and 3 hertz);
- thin scratches that come and go;
- dust specks;
- a gate weave of one pixel.

Nothing is stored: every value comes from the time and a seed. Pair it with [lens](lens.md)'s vignette and grain for the whole look.

Header only. The header's version line reads "v0.1" (`include/sh4der_film.h:49`).

## Cost

| Item | Cost | Status |
|---|---|---|
| RAM | 92 B parameter block | as-stated (README effect table) |
| VRAM | 0 of its own | as-stated |
| PVR | one multiply rectangle, up to 3 lines and 12 small squares a frame, on the translucent list | as-stated (counts) |
| CPU | two sines a frame | as-stated (counts) |
| Console time | not measured | OPEN |

The multiply rectangle is a full-screen translucent layer. Any such layer costs about 3.1 ms of PVR time at 640 × 480 (PVR-002, MEASURED).

## Pitfalls

The brightness limits (8 % and 3 Hz) are there to keep it from strobing. Keep them in your tuning.

## Related

- [lens](lens.md)
- [Budgets](../limits/budgets.md)
