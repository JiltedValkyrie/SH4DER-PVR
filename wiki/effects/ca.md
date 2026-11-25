---
title: ca (frame texture and chromatic aberration)
tags: [effect, ca, post]
updated: 2026-11-25
sources: [include/sh4der_ca.h, src/sh4der_ca.c, README.md "The frame texture and chromatic aberration"]
---
# ca: the frame texture and chromatic aberration

**What you will learn:** how the world is drawn into a texture, how the colour fringe is made from it, and what it costs in video memory.
**For:** a programmer who wants a lens colour fringe, or any effect that needs the whole frame as a texture.

## What it does

The world is drawn into a 640 × 480 or 320 × 240 [render target](../glossary.md). The screen pass then draws it as one quad, or as three channel meshes (red, green, blue) at slightly different scales (README:706-747).

`src/sh4der_ca.c` does the setup, begin, channel and quad, using KallistiOS's `pvr_scene_begin_rtt` with `PVR_TXRFMT_X32_STRIDE` and direct render. The size and KB maths are inline.

Version: v0.50, runs on screen on the demo disc (ca.h:34).

## Call order

See the code block at README:~722-738 and ca.h:23-27. `AGENTS.md` has no section of its own.

## Cost

Video memory is exact arithmetic (README:~712; ca.h:11-12):

| Frame texture | One | Two (`SH4DER_CA_TEXTURES` 2) |
|---|---|---|
| 640 × 480 (640 × 512 rows × 2 B) | 640 KB | 1,280 KB |
| 320 × 240 | 160 KB | 320 KB |

As-stated: three meshes of 63 vertices each, a multiply and an add per vertex (ca.h:27-29). Two scenes a frame (README:~744). There are no times.

## Recommended limits

- `SH4DER_CA_TEXTURES` 2. The mesh is 8 × 6 cells.
- The frame time must hold both scenes (README:~744).

## Tuning

There is no README table. The macros are at ca.h:75-78. The meaning of strength is at README:741.

## Pitfalls

- A setup that cannot get video memory returns 0 and gives `short_kb`. The effect is then off, and you draw normally. Print `short_kb` once (README:~742-745).
- There is one stride register for all stride textures. The setup sets it to the frame width. [trail](trail.md) shares it: never run both at once.

## Open

- The cost of the second scene on a console.

## Related

- [warp](warp.md), [taa](taa.md), [trail](trail.md)
- [Video ledger](../hardware/ledger-video.md)
