---
title: Shared parts [core] (the feather, tube shape, breath tracker and glint)
tags: [effect, core]
updated: 2026-11-26
sources: [include/sh4der_core.h, src/sh4der_core.c, README.md "The core shared parts", README.md "Shared tuning"]
---
# Shared parts [core]: the feather, tube shape, breath tracker and glint

**What you will learn:** what every other effect borrows from `sh4der_core.h`, and what it costs.
**For:** a programmer using any effect, or building a new effect from the shared pieces.

> **In plain words.** This is the shared toolbox the other effects borrow from: a soft edge, the shapes of light tubes and blockers, a counter that turns a body's breathing into a number from 0 (breathing out) to 1 (breathing in), a glint (a small bright flare on a light that brightens when it is close), and colour ramps. Nothing here is drawn on its own. Use it when you write your own effect, so you do not write these pieces again.

## Try this first

It holds the shared maths the other effects call. Its settings are macros that the effect that calls them should set.

1. Breathing: `SH4DER_BREATH_WINDOW` = 90 frames (3.0 s) and `SH4DER_BREATH_SPAN` = 0.0013, the least chest travel that counts as a full breath (include/sh4der_core.h:90-91; ADVISED, README "Shared tuning"). Reset a `sh4der_breath_t` with `sh4der_breath_reset`, then once a frame call `sh4der_breath_push(&b, sh4der_breath_raw(chest, pelvis_origin, pelvis_forward))` (include/sh4der_core.h:147-154).
2. Glints: `SH4DER_GLINT_BRIGHTNESS` = 0.4 at full, full within `SH4DER_GLINT_NEAR` = 3.0 and none past `SH4DER_GLINT_FAR` = 6.5 camera depth. Call `sh4der_glint(depth, fade)` (include/sh4der_core.h:101-103, 162; ADVISED).
3. Colour ramps, once at load: `sh4der_ramp_colours(avg, white, edge, core)` with `SH4DER_RAMP_EDGE_SAT` = 1.6 and `SH4DER_RAMP_CORE_SAT` = 0.25 (include/sh4der_core.h:113-114, 158; ADVISED). `white` (0 to 1): no recommended value yet; the test is the look on screen with it at 0 and at 1.
4. Caps: `SH4DER_TUBE_MAX` = 2 tubes and `SH4DER_BLOCKER_MAX` = 2 blockers an effect frame holds (include/sh4der_core.h:85-86; ADVISED).

On a console: NOT RUN for these numbers. The page says: "None of it is measured on a console."

Header: [include/sh4der_core.h](../../include/sh4der_core.h). Call order: [Call order](#call-order).

## What it does

`sh4der_core.h` and `src/sh4der_core.c` hold the parts every effect uses (core.h:8-40; README:121-127, 1130-1136):

- the soft column **feather** edge, taken from [sh4dow](sh4dow.md);
- the **tube** and **blocker** shapes the [Glowing tubes](blade.md) uses;
- the **breath tracker**, a number from 0 (out) to 1 (in) that follows a body's chest, for [Inner light](holo.md);
- the **glint**, which sets how bright a flare quad is by its depth from the camera;
- the **colour ramp** (EDGE and CORE from a texture's average colour);
- the SH4ZAM helper steps, with a plain C mirror for the host. See [the SH4ZAM rule](../methods/sh4zam-rule.md).

It is maths only. Nothing here talks to the [PVR](../glossary.md) (core.c:32, :69).

Version: v1.00, used in a released game (core.h:49; README:119).

## Call order

It has no call order of its own; the other effects call it. The breath tracker is used as shown in `AGENTS.md` "Call order: holo".

## Cost

All as-stated by the source. None of it is measured on a console.

| Item | Cost | Source |
|---|---|---|
| Breath tracker state | 364 B of RAM per tracker | README:1242 |
| Everything else | stateless | README:1130-1136 |
| A helper step | one SH4ZAM instruction or call | README:1135 |
| VRAM, sound RAM | 0 | |

## Recommended limits

- The breath window is 90 frames (3.0 s) (README:1242).
- Blade tube and blocker counts are capped by `SH4DER_TUBE_MAX` and `SH4DER_BLOCKER_MAX` (blade.h:103-104).

## Tuning

The `SH4DER_BREATH_*`, `SH4DER_GLINT_*` and `SH4DER_RAMP_*` macros. Their defaults are in `README.md` "Shared tuning" (README:1238-1258).

## Pitfalls

- The library's `include` folder must be on the include path: `sh4dow_dc.h` is in it.
- A non-finite input reads as "no effect this frame" (README:1136). Nothing crashes, but the effect goes dark.

## Related

- [Effects index](README.md)
- [The SH4ZAM rule](../methods/sh4zam-rule.md)
- [Soft character shadows](sh4dow.md)
