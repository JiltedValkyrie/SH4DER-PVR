---
title: "Off-screen skip [viewcull]: leave out what the camera cannot see before it is placed"
tags: [effect, viewcull, routine, camera, prototype]
updated: 2026-11-28
sources: [include/sh4der_viewcull.h, test/viewcull_test.c, demo/psm/psm_scene.h (PSM_PLACE_GROUND), demo/psm/test/psm_place_cmp.c, README.md "The viewcull helper"]
---
# Off-screen skip [viewcull]: leave out what the camera cannot see before it is placed

**What you will learn:** how a program can tell, cheaply, that a whole box or a whole patch of ground is off the screen, why that matters most while the camera turns, and what has and has not been proven.
**For:** a programmer whose ground, terrain or object list is placed on the screen again every frame the camera moves.

> **In plain words.** Before a point can be drawn, the SH-4 works out where it lands on the screen ("placing" it). A still camera can keep last frame's answers, but a turning camera has to work them all out again, including the many points that end up off the screen. This helper draws a box round a group of points and asks one quick question: is the whole box off the screen? If it is, none of those points are placed. The picture does not change. Only the work does.

## Try this first

1. Keep the block size at `SH4DER_VIEWCULL_BLOCK` = 4 (4 x 4 cells, so 5 x 5 points a block) (include/sh4der_viewcull.h; README "Viewcull tuning"). ADVISED: no console run has tried another size.
2. Once a frame, after the camera is set: `sh4der_viewcull_prepare(&vc, rows, znear, zfar)` with the camera's world-to-clip rows, the near plane and the draw distance (0 for none). ADVISED.
3. For a grid of points: `sh4der_viewcull_grid(&vc, pts, rows, cols, SH4DER_VIEWCULL_BLOCK, seen)`, then place only the points of blocks with `seen` 1, and set every cell of a block with `seen` 0 to "not sent". ADVISED.
4. For one object with a box: `sh4der_viewcull_box(&vc, lo, hi)`. A 0 means skip it. ADVISED.

On a console: NOT RUN. The host counts are below.

Header: [include/sh4der_viewcull.h](../../include/sh4der_viewcull.h). Call order: [Call order](#call-order).

## What it looks like

Nothing changes on the screen. Turning the camera should cost the SH-4 less.

## What it does

The screen's edges, the near plane and the draw distance are six flat planes in the world. A box is off the screen when it lies wholly outside one of them. For each plane the helper finds the box corner that reaches farthest inward, using two dot products (FIPR on the SH-4). It does not transform all eight corners. When that corner is still outside, so is the whole box.

It keeps any box within rounding of an edge, and any box whose numbers are not finite. A box it leaves out is never one that the eight-corner test would keep (test/viewcull_test.c, 20,000 boxes).

For a grid, it makes the box of each block from the block's own points, so the box holds every triangle drawn between them.

Header only. CPU only. It does not load or read the SH-4's matrix register (XMTRX), so the caller's matrix stays loaded.

Version: PROTOTYPE v0.1, host tested (`test/viewcull_test.c`).

## Where it came from

Notes from the shadow demo's camera-turn deep dive (2026-11-28, ESTIMATE and DERIVED, not console figures):

- **Why turning was slow.** A turning camera placed the walled square's whole ground window and every model again each frame (the cost model gave 89 to 118 % of the measured rise of the frame's build part, PVR-198).
- **The ground was the part left.** The placement options already skipped models out of view. Even with them on, the ground window was still placed in full: +5 to +9 ms at the boot view (DERIVED from PVR-210 and PVR-211).
- **The demo's switch.** The demo's ground option, `PSM_PLACE_GROUND`, is this helper's first client. It is off by default.

## Call order

1. Once a frame: `sh4der_viewcull_prepare(&vc, rows, znear, zfar)`. The rows are in column order: clip X = m[0] x + m[4] y + m[8] z + m[12], with W (the view depth) in row 3. That is the form `sh4der_psm_cam_rows` writes.
2. A grid: `sh4der_viewcull_grid_blocks(rows, cols, bs, &nbr, &nbc)` sizes the `seen` bytes. `sh4der_viewcull_grid(...)` fills them, and `sh4der_viewcull_grid_index(r, c, bs, nbc)` gives the block of cell (r, c).
3. A box: `sh4der_viewcull_box(&vc, lo, hi)`, or `sh4der_viewcull_fit` first to make the box of a list of points.
4. Never read a placed value (clip or screen position) of a point in a block that was left out. Nothing wrote it this frame.

## Cost

| Item | Cost | Status |
|---|---|---|
| RAM | one `sh4der_viewcull_t` (196 B), a byte a block | as stated (header) |
| CPU, a box | 12 FIPR (two a plane, six planes) and a few compares | ESTIMATE (not measured) |
| CPU, a grid | one pass over its points for the blocks' boxes, then a box test a block | ESTIMATE (not measured) |
| Ground points placed, square's views, all four options | sky 0 of 5,141; boot 4,078 of 5,335 (76 %); down 4,381 of 6,111 (72 %) | DERIVED (host counts, demo/psm/test/psm_place_cmp.c) |
| VRAM | 0 | |

No row in the ledger backs a time here.

## Recommended limits

- Cull before placing. A box out of view costs 12 FIPR, but its points cost a transform each.
- Status ADVISED. To prove it, run the walled square's turning stages with `PSM_PLACE_GROUND` on and off (vc-g's script, stage option 128). Read p0 and the frame on a console.

## Tuning

| Name | Default | Meaning |
|---|---|---|
| `SH4DER_VIEWCULL_BLOCK` | 4 | Cells a side of a grid's block |

A smaller block leaves out more and costs more boxes. A larger one costs fewer boxes and leaves out less. Neither has been timed.

## Pitfalls

- A block that is kept may still have cells off the screen. The caller's own cell tests still run on kept blocks.
- The matrix must be the world-to-clip one with W as the view depth. A matrix in another layout culls the wrong boxes.
- Planes only: it knows nothing of walls or hills in front of a box, so it does no occlusion culling.

## Open

- What it saves on a console while the camera turns (vc-g with stage option 128, NOT RUN).
- Whether a block of 4 is the best size on the SH-4.

## Related

- [Sun shadow map [psm]](psm.md) (the demo that uses it), [Send checks [guard]](guard.md), [Distance ramp [ramp]](ramp.md), [Tools and safety guide](../start-here/genre-tools-and-safety.md)
