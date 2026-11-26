---
title: "ramp (one linear 0..1 ramp between two distances)"
tags: [effect, ramp]
updated: 2026-11-26
sources: [include/sh4der_ramp.h, test/ramp_test.c, README.md "The ramp helper", AGENTS.md "Files"]
---
# ramp: one linear 0..1 ramp between two distances

**What you will learn:** how a strength eases in or out between two distances, in the two forms the library offers (one call, or a prepared ramp for a loop), and what each costs.
**For:** a programmer fading a shadow, a fog, a level of detail or a light's reach by camera distance.

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

Nothing has run on a console. No row in the ledger backs a number here.

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

- [pool](pool.md) (a fade by distance for light pools), [psm](psm.md) (shadow tiles and the shadow demo), [tilecut](tilecut.md)
