---
title: Screen filters [visor]
tags: [effect, visor, post]
updated: 2026-11-26
sources: [include/sh4der_visor.h, src/sh4der_visor.c, README.md "The visor effect"]
---
# Screen filters [visor]

**What you will learn:** how the night, amber and thermal visor looks are built from tables, and what they cost.
**For:** a programmer adding helmet or goggle overlays.

> **In plain words.** The visor makes the picture look as if it is seen through a helmet or goggles: green night vision, a warm amber view, or a thermal picture where hot things glow white and cold things go dark blue. It adds grain, faint scanlines and a dark edge, and it fades in and out. Use it for night goggles, a thermal scope, an old monitor or a robot's eye view. Each look is a table of numbers, so a new look is mostly new numbers rather than new code.

## Try this first

Give the picture a night-vision, amber or thermal look, with a fade in and out.

1. At load, call `sh4der_visor_preset(SH4DER_VISOR_NIGHT, &d)` (`include/sh4der_visor.h:143`). NIGHT's row is grain 0.25, bloom 0.35, vignette 0.45 and scanline 0.15 (`include/sh4der_visor.h:113`). AMBER is at `:115`, and THERMAL, which draws a map rather than a grade, is at `:114` (ADVISED, the header's shipped presets).
2. Fade: `sh4der_visor_fade(&f, on, dt)` (`include/sh4der_visor.h:224`) eases in over `SH4DER_VISOR_FADE_IN_S` = 0.25 s and out over `SH4DER_VISOR_FADE_OUT_S` = 0.40 s (`include/sh4der_visor.h:64-65`, ADVISED).
3. Grade: `sh4der_visor_grade(&d, amount, &g)` (`include/sh4der_visor.h:153`), then `sh4der_look_solve` (`include/sh4der_look.h:214`) for the layers. A fade of 0 skips the draws (`include/sh4der_visor.h:33-35`, ADVISED).
4. Grain: a 32-pixel tile (`SH4DER_VISOR_GRAIN_TEX` = 32), scrolled each frame by `SH4DER_VISOR_GRAIN_SPEED_U` = 0.37 and `SH4DER_VISOR_GRAIN_SPEED_V` = 0.61 tile widths (`include/sh4der_visor.h:67-69`, ADVISED). Draw it with `sh4der_visor_draw_grain` (`include/sh4der_visor.h:253`).
5. Thermal: the ramp's stops are cold (0.02, 0.02, 0.20), mid (0.95, 0.25, 0.05) and hot (1.00, 0.95, 0.80) (`include/sh4der_visor.h:93-97`, ADVISED), turned into a 64-entry table by `sh4der_visor_lut` (`include/sh4der_visor.h:164`).

On a console: no figures (the page: "As-stated. None of it is measured on a console.").

Header: [include/sh4der_visor.h](../../include/sh4der_visor.h). Call order: [Call order](#call-order).

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
