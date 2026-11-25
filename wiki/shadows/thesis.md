---
title: The thesis: cached per-vertex sun visibility
tags: [shadows, research]
updated: 2026-11-25
sources: [demo/psm/THESIS_SH4_SHADOWS.md, demo/psm/GPU_GEMS_NOTES.md]
---
# The thesis: cached per-vertex sun visibility

**What you will learn:** the idea in `demo/psm/THESIS_SH4_SHADOWS.md`, its predictions, and which have evidence.
**For:** anyone deciding whether to build it, and anyone writing a test for it.

**Status:** a design document, draft of 2026-11-25. No code written, nothing run (`THESIS_SH4_SHADOWS.md:3`).

## The idea, in plain words

Do not draw the sun's shadow again every frame. Most of a scene does not move, and the sun moves slowly. Instead:

- Keep **one byte of sun visibility for every static vertex**.
- Work it out with rays through a coarse height grid, using exact triangles only where there are overhangs.
- Bake it at load. When the sun moves, redo a slice of the vertices each frame.
- Treat moving things (people, animals) as simple shapes (spheres, capsules, boxes), checked only on receivers near them.
- Write the result into the vertex colour, which costs the PVR nothing. An optional overlay can sharpen edges.

## Worked numbers (all ESTIMATE)

- A full cast of the town: about 48.5 ms (line 242).
- At 14:00 with a 2 ms slice, about 180 vertices a frame must be redone; 1,183 fit.
- At 16:00, 719 must be redone and 570 fit. The shadow lags by up to about 1.9 m (line 439).

## Predictions

| # | Prediction | Test | Evidence |
|---|---|---|---|
| P1 | One grid step costs 12-16 cycles; set-up about 30 | bench | none |
| P2 | One batch keeps the grid in the cache | bench | none |
| P3 | A 2 ms slice follows a sun at ×60 at 14:00 but not 16:00 | console | none |
| P4 | The interval variant matches a fresh ray cast | host, then console | none |
| P5 | It agrees with route B's `lit_plane` | host | none |
| P6 | A step-texture overlay draws an edge at half lit | console | none |
| P7 | Punch-through does not threshold vertex alpha | console | **overturned** by spike c4 test 6 (one run): vertex alpha 128 drew 2,048 of 32,768, so the tested alpha is texture × vertex (PVR-064). c1's "every pixel drawn" was the ONE/ZERO blend (PVR-061), not a sign about vertex alpha |
| P8 | Culling unseen groups saves time in proportion to their share | host, console | none |
| P9 | Proxies for 8 people and 4 hens cost less than one slice | bench | none |
| P10 | The frame model explains the town HUD's frame rate | console | none |

## What has changed since it was written

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
