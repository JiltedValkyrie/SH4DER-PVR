---
title: Screen filters [visor]
tags: [effect, visor, post]
updated: 2026-11-26
sources: [include/sh4der_visor.h, src/sh4der_visor.c, README.md "The visor effect"]
---
# Screen filters [visor]

**What you will learn:** how the night, amber and thermal visor looks are built from tables, and what they cost.
**For:** a programmer adding helmet or goggle overlays.

## What it does

Visor looks as tables of numbers: night, amber and thermal. Each look is (README:598-600; visor.h:8-40):

- a tint through the [Exposure and grade](look.md) grade;
- grain;
- a scanline tile;
- a dark vignette disc;
- a fade in and out.

Thermal cannot be a straight line per channel. It uses a 64-entry luma ramp over the 80 × 60 grid, into a 128 × 64 texture.

`src/sh4der_visor.c` holds the headers and two video-memory textures, built on the [Fake focus blur](focus.md) init.

Version: v0.10, passes its maths tests (visor.h:37).

## Call order

The order is in visor.h:~31-33. `AGENTS.md` has no section of its own. A fade of 0 skips the draws (visor.h:~33).

## Cost

As-stated. None of it is measured on a console.

| Item | Cost | Source |
|---|---|---|
| Grain | 2,048 B | visor.h:79-81 |
| Scanline tile | 128 B | visor.h:79-81 |
| Thermal ramp | 128 B of RAM | visor.h:79-81 |
| Thermal map | the focus map, 16 KB of VRAM | focus.h:78 |

## Recommended limits

- Tiles are powers of two (README:608, 611).

## Tuning

See `README.md` "Visor tuning" (README:602-615).

## Pitfalls

- Bloom here is an amount only. The client passes it to its own bloom pass; the visor does not draw it (visor.h:16).
- The thermal map is the previous frame, 80 × 60 samples: "the coarse grid a thermal camera has" (visor.h:~24).
- If init fails, any texture already taken is freed again and it returns −1 (visor.c:54-55).

## Open

- The thermal ramp's fit error is printed by the host test. No number for it is in the text (visor.h:~25).

## Related

- [Exposure and grade](look.md), [Fake focus blur](focus.md), [Screen overlays](lens.md)
