---
title: bump (normal map, highlight power 2)
tags: [effect, bump]
updated: 2026-11-25
sources: [include/sh4der_bump.h, README.md "The normal map effect", AGENTS.md "Call order: normal map (highlight power 2)", demo/torchwalk_ab.c]
---
# bump: a normal map lit by a moving light

**What you will learn:** how a normal map is lit with plain blend passes instead of the PVR's bump unit, and what it costs in draws and video memory.
**For:** a programmer who wants relief on a large flat surface, such as a floor or a wall, lit by a torch or another moving light.

## What it does

A normal map lit by a moving light, using ordinary add and multiply passes of grey textures. The PVR's own [bump unit](../glossary.md) is not used. The normals are kept decoded in RAM.

The Dreamcast-only parts are under `_arch_dreamcast` (bump.h:237+): the send calls (`sh4der_bump_send_header`, `_send_fan`, `_submit_fan`), the grey palette and the `pvr_txr_load` use. The host parts are `sh4der_bump_from_height`, `_texel`, `_normal_decode`, `_at` and `sh4der_bump_vertex_light`.

Version: v0.14 (bump.h:43). The PVR bump unit passes of v0.13 are gone; the highlight power 2 passes are the method.

## Call order

The full steps are in `AGENTS.md` "Call order: normal map (highlight power 2)" and README:1192. `demo/torchwalk_ab.c` shows the passes. In outline:

1. **Once:** make the normal map, decode it, and make the nine grey maps (the squares and the biased products). Set the grey palette. Compile two translucent headers: the grey add and the picture.
2. **Per surface:** a `sh4der_bump_frame_t`.
3. **Per cell** (the floor is cut into cells, one light each): the direction to the light, the half vector, and the weights of the passes that would change a byte.
4. **Draw:** the flat level, then each grey pass, then the picture.

## Cost

As-stated counts (README:1202). There are no times; none of it is measured on a console.

| Item | Cost |
|---|---|
| Draws | up to 8 per lit cell (the level, at most 6 grey passes, the picture), so a lit pixel is filled up to 8 times |
| VRAM | nine grey maps per surface: 4 KB each at 64 × 64, 16 KB each at 128 × 128 |
| RAM | normals at 3 B a texel: 12 KB at 64 × 64 |
| Allocation | none by the header |

`demo/torchwalk_ab.c` shows a cost label on the console (draws, lit cells, TA, video memory, ms). It has no tests, by design (`AGENTS.md`).

## Recommended limits

- A fan has at most 12 corners (bump.h:70; README:1210).
- The side is a power of two, up to 256 (README:1195). No mip levels.
- At most six grey passes.

## Tuning

See `README.md` "Normal map tuning" (README:1204-1210).

## Pitfalls

- The azimuth's direction against the u axis: the `flip` flag turns it round (bump.h:31-32).
- Each lit pixel is filled up to 8 times. Fill is the cost; keep the lit area small.

## Open

These are "Not checked on a real console" (README:1200; `AGENTS.md`):

- the azimuth's turn against the texture's v axis;
- the signs of the axes in the products;
- banding of the 8-bit grey maps through the 565 palette.

## Related

- [torch](torch.md)
- [Bump compare chain](../shadows/bump-compare-chain.md), a different use of the PVR's bump unit
- [PVR ledger](../hardware/ledger-pvr.md)
