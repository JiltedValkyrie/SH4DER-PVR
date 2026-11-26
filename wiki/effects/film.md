---
title: Old film wear [film] (old projector wear)
tags: [effect, film]
updated: 2026-11-26
sources: [include/sh4der_film.h, README.md effect table, test/film_test.c]
---
# Old film wear [film]: flicker, scratches, dust and gate weave

**What you will learn:** what the film effect draws and what it costs.
**For:** a programmer making an old newsreel, a flashback or a found-footage look.

> **In plain words.** This makes a clean picture look like old film run through a projector. The brightness wobbles a few per cent, thin scratches come and go, dust specks appear, and the picture shakes by up to a pixel now and then. The wobble is kept slow and small on purpose, because a fast on-and-off flicker can harm some viewers. Use it for a newsreel, a flashback, a silent-film scene or a found-footage look, with the [lens overlays](lens.md) for the grain and the dark corners.

## Try this first

Lay film wear over a finished picture: a slow flicker, scratches, dust and a small shake.

1. At load, call `sh4der_film_init(&f, width, height)` (`include/sh4der_film.h:147`). Its defaults are 60 ticks a second (`:153`), a flicker of 0.025 (2.5 % deep, 5 % peak to peak, `:154`), sines of 1.3 and 2.1 Hz (`:155-156`) and a weave of 1 pixel at 2.3 Hz (`:157-158`). The header calls them "mild on purpose" (`include/sh4der_film.h:34-38`, ADVISED).
2. Keep the limits: flicker at most `SH4DER_FILM_FLICKER_MAX` = 0.04 (8 % peak to peak), and a rate of at most `SH4DER_FILM_RATE_MAX_HZ` = 3.0 Hz (`include/sh4der_film.h:75-76`, ADVISED). `sh4der_film_valid(&f)` refuses a block that goes past them (`include/sh4der_film.h:181-184`).
3. Scratches: `scratch_min` = 6 and `scratch_max` = 20 ticks, alpha 0.20 to 0.40, 80 % light, with at most `SH4DER_FILM_SCRATCH_SLOTS` = 3 alive at once (`include/sh4der_film.h:73`, `:159-161`, `:168-169`, ADVISED).
4. Specks: `specks_min` = 6 to `specks_max` = 12 a tick, 75 % dark, never more than `SH4DER_FILM_SPECKS_MAX` = 12 (`include/sh4der_film.h:74`, `:165`, `:170-171`, ADVISED).
5. Draw: the flicker's colour from `sh4der_film_exposure_argb` (`include/sh4der_film.h:251`); add the weave from `sh4der_film_weave` to the projection centre, not to captions (`include/sh4der_film.h:268`); call `sh4der_film_scratch` and `sh4der_film_specks` each frame (`:289`, `:329`, ADVISED).

Run on a console by the owner in Match Light (her test; no measured numbers recorded yet). The page: "Console time | not measured | OPEN". One full-screen translucent layer is MEASURED at about 3.1 ms at 640 x 480 ([PVR-002](../hardware/ledger-pvr.md)).

Header: [include/sh4der_film.h](../../include/sh4der_film.h).

## What it does

It draws the wear of old film in a projector (README effect table):

- a brightness that swings a few per cent; it is never a strobe (at most 8 % peak to peak and 3 hertz);
- thin scratches that come and go;
- dust specks;
- a gate weave of one pixel.

Nothing is stored: every value comes from the time and a seed. Pair it with [Screen overlays](lens.md)'s vignette and grain for the whole look.

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

- [Screen overlays](lens.md)
- [Budgets](../limits/budgets.md)
