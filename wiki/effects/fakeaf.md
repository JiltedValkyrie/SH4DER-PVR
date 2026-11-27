---
title: "fakeaf (fake anisotropic filtering)"
tags: [effect, filter, texture, mip, bias, floor]
updated: 2026-11-26
sources: [include/sh4der_fakeaf.h, test/fakeaf_test.c, demo/psm/fakeaf.h, demo/psm/corr/corr_modes.h, README.md "The fakeaf effect", wiki/hardware/ledger-pvr.md PVR-180]
---
# fakeaf (fake anisotropic filtering)

**What you will learn:** why a floor seen at a grazing angle looks blurred across the view or shimmers along it, how the mip bias per polygon by slope works, what the distance ranges with stretched copies would add, and what has and has not been measured on a console.
**For:** a programmer with a long floor or road in view, where the PVR's one mip level per polygon is either too soft across the view or too noisy along it.

> **In plain words.** The PVR picks one mip level per pixel, from the larger of the two texture footprints. On a floor at a grazing angle that is the footprint along the view, so the level is too coarse across the view and the picture is soft. This page offers two cheap fixes. The first sets a mip bias on each polygon from how steeply it is seen, which is one number per polygon and costs no extra pass. The second cuts the floor into distance ranges, each with a copy of the texture squashed along the view, so the far floor is sharp across and filtered along. The bias has been timed on a real console. The ranges have not.

## Try this first

What it is for: a long floor (a corridor, a road, a hall) with a texture that smears at a grazing slope.

1. Give each floor polygon a bias code from its slope: `faf_bias_code(ndv, strength, least)` with `ndv` = |normal . direction to the eye| (`include/sh4der_fakeaf.h`, the BIAS section). Use `strength` 1.0 and `least` 1 to start.
2. Batch the polygons by code, one polygon header a code. `FAF_BIAS_NONE` (4) is 1.00, and a code under 4 is sharper.
3. Time it against plain bilinear on the console. The ledger's corridor: 7.20 ms bias against 6.17 ms bilinear (PVR-180).

Header: [include/sh4der_fakeaf.h](../../include/sh4der_fakeaf.h). Call order: [below](#call-order).

## What it does

**The bias.** `faf_bias_code` gives a polygon a mip bias from its slope. The bias is `1 - strength x (1 - sqrt(ndv))`, taken to quarters (the PVR's code is the bias x 4, 1 .. 15). A polygon facing the eye keeps its level; a grazing one gets a sharper level (a bias under 1.00), never sharper than `least`. No pass and no fill are added, and the cost is the polygon batching.

**The ranges.** `faf_table_default` or `faf_table_from` make a table of distance ranges. Range 0 is the base texture. Each further range starts where the floor's texels a pixel along the view double, a square root of two apart, and fades in across a zone (`faf_alpha`) from the range before. `faf_stretch` makes each range's copy: the texture box-filtered and halved along the travel direction only, so it is sharp across and filtered along. A mesh with vertices on the zone edges (`faf_edges`, `faf_cuts`) draws each piece once opaque and, inside a zone, again translucent with the previous range's source.

Header only, all `static inline`. Plain C with `SH4DER_CORE_NO_SH4ZAM`; the square roots and reciprocals are SH4ZAM's on the console.

## Call order

1. At load: `faf_table_default(t)` (or `faf_table_from`), `faf_table_ok(t, n)`; for each range k above 0, `faf_stretch` of the texture into its own buffer (`faf_stretch_bytes` tells the video memory first).
2. Bias, every frame: for each floor polygon, `faf_bias_code(ndv, strength, least)`; batch by code.
3. Ranges, every frame, only if the bias is not enough: `faf_piece(t, n, dist, &opaque, &blend)` says which copy draws a piece; `faf_alpha(&t[k], dist)` gives the blend's vertex alpha; `faf_edges` and `faf_cuts` split a long quad at the zone edges.

## Cost

| Item | Cost | Status |
|---|---|---|
| Corridor at 640 x 480, bias by slope (least 0.50 and 0.25) | 7.21 and 7.20 ms PVR (PVR-180); 7.19 and 7.24 ms (PVR-207) | MEASURED, one console a run; the two runs agree within 0.04 ms |
| Corridor, a fixed bias of 0.50 | 7.71 ms (PVR-180); 7.68 ms (PVR-207) | MEASURED |
| Corridor, bilinear, same stages | 6.17 ms (PVR-180); 6.14 ms (PVR-207) | MEASURED |
| Corridor, trilinear | 9.44 ms (PVR-180) | MEASURED |
| The level the bias picks on the PVR | not read back | OPEN (ledger PVR-180, "why: OPEN") |
| Ranges | no figures | NOT MEASURED |
| Three stretched copies of a 256 x 256 RGB565 texture | 114,688 bytes (112 KB) | exact, by the code |
| Fill of the ranges, a 3 x 3 m corridor at focal 400 (3V; 3D) | 0.068 and 0.478 of a screen | ESTIMATE, a model (`faf_fill_3v`, `faf_fill_3d`), not a measurement |

The bias is the cost of a sharper level on the PVR. PVR-180's stage means show that a sharper bias costs PVR time (+1.0 to +1.5 ms over bilinear); why is OPEN.

## Recommended limits

- Start with the bias, not the ranges. A bias costs batching only; the ranges cost copies in video memory and a cut mesh.
- Give a sharper bias only to floor polygons at a grazing slope (`ndv` low). Facing polygons keep 1.00.
- Keep one range to about a doubling of distance. A stretched copy has no mip levels (PVR mip maps are square), so a range that spans more than that is undersampled.
- Do not claim that the bias sharpens the picture until the level the PVR picks has been read back (OPEN).

## Tuning

The default table (`faf_table_default`, an eye about 1.6 m above the floor, invented starting values, ESTIMATE):

| Range | From (m) | Ramp (m) | Bias code | Source shift |
|---|---|---|---|---|
| 0 | 0 | 0 | 4 | 0 |
| 1 | 6 | 2 | 3 | 1 |
| 2 | 14 | 4 | 2 | 2 |
| 3 | 30 | 8 | 2 | 3 |

`faf_table_from(t, n, focal, eye_h, texels_m, share)` builds the table from the floor's numbers instead. `FAF_RANGES_MAX` (4) and `FAF_BIAS_NONE` (4) are fixed in the header. `README.md` "The fakeaf effect" has the same table.

## Which filter tool

| Tool | What it does | Where it runs |
|---|---|---|
| fakeaf bias (this page) | a mip bias per polygon by slope | console, PVR-180 |
| fakeaf ranges (this page) | stretched copies per distance range, a ramp between | helpers only |
| [ramp](ramp.md) | the linear 0 .. 1 ramp the ranges use for their zones | CPU only |

## Maturity

ALPHA v0.4 (`wiki/status/maturity.md`): the bias code has run on a console; the ranges have no figures. Host test: `test/fakeaf_test.c`, on the plain path and on SH4ZAM's software path; both pass. Byte check (MEASURED on the host, 2026-11-26): every helper's output is bit-equal between the two paths except the ranges of `faf_table_from`, where 96 of 320 range values differ, the largest by 9.5e-07 m (SH4ZAM's reciprocal and root; the console's FSRRA path NOT RUN). The default table and the bias codes are equal.
