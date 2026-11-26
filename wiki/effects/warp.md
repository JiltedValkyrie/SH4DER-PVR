---
title: Screen warp [warp]
tags: [effect, warp, post]
updated: 2026-11-26
sources: [include/sh4der_warp.h, src/sh4der_warp.c, README.md "The warp effect", demo/psm/bench/NOTES.md "Hardware result h2"]
---
# Screen warp [warp]

**What you will learn:** how the warp bends the frame through a mesh, and what it measured on a console.
**For:** a programmer adding shockwaves, heat haze, underwater wobble or speed blur.

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
- VRAM: the frame texture, 160 KB at 320 × 240 or 640 KB at 640 × 480 (README effect table).

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
