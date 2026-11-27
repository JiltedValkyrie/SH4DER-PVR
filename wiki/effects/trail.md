---
title: Motion blur [trail]
tags: [effect, trail, post]
updated: 2026-11-26
sources: [include/sh4der_trail.h, src/sh4der_trail.c, README.md "The trail effect"]
---
# Motion blur [trail]

**What you will learn:** how motion blur and the TAA blend draw the frame before over this one, and what that needs from the screen setup.
**For:** a programmer adding motion blur, or the history layers for [Smooth edges over time](taa.md).

> **In plain words.** When the camera turns fast, the picture smears the way a fast-moving thing smears to the eye. This effect lays copies of the frame before over this one, each moved a little further along the turn, so the picture streaks, and the same copies make the blend for [smooth edges over time](taa.md). Use it for a speed boost, a stunned view or ghost trails behind fast things. It needs the screen at 640 x 480, with no 2X anti-aliasing (edge smoothing) and dithering off.

> **Ghosting.** A stronger smear than TAA: copies of the last frame are drawn along the motion. It costs more (about 25 ms of PVR a frame with three copies, an estimate), so the horror and night recipe suggests two copies at most: [Ghosting with TAA](../guides/horror-night-game.md#ghosting-with-taa).

## Try this first

Smear the picture along a fast camera turn, so a boost or a stunned view reads as fast.

1. At load, call `sh4der_trail_setup()` (`include/sh4der_trail.h:167`). It sets 640 x 480, no AA 2X, dithering off and vertical scale 1.0 (`include/sh4der_trail.h:22-24`), and takes a 128-byte mask (`SH4DER_TRAIL_MASK_BYTES` = 128, `include/sh4der_trail.h:157`, ADVISED).
2. Each frame, call `sh4der_trail_step(&t, yaw, pitch, fx, fy, &out)` (`include/sh4der_trail.h:109`). A turn over `SH4DER_TRAIL_CUT_RAD` = 0.35 radians in one frame is a cut, with no history that frame (`include/sh4der_trail.h:73`, ADVISED).
3. The blur rises from none at `SH4DER_TRAIL_SPEED_LO` = 1.0 pixel a frame to `SH4DER_TRAIL_WEIGHT_HI` = 0.5 at `SH4DER_TRAIL_SPEED_HI` = 8.0 pixels a frame (`include/sh4der_trail.h:69-71`, ADVISED).
4. Copies: `SH4DER_TRAIL_COPIES` = 3, each offset by at most `SH4DER_TRAIL_SHIFT_MAX` = 8 pixels (`include/sh4der_trail.h:67`, `:72`, ADVISED). Draw them with `sh4der_trail_draw` (`include/sh4der_trail.h:182`) in the translucent list. The colour fringe's render target shares the stride register, so never run both in one frame (`include/sh4der_trail.h:24`, ADVISED).

On a console: no figures for the effect (the page: "As-stated. None of it is measured on a console."). One full-screen translucent layer is MEASURED at about 3.1 ms at 640 x 480 ([PVR-002](../hardware/ledger-pvr.md)).

Header: [include/sh4der_trail.h](../../include/sh4der_trail.h). Call order: [Call order](#call-order).

## What it does

The frame before is drawn over this one through the front-buffer copy. A column mask sits in destination alpha, and each copy is drawn in two passes. The history weight rises with camera speed (README:512-514; trail.h:8-40). It gives motion blur and the temporal AA blend.

`src/sh4der_trail.c` is KallistiOS drawing, Dreamcast only. The maths is inline in the header.

Version: v0.10, passes its maths tests (trail.h:39).

## Call order

The order is in trail.h:~29-37: the layers per frame and `sh4der_trail_setup`. `AGENTS.md` has no section of its own.

## Cost

As-stated. None of it is measured on a console.

| Item | Cost | Source |
|---|---|---|
| Mask | 128 B of VRAM | trail.h:157; README:101 |
| History | 0 VRAM (it is the front buffer) | trail.h:~12-15 |
| Layers per frame | keep (1), mask (1), and two per copy; each is a full-screen translucent quad | trail.h:~26-28 |

Each full-screen translucent layer costs the PVR about 3.1 ms at 640 × 480 (MEASURED, one console, 2026-11-25, h1 and Compare by blending [c1]; see the [PVR ledger](../hardware/ledger-pvr.md)). So two copies come to six layers, about 19 ms of PVR time (ESTIMATE from that rate).

## Recommended limits

- Preconditions: 640 × 480, no AA 2X, dithering off, vertical scale 1.0 (trail.h:~22-24).
- `COPIES` 2 or 3 (README:520).
- `SHIFT_MAX` 8 pixels (README:525).
- A copy is 1:1. It moves x by an even count only, and y by any count (trail.h:~19-20).

## Tuning

See `README.md` "Trail tuning" (README:516-527).

## Pitfalls

- The stride register is shared with the render to texture of [Colour fringe](ca.md). Never run both at once (trail.h:~25).
- Without the two passes and the mask, or with the wrong offset sign, every edge gets comb-like fringes (trail.c:~136-137).
- The setup turns dithering off.

## Open

- **The front-buffer constants.** trail.c:143-144 says the quad's 2 × 320 split, the 1/1024 texel nudge and v from 0 to 960/1024 "come from the library's own hardware measurements of the front buffer's layout". It says to treat them as measured constants, not as something the file proves. No data, date or photo for that measurement is in the repository.
- The cost of the trail on a console.

## Related

- [Smooth edges over time](taa.md), [Colour fringe](ca.md)
- [PVR ledger](../hardware/ledger-pvr.md), [Video ledger](../hardware/ledger-video.md)
