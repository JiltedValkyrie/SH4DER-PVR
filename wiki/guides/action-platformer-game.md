---
title: "Third-person action and platformer: a recipe for the hero, the world and the glow"
tags: [guide, action, platformer, game-type, recipe, effect]
updated: 2026-11-26
sources: [wiki/guides/racing-game.md, wiki/start-here/call-orders.md, wiki/effects/names.md, wiki/limits/frame-budget.md, wiki/limits/recommended-limits.md, wiki/status/maturity.md, wiki/effects/blade.md, wiki/effects/bloom.md, wiki/effects/holo.md, wiki/effects/uvwarp.md, wiki/effects/sss.md, wiki/effects/motes.md, wiki/effects/rise.md, wiki/effects/shafts.md, wiki/effects/pool.md, wiki/effects/aobake.md, wiki/effects/psm.md, wiki/effects/look.md, wiki/effects/glow.md, wiki/effects/spark.md, wiki/effects/frame.md, wiki/effects/trail.md, wiki/effects/water.md, wiki/effects/torch.md, wiki/start-here/genre-light-and-glow.md, wiki/start-here/genre-surfaces-and-materials.md, wiki/hardware/ledger-pvr.md, wiki/hardware/ledger-sh4.md, wiki/hardware/ledger-bus-dma.md, wiki/hardware/ledger-video.md, AGENTS.md, README.md, include/sh4der_pool.h, include/sh4der_guard.h, include/sh4der_frame.h, include/sh4der_glow.h, include/sh4der_shafts.h, include/sh4der_look.h, include/sh4der_aobake.h, include/sh4der_spark.h, include/sh4der_psm.h, include/sh4der_bloom.h, include/sh4der_blade.h, include/sh4der_holo.h, include/sh4der_motes.h, include/sh4der_rise.h, include/sh4der_uvwarp.h, include/sh4der_sss.h, demo/psm/hero/README.md, demo/psm/ao/ao4_lamp.h]
---
# Third-person action and platformer: a recipe for the hero, the world and the glow

**What you will learn:** how to build one frame of a third-person action or platformer game: a hero with a shadow, glowing items, and rooms with corner shading. You will see what each part costs, what to cut first, and where the sources stop.

**For:** a developer making a game like this on the Dreamcast who has never used SH4DER-PVR.

No program in the ledger runs this whole recipe. Each number names its part.

## The short version

- Start with the hero's blob shadow from a light pool, then baked corners for rooms.
- Build one untextured translucent header for the pool: source alpha blending, depth test on, depth write off, no culling.
- The hero demo's every pass totals 18.76 ms (MEASURED, PVR-178): over 60 Hz at 16.7 ms, inside 30 Hz at 33.3 ms.
- Cut order: glow's ray copies, specks and shafts, then the shadow target for blobs (about 3.2 ms of PVR uncut, MEASURED, PVR-004).
- The glowing tubes, bloom rings, inner light, texture ripple and thin-part glow are NOT RUN, and the sources disagree on several version labels.

## What this kind of game needs

Your hero is a **skinned** body: a mesh that bends over bones. He is opaque and needs a shadow. Glowing items, animals, plants and water sit beside him. The camera follows him, so the picture changes every frame. Rooms and ruins stay still, so their corner shading can be baked at load.

Tags: **MEASURED** = a [ledger](../hardware/ledger-pvr.md) row, one console. **ADVISED** = a header, README table or demo source, cited. **NOT RUN** = no console run. **ESTIMATE** = a named model. **DERIVED** = arithmetic on measured numbers.

A frame has 16.7 ms at 60 Hz and 33.3 ms at 30 Hz ([frame budget](../limits/frame-budget.md), section 1). The SH-4 is the main processor and the PVR the graphics chip ([glossary](../glossary.md)).

## Effects that fit, and why

| Effect | What it gives this game | Where | Status |
|---|---|---|---|
| Glowing tubes [blade] | a glowing sword or power rod | [header](../../include/sh4der_blade.h), [page](../effects/blade.md) | NOT RUN |
| Mesh bloom [bloom] | a glow round a blade tip or lamp, no frame read | [header](../../include/sh4der_bloom.h), [page](../effects/bloom.md) | NOT RUN |
| Inner light [holo] | a breathing glow inside a crystal or spirit | [header](../../include/sh4der_holo.h), [page](../effects/holo.md) | NOT RUN |
| Texture ripple [uvwarp] | a flag or force field on a strip | [header](../../include/sh4der_uvwarp.h), [page](../effects/uvwarp.md) | NOT RUN |
| Light through thin parts [sss] | leaves and ears glow with the sun behind | [header](../../include/sh4der_sss.h), [page](../effects/sss.md) | NOT RUN |
| Drifting specks [motes] | pollen and dust near the hero | [header](../../include/sh4der_motes.h), [page](../effects/motes.md) | host tests only |
| Rising lights [rise] | lanterns that lift off and settle as stars | [header](../../include/sh4der_rise.h), [page](../effects/rise.md) | host tests only |
| Dark shafts of light [shafts] | sun shapes through trees | [header](../../include/sh4der_shafts.h), [page](../effects/shafts.md) | PROTOTYPE; demo disc (REPORTED) |
| Light pools [pool] | the hero's blob shadow; lamp light | [header](../../include/sh4der_pool.h), [page](../effects/pool.md) | disc ALPHA; fade PROTOTYPE |
| Baked corner shading [aobake] | contact shadow in rooms, paid at load | [header](../../include/sh4der_aobake.h), [page](../effects/aobake.md) | load time MEASURED |
| Sun shadow map [psm] | the sun's shadow of hero and ground | [header](../../include/sh4der_psm.h), [page](../effects/psm.md) | ALPHA |
| Exposure and grade [look] | the picture adapts indoors and out | [header](../../include/sh4der_look.h), [page](../effects/look.md) | PROTOTYPE; demo disc (REPORTED) |
| Screen bloom [glow] | lamps and sun bleed into the picture | [header](../../include/sh4der_glow.h), [page](../effects/glow.md) | PROTOTYPE; cost ESTIMATE |
| Sparks and tracers [spark] | hit sparks, dust, lamp halos | [header](../../include/sh4der_spark.h), [page](../effects/spark.md) | PROTOTYPE |

**Left out.** Smooth edges [taa] and Motion blur [trail] blend full-screen history layers. Two trail copies make six layers, about 19 ms of PVR (ESTIMATE from [PVR-002](../hardware/ledger-pvr.md)). Our advice, not tested: leave both out at 60 Hz. Corner darkening [ao] has no user yet. Planar reflection [pl4nar] is costed in the [racing guide](./racing-game.md). Water [water] and the torch [torch] have no ledger rows.

**Source conflicts.** blade.h:34, bloom.h:31 and holo.h:31 say "Version: v1.00" (their effect pages add "used in a released game"); maturity.md gives the blade and holo PROTOTYPE v0.1 and the bloom rings ALPHA v0.3 (from the lamp demo's halo loop). The sources disagree. This page keeps NOT RUN, because no ledger row times these calls. glow.h:58 and shafts.h:34 say "Version: v0.50" (their effect pages say they run on the demo disc); glow.h:17 and maturity.md give PROTOTYPE v0.1. The sources disagree. sss.h:39 says v0.50 and maturity.md gives ALPHA v0.3 from the lamp demo's shade loop; this page keeps NOT RUN for sss on a body.

## The order of one frame

A **translucent list** is the set of see-through shapes the chip blends over the picture. A **render target** is a texture the chip draws into.

1. **Read last frame's picture (SH-4 work).** The look's eye step, the glow's upload and the shafts' map use the frame on screen, one or two frames old ([frame read](../effects/frame.md); look.h:25).
2. **Per-frame maths.** Blade and holo `prepare`, the sss light direction, and the pool, spark, motes and rise calls ([call orders](../start-here/call-orders.md)).
3. **A shadow target, if you use one.** Draw it and finish it before the screen scene, since the screen reads it. Our advice, not tested.
4. **Opaque ground, props, then the hero.** His skin runs on the SH-4 before the send (0.81 ms, [SH4-079](../hardware/ledger-sh4.md)).
5. **Motes, if masked,** go in a punch-through list, which draws or skips pixels by a mask ([motes](../effects/motes.md)).
6. **The translucent list, after the world.** Blob shadow, light pools (AGENTS, "Call order: pool"), the glowing tubes and their mesh bloom, then sparks (AGENTS, "Call order: spark", step 4) and rise lights ([rise](../effects/rise.md)).
7. **Dark shafts, look layers, then glow.** The shafts header's demo uses z 425, 435 and 447 (shafts.h:47-48). Look layers are full screen, under the HUD (AGENTS, "Call order: look", step 3).
8. **The HUD.** Anything that must not glow sits under the threshold, or is drawn after the glow (glow.h:84-85).
9. **Stamp, then read.** `sh4der_frame_stamp_draw` goes last, then `sh4der_frame_read` (frame.h:28-32). Take gain and glow off the read (`sh4der_frame_degain`, `sh4der_glow_unfeed`; glow.h:81).

The sources do not place the punch-through list (step 5), or the blade against the pools (step 6). Those are our choices, not tested.

## What it costs

| Part | Video memory | Triangles or vertices | Frame time | Status |
|---|---|---|---|---|
| Full-screen translucent layer, 640 × 480 | 0 | 4 vertices | +3.1 ms each ([PVR-002](../hardware/ledger-pvr.md): "about 3,112 us") | MEASURED, one console |
| Render-target pass, uncut | its bytes | whole tile table | about 3.2 ms PVR ([PVR-004](../hardware/ledger-pvr.md)); 344 to 375 us cut ([PVR-092](../hardware/ledger-pvr.md), [PVR-156](../hardware/ledger-pvr.md)) | MEASURED, one console |
| Hero, opaque (hero demo) | 0 | 1,465 vertices, 1,660 triangles | +3.28 ms SH-4, +0.30 ms PVR ([PVR-178](../hardware/ledger-pvr.md)) | MEASURED, one console, three runs |
| Hero's shadow on a target (hero demo) | not in the row | +2,594 vertices | +2.08 ms SH-4, +5.05 ms PVR ([PVR-178](../hardware/ledger-pvr.md)) | MEASURED, one console, three runs |
| Blob shadow (pool disc) | 0 | 9 vertices, 8 triangles (exact) | not measured; ESTIMATE: screen share × 3.1 ms | ADVISED; no run |
| Frame read, 80 × 60 | 0 | 4,800 uncached reads ([look](../effects/look.md)) | not measured here; 1,196 us in another port ([VID-005](../hardware/ledger-video.md)) | REPORTED |
| Grade | 0 | 0 to 6 full-screen quads | ESTIMATE: layers × 3.1 ms | ESTIMATE |
| Screen bloom | 16 KB texture, plus a 1 KB one | 3 layers, up to 4 ray copies | ESTIMATE: 9.3 ms PVR (3 × PVR-002), plus rays | ESTIMATE |
| Dark shafts | 16 KB texture | 4,800 cells; one 8 × 6 mesh | not measured | PROTOTYPE; no row |
| Baked corner shading | a byte a vertex in RAM | per vertex | load: 252 to 254 ms for 2,142 vertices ([SH4-076](../hardware/ledger-sh4.md)); draw: 32 to 44 ns ([SH4-077](../hardware/ledger-sh4.md), DERIVED) | MEASURED (load) |
| Glowing tubes, mesh bloom, inner light, ripple, thin-part glow, specks, rising lights, sparks | 0 (ripple table 2 KB) | bloom 24 vertices; motes 384 (exact) | not measured; sparks ESTIMATE 25 us SH-4 (96 × 260 ns, [BUS-001](../hardware/ledger-bus-dma.md)) | NOT RUN, host tests or no row |
| Sun shadow map | a 128 KB map in the town | per caster | not measured for a hero; town routes MEASURED (PVR-108 to PVR-113) | ALPHA |

One full-screen layer is 19 % of 16.7 ms and 9 % of 33.3 ms (DERIVED from PVR-002). The hero demo's every pass together is 18.76 ms (PVR-178): over 60 Hz, inside 30 Hz.

**An attribution to know.** The maturity page and two genre pages credit the lamp program's 205 µs halo and 187 µs shade ([SH4-083](../hardware/ledger-sh4.md)) to bloom and sss. Those are the lamp demo's own code (`demo/psm/ao/ao4_lamp.h`), so this page does not use them.

## Settings to start with

**Glowing tubes**
- `SH4DER_BLADE_PERIOD` 192, `SH4DER_BLADE_PULSES` 2 (README:241-262). ADVISED, NOT RUN.
- Give each blade its own phase, 57 times its object's index (README:229).

**Mesh bloom**
- `SH4DER_BLOOM_TIP` 0.90, `SH4DER_BLOOM_DISC_RADIUS` 0.14 (bloom.h:73,77). ADVISED, NOT RUN.
- Ring glows for a lamp or sun: no value yet.

**Inner light**
- `SH4DER_HOLO_BASE` 0.60, `SH4DER_HOLO_AMP` 0.40, `SH4DER_HOLO_BODY` 0.62 (holo.h:61-63). ADVISED.
- The breath needs a chest, pelvis and forward direction (AGENTS, holo). A crystal: no value yet.

**Texture ripple**
- A 64 × 16 table, 2 KB; `SH4DER_UVWARP_SIDE_MAX` 256 (uvwarp.h:73). ADVISED.
- Fade the weight to 0 at both ends. Scroll speed: no value yet.

**Light through thin parts**
- Leaf: absorption 1.2, 0.3, 1.8; wrap 0.8; distortion 0.2 (AGENTS, "Starting points per material"). ADVISED: invented values.
- Pass a shadow `visibility` term (AGENTS, sss step 3).

**Drifting specks**
- `max_draw` 96 by default (motes.h:125), 128 at most (motes.h:61). ADVISED.
- Up to 256 slots an emitter (README motes table). ADVISED.

**Rising lights**
- `SH4DER_RISE_TWINKLE_IN` 1.0 s (rise.h:69). ADVISED.
- Rise speed: no value yet.

**Dark shafts of light**
- Strength 0.50, off by default, range 0.25 to 3.00 (shafts.h:44-46). ADVISED.
- `SH4DER_SHAFTS_NEAR` 0.25, `SH4DER_SHAFTS_FAR` 0.9 (README shafts table). ADVISED.

**Light pools**
- Blob: 8 triangles, centre lifted 2 to 4 cm (pool.h, RECOMMENDED block). ADVISED.
- Blob alpha: AGENTS.md and the pool header both give 0.50 by day and 0.25 at night, the street demo's values. No console run has tested the blob's alpha on a platformer.
- `SH4DER_POOL_REACH` 60 m and `SH4DER_POOL_FADE_W` 10 m are the street demo's. PROTOTYPE, NOT RUN.

**Baked corner shading**
- `SH4DER_AOBAKE_RAYS` 64, `SH4DER_AOBAKE_REACH` 3.0, `SH4DER_AOBAKE_PUSH` 0.02 (aobake.h:64-66). ADVISED for this copy.
- Bake once at load, never in a frame ([aobake](../effects/aobake.md)).

**Sun shadow map**
- `SH4DER_PSM_BIAS_A` 0.02 m, `SH4DER_PSM_BIAS_B` 1.5 texels (psm.h:91-92). ADVISED, not measured.
- A distance and a fade for your level: no value yet.

**Exposure and grade**
- `SH4DER_LOOK_GAIN_HI` 1.5, `SH4DER_LOOK_LIGHT_S` 2.0, `SH4DER_LOOK_DARK_S` 6.0 (README look table). ADVISED, by the author's eye.
- Keep the gain between 0.4 and 2.0 ([recommended limits](../limits/recommended-limits.md)). ADVISED.

**Screen bloom**
- `SH4DER_GLOW_THRESHOLD` 0.62, `SH4DER_GLOW_POWER` 2, `SH4DER_GLOW_STRENGTH` 0.45 (glow.h:113-115). ADVISED. The sources disagree: glow.h:113 gives 0.62, and glow.h:67-71 recommends 0.80 for the author's game.
- Never draw the darken without the bloom (glow.h RECOMMENDED block). ADVISED.

**Sparks and tracers**
- `SH4DER_SPARK_MAX` 24, `SH4DER_SPARK_MIN_PX` 0.8, `SH4DER_SPARK_INSET` 0.0078125 (spark.h:11-12). ADVISED.
- `SH4DER_SPARK_KEEP` goes to what must never be pushed out, such as a flash or a tracer (spark.h:17).

## What to cut first

The least picture lost comes first.

1. **Glow's ray copies** (`SH4DER_GLOW_RAYS`, glow.h:118). You lose the sun streaks. Saving not measured.
2. **Fewer specks.** Lower `max_draw` below 96. Saving not measured.
3. **Shafts off.** You lose the sun shapes. Saving not measured.
4. **The hero's detail overlay**, if you have one: +1.44 ms SH-4 and +1.09 ms PVR in the hero demo (PVR-178).
5. **Shadow target to blobs.** You save the target's pass, about 3.2 ms of PVR uncut (PVR-004); the hero demo's whole shadow was +5.05 ms (PVR-178). You lose the shadow's shape.
6. **Grade layers off.** You save 3.1 ms a layer (PVR-002). You lose the grade.
7. **Screen bloom off.** You save an ESTIMATED 9.3 ms of PVR. You lose the glow.
8. **Shorter pool reach.** You lose far pools. Saving not measured.

The glowing tubes, inner light and thin-part glow have no measured row, so cut them last and measure them first.

## Common mistakes

- Drawing a pool before the ground, or writing depth from it (pool.h, RECOMMENDED block).
- Calling `sh4der_pool_reach_make` for each lamp. Call it once a frame ([pool](../effects/pool.md), Pitfalls).
- Piling translucent quads into one tile: 256 in one tile took 6,385 us, spread out 82 us ([PVR-009](../hardware/ledger-pvr.md)).
- An unaligned 4-byte write stops a console ([VID-009](../hardware/ledger-video.md)). A triangle with no screen area draws bright lines ([PVR-080](../hardware/ledger-pvr.md)).
- Baking inside a frame. A static sun bake took 152 ms on a console ([SH4-070](../hardware/ledger-sh4.md)).
- A sleeping `pvr_wait_render_done` wakes up to 10 ms late. Poll it where the wait runs every frame ([PVR-117](../hardware/ledger-pvr.md)).
- Route A, where the PVR draws the shadow map, has no self-shadowing ([psm](../effects/psm.md), Pitfalls).
- An emulator can read the frame black. Judge the read on a console ([shafts](../effects/shafts.md), Pitfalls).

## Try this first

1. **Include and test.** Include `sh4der_pool.h` and `sh4der_guard.h`, and link SH4ZAM on the Dreamcast ([setup](../start-here/setup.md)). Run `make test` on the host. Credit SH4DER-PVR as [the licence](../start-here/license-and-credit.md) says.
2. **At load,** make one untextured translucent header: blend source alpha with one minus source alpha, depth test on, depth write off, no culling (AGENTS, "Call order: pool", step 1).
3. **Each frame, after the ground,** call `sh4der_pool_disc` with the centre under the hero's feet, lifted 2 to 4 cm. The ground axes are the radii, `n` is 8, the colour is 0 and the alpha 0.50 by day, 0.25 at night (AGENTS.md, "Call order: pool", step 4; ADVISED). It writes 9 vertices. No radius is recommended yet.
4. **Send it.** Transform with your own matrix; triangle k is 0, k + 1, (k + 1) % 8 + 1. Test each in view space with `sh4der_tri_area_ok` (guard.h:55-70) before you send it.
5. **You should see** a soft dark patch under the hero, darkest at the centre. If it flickers, lift it more ([light and glow](../start-here/genre-light-and-glow.md)).

## Related

- [Racing game recipe](./racing-game.md), the worked example this page follows in form.
- [What do you want to make?](../start-here/what-do-you-want.md), [call orders](../start-here/call-orders.md), [effect index](../effects/README.md).
- Genre guides: [light and glow](../start-here/genre-light-and-glow.md), [surfaces](../start-here/genre-surfaces-and-materials.md), [shadows](../start-here/genre-shadows.md), [camera](../start-here/genre-camera-and-screen.md).
- [Frame budget](../limits/frame-budget.md), [budgets](../limits/budgets.md), [recommended limits](../limits/recommended-limits.md).
- [Licence and credit](../start-here/license-and-credit.md): required when you ship any of this.
