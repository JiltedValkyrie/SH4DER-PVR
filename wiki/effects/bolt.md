---
title: Lightning [bolt] (branching strands)
tags: [effect, bolt, particles]
updated: 2026-11-26
sources: [include/sh4der_bolt.h, src/sh4der_bolt.c, README.md "The bolt effect"]
---
# Lightning [bolt]: branching strands that re-roll

**What you will learn:** how the bolt draws lightning that changes a few times a second, and how much it sends to the PVR.
**For:** a programmer adding lightning, an electric arc or a spark gap.

> **In plain words.** Lightning or an electric arc: thin, jagged lines of light that branch off a root. A few times a second the shape is drawn again, while the first half of each line stays put, so it flickers the way real lightning does. Use it for an arc between two posts, a spark gap or a crack of light. It is a set of flat lines drawn with added light, so it takes a few kilobytes of memory.

## Try this first

For lightning, an arc or a spark gap: thin jagged lines that change a few times a second.

1. Strands: `SH4DER_BOLT_STRANDS` = 4 jagged lines, at most `SH4DER_BOLT_STRANDS_MAX` = 8 (include/sh4der_bolt.h:64, 60; ADVISED, README:1218).
2. Shape: `SH4DER_BOLT_SEGS` = 6 segments, and `SH4DER_BOLT_FORKS` = 1 fork of `SH4DER_BOLT_FORK_SEGS` = 3 segments (include/sh4der_bolt.h:65-67; ADVISED, README:1219-1221).
3. Re-roll: `SH4DER_BOLT_REROLL_S` = 0.1333 s between new pictures, keeping `SH4DER_BOLT_KEEP_SEGS` = 3 segments (include/sh4der_bolt.h:78, 68; ADVISED, README:1222, 1232).
4. Width: `SH4DER_BOLT_WIDTH_PX` = 2.0 pixels on the screen, at any distance (include/sh4der_bolt.h:24-25, 74; ADVISED, README:1228).
5. Each frame: `sh4der_bolt_update` once, then `sh4der_bolt_draw` inside the translucent list (include/sh4der_bolt.h:29-30).

On a console: no figures for time. The page says: "There are no times." The header and README give no console status.

Header: [sh4der_bolt.h](../../include/sh4der_bolt.h). Steps in full: [Call order](#call-order).

## What it does

Thin, jagged, branching strands from a root, with a few forks. A few times a second the pattern is drawn again, while the first half of each strand stays put (the "re-roll"). The strands are additive quads of a constant pixel width (README:1020-1044).

The state and quad maths are inline and host tested. `src/sh4der_bolt.c` is the KallistiOS drawing: untextured additive quads through `sh4der_spark_quad`.

Version: v0.10, passes its maths tests (bolt.h:38).

## Call order

See README:1020-1044. `AGENTS.md` has no section of its own. It uses the [Sparks and tracers](spark.md) view and screen records (README:835).

## Cost

As-stated counts for the default look (README:1029; bolt.c:13-15). There are no times.

| Item | Cost |
|---|---|
| State | about 3.3 KB (3,304 B at 8 strands; yours) (bolt.h:32) |
| Header | 32 B |
| VRAM, sound RAM | 0 |
| TA per frame | 36 core + 24 halo quads = 120 triangles = 240 vertices, 7.7 KB |

## Recommended limits

From bolt.h:60-63:

- strands at most 8;
- segments 3 to 12;
- forks at most 3;
- fork segments 1 to 6.

The defaults are 4 strands of 6 segments with 1 fork of 3, re-rolled every 0.1333 s (bolt.h:58-81).

## Tuning

There is no README table. The `SH4DER_BOLT_*` macros are at bolt.h:58-81.

## Pitfalls

- Use both [guards](guard.md).
- Depth test on, no depth write, no culling, blend one / one (README:~1027).

## Related

- [Sparks and tracers](spark.md), [Send checks](guard.md)
