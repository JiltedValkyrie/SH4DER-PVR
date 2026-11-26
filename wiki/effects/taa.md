---
title: Smooth edges over time [taa] (temporal AA maths)
tags: [effect, taa, post]
updated: 2026-11-26
sources: [include/sh4der_taa.h, README.md "The temporal AA maths", README.md "Recommended settings"]
---
# Smooth edges over time [taa]: temporal AA maths

**What you will learn:** what the TAA header computes, where the history comes from, and how far to push its weight.
**For:** a programmer smoothing a still picture without ghosting a moving one.

> **In plain words.** Edges look jagged because each pixel is either inside a shape or outside it. This effect nudges the camera by a fraction of a pixel each frame and blends each new frame with the ones before, so the edges average out into a smoother line. A moving camera would leave a ghost, a faint copy of the old picture, so the blend weakens as the camera turns faster. Use it when the camera mostly rests: a cutscene, a menu over a 3D backdrop or a photo mode.

> **Ghosting.** TAA's blend leaves a faint trail of the last frame behind moving things. The horror and night recipe covers the look and its cost (about 6 ms of PVR a frame, an estimate): [Ghosting with TAA](../guides/horror-night-game.md#ghosting-with-taa). The Match Light short film demo uses it: [demo/matchlight](../../demo/matchlight/).

## Try this first

Smooth the jagged edges of a still picture by blending in the frame before, and back off when the camera moves.

1. At load, call `sh4der_taa_init(&t)` (`include/sh4der_taa.h:197`). Each frame, call `sh4der_taa_step(&t, yaw, pitch, fx, fy, &out)` (`include/sh4der_taa.h:214`). Yaw turning right and pitch looking up are positive (`include/sh4der_taa.h:36-37`, ADVISED).
2. Weights: still camera 0.675 = 0.90 x `SH4DER_TAA_WEIGHT_HI` (0.75 by default, `include/sh4der_taa.h:84`); moving camera 0.30. Mix the two by the turn and the walk, and never go above 0.9 (`include/sh4der_taa.h:48-53`, `:57`, ADVISED).
3. The walk speed that counts as moving, `walk_full`, is 1.5 world units a second, the header's demo value (`include/sh4der_taa.h:54-55`, ADVISED). A turn over `SH4DER_TAA_CUT_RAD` = 0.35 radians in one frame is a cut: it blends nothing that frame (`include/sh4der_taa.h:88`, `:61`, ADVISED).
4. Keep the blended result as the next frame's history, and use the 320 x 240 frame texture: two history textures are 320 KB there, against 1,280 KB at 640 x 480 (`include/sh4der_taa.h:58-63`, ADVISED).

Run on a console by the owner in Demo disc and Match Light (her test; no measured numbers recorded yet; released in the public Match Light film). The page: "As-stated. None of it is measured on a console."

Header: [include/sh4der_taa.h](../../include/sh4der_taa.h). Call order: [Call order](#call-order).

## What it does

Header-only maths (README:433-435; taa.h:8-60):

- sub-pixel jitter over 8 phases (Halton 2, 3);
- a history weight that falls as the camera's speed in pixels rises;
- a pan shift;
- a cut on a big turn.

The history layers themselves are drawn by [Motion blur](trail.md), through the front-buffer copy.

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

- [Motion blur](trail.md), [Colour fringe](ca.md) (the frame texture)
