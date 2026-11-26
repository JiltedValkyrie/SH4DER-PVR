---
title: Fake focus blur [focus] (fake depth of field)
tags: [effect, focus, post]
updated: 2026-11-26
sources: [include/sh4der_focus.h, src/sh4der_focus.c, README.md "The focus effect"]
---
# Fake focus blur [focus]: fake depth of field

**What you will learn:** how a sharp region is kept while the rest of the frame softens, without a depth read.
**For:** a programmer who wants a tilt-shift look, or a sharp subject on a soft background.

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
