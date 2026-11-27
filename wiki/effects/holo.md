---
title: Inner light [holo]
tags: [effect, holo]
updated: 2026-11-26
sources: [include/sh4der_holo.h, src/sh4der_holo.c, README.md "The holo effect", AGENTS.md "Call order: holo"]
---
# Inner light [holo]

**What you will learn:** how the holo effect makes an inner light breathe with a translucent body, and what it costs.
**For:** a programmer drawing a hologram, a ghost, or any see-through body with a glow inside.

> **In plain words.** A light sits inside a see-through body, like a hologram's chest, and it breathes with the body. The core is brightest at its centre and fades out toward the edges, so the body looks lit from within. A small bright flare, the glint, can sit at the chest when the camera is close. Use it for a hologram, a ghost or a glowing creature whose inside should pulse with its breathing.

## Try this first

**What it is for:** a glowing core inside a see-through body that breathes with the pose.

1. **Start from the defaults.** `sh4der_holo_params_default` (include/sh4der_holo.h:90) fills them. The values are the macros at include/sh4der_holo.h:59-69 and the table at README:314-328 (ADVISED).
2. **Set the body's level.** `SH4DER_HOLO_BODY` = 0.62 (include/sh4der_holo.h:63; README:322; ADVISED). Bake the texture 1 / 0.62, about 1.6 times, brighter, so the body away from the core shows the texture as painted (README:288).
3. **Set the breath.** `SH4DER_HOLO_BASE` = 0.60 is the core strength at breath out, and `SH4DER_HOLO_AMP` = 0.40 is added at breath in (include/sh4der_holo.h:61-62; README:320-321; ADVISED). The core never goes out.
4. **Set the core's size.** `SH4DER_HOLO_INNER` = 0.010 (full core) and `SH4DER_HOLO_OUTER` = 0.055 (no core), in the bind model's units (include/sh4der_holo.h:68-69; README:327-328; ADVISED). The centre, `SH4DER_HOLO_X`, `_Y` and `_Z` (include/sh4der_holo.h:65-67), is the default model's chest. No recommended value yet for another body: test by placing the core on its chest in the bind pose.
5. **Set the glint.** `SH4DER_GLINT_OFFSET` = 0.0075: draw the glint that far toward the viewer, depth tested, with no depth write (include/sh4der_core.h:104; README:1666; ADVISED).

On a console: no figures. The page says "None of it is measured on a console" (Cost).

Read next: [the header](../../include/sh4der_holo.h), [the call order](../../AGENTS.md#call-order-holo).

## What it does

A light inside a translucent body follows the body's own breathing. At load, each vertex gets one weight byte. Each frame fills a 64-entry colour table, and each vertex then costs one table load. Alpha is constant (README:258-272; holo.h:8-40).

Version: v1.00, used in a released game (holo.h:31).

## Call order

The full steps are in `AGENTS.md` "Call order: holo" and README:274-289:

1. At load, call `sh4der_holo_weights` on the bind positions.
2. Each frame, feed the breath tracker. Then call `sh4der_holo_prepare` with the breath and the fade.
3. Per vertex, call `sh4der_holo_argb(frame, weight)`. Draw alpha-blended, texture × colour.

## Cost

All as-stated (README:310). None of it is measured on a console.

| Item | Cost |
|---|---|
| Frame state | 272 B (256 B of it is the table) |
| Parameters | 44 B |
| Weights | 1 B per vertex, made once with one distance test per vertex |
| Per frame | 64 multiply-adds |
| Per vertex | one load |
| VRAM | 0 |

## Recommended limits

- Weights run from 0 to 63 (64 levels) (README:263, 297).
- The glint is at full strength within 3 units, and gone from 6.5 units (README:272, 1248-1249).

## Tuning

See `README.md` "Holo tuning" (README:291-308).

## Pitfalls

- Bake the texture brighter by 1 / body, so the colour table can bring it back down (README:268).
- Bad parameters clear the table to 0. A NaN breath counts as 0.5. A NaN raw value is not remembered (README:289).
- Draw the glint 0.0075 toward the viewer, depth tested, with no depth write (README:272).

## Related

- [Shared parts](core.md), which holds the breath tracker
- [Glossary](../glossary.md)
