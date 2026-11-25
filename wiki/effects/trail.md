---
title: trail
tags: [effect, trail, post]
updated: 2026-11-25
sources: [include/sh4der_trail.h, src/sh4der_trail.c, README.md "The trail effect"]
---
# trail

**What you will learn:** how motion blur and the TAA blend draw the frame before over this one, and what that needs from the screen setup.
**For:** a programmer adding motion blur, or the history layers for [taa](taa.md).

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

Each full-screen translucent layer costs the PVR about 3.1 ms at 640 × 480 (MEASURED, one console, 2026-11-25, h1 and c1; see the [PVR ledger](../hardware/ledger-pvr.md)). So two copies come to six layers, about 19 ms of PVR time (ESTIMATE from that rate).

## Recommended limits

- Preconditions: 640 × 480, no AA 2X, dithering off, vertical scale 1.0 (trail.h:~22-24).
- `COPIES` 2 or 3 (README:520).
- `SHIFT_MAX` 8 pixels (README:525).
- A copy is 1:1. It moves x by an even count only, and y by any count (trail.h:~19-20).

## Tuning

See `README.md` "Trail tuning" (README:516-527).

## Pitfalls

- The stride register is shared with the render to texture of [ca](ca.md). Never run both at once (trail.h:~25).
- Without the two passes and the mask, or with the wrong offset sign, every edge gets comb-like fringes (trail.c:~136-137).
- The setup turns dithering off.

## Open

- **The front-buffer constants.** trail.c:143-144 says the quad's 2 × 320 split, the 1/1024 texel nudge and v from 0 to 960/1024 "come from the library's own hardware measurements of the front buffer's layout". It says to treat them as measured constants, not as something the file proves. No data, date or photo for that measurement is in the repository.
- The cost of the trail on a console.

## Related

- [taa](taa.md), [ca](ca.md)
- [PVR ledger](../hardware/ledger-pvr.md), [Video ledger](../hardware/ledger-video.md)
