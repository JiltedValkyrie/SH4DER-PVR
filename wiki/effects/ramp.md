---
title: "Distance ramp [ramp]: one linear 0..1 ramp between two distances"
tags: [effect, ramp]
updated: 2026-11-26
sources: [include/sh4der_ramp.h, test/ramp_test.c, README.md "The ramp helper", AGENTS.md "Files"]
---
# Distance ramp [ramp]: one linear 0..1 ramp between two distances

**What you will learn:** how a strength eases in or out between two distances, in the two forms the library offers (one call, or a prepared ramp for a loop), and what each costs.
**For:** a programmer fading a shadow, a fog, a level of detail or a light's reach by camera distance.

> **In plain words.** A ramp is a number that rises or falls smoothly from 0 to 1 between two distances, such as a shadow that fades in as you walk toward it. It draws nothing itself; other code uses the number as a strength. Use it when a strength should ease in or out with distance, instead of switching on and off.

## Try this first

It turns a distance into a strength from 0 to 1 that eases between two distances. It has no tuning macros: the two distances are yours.

1. The two distances: where the effect starts and where it is full. No recommended value yet. The test that would settle them is the same view with the fade against a hard step, watching PVR time and frame time on a console (include/sh4der_ramp.h:7-8).
2. For a loop: `sh4der_ramp_make(&r, start, end)` once at load (one reciprocal) (include/sh4der_ramp.h:58-61; ADVISED).
3. Per point: `sh4der_ramp_up(&r, d)` rises from 0 at the start to 1 at the end; `sh4der_ramp_down(&r, d)` is the fade-out, 1 at the start and 0 at the end (include/sh4der_ramp.h:64-70; ADVISED).
4. For one point: `sh4der_ramp_in(from, len, d)`, one call with one reciprocal (include/sh4der_ramp.h:44-50; ADVISED).
5. An end at or before the start, or a length of 0 or less, is a step at the start and not a fade (include/sh4der_ramp.h:43, 47, 60).

On a console: no figures.

Header: [include/sh4der_ramp.h](../../include/sh4der_ramp.h). Call order: [Call order](#call-order).

## What it does

A linear 0 .. 1 ramp between two distances, saturated at both ends. It is a helper, not a picture: it draws nothing. The rising form gives 0 up to the start and 1 from the end on; the falling form is its complement, a strength that fades out with distance.

The same ramp serves a fake anisotropic filter's range weight, a shadow tile's strength by camera distance and the form of a depth fade. The library's copy is `include/sh4der_ramp.h`.

Header only. CPU only.

Version: v0.10, host tested (`test/ramp_test.c`).

## Call order

1. One call: `sh4der_ramp_in(from, len, d)` gives 0 up to `from`, 1 from `from + len` on. One reciprocal (FSRRA on the SH-4) a call.
2. A loop: `sh4der_ramp_make(&r, start, end)` once (one reciprocal), then `sh4der_ramp_up(&r, d)` or `sh4der_ramp_down(&r, d)` a point (a multiply and a saturate).
3. A length of 0 or less, or an end at or before the start, is a step at `from`.

## Cost

| Item | Cost | Status |
|---|---|---|
| RAM | one `sh4der_ramp_t` (two floats, 8 B) | as-stated (`include/sh4der_ramp.h`) |
| CPU, one call | one reciprocal and a saturate | ESTIMATE (not measured) |
| CPU, prepared, a point | one multiply and a saturate | ESTIMATE (not measured) |
| VRAM | 0 | |

No row in the ledger backs a number here.

## Recommended limits

- Prepare the ramp once for a loop over many points. The one-call form takes a reciprocal each time.
- Use it for a CPU strength only. A strength the PVR must evaluate per pixel from a texture is a different job.
- Status ADVISED. To prove: the same view with a faded effect against a hard step, PVR time and frame time, on a console.

## Tuning

None. The ramp has no tuning macros; the two distances are the caller's.

## Pitfalls

- A ramp with its two ends the same is a step, not a divide by zero. Do not rely on a step to fade.
- The output is clamped to 0 .. 1 for any distance, including far outside the range.

## Open

- The console cost of each form.

## Related

- [Light pools](pool.md) (a fade by distance for light pools), [Sun shadow map](psm.md) (shadow tiles and the shadow demo), [Tile-cut pass](tilecut.md)
