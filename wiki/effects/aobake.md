---
title: "Baked corner shading [aobake]: baked occlusion against exact shapes"
tags: [effect, ao, bake, load]
updated: 2026-11-26
sources: [include/sh4der_aobake.h, test/aobake_test.c, demo/psm/ao/ao4_scene.h, README.md "The aobake effect", tools/ao_bake_core.h]
---
# Baked corner shading [aobake]: baked occlusion against exact shapes

**What you will learn:** how one ambient occlusion byte a vertex is baked at load by casting rays at spheres, boxes and a ground plane, what it costs on the console, and which of the three occlusion tools fits your job.
**For:** a programmer who has static geometry and wants a contact shadow in the vertex colour, paid once at load.

> **In plain words.** Before the program runs, this code walks every vertex of a still scene and casts a fan of rays into the space above it. A ray that hits a nearby box, sphere or the floor takes away some light, so a corner, or the ground under a box, gets a darker byte. The bake runs once at load, and the frame pays one multiply a vertex. Use it for scenery that does not move.

## Try this first

What it is for: contact shading on still scenery, computed once at load from boxes, spheres and a floor.

1. Fill a `sh4der_aobake_occl_t` with your boxes and spheres, and set `ground` to 1 with `ground_y` at the floor's height (`include/sh4der_aobake.h:75-79`; the steps are in the top comment, `:35`). No value to set.
2. `SH4DER_AOBAKE_RAYS` = 64 rays a vertex, built with `sh4der_aobake_rays` (`include/sh4der_aobake.h:64`; `README.md:735`, ADVISED: a start value, and each call takes its own count).
3. `SH4DER_AOBAKE_REACH` = 3.0 m, how far a ray looks for an occluder (`include/sh4der_aobake.h:65`; `README.md:736`, ADVISED).
4. `SH4DER_AOBAKE_PUSH` = 0.02 m off the surface (`include/sh4der_aobake.h:66`; `README.md:737`, ADVISED). Then `sh4der_aobake_bytes`, with a tick every 64 vertices for a watchdog (`include/sh4der_aobake.h:172-181`; `README.md:729`).
5. In the frame, multiply the ambient light by the 0..1 share from `sh4der_aobake_open` (`include/sh4der_aobake.h:186`). No value to set.

On a console: NOT RUN for this copy: the page says it "has not run on a console since the move: its status is ADVISED". The code this header moved unchanged took 254 ms for scene A on one console (SH4-076, MEASURED).

Header: [include/sh4der_aobake.h](../../include/sh4der_aobake.h). Call order: [below](#call-order).

## What it does

For each vertex, `sh4der_aobake_bytes` casts cosine-weighted rays over the vertex's hemisphere (a fixed Hammersley set, the same one the host tool uses) against your exact occluders: spheres, boxes, and optionally the plane `y = ground_y`. A hit at distance `t` inside the reach takes `1 - t / reach` of a ray. The byte is `255 * (1 - sum / rays)`: 255 open, 0 closed. A start inside a sphere or a box is 0.

It multiplies the ambient light only. Nothing is drawn by the header and nothing runs a frame: one byte is kept per vertex.

Header only, all `static inline`. The caller owns the occluders, the ray array, the vertices and the bytes.

## Call order

The steps are in `AGENTS.md`'s "Files" list and the header's top comment:

1. At load: fill a `sh4der_aobake_occl_t` with pointers to your spheres and boxes, and `ground = 1` for a floor at `ground_y`.
2. `sh4der_aobake_rays(dirs, n)` into an array you own.
3. `sh4der_aobake_bytes(...)` over the vertices and their unit normals, with a `tick` every 64 vertices if you have a watchdog.
4. Keep the bytes. `sh4der_aobake_open(byte, exaggerate)` gives the 0..1 share the light uses.

## Cost

| Item | Cost | Status |
|---|---|---|
| Load-time bake, scene A (2,142 vertices, 64 rays) | 252 to 254 ms | MEASURED, one console (SH4-076, SH4-082) |
| Load-time bake, scene B (2,544 vertices) | 391 to 393 ms | MEASURED, one console (SH4-082) |
| Per ray | 1.85 to 2.41 us | DERIVED from SH4-076 (371 to 483 cycles) |
| Per vertex | ESTIMATE: 64 rays times the occluders a ray tests | not measured as its own number |
| Run time, the lit vertices | +222 us of SH-4 a frame for 2,142 vertices, +280 us for 2,544 (ao4-i, two runs); +69 and +113 us in an earlier build, cause OPEN (Q91) | MEASURED, one console (PVR-176, SH4-082, SH4-077; the frame-budget row 'Baked AO at draw') |
| RAM | one byte a vertex, plus the 64-ray array (768 B) | as-stated by the code |
| VRAM | 0 | |

This copy of the code is the one measured (the move kept the bytes), but it has not run on a console since the move: its status is ADVISED. See [Bake tool](ao-bake.md) for the host tool's cost, which is not measured.

## Recommended limits

- 64 rays a vertex (`SH4DER_AOBAKE_RAYS`). The bake is linear in the ray count, so fewer rays are the first saving.
- Bake at load only. A moving occluder needs a new bake: this is not a per-frame effect.
- Read the byte as a multiply on the ambient light, not as physical light.

## Tuning

See `README.md` "Aobake tuning": `SH4DER_AOBAKE_RAYS`, `SH4DER_AOBAKE_REACH`, `SH4DER_AOBAKE_PUSH`. They are start values; each call takes its own.

## Which occlusion tool

| Tool | What it is | Where it runs |
|---|---|---|
| [Corner darkening [ao]](ao.md) (`sh4der_ao.h`) | crease darkening from the angle between faces, and a screen cavity cue | console (no rays, no occluders) |
| aobake (this page) | rays at exact spheres, boxes and a plane, once at load | console |
| [Bake tool [ao-bake]](ao-bake.md) (`tools/ao_bake_core.h`) | the same rule over any triangle list, with a closed-object inside test | host build tool only |

The console's triangle-mesh bake (`demo/psm/psm_ao.h`, 12 golden-ratio rays, its own reach) has its own bytes and is not merged with this header. It moves later as `sh4der_aomesh.h`.

## Pitfalls

- A vertex inside a closed shape is 0. That is correct for a point sunk into a prop, and wrong for a point you meant to stand outside: check the occluders' extents first.
- A ray that only touches a box face does not hit it (the test ignores grazing contact).
- `nrays` 0, a reach of 0 or a count of 0 write nothing harmful: every byte is 255 (open).

## Open

- The bake's time on a console for a mesh other than the two scenes of Room shading, four ways [ao4] and the room is not measured.
- Per-vertex cost is an estimate until a console run times a count of rays and occluders.
- The `meshbuild` helper (building the shapes into caller buffers) is parked until a second client exists.
- `psm_ao.h`'s triangle-mesh bake moves next, as `sh4der_aomesh.h`.

## Related

- [Corner darkening](ao.md), [Bake tool](ao-bake.md), [limits: recommended](../limits/recommended-limits.md), [frame budget](../limits/frame-budget.md)
