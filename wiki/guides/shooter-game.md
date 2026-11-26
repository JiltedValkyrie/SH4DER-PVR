---
title: "First-person shooter: a recipe for the gun, the hits and the dark"
tags: [guide, shooter, game-type, recipe, effect]
updated: 2026-11-26
sources: [wiki/guides/racing-game.md, wiki/effects/names.md, wiki/start-here/call-orders.md, wiki/glossary.md, wiki/limits/frame-budget.md, wiki/limits/recommended-limits.md, wiki/hardware/ledger-pvr.md, wiki/hardware/ledger-sh4.md, wiki/hardware/ledger-bus-dma.md, wiki/start-here/genre-tools-and-safety.md, wiki/effects/ (the pages the tables link), include/sh4der_spark.h, include/sh4der_pool.h, include/sh4der_aobake.h, include/sh4der_warp.h, include/sh4der_lens.h, include/sh4der_torch.h, include/sh4der_frame.h, include/sh4der_fault.h, README.md, AGENTS.md]
---
# First-person shooter: a recipe for the gun, the hits and the dark

**What you will learn:** how to build one frame of a first-person shooter with this library: a gun's flashes and tracers, hit feedback on the screen, dark rooms and a few outdoor props. You will also learn what each part costs, what to cut first, and what goes wrong.
**For:** a developer making a first-person shooter on the Dreamcast who has never used SH4DER-PVR.

No shooter has run on a console from this library. The nearest demo is `demo/torchwalk_ab.c`, a first-person walk with the torch. Each number below says what it rests on.

## The short version

- Start with sparks and tracers for the muzzle flash and hits, then the screen overlays for hit feedback.
- Build a spark pool of 24 quads on a 64 x 64 atlas, where the flash and tracer are never pushed out (ADVISED).
- Budget: each full-screen layer is 3.1 ms of PVR (MEASURED, PVR-002), so count layers before adding more.
- Cut order: second views to their tiles (3,312 to 375 us of PVR on a 20-tile target, MEASURED, PVR-092), then specks and far pools.
- The whole shooter is NOT RUN on a console, and the sources disagree on the torch's near light.

## What this kind of game needs

A shooter frame has a gun near the eye, short bright flashes, a screen that reacts to hits, dark rooms, and sometimes fog, snow or an explosion. The library gives picture parts, not game logic. It has no gun, no hit test, no ammo and no enemies. The README's spark section says it plainly: "Nothing here knows a weapon."

Two facts shape the frame:

- **A full-screen translucent layer costs about 3.1 ms of PVR time** at 640 x 480 (PVR-002, MEASURED, one console; blend and additive cost the same in that bench). Count your layers before you add anything else.
- **Corners are baked at load.** A bake of 2,142 vertices took 252 to 254 ms (SH4-076 and SH4-082, MEASURED, one console; the ao4 scene's bake, whose code the library's `aobake.h` now holds). Never bake inside a frame.

Our advice, not tested: aim for 60 Hz if you can.

## Effects that fit, and why

| Effect | What it gives this game | Where it is | Status |
|---|---|---|---|
| Sparks and tracers [spark] | muzzle flash, tracer, impact sparks | [header](../../include/sh4der_spark.h), [page](../effects/spark.md) | ADVISED; pool draw Run on a console by the owner in Match Light (her test; no measured numbers recorded yet) |
| Screen overlays [lens] | hit wedges, damage flash, vignette, grain | [header](../../include/sh4der_lens.h), [page](../effects/lens.md) | ADVISED; NOT RUN |
| Screen spotlight [torch] | a flashlight in a dark corridor | [header](../../include/sh4der_torch.h), [page](../effects/torch.md) | ADVISED; Run on a console by the owner in Torch walk; Demo disc (her test; no measured numbers recorded yet) |
| Baked corner shading [aobake] | contact shadow in rooms, baked at load | [header](../../include/sh4der_aobake.h), [page](../effects/aobake.md) | load MEASURED; copy ADVISED |
| Light pools [pool] | a lamp's pool; a blob shadow under each enemy (our advice, not tested) | [header](../../include/sh4der_pool.h), [page](../effects/pool.md) | ADVISED; reach fade Run on a console by the owner in Street car demo, Car on hills and Drive a car on hills (her test; no measured numbers recorded yet) |
| Exposure and grade [look] | eyes adjust leaving a dark room | [header](../../include/sh4der_look.h), [page](../effects/look.md) | on the demo disc; cost NOT MEASURED |
| Screen warp [warp] | shockwave from an explosion | [header](../../include/sh4der_warp.h), [page](../effects/warp.md) | SH-4 MEASURED; PVR REPORTED |
| Drifting specks [motes] | dust in a light beam, embers | [header](../../include/sh4der_motes.h), [page](../effects/motes.md) | Run on a console by the owner in Match Light (her test; no measured numbers recorded yet) |
| Mist bank [mist] | fog low in a yard | [header](../../include/sh4der_mist.h), [page](../effects/mist.md) | Run on a console by the owner in Match Light (her test; no measured numbers recorded yet) |
| Dented ground [deform] | footprints in snow | [header](../../include/sh4der_deform.h), [page](../effects/deform.md) | Run on a console by the owner in Match Light (her test; no measured numbers recorded yet) |
| Lightning [bolt] | an electric weapon | [header](../../include/sh4der_bolt.h), [page](../effects/bolt.md) | NOT RUN |
| Screen filters [visor] | night goggles (added by us) | [header](../../include/sh4der_visor.h), [page](../effects/visor.md) | NOT RUN |
| Tile-cut pass [tilecut] | for a second view, such as a scope | [header](../../include/sh4der_tilecut.h), [page](../effects/tilecut.md) | MEASURED (PVR-092) |
| Crash and hang screen [fault] | development: a report when the build dies | [header](../../include/sh4der_fault.h), [page](../effects/fault.md) | PROTOTYPE |

Left out. Corner darkening [ao] reads the frame, as Exposure and grade does ([ao page](../effects/ao.md)), and baked corners cover the same corners (our reading). Sun shadow map [psm] costs most. In the town shadow demo, route A (the PVR draws the map) took 15.9 ms of PVR (PVR-108; the HUD's figure, map and screen together, PVR-197), and route B (a depth map on the SH-4) took 40.2 ms of SH-4 (PVR-112). A blob shadow is the cheaper choice for a yard (our advice, not tested).

## The order of one frame

The sources give exact calls for sparks, pools and the look (AGENTS.md), and one shape most effects share ([call orders](../start-here/call-orders.md)). They give no order for the other effects here. Where they are silent, the steps are our advice, not tested.

1. **At load, once.** Bake the room. Load the spark atlas and reset the pool. Make the mist noise and the speck mask. Set up the dent window and the look's bins. A 0 from an init means no video memory: draw nothing for that effect.
2. **Game update, before any draw.** Start the shot's sparks, stamp dent presses (feet, wheels), recentre the dent window, step the torch, then `sh4der_spark_update` with the time step.
3. **Read the last picture.** The look reads the finished frame on its grid, takes the gain off, and steps the eye ([frame read](../effects/frame.md)). The gain goes into this frame's grade.
4. **Frame texture, if you use warp.** Draw the world into the render target and wait for it to finish before the screen uses it, as the [racing guide](./racing-game.md) does for its mirror. The warp draws that texture through its mesh (warp.h, lines 13 to 15). A target pass costs about 3.2 ms of PVR (PVR-004, MEASURED).
5. **Opaque list.** The world with the baked corner bytes multiplied into the ambient light, the gun, and the dent ground with heights read per vertex.
6. **Punch-through list.** The specks. Their list writes depth, so it comes after the world ([motes page](../effects/motes.md)).
7. **Translucent list, after the world.** Lamp pools after the ground, mist, sparks and tracers, lightning, dent prints.
8. **Overlays.** The torch carve darkens what its near light lit, so it comes after the world (README, torch section). Then the lens vignette, grain, wedges and flash, whose depth keys sit under the HUD ([lens page](../effects/lens.md)).
9. **Grade, then HUD.** The look's full-screen layers go under the HUD. Then the HUD. If the vertex buffer can fill, send the HUD first. In one run the HUD and the map inset were off screen for about 20 s while the buffer read 100 % full (PVR-185, MEASURED, one run). That the full buffer caused it is an ESTIMATE (PVR-185; PVR-149). [Frame budget](../limits/frame-budget.md), section 3, states it as fact, so the sources disagree.
10. **At every send.** Check each header with `sh4der_poly_hdr_ok` and each triangle with `sh4der_tri_area_ok`.

## What it costs

A 60 Hz frame has 16.7 ms; a 30 Hz frame has 33.3 ms. The SH-4 and the PVR are separate bounds, and the frame costs about the larger of the two, or their sum when they run in turn ([frame budget](../limits/frame-budget.md), section 1). One full-screen layer is nearly a fifth of a 60 Hz frame (ESTIMATE: 3.1 of 16.7 ms).

| Part | Video memory | Triangles or vertices | Frame time | Status |
|---|---|---|---|---|
| Sparks and tracers, 24 quads | 1,256 B RAM; 3 KB atlas | 96 vertices (exact) | under 25 us of SH-4 (ESTIMATE: 96 at 142 to 260 ns, BUS-003, BUS-001) | Run on a console by the owner in Match Light (her test; no measured numbers recorded yet) |
| Lens flash | none | one full-screen layer | 3.1 ms of PVR while shown | MEASURED per layer (PVR-002) |
| Lens grain | 8 KB | one full-screen textured layer | not measured | NOT RUN |
| Lens vignette | none | 594 vertices, 288 triangles at the defaults (32 spokes x 8 rings, per frame; `src/sh4der_lens.c:19`) | not measured | NOT RUN |
| Torch | none | 306 vertices, 288 triangles (exact) | not measured (ESTIMATE: carve covers the screen, about 3.1 ms) | Run on a console by the owner in Torch walk; Demo disc (her test; no measured numbers recorded yet) |
| One light pool | none | 10 triangles, 176 B (exact) | not measured (ESTIMATE: area share of a full-screen layer) | Run on a console by the owner in Car on a wet night road; Drive a car on hills; Drive a wet night road; Street car demo (her test; no measured numbers recorded yet) |
| Mist, 12 sheets | 8 KB | 48 triangles (exact) | not measured | Run on a console by the owner in Match Light (her test; no measured numbers recorded yet) |
| Drifting specks | 512 B (optional) | 384 vertices at 96 sprites (exact) | not measured | Run on a console by the owner in Match Light (her test; no measured numbers recorded yet) |
| Dented ground | none; 25.6 KB RAM | no new triangles | not measured | Run on a console by the owner in Match Light (her test; no measured numbers recorded yet) |
| Screen warp, 40 x 30 mesh | 256 KB frame texture (ESTIMATE: 512 x 256 x 2 B, `warp.h:69-70`; `README.md:64` gives 160 KB, so the sources disagree) | 2,460 vertices | SH-4 2,250 us (BUS-004, MEASURED); PVR 6,466 us (`warp.md:36`, no ledger row) | PVR REPORTED |
| Exposure read | none | 4,800 samples | about 2.6 ms of SH-4 (ESTIMATE: 9,600 B at the 3.7 MB/s of SH4-040) | ESTIMATE |
| Exposure layers | none | up to 6 quads; none when neutral | 3.1 ms of PVR each (PVR-002); 18.7 ms at six | MEASURED per layer |
| Lightning | 3.3 KB RAM | 240 vertices, 120 triangles (exact) | not measured | NOT RUN |
| Baked corners | one byte a vertex | none at run time | load 252 to 254 ms (SH4-076, SH4-082); a frame +222 us of SH-4 in an ao4 build (PVR-176) | MEASURED |
| Second view (tile cut) | 14,400 B RAM | 3,312 to 375 us of PVR, a 20-tile map pass in the compare program (PVR-092) | MEASURED | ALPHA |

No total. No shooter has run as a whole, so the sum is NOT RUN. The nearest measured frame is a corridor filter test: 16.63 to 16.66 ms at 640 x 480, 60 Hz held (PVR-180, MEASURED, one console). It is a filter test, not a game.

## Settings to start with

**Sparks and tracers**
- `SH4DER_SPARK_MAX` 24, `SH4DER_SPARK_MIN_PX` 0.8, `SH4DER_SPARK_INSET` 0.0078125 for a 64 x 64 atlas (spark.h, lines 10 to 12). ADVISED.
- `SH4DER_SPARK_KEEP` on the flash and the tracer, and nothing else (spark.h, line 17). ADVISED.

**Screen overlays**
- `SH4DER_LENS_VIG_SPOKES` 32, `SH4DER_LENS_GRAIN_AMP` 0.06 (README, lens tuning). ADVISED. Spokes 16 give a worst error of 9.1 out of 255; 32 give 2.3 (a host sweep, ESTIMATE: `include/sh4der_lens.h:106`, README 'The lens effect').
- `SH4DER_LENS_HIT_LIFE_S` 1.5 for a hit wedge (README, lens tuning). ADVISED. The flash's default colour is blue-white; pick your own for damage (our advice).

**Screen spotlight**
- Near radius 6.0, strength 0.85, reach 5.0, carve full under luminance 0.30 (README.md:1641, recommended settings). ADVISED, found by eye.
- The sources disagree on the near light. The header's defaults are `SH4DER_TORCH_NEAR_WU` 7.0 and `SH4DER_TORCH_NEAR_K` 0.425 (`torch.h:98-99`); the README's set above is radius 6.0 and strength 0.85 (README.md:1641). Pick one set. Under AA 2X, scale x once (VID-010).

**Baked corners**
- `SH4DER_AOBAKE_RAYS` 64, `SH4DER_AOBAKE_REACH` 3.0, `SH4DER_AOBAKE_PUSH` 0.02 (aobake.h, line 7). ADVISED start values.
- Bake once per scene, at load, never per frame (aobake.h, line 13). Its load time is in the table above.

**Light pools**
- Disc of 10 triangles for a lamp (a blob shadow takes 8, pool.h:8), lifted 2 to 4 cm; blob alpha 0.50 by day, 0.25 at night (pool.h, lines 8 to 10). ADVISED.
- `SH4DER_POOL_REACH` 60 m and `SH4DER_POOL_FADE_W` 10 m (pool.h, lines 100 to 101). Run on a console by the owner in Street car demo, Car on hills and Drive a car on hills (her test; no measured numbers recorded yet); a shorter indoor reach is our guess.

**Screen warp**
- `SH4DER_WARP_RING_SPEED` 180.0, `SH4DER_WARP_RING_AMP` 5.0 (README, warp tuning). ADVISED; no ledger row times the ring.
- Mesh by `pvr_prim` (`SH4DER_WARP_MESH_SEND_PRIM` 1, warp.c lines 50 to 71). MEASURED (BUS-004).

**Exposure and grade**
- Gain between 0.4 and 2.0 (`look.h:52-53`; README.md:1635, recommended settings); `SH4DER_LOOK_GAIN_LO` 0.75, `SH4DER_LOOK_GAIN_HI` 1.5 (`look.h:85-86`). ADVISED.
- Eye 2 s into light, 6 s into dark (`SH4DER_LOOK_LIGHT_S`, `SH4DER_LOOK_DARK_S`). Hold the eye while a torch is lit (README, recommended settings). ADVISED, "felt, not seen".

**Specks, fog and snow**
- Specks: `max_draw` 96 (README, motes section). ADVISED default.
- Mist: `SH4DER_MIST_SLICES` 12, `SH4DER_MIST_OPACITY` 0.7 (README, mist tuning). ADVISED defaults.
- Dents: `SH4DER_DEFORM_CELL` 0.25, refill 0 for snow (README, deform section). ADVISED.

## What to cut first

Our advice, not tested. The costs are the ones in the table above.

1. **Second views, to their tiles.** The picture stays the same: 3,312 to 375 us of PVR for a 20-tile target (PVR-092, MEASURED, the compare program's map pass). Cut when every table is found: with one table of two cut, the street demo's frame rose 3.05 ms (PVR-200).
2. **Specks and far pools.** Fewer specks, a shorter pool reach. You lose distant detail. Saving NOT MEASURED.
3. **Grain, then vignette.** Grain is a full-screen textured layer (not measured); spokes 16 lose a little smoothness.
4. **Mist.** Fewer sheets (12 to 4). You lose fog thickness. Fill NOT MEASURED.
5. **Exposure.** A neutral grade draws no layer. Turning the eye off saves 3.1 ms of PVR per layer (PVR-002) and the read (about 2.6 ms, ESTIMATE). You lose the adjusting feel.
6. **Warp.** Use the coarse 20 x 15 grid (its time has no row) instead of 40 x 30, which cost 2.25 ms of SH-4 (BUS-004). Drop warp, and if nothing else uses the frame texture, its target pass goes too (PVR-004). You lose the shockwave.
7. **Lightning, then the torch.** Fewer strands (bolt.h macros). The torch goes last: its carve covers the screen, and you lose the flashlight.

## Common mistakes

- **Every spark marked KEEP.** A full pool then refuses plain quads and counts them (spark.h, line 53).
- **Zero-area triangles.** They draw bright lines on a real console; an emulator draws nothing (PVR-080; README, real hardware notes).
- **A polygon header never compiled.** On a real console it ends the list, and the console can stop (PVR-081, FACT).
- **Baking in a frame.** Bake at load, once per scene (aobake.h, line 13). A 152 ms static bake showed as one long frame (SH4-070).
- **Reading video memory the slow way.** A 16-bit loop from uncached video memory ran at 3.7 MB/s (SH4-040). A 32-bit `shz_memcpy32` through the operand cache ran at 38.6 to 38.7 MB/s (SH4-041); a cached range must be invalidated first, or it reads back old data (SH4-006). The look reads uncached ([look page](../effects/look.md)).
- **Scaling x twice under AA 2X.** The torch spot becomes an oval on the right edge (VID-010; torch page).
- **The fault test in a player's build.** `SH4DER_FAULT_TEST` adds test hooks that fault or hang on purpose (fault.h, lines 15 and 72).

## Try this first

Our advice, not tested. It follows the README's spark section and gives a muzzle flash and a tracer.

1. Include `sh4der_spark.h` in the file that owns the gun. Link SH4ZAM as KallistiOS says.
2. At load, load a 64 x 64 atlas with no alpha (3 KB, VQ RGB565). Call `sh4der_spark_init(texture, 64, 64, format)`, then `sh4der_spark_reset(&pool)`.
3. On a shot: a KEEP billboard at the muzzle for 2 frames, a KEEP | RETREAT | TAIL0 streak to the hit for 4 frames, and a billboard plus `sh4der_spark_burst` at the hit. Life is in seconds: 2 frames is 0.033 s at 60 Hz (our arithmetic).
4. Each frame, `sh4der_spark_update(&pool, dt, gravity)`. In the translucent list, after the world, fill a view and a screen (`sh4der_spark_screen_make`), then `sh4der_spark_draw`.
5. You should see a muzzle flash for two frames and a streak that shrinks at its muzzle end. With the pool full of burst sparks, the flash and tracer stay and `dropped` counts the refused plain quads (spark.h, line 131). The stray-line check needs a real console (PVR-080). This draw as a pool: Run on a console by the owner in Match Light (her test; no measured numbers recorded yet).

## Related

- [Racing game: a recipe for the car, its shadow and its lights](./racing-game.md), the worked example this page follows.
- [What do you want to make?](../start-here/what-do-you-want.md); genre guides: [light and glow](../start-here/genre-light-and-glow.md), [shadows](../start-here/genre-shadows.md), [camera and screen](../start-here/genre-camera-and-screen.md), [tools and safety](../start-here/genre-tools-and-safety.md).
- [Frame budget](../limits/frame-budget.md), [recommended limits](../limits/recommended-limits.md), ledgers [PVR](../hardware/ledger-pvr.md), [SH-4](../hardware/ledger-sh4.md), [bus and DMA](../hardware/ledger-bus-dma.md).
- [Glossary](../glossary.md), [effect names](../effects/names.md), [licence and credit](../start-here/license-and-credit.md) (required when you use any of this).
