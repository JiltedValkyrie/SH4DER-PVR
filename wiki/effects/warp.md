---
title: Screen warp [warp]
tags: [effect, warp, post]
updated: 2026-11-26
sources: [include/sh4der_warp.h, src/sh4der_warp.c, README.md "The warp effect", demo/psm/bench/NOTES.md "Hardware result h2"]
---
# Screen warp [warp]

**What you will learn:** how the warp bends the frame through a mesh, and what it measured on a console.
**For:** a programmer adding shockwaves, heat haze, underwater wobble or speed blur.

> **In plain words.** The warp bends the finished picture, the way heat, water or a blast would. A ring can push outward like a shockwave, a column can shimmer like haze over a fire, and the whole screen can wobble as if it were underwater. Use it for explosions, heat haze, underwater levels, a boost blur or a retro wipe between scenes. It draws the frame, copied into a [render-to-texture](../glossary.md#drawing) picture, through a grid whose points are pushed, so the picture bends while the grid stays put.

## Try this first

Bend the finished picture with a shockwave ring, heat haze, a wobble, a speed blur or big pixels.

1. The frame texture is 512 x 256 RGB565, with the 320 x 240 picture in its top-left corner (`SH4DER_WARP_TEX_W` = 512, `SH4DER_WARP_TEX_H` = 256, `include/sh4der_warp.h:69-70`, ADVISED). The Cost section keeps the note on the README's sizes.
2. Grid: 20 x 15 cells for the coarse mesh (`SH4DER_WARP_GRID_X` = 20, `SH4DER_WARP_GRID_Y` = 15, `include/sh4der_warp.h:72-73`), or 40 x 30 for the fine one (`SH4DER_WARP_FINE_X` = 40, `SH4DER_WARP_FINE_Y` = 30, `include/sh4der_warp.h:74-75`). Build each grid row with `sh4der_warp_row` (`include/sh4der_warp.h:206`, ADVISED).
3. Shockwave: `SH4DER_WARP_RING_SPEED` = 180 pixels a second, `SH4DER_WARP_RING_WIDTH` = 14, `SH4DER_WARP_RING_AMP` = 5 pixels (`include/sh4der_warp.h:76-78`). Heat haze: `SH4DER_WARP_HAZE_AMP` = 1.5, `SH4DER_WARP_HAZE_PERIOD` = 24, `SH4DER_WARP_HAZE_RISE` = 40 (`include/sh4der_warp.h:79-81`). Wobble: `SH4DER_WARP_WOBBLE_AMP` = 1.2, `SH4DER_WARP_WOBBLE_PERIOD` = 96, `SH4DER_WARP_WOBBLE_TURN` = 4.0 s (`include/sh4der_warp.h:83-85`, ADVISED).
4. Speed blur: at most `SH4DER_WARP_BLUR_COPIES` = 4 copies, the outermost zoomed by `SH4DER_WARP_BLUR_ZOOM` = 0.06 at full strength (`include/sh4der_warp.h:86-87`, `sh4der_warp_blur_scale` at `:228`, ADVISED).
5. Pixelate: `SH4DER_WARP_PIXEL_CELL` = 16 frame pixels, which divides 320 and 240 (`include/sh4der_warp.h:88`, ADVISED).

On a console: MEASURED for the pass sends (the page: "Measured on a console"; [BUS-004](../hardware/ledger-bus-dma.md)). The values above are header defaults, not tuned on a console.

Header: [include/sh4der_warp.h](../../include/sh4der_warp.h). Call order: [Call order](#call-order).

## What it does

The frame is a 512 × 256 texture with the 320 × 240 picture in its top-left corner. It is drawn through a grid mesh whose texture coordinates are pushed (README:480-482; warp.h:8-45). The pushes come from:

- a shockwave ring;
- a heat haze column;
- an underwater wobble.

It also does radial speed blur (as scaled copies), pixelate, and a 4 × 4 ordered dither.

`src/sh4der_warp.c` is KallistiOS drawing, Dreamcast only. The header is host-tested maths.

Version: v0.10, passes its maths tests (warp.h:39). It has now run on a console in the h2 bench, below.

## Call order

The order is in warp.h:~31-34. `AGENTS.md` has no section of its own.

## Cost

**Measured on a console.** h2 bench, one console, one run, 2026-11-25. Least and mean of 8 frames, the draw call alone (demo/psm/bench/NOTES.md h2 "The warp"; photo PXL_20261125_072120744.jpg).

| Pass | Vertices | By `pvr_prim` | By store-queue runs | PVR render |
|---|---|---|---|---|
| Mesh 40 × 30 | 2,460 | 2,250 / 2,275 us | 2,394 / 2,442 us | 6,466 / 6,503 us |
| Pixel pass | 1,200 | 440 / 440 us | 290 / 293 us | 3,184 / 3,183 us |
| Mesh maths alone (31 rows of `sh4der_warp_row`) | | 632 us | | |

What this shows:

- The pixel pass gains from store-queue runs: 241 ns a vertex against 366.
- The mesh does not. It is 6 % slower by runs, and this is not explained. Until it is, "a strip in one run is quicker" holds for the pixel pass only.
- The maths is 632 us of the mesh's 2,250 us. Filling and sending the vertices is the larger part.

As-stated, not timed:

- The strip block is one grid row, 2.6 KB, 32-byte aligned, sent in one store-queue run (warp.c:50-53).
- The sideways waves cost one sine per source per row; only the ring is per vertex (warp.h:~28).
- VRAM: the frame texture, 512 × 256 RGB565 (`include/sh4der_warp.h:13-14`, `SH4DER_WARP_TEX_W` 512 at line 69): 256 KB by arithmetic (ESTIMATE, width × height × 2 bytes). The README effect table gives 160 KB and 640 KB; that conflicts with the header, and this page follows the header.

**How the source follows these numbers.** `src/sh4der_warp.c:50-71` picks the send per pass from h2 test 7:

- `SH4DER_WARP_MESH_SEND_PRIM` defaults to 1: the mesh goes by `pvr_prim` (914 ns a vertex, against 973 by runs).
- `SH4DER_WARP_PIXEL_SEND_PRIM` defaults to 0: the pixelate pass goes in store-queue runs (241 ns a vertex, against 366 by `pvr_prim`).
- `SH4DER_WARP_SEND_PRIM`, when defined, sets both. The bench builds the file once each way with it.

All MEASURED, one console, one run, 2026-11-25.

## Recommended limits

- `SOURCES` 4 (README:492).
- `BLUR_COPIES` at most 4 (README:507).
- Pixel cell 16, which divides 320 and 240 (README:509).
- The copy zoom is never below 1 (warp.h:227).

## Tuning

See `README.md` "Warp tuning" (README:484-510).

## Pitfalls

- The sample point is clamped to the frame, so no black is pulled in at the edges (warp.h:~21).
- The dither is a screen door, not a palette. The PVR has no per-pixel quantiser (warp.h:~28).
- The send is chosen per pass (see "How the source follows these numbers" above). A client that changes the mesh's size or block layout should time both ways on a console before changing a default.

## Open

- Why the mesh is slower by store-queue runs than by `pvr_prim` on the console (h2).

## Related

- [Colour fringe](ca.md) (the frame texture)
- [PVR ledger](../hardware/ledger-pvr.md), [SH-4 ledger](../hardware/ledger-sh4.md)
- [Benchmarking](../methods/benchmarking.md)
