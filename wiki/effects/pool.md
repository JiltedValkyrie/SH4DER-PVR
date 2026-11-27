---
title: Light pools [pool] (fading round or wedge-shaped light on a plane)
tags: [effect, pool]
updated: 2026-11-26
sources: [include/sh4der_pool.h, README.md "The pool effect", AGENTS.md "Call order: pool"]
---
# Light pools [pool]: fading round or wedge-shaped light on a plane

**What you will learn:** how a lamp's pool of light or a blob shadow is drawn as a fading fan, and why fill is its cost.
**For:** a programmer lighting coarse ground under street lamps or headlights, or adding a blob shadow under a car.

> **In plain words.** A patch of light lying on the ground, like the pool under a street lamp or a headlight's beam. It is brightest in the middle and fades to nothing at the edge, so it has no hard rim. A dark version under a car makes a blob shadow. Its cost is how much of the screen it covers, so use it where the ground is too coarse to light corner by corner.

## Try this first

For light or a blob shadow lying on the ground, drawn as a fan of triangles that fades to nothing.

1. A disc of 10 triangles for a lamp, 8 for a blob shadow, and a wedge of 2 to 4 for a headlight (include/sh4der_pool.h:8; ADVISED, the street demo). The cap is `SH4DER_POOL_MAX_N` = 16 (include/sh4der_pool.h:98; ADVISED, README:1078).
2. Lift the centre 2 to 4 cm off the ground. A blob shadow's alpha is 0.50 by day and 0.25 at night (include/sh4der_pool.h:8-10; ADVISED, the street demo's values).
3. Fade by distance: `SH4DER_POOL_REACH` = 60 m and `SH4DER_POOL_FADE_W` = 10 m, with `sh4der_pool_reach_make` once a frame and `sh4der_pool_reach_fade` per lamp (include/sh4der_pool.h:100-101; ADVISED, README:1080-1081; the fade is PROTOTYPE, run on a console by the owner in Street car demo, Car on hills and Drive a car on hills (her test; no measured numbers recorded yet), include/sh4der_pool.h:28-29).
4. Do not put a reach on the player's own headlights: they are always near, so use a wedge for those (include/sh4der_pool.h:32).
5. How many lamps to draw: no recommended value yet. Test: the same view with pools on and off, no key pressed, PVR time (include/sh4der_pool.h:14).

Run on a console by the owner in Street car demo, Car on hills and Drive a car on hills (her test; no measured numbers recorded yet) for the fade. The beam's switch is off in the listed demos. The disc and the wedge are ADVISED, and their console test is step 5 (include/sh4der_pool.h:14).

Header: [sh4der_pool.h](../../include/sh4der_pool.h). Steps in full: [Call order](#call-order).

## What it does

A small fan of triangles lying on the ground. Its vertex alpha fades to nothing at the rim (a disc) or at the far arc (a wedge). It is drawn translucent, after the ground (README:946-972):

- a disc: a street lamp's pool, or an ellipse for a blob shadow;
- a wedge: a headlight's beam;
- a fade by distance: a lamp's pool is scaled by a ramp that is 1 inside a reach less a width and 0 at the reach, so it does not pop in (`sh4der_pool_reach_make`, `sh4der_pool_reach_fade`);
- a beam: a wedge with arcs along its length, each vertex faded by its own distance from the eye, for another car's headlight (`sh4der_pool_beam`).

Header only. You transform and submit the vertices.

Version: v0.10, passes its maths tests (pool.h:39).

## Call order

The full steps are in `AGENTS.md` "Call order: pool":

1. At load: compile two untextured translucent headers, depth test on, depth write off, no culling. One is additive, for light; one is blended, for shadow.
2. Each frame, in the translucent list after the ground: `sh4der_pool_disc` per lamp near the camera, `sh4der_pool_wedge` per headlight.
3. Transform the vertices and send the fan. Test each triangle with `sh4der_tri_area_ok`.
4. A blob shadow is a disc of colour 0, alpha 0.50 by day down to 0.25 at night (the street demo's values), through the blended header.
5. The fade by distance: `sh4der_pool_reach_make` once a frame; `sh4der_pool_reach_fade` a lamp; skip the lamp at 0, else multiply the pool's and the halo's alpha by it. For another car's headlight, `sh4der_pool_beam` with the reach and the eye, and `sh4der_pool_beam_tris`.

## Cost

| Item | Cost | Status |
|---|---|---|
| RAM | 16 B a vertex in your array (176 B for a 10-triangle disc) | as-stated (README:955) |
| CPU per rim vertex | one sine-cosine pair and six FMAs | as-stated (README:955) |
| PVR fill of the fan | not measured | OPEN (README:955) |
| VRAM | 0 | |
| `sh4der_pool_reach_make` | one FSRRA reciprocal a call | ESTIMATE (pool.h) |
| `sh4der_pool_reach_fade` | a subtract, a multiply and a saturate a lamp | ESTIMATE (pool.h) |
| `sh4der_pool_reach_verts` | one FSRRA square root a vertex that has alpha | ESTIMATE (pool.h) |
| A beam of n x rows | 1 + rows (n + 1) vertices, n + 2 n (rows - 1) triangles (2 x 3: 10 and 10; the wedge: 4 and 2); the wedge's fill | exact counts; time NOT MEASURED |

## Recommended limits

- `SH4DER_POOL_MAX_N` 16 triangles a fan (pool.h:62). Your array must hold `MAX_N` + 2 vertices (README:966).
- `SH4DER_POOL_REACH` 60 m and `SH4DER_POOL_FADE_W` 10 m: the street demo's lamp values, a start value, ADVISED (run on a console by the owner in Street car demo, Car on hills and Drive a car on hills for the fade as library code, her test; no measured numbers recorded yet). To prove: lamps driven past the reach with the fade against a hard cut.
- `SH4DER_POOL_BEAM_ROWS` 3 arcs a beam, at most `SH4DER_POOL_BEAM_ROWS_MAX` 4: ADVISED, not measured.
- Limit lamps by distance: a pool is fill, and fill is what the translucent list costs (`AGENTS.md`). This is ADVISED; see [recommended limits](../limits/recommended-limits.md).

## Tuning

See `README.md` "Pool tuning" (README:968-972).

## Pitfalls

- Lift the fan 2-4 cm off the ground (README:953).
- Do not call `sh4der_pool_reach_make` for each lamp: once a frame (it takes a reciprocal).
- Do not put a reach on the player's own headlights: they are always near. Use the wedge.
- A fan seen edge-on has no area. Use the area [Send checks](guard.md) (README:954).
- For a lamp's halo use a camera-facing quad ([Sparks and tracers](spark.md)). For its reflection on a body, draw that quad in the [Planar reflection](pl4nar.md) mirror pass.

## Status of the parts

- The disc and the wedge: ALPHA v0.2.
- The fade by distance and the beam: PROTOTYPE v0.1, added 2026-11-26. Host tested (`test/pool_test.c`: the reach fade gives the same bits as the street demo's old lamp line on the plain C path). Run on a console by the owner in Street car demo, Car on hills and Drive a car on hills (her test; no measured numbers recorded yet) for the fade; the beam has no figures. The street demo's hills build calls `sh4der_pool_reach_fade` for its lamps; its headlights are plain wedges unless `demo_auto_beam(1)` or `-DPL4NAR_HEAD_BEAM` turns the beam on (off at the start; no autorun stage uses it yet; the hills build keeps the wedge and scales it by the car's distance).

## Open

- The fade by distance and the beam on a console: the look and the cost.
- The PVR's fill cost of a fan on a console.

## Related

- [Soft edges for light](softfade.md), [Send checks](guard.md), [Sparks and tracers](spark.md), [Planar reflection](pl4nar.md), [Soft character shadows](sh4dow.md)
