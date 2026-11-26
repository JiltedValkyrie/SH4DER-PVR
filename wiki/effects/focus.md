---
title: Fake focus blur [focus] (fake depth of field)
tags: [effect, focus, post]
updated: 2026-11-26
sources: [include/sh4der_focus.h, src/sh4der_focus.c, README.md "The focus effect"]
---
# Fake focus blur [focus]: fake depth of field

**What you will learn:** how a sharp region is kept while the rest of the frame softens, without a depth read.
**For:** a programmer who wants a tilt-shift look, or a sharp subject on a soft background.

> **In plain words.** The picture is drawn twice: a soft, blurred copy, with a sharp copy over it inside a shape you choose (a circle, a band across the middle, or a box). It looks like a camera lens focused on one thing while the rest blurs. Use it for a close-up face, a scope's soft rim, or a tilt-shift look (a band of sharp picture with the top and bottom blurred, like a photo of a toy town). The blur is faked from a small copy of the screen, since the console cannot tell how far away each thing is.

## Try this first

It keeps one shape of the picture sharp and lets the rest go soft.

1. Pick the shape and its edge: `sh4der_focus_mask_default(&m, SH4DER_FOCUS_DISC)` (or `SH4DER_FOCUS_BAND`, `SH4DER_FOCUS_RECT`), with `SH4DER_FOCUS_FEATHER` = 48.0 pixels for the soft edge (include/sh4der_focus.h:67, 98-106; ADVISED, README "Focus tuning").
2. Shape sizes: `SH4DER_FOCUS_DISC_R` = 120.0 pixels, `SH4DER_FOCUS_BAND_H` = 60.0 (half height), `SH4DER_FOCUS_RECT_W` = 100.0 and `SH4DER_FOCUS_RECT_H` = 80.0 (half sizes) (include/sh4der_focus.h:69-72; ADVISED).
3. At load: `sh4der_focus_init(&g)`, which returns -1 when there is no video memory (then draw nothing). The blur grid is `SH4DER_FOCUS_SRC_W` = 80 by `SH4DER_FOCUS_SRC_H` = 60, blurred into a `SH4DER_FOCUS_TEX_W` = 128 by `SH4DER_FOCUS_TEX_H` = 64 texture (include/sh4der_focus.h:59-62, 217; ADVISED).
4. The mesh is `SH4DER_FOCUS_GRID_X` = 16 by `SH4DER_FOCUS_GRID_Y` = 12 cells, 408 vertices (include/sh4der_focus.h:63-64, 80; ADVISED).
5. Each frame: `sh4der_focus_box565(src, dst)`, then `sh4der_focus_upload(&g, dst)`, then `sh4der_focus_submit(&g, &m, SH4DER_FOCUS_Z)` with `SH4DER_FOCUS_Z` = 1.0. Draw it after the frame and before the HUD (include/sh4der_focus.h:30-31, 73, 185, 219, 221; ADVISED).

On a console: NOT RUN. The page says: "As-stated. None of it is measured on a console."

Header: [include/sh4der_focus.h](../../include/sh4der_focus.h). Call order: [Call order](#call-order).

## What it does

The [Exposure and grade](look.md) read's 80 × 60 grid is box-blurred on the SH-4 into a 128 × 64 RGB565 texture. A mesh whose vertex alpha is 1 − sharpness draws that texture over the sharp frame. It keeps a disc, a band or a rectangle sharp (README:574-576; focus.h:8-45).

There is no real depth read-back on this hardware path, so it is a fake (focus.h:~11).

`src/sh4der_focus.c` allocates the texture, compiles a header (source alpha / inverse source alpha), uploads and submits.

Version: v0.10, passes its maths tests (focus.h:33).

## Call order

The order is in focus.h:~28-30: `box565`, upload, submit. `AGENTS.md` has no section of its own. Draw the mesh after the frame and before the HUD (focus.h:~42).

## Cost

As-stated. None of it is measured on a console.

| Item | Cost | Source |
|---|---|---|
| Blur texture | 16,384 B of VRAM | focus.h:78 |
| Mesh | 408 strip vertices a frame at the default 16 × 12 grid | focus.h:80 |
| Blur RAM | 1,440 B, a ring of three row sums | focus.h:184 |
| The "/9" of the box blur | a multiply and shift, because the SH-4 divides in software | focus.h:155 |

## Recommended limits

- Texture sides are powers of two, and the grid sits in the top-left corner (README:584-585).

## Tuning

See `README.md` "Focus tuning" (README:578-596).

## Pitfalls

- An init with no VRAM returns −1 (focus.h:216). Draw nothing then.
- The blurred map comes from the frame read, and the frame on screen is one or two frames old (README:1106). So the soft part lags the sharp frame a little.

## Related

- [Exposure and grade](look.md), [Frame read](frame.md), [Screen filters](visor.md) (its thermal mode uses the same 16 KB map)
