---
title: PSM routes A, B and AB
tags: [shadows, psm]
updated: 2026-11-25
sources: [README.md "The psm effect", demo/psm/ROUTE_AB_NOTES.md, demo/psm/THESIS_SH4_SHADOWS.md, demo/psm/psm_dc.c, logs/benchmarks/older]
---
# PSM routes A, B and AB

**What you will learn:** the three ways this project makes and uses a shadow map, what each costs on a console, and what is still unknown.
**For:** a programmer choosing between them, and anyone continuing the shadow-map work.

A shadow map needs two jobs: **draw** the map (depth from the light), then **compare** each receiver against it. The routes differ in who does each job.

| Route | Draws the map | Compares | Self-shadowing | Code |
|---|---|---|---|---|
| A | PVR: casters flat dark on white, into a render target | Nobody. The ground is drawn again as a multiply layer that looks the target up per pixel. | No | `include/sh4der_psm_pvr.h` (target pass and layer not written in the library; the town demo has its own) |
| B | SH-4: 16-bit depth into RAM | SH-4, per receiver vertex, 2 × 2 PCF; the result goes into the vertex colour | Yes | `include/sh4der_psm_raster.h` |
| AB | PVR: depth as a colour code through a ramp texture | SH-4, as B, after PVR-DMA copies the map back, one frame late | Yes | design; the town demo has a **stub** in which the SH-4 fills the map |

Placement (uniform map or PSM fit, one world-to-map matrix, bias) is shared: `include/sh4der_psm.h`. The equations are pinned in `test/psm_test.c`. Two choices differ from the source chapter: the fit axis is the mean direction (the chapter uses a minimal cone), and the bias is in metres (the chapter squares it).

## Route A

- **Catch:** a PSM is projective, and the PVR interpolates texture coordinates per vertex, so a lookup is exact only at the vertices. On the host, a 4 m ground cell was off by up to 57 texels at 6 m ahead; a 1 m cell by 1.1 (README "The psm effect"). Give the ground fine cells near the camera.
- **Console (town, one view):** SH-4 13.3-13.6 ms, PVR 15.9 ms, about 27,000 TA vertices, 18 fps. MEASURED, see [scenes](../limits/scenes.md).
- **Map:** 256 × 256 render target, 128 KB of video memory with `pvr_scene_begin_rtt`.

## Route B

- **Catch:** an edge is only as sharp as the receivers' vertex spacing. Walls need extra "eave bands" of about 400 triangles (`TOWN_MOCKUP.md:88`).
- **Console (town, one view):** SH-4 40.2 ms (uniform), 43.9 ms (PSM); PVR 7.1 ms; 13-14 fps. MEASURED.
- **Model:** raster 12-43 cycles a texel, compare 184-489 cycles a vertex (ESTIMATE, `THESIS_SH4_SHADOWS.md` F13, F14). The model has not been checked against a split console timing.
- **Map:** 128 KB of RAM at 256 × 256. A 512 map misses the 16 KB operand cache; the demo labels it "not speed".

## Route AB

What spikes s2 and s3 settled (MEASURED, 2026-11-24, one console):

- A texel reaches a 16-bit target exactly through REPLACE, with dither off (PVR-022, PVR-023).
- PVR-DMA copies the target back: 0.87 ms for 128 × 128, 3.5 ms for 256 × 256 (BUS-021).
- Render 2 rows taller, or the last 16 pixels arrive one render late (PVR-021).
- 14-bit codes are exact; 16-bit codes from 32,768 are one under (PVR-024).
- 512 × 512 is out (PVR-026).

The SH-4 side at 128 × 128: copy back 0.87 ms; 20,000 random map reads from the RAM copy 2.8 ms (SH4-043). The verdict in `ROUTE_AB_NOTES.md:176` is "doubtful but testable".

**The town's "AB stub" (87 ms) is not route AB.** The SH-4 fills both maps in it.

## Open

- The PVR's render time for a caster pass, by `pvr_get_stats`.
- The cost of sending the casters.
- Route B's time split into raster and compare on a console.
- Route AB with the PVR really drawing the map.
- Why route A runs at 18 fps with SH-4 13.6 ms and PVR 15.9 ms.

## Related

- [psm effect page](../effects/psm.md)
- [Bump compare chain](bump-compare-chain.md)
- [Recommended limits](../limits/recommended-limits.md), "Shadows"
