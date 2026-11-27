---
title: "Fighting and sports arena: a recipe for the fighters, the hits and the stadium lights"
tags: [guide, fighting, sports, arena, game-type, recipe, effect]
updated: 2026-11-26
sources: [wiki/guides/racing-game.md, wiki/effects/names.md, wiki/start-here/call-orders.md, wiki/limits/frame-budget.md, wiki/limits/recommended-limits.md, wiki/limits/budgets.md, wiki/status/maturity.md, wiki/hardware/ledger-pvr.md, wiki/hardware/ledger-sh4.md, wiki/hardware/ledger-bus-dma.md, wiki/effects/sss.md, wiki/effects/spark.md, wiki/effects/lens.md, wiki/effects/warp.md, wiki/effects/pool.md, wiki/effects/aobake.md, wiki/effects/look.md, wiki/effects/trail.md, wiki/effects/focus.md, wiki/effects/taa.md, wiki/effects/split.md, wiki/effects/tilecut.md, wiki/effects/psm.md, wiki/effects/blade.md, wiki/effects/bloom.md, wiki/effects/frame.md, wiki/effects/guard.md, include/sh4der_sss.h, include/sh4der_spark.h, include/sh4der_lens.h, include/sh4der_warp.h, include/sh4der_pool.h, include/sh4der_aobake.h, include/sh4der_look.h, include/sh4der_trail.h, include/sh4der_focus.h, include/sh4der_guard.h, include/sh4der_tilecut.h, README.md (look, warp, lens, spark, pool tuning tables; Recommended settings), AGENTS.md (call orders: spark, pool, sss, look; Where it looks best on the PVR), demo/psm/ao/ao4_lamp.h]
---
# Fighting and sports arena: a recipe for the fighters, the hits and the stadium lights

**What you will learn:** how to build one frame of a fight or a stadium match: skin under arena light, hit sparks and a flash, a shadow under each fighter, a soft replay. Each part's cost, and what to cut first.
**For:** a developer making a fighting or sports game on the Dreamcast who has never used SH4DER-PVR.

No source here is about fighting or sports; the effects are general. This page says where the evidence stops.

**Status tags.** **MEASURED**: a ledger row backs the number (its ID is given), from one console. **ADVISED**: a start value from a header, the README or demo source (file and line given). **no figures**: the owner has run that code on a console; no number is recorded. **ESTIMATE**: arithmetic or a model, named. A host count is an exact PC count, not a console number. Terms: [glossary](../glossary.md).

## The short version

- Start with sparks and the screen flash for hits, the two parts the recipe below builds.
- Build the spark pool at load with 24 sparks (ADVISED) on a 64 x 64 atlas, and test each quad's area before it is sent.
- The budget is 16.7 ms at 60 Hz, and one full-screen layer alone costs 3.1 ms of PVR (MEASURED, PVR-002).
- Cut order: the shockwave (saving not measured), then the grade's layers (3.1 ms of PVR each, MEASURED, PVR-002), then light pools.
- Skin glow has no figures, and the sources disagree on the blade's and bloom's version labels.

## What this kind of game needs

- **Skin and cloth up close.** They must look right at short range.
- **Bright lights, dark stands.** Brightness must follow the scene.
- **A fast answer to a hit.** Sparks, a flash, a hit direction and, on a big hit, a shock.
- **A static arena.** Seats and corners never move, so their shading can be baked at load.
- **Replays.** A sharp fighter on a soft background.

Two fighters are two skinned bodies. One skinned hero of 1,465 vertices took 0.81 ms of skin on one console (SH4-079, MEASURED).

## Effects that fit, and why

| Effect | What it gives this game | Where | Status |
|---|---|---|---|
| Light through thin parts [sss] | Ears and fingers glow with light from behind; from the front it gives 0 | [header](../../include/sh4der_sss.h), [page](../effects/sss.md) | ALPHA v0.3 (lamp shade, SH4-083); no skin run |
| Exposure and grade [look] | Brightness follows the lights and the stands | [header](../../include/sh4der_look.h), [page](../effects/look.md) | PROTOTYPE v0.1; no row |
| Sparks and tracers [spark] | A burst on a hit; a flash that is never pushed out | [header](../../include/sh4der_spark.h), [page](../effects/spark.md) | PROTOTYPE v0.1 |
| Screen overlays [lens] | Hit flash, hit-direction wedges, vignette | [header](../../include/sh4der_lens.h), [page](../effects/lens.md) | PROTOTYPE v0.1; no row |
| Screen warp [warp] | Shockwave on a big hit; works on a 320 × 240 picture | [header](../../include/sh4der_warp.h), [page](../effects/warp.md) | ALPHA v0.3 (send times, BUS-004) |
| Light pools [pool] | Blob shadow under each fighter; light on the floor | [header](../../include/sh4der_pool.h), [page](../effects/pool.md) | ALPHA v0.2; distance fade PROTOTYPE |
| Corner darkening [aobake] | Seats and corners, baked at load | [header](../../include/sh4der_aobake.h), [page](../effects/aobake.md) | ALPHA v0.5 (suite copy); this header ADVISED |
| Shadow tile [psm_pvr], with [tilecut] | A small shadow picture under each nearest fighter (your target pass) | [header](../../include/sh4der_psm_pvr.h), [page](../effects/psm.md), [tile cut](../effects/tilecut.md) | psm ALPHA v0.5 (town); tile measured for the hens |
| Mesh bloom [bloom] | Glow round stadium lamp heads | [header](../../include/sh4der_bloom.h), [page](../effects/bloom.md) | Rings ALPHA v0.3 (maturity.md; its evidence is the lamp demo's halo loop, SH4-083) |
| Fake focus blur [focus] | Sharp fighters, soft background | [header](../../include/sh4der_focus.h), [page](../effects/focus.md) | PROTOTYPE v0.1; no program uses it |
| Glowing tubes [blade] | A power weapon's glow | [header](../../include/sh4der_blade.h), [page](../effects/blade.md) | PROTOTYPE v0.1 (maturity.md; the header says v1.00, see below) |
| Texture ripple [uvwarp] | A crowd banner or flag | [header](../../include/sh4der_uvwarp.h), [page](../effects/uvwarp.md) | PROTOTYPE v0.1; no program uses it |
| Split screen [split] | Replay comparison, divider in the middle | [header](../../include/sh4der_split.h), [page](../effects/split.md) | ALPHA v0.2; a moving divider is OPEN |
| Motion blur [trail] | Speed in a replay | [header](../../include/sh4der_trail.h), [page](../effects/trail.md) | PROTOTYPE v0.1; our advice: too dear live (ESTIMATE, below) |
| Smooth edges over time [taa] | Our advice: still replay shots, not a live fight | [header](../../include/sh4der_taa.h), [page](../effects/taa.md) | PROTOTYPE v0.1 |

Left out for now: screen bloom [glow] (no measured cost), soft edges [softfade] (no figures), dust specks [motes] (run by the owner in Match Light, her test; no measured numbers recorded yet; released in the public Match Light film), and a sun shadow map (a stadium light is a lamp).

**Source conflicts.** blade.h:34 and bloom.h:31 say "Version: v1.00", and blade.md and bloom.md add "used in a released game"; maturity.md gives the blade PROTOTYPE v0.1 and the bloom rings ALPHA v0.3. The sources disagree; this page keeps the maturity labels. look.h:36 and taa.h:39 say "Version: v0.50", and look.md and taa.md say they run on the demo disc; maturity.md gives look and taa PROTOTYPE v0.1 with no ledger row. The sources disagree; this page keeps the maturity label.

## The order of one frame

1. **At load,** once: `sh4der_spark_init`, `sh4der_spark_reset`, `sh4der_lens_init`, the look's load calls (`sh4der_look_bins`, `sh4der_look_params_default`, `sh4der_look_eye_reset`), and `sh4der_sss_init` for each skin material. Bake the arena. Check each return: where the header says 0 means refused, draw nothing for that effect ([call orders](../start-here/call-orders.md)).
2. **Read the screen.** The look's eye and grade come from a read of the picture on screen, so read before this frame's layers (our advice, not tested). That picture is one or two frames old ([frame read](../effects/frame.md)).
3. **Step and shade.** Update the sparks, the flash and the wedges with this frame's time. Put `sh4der_sss_rgb` into each skin vertex's offset colour before the send (AGENTS.md, sss, step 4).
4. **Target passes early.** Draw a shadow tile's casters into its small target before the screen scene, wait for it to finish, and cut it to its tiles ([tile cut](../effects/tilecut.md)). As the racing guide's mirror does. Poll the wait if you wait every frame: a sleeping wait can run 10 ms late ([PVR-117](../hardware/ledger-pvr.md)).
5. **Opaque list.** The arena and both fighters.
6. **Translucent list, after the world.** Light pools, shadow tiles, lamp glow and sparks (AGENTS.md, spark, step 4: after the world). The order among them is our advice, not tested. Test each triangle with `sh4der_tri_area_ok`.
7. **Screen layers, under the HUD.** The look's grade layers; the focus mesh after the frame ([focus](../effects/focus.md)); the lens overlays, which sort under the HUD ([lens](../effects/lens.md)). The sources give no slot for warp or trail; read their headers before placing them.
8. **HUD, then the stamp.** The frame read's stamp is drawn last, over everything ([frame read](../effects/frame.md)).

## What it costs

60 Hz is 16.7 ms a frame; 30 Hz is 33.3 ms. Brackets show each time as a share of both. Overlapped SH-4 and PVR cost about the larger; run in turn, about their sum ([frame budget](../limits/frame-budget.md), section 1). No whole-game frame is measured here.

| Part | Video memory | Triangles or vertices | Frame time (16.7 / 33.3 ms) | Status |
|---|---|---|---|---|
| Empty 640 × 480 frame | none | none | 3.15 ms PVR (19 % / 9 %) | MEASURED, PVR-001 |
| One full-screen translucent layer | none | one quad | 3.1 ms PVR (19 % / 9 %) | MEASURED, PVR-002 |
| Look's screen read | none | 4,800 uncached loads | about 2.5 ms SH-4 (15 % / 8 %) | ESTIMATE: 9,600 bytes at about 3.8 MB/s, a rate DERIVED from BUS-025 and SH4-041 |
| Trail, 3 copies | 128 B | 8 full-screen quads | about 25 ms PVR (150 % / 75 %) | ESTIMATE: 8 times PVR-002 |
| Shockwave mesh, 40 × 30 | 256 KB (arithmetic) | 2,460 vertices | 2.25 ms SH-4 send (13 % / 7 %) | MEASURED, BUS-004. PVR render, about 6.5 ms, is in the bench notes (REPORTED) |
| Sparks, 24 quads | 1,256 B RAM, 3 KB atlas | at most 96 vertices | 14 to 25 µs SH-4 (under 0.2 %) | ESTIMATE: 96 vertices at 142 to 260 ns (BUS-003, BUS-001) |
| Bloom rings, a lamp's glow | none | 129 vertices, 240 triangles (the lamp demo's halo) | 205 µs SH-4 (1.2 % / 0.6 %), for that demo loop only (SH4-083) | MEASURED for the lamp demo's loop, which writes an older fade out by hand; the library call: Run on a console by the owner in Lamp with a lit shade (her test; no measured numbers recorded yet) |
| The lamp demo's shade loop (calls sss_rgb and normalises) | 40 B RAM a material | per vertex | 187 µs for 65 vertices (SH4-083); about 2.9 µs a vertex (DERIVED from that loop) | MEASURED for the lamp demo's loop only; sss: Run on a console by the owner in Lamp with a lit shade; Sunset sea (her test; no measured numbers recorded yet); skin no figures |
| Skin, two fighters | none | 2 × 1,465 (a reference body) | about 1.6 ms SH-4 (10 % / 5 %) | ESTIMATE: 2 × 1,465 × 553 ns (553 ns DERIVED from SH4-079) |
| Baked corner shading | none | 2,142 and 2,544 | load 252 to 254 ms and 391 to 393 ms; frame +69 to +277 µs SH-4 | MEASURED, SH4-076, SH4-077, SH4-082 |
| Shadow tile, 128 × 128 target | 32 KB (arithmetic) | casters, not counted | 3,256 µs uncut, 344 µs cut (hens' 32 px tiles) | MEASURED, PVR-156; a 128 px fighter tile is not measured |
| Light pools | none | 8 to 10 triangles a disc | not measured | OPEN ([pool](../effects/pool.md)) |

A shockwave's PVR render and one full-screen layer each take a large share of 16.7 ms.

## Settings to start with

- **Light through thin parts [sss]:** in `sh4der_sss_init`, absorb {0.5, 1.8, 3.0}, wrap 0.5, distortion 0.3, gain 0.6; the path is a baked thickness times 2.0 (AGENTS.md). Invented starting values: ADVISED.
- **Exposure and grade [look]:** `SH4DER_LOOK_LIGHT_S` 2.0 and `SH4DER_LOOK_DARK_S` 6.0 seconds (README, "Look tuning"). Keep the gain between 0.4 and 2.0 (README, "Recommended settings"). ADVISED, by eye.
- **Sparks [spark]:** `SH4DER_SPARK_MAX` 24, `SH4DER_SPARK_MIN_PX` 0.8, `SH4DER_SPARK_INSET` 0.0078125 for a 64 × 64 atlas (spark header). ADVISED. Burst size and life: no recommended value yet.
- **Flash and wedges [lens]:** `SH4DER_LENS_FLASH_PEAK` 0.35, `SH4DER_LENS_FLASH_ATTACK_S` 0.03, `SH4DER_LENS_FLASH_DECAY_S` 0.25, `SH4DER_LENS_HIT_MAX` 4 (README, "Lens tuning"). ADVISED; no figures.
- **Shockwave [warp]:** `SH4DER_WARP_RING_SPEED` 180.0, `SH4DER_WARP_RING_WIDTH` 14.0, `SH4DER_WARP_RING_AMP` 5.0 (README, "Warp tuning"). Coarse grid: `SH4DER_WARP_GRID_X` 20 by `SH4DER_WARP_GRID_Y` 15 (not measured; the bench timed 40 × 30).
- **Light pools [pool]:** `SH4DER_POOL_REACH` 60 m and `SH4DER_POOL_FADE_W` 10 m (the fade: Run on a console by the owner in Street car demo (her test; no measured numbers recorded yet)). A blob of 8 triangles, alpha 0.50 by day and 0.25 at night, lifted 2 to 4 cm (pool header; the same values in AGENTS.md, "Call order: pool", step 4).
- **Corner shading [aobake]:** `SH4DER_AOBAKE_RAYS` 64, `SH4DER_AOBAKE_REACH` 3.0, `SH4DER_AOBAKE_PUSH` 0.02 (aobake header, RECOMMENDED). Bake at load. ADVISED for this copy.
- **Shadow tile size:** 128 px for a person, 32 px for a small caster ([frame budget](../limits/frame-budget.md), "The advised default look"). ADVISED. The 128 px fighter tile is not measured.

## What to cut first

1. **Shockwave:** ring on big hits, on the coarse grid. You lose some edge smoothness (our advice, not tested). Saving: not measured.
2. **Read grid:** lower `SH4DER_LOOK_GRID_X` and `SH4DER_LOOK_GRID_Y` from 80 and 60. You lose exposure accuracy (focus shares the grid). Saving: ESTIMATE, by loads.
3. **Grade:** use fewer lines. A neutral grade draws no layer. Each layer you drop saves 3.1 ms of PVR (PVR-002).
4. **Light pools:** a shorter `SH4DER_POOL_REACH`, or a disc in place of a wedge. You lose far light.
5. **Skin glow on the far fighter:** turn sss off for that body. Saving: not measured. The 0.81 ms in SH4-079 is skinning, which sss does not change.
6. **Sparks:** a smaller `SH4DER_SPARK_MAX`. Saving under 25 µs (ESTIMATE), so cut this last.
7. **Shadow tile:** a blob disc in its place. You lose the shape of the shadow.

## Common mistakes

- **Front light on skin.** Sss gives 0 when lit from the front. Use a rim light or a lamp behind the fighter (AGENTS.md, "Where it looks best").
- **No visibility term.** Light then passes through skin in shadow ([sss](../effects/sss.md), Pitfalls).
- **Thin ears on big triangles.** The glow smears. Give them vertices of their own ([sss](../effects/sss.md)).
- **Quads sent without the area test.** One corridor run dropped 21 and 38 zero-area quads (PVR-182, MEASURED, one console) ([send checks](../effects/guard.md)).
- **Reach made once per lamp.** Call `sh4der_pool_reach_make` once a frame ([pool](../effects/pool.md), Pitfalls).
- **A full vertex buffer.** It drops the end of the list, HUD included: one town view lost its HUD and inset for 20 s (PVR-185, MEASURED, one run).
- **A sleeping render-done wait every frame.** It can run 10 ms late (PVR-117).
- **Look gain outside 0.4 to 2.0.** RGB565 bands past that ([look](../effects/look.md)).

## Try this first

1. **Include** `sh4der_spark.h`, `sh4der_lens.h` and `sh4der_guard.h` (for `sh4der_tri_area_ok`). Credit the library in your game ([licence and credit](../start-here/license-and-credit.md)).
2. **At load,** load the spark atlas (a grid of cells, no alpha). Call `sh4der_spark_init(texture, w, h, txr_format)`, `sh4der_spark_reset(&pool)` and `sh4der_lens_init()`. Check the returns: a 0 from `sh4der_spark_init` means a NULL atlas, so draw no sparks.
3. **On a hit,** call `sh4der_spark_burst` with the bounced shot as `dir`; `sh4der_spark_billboard(..., SH4DER_SPARK_KEEP)` for the flash spot; `sh4der_lens_flash_hit(&flash, peak)`; and `sh4der_lens_hit_add(slots, SH4DER_LENS_HIT_MAX, right, fwd)`. Size, speed, life and colour: no recommended value yet.
4. **Each frame,** call `sh4der_spark_update`, `sh4der_lens_flash_step` and `sh4der_lens_hit_tick`. In the translucent list after the world, call `sh4der_spark_draw`. After the scene and before the HUD, call `sh4der_lens_flash_draw` and `sh4der_lens_hits_draw`.
5. **What you should see:** sparks that fall and fade; a blue-white flash that fades (decay constant 0.25 s); a red wedge on the attacker's edge, gone after 1.5 s. Time the frame with the flash on and off: it is one full-screen layer, 3.1 ms of PVR (PVR-002). No figures for these headers together (both PROTOTYPE v0.1).

## Related

- [Racing game: a recipe for the car, its shadow and its lights](./racing-game.md): the worked example.
- [What do you want to make?](../start-here/what-do-you-want.md), and the genre guides: [camera and screen](../start-here/genre-camera-and-screen.md), [light and glow](../start-here/genre-light-and-glow.md), [surfaces](../start-here/genre-surfaces-and-materials.md).
- [Frame budget](../limits/frame-budget.md), [recommended limits](../limits/recommended-limits.md), [budgets](../limits/budgets.md), [call orders](../start-here/call-orders.md).
- [Licence and credit](../start-here/license-and-credit.md): required when you ship any of this.
