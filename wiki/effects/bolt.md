---
title: Lightning [bolt] (branching strands)
tags: [effect, bolt, particles]
updated: 2026-11-26
sources: [include/sh4der_bolt.h, src/sh4der_bolt.c, README.md "The bolt effect"]
---
# Lightning [bolt]: branching strands that re-roll

**What you will learn:** how the bolt draws lightning that changes a few times a second, and how much it sends to the PVR.
**For:** a programmer adding lightning, an electric arc or a spark gap.

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
