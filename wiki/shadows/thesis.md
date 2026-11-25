---
title: "The thesis: cached per-vertex sun visibility"
tags: [shadows, research]
updated: 2026-11-25
sources: [demo/psm/THESIS_SH4_SHADOWS.md, demo/psm/GPU_GEMS_NOTES.md]
---
# The thesis: cached per-vertex sun visibility

**What you will learn:** the idea in `demo/psm/THESIS_SH4_SHADOWS.md`, its predictions, and which have evidence.
**For:** anyone deciding whether to build it, and anyone writing a test for it.

**Status:** a design document, draft of 2026-11-25 (`THESIS_SH4_SHADOWS.md:3`). Its ray bench ran twice on one console on 2026-11-25, ray1 and ray2, and tested P1, P2 and P9 ([SH-4 ledger](../hardware/ledger-sh4.md#ray-bench-ray1-thesis-p1-p2-p9), SH4-051 to SH4-055; [ray2](../hardware/ledger-sh4.md#ray-bench-ray2-repeat-of-ray1-the-clock-the-ray-triangle-test), SH4-056 to SH4-066). ray2 also measured the ray-triangle test that the exact-triangle part needs: 33 cycles a test prepared for the sun, 62 for any direction (SH4-066). The rest of the design is not built.

## The idea, in plain words

Do not draw the sun's shadow again every frame. Most of a scene does not move, and the sun moves slowly. Instead:

- Keep **one byte of sun visibility for every static vertex**.
- Work it out with rays through a coarse height grid, using exact triangles only where there are overhangs.
- Bake it at load. When the sun moves, redo a slice of the vertices each frame.
- Treat moving things (people, animals) as simple shapes (spheres, capsules, boxes), checked only on receivers near them.
- Write the result into the vertex colour, which costs the PVR nothing. An optional overlay can sharpen edges.

## Worked numbers (all ESTIMATE)

These rest on the step and set-up costs that ray1 overturned (P1 below; errata O10, O11). They are kept as the thesis wrote them and need redoing.


- A full cast of the town: about 48.5 ms (line 242).
- At 14:00 with a 2 ms slice, about 180 vertices a frame must be redone; 1,183 fit.
- At 16:00, 719 must be redone and 570 fit. The shadow lags by up to about 1.9 m (line 439).

## Predictions

| # | Prediction | Test | Evidence |
|---|---|---|---|
| P1 | One grid step costs 12-16 cycles; set-up about 30 | bench | **overturned** by ray1 and again by ray2 (two runs, twelve fits): 22.07 to 22.21 cycles a step and 70.88 to 76.04 to set up, on both suns and both grids (SH4-052, SH4-059) |
| P2 | One batch keeps the grid in the cache | bench | **mixed** (ray1 and ray2): batching beats interleaving with the vertex transform by 89 to 164 cycles a ray, a spread between the runs that is OPEN (SH4-053, SH4-062, Q68); the predicted cold cost of at most 18 us was 46 to 47 us in both runs (the other purged arrays may hold the rest, ESTIMATE), and 0 with the grid in OC-RAM; OC-RAM made the interleaved slice slower in both runs (SH4-054, SH4-063). The stand-ins cost less at 16:00 than at 14:00, so 5.9's 16:00 figure is untested (Q63) |
| P3 | A 2 ms slice follows a sun at ×60 at 14:00 but not 16:00 | console | none |
| P4 | The interval variant matches a fresh ray cast | host, then console | none |
| P5 | It agrees with route B's `lit_plane` | host | none |
| P6 | A step-texture overlay draws an edge at half lit | console | none |
| P7 | Punch-through does not threshold vertex alpha | console | **overturned** by spike c4 test 6 (one run): vertex alpha 128 drew 2,048 of 32,768, so the tested alpha is texture × vertex (PVR-064). c1's "every pixel drawn" was the ONE/ZERO blend (PVR-061), not a sign about vertex alpha |
| P8 | Culling unseen groups saves time in proportion to their share | host, console | none |
| P9 | Proxies for 8 people and 4 hens cost less than one slice | bench | **stands** (ray1 and ray2): 312 us a pass, 15.6 % of a 2 ms slice; but 137.80 to 137.87 cycles a pair, above the thesis's 15 to 100 (SH4-055, SH4-064, errata O12) |
| P10 | The frame model explains the town HUD's frame rate | console | none |

The bench that tests P1, P2 and P9:

![The ray bench, run on one console on 2026-11-25 (image psm-bench-ray1, one run, MEASURED; photos in bench/RAY1_RESULTS_cortana.md and _junior.md): a ray walking the 4 KB height grid costs 22.07 to 22.21 cycles a step and 70.88 to 74.09 to set up (the page's fit, P1.1 to P1.4), against the thesis ESTIMATE of 12 to 16 and about 30, so P1 is overturned; the slice of 1,024 stand-in rays costs 563.36 / 548.78 cycles a ray cold and 554.19 / 539.68 warm (14:00 / 16:00) batched, interleaving with the vertex transform adds 117 / 89 cycles a ray (DERIVED; 27.7 to 40.3 cycles a miss, SH4-047, so the miss count is an ESTIMATE), and the grid in OC-RAM makes the interleaved slice slower overall (P2); the movers' proxy pass takes 312 us, under a 2 ms slice, so P9 stands. OPEN: CLOCK.2's cycles a ns, the P1.4 n 48 ns gap, why 16:00 costs less than 14:00 with these stand-ins.](../images/ray-bench-plan.svg)

## What has changed since it was written

- ray1 ran (one console, 2026-11-25). P1's constants are overturned and 5.9's budget (338 and 702 cycles a ray, k ≤ 1,183 and ≤ 570) rests on them: P1.4's fit at the thesis's mean n 22 gives 560 cycles, and the slice measured 554 (errata O10, O11).
- The 27.7-cycle cost of a missing step (section 5.9) comes from 20,000 reads over a 32 KB map, twice the operand cache, so it mixes hits and misses; on a 512 KB map the same read costs 40.3 cycles ([SH4-047](../hardware/ledger-sh4.md)). ray1's interleaving extra, 89 to 117 cycles a ray, is 2.2 to 4.2 such reads if all of it is grid misses (ESTIMATE).
- Line 498 says chip bench h1 "has not run". It has, and h2 too ([SH-4 ledger](../hardware/ledger-sh4.md)). Their cache numbers bear on P1, P2 and P9.
- F20 and F21 say c3 and c2 are "built, not run". c3b has run ([bump compare chain](bump-compare-chain.md)); c2 has not.
- P7 named the run that would overturn it, and c4 test 6 did (PVR-064). The thesis text still says "supported".
- Its town figure (25 fps, 28,699 TA vertices) has no filed photo (errata O5). The filed photos are in [scenes](../limits/scenes.md).

## Its own warnings (lines 517-533)

- Every measurement is one console and one run.
- Every cycle count is a model.
- The tower is 24 m with its roof to 28 m, but some tables assume 13 m.
- A height grid has no overhangs, and wall vertices hit themselves.
- A sun turned by hand (30°/s) cannot be followed by any slice.

## Related

- [Shadows](README.md)
- [PSM routes](psm-routes.md)
- [Open questions](../open-questions.md)
