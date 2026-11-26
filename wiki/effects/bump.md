---
title: Normal map [bump] (highlight power 2)
tags: [effect, bump]
updated: 2026-11-26
sources: [include/sh4der_bump.h, README.md "The normal map effect", AGENTS.md "Call order: normal map (highlight power 2)", demo/torchwalk_ab.c]
---
# Normal map [bump]: lit surface bumps with no bump unit

**What you will learn:** how a normal map is lit with plain blend passes instead of the PVR's bump unit, and what it costs in draws and video memory.
**For:** a programmer who wants relief on a large flat surface, such as a floor or a wall, lit by a torch or another moving light.

> **In plain words.** A normal map is a picture that stores which way each tiny patch of a surface faces. This effect lights that picture with a moving torch, so a flat floor looks grooved, and the grooves catch the light as the torch moves. It uses ordinary blend passes, not the PVR's [bump unit](../glossary.md), so every lit spot costs several extra draws. Use it for a big flat surface under a light that moves.

## Try this first

**What it is for:** putting relief on a big flat surface that a moving light sweeps across.

1. **Make the normal map once.** `sh4der_bump_from_height` (include/sh4der_bump.h:155) writes it from a height map. The side is a power of two up to 256 (README:1612). No recommended value yet for the side. The cost is 4 KB a grey map at 64 × 64 and 16 KB at 128 × 128 (README:1618). Test both on the torch walk's cost label.
2. **Set the strength.** `SH4DER_BUMP_STRENGTH` = 0.65, the highlight at full light (include/sh4der_bump.h:67; README:1624; ADVISED). The torch walk scales the highlight by it.
3. **Keep fans small.** `SH4DER_BUMP_MAX_FAN` = 12 corners at most for `sh4der_bump_send_fan` (include/sh4der_bump.h:70; README:1627; ADVISED, a limit).
4. **Send in this order.** At most six grey passes a cell (README:1615; ADVISED, a limit). Send the header once with `sh4der_bump_send_header` (include/sh4der_bump.h:259), then each cell with `sh4der_bump_send_fan` (include/sh4der_bump.h:270).
5. **Check the turn.** `sh4der_bump_frame_t.flip` = 0 for a normal map whose y runs up the image (include/sh4der_bump.h:74). No recommended value yet for the azimuth's turn on a console. If the highlight moves the wrong way, set `flip` to 1 and test it on a console (include/sh4der_bump.h:31-32; open question Q42 in ../open-questions.md).

On a console: NOT RUN. The page says "none of it is measured on a console" (Cost), and the header lists what is "NOT checked on a real console" (include/sh4der_bump.h:31).

Read next: [the header](../../include/sh4der_bump.h), [the call order](../../AGENTS.md#call-order-normal-map-highlight-power-2).

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

- [Screen spotlight](torch.md)
- [Bump compare chain](../shadows/bump-compare-chain.md), a different use of the PVR's bump unit
- [PVR ledger](../hardware/ledger-pvr.md)
