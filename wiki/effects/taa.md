---
title: taa (temporal AA maths)
tags: [effect, taa, post]
updated: 2026-11-25
sources: [include/sh4der_taa.h, README.md "The temporal AA maths", README.md "Recommended settings"]
---
# taa: temporal AA maths

**What you will learn:** what the TAA header computes, where the history comes from, and how far to push its weight.
**For:** a programmer smoothing a still picture without ghosting a moving one.

## What it does

Header-only maths (README:433-435; taa.h:8-60):

- sub-pixel jitter over 8 phases (Halton 2, 3);
- a history weight that falls as the camera's speed in pixels rises;
- a pan shift;
- a cut on a big turn.

The history layers themselves are drawn by [trail](trail.md), through the front-buffer copy.

Version: v0.50, runs on screen on the demo disc (taa.h:39).

## Call order

The order is in taa.h:~30-34 ("`sh4der_taa_step` once"). `AGENTS.md` has no section of its own.

## Cost

As-stated. None of it is measured on a console.

| Item | Cost | Source |
|---|---|---|
| History | the front buffer read as a texture, 0 VRAM | taa.h:~14-16 |
| The recommended blend | only with the 320 × 240 frame texture: 320 KB, against 1,280 KB at 640 × 480 | README:1224; README:710-715 |

## Recommended limits

- Never set the weight above 0.9. Past it a ghost lasts ten frames or more (taa.h:~58).
- A ghost lasts about 1 / (1 − weight) frames, so 3 frames at 0.675 (taa.h:~60).
- `CUT_RAD` is 0.35 (README:446).

## Tuning

See `README.md` "TAA tuning" (README:437-447) and "Recommended settings" (README:1224, 1228-1236).

## Pitfalls

- Yaw right and pitch up are positive. A client whose yaw runs the other way passes a negated delta (taa.h:~37-38).
- Blending only the one frame before is sharper in motion and flickers more at rest (taa.h:~62).

## Related

- [trail](trail.md), [ca](ca.md) (the frame texture)
