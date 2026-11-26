---
title: "RPG, town and adventure: a recipe for the town, the lamps and the walk"
tags: [guide, rpg, adventure, town, game-type, recipe, effect]
updated: 2026-11-26
sources: [wiki/guides/racing-game.md, wiki/effects/names.md, wiki/start-here/call-orders.md, wiki/start-here/what-do-you-want.md, wiki/start-here/genre-shadows.md, wiki/start-here/genre-light-and-glow.md, wiki/start-here/genre-water-and-weather.md, wiki/start-here/genre-camera-and-screen.md, wiki/start-here/demo-guide.md, wiki/shadows/psm-routes.md, wiki/effects/aobake.md, wiki/effects/aomesh.md, wiki/effects/psm.md, wiki/effects/pool.md, wiki/effects/motes.md, wiki/effects/mist.md, wiki/effects/deform.md, wiki/effects/look.md, wiki/effects/focus.md, wiki/effects/film.md, wiki/effects/timebase.md, wiki/effects/warp.md, wiki/effects/tilecut.md, wiki/effects/frame.md, wiki/limits/frame-budget.md, wiki/limits/budgets.md, wiki/limits/recommended-limits.md, wiki/status/maturity.md, wiki/hardware/ledger-pvr.md, wiki/hardware/ledger-sh4.md, wiki/hardware/ledger-bus-dma.md, AGENTS.md, include/sh4der_aobake.h, include/sh4der_aomesh.h, include/sh4der_pool.h, include/sh4der_motes.h, include/sh4der_mist.h, include/sh4der_deform.h, include/sh4der_look.h, include/sh4der_psm.h, include/sh4der_timebase.h, include/sh4der_focus.h, include/sh4der_film.h, include/sh4der_rise.h]
---
# RPG, town and adventure: a recipe for the town, the lamps and the walk

**What you will learn:** how to build one frame of a town, village or ruin walk: baked corners, a sun's shadow, lamp pools, snow, fog, water and screen looks. Also what each part costs, what to cut first, and what goes wrong.
**For:** a developer making an RPG, town or adventure game on the Dreamcast who has never used SH4DER-PVR.

Status tags: **MEASURED** means a [ledger](../hardware/ledger-pvr.md) row backs it, from one console. **ADVISED** means a header, README table or demo source says so, with file and line. **NOT RUN** means no console run exists. **ESTIMATE** means arithmetic or a model, named. A count from the code is an exact host count, not a console number. Most effects here have no console run: treat every setting as a start.

## What this kind of game needs

A town is mostly still, so its corners can be baked once at load ([aobake] for boxes, [aomesh] for triangle walls). It has one sun and some lamps. The sun's shadow must reach passers-by, and a lamp throws a pool on the road. A few people move. The advised default is a shadow tile for each of the few nearest casters, not one map for the whole town (the owner's call of 2026-11-25; [frame budget, "The advised default look"](../limits/frame-budget.md), line 14; the tile count per frame is not measured). The old town's whole-map route ran at 18 frames a second in one view (PVR-108, MEASURED, one console).

Air, ground and screen need effects too. The HUD and dialogue must never drop out.

The budget: 16.7 ms a frame at 60 Hz, 33.3 ms at 30 Hz. If the SH-4 (main processor) and the PVR (graphics chip) overlap, a frame costs about the larger of the two; if they run in turn, about their sum ([frame budget, section 1](../limits/frame-budget.md)).

## Effects that fit, and why

| Effect | What it gives this game | Where it is | Status |
|---|---|---|---|
| Baked corner shading [aobake] | Dark creases at walls and boxes, baked at load | [header](../../include/sh4der_aobake.h), [page](../effects/aobake.md) | ALPHA v0.5 |
| Mesh corner shading [aomesh] | The same, for triangle walls | [header](../../include/sh4der_aomesh.h), [page](../effects/aomesh.md) | PROTOTYPE v0.1 |
| Sun shadow map [psm], route A | Sun shadow on the ground; the PVR draws casters | [header](../../include/sh4der_psm.h), [routes](../shadows/psm-routes.md) | ALPHA v0.5 |
| Light pools [pool] | Lamp pools, blob shadows | [header](../../include/sh4der_pool.h), [page](../effects/pool.md) | ALPHA v0.2 |
| Drifting specks [motes] | Pollen, dust, fireflies | [header](../../include/sh4der_motes.h), [page](../effects/motes.md) | PROTOTYPE |
| Rising lights [rise] | Lanterns that settle as stars | [header](../../include/sh4der_rise.h) | PROTOTYPE; NOT RUN |
| Mist bank [mist] | Fog over a pond, with a shape | [header](../../include/sh4der_mist.h), [page](../effects/mist.md) | PROTOTYPE; NOT RUN |
| Water waves [water] | A pond or lake | [header](../../include/sh4der_water.h), [page](../effects/water.md) | PROTOTYPE |
| Dented ground [deform] | Snow keeps footprints; mud closes them | [header](../../include/sh4der_deform.h), [page](../effects/deform.md) | PROTOTYPE; NOT RUN |
| Texture ripple [uvwarp] | Banners, flags | [header](../../include/sh4der_uvwarp.h) | PROTOTYPE; NOT RUN |
| Exposure and grade [look] | Eyes adapt indoors and out | [header](../../include/sh4der_look.h), [page](../effects/look.md) | PROTOTYPE |
| Fake focus blur [focus] | A close-up keeps one part sharp | [header](../../include/sh4der_focus.h), [page](../effects/focus.md) | PROTOTYPE; NOT RUN |
| Old film [film], tape wobble [timebase] | A flashback in worn film or tape | [film](../../include/sh4der_film.h), [timebase](../../include/sh4der_timebase.h) | PROTOTYPE; NOT RUN |
| Screen warp [warp] | A spell's shockwave | [page](../effects/warp.md) | ALPHA v0.3 |
| Tile-cut pass [tilecut] | Trims a small shadow target (route A) | [header](../../include/sh4der_tilecut.h), [page](../effects/tilecut.md) | ALPHA v0.5 |

**Left out.** Corner darkening [ao] ([aobake] does the job). Planar reflection [pl4nar] (a second world draw, PVR-175). Screen bloom [glow] and shafts [shafts] (they read the picture back). Screen spotlight [torch], screen filters [visor] and inner light [holo] suit a lantern or magic scene; all NOT RUN.

**Source conflict.** look.h:36 says "Version: v0.50", and look.md says it runs on screen on the demo disc. maturity.md gives look PROTOTYPE v0.1 with no ledger row. The sources disagree; this page keeps the maturity label.

## The order of one frame

1. **At load.** Bake static scenery with [aobake] or [aomesh]. Make two untextured translucent headers for pools (depth test on, depth write off, no culling; AGENTS.md, pool call order). Fill the look's parameters (AGENTS.md, look call order). Why: a static sun bake of 152 ms does not fit in a frame (SH4-070, MEASURED, one console).
2. **Look read.** Read the picture on screen on the grid, and step the eye (AGENTS.md, "Call order: look", steps 2 and 3). Why: the read sees last frame's picture ([frame](../effects/frame.md)); the gain goes into the grade at the end.
3. **Shadow target, route A.** Draw the people's casters into the target; wait for the render before anything reads it, and after each tile cut ([tilecut](../effects/tilecut.md)). Why: the racing recipe's rule for the mirror pass; applying it here is our advice, not tested.
4. **Ground heights.** Stamp each foot, recentre the window, read each ground vertex's height (deform steps 1 to 3). Why: the ground needs its heights before it is drawn.
5. **Opaque world.** Draw ground, walls, props and people. Multiply each vertex's ambient light by its baked byte (aobake, step 4). Why: the byte is a multiply on ambient light only.
6. **Translucent list, after the ground.** Pools, the shadow layer that looks up the target, mist sheets (blend source alpha, depth test on, depth write off), camera-facing quads for lamp halos ([spark](../effects/spark.md)), rising lights (additive, `sh4der_rise.h` lines 15 to 16). Why: each adds light or fog over what is drawn.
7. **Punch-through list.** Motes after the world, with `sh4der_motes_draw_masked` (`sh4der_motes.h` lines 29 to 31). That list writes depth ([motes](../effects/motes.md), Pitfalls).
8. **Screen layers.** Focus after the frame and before the HUD ([focus](../effects/focus.md)). The look's grade layers full screen, depth test always, no depth write. Film over the finished picture. Timebase bands, with no HUD drawn through them.
9. **HUD.** A full vertex buffer drops the end of the list silently: in one run the HUD and inset vanished (PVR-185, MEASURED, one console; the buffer as the cause is an ESTIMATE). Send the text early or give it its own room ([frame budget, section 3](../limits/frame-budget.md)). OPEN: no source gives one order that is both early and over the grade layers. Our advice, not tested: a HUD list of its own.
10. **Frame stamp last**, if you use the frame read ([frame](../effects/frame.md)).

Silent in the sources: the water's call order, and route A's ground-layer order. water.md cites README lines 1148 to 1156 for the water calls, but those lines are the psm calls; README "The water effect" (line 1564) lists the functions only.

## What it costs

No total is measured. Read each row alone; never add rows as if measured. An empty frame costs 3.15 ms of PVR (PVR-001, MEASURED).

Against 16.7 and 33.3 ms: the old town's route A had SH-4 at 13.6 ms and PVR at 15.9 ms, each under 16.7 ms, but its frame ran at 18 frames a second, so it misses both.

| Part | Video memory | Triangles or vertices | Frame time | Status |
|---|---|---|---|---|
| Baked corner shading [aobake] | none | 2,142 and 2,544 vertices (exact, scenes A and B) | Load 254 and 393 ms; frame +216 and +277 us SH-4 (SH4-076, SH4-082) | MEASURED, one console, other scenes |
| Mesh corner shading [aomesh] | none | 8,697 vertices, 5,320 triangles (exact host count) | Host PC 66 to 78 ms (aomesh page; no ledger row); console not measured | Host PC time only, no ledger row; NOT RUN on a console |
| Shadow tiles, route A | 32 KB for a 128 × 128 tile (ESTIMATE: 2 bytes a pixel); the town's whole map was 128 KB (PVR-108) | the casters you draw | Uncut pass about 3.2 ms (PVR-004: 3,213 us at 320 × 240); PVR-156's 128 × 128 target: 3,256 us uncut, 344 us cut to 4 × 5 tiles | MEASURED, one console |
| Route A, whole old-town map | 128 KB | 27,031 TA vertices (PVR-108) | SH-4 13.6 ms, PVR 15.9 ms; 18 fps, misses 30 Hz | MEASURED, old town, one run |
| Light pools [pool] | none | 10 triangles, 11 vertices (exact, `sh4der_pool.h` lines 47 to 49) | Not measured; fill ESTIMATE from +3.1 ms per full-screen layer (PVR-002), by area | ALPHA v0.2 |
| Drifting specks [motes] | 512 B mask (exact) | 96 sprites × 4 = 384 vertices (exact) | Not measured | NOT RUN |
| Mist bank [mist] | 8 KB noise (yours) | 48 triangles at 12 sheets (exact) | Not measured; fill OPEN | NOT RUN |
| Dented ground [deform] | none | 25,600 cells of RAM (exact, `sh4der_deform.h` lines 13 to 14) | Not measured | NOT RUN |
| Water [water] | none | 2,016 to 23,040 triangles (exact, demo/OCEAN.md) | Not measured; demo ran (REPORTED) | PROTOTYPE |
| Exposure and grade [look] | none | 0 to 6 full-screen layers (exact) | +3.1 ms each (PVR-002); six: ESTIMATE 18.7 ms (arithmetic); grid read OPEN | PROTOTYPE |
| Film and tape [film], [timebase] | tape: 640 KB frame texture (ESTIMATE: 640 × 512 × 2 bytes) | film: one rectangle, 3 lines (exact) | Not measured | NOT RUN |
| Fake focus blur [focus] | 16 KB (ESTIMATE: 128 × 64 × 2 bytes) | 408 vertices, 384 triangles (exact) | Not measured | NOT RUN |
| Screen warp [warp] | 256 KB (ESTIMATE: 512 × 256 × 2 bytes) | mesh 2,460; pixel pass 1,200 (exact) | SH-4 mesh 2,250 us one at a time, 2,394 us in runs (BUS-004); PVR 6,466 us is in the warp page's bench table only | SH-4 MEASURED, one console (BUS-004); PVR REPORTED (warp page bench, no ledger row) |
| Tile cut [tilecut] | none extra | the table | PVR 3,312 to 375 us on a 20-tile target (PVR-092); SH-4 begin 126 us, end 130 us (PVR-205) | MEASURED, one console |

## Settings to start with

- **Baked corner shading [aobake].** `SH4DER_AOBAKE_RAYS` 64, `SH4DER_AOBAKE_REACH` 3.0f, `SH4DER_AOBAKE_PUSH` 0.02f (`sh4der_aobake.h` lines 64 to 66). ADVISED.
- **Sun shadow, route A [psm].** `warp_near` 4 m, above the 0.3 m near plane ([psm](../effects/psm.md), Pitfalls). `SH4DER_PSM_BIAS_A` 0.02f and `SH4DER_PSM_BIAS_B` 1.5f (`sh4der_psm.h` lines 91 to 92): ADVISED, not measured. Full strength to 40 m, none at 50 m: chosen numbers (frame budget, line 22), ADVISED.
- **Light pools [pool].** A 10-triangle disc, lifted 2 to 4 cm, blob alpha 0.50 by day, 0.25 at night (`sh4der_pool.h` lines 8 to 10). ADVISED. `SH4DER_POOL_REACH` 60.0f and `SH4DER_POOL_FADE_W` 10.0f (lines 100 to 101) are car-lamp values; a town needs its own (our advice, not tested).
- **Drifting specks [motes].** `max_draw` 96, `flat_px` 3.0 (`sh4der_motes.h` lines 124 to 125). ADVISED. The defaults assume a 4000-unit box (line 119): scale them.
- **Mist bank [mist].** `SH4DER_MIST_SLICES` 12, `SH4DER_MIST_OPACITY` 0.7, `SH4DER_MIST_FEATHER` 2.0 (`sh4der_mist.h` lines 89 to 92). ADVISED defaults.
- **Dented ground [deform].** `SH4DER_DEFORM_CELL` 0.25f (`sh4der_deform.h` line 84). Refill 0 keeps snow dents (line 114). ADVISED.
- **Exposure and grade [look].** `SH4DER_LOOK_GAIN_HI` 1.5f (`sh4der_look.h` line 86); `SH4DER_LOOK_LIGHT_S` 2.0 (line 87). Keep the gain at 0.4 to 2.0 ([recommended limits](../limits/recommended-limits.md)). ADVISED.

## What to cut first

Least picture lost first. Costs rank only where a number exists; the rest follow what the eye loses.

1. **Screen layers** ([film], [timebase], [focus]). Lost: the look. A full-screen layer is about 3.1 ms of PVR (PVR-002, MEASURED). Screen warp ([warp]) is dearer: its mesh pass is 6,466 us of PVR on the warp page's bench (REPORTED, no ledger row).
2. **Water.** Fewer triangles (the sea demo's PERFORMANT row, 2,016). Lost: wave detail. Cost not measured.
3. **Mist.** Fewer sheets (our advice, not tested; mist.md says keep the bank low or use fewer sheets, ADVISED). Lost: fog depth. Fill OPEN.
4. **Motes.** Lower `max_draw`. Lost: specks.
5. **Light pools.** Limit lamps by distance. Lost: far light. NOT RUN.
6. **Shadow tiles.** Fewer people get a tile. Lost: their shadows. Route B, if you use it, goes before tiles: it is the dearest shadow measured here (40.2 to 43.9 ms of SH-4, PVR-112 and PVR-113).
7. **Keep baked corner shading.** Its frame cost is 216 to 277 us of SH-4 (SH4-082), and its bake is paid at load.

## Common mistakes

- **Baking something that moves, or reading the byte as real light.** Movers stay open, so bake only the static part (`sh4der_aomesh.h` line 23). The byte multiplies ambient light only (`sh4der_aobake.h` line 14).
- **Drawing a pool before the ground, or writing depth from it** (`sh4der_pool.h` line 17).
- **Calling `sh4der_pool_reach_make` for each lamp.** Call it once a frame ([pool](../effects/pool.md), Pitfalls).
- **Stacking translucent triangles in one tile.** 256 in one tile took 6,385 us; spread out, 82 us (PVR-009, MEASURED).
- **A shadow with no range.** The old town's whole-map shadow ran at 13 to 18 frames a second (PVR-108, PVR-112, PVR-113; MEASURED, one run each).
- **Cutting one table of two.** The street demo's frame got 3.05 ms slower and its mirror's PVR time did not fall (PVR-200, MEASURED, one run).
- **Drawing the HUD through the tape pass** (`sh4der_timebase.h` line 49).

## Try this first

1. Include `sh4der_aobake.h` from the library's `include` folder. Build with KallistiOS and SH4ZAM ([setup](../start-here/setup.md)), and keep the credit.
2. At load, fill a `sh4der_aobake_occl_t` with your boxes, with `ground` 1 and `ground_y` at the floor. Call `sh4der_aobake_rays(dirs, SH4DER_AOBAKE_RAYS)` into a `float dirs[64][3]` array you own.
3. At load, call `sh4der_aobake_bytes` over your static vertices and unit normals, with a tick every 64 vertices (arguments at `sh4der_aobake.h` line 174). Keep the bytes, and time the load.
4. Each frame, multiply each vertex's ambient light by `sh4der_aobake_open(byte, 0)`.
5. Corners where walls meet floors read darker. Compare the frame with the bake on and off: in the town's AO build the bytes moved the still frame by 0.2 ms and a turn by 0.6 ms (PVR-135, MEASURED, one console; the row calls it nothing measurable). Your scene may differ.

## Related

- [Racing game recipe](./racing-game.md); [What do you want to make?](../start-here/what-do-you-want.md); [demo guide](../start-here/demo-guide.md) (its walled square is NOT RUN).
- Genre guides: [shadows](../start-here/genre-shadows.md), [light and glow](../start-here/genre-light-and-glow.md), [water and weather](../start-here/genre-water-and-weather.md), [camera and screen](../start-here/genre-camera-and-screen.md).
- [Frame budget](../limits/frame-budget.md), [recommended limits](../limits/recommended-limits.md), and [licence and credit](../start-here/license-and-credit.md), which you need when you ship any of this.
