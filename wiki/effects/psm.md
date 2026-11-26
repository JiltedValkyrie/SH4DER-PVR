---
title: Sun shadow map [psm] (shadow map placement and the two map routes)
tags: [effect, psm, shadows]
updated: 2026-11-26
sources: [include/sh4der_psm.h, include/sh4der_psm_raster.h, include/sh4der_psm_pvr.h, README.md "The psm effect", demo/psm/psm_demo.c, logs/benchmarks/older/PXL_20261125_0701*.jpg]
---
# Sun shadow map [psm]: shadow maps for a directional light

**What you will learn:** what the three psm headers do, which route draws the map, and what the town demo measured on a console.
**For:** a programmer adding sun shadows from a [shadow map](../glossary.md), and anyone choosing between the routes.

## What it does

Three headers, all inline and host tested:

- **`sh4der_psm.h`**: where the map's texels go. A uniform (orthographic) map, or a perspective shadow map ([PSM](../glossary.md)) fitted in the camera's post-projective space, so near ground gets more texels. It gives one world-to-map matrix, the depth key, a world-space bias, texel and pixel footprints, the frustum corners and a slab clip.
- **`sh4der_psm_raster.h`**: route B. The SH-4 rasterises casters as 16-bit depth into your RAM buffer, then compares per receiver vertex, giving 0..1 for the vertex colour.
- **`sh4der_psm_pvr.h`**: route A. The SH-4 clips casters into triangles for a render target and gives each ground vertex its lookup coordinates.

The routes, in plain words (the full page is [PSM routes](../shadows/psm-routes.md)):

| Route | Who draws the map | Who compares | Self-shadowing |
|---|---|---|---|
| A | the PVR, into a render target (a silhouette, flat dark on white) | nobody; the ground looks the target up per pixel | no |
| B | the SH-4, 16-bit depth in RAM | the SH-4, per receiver vertex, 2 × 2 PCF | yes |
| AB | the PVR draws a depth code; PVR-DMA copies it back | the SH-4, as in B, one frame late | yes |

Version: v0.10 for all three (psm.h:65; psm_raster.h:43; psm_pvr.h:35).

## Call order

See README:974-1019 ("Calls" at README:990). `AGENTS.md` has a file entry, but no call-order section. The worked client is `demo/psm/psm_demo.c` on the host (`make psm-demo`); the console demo is the town in `demo/psm/`.

## Cost

**Measured on a console: the town demo.** HUD readings from photos, one console, 2026-11-25, `logs/benchmarks/older/PXL_20261125_0701*.jpg`. Map 256 × 256, DIST INF, casters ALL, one camera view (x 0.7, z −3.5, heading 50, pitch −17), 8 people and 4 hens as placeholder blocks.

| Route / map | SH-4 ms | PVR ms | TA vertices | FPS | Map memory |
|---|---|---|---|---|---|
| A, uniform | 13.3-13.6 | 15.9 | 27,031-27,075 | 18 | 128 KB VRAM (render target) |
| A, PSM | 13.5-13.6 | 15.9 | 27,023-27,075 | 18 | 128 KB VRAM |
| B, uniform | 40.2 | 7.1 | 15,142 | 14 | 128 KB RAM (16-bit depth) |
| B, PSM | 43.9 | 7.1 | 15,142 | 13 | 128 KB RAM |
| AB stub, uniform | 87.3 | 7.2 | 15,142 | 8 | 2 × 128 KB RAM |
| AB stub, PSM | 87.9 | 7.0 | 15,142 | 8 | 2 × 128 KB RAM |

Read it with care:

- The AB stub is a stand-in: the SH-4 fills the map in place of the PVR, one frame late. It is not route AB's real cost.
- Route A's frame rate (18) is lower than its SH-4 and PVR times suggest. Where the rest of the frame goes is OPEN.
- These are one view on one console. They are not a budget for other scenes.

As-stated, not timed on a console:

- `sh4der_psm.h`: 80 B a map description (`sizeof` on the host, 2026-11-25, Parity; README:988's 76 B leaves out the kind field); 4 × 4 products at build; a lookup is a 4 × 3 transform and a divide (README:988).
- `sh4der_psm_raster.h`: RAM size × size × 2 B (32 KB at 128 × 128). One FTRV per caster vertex in a batch, three adds and one reciprocal per covered texel, three transforms per receiver vertex (psm_raster.h:34-42).
- The MOVCA clear: 16.2 cycles a cache line against 41.9 for a plain write that misses (psm_raster.h:119-121). These are the h1 cache lines (MEASURED, one console, 2026-11-25; see the [SH-4 ledger](../hardware/ledger-sh4.md)).
- `sh4der_psm_pvr.h`: per-vertex lookup error on the host only. With a 256 map, a 4 m ground cell is off by up to 57 texels at 6 m and 1.5 at 20 m; a 1 m cell by 1.1 and 0.07; a 0.25 m cell by 0.06 and 0.004 (README:985). This is ESTIMATE from the host model.

## Recommended limits

- `PSM_MAX_PTS` 64 (a slab clip of a frustum gives 32).
- `PSM_RASTER_SIZE_MAX` 1,024 a side. Maps of 512 miss the SH-4's 16 KB data cache, and the demo labels them "not speed" (psm_scene.h:1522-1523).
- `PSM_PVR_MAX_TRIS` 10 per world triangle (psm_pvr.h:52).
- Route A's map is capped at 256 in the demo.
- A perspective map on route A needs fine ground cells near the camera.

## Tuning

See `README.md` "Psm tuning" (README:1001-1018).

## Pitfalls

- A build returns 0 for a degenerate frame (a cone near 90 degrees, or a point behind the camera). Fall back to a uniform map (README:976-982).
- Raise `warp_near` (4 m in the example), or the map degenerates with a render near plane of 0.3 m (README:~980).
- Only a uniform map is exact for per-vertex lookups. The PVR has no q coordinate (README:~987).
- Route A has no self-shadowing.
- The fit axis is the mean direction and the bias is in metres. Both differ from the chapter the method comes from; the equations are pinned in `test/psm_test.c`.

## Open

- Route B's time on a console apart from the town frame (README:984, 986).
- Route A's render-target pass and layer are not written as library code (README:985).
- The PVR's render time for a caster pass.
- Why route A runs at 18 fps with 13.6 ms SH-4 and 15.9 ms PVR.

## Related

- [PSM routes](../shadows/psm-routes.md), [bump compare chain](../shadows/bump-compare-chain.md)
- [Soft character shadows](sh4dow-dc.md), [Light pools](pool.md) (blob shadows)
- [PVR ledger](../hardware/ledger-pvr.md), [Bus and DMA ledger](../hardware/ledger-bus-dma.md)
