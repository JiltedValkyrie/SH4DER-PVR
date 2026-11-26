---
title: Mist bank [mist] (a bank of slices)
tags: [effect, mist]
updated: 2026-11-26
sources: [include/sh4der_mist.h, README.md "The mist effect (a bank of slices)", test/mist_test.c]
---
# Mist bank [mist]: a fog bank with a shape

**What you will learn:** how a fog bank that things walk into is drawn as upright translucent sheets, and why fill is its whole cost.
**For:** a programmer adding ground fog in a street, mist on water, steam or low cloud.

## What it does

Table fog tints what is far away but has no shape. This module gives it one (README "The mist effect"):

- 12 upright sheets by default (at most 16), spaced through the bank's depth.
- Each sheet has a soft top, drifting noise and a gentle wobble, hashed from its number, with no state.
- A sheet fades as the eye nears it, so walking through never shows a hard plane.
- `sh4der_mist_slice_alpha` gives each sheet the alpha that makes the whole bank stop the share of light you ask for.

Header only; you project and submit. Version v0.10 (`include/sh4der_mist.h:65`).

## Call order

1. At load, fill the noise texture once with `sh4der_mist_noise`.
2. Each frame, for each sheet, `sh4der_mist_slice` gives six vertices (4 triangles).
3. Project them and test with `sh4der_tri_area_ok`.
4. Submit with blend source alpha / 1 − source alpha, depth test on, depth write off, no culling.

## Cost

| Item | Cost | Status |
|---|---|---|
| RAM | 68 B | `sizeof` on the host, 2026-11-25, Parity (no pointers, so the SH-4 size matches); the README effect table's 64 B is out of date |
| VRAM | 8 KB for a 64 × 64 noise texture (yours) | as-stated |
| Triangles | 4 a sheet, 48 at the default | as-stated |
| PVR fill | every sheet covers its whole area on screen; not measured | OPEN |

For scale: a full-screen translucent layer costs about 3.1 ms at 640 × 480 (PVR-002, MEASURED). Twelve sheets across the whole view could cost far more than the effect is worth.

## Recommended limits

Keep the bank low and partial (a band, not a wall across the view), or use fewer sheets. ADVISED. The test: PVR render time by `pvr_get_stats` with the bank on and off.

## Related

- [Budgets](../limits/budgets.md)
- [Recommended limits](../limits/recommended-limits.md)
