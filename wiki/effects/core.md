---
title: core (the shared parts)
tags: [effect, core]
updated: 2026-11-25
sources: [include/sh4der_core.h, src/sh4der_core.c, README.md "The core shared parts", README.md "Shared tuning"]
---
# core: the shared parts

**What you will learn:** what every other effect borrows from `sh4der_core.h`, and what it costs.
**For:** a programmer using any effect, or building a new effect from the shared pieces.

## What it does

`sh4der_core.h` and `src/sh4der_core.c` hold the parts every effect uses (core.h:8-40; README:121-127, 1130-1136):

- the soft column **feather** edge, taken from [SH4DOW-DC](sh4dow-dc.md);
- the **tube** and **blocker** shapes the [blade](blade.md) uses;
- the **breath tracker**, a number from 0 (out) to 1 (in) that follows a body's chest, for [holo](holo.md);
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

- `sh4dow-dc/include` must be on the include path (README:1136).
- A non-finite input reads as "no effect this frame" (README:1136). Nothing crashes, but the effect goes dark.

## Related

- [Effects index](README.md)
- [The SH4ZAM rule](../methods/sh4zam-rule.md)
- [sh4dow-dc](sh4dow-dc.md)
