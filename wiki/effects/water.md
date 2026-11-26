---
title: Water waves [water]
tags: [effect, water]
updated: 2026-11-26
sources: [include/sh4der_water.h, README.md "The water effect", demo/OCEAN.md]
---
# Water waves [water]

**What you will learn:** what the water header computes per vertex, and where its numbers come from.
**For:** a programmer drawing a sea or lake surface as a vertex mesh.

> **In plain words.** A sea or lake is drawn as a grid of points (a mesh; each point is a vertex). This code works out how each point moves, tilts and shines, from a few long waves and small ripples. The viewer sees water that rolls, reflects the sky and glows where a wave top is lit from behind. Use it for a sea, a lake or a puddle; the [glossary](../glossary.md) explains the chip terms.

## Try this first

What it is for: give a water mesh its waves, its sky reflection and its glow, one vertex at a time.

1. Build a wave table of `sh4der_water_wave_t` entries, 28 B each (include/sh4der_water.h:25-32). Start from the demo's shape: four long swells and three short ripples (demo/OCEAN.md:11-12, ADVISED). The demo's own numbers are in demo/ocean_scene.h, which is not one of this page's sources, so there is no recommended value yet for amplitude, wavelength or speed. The test: draw the mesh at the eye heights you ship and look for shimmer on the crests.
2. Keep every wave valid: `dx` and `dz` a unit vector, `wavelength` above 0. The header skips a wave that breaks this (include/sh4der_water.h:26-28). This is a rule of the header, not a tuning value.
3. Pass `footprint` to `sh4der_water_sample` as the distance between neighbouring vertices, the vertex spacing (include/sh4der_water.h:54-57, ADVISED). A wave shorter than 2 spacings fades to nothing; a wave of 6 spacings or more is at full strength (include/sh4der_water.h:55-56). Pass 0 to turn the fade off.
4. Pass `swell` and `ripple` to `sh4der_water_sample` as 1, meaning as authored (include/sh4der_water.h:58-59, ADVISED). Turn one down to 0 to see what it adds to the picture.
5. Crest glow: `thin_path` and `thick_path` for `sh4der_water_crest_sss` have no recommended value yet; the header gives none. The test: put the sun behind a crest, then in front of it. The glow must fade to nothing with the sun in front (AGENTS.md, the sss worked examples).

Run on a console by the owner in Sunset sea (her test; no measured numbers recorded yet). This page says: 'None of it is measured on a console.' The ocean demo runs on a real Dreamcast, but its steady 33.3 ms frame is not yet measured (demo/OCEAN.md:5).

Header: [sh4der_water.h](../../include/sh4der_water.h). Call order: [Call order](#call-order), below.

## What it does

Header only, all inline, all per vertex (README:1148-1156; water.h:8-16):

- directional waves;
- filtered ripple normals;
- Fresnel (Schlick, F0 0.0204);
- a reflected direction;
- crest transmission through [Light through thin parts](sss.md), with `sh4der_water_crest_sss`.

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

- ~~RAM per wave: 24 B or 28 B?~~ Answered 2026-11-25: 28 B (`sizeof` on the host, Parity; no pointers, so the SH-4 size matches), as water.h:11 says. README:1155's 24 B is out of date.
- A steady 33.3 ms frame for the ocean demo on hardware (OCEAN.md:5, 116).
- The cost of the ocean demo's BLOOM row, which only a console can measure (OCEAN.md:~128).

## Related

- [Light through thin parts](sss.md)
- [Planar reflection](pl4nar.md), for reflections
- [Budgets](../limits/budgets.md)
