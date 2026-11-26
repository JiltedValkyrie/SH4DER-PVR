---
title: "Soft edges for light [softfade]: a glow, a pool or a beam fades out at a wall the SH-4 knows"
tags: [effect, softfade, light]
updated: 2026-11-26
sources: [include/sh4der_softfade.h, test/softfade_test.c, README.md "The softfade effect", wiki/hardware/ledger-pvr.md PVR-157 PVR-203 PVR-204 PVR-205, wiki/hardware/ledger-sh4.md SH4-083]
---
# Soft edges for light [softfade]: a glow, a pool or a beam fades out at a wall the SH-4 knows

**What you will learn:** how a light is faded per vertex near planes and walls with no depth read, the three tiers (near, fit, fade), and what is and is not measured.
**For:** a programmer whose lamp halo is cut on a hard line by a wall, or whose headlight beam runs through a barrier.

## What it does

A camera-facing glow that passes through a wall is cut on a hard line. A pool or a beam of light on the ground is cut the same way where a wall stands on it. The PVR cannot read the depth behind a pixel. The SH-4 knows where the walls are, so it takes the light off the vertices near them.

The caller names the surfaces: planes (endless) and walls (a plane over a rectangle). Three tiers, cheapest first:

- near: does this light need a fade at all;
- fit: a 4-vertex halo's radius and alpha are scaled so the quad never reaches the surface (the glow shrinks, it does not fade);
- fade: each vertex keeps 0 on or behind a surface and 1 from the fade distance in front of it, a smooth step between.

Header only, maths only. It draws nothing.

Version: v0.10. PROTOTYPE v0.1: host tested (`test/softfade_test.c`), NOT RUN on a console.

## Call order

1. At load: `sh4der_softfade_make(&f, SH4DER_SOFTFADE_DIST)`; a `sh4der_softfade_plane_t` a floor or an endless wall (`sh4der_softfade_plane` from a point and a unit normal toward the lit side); a `sh4der_softfade_wall_t` a barrier or a building front (`sh4der_softfade_wall`).
2. A light a frame: `sh4der_softfade_near` with the light's centre and radius. A 0: draw it plain.
3. A 4-vertex halo: `k = sh4der_softfade_fit(...)`; multiply its radius and its alpha by k.
4. A ring halo (`sh4der_bloom_rings`): `sh4der_softfade_levels` over its positions and levels. A pool or a beam (`sh4der_pool_beam`): `sh4der_softfade_argb` over its vertices. A 0 back: nothing has light, do not send it.

## Cost

| Item | Cost | Status |
|---|---|---|
| RAM | 16 B a plane, 60 B a wall, 8 B the fade distance | ESTIMATE (a host size count; not checked on the SH-4) |
| VRAM, PVR passes, textures | 0 | FACT (the header: no PVR pass, no video memory, no texture) |
| CPU, a plane a vertex | one 3-term dot, a subtract, a multiply, a saturate, a smooth step | ESTIMATE (not measured) |
| CPU, a wall a vertex | the plane's, and two more dots and two saturates | ESTIMATE (not measured) |
| CPU, near or fit | once a light, a dot a surface | ESTIMATE (not measured) |
| A lamp's halo of 129 vertices faded against a room's planes, another program's own code | 205 us of SH-4 a frame, the halo's build included | MEASURED, one console, for the old hand-written fade (SH4-083); not this header |

Nothing in this header has run on a console.

## Recommended limits

- `SH4DER_SOFTFADE_DIST` 0.30 m: ADVISED, a start value.
- `SH4DER_SOFTFADE_MAX` 8 planes and 8 walls a call: pass only the surfaces near the light.
- A halo that fades needs rings: the lamp test's halo is 8 rings (`AO4_LAMP_HALO_RINGS` `8u`, `demo/psm/ao/ao4_lamp.h:48`; the header's "Do" line says the same, `include/sh4der_softfade.h`; no ledger row names the count). A beam needs 3 or 4 rows. ADVISED.
- To prove: a glow at a wall with the fade against the plain glow on a console, the SH-4 time and the picture.

## Tuning

See `README.md` "Softfade tuning".

## Pitfalls

- Planes and rectangles only. Nothing fades against a car, a person or a curved shape: no depth is read.
- The fade is per vertex. Where a wall crosses a triangle, a hard edge of at most that triangle stays.
- A light within the fade distance in front of a wall is dimmed too.
- A plane's normal points toward the lit side. The wrong way round, everything in front of it goes dark.
- Call `sh4der_softfade_make` once, not a vertex: it takes a reciprocal.

## The other route

A camera depth map the PVR draws fades a glow per pixel against any shape. It ran on a console in a test program: the hard line went (PVR-204, MEASURED, the pictures); the map cost 5.1 ms of PVR and 3.4 ms of send, 1.4 ms of PVR cut to its own tiles (PVR-203, PVR-205, MEASURED, one console). It is not in the library.

## Open

- Everything on a console: the cost a vertex, and the look.
- One client in the tree: the lamp test's halo (`demo/psm/ao/ao4_lamp.h`) fades through `sh4der_softfade_levels` since 2026-11-26; on the host its levels equal the old hand-written fade's (`demo/psm/ao/test/ao4_lamp_test.c` prints it when `python demo/psm/ao/build.py --spike lamp --host-only` runs; `make test` does not run that test. The author's host run printed: largest difference 0 of 255, 0 of 387 channels over 0.75, where 387 is 3 channels x 129 halo vertices. This page did not re-run it). That build has NOT RUN on a console; the 205 us in the table is the old code's.

## Related

- [Light pools](pool.md), [Sparks and tracers](spark.md), [Distance ramp](ramp.md)
