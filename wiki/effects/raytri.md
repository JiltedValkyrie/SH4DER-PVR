---
title: "A ray against a triangle [raytri]: one kernel for the bakes and the ray bench"
tags: [effect, routine, ray, triangle, bake, ao, prototype]
updated: 2026-11-26
sources: [include/sh4der_raytri.h, test/raytri_test.c, include/sh4der_aomesh.h, demo/psm/bench/ray_kernels.h, demo/psm/bench/ray_test.c, README.md "The raytri effect", AGENTS.md]
---
# A ray against a triangle [raytri]: one kernel for the bakes and the ray bench

**What you will learn:** what it means to ask whether a ray hits a triangle, the one piece of arithmetic that answers it for the library's bakes and the ray bench, what it can and cannot promise (it is not watertight, and its hit test has no far limit), and how to call it.
**For:** a programmer who casts rays at triangles (a bake, a shadow test, a pick) and wants the same answer everywhere, with no divide where none is needed.

> **In plain words.** Picture a torch beam aimed in one direction at a sheet of card floating in the air. Does the beam hit the card, and if it does, how far along the beam did it land? This file is the maths that answers that for one beam and one sheet. It works out a few numbers, checks each one against a simple rule, and only then does the one division needed for the distance. Two other pieces of code used to do this sum in their own ways. Now both call this one copy, so they agree.

## Try this first

Nothing on screen. It is a routine: a bake or a test calls it. There are no tuning macros, so there is nothing to set.

1. For one ray and one triangle, call `sh4der_raytri_num(o, d, a, e1, e2, &n)`. `a` is a corner, `e1` and `e2` are the two edges from that corner (`b - a`, `c - a`). It returns 0 when the ray misses the triangle's plane or lands outside it. Otherwise it returns 1 and fills `n`.
2. For a hit test with no divide, call `sh4der_raytri_hit(&n, 1e-4f)`. It returns 1 when the ray meets the triangle further than 1e-4 along it, from either side.
3. For a distance, call `sh4der_raytri_t(&n, reach, &recips)`. It returns the distance when it is above 1e-4 and below `reach`, else -1. `recips` must not be NULL.
4. For many rays along one direction over many triangles, prepare each triangle once with `sh4der_raytri_dir_prep`, then call `sh4der_raytri_dir_hit` per ray.

Where the numbers come from: `include/sh4der_raytri.h` (the functions), `test/raytri_test.c` (the checks, run by `make test`).

## What it does

It takes the ray's origin `o`, its direction `d` and the triangle's corner and edges. Then:

1. The determinant `det = e1 . (d x e2)` is the first test. When it is within 1e-9 of zero, the ray runs (nearly) parallel to the triangle's plane, and the ray is refused.
2. If `det` is negative, the sign is taken off once: `det`, and the numbers after it, are each negated. A negate is exact, so this is the same test as the two-sided form, with one copy of the comparisons.
3. Two edge numbers `un` and `vn` are compared with `det`. Outside the triangle, the ray is refused. The order is `u`, then `v`, then `u + v`.
4. The last number `wn` says how far along the ray the hit is, as `wn / det`. The hit test compares it with `eps * det` (no divide). The distance test divides once, by the reciprocal of `det`.

The division is the only one. The hit test has none.

## Used by

- `include/sh4der_aomesh.h` (`sh4der_aomesh_tri_`): the mesh bake's test for each ray and triangle, then the distance. Its bytes are pinned, and the pin is unchanged by this kernel (see "Limits" and the README section).
- `demo/psm/bench/ray_kernels.h`: `ray_tri` (the general form, with the bench's own |det| rule on top) and `ray_tri_sun` (the prepared form for one sun). The bench's reference functions (`ray_tri_ref`, `ray_tri_sun_ref`) stay as they were, so the bench still checks the kernel against them.
- Not used by `tools/ao_bake_core.h`. That host tool scales its numbers by the reciprocal before it compares them: a different arithmetic, which would move its bytes. It is left alone.

## Limits

- **Not watertight.** Each triangle is worked out from its own corner and edges. A ray through an edge that two triangles share is taken by at least one of them on the test's sampled diagonal (both windings), but no proof covers every ray. A watertight test needs one shared ordering of the edge functions. That is a later option and is NOT in this change.
- **No far limit in the hit test.** `sh4der_raytri_hit` answers "is the hit beyond eps", at any distance. Only `sh4der_raytri_t` takes a reach.
- **A NaN direction misses.** The numbers come out NaN and every comparison with NaN is false, so the hit and the distance are misses. The numerators are still returned as numbers.
- **Two sizes of the same test.** The band (1e-9) and the distance floor (1e-4) are the constants the callers have always used. The bench refuses a |det| of exactly 1e-9 and a NaN, where the library's band does not: that is the bench's own rule, kept in `ray_tri`.
- **A later option, NOT in this change:** a reach test with no reciprocal would save the division, but it compares in a different arithmetic and can move the mesh bake's bytes at the limit.

## Cost

| Item | Cost | Status |
|---|---|---|
| The numerators | one cross product and three dot products, then the edge tests | by construction |
| A distance | one reciprocal, taken only for a triangle that passed the tests | counted on the host (the mesh bake's count: 134,879 reciprocals in the town) |
| The hit test | no divide | by construction |
| Time on the SH-4 | NOT MEASURED | the bench (`demo/psm/bench`) is the place to measure it; its cycle figure (TOWN-063 assumes 50) is NOT RE-MEASURED |
| RAM | nothing kept; 64 B a prepared triangle a direction | exact, by the code |
| VRAM | 0 | |

## Checks

`test/raytri_test.c` (run by `make test` and `make test-shz`):

- Edge cases: a parallel ray is refused; a triangle and its reverse (the two signs of `det`) both take the same ray from above and from below; a ray through the edge at `u = 0` and one through the edge at `u + v = 1` hit, and one a little past the second misses; the distance of the `u = 0` ray is exactly 1, and a reach under it gives -1; a distance under 1e-4 is -1; a NaN ray misses.
- The shared diagonal of a quad: 8,194 rays, 4,097 samples of the diagonal for each winding. The first winding is shot from above, the second from below. None is missed by both triangles (in this run every one was taken by both, since the diagonal is the closed edge of each triangle, so the test does not show a gap; it shows no miss).
- 10,000 random rays (half of them aimed near the triangle): the hit and the distance agree. In this run there were 0 disagreements, and any would have to be within 1e-5 (relative) of eps. A double-precision test agrees with the hit (0 differ).
- 10,000 random rays: the prepared form against the general numbers, by the term magnitudes (largest scaled difference 2.7e-7 on the plain C path; the tolerance is 1e-5).

- 10,000 rays aimed at the limits (within 1e-6 of each edge, and at a distance near eps) and 10,000 numerators within 8 ulp of eps: here every disagreement branch runs. In this run: the hit and the distance disagree on 184 (all within 5.6e-8 relative of eps); the hit and the double test differ on 687 (all within 1e-4 of a limit); the prepared and the general form differ on 438 (all within 1e-4 of a limit).

The check counts and the printed numbers are in the test's output. The sampled checks are not proofs.

**The bench's bytes were moved on purpose.** Until 2026-11-26 the prepared form kept the bench's plain divide. It now takes the reciprocal through SH4ZAM like every other reciprocal in the library, because that is the library's rule. On the host nothing moved: 100,000 random triangles and 2,000,000 rays, 0 ulp and 0 flipped hits on both paths, and the bench's dump byte-equal. On a console the SH-4's reciprocal is an approximation, so the bench's bytes there may differ: NOT MEASURED.

## Recommended limits

- Use it for a bake or a test that must give the same answer on the host and the console. Do not use it as a watertight edge test: a ray through a shared edge can be missed by both triangles in principle.
- A hit test with no far limit: say so in the caller, and give the distance a reach when it matters.
- Status PROTOTYPE v0.1, host tested. The console run is NOT RUN. Time is NOT MEASURED.

## Which kind of test is this

| You want | Call | Costs a divide? |
|---|---|---|
| Is there a hit beyond eps? | `sh4der_raytri_hit` on the numerators | no |
| How far is the hit? | `sh4der_raytri_t` | one reciprocal |
| Many rays, one direction | `sh4der_raytri_dir_prep` once, `sh4der_raytri_dir_hit` per ray | one reciprocal a triangle (prepare) |

## Start-here links

- Shadows and occlusion: [genre-shadows](../start-here/genre-shadows.md). The bake that uses this is [aomesh](aomesh.md).
- Call orders: [call-orders](../start-here/call-orders.md).
- A bigger picture of what each routine is for: [what-do-you-want](../start-here/what-do-you-want.md).

## Related

- [aomesh](aomesh.md): the baked occlusion over a triangle mesh, the first caller.
- [aobake](aobake.md): the baked occlusion against exact shapes (spheres, boxes, a plane), which does its own tests.
- `demo/psm/bench/ray_kernels.h` and `demo/psm/bench/ray_test.c`: the ray bench, a second caller.

## Related

- [What do you want to make?](../start-here/what-do-you-want.md): the starter, for a picture or a goal.
- [Tools and safety](../start-here/genre-tools-and-safety.md): the genre guide that puts this routine with the other tools.
- [Mesh corner shading](aomesh.md): the bake that calls this kernel for each vertex.
