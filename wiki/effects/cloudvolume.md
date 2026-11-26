---
title: Cloud deck [cloudvolume] (a cloud slab you fly through)
tags: [effect, cloudvolume]
updated: 2026-11-26
sources: [include/sh4der_cloudvolume.h, README.md effect table, demo/cloudvolume.c, test/cloudvolume_test.c]
---
# Cloud deck [cloudvolume]: a cloud slab you fly through

**What you will learn:** how the SH-4 ray-marches a cloud layer into a small buffer that the PVR draws over the sky, and what that is estimated to cost.
**For:** a programmer making a flight over and through cumulus.

> **In plain words.** A layer of lit clouds that you can fly through. The main processor follows each line of sight through a slab of cloud in small steps, and the result is drawn once as a picture over the sky. The viewer sees puffy cumulus (heap clouds), sunlit on top and dark underneath, and white when inside one. Use it for a flight through clouds; expect it to be the scene's main cost (the [glossary](../glossary.md) explains the chip terms).

## Try this first

What it is for: a lit cloud slab you fly through, worked out on the main processor and drawn once as a sheet over the sky.

1. Start with the default march steps: `SH4DER_CLOUDVOLUME_STEPS` = 16 (include/sh4der_cloudvolume.h:97, ADVISED). The most a ray takes is `SH4DER_CLOUDVOLUME_MAX_STEPS` = 64 (include/sh4der_cloudvolume.h:81, ADVISED).
2. Map and noise sizes: `SH4DER_CLOUDVOLUME_MAP` = 128 and `SH4DER_CLOUDVOLUME_NOISE` = 32, both powers of two (include/sh4der_cloudvolume.h:78-79, ADVISED).
3. Cloud cover: `SH4DER_CLOUDVOLUME_COVERAGE` = 0.60 and `SH4DER_CLOUDVOLUME_EROSION` = 0.42 (include/sh4der_cloudvolume.h:91-92, ADVISED).
4. Density: `SH4DER_CLOUDVOLUME_DENSITY` = 0.010 a metre in full cloud (include/sh4der_cloudvolume.h:90, ADVISED). The header's note says 63 % is hidden after 100 m.
5. Cost test: read the march's counters in `sh4der_cloudvolume_frame_t` (rays, samples, skips, hits, noise reads) with the cycle counter, in the demo, and set them against this page's 14 to 22 ms a frame (ESTIMATE, Cost). No recommended frame budget yet.

On a console: NOT RUN. This page says: 'ESTIMATE from instruction counts, not measured on a console'; its Open list says: 'The march's time on a console.'

Header: [sh4der_cloudvolume.h](../../include/sh4der_cloudvolume.h).

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
