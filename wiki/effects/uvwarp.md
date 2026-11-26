---
title: Texture ripple [uvwarp] (per-vertex texture offset)
tags: [effect, uvwarp]
updated: 2026-11-26
sources: [include/sh4der_uvwarp.h, README.md "The uvwarp effect"]
---
# Texture ripple [uvwarp]: per-vertex texture offset

**What you will learn:** how a small table pushes each vertex's texture coordinate, and how to keep it precise over time.
**For:** a programmer who wants a crawling arc, a rippling banner or a shimmering strip: a distortion the PVR cannot read per pixel.

## What it does

Each vertex's texture coordinate (u, v) is pushed by a small two-channel byte table. The table is read bilinear, wraps on both axes, and scrolls with time. There is a prepared form for a mesh loop: `sh4der_uvwarp_prepare` once, then `sh4der_uvwarp_offset_p` per vertex (README:1138-1146).

Header only, byte reads only.

Version: v0.10 (uvwarp.h:49).

## Call order

See README:1141-1143. `AGENTS.md` has no section of its own. The build tool downsamples the source map; you own the table and the weights.

## Cost

| Item | Cost | Status |
|---|---|---|
| RAM | 2 KB for a 64 × 16 table | as-stated (README:1145) |
| CPU per vertex | one bilinear read and a few FMACs | as-stated (README:1145) |
| Accuracy | 1.1e-3 of the range at worst; 1.02e-3 on a 256-wide table | host test, not a console (uvwarp.h:46) |
| VRAM, sound RAM | 0 | |

## Recommended limits

- Table width and height from 1 to 256 (`SH4DER_UVWARP_SIDE_MAX`, uvwarp.h:73). A 256 × 256 table (128 KB) is treated as a build error.
- Coordinates must be finite and under 2^23 (uvwarp.h:74; README:1146).

## Pitfalls

- Wrap the time to the scroll period. The plain form loses precision as time grows (README:1146).
- Fade the weight to 0 at the mesh's ends, so the ends stay put (README:~1144).
- It needs vertices where the warp changes. The PVR only interpolates between them.

## Related

- [Screen warp](warp.md), the per-pixel route through the frame texture
