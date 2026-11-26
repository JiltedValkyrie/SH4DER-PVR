---
title: Screen overlays [lens]
tags: [effect, lens, post]
updated: 2026-11-26
sources: [include/sh4der_lens.h, src/sh4der_lens.c, README.md "The lens effect"]
---
# Screen overlays [lens]

**What you will learn:** what the lens overlays are, and the one piece of video memory they use.
**For:** a programmer adding a vignette, film grain, hit-direction wedges or a full-screen flash.

## What it does

Overlays that need no frame read (README:529-531; lens.h:8-45):

- a vignette mesh (spokes × rings);
- film grain that keeps the picture's mean, from a 64 × 64 ARGB4444 tile;
- hit-direction wedges at the screen's edge;
- a full-screen flash.

`src/sh4der_lens.c` is the KallistiOS side, with six polygon headers. The maths is in `sh4der_lens.h` and is host tested (lens.c:16).

Version: v0.10, passes its maths tests (lens.h:34).

## Call order

The init and draw functions are at lens.h:~407-417. `AGENTS.md` has no section of its own.

## Cost

| Item | Cost | Status |
|---|---|---|
| Grain tile | 64 × 64 × 2 B = 8 KB, the only VRAM the lens uses | as-stated (lens.h:44; lens.c:16; README:546) |
| Vignette quality | worst Gouraud error 9.1/255 at 16 spokes, 4.0 at 24, 2.3 at 32 | ESTIMATE, a host sweep, not hardware (README:541; lens.h:68) |
| Vignette vertex count | spokes × rings; the count is not stated | |

## Recommended limits

- `SPOKES_MAX` 32, `RINGS_MAX` 16 (README:571-572; lens.h:101-102).
- `HIT_MAX` 4 wedges (README:549).
- Autosort z values 430, 432, 950 and 960, under the HUD at 1000 (README:567-570).

## Tuning

See `README.md` "Lens tuning" (README:533-572).

## Pitfalls

- With no VRAM at init, the lens draws what it can without the grain. It does not retry until the next PVR start (lens.h:~407; lens.c:29, :51).
- A PVR reset clears VRAM, so call the forget function at a PVR start (lens.c:103).
- Rebuild the rim table when the screen size changes (lens.h:~410).

## Related

- [Screen filters](visor.md), [Fake focus blur](focus.md)
- [Video ledger](../hardware/ledger-video.md)
