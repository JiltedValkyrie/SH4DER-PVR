---
title: "Sky shading [aosky]: occlusion of the sky drawn into a shadow target"
tags: [effect, ao, psm, pvr, shadow]
updated: 2026-11-26
sources: [include/sh4der_aosky.h, test/aosky_test.c, demo/psm/ao_layer.h, demo/psm/ao/ao4_draw.h, README.md "The aosky effect", wiki/hardware/ledger-pvr.md (PVR-127, PVR-128, PVR-130, PVR-135, PVR-138, PVR-140, PVR-152, PVR-153, PVR-176, PVR-186), wiki/hardware/ledger-sh4.md (SH4-071, SH4-072, SH4-078)]
---
# Sky shading [aosky]: occlusion of the sky drawn into a shadow target

**What you will learn:** how the sky's occlusion of the ground is drawn as a few extra shadows into the sun's shadow target, what it costs on the PVR and the SH-4, and which result is measured and which is not.
**For:** a programmer who already draws the sun's shadow into a render target for a flat ground, and wants contact darkening under props without a bake.

> **In plain words.** The sun's shadow is drawn a few extra times, each time slid toward a different patch of sky and drawn as faint black. Where those faint shadows pile up, the ground gets darker, the way it does under a prop. It adds no new picture and no new pass, because it rides on the sun's shadow target. Use it on a flat ground that already has a sun shadow; it does not darken walls or moving things.

## Try this first

What it is for: darker ground under props, from a few sky directions drawn into the sun's shadow target you already have.

1. Make the directions' header with plain blending: `sh4der_aosky_header(&h, 0)`, not 1 (`include/sh4der_aosky.h:8`, `:55`, ADVISED). The depth union (1) failed on the PVR (PVR-127, MEASURED).
2. Take k = 4 directions. `sh4der_aosky_count` rounds k to 0, 1, 2, 4 or 8, and `SH4DER_AOSKY_DIRS_MAX` = 8 is the most a call takes (`include/sh4der_aosky.h:85`, `:104`; `README.md:763`, ADVISED). Draw one side: it gives the same darkening as both sides for half the vertices (PVR-128, MEASURED).
3. `SH4DER_AOSKY_FLOOR` = 0.5, what is left of the ground's light with every direction blocked (`include/sh4der_aosky.h:87`; `README.md:764`, ADVISED). `SH4DER_AOSKY_Z0` = 1.25 is direction 0's depth in the map pass (`include/sh4der_aosky.h:88`; `README.md:765`, ADVISED).
4. Each frame, after the sun's casters: for each direction `d < k`, `sh4der_aosky_slope(k, d, s)` (`include/sh4der_aosky.h:114`), then `sh4der_aosky_dir` (`:202`) with `sh4der_aosky_argb(k, strength)`. No value to set.
5. Cap k at what the frame pays: about 0.35 to 0.45 ms of PVR a direction for scenes of about 1,100 caster triangles (the page's Recommended limits). ESTIMATE: arithmetic from the target passes in PVR-140 (451 us and 347 us a direction).

On a console: NOT RUN for this copy. The page's status: "its status is ADVISED for the copy and PROVEN for the start settings."

Header: [include/sh4der_aosky.h](../../include/sh4der_aosky.h). Call order: [below](#call-order).

## What it does

The sun's casters are drawn once into the shadow target as the sun sees them. `sh4der_aosky_dir` draws the same casters again, once per sky direction, slid along that direction onto the ground and mapped into the target, each time as a faint black. Where the shadows pile up the ground gets darker: occlusion of the sky in K steps. No new target and no new pass; the layer that carries the sun's shadow carries this too.

The header is the maths and the sender. The caller owns the casters, the target, the emit callback, the layer and both scratch arrays (`xy`, `ok`). Header only, all `static inline`.

## Call order

1. At load (KallistiOS): `sh4der_aosky_header(&h, 0)` for the directions' translucent header. Plain blending.
2. Each frame, in the map pass after the sun's casters: for each `d < k`, `sh4der_aosky_slope(k, d, s)`, then `sh4der_aosky_dir(&casters, map, s, ground_y, size, z, sh4der_aosky_argb(k, strength), cull, 1, xy, ok, emit, user)`.
3. Keep `xy` (float `[vn][2]`) and `ok` (uint8 `[vn]`) as your own scratch.
4. `sh4der_aosky_dir` leaves its matrix in XMTRX on the Dreamcast. Load your own matrix again before you transform anything else.

The demo's version of the call sits in `demo/psm/ao/ao4_draw.h` (`ao4_draw_dir`), with the scenes of Room shading, four ways [ao4]' scratch.

## Cost

| Item | Cost | Status |
|---|---|---|
| One sky direction, PVR, scene A (target A minus target S, over K 4) | 451 us | DERIVED from PVR-140 (MEASURED target passes) |
| The same, scene B | 347 us | DERIVED from PVR-140 |
| One sky direction, PVR, one side, 1,164 caster triangles (Town corner shading, build 0 [ao0]) | 753 us | DERIVED from PVR-130 |
| The depth union, both sides, 1,164 triangles (Town corner shading, build 0) | 1,213 us | MEASURED (PVR-130), not the start setting |
| SH-4 send, scenes of Room shading | 651 to 852 ns a vertex | MEASURED per vertex, DERIVED (SH4-078) |
| SH-4 send, Town corner shading, build 0 | 522 ns a vertex; 1,824 us a direction, both sides | MEASURED (SH4-071) |
| One side | the same darkening for half the vertices; the work is not saved | MEASURED (PVR-128); DERIVED (SH4-072) |
| Frame, the town at K 4 | +10.0 ms (SH-4 +9.8, PVR +1.4 mean) | MEASURED (PVR-135, a different scene) |
| VRAM | the shadow target the sun already uses; no new target | |
| RAM | the caller's `xy` and `ok` arrays: 9 bytes a vertex | by the code |

The scenes of Room shading ran this method in Room shading, build d, Room shading, build h, Room shading, build i and Room shading, build j (PVR-138, PVR-152, PVR-176, PVR-186; the target passes in PVR-140 and PVR-153). This copy of the code is the same maths, checked on the host against the scenes of Room shading; it has not run on a console since the move, so its status is ADVISED for the copy and PROVEN for the start settings.

## What the console said

- **The depth union does not work on the PVR.** One darkening a direction, by depth "greater" with write, gives texels that are not their level's: Town corner shading, build 0 test 1, 477 to 2,541 wrong texels against plain blending's 460 to 1,764. The cause is OPEN (PVR-127; the same question as PVR-046). Use plain blending (`sh4der_aosky_header(h, 0)`). The union stays in the header for a client that wants to test it.
- **One side gives the same darkening as both sides** for half the vertices: the wrong-texel counts and the values match (PVR-128, MEASURED; the reading DERIVED).

## Recommended limits

- Start with k = 4 directions, one side, plain blending. That is what the scenes of Room shading ran.
- Cap k at what the frame can pay: about 0.35 to 0.45 ms of PVR a direction for scenes of about 1,100 caster triangles (DERIVED).
- Keep the casters closed and wound one way. A caster that is not closed sets its `open` flag and is drawn on both sides.

## Tuning

See `README.md` "Aosky tuning": `SH4DER_AOSKY_DIRS_MAX`, `SH4DER_AOSKY_FLOOR`, `SH4DER_AOSKY_Z0`, `SH4DER_AOSKY_ZSTEP`, `SH4DER_AOSKY_XY_LIMIT`, `SH4DER_AOSKY_AREA_MIN`. `SH4DER_AOSKY_COT_EL` is cot(60 degrees), a constant, not a choice. The ring of directions (22.5 degrees, then every 45) is fixed so that no direction runs along an axis-aligned wall.

## Which occlusion tool

| Tool | What it is | Where it runs |
|---|---|---|
| aosky (this page) | sky directions drawn as shadows into the sun's target | console; the ground only |
| [Baked corner shading [aobake]](aobake.md) (`sh4der_aobake.h`) | rays at exact spheres, boxes and a plane, one byte a vertex, once at load | console |
| [Corner darkening [ao]](ao.md) (`sh4der_ao.h`) | crease darkening from face angles, and a screen cavity cue | console; no rays, no target |
| [Sun shadow map [psm]](psm.md) (`sh4der_psm_pvr.h`) | the sun's shadow target this draws into | console |

## Pitfalls

- A caster that is not closed, or wound both ways, draws its shadow with the wrong darkening on one side. Check the casters' winding first (the Room shading, four ways scene test holds it for the demo's scenes).
- `sh4der_aosky_dir` leaves its matrix in XMTRX. Anything transformed after it with the old matrix is wrong.
- A vertex with `w <= 0` in a warped map drops its triangle. Nothing is clipped. The warped-map case is not handled.
- The receiver is the flat ground. Walls, movers and self-shadowing are not drawn by this.

## Open

- A console run of this copy of the code (the scenes of Room shading through the moved header).
- The cause of the depth union's failure (PVR-127, OPEN).
- A warped-map case. Today a warped map is refused by the triangle drop, not clipped.
- The mesh-shape builder (`meshbuild`) is parked until a second client exists.
- `demo/psm/psm_ao.h`'s triangle-mesh bake moves later as `sh4der_aomesh.h`; it is not this page.

## Related

- [Baked corner shading](aobake.md) (the per-vertex bake at load), [Corner darkening](ao.md) (crease and cavity), [Sun shadow map](psm.md) (the sun's shadow target this draws into)
- [limits: recommended](../limits/recommended-limits.md), [frame budget](../limits/frame-budget.md)
