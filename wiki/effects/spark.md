---
title: Sparks and tracers [spark] (pooled additive quads)
tags: [effect, spark, particles]
updated: 2026-11-26
sources: [include/sh4der_spark.h, src/sh4der_spark.c, README.md "The spark effect", AGENTS.md "Call order: spark"]
---
# Sparks and tracers [spark]: pooled additive quads

**What you will learn:** how the fixed pool of 24 sparks works, what it costs, and how it protects the quads that matter.
**For:** a programmer adding muzzle flashes, tracers, impact sparks or lamp halos.

## What it does

A fixed pool of 24 short-lived additive quads that face the camera. There are two kinds:

- a billboard (a flash);
- a streak (a tracer, a spark), with a pixel floor, a retreating tail, a fade along its length and gravity.

A quad marked `SH4DER_SPARK_KEEP` is never pushed out by a plain one. When every slot is KEEP, a new plain quad is dropped and counted (README:~812).

The header is the pool and the quad maths, inline and host tested. `src/sh4der_spark.c` is the KallistiOS drawing: one additive header, both guards.

Version: v0.10, passes its maths tests (spark.h:66).

## Call order

The full steps are in `AGENTS.md` "Call order: spark" and README:806-846:

1. At load: load your atlas (no alpha; black adds nothing), `sh4der_spark_init`, `sh4der_spark_reset`.
2. On an event: `sh4der_spark_billboard`, `_streak` or `_burst`.
3. Once a frame: `sh4der_spark_update`.
4. In the translucent list, after the world: fill a view and a screen record, then `sh4der_spark_draw`.

## Cost

As-stated counts (README:816; spark.h:55-57). There are no times.

| Item | Cost |
|---|---|
| Pool | 24 × 52 B + 8 B = 1,256 B (yours) |
| Header | 32 B |
| VRAM | your atlas; 3 KB for a 64 × 64 VQ RGB565 one |
| TA per frame | at most 24 quads = 96 vertices, 3,104 B |
| Code | `draw` is 2,776 B (SH-4, -O3) |

## Recommended limits

- `SH4DER_SPARK_MAX` 24 (spark.h:89).
- `LIFE_MAX` 60 s: the age is held in 16-bit milliseconds (spark.h:92).
- The atlas has no alpha.

## Tuning

See `README.md` "Spark tuning" (README:839-846).

## Pitfalls

- The screen record must set both depth fields, or be made with `_make` (README:835).
- Draw inside the translucent list, after the world.
- Use both [guards](guard.md). A client with its own vertex path calls `sh4der_spark_quad` and tests both triangles with `sh4der_tri_area_ok`.

## Related

- [Lightning](bolt.md) (uses the same view and screen records), [Drifting specks](motes.md), [Send checks](guard.md), [Light pools](pool.md) (a lamp's halo)
