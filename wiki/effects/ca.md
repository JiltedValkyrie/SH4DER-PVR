---
title: Colour fringe [ca] (frame texture and chromatic aberration)
tags: [effect, ca, post]
updated: 2026-11-26
sources: [include/sh4der_ca.h, src/sh4der_ca.c, README.md "The frame texture and chromatic aberration"]
---
# Colour fringe [ca]: the frame texture and chromatic aberration

**What you will learn:** how the world is drawn into a texture, how the colour fringe is made from it, and what it costs in video memory.
**For:** a programmer who wants a lens colour fringe, or any effect that needs the whole frame as a texture.

> **In plain words.** The colour fringe pulls the colours a little apart toward the screen's edges, the way a cheap camera lens does: the corners show red and blue edges, and the middle stays clean. To do it, the world goes into a [render-to-texture](../glossary.md#drawing) picture, and that picture is drawn three times, once per colour. Use it for a scope or camera look, a hit or glitch, or a dream or old-tape look. It needs spare video memory, and when there is not enough it turns itself off and draws the picture as normal.

## Try this first

Split the colours toward the corners of the screen, so the edges show a lens fringe.

1. At load, call `sh4der_ca_setup(&ca, half, 1)` (`include/sh4der_ca.h:176`): `half` 0 for 640 x 480 (640 KB), 1 for 320 x 240 (160 KB) (`include/sh4der_ca.h:11-12`). Use 640 x 480 for the fringe alone, and 320 x 240 when a temporal blend rides on it or video memory is short (`include/sh4der_ca.h:48-49`, ADVISED). The setup sets the PVR's one stride register, which the motion blur shares (`include/sh4der_ca.h:173-174`).
2. If setup returns 0, print `short_kb` once and draw as before (`include/sh4der_ca.h:19-21`, `:50-51`, ADVISED).
3. Strength 1.00 puts red and blue `SH4DER_CA_SPLIT_PX` = 3.0 screen pixels apart in the corner (`include/sh4der_ca.h:75`, `:46`, ADVISED). The useful range is 0.25 to 4.00; 12 pixels is a damaged lens (`include/sh4der_ca.h:46-47`, ADVISED).
4. Draw the world with `sh4der_ca_begin(&ca, 0)` in place of `pvr_scene_begin` (`include/sh4der_ca.h:181-183`). Then draw green with `sh4der_ca_channel(..., SH4DER_CA_GREEN, ...)` in the opaque list, and red and blue in the translucent list (`include/sh4der_ca.h:190-192`, ADVISED). Draw the HUD after, sharp (`include/sh4der_ca.h:52`, ADVISED).

On a console: NOT RUN (the page: "There are no times.").

Header: [include/sh4der_ca.h](../../include/sh4der_ca.h). Call order: [Call order](#call-order).

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
- There is one stride register for all stride textures. The setup sets it to the frame width. [Motion blur](trail.md) shares it: never run both at once.

## Open

- The cost of the second scene on a console.

## Related

- [Tape wobble](timebase.md): the frame texture drawn as bands, each row moved sideways (`sh4der_ca_channel_rows`)
- [Screen warp](warp.md), [Smooth edges over time](taa.md), [Motion blur](trail.md)
- [Video ledger](../hardware/ledger-video.md)
