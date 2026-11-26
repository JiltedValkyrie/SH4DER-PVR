---
title: Cloud deck [cloudvolume] (a cloud slab you fly through)
tags: [effect, cloudvolume]
updated: 2026-11-26
sources: [include/sh4der_cloudvolume.h, README.md effect table, demo/cloudvolume.c, test/cloudvolume_test.c]
---
# Cloud deck [cloudvolume]: a cloud slab you fly through

**What you will learn:** how the SH-4 ray-marches a cloud layer into a small buffer that the PVR draws over the sky, and what that is estimated to cost.
**For:** a programmer making a flight over and through cumulus.

## What it does

A software ray march through a cloud slab (README effect table; `include/sh4der_cloudvolume.h:8-20`):

- a cover map, a tiling noise volume that erodes the edges, and sunlight with absorption and forward scatter;
- the result goes into a small 8-bit buffer;
- the PVR draws that buffer as one filtered quad over its own sky.

It is a real fly-through: the whiteout inside a cloud comes out of the march itself. `demo/cloudvolume.c` is a Dreamcast demo with a sea under the clouds and on-screen controls.

Version v0.50: host prototype and Dreamcast demo (`include/sh4der_cloudvolume.h:60`).

## Cost

ESTIMATE from instruction counts, not measured on a console (`include/sh4der_cloudvolume.h:13`).

| Item | Cost | Status |
|---|---|---|
| RAM | about 143 KB: maps 66,560 B, frame block 1,072 B, buffers (24 KB at 128 × 96) | as-stated |
| VRAM | 32 KB at the default, 128 KB at the top resolution | as-stated |
| CPU | 14-22 ms a frame at the default | ESTIMATE |
| CPU per sample | about 16 cycles in empty air, 42 reading the cover map, 38 more in cloud, 52 a noise octave | ESTIMATE |

The PVR draws one blended full-screen quad. Any such layer costs about 3.1 ms at 640 × 480 (PVR-002, MEASURED).

The march counts its rays, samples, skips, hits and noise reads in its frame block, so a console run can check the estimate.

## Recommended limits

At 14-22 ms (ESTIMATE), the march alone takes half or more of a 30 fps frame. Treat it as the scene's main cost. ADVISED until measured. The test: the march's own counters plus the cycle counter, in the demo.

## Open

- The march's time on a console.

## Related

- [Budgets](../limits/budgets.md)
- [Screen bloom](glow.md)
