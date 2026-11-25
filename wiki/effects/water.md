---
title: water
tags: [effect, water]
updated: 2026-11-25
sources: [include/sh4der_water.h, README.md "The water effect", demo/OCEAN.md]
---
# water

**What you will learn:** what the water header computes per vertex, and where its numbers come from.
**For:** a programmer drawing a sea or lake surface as a vertex mesh.

## What it does

Header only, all inline, all per vertex (README:1148-1156; water.h:8-16):

- directional waves;
- filtered ripple normals;
- Fresnel (Schlick, F0 0.0204);
- a reflected direction;
- crest transmission through [sss](sss.md), with `sh4der_water_crest_sss`.

Version: v0.50, runs on screen on the demo disc (README:119).

## Call order

`AGENTS.md` has no section of its own for water. README:1148-1156 lists the calls. For the crest glow, see `AGENTS.md` "Call order: sss".

The worked client is the sunset ocean demo: `demo/ocean.c`, described in `demo/OCEAN.md`.

## Cost

As-stated. None of it is measured on a console.

| Item | Cost | Source |
|---|---|---|
| RAM | the caller's wave table; the size per wave is disputed (see Open) | README:1155; water.h:11 |
| CPU per wave per sample | one sine-cosine pair and a few multiply-adds | README:1155 |
| VRAM, sound RAM | 0 | |

The ocean demo's own budget, from `demo/OCEAN.md`:

- Status (OCEAN.md:5, dated 2026-11-18): it runs on a real Dreamcast. A steady 33.3 ms frame is not yet measured on hardware.
- TA data per frame is an ESTIMATE: from about 15 KB at 390 triangles to 885 KB at 23,040, plus 77 KB of sky (OCEAN.md:180-188).
- The scene-update ratios are host plain-C ratios, not console times (OCEAN.md:180-193).

## Recommended limits

- Directions must be unit length and wavelengths above 0, or the wave is skipped (README:1156; water.h:26-28).
- The result is per vertex, so the mesh needs enough vertices where the waves change (README:1156).
- The ocean demo's PERFORMANT preset is 2,016 triangles with SH4DOW off (OCEAN.md:118-135). It is a preset, not a proven limit.

## Pitfalls

- `sh4der_water.h` does not check the wave pointer for NULL (water.h:61).

## Open

- **RAM per wave: 24 B or 28 B?** README:1155 says 24 B a wave. water.h:11 says "7 values, 28 B". One of them is wrong.
- A steady 33.3 ms frame for the ocean demo on hardware (OCEAN.md:5, 116).
- The cost of the ocean demo's BLOOM row, which only a console can measure (OCEAN.md:~128).

## Related

- [sss](sss.md)
- [pl4nar](pl4nar.md), for reflections
- [Budgets](../limits/budgets.md)
