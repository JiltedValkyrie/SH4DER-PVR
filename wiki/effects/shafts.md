---
title: Dark shafts of light [shafts] (shadow shafts)
tags: [effect, shafts, post]
updated: 2026-11-26
sources: [include/sh4der_shafts.h, src/sh4der_shafts.c, README.md "The shadow shafts"]
---
# Dark shafts of light [shafts]: dark shapes cast as rays from a light

**What you will learn:** how dark shapes in the frame cast darker copies away from a light, and what that costs per frame.
**For:** a programmer who wants light shafts through a dark scene: the dark twin of the [Screen bloom](glow.md)'s god rays.

> **In plain words.** Dark things in the picture, such as a tree or a pillar against a low sun, cast darker copies of themselves away from the light, so they look as if they throw shade toward you. It starts from a small copy of the picture that the console reads back (a [frame read](frame.md)), so it reads the picture each frame. Use it for trees, bars or a figure against a bright window, and keep the strength low: a strong shade looks like a second, wrong shadow.

## Try this first

For dark shapes that cast shade toward the viewer from a light, such as trees against a low sun.

1. Strength 0.50, off by default, useful range 0.25 to 3.00, passed to `sh4der_shafts_scale` (include/sh4der_shafts.h:23, 45-46; ADVISED).
2. Which cells cast: `SH4DER_SHAFTS_DARK_FROM` = 0.30 and `SH4DER_SHAFTS_DARK_FULL` = 0.10 (include/sh4der_shafts.h:73-74; ADVISED, README:863-864).
3. How near the light, in screen heights: `SH4DER_SHAFTS_NEAR` = 0.25 and `SH4DER_SHAFTS_FAR` = 0.9 (include/sh4der_shafts.h:78-79; ADVISED, README:868-869).
4. Facing the light: `SH4DER_SHAFTS_FACE_FROM` = 0.5 and `SH4DER_SHAFTS_FACE_FULL` = 0.95 (include/sh4der_shafts.h:80-81; ADVISED, README:870-871).
5. Each frame: `sh4der_shafts_map`, then `sh4der_shafts_upload`, then `sh4der_shafts_draw`. Clear the map once when the light leaves the view, not every frame (include/sh4der_shafts.h:21, 49; ADVISED).

Run on a console by the owner in Demo disc (her test; no measured numbers recorded yet). The page says: "There are no times."

Header: [sh4der_shafts.h](../../include/sh4der_shafts.h). Steps in full: [Call order](#call-order).

## What it does

The frame's dark cells cast a darker copy of themselves, pushed away from a light. It works from the [Frame read](frame.md) read (README:749-777).

`src/sh4der_shafts.c` does the init, upload and draw. The map maths is inline.

Version: v0.50, runs on screen on the demo disc (shafts.h:34).

## Call order

See the code at README:~753-766. `AGENTS.md` has no section of its own.

## Cost

As-stated counts (shafts.h:26-28; README:~753). There are no times.

| Item | Cost |
|---|---|
| Map and texture | glow's size, 128 × 64 RGB565: 16 KB of VRAM + 16 KB of RAM |
| SH-4 per frame | 4,800 cells (luma, distance, smooth step) |
| PVR | one mesh of 8 × 6 cells |

## Recommended limits

- The map is glow-sized, 128 × 64. The frame read grid is 80 × 60.

## Tuning

There is no README table. The macros are at shafts.h:73-81, described at README:772-773.

## Pitfalls

- It needs the frame read. Where the read sees black (an emulator without full frame-buffer emulation), the whole frame counts as dark and casts. Gate it as you gate the eye and the glow (README:~776).
- It eases half a texel per upload, because the read is a frame or two old.
- Clear the map once when the light leaves the view.

## Related

- [Screen bloom](glow.md), [Frame read](frame.md)
