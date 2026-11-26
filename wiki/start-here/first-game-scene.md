---
title: "Your first game scene: three effects together"
tags: [start, learning]
updated: 2026-11-26
sources: [include/sh4der_pool.h, include/sh4der_blade.h, include/sh4der_spark.h, include/sh4der_guard.h, README.md, AGENTS.md, wiki/effects/names.md, wiki/effects/pool.md, wiki/effects/blade.md, wiki/effects/spark.md, wiki/effects/guard.md, wiki/guides/racing-game.md, wiki/limits/recommended-limits.md, wiki/limits/frame-budget.md, wiki/hardware/ledger-pvr.md, wiki/hardware/ledger-bus-dma.md, wiki/start-here/setup.md]
---
# Your first game scene: three effects together

**What you will learn:** how to put three effects in one scene: a light pool, a glowing cable and sparks. You will see the calls, their order, what the scene should look like, and what to check when it does not work.
**For:** a developer who has built the host tests (see [Your first hour](first-hour.md)) and now wants more than one effect on screen.

**Status: NOT RUN on a console.** Nothing here has run as one scene on a Dreamcast. The values come from the headers and READMEs, each with its file and line. The code blocks show calls by name, not a full program. Your scene needs your own models, camera and game code.

## The scene

A dark road at night, with three things on it:

1. A street lamp throws a round pool of warm light on the road. This is [Light pools [pool]](../effects/pool.md).
2. A power cable runs over the road and glows with moving light. This is [Glowing tubes [blade]](../effects/blade.md).
3. Where the cable touches a post, sparks fly out and fall. This is [Sparks and tracers [spark]](../effects/spark.md).

All three draw in the translucent list: the part of the frame drawn after the solid world, blended with what is already there. The [glossary](../glossary.md) explains the list.

The headers are [sh4der_pool.h](../../include/sh4der_pool.h), [sh4der_blade.h](../../include/sh4der_blade.h) and [sh4der_spark.h](../../include/sh4der_spark.h). The spark draw is in `src/sh4der_spark.c`, and it builds for the Dreamcast. Every triangle you send goes through the send checks in [sh4der_guard.h](../../include/sh4der_guard.h) ([Send checks [guard]](../effects/guard.md)).

## Start values

| Setting | Start value | Source | Status |
|---|---|---|---|
| Lamp disc, triangles | 10 | `sh4der_pool.h:8` | ADVISED |
| Lamp centre lifted off the road | 2 to 4 cm | `sh4der_pool.h:9` | ADVISED |
| Most triangles in one fan | 16, so the array holds 18 vertices | `sh4der_pool.h:98`, `:66-67` | ADVISED |
| Blade period | 192 ticks, which is 6.4 s at 30 Hz | `sh4der_blade.h:62` | ADVISED |
| Blade pulses in a period | 2 | `sh4der_blade.h:63` | ADVISED |
| Blade phase per object | 57 × the object's index | `wiki/effects/blade.md:51` | ADVISED |
| Spark pool | 24 quads | `sh4der_spark.h:101` | ADVISED |
| Least streak width on screen | 0.8 px | `sh4der_spark.h:102` | ADVISED |
| Spark atlas | 64 × 64, RGB565 with VQ, no alpha | `sh4der_spark.h:12`; no alpha: `sh4der_spark.h:56-57` | ADVISED |
| Atlas in video memory | 3 KB | `sh4der_spark.h:12` | ESTIMATE (arithmetic on the size) |
| Lamp colour and alpha | radius 7.5 m; alpha 0.60 × brightness | the street demo, in the [racing guide](../guides/racing-game.md), section 3 | ADVISED (a demo value; the street demo ran on a console, but its lamp pool was not timed alone: `sh4der_pool.h:11`) |

No header gives a start value for a spark burst. The burst takes its values from your code (`sh4der_spark.h:236-241`). Pick them and tune them by eye.

## What is loaded once

Load your spark atlas into video memory before the call below. The header compiles its polygon header for that atlas.

```c
/* Load time, once. Names are the library's; KallistiOS names are marked. */
sh4der_spark_reset(&sparks);                                  /* sh4der_spark.h:152: empties the pool */
sh4der_spark_init(atlas, 64, 64,                              /* sh4der_spark.h:425 (KallistiOS draw) */
                  PVR_TXRFMT_RGB565 | PVR_TXRFMT_VQ_ENABLE);  /* sh4der_spark.h:422 */
sh4der_blade_params_default(&blade);                          /* sh4der_blade.h:135 */
if (!sh4der_blade_params_valid(&blade)) { /* stop: bad settings */ }   /* sh4der_blade.h:141 */
/* One untextured translucent header, compiled with KallistiOS: depth test on, depth write off,
   no culling, additive (source alpha / one). The lamp and the cable use it. AGENTS.md, "Call order: pool", step 1 asks for a second, blended header for shadows; this scene does not need it. */
```

## What runs each frame, in order

1. **Clock.** Advance `dt` (seconds) and `tick`, a 30 Hz counter (`sh4der_blade.h:22-23`).
2. **Cable state.** Call `sh4der_blade_prepare(&cable, &blade, tick + phase, 1.0f)`. A fade of 1 means no cloak (AGENTS.md, "Call order: blade", step 2). A return of 0 means the frame is dark (`sh4der_blade.h:146`).
3. **Sparks.** Call `sh4der_spark_update(&sparks, dt, NULL)`. NULL means no gravity (`sh4der_spark.h:272-275`).
4. **Events.** When the cable touches the post, your game code calls `sh4der_spark_burst(...)`, or a billboard for a flash (`sh4der_spark.h:206-241`).
5. **Opaque world.** Draw the road, the post and the cable's solid parts with your own code.
6. **Translucent list, after the world:**
   - **Lamp pool.** Call `sh4der_pool_disc(...)` with 10 triangles. Transform the 11 vertices with your own matrix. Send each triangle after `sh4der_tri_area_ok` passes (`sh4der_pool.h:64`; `sh4der_guard.h:69`).
   - **Glowing cable.** Call `sh4der_blade_argb(&cable, ...)`, then draw the cable's triangles with the additive header (`sh4der_blade.h:158`; [README](../../README.md), "The blade effect", call step 3).
   - **Sparks.** Call `sh4der_spark_draw(&sparks, &view, &screen)` (`sh4der_spark.h:427-429`). Fill `view` with the camera's right, up and forward axes and its eye, the near plane, the focal length in pixels, the least width, the inset and the atlas's cells (`sh4der_spark.h:136-143`). Make `screen` with `sh4der_spark_screen_make(cx, cy, focal_y_px)` (`sh4der_spark.h:405`).
7. **HUD and the rest,** last, with your own code.

```c
/* Each frame. Names in lowercase are your own variables. */
sh4der_blade_prepare(&cable, &blade, tick + cable_phase, 1.0f);   /* 1.0: no cloak */
sh4der_spark_update(&sparks, dt, NULL);                           /* NULL: no gravity */

/* opaque: road, post, solid cable parts (your code) */

/* translucent list, after the world */
n = sh4der_pool_disc(lamp_centre, lamp_u, lamp_v, 10u, lamp_rgb, lamp_alpha, pool);
/* transform pool[0 .. n-1]; send triangle k as 0, k+1, (k+1) % 10 + 1, each through sh4der_tri_area_ok */

sh4der_blade_argb(&cable, cable_rgb, cable_a, cable_xyz, cable_stride, cable_argb, out_stride, cable_count);
/* send the cable's triangles with the additive header */

sh4der_spark_draw(&sparks, &view, &screen);
/* HUD (your code) */
```

These blocks show calls, not a whole program. The fan's triangle order is from `sh4der_pool.h:62-63`.

## What you should see

Expected from the headers. Nothing here was seen on a console.

- A round, warm pool on the road under the lamp, brightest at its centre and fading to nothing at its rim (`sh4der_pool.h:47-49`).
- A cable with bright streaks that circle it and a dark knot that travels along it, over one period of 6.4 s (`sh4der_blade.h:35-38`, `:62`).
- Sparks from the contact point that fly out and fall, each dying at the end of its life (`sh4der_spark.h:79-80`).

## What it costs

| Part | Number | Status |
|---|---|---|
| Lamp disc vertex array | 16 B a vertex, 11 vertices: 176 B | ESTIMATE (arithmetic, `wiki/effects/pool.md:39`) |
| Lamp disc fill | its share of the screen's area | ESTIMATE (a linear share of the next row) |
| Full-screen translucent layer, 640 × 480 | +3.1 ms of PVR each | MEASURED (PVR-002, `wiki/hardware/ledger-pvr.md:36`) |
| Send per vertex | 142 to 260 ns | MEASURED (BUS-003 and BUS-001, `wiki/hardware/ledger-bus-dma.md:20`, `:22`) |
| Spark send, 24 quads (96 vertices) | about 14 to 25 us of SH-4 | ESTIMATE (arithmetic on the row above; `sh4der_spark.h:11-16` says under 25 us) |
| Cable draw | none in its header | NOT MEASURED (`wiki/effects/blade.md:29-36`) |
| Whole scene frame | none | NOT MEASURED |

## If it does not work

Quick checks. The full list is in [When it looks wrong](looks-wrong.md).

- **No lamp pool, or it sits in the wrong place.** Check the lift (2 to 4 cm, `sh4der_pool.h:9`) and that the pool is drawn after the road (`sh4der_pool.h:17`).
- **The cable is dark.** `sh4der_blade_prepare` returned 0, which means bad parameters (`wiki/effects/blade.md:50`).
- **Sparks are missing.** The pool may be full of KEEP quads, so a new plain quad is dropped and counted (`sh4der_spark.h:50-54`, `:131`).
- **Sparks sit far behind the road.** The screen record's depth fields do not match your world (`sh4der_spark.h:392-396`).
- **A bright line in the pool.** A fan seen edge-on has no screen area (`wiki/effects/pool.md:64`).

## Credit

A game that uses any of this library must credit it (AGENTS.md, "Read first"). See [Licence and credit](license-and-credit.md). The call orders for every effect are in [Call orders](call-orders.md).

