---
title: "Heat haze reference: floating hexagons, measured from a video"
tags: [research, heat, haze, reference, car]
updated: 2026-11-25
sources: [the owner's reference video (kept outside the repository), demo/pl4nar/src/heat.h]
---
# Heat haze reference: floating hexagons, measured from a video

**What you will learn:** what the heat haze in the owner's reference video does, pixel by pixel: the size of its hexagons, how they move, how far they shift the picture, and where on the screen the haze sits. These are the numbers to match when reworking `demo/pl4nar/src/heat.h`.
**For:** whoever reworks the street demo's heat haze (Senior), and the owner.

> **What shipped is different.** The street demo's heat haze version 2 (`demo/pl4nar/src/heat.h`, not committed) is a smooth wobble under a pixel, not hexagons. The owner approved it as host pictures, 2026-11-25. A first hexagon effect built to this note's numbers (a few floating hexagons that pop on and off, each with a shift rolled ten times a second) was judged wrong by the owner on 2026-11-26: scattered popping tiles are not the look. The library's hexagon effect is now a lattice fixed on the screen and moved by a smooth wave, from the owner's own formulas: [Heat haze hexagons [hexheat]](../effects/hexheat.md). This note stays as the record of what the video shows.

## The source

- **The video.** The owner's reference video of a driving game's heat haze. It stays outside the repository, and the game is not named here.
  - 640 × 480, 30 frames a second, 51.1 s, 1,533 frames, H.264 at about 1.09 Mbit/s.
  - **Frames 1 to 3 and 1,531 to 1,533 are a still of the scene.** The camera is parked: a road leading to a wall, a gantry, banners, trees and a mountain, with distant cars at the road's far end.
  - Every frame between those is black except where the picture differs from that still. So the video is the haze shown as a difference against the still (ESTIMATE: how the video was made is not known).
  - That reading fits the evidence. The logo stays lit for up to 5.9 s straight, which a frame-to-frame difference would not show (MEASURED). The lit edges line up with the still's own edges.
- **New pictures 10 times a second.** The content changes on every third frame only: 509 distinct haze pictures, one every 0.1 s (MEASURED). Whether the effect itself steps at 10 Hz or the capture does is OPEN. Below, **a step is 0.1 s**.
- **Method.**
  - Every frame was extracted; nothing was skipped.
  - The hexagons were tracked in the rows above the dense band, where they stand apart: rows 110 to 200, nearest match within 6 px a step.
  - The shift was measured on the still's clean edges. Across a sharp edge, a shift shows as a bright stripe whose width is the shift along that edge's normal.
  - A fit of a whole shift to 8 × 8 blocks was tried and dropped. It explained only 45 % of the difference, and its answers moved to wherever the search window ended.
- **Limits of this video.**
  - The difference is an absolute value, so the sign of a shift (up or down, left or right) cannot be read.
  - The video's compression adds about 1 level of noise.
  - A flat area (sky, road) shows no difference whatever the shift, so the haze is seen only where the scene has detail.

## The numbers

Each figure is MEASURED from this one video unless it says otherwise.

| What | Figure | How |
|---|---|---|
| **Shape** | Flat-topped hexagons: flat top and bottom edges, points left and right | Crops of the six longest-tracked rings at 10× |
| **Size** | 14 px wide (half of them 13 to 20 px), 13 px tall (10 to 14 px): about **2.7 to 2.9 % of the screen's height**, 2.2 % of its width | Ring extents over 339 tracks of 5 steps or more |
| **How many** | About 20 hexagons a step in rows 110 to 200 alone. In the dense band, a strong edge pixel at row 208 is inside a hexagon 27 % of the time | Blob count; one edge row over 509 steps |
| **Drift** | **Upward, about 0.2 px a step (2 px a second)**; half of the tracks between 0.11 and 0.33 px a step. Sideways about 0 on average, but each track leans its own way: half between -0.14 and +0.11 px a step, so paths go up to about 35° either side of vertical | Straight-line fit to each track |
| **Wobble** | 0.6 px on average about each track's straight line | Residual of the fit |
| **Lifetime** | Median 1.7 s; a quarter over 3.4 s; the longest 11.7 s. A track can break and restart, so these are lower bounds (ESTIMATE) | Track lengths |
| **Fade** | None seen. A ring's brightness is the same over its life (44, 44, 45, 45, 47 levels by fifth of life): hexagons appear and go at full strength (ESTIMATE: brightness is shift × local contrast, not the shift alone) | Ring brightness by age |
| **Shift size** | **1 to 5 px, median 2 px** vertically (mean 2.8, 90 % at or under 5); sideways also median 2 px (mean 2.6, 90 % at or under 4), but from only 3 clean vertical edges, so weak | 5,136 stripe widths on 48 horizontal edges; 290 on 3 vertical edges |
| **Shift direction** | No clear preference between vertical and sideways in these numbers. The sign is unknown (absolute difference) | As above |
| **Shift over time** | The shift under a hexagon changes from step to step: between two steps the same edge pixel's stripe width stays the same in 30 % of cases, changes by 1 px in 38 %, by 2 px in 19 %, by 3 px or more in 13 %. A hexagon moves only 0.2 px a step, so the change is in the shift itself: **each hexagon's shift flickers every 0.1 s** (compression noise is part of the 1 px changes: ESTIMATE) | One edge row, 15,938 pairs of steps |
| **Inside a hexagon** | Not clearly uniform. Along one long edge the stripe width holds steady over some runs of 10 to 15 px (6 6 6 6 6 6) and ramps over others (1 2 3 3 3 4). Both are seen; OPEN | Stripe widths along row 208 |
| **Where on screen** | **A band from row 282 up to about row 130** (59 % to 27 % of the height), across the whole width. The bottom edge is sharp: on the left the mean difference falls from 13.3 to 1.0 between rows 270-279 and 280-289, where the scene's detail is the same. The top fades: on the left mountain, whose detail is even, the difference per unit of detail is 2.0 at row 190, 0.98 at 180, 0.65 at 160, 0.43 at 140, 0.13 at 130 and 0.04 at 120. Densest between rows 190 and 280 | Mean difference over 509 steps, by rows and by columns |
| **Its place in the scene** | The band sits on the distant scene above the road's far end: the wall, the gantry, the banners, the trees. Its bottom edge is at the wall's foot, just above the distant cars (rows 298 to 312) and the road | The still |
| **Near objects** | Not testable: no near object is in view (no player car on screen). The road and its white stripe (rows 330 to 420, strong edges) show no difference at all | Mean difference |
| **Colour and blur** | No general tint: on flat pixels 90 % show no difference (the 90th percentile is 1 level). The lit edges carry the scene's own colours. About 2.7 % of flat pixels differ by more than 20 levels; some of these are bright filled discs, which are OPEN. Blur cannot be told apart from the shift in this video: OPEN | 16 % of the band's pixels are flat (no step over 4 levels within 6 px) |

## What it means for `heat.h` (ESTIMATE)

The current `heat.h` copies a window of the frame and draws it back through a mesh whose samples wander 2 px across and 1 px up and down (`HEAT_AMP_X`, `HEAT_AMP_Y`), over a 384 × 64 band 30 m ahead. The reference is different in kind:

- **Pieces, not a smooth wave.** Separate flat-topped hexagons about 14 px wide (3 % of the height). Each shifts what is under it by 1 to 5 px (median 2), and each hexagon's shift changes every 0.1 s.
- **Motion.** The hexagons rise about 2 px a second, each leaning its own way, live about 2 s, and appear and go without a fade.
- **Placement.** A band sitting on the horizon. Its bottom edge is sharp at the road's far end and its top fades out over about 150 px. It is thickest near the horizon and covers the full width.
- **On the PVR.** One copied window as today, drawn as small hexagon fans (6 triangles each) with the texture coordinates shifted per hexagon, in place of one wandering mesh. The draw would cost a few hundred vertices; the copy stays as it is.

## Pictures

The reference pictures were recycled (restorable from the Recycle Bin). They were kept outside the repository, so the video's frames stayed out.

## Related

- [Frame budget, section 6](../limits/frame-budget.md#6-the-car-demo-practices-so-far): the heat haze's cost (ESTIMATE)
- [Depth shading ideas](depth-shading-ideas.md): X56, the window copy as a general tool
- [Suite 13](../methods/suite-13.md): no heat stage in its committed list
