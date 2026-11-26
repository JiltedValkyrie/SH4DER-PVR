---
title: "Flight and space: a recipe for the sun, the clouds and the speed"
tags: [guide, flight, space, game-type, recipe, effect]
updated: 2026-11-26
sources: [include/sh4der_glow.h, include/sh4der_bloom.h, include/sh4der_spark.h, include/sh4der_bolt.h, include/sh4der_look.h, include/sh4der_trail.h, include/sh4der_warp.h, include/sh4der_water.h, include/sh4der_hexheat.h, include/sh4der_cloudvolume.h, include/sh4der_lens.h, include/sh4der_motes.h, wiki/guides/racing-game.md, wiki/start-here/call-orders.md, wiki/start-here/genre-light-and-glow.md, wiki/start-here/genre-water-and-weather.md, wiki/start-here/genre-camera-and-screen.md, wiki/effects/glow.md, wiki/effects/bloom.md, wiki/effects/spark.md, wiki/effects/look.md, wiki/effects/trail.md, wiki/effects/warp.md, wiki/effects/cloudvolume.md, wiki/effects/hexheat.md, wiki/effects/lens.md, wiki/effects/motes.md, wiki/effects/bolt.md, wiki/effects/water.md, wiki/effects/split.md, wiki/effects/rise.md, wiki/limits/frame-budget.md, wiki/hardware/ledger-pvr.md, wiki/hardware/ledger-sh4.md, wiki/hardware/ledger-bus-dma.md, wiki/hardware/ledger-video.md, demo/OCEAN.md, AGENTS.md]
---
# Flight and space: a recipe for the sun, the clouds and the speed

**What you will learn:** which effects suit a flight or space game, what each costs on a Dreamcast, the order of one frame, and what to cut when the frame is slow.
**For:** a developer making a flight or space game on the Dreamcast who has never used SH4DER-PVR.

## The short version

- Start with sparks for tracers and exhaust, and mesh bloom for engine glow, which needs no frame read.
- Build the spark pool, then the engine glow, and time the frame with sprites on and off (our advice, not tested).
- Budget: an empty frame is 3.15 ms of PVR (MEASURED, PVR-001), and each full-screen layer adds 3.1 ms (MEASURED, PVR-002).
- Cut order: glow rays (about 12.4 ms of PVR, ESTIMATE), then trail down to two copies, then screen bloom (9.3 ms, ESTIMATE).
- The heat haze, motion blur and cloud deck are NOT RUN, and the sources disagree on the glow threshold (0.62 or 0.80, ADVISED).

## What this kind of game needs

A flying ship needs a bright sun, engine glow, sparks from guns and hits, a sense of speed, shimmering hot air, a sea or cloud deck below, and eyes that adjust from sunlight into dark space.

The budget is the hard part. The main processor (the SH-4) and the graphics chip (the PVR; terms in the [glossary](../glossary.md)) share each frame. A frame has 16.7 ms at 60 frames a second (60 Hz) and 33.3 ms at 30 (30 Hz). The PVR spends 3.15 ms on an empty 640 × 480 frame (PVR-001, MEASURED). Each full-screen translucent layer, a glow or flash over the whole screen, adds about 3.1 ms (PVR-002, MEASURED). Pick a few such layers and keep the rest small.

Tags: **MEASURED** = a ledger row (ID given), one console. **ADVISED** = a header or demo value, with file and line. **NOT RUN** = no console run. **ESTIMATE** = arithmetic or a named model. A host count is exact on the PC, not a console number.

## Effects that fit, and why

| Effect | What it gives this game | Where it is | Status |
|---|---|---|---|
| Screen bloom [glow] | Sun rays and bright parts bleed light. Needs the frame read. | [header](../../include/sh4der_glow.h), [page](../effects/glow.md) | ADVISED values; PROTOTYPE v0.1 (`glow.h:17`). The sources disagree: `glow.h:58` and the glow page say Version v0.50, runs on the demo disc |
| Mesh bloom [bloom] | Glow on an engine nozzle, a sun disc or a lamp. No frame read. | [header](../../include/sh4der_bloom.h), [page](../effects/bloom.md) | The sources disagree: `bloom.h:31` says v1.00 (used in a released game); maturity.md says ALPHA v0.3 for rings and PROTOTYPE v0.1 for the blade glow |
| Sparks and tracers [spark] | Exhaust, tracers, hit flashes from a pool of 24 quads. | [header](../../include/sh4der_spark.h), [page](../effects/spark.md) | PROTOTYPE v0.1; vertex cost MEASURED (BUS-001, BUS-003) |
| Lightning [bolt] | A weapon's arc. | [header](../../include/sh4der_bolt.h), [page](../effects/bolt.md) | PROTOTYPE v0.1 (maturity.md); NOT RUN |
| Drifting specks [motes] | Dust and embers in space. | [header](../../include/sh4der_motes.h), [page](../effects/motes.md) | PROTOTYPE v0.1 (maturity.md); NOT RUN |
| Exposure and grade [look] | The eye adjusts from sun to dark; a colour grade. | [header](../../include/sh4der_look.h), [page](../effects/look.md) | PROTOTYPE v0.1 (maturity.md); on the demo disc, no ledger row; read not timed. The sources disagree: `look.h:36` says Version v0.50 (runs on the demo disc) |
| Screen overlays [lens] | A flash on a hit, a dark vignette. | [header](../../include/sh4der_lens.h), [page](../effects/lens.md) | PROTOTYPE v0.1 (maturity.md); no console row |
| Screen warp [warp] | A shockwave ring, heat wobble, speed blur. | [header](../../include/sh4der_warp.h), [page](../effects/warp.md) | Send cost MEASURED (BUS-004); PVR cost not in the ledger |
| Heat haze hexagons [hexheat] | Hexagons over a hot far horizon. | [header](../../include/sh4der_hexheat.h), [page](../effects/hexheat.md) | NOT RUN; fits when you already draw a target pass |
| Motion blur [trail] | Streaks on turns and boosts. | [header](../../include/sh4der_trail.h), [page](../effects/trail.md) | NOT RUN; by ESTIMATE, 2 copies (6 layers) alone pass 16.7 ms |
| Water waves [water] | A sea below. | [header](../../include/sh4der_water.h), [page](../effects/water.md) | REPORTED on a console; frame time OPEN |
| Cloud deck [cloudvolume] | Clouds to fly through. | [header](../../include/sh4der_cloudvolume.h), [page](../effects/cloudvolume.md) | NOT RUN; cost is ESTIMATE |

Left out. Rising lights [rise] (lights that lift into stars; a city's lights stay put, our judgement). Split screen [split] (a two-view tool; a moving divider has not run on a console, PVR-144 and PVR-161 cover the middle position).

## The order of one frame

The sources give the inside order of some passes: glow (`glow.h:47-49`), look and spark (AGENTS.md). Nothing sources a whole flight frame, the cloud deck or the warp. This order is our advice, not tested.

1. **Read the last picture.** The frame read (the SH-4 copies a grid of pixels off the screen) for look and glow, and trail's frame call (`trail.h:169`). The screen holds a picture one or two frames old (VID-006), so read it before you draw over it.
2. **Do the maths.** Look's eye step and solve, glow's upload (it returns the map's mean), the spark update, the motes collect, the bolt re-roll.
3. **Render target passes** (pictures the chip draws, to use later as textures), such as the hexagon band. The screen reads them, so they finish before the screen scene starts. The [racing guide](./racing-game.md) does the same with its mirror (section 4).
4. **Opaque world**: sky, sea, ground, ship. Blends sit on a finished solid world. The cloud deck is one quad over its own sky (`cloudvolume.h:9-11`). Our advice, not tested: draw it after the sky, then check it does not cover the ship.
5. **Punch-through list: motes** (shapes with hard, cut-out edges), after the world, depth write on ([genre guide](../start-here/genre-water-and-weather.md), step 5).
6. **Translucent list: mesh bloom, sparks, bolts, hexagons**, after the world. These are see-through or additive shapes (they add light to what is under them), so no depth write. Keep sprites spread out (PVR-009).
7. **Full-screen layers**: screen bloom (its bloom, halo, then rays), look's grade layers, trail's copies, the lens flash, the warp. Each covers the screen, so count them. The sources do not rank these; test the order. The warp needs the picture as a texture (`warp.h:13-14`; [colour fringe](../effects/ca.md) shows how).
8. **HUD last.** Look and lens layers sit under it (`lens.h:95`).

## What it costs

A frame costs about the larger of the SH-4 and PVR when they overlap, or their sum when they do not ([frame budget](../limits/frame-budget.md), section 1). Each row is one part alone. Percentages are of 16.7 ms and 33.3 ms.

| Part | Video memory | Triangles or vertices | Frame time | Status |
|---|---|---|---|---|
| Empty 640 × 480 frame | none | none | 3.15 ms PVR (19 % / 9 %) | MEASURED, PVR-001 |
| One full-screen translucent layer | none | 2 triangles | +3.1 ms PVR each (19 % / 9 %) | MEASURED, PVR-002 |
| One render target pass | w × h × 2 bytes (arithmetic) | your scene again | 3.2 ms PVR floor | MEASURED, PVR-004 |
| Frame read, 80 × 60 | none | 4,800 loads | about 2.6 ms SH-4 (ESTIMATE: 9,600 bytes at the 3.7 MB/s of SH4-040) | Another port REPORTED 1,196 us (VID-005); ours not measured |
| Screen bloom | 16 KB texture, 1 KB shrink (`glow.h:107-112`) | 3 layers, up to 4 ray copies | 9.3 ms PVR (56 % / 28 %), ESTIMATE: 3 × PVR-002 | NOT RUN |
| Exposure and grade | none | 0 to 6 quads | 3.1 ms PVR a layer; 2 layers 6.2 ms (37 % / 19 %), ESTIMATE | NOT RUN timed |
| Mesh bloom | none | 24 vertices (blade glow); up to 193 (rings, `bloom.h:59`) | ESTIMATE: 205 us SH-4 for a 129-vertex halo, as the lamp program's own loop timed it (SH4-083, MEASURED in that loop; the library call alone is not timed) | The sources disagree on status (`bloom.h:31` v1.00; maturity.md ALPHA v0.3 for rings); PVR not measured |
| Sparks | your atlas, about 3 KB at 64 × 64 (README, as stated) | 96 vertices (24 quads) | 14 to 25 us SH-4 (ESTIMATE: 96 × 142 to 260 ns, BUS-003, BUS-001) | Pool NOT RUN |
| Lightning and motes | motes 512 B mask | bolt 240 vertices; motes 384 | bolt 34 to 62 us; motes 55 to 100 us SH-4 (ESTIMATE, same rates) | NOT RUN |
| Screen warp | 256 KB, 512 × 256 RGB565 (`warp.h:13-14`, arithmetic); `README.md:64` gives 160 KB, so the sources disagree | mesh 2,460 vertices | 2.25 ms SH-4 for the mesh (BUS-004, MEASURED, h2) | PVR not in the ledger |
| Heat haze hexagons | 64 KB band (arithmetic) | about 17 hexagons of 4 triangles (host count) | far world sent again: not measured | NOT RUN |
| Motion blur | 128 B mask | 2 copies = 6 layers; 3 copies = 8 | 18.7 ms at 2 copies (112 % / 56 %); 24.9 ms at 3: ESTIMATE, layers × PVR-002 | NOT RUN |
| Cloud deck | 16 KB (8-bit) or 32 KB (RGB565) at 128 × 128 (`cloudvolume.h:16-17`) | one quad | SH-4 14 to 22 ms (ESTIMATE, instruction counts) plus one layer, 3.1 ms PVR (ESTIMATE) | NOT RUN |
| Water waves | none | sea mesh 2,016 to 23,040 triangles (`demo/OCEAN.md`) | not measured; a steady 33.3 ms not yet on hardware (OCEAN.md:5) | REPORTED on a console |

One ESTIMATE sum, kept apart from the MEASURED empty frame: glow's 9.3 ms plus look's two layers, 6.2 ms, make 15.5 ms of PVR (93 % of 16.7 ms; 47 % of 33.3 ms). At 60 Hz those two take nearly the whole frame. At 30 Hz the cloud march's 14 to 22 ms of SH-4 takes most of the rest. The cloud page's 143 KB RAM total does not add up (about 90 KB by arithmetic; OPEN).

## Settings to start with

- **Screen bloom:** `SH4DER_GLOW_THRESHOLD` 0.62, `SH4DER_GLOW_POWER` 2, `SH4DER_GLOW_STRENGTH` 0.45, `SH4DER_GLOW_RADIUS` 0.5, `SH4DER_GLOW_DARKEN_MAX` 0.25, no rays (`glow.h:8-9`). ADVISED. The sources disagree on the threshold: `glow.h:8-9` and `README.md:486` say 0.62; `glow.h:68-70` and `README.md:1636` say 0.80. Rays fade out `SH4DER_GLOW_SUN_EDGE` 0.25 screen heights past the edge (`glow.h:124`).
- **Sparks:** `SH4DER_SPARK_MAX` 24, `SH4DER_SPARK_MIN_PX` 0.8, `SH4DER_SPARK_INSET` 0.0078125 for a 64 × 64 atlas; `SH4DER_SPARK_KEEP` on flashes and tracers (`spark.h:11-17`). ADVISED.
- **Exposure and grade:** `SH4DER_LOOK_LIGHT_S` 2.0 and `SH4DER_LOOK_DARK_S` 6.0 (`look.h:87-88`); eye strength 0.25 (`look.h:46-53`). ADVISED, the author's eye. Our advice, not tested: hold the eye during a flash, as `look.h:55-56` does for a flashlight.
- **Screen warp:** `SH4DER_WARP_SOURCES` 4, `SH4DER_WARP_BLUR_COPIES` 4 at most, `SH4DER_WARP_PIXEL_CELL` 16 (`warp.h:71, 86, 88`); `SH4DER_WARP_MESH_SEND_PRIM` 1 and `SH4DER_WARP_PIXEL_SEND_PRIM` 0 (`src/sh4der_warp.c:67, 70`). ADVISED; the send choice is MEASURED (BUS-004).
- **Motion blur:** `SH4DER_TRAIL_COPIES` 3 by default, 2 the lowest (`trail.h:67`); `SH4DER_TRAIL_SHIFT_MAX` 8 (`trail.h:72`). ADVISED, NOT RUN.
- **Cloud deck:** `SH4DER_CLOUDVOLUME_STEPS` 16 by default, `SH4DER_CLOUDVOLUME_MAX_STEPS` 64 (`cloudvolume.h:81, 97`). ADVISED; the cost is ESTIMATE.
- **Water waves:** `footprint` at your vertex spacing, or 0 to switch the fade off; full at 6 footprints, gone at 2 (`water.h:54-57`). ADVISED.
- **Screen overlays:** `SH4DER_LENS_VIG_SPOKES` 32, `SH4DER_LENS_Z_VIG` 430 (`lens.h:68, 95`). ADVISED.
- **No recommended value yet** for mesh bloom (README "Bloom tuning" defaults only), lightning (`SH4DER_BOLT_STRANDS` 4, `SH4DER_BOLT_SEGS` 6, `bolt.h:64-65`) or motes (`max_draw` 96, `motes.h:125`). Time each on and off on a console.

## What to cut first

Our advice, not tested. The costs are ESTIMATES unless a row says MEASURED.

1. **Glow rays off.** Do not call `sh4der_glow_draw_rays` (`glow.h:49`). Saves up to four full-screen layers, about 12.4 ms PVR (ESTIMATE). You lose the sun's rays.
2. **Trail to 2 copies, then off.** Each copy is two layers, about 6.2 ms PVR (ESTIMATE). You lose the streak, then all motion blur.
3. **Screen bloom off, mesh bloom on.** Saves about 9.3 ms PVR (ESTIMATE). You lose the bleed of bright parts; the engine glow stays.
4. **Fewer look grade layers.** Each is about 3.1 ms PVR (ESTIMATE). You lose what that layer's tint or lift did.
5. **Cloud deck steps down, then off.** Its SH-4 time scales with the steps (ESTIMATE). You lose cloud detail, then the clouds.
6. **Warp when needed, and no more.** The mesh costs 2.25 ms of SH-4 (BUS-004). You lose the shockwave or haze in the frames it is off.
7. **Sparks, bolts, motes and mesh bloom last.** Each is under a millisecond of SH-4 (ESTIMATE; SH4-083 times the lamp program's halo, not the library call alone).

## Common mistakes

- **The glow feeds itself** unless you take its own glow and gain off the read (`sh4der_glow_unfeed`, `sh4der_frame_degain`; [glow page](../effects/glow.md)).
- **Darken without bloom** drops the scene's mean brightness (`glow.h:16`).
- **Trail and colour fringe share one stride register** (`trail.h:23-24`; VID-003). Trail also needs 640 × 480, no AA 2X, dithering off (`trail.h:22-23`).
- **Stacked translucent shapes in one tile:** 256 in one tile took 6,385 us; spread, 82 us (PVR-009, MEASURED). Spread sparks, motes and lamps.
- **Sprites outside the translucent list or before the world,** or quads sent without the area test (`spark.h:18-19`; `sh4der_tri_area_ok`, [send checks](../effects/guard.md)).
- **The eye chases itself** if its read still holds last frame's gain (`look.h:57-58`).
- **Hexagons:** do not put near things in the band, and do not sample outside it (`hexheat.h:14`).
- **Lens grain** is skipped if there was no video memory at init, until the next PVR start ([lens page](../effects/lens.md)).
- **Motes drawn before the world.** Their punch-through list writes depth ([motes page](../effects/motes.md)), so it goes after the world.
- **Water:** the header does not check the wave pointer for NULL ([water page](../effects/water.md)).
- **Warp send modes** rest on one run (BUS-004). Time both before you change a default ([warp page](../effects/warp.md)).

## Try this first

Our advice, not tested. It follows the spark section of the README and the headers.

1. Include `sh4der_spark.h`, `sh4der_bloom.h` and `sh4der_guard.h`, and build `src/sh4der_spark.c` and `src/sh4der_bloom.c` (bloom's .c is maths, with no drawing calls, [bloom page](../effects/bloom.md)). Link SH4ZAM as [setup](../start-here/setup.md) says.
2. At load, put your sprite atlas (no alpha) in video memory and call `sh4der_spark_init(texture, w, h, format)`, then `sh4der_spark_reset`. Make one additive translucent header (source alpha, one), depth test on, no depth write.
3. On a shot, call `sh4der_spark_streak` for a tracer with `SH4DER_SPARK_KEEP`, or `sh4der_spark_billboard` for a flash. Once a frame, call `sh4der_spark_update(pool, dt, gravity)`.
4. In the translucent list after the world, fill the view and screen records and call `sh4der_spark_draw`. For an engine glow, draw bloom levels with `sh4der_bloom_disc`, additive, depth test on, no depth write (the blend rule in `bloom.h:28-29`).
5. You should see short streaks leave the nozzle and fade, with a soft glow round it. Then time the frame with sprites on and off (`spark.h:16`).

## Related

- [Racing game: a recipe for the car, its shadow and its lights](./racing-game.md): the worked example for full-screen costs.
- [What do you want to make?](../start-here/what-do-you-want.md)
- Genre guides: [light and glow](../start-here/genre-light-and-glow.md), [water and weather](../start-here/genre-water-and-weather.md), [camera and screen](../start-here/genre-camera-and-screen.md).
- [Frame budget](../limits/frame-budget.md), [budgets](../limits/budgets.md), [recommended limits](../limits/recommended-limits.md).
- [Licence and credit](../start-here/license-and-credit.md): required when you use this library in a game.
