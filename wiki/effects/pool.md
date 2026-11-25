---
title: pool (light pools on a plane)
tags: [effect, pool]
updated: 2026-11-25
sources: [include/sh4der_pool.h, README.md "The pool effect", AGENTS.md "Call order: pool"]
---
# pool: light pools on a plane

**What you will learn:** how a lamp's pool of light or a blob shadow is drawn as a fading fan, and why fill is its cost.
**For:** a programmer lighting coarse ground under street lamps or headlights, or adding a blob shadow under a car.

## What it does

A small fan of triangles lying on the ground. Its vertex alpha fades to nothing at the rim (a disc) or at the far arc (a wedge). It is drawn translucent, after the ground (README:946-972):

- a disc: a street lamp's pool, or an ellipse for a blob shadow;
- a wedge: a headlight's beam.

Header only. You transform and submit the vertices.

Version: v0.10, passes its maths tests (pool.h:39).

## Call order

The full steps are in `AGENTS.md` "Call order: pool":

1. At load: compile two untextured translucent headers, depth test on, depth write off, no culling. One is additive, for light; one is blended, for shadow.
2. Each frame, in the translucent list after the ground: `sh4der_pool_disc` per lamp near the camera, `sh4der_pool_wedge` per headlight.
3. Transform the vertices and send the fan. Test each triangle with `sh4der_tri_area_ok`.
4. A blob shadow is a disc of colour 0, alpha about 0.4, through the blended header.

## Cost

| Item | Cost | Status |
|---|---|---|
| RAM | 16 B a vertex in your array (176 B for a 10-triangle disc) | as-stated (README:955) |
| CPU per rim vertex | one sine-cosine pair and six FMAs | as-stated (README:955) |
| PVR fill of the fan | not measured | OPEN (README:955) |
| VRAM | 0 | |

## Recommended limits

- `SH4DER_POOL_MAX_N` 16 triangles a fan (pool.h:62). Your array must hold `MAX_N` + 2 vertices (README:966).
- Limit lamps by distance: a pool is fill, and fill is what the translucent list costs (`AGENTS.md`). This is ADVISED; see [recommended limits](../limits/recommended-limits.md).

## Tuning

See `README.md` "Pool tuning" (README:968-972).

## Pitfalls

- Lift the fan 2-4 cm off the ground (README:953).
- A fan seen edge-on has no area. Use the area [guard](guard.md) (README:954).
- For a lamp's halo use a camera-facing quad ([spark](spark.md)). For its reflection on a body, draw that quad in the [pl4nar](pl4nar.md) mirror pass.

## Open

- The PVR's fill cost of a fan on a console.

## Related

- [guard](guard.md), [spark](spark.md), [pl4nar](pl4nar.md), [sh4dow-dc](sh4dow-dc.md)
