---
title: "Racing game: a recipe for the car, its shadow and its lights"
tags: [guide, racing, game-type, recipe, effect]
updated: 2026-11-26
sources: [demo/pl4nar/src/demo.c, demo/pl4nar/build.py, include/sh4der_pool.h, include/sh4der_pl4nar.h, include/sh4der_psm.h, include/sh4der_psm_pvr.h, include/sh4der_tilecut.h, include/sh4der_spark.h, README.md, AGENTS.md, wiki/limits/frame-budget.md, wiki/hardware/ledger-pvr.md, wiki/methods/suite-13.md]
---
# Racing game: a recipe for the car, its shadow and its lights

**What you will learn:** how to build one racing frame with this library: a car on a road, with its reflection in the paint and the wet tarmac, a shadow under the car, and street lamps and headlights. You will also learn what each part costs, what to turn off first when the frame is too slow, and what goes wrong.
**For:** a developer making a racing game on the Dreamcast who has never used SH4DER-PVR.

The guide follows one working example: the street driving demo in `demo/pl4nar`. Each value you see is taken from that demo's source, or from the library's header or wiki page, with its file and line. Where the code and the docs disagree, the guide says so at the end.

## The short version

- The effects to start with are the reflection on the paint and wet road, then light pools and headlights.
- Build the mirror at 256 x 128, with the street drawn upside down (ADVISED).
- The c7 build's day frame is 25.25 ms (MEASURED, PVR-174): it holds 30 Hz but is about 8.6 ms over 60 Hz (ESTIMATE).
- Cut the heat haze when it is on (ESTIMATE, 0.4 to 0.8 ms of SH-4), then halos and light pools, whose savings are not measured.
- The car programs are NOT RUN on a console as a set, and the sources disagree on the starting target size and the paint's F0.

## Read this first

- **Status tags.** Every number says where it comes from. **MEASURED** means a row in the [hardware ledger](../hardware/ledger-pvr.md) backs it, from one Dreamcast. **ADVISED** means a start setting from a header or the demo's source. **NOT RUN** means the car programs have not run on a console yet ([suite 13](../methods/suite-13.md)). **ESTIMATE** means arithmetic or a model. **DERIVED** means arithmetic on measured numbers, such as a difference or a ratio. **OPEN** means nobody has measured it yet. A measured number and an estimate are never mixed in one cell.
- **The car programs.** The car programs on the suite 13 disc have not run on a console as a set. Some earlier car builds have console rows in the ledger. Each section says which numbers come from those.
- **Headlights and lamps.** The library can fade a pool by its distance from the eye (a prototype, not run on a console), and the street demo fades its lamps by distance. Section 3 says what each does, and what is not built.
- **Credit.** A game that uses any of this library must credit it. See [licence and credit](../start-here/license-and-credit.md).
- **Words.** Terms are in the [glossary](../glossary.md). The ones this page uses most: **c5** and **c7** are two builds of the street car demo, in the ledger's order (c5 the older, about 110 ms a frame in PVR-160; c7 the later, about 25 ms in PVR-174). An **autorun** is the demo's scripted run through numbered **stages**, with no keys pressed; a key press marks a stage TOUCHED. The **HUD** is the on-screen readout. **SH-4 bound** means the main processor's time sets the frame. **TA KB** is the vertex data sent to the graphics chip for one scene.

## The five parts

1. Car shadows.
2. Reflections on the car and the wet road.
3. Headlights and street lamps.
4. A starter recipe for one racing frame, with its budget and what to turn off first.
5. When it looks wrong.

## 1. Car shadows

In the pl4nar car demo (`demo/pl4nar/`), each car's shadow is a **tile**: a small square of shadow picture, drawn for one car and laid on the ground under it. Terms are in the [glossary](../glossary.md).

**What a tile is.** The mirror pass draws the car's outline dark into a patch of the [reflection target](../glossary.md). A flat square on the ground then multiplies the road by that patch. The square is `TILE_M` = 10 m a side (ADVISED, client tuning: [`demo.c:41`](../../demo/pl4nar/src/demo.c)).

**How many cars get one.** `TILE_N` = 8 (`demo.c:40`) sizes the slot array. Slot 0 is always the player. The hills build (`-DPL4NAR_HILLS`) has two layouts (`demo.c:340-347`):

- **Big** (the default): four slots of 128 x 128 px. The player and three cars.
- **Small**: eight slots of 32 px at the 128 x 64 mirror. The player and seven cars. Chosen by `demo_auto_tile_set(1)` (`demo.c:362`) or `-DPL4NAR_TILES8`.

The plain map build (`build.py` without `--hills`) has neither. It draws up to eight tiles, nearest cars within 50 m (`demo.c:2012-2058`). Tiles are drawn by day only (`demo.c:1936`, `2012`).

**Tile size.** A bigger tile gives more texels on the same 10 m: 12.8 px a metre at 128 px, 3.2 px at 32 px (ESTIMATE, arithmetic). It costs more fill in the target pass. The ground square is the same 10 m whatever the size (`demo.c:2073`), so nothing on screen changes with it. The ledger times a target that holds 32 px hen tiles (PVR-156, a cut, not a tile cost); no row we found times a 128 px car tile. The 128 px size is ADVISED ([frame budget](../limits/frame-budget.md), section 6).

**Fade by distance, no pop (hills build).** These are the demo's own tuning, not library defaults. The library has no fade.

- `TILE_FADE_NEAR` = 40 m (`demo.c:52`): full strength inside it.
- `TILE_FADE_FAR` = 50 m (`demo.c:53`): none at or past it.
- `TILE_FADE_IN` = 0.5 s (`demo.c:54`): a newly given slot grows in over this.

Strength falls in a straight line from 40 to 50 m (`demo.c:1988`). A slot is given up only past 50 m (`demo.c:1960`), where strength is already 0, so no tile pops out. The map build has no fade: its tile is dropped the frame a car passes 50 m.

**Where the tiles live.** In the hills build the mirror sits at the target's top left. The tile region sits to its right: 256 x 256 px (`RT_TILES_W`, `RT_TILES_H`, `demo.c:48-49`). The width is 512 when the mirror is 256 px wide or less, else 1024. The height is at least 256 (`demo.c:50-51`). The 128 x 64 start gives 512 x 256, which is 256 KB (ESTIMATE, w x h x 2 bytes). `-DPL4NAR_HILLS` (`build.py --hills`) sets this.

**Cutting the pass to its tiles.** A target pass costs about 3.15 ms whatever its size (PVR-004: the times are MEASURED, one console). The cause, that the pass walks the screen's whole tile table, is an ESTIMATE (PVR-092 supports it; not proven). [`sh4der_tilecut`](../../include/sh4der_tilecut.h) ends the table after the target's own tiles for one pass, then restores it. Other programs, one console each: PVR-092, 3,312 to 375 us (20 tiles); PVR-205, 5,075 to 1,425 us, frame 1,869 us shorter.

The rules: wait for render done after every cut pass; never cut while a render reads the table; if the end check fails, restart the PVR. The demo's backend keeps them (`gfx_pvr.c:278-319`), and cuts only on autorun stages that ask (`demo.c:363`). Warning: with one of two tables cut, the hills build's frame got 3.05 ms slower (PVR-200, MEASURED). The refusal that prevents this has not run on a console.

**The cheap fallback: a blob.** A disc of 8 triangles at alpha 0.50 by day, down to 0.25 at night, is ADVISED: the header's note gives the street demo's values (`include/sh4der_pool.h:9`). The demo's blob is also 8 triangles (`demo.c:1829`, `1831`), at alpha 0.50 by day and 0.25 at night (`demo.c:2134`), compiled only with `-DPL4NAR_BLOBS` (`demo.c:2126`, `2133`). In an earlier hills build at 128 x 64, the frame averaged 17.85 ms with blobs and 18.63 ms with tiles laid on the ground (the ledger's "mesh" stage; frame budget section 6 calls it the shadow laid on the ground's triangles): +0.78 ms, a DERIVED difference (PVR-199, MEASURED, one run).

**Two shadow routes.** Route A: the chip draws the caster map. Route B: the main CPU draws a depth map in RAM ([psm routes](../shadows/psm-routes.md)). The car demo uses neither. Its tile copies route A's first step (dark casters into a target, then a multiply layer), but the casters are the car's two boxes slid along the sun, not a depth map. For scale only, the town gave route A PVR 15.9 ms (PVR-108) and route B SH-4 40.2 ms (PVR-112), one console each.

**Decision table.** The car programs have not run on a console (suite 13 pending), so car-tile costs here are NOT RUN unless a ledger row says otherwise. The sources give no frame-time budget, so the columns name layouts. Their order is an ESTIMATE from target pixels and cut size: at 128 x 64 the cut is 16 tiles (small) or 96 (big) (ESTIMATE, `demo.c:367-376`).

| Cars needing a shadow | Low: blobs | Medium: small layout | High: big layout |
|---|---|---|---|
| 1 (player) | Blob, `-DPL4NAR_BLOBS`. ADVISED (`sh4der_pool.h:8-9`). Cost NOT RUN. | One 32 px tile. Cost NOT RUN. | One 128 px tile. Cost NOT RUN. |
| About 4 | Blobs within 80 m (`demo.c:56`). ADVISED. Cost NOT RUN. | Four of eight slots, 32 px. Cost NOT RUN. | All four slots, 128 px. Cost NOT RUN. |
| About 8 | Blobs within 80 m. Cost NOT RUN. | All eight slots, 32 px. Cost NOT RUN. | Four tiles; the rest get blobs only with `-DPL4NAR_BLOBS` (`demo.c:2142-2145`). Cost NOT RUN. |

No recommended value yet says which column to pick at a frame time. The test: the car-hills run in [suite 13](../methods/suite-13.md), frame line read for each layout.

**Try this first.** Four client settings in `demo/pl4nar/src/demo.c`: `TILE_M` 10.0 m (`:41`), `TILE_FADE_NEAR` 40.0 m (`:52`), `TILE_FADE_FAR` 50.0 m (`:53`), `TILE_FADE_IN` 0.5 s (`:54`). The three fade settings are read only in the hills build. Each is ADVISED as the demo's chosen value ([frame budget, section 6](../limits/frame-budget.md#6-the-car-demo-practices-so-far)) and NOT RUN on a console for car tiles. Change one at a time. Film a car driving away from 60 m to 30 m with its tile on. Any pop is the fade's fault. No recommended value yet for any of the four.

## 2. Reflections on the car and the wet road

A **planar reflection** draws the world a second time, flipped about one flat plane (here, the road). The picture goes into a small **render target**, a texture the graphics chip draws into. The car's paint and glass look that picture up, one vertex at a time. A **Fresnel** share (more reflection at a shallow angle) sets how much shows. The effect is [`sh4der_pl4nar`](../../include/sh4der_pl4nar.h) ([effect page](../effects/pl4nar.md); [glossary](../glossary.md)).

### The steps

`AGENTS.md` ("Call order: pl4nar") has **seven** numbered steps:

1. At load, make the target, a layer header and a material.
2. Each frame, build the mirrored view before the screen is drawn. If it fails, skip the layer.
3. Run the mirror pass: draw the world at low detail, culling reversed.
4. Draw the opaque car and road as usual, and keep the car's screen vertices.
5. Look up each paint or glass vertex, and copy it with new texture coordinates and colour.
6. In the translucent list, send the layer's header, then its triangles.
7. On a size change or at exit, free the target. This waits for the graphics chip.

### Target sizes

Bytes are width x height x 2 (16 bits a pixel).

| Mirror size | Library target, with the render-to-texture flag | Car demo's target (map build) |
| --- | --- | --- |
| 128 x 64 | 16 KB (ESTIMATE, arithmetic) | 32 KB (MEASURED, PVR-161, one console) |
| 256 x 128 | 64 KB (ESTIMATE, arithmetic) | 128 KB (MEASURED, PVR-161, one console) |
| 512 x 256 | 256 KB (ESTIMATE, arithmetic) | 512 KB (MEASURED, PVR-161, one console) |

Without `-DSH4DER_PL4NAR_HAVE_RTT` the library's target is 1,024 pixels wide, so the same sizes cost 128, 256 and 512 KB (ESTIMATE, arithmetic; `include/sh4der_pl4nar.h:472-475`). So 512 x 256 needs that flag, on a KallistiOS with `pvr_scene_begin_rtt`. In the map build the demo's target is twice the mirror's width (`demo/pl4nar/src/demo.c:58`); the street build's is the mirror's width (`demo.c:65`).

The map and street builds start at 256 x 128 (index 0 of `s_sizes`, `demo.c:235, 244`). The hills and wet builds start at 128 x 64 (`demo.c:550, 559`), where the header and AGENTS.md say to start at 256 x 128. The wet build cycles 256 x 128, 128 x 64 and 64 x 32 (`demo.c:242`); it has no 512 x 256 size.

### Tuning, in order

Tune in this order: `f0`, `gain`, `ground_keep`, `reach`, then the field-of-view scale. AGENTS.md's engine section ("Tuning, in the order that matters") differs: it puts the target size and the world drawn first. See the last section for the disagreement.

| Setting | Default | What it does | Source | Status |
| --- | --- | --- | --- | --- |
| `f0` (`SH4DER_PL4NAR_F0_PAINT`) | 0.06 | Share seen head on | `include/sh4der_pl4nar.h:112` | ADVISED |
| `gain` | 1.0 (paint preset) | Scale on the layer's alpha | `include/sh4der_pl4nar.h:150` | ADVISED |
| `ground_keep` (`SH4DER_PL4NAR_GROUND_KEEP`) | 0.55 | Layer share kept where the reflected ray runs into the road | `include/sh4der_pl4nar.h:121` | ADVISED |
| `reach` (`SH4DER_PL4NAR_REACH`) | 18.0 in the street build; 9.0 in the map build (`demo.c:197`, `:194`) | How far a body's reflected ray is followed (world units; 1 m in the demo) | `include/sh4der_pl4nar.h:116` | ADVISED |
| field-of-view scale (`SH4DER_PL4NAR_FOV_SCALE`) | 1.55 body, 1 flat | Wider keeps more picture, costs sharpness | `include/sh4der_pl4nar.h:118` | ADVISED |

The first client's paint read best at 0.14 and its glass at 0.24 (README, "Materials"). The street build uses 0.14 for paint (`demo.c:239`) and 0.24 for glass (`demo.c:1443`, paint plus 0.10), with gain 1.0 by day and 2.0 at night (`demo.c:196-197`). The map build uses paint 0.25 (`demo.c:237`), gain 0.9 by day and 3.0 at night (`demo.c:193-194`). Client values judged on a host picture: ADVISED, NOT RUN on a console.

### What the mirror pass costs

Ledger rows are in the [PVR ledger](../hardware/ledger-pvr.md).

| Item | Number | Status |
| --- | --- | --- |
| Lookups, car body, build c7 | 0.59 to 0.65 ms | MEASURED (PVR-174, one console, one run) |
| Lookups, car body, build c5 | 0.92 to 0.97 ms | MEASURED (PVR-160, one console, one run) |
| Cost per looked-up vertex | about 2.0 us (320 vertices, count not checked on c7) | ESTIMATE, arithmetic |
| Frame by day, reflection on / off, c5 (stage 1 / off) | 110.17 ms / 69.23 ms | MEASURED (PVR-160, one console) |
| Frame cost of the reflection, c5 | about 41 ms | DERIVED, subtraction of the two MEASURED frames above |
| Mirror pass PVR, 256 x 128 | 3.23 ms (no key pressed) | MEASURED (PVR-174, one console) |
| Mirror pass PVR, 128 x 64 and 512 x 256 | 3.20 and 3.02 ms | MEASURED (PVR-175, one console), in touched stages (ten of thirteen were touched; the 3.20 is stage 13) |
| Mirror pass SH-4, c7 | not isolated | OPEN (PVR-174; `include/sh4der_pl4nar.h:13`) |
| Layer PVR, one car body | under 0.5 ms | ESTIMATE (README cost table) |

The flat 3 ms at every size is probably the target pass walking the screen's tile table (ESTIMATE; open as Q97 in [open questions](../open-questions.md)).

### What to cut first

The sources give three cuts, in this order (`README.md`, "What to turn down"): the target's size; the rate (`sh4der_pl4nar_due(frame, 2)` halves the mirror pass, at one frame's delay); and the triangles the mirror pass draws.

Two cautions. Size may save less than the list implies: the mirror's PVR time stayed near 3 ms at every size (PVR-175). The header advises cutting the mirror pass to its own tiles (`include/sh4der_pl4nar.h:15-16`), but with one of two tables found, that cut did not lower the mirror's PVR time and the frame rose 3.05 ms (PVR-200). The sources do not rank halos, light pools or the layer off ([frame budget, section 6](../limits/frame-budget.md#6-the-car-demo-practices-so-far)).

### Paint, glass and the wet road

Presets from the README "Materials" table, all ADVISED. Paint and glass were judged on a host picture; the rest are untested gains.

| Preset | F0 | Gain | Ground keep | Reach | Blend | Use (`include/sh4der_pl4nar.h:159-167`) |
| --- | --- | --- | --- | --- | --- | --- |
| `PAINT` | 0.06 | 1.0 | 0.55 | 18 | additive | clear coat on a body; keeps the paint's colour |
| `GLASS` | 0.10 | 1.0 | 0.55 | 18 | blended | a window on a body |
| `WET_ROAD` | 0.0204 | 0.7 | 1.0 | 0 (flat) | blended | a film of water on rough ground in the plane |

The wet demo overrides these: its road uses F0 0.15 (`demo.c:936`) and its own gain table (`demo.c:910`), not the preset's 0.7.

### The rain stages

Y cycles the rain: dry, damp, wet. The wet build is always at night (`controls.h:53`, `demo.c:164-165`). The **grit** is the speckled layer laid over the road. The **streak** stretches the reflection toward the horizon, so a lamp's image runs toward the viewer (1 is none). Each level sets these client values (`demo.c:910-916`, `demo.h:111-116`):

| Rain | Reflection share | Streak (1 = none) | Road tone kept | Grit alpha | Light pools |
| --- | --- | --- | --- | --- | --- |
| DRY | 0.0 (no lookup; the grit still draws) | 1.0 | 1.00 | 1.00 | 100 % |
| DAMP | 1.0 | 1.5 | 0.75 | 0.80 | 80 % |
| WET | 2.2 | 2.6 | 0.50 | 0.55 | 60 % |

Damp and wet draw a soft reflection pool round the car, fading 40 m behind and 70 m ahead (`demo.c:904-905`), on a 13 x 35 grid of flat lookups. They were set on the host, mostly by eye (`demo.c:910-916`, `demo.h:111`): ADVISED. The car-wet program is NOT RUN on a console ([suite 13](../methods/suite-13.md); PVR-214 is proposed), so no rain cost exists yet.

### Try this first

1. Target 128 x 64, the hills and wet builds' start (`demo.c:550, 559`). The header says start at 256 x 128 and go down (`include/sh4der_pl4nar.h:8`): ADVISED either way.
2. `f0` 0.06 for paint (`include/sh4der_pl4nar.h:112`), ADVISED. If the paint reads dead, raise it toward 0.14.
3. `gain` 1.0 (`include/sh4der_pl4nar.h:150`), ADVISED.
4. `ground_keep` 0.55 (`include/sh4der_pl4nar.h:121`), ADVISED.
5. `reach` 18.0 (`include/sh4der_pl4nar.h:116`), ADVISED.

To prove it, run the demo's size stages with no key pressed and time the PVR and SH-4 apart (`include/sh4der_pl4nar.h:14`).

## 3. Headlights and street lamps

A street lamp throws a pool of light on the road; a car's headlights throw a cone of light ahead. This section covers how the library draws them, what it fades, and what the driving demo (`demo/pl4nar`) does. Header: [`sh4der_pool.h`](../../include/sh4der_pool.h). Effect page: [light pools](../effects/pool.md). Line numbers are at commit e48edf3.

### Light pools, in plain words

A pool is a flat fan of triangles that lies on the road. A lamp gets a **disc**. A headlight gets a **wedge**, a cone of light on the road. You draw it on top of the road with **additive** blending: each pixel's colour is added to what is already there, so the pool only makes the road brighter. Terms are in the [glossary](../glossary.md).

### What the library fades, and what it does not

The header's fade runs across the fan. A disc's centre has full alpha and its rim has 0 (`sh4der_pool.h:32-33`). A wedge's apex has full alpha and its far arc has 0 (`:35-36`). The header says the light "dies with distance". That distance runs along the fan from its apex. It is not the distance from the camera.

The library fades a pool by its distance from the eye from commit 083d3f6. Before that commit, at e48edf3, it did not. A fade that softens a glow where it meets any shape (a camera depth map) is **not in the library**. A fade against the planes and rectangles you name is a separate header, `sh4der_softfade.h` (below).

### The distance fade (PROTOTYPE, not run on a console)

The library fades a pool by its distance from the eye. Its two settings come from the street demo's lamps:

- `SH4DER_POOL_REACH` `60.0f`: metres within which a lamp's pool is drawn. ADVISED (the demo's `LAMP_NEAR`).
- `SH4DER_POOL_FADE_W` `10.0f`: the pool fades to nothing over the last this many metres of the reach, so it does not pop in. ADVISED (the demo's `LAMP_FADE_W`).
- A beam for another car's headlight has `SH4DER_POOL_BEAM_ROWS` `3` arcs, at most `SH4DER_POOL_BEAM_ROWS_MAX` `4`. `SH4DER_POOL_BEAM_VTX_MAX` `69` is the most vertices a beam takes (exact arithmetic: 1 + 4 × 17).

The calls, in the order you use them:

1. `sh4der_pool_reach_make(&r, reach, width)` once a frame.
2. `sh4der_pool_reach_fade(&r, dist)` for each lamp. It returns 0 to 1. Multiply the pool's alpha and the halo's alpha by it, and skip the lamp when it is 0. `sh4der_pool_reach_fade2(&r, dist2)` does the same from a squared distance.
3. `sh4der_pool_reach_verts(&r, eye, vtx, nv)` for a fan's vertices, when each one needs its own fade.
4. `sh4der_pool_beam(apex, dir, side, length, half_angle, n, rows, rgb, alpha, reach_or_NULL, eye, out)` and `sh4der_pool_beam_tris(n, rows, idx)` for another car's headlight.

The player's own headlights stay `sh4der_pool_wedge`, with no reach: they are always near.

Status: PROTOTYPE v0.1 inside the pool header. Host tested (`test/pool_test.c`: the reach fade gives the same bits as the street demo's old lamp line on the plain C path). **NOT RUN on a console** as this code. Its cost is NOT MEASURED; the estimate is in the header's second RECOMMENDED block ([sh4der_pool.h](../../include/sh4der_pool.h)). The street demo's hills build calls `sh4der_pool_reach_fade` for its lamps. Its headlights start as plain wedges; the beam switch below turns on `sh4der_pool_beam`.

Do not call `sh4der_pool_reach_make` once for each lamp. Do not put a reach on the player's own headlights.

### The headlight beam switch (street build, NOT RUN on a console)

A car's headlight in the street demo starts as two plain wedges. The switch `-DPL4NAR_HEAD_BEAM` (or `demo_auto_beam(1)` in code) turns on `sh4der_pool_beam` for the beam: 2 across, 3 rows, with reach `LAMP_NEAR` (60 m) and fade width `LAMP_FADE_W` (10 m), measured from the eye. The spill wedge is scaled by the car's distance. The street demo's autorun now has 12 stages (84 s). Stage 11, "Headlights fade with distance", turns the beam on; stage 12 is the two plain wedges for comparison. In stage 11 a car driving away at 14 m/s is 50 m off at 4.6 s and 60 m off at 5.3 s. NOT RUN on a console.

The hills build keeps the long wedge with no fade along it; only its spill is scaled by the car's distance.

The host counts are exact counts, not console numbers. Over one stretch of road, a 210-frame night fly-by in the street build sent 251,468 triangles with the switch off and 252,022 with it on. The two autorun stages cover different stretches of road, so their counts are not a like-for-like comparison: with the fade, 264,908 triangles were sent (most in a frame: 1,351), without it 254,020 (most in a frame: 1,335). These are host numbers; NOT RUN on a console.

### Soft edges at planes and walls (not the depth fade)

`sh4der_softfade.h` fades a light per vertex near planes and rectangular walls that you name. It reads no depth, so it does not fade against a car, a person or a curved shape. Status: PROTOTYPE v0.1, host tested, NOT RUN on a console; cost NOT MEASURED for this header. Plain words and the calls are in [Soft edges for light](../effects/softfade.md).

The camera depth-map route, which softens a glow against any shape, is not in the library. A separate test program ran it on a console: the hard line went (PVR-204, MEASURED, the pictures), and the map cost 5.1 ms of PVR (PVR-203, MEASURED, one console; a different program's own code, not this library).

A wall-meeting glow was looked at in a spike, not in the library: `demo/psm/ao/d1_spike.c`, row [PVR-204](../hardware/ledger-pvr.md) (MEASURED: the pictures, one run, one console; the row gives no time).

### What the driving demo does (client code, not the library)

Only a build with both `PL4NAR_MAP` and `PL4NAR_HILLS` fades its lamps. `--hills` adds the second define ([build.py](../../demo/pl4nar/build.py), line 110) to the default map build (line 111). There a lamp's brightness is set by (`demo.c:1681`):

`const float lk = s_lamp * shz_saturatef((range - shz_sqrtf(dx * dx + dz * dz)) * (1.0f / LAMP_FADE_W));`

In plain words: `dx` and `dz` give the flat distance from the lamp to the car. A lamp is full until 10 m short of its range, then fades to nothing at the range edge. The other builds cut a lamp off at its range with no fade: the default map build by radius (`demo.c:1679`), the `--street` build by distance along z (`demo.c:1709`).

| Item | Exact value | Where | Status |
|---|---|---|---|
| Lamp range, screen | `LAMP_NEAR` `60.0f` | demo.c:124 | ADVISED |
| Lamp range, mirror | `LAMP_MIRROR` `40.0f`; `90.0f` in the wet build | demo.c:126, 128 | ADVISED |
| Fade width | `LAMP_FADE_W` `10.0f` (map and hills build) | demo.c:55 | ADVISED |
| Lamp pool | disc, 10 triangles, radius 7.5 m, alpha `0.60f` x brightness | demo.c:1662, 1688, 1712 | ADVISED |
| Headlight beam | wedge, `30.0f` m, half angle `0.11f`, 2 triangles, alpha `0.45f` x brightness | demo.c:1805, 1809 | ADVISED |
| Headlight spill | wedge, `20.0f` m, half angle `0.30f`, 4 triangles, alpha `0.22f` x brightness | demo.c:1806, 1810 | ADVISED |
| Headlight halo | head `0.55f`, tail `0.40f` (half size, m) | demo.c:1815, 1816 | ADVISED |

Each headlight is two wedges, a beam and a spill, drawn at each side of the car (`demo.c:1797-1810`). The headlights belong to the car, so their distance from the car never changes. With the demo's beam switch off (its default), the demo gives them no distance fade. The switch is described below.

### The halo

`halo()` (`demo.c:1624-1644`) draws one camera-facing square with `sh4der_spark_quad`, in the light's colour times its gain (`demo.c:1632`). Halos are not in the spark pool. Each one is a record on the stack (`demo.c:1625`), so the pool's 24-quad limit does not cap them. For the quad's cost model, see the [spark page](../effects/spark.md).

### The mirror pass

The [planar reflection](../effects/pl4nar.md) pass draws each lamp's halo at its mirrored place, not its pool (`demo.c:2531-2536`, `draw_lamps(2)`). Its range is `LAMP_MIRROR` (`demo.c:1661`). The comment says the lit lamp heads are "what sweeps over the paint at night" (`demo.c:2531`). Pools are drawn on the screen only (`demo.c:2566-2567`). Car lights are mirrored only in the wet build (`demo.c:2537-2538`, `wet_lights(1)`).

### Limit lamps by distance

ADVISED: "Limit light pools and halos by distance; their cost is translucent fill" ([recommended limits](../limits/recommended-limits.md), row 66; the header says the same, `sh4der_pool.h:14`). The demo applies `LAMP_NEAR`: a lamp past it is skipped and not drawn (`demo.c:1679`, `:1709`).

### Cost

- **Fill.** A pool or a halo costs what it covers on screen. A full-screen translucent layer at 640 × 480 adds about 3,112 µs of PVR time ([PVR-002](../hardware/ledger-pvr.md), MEASURED, one console; blend and additive cost the same). A pool's share is an ESTIMATE, a model: its share of the screen's area times that figure (linear fill).
- **Spread, do not stack.** 256 translucent triangles in one tile took 6,385 µs; spread over the screen, 82 µs ([PVR-009](../hardware/ledger-pvr.md), MEASURED, one console).
- **Halo.** 260 ns a vertex ([BUS-001](../hardware/ledger-bus-dma.md), MEASURED, one console). ESTIMATE, a model: 4 vertices at that cost, about 1.0 µs of SH-4 a halo, submission only.
- **Not measured.** No ledger row we found times a lamp pool, a headlight or this demo's halo on its own (OPEN). The whole mirror pass is 3.2 to 3.3 ms of PVR with the reflection on ([PVR-175](../hardware/ledger-pvr.md), MEASURED, one console; some stage names in that run do not match). The ledger's console time for a halo is in another program: 205 µs of SH-4 a frame for 129 vertices ([SH4-083](../hardware/ledger-sh4.md), MEASURED, one console).
- **Prove it.** ADVISED test (`sh4der_pool.h:13`): the same view with pools on and off, no key pressed, PVR time.

### Try this first

The car programs have not run on a console ([suite 13](../methods/suite-13.md), NOT RUN), so these values are untested there. They are the demo's own:

1. Lamps are night-only: `s_lamp` ramps up with night, and nothing draws at or under 0.02 (`demo.c:743`, `:1665`).
2. One 10-triangle disc a lamp, radius 7.5 m, alpha 0.60 x brightness, within `LAMP_NEAR` (60 m).
3. Two wedges a headlight, as in the table. Test each triangle with `sh4der_tri_area_ok` (`sh4der_pool.h:40-41`). Draw in the translucent list after the road ([AGENTS.md](../../AGENTS.md), "Call order: pool"). Run the pools-on and pools-off test under Cost before you tune.

The demo's lamp line at `demo.c:1681` is the same arithmetic as `sh4der_pool_reach_fade`; the host test checks that the two agree. Your own code can call the library instead of copying the line.

## 4. A starter recipe for one racing frame

**What is measured.** The car programs on suite 13 (car-tech, car-hills, car-wet) have not run on a console ([suite 13](../methods/suite-13.md)), so their numbers are NOT RUN. The console numbers here come from the street demo's build c7 (PVR-174, PVR-175), the map build that car-tech uses. The steps follow the demo's own calls, not a library example. Terms: [glossary](../glossary.md).

### The order of one frame

This is `demo_draw` in [demo.c](../../demo/pl4nar/src/demo.c), lines 2512 to 2654, map build.

1. Copy the heat window (`heat_copy`, 2545) before any scene opens.
2. Draw the mirror pass into a [render target](../glossary.md) (2572 to 2599): the street, upside down. It goes first: the screen's reflection layer reads the target, which must be finished before the screen scene starts (2606).
3. Draw the opaque street and the car body (2608 to 2613).
4. Compute each paint and glass vertex's place in the target (1447 to 1482).
5. Open the translucent list (2614). It blends with what is already drawn, so it comes after the solid world.
6. Draw the shadow tiles and road occlusion (2622; the tiles and the road occlusion are map build only, and the blob discs need `-DPL4NAR_BLOBS`).
7. Draw the light pools and headlight beams (2624 to 2625).
8. Draw the reflection layer: paint added, then glass blended (2626).
9. Draw the halos (2629 to 2630).
10. Draw the heat haze if on (2639 to 2646; a shimmer of hot air over the road's far end; off at start, line 273), then the hexagon heat if its band is on (2650; on only in a build with `PL4NAR_HEAT_HEX`, `demo.c:71-72`).
11. Draw the HUD and end the frame (2652 to 2653).

### What it costs

TA KB is the vertex data sent to the graphics chip for one scene.

| Item | Video memory | Work a frame | Time | Status |
|---|---|---|---|---|
| Mirror target, 128 × 64 | 32 KB | not counted | mirror PVR 3.20 ms | MEASURED, PVR-161, PVR-175 (keys pressed) |
| Mirror target, 256 × 128 | 128 KB | TA 15 KB mirror, 84 KB screen | mirror PVR 3.23 ms; frame 25.25 ms | MEASURED, PVR-161, PVR-174 (stage 1, no key) |
| Mirror target, 512 × 256 | 512 KB | not counted | mirror PVR 3.02 ms; 3.30 to 3.39 ms live | MEASURED, PVR-161, PVR-175, PVR-187 |
| Car body and lookups | none in video memory | 364 vertices, 320 looked up | lookup 0.64 ms | time MEASURED (PVR-174); count ESTIMATE (code, PVR-160) |
| Shadow tiles, 8 slots (map build) | none extra, inside the target (demo.c:58, 2017) | up to 8 casters, 2 triangles each (demo.c:40, 2112) | not measured | count MEASURED, at most 4 seen (PVR-175); time not measured |
| Shadow tiles, 4 slots (hills build) | 256 KB target (ESTIMATE, 512 × 256 × 2 B; demo.c:45 to 47) | 4 casters (demo.c:340) | +0.78 ms over blobs in one earlier hills run (PVR-199, DERIVED from MEASURED stage means) | car-hills NOT RUN; PVR-199 MEASURED (one run) |
| Light pools | none (untextured) | 10 triangles a lamp, 6 a headlight (demo.c:1690, 1809) | not measured (include/sh4der_pool.h:10) | count ESTIMATE; time not measured |
| Halos | not in these sources | 2 triangles each (demo.c:1642) | not measured | count ESTIMATE; time not measured |

The 32, 128 and 512 KB figures are the map build's target, twice the mirror's width (demo.c:58). The library's 256 × 128 target is 64 KB (ESTIMATE, arithmetic; include/sh4der_pl4nar.h:8).

### Frame time at 60 and 30 Hz

60 Hz is 16.7 ms a frame; 30 Hz is 33.3 ms ([budgets](../limits/budgets.md)).

| Part (c7 stage 1: day, 256 × 128, no key) | Time | Against 16.7 ms | Against 33.3 ms | Status |
|---|---|---|---|---|
| Whole frame | 25.25 ms | about 8.6 ms over (ESTIMATE) | holds | MEASURED, PVR-174 |
| SH-4 (CPU less PVR waits) | 21.25 ms | over | holds | MEASURED, PVR-174 |
| Car lookups (inside SH-4) | 0.64 ms | fits | fits | MEASURED, PVR-174 |
| Screen PVR (day: tiles in; lamps, pools and halos off, demo.c:743) | 7.45 ms | fits | fits | MEASURED, PVR-174 |
| Mirror PVR | 3.23 ms | fits | fits | MEASURED, PVR-174 |
| Tiles, pools, halos, heat alone | not measured | not measured | not measured | heat NOT RUN |

Total not known. The PVR pair sums to 10.68 ms (ESTIMATE). The frame is about 4 ms over the SH-4 time, so it is SH-4 bound ([frame budget, section 1](../limits/frame-budget.md)).

### What to turn off first

Least picture lost first. Each saving is tagged.

1. **Heat haze** (a shimmer of hot air over the road's far end). It starts off. On: ESTIMATE 0.4 to 0.8 ms SH-4 and 0.1 to 0.2 ms PVR (`demo/pl4nar/src/heat.h:5-8`, not run; [frame budget, section 6](../limits/frame-budget.md#6-the-car-demo-practices-so-far)). Lost: the far shimmer.
2. **Halos** (`draw_lamps(1)`, `draw_car_lights(1)`). Saving not measured. Lost: the lamp glow.
3. **Light pools** (`draw_lamps(0)`, `draw_car_lights(0)`). Saving not measured. Lost: light on the road.
4. **Mirror target size.** 128 KB to 32 KB saves 96 KB of video memory (DERIVED from the MEASURED free memory, PVR-161) and about 0.03 ms of mirror PVR (ESTIMATE, PVR-175). No frame time saved. Lost: sharpness. The [AGENTS](../../AGENTS.md) advice to go down first is about time; these numbers do not back it.
5. **Mirror detail and distance** (`LOD_MIRROR` 15, demo.c:34; `DRAW_BACK` 48, `DRAW_FWD` 336, demo.c:93, 96). The c5 to c7 changes together took the frame from 110 to 25 ms (PVR-160, PVR-174); no single share is measured. Lost: far buildings in the reflection.
6. **Shadow tiles** (`DEMO_BTN_TILES`, demo.c:613). Saving not measured: no run with tiles off. Lost: the car's shadow.
7. **The whole reflection.** No key turns it off in the current build: `controls.h` has no row for it, and `demo/pl4nar/README.md` (line 92) says the reflection is always on live. X was the reflection key in the build PVR-175 ran; now X is the heat. c5: 110 ms on, 69 ms off (PVR-160, one console, older build). About 41 ms of that is the mirror's SH-4 (DERIVED). Lost: the reflection.

### A recipe you can follow end to end

Use `MAP=1`, which sets `-DPL4NAR_MAP` (`demo/pl4nar/Makefile:34`). Without it, and without `-DPL4NAR_BLOBS`, the demo draws no car shadow (demo.c:2126 to 2132).

1. **Set up.** Use a current KallistiOS and the SH4ZAM maths library it carries. Include `sh4der_pl4nar.h`, `sh4der_pool.h` and `sh4der_spark.h`. You should see the road and car, nothing new.
2. **Plane and normals.** Set the plane `n = (0, 1, 0)`, `d = 0` (demo.c:314). Bake normals for the reflective parts with `tools/pl4nar_normals.py` ([AGENTS](../../AGENTS.md)).
3. **At load.** Call `sh4der_pl4nar_target_init(&target, 256, 128)`. A 0 means no video memory, so draw no layer. Then the paint and glass presets, and one layer header each: additive (1) for paint, blended (0) for glass (`demo/pl4nar/src/gfx_pvr.c:229` to 230). Check that `pvr_mem_available()` drops by the target's bytes.
4. **Each frame, before the screen.** Call `sh4der_pl4nar_view_build` with the plane, eye, target, field of view `0.96 × 1.55` and a fade of a quarter of the reach (demo.c:2557). A 0 means draw no layer.
5. **Mirror pass.** Open the target with culling reversed (`GFX_CULL_MIRROR`, demo.c:2580). Draw sky, buildings and props at low detail, and skip the car. The demo wraps its pass in its own `gfx_reflect_begin` and `gfx_reflect_end` (2572, 2599); `sh4der_pl4nar_pass` is the library's version. You should see the street upside down, horizon at the top. Upright means the mirror is missing; empty means the cull is the wrong way ([AGENTS](../../AGENTS.md)).
6. **Shadow casters.** Draw the cars' casters into the target's right half (`draw_tiles`, demo.c:2585). Day only (demo.c:2012).
7. **Screen.** Draw sky, road, buildings and props, then the car body opaque (demo.c:2608 to 2613, 1431).
8. **Lookups.** Call `sh4der_pl4nar_begin`, then `sh4der_pl4nar_shade` and `sh4der_pl4nar_argb` per paint and glass vertex. The map build's u is `gfx_reflect_u_scale()` halved, because the mirror is the left half (demo.c:1446 to 1470). Reload your own rows before any other transform (2620).
9. **Translucent list.** Open it (2557), then shadows (2565), pools and beams (2567 to 2568; [pool](../effects/pool.md)), the layer (2569), and halos (2572 to 2573; [spark](../effects/spark.md)). You should see a dark shadow under the car, the street in the paint, and lamp glow at night.
10. **Finish.** Draw the HUD, then `gfx_frame_end` (2595 to 2596).
11. **Check the frame runs.** The mirror is upside down, the car has a shadow, and you time the frame on your own console. The c7 build ran at 25.25 ms (PVR-174); yours will differ.

### Try this first

- **Mirror 256 × 128** (`s_sizes[0]`, demo.c:244). The map build's target for it is 512 × 128, 128 KB (demo.c:58; PVR-161, MEASURED). ADVISED for the library's own 256 × 128 target (include/sh4der_pl4nar.h RECOMMENDED block).
- **Body field of view 0.96 × 1.55** (`FOV_MAIN`, demo.c:107; `SH4DER_PL4NAR_FOV_SCALE` 1.55, include/sh4der_pl4nar.h:118). ADVISED.
- **Paint F0 0.25** in the map build (`s_f0`, demo.c:237). This is the owner's look, not a recommended value. Physical paint is 0.06 (include/sh4der_pl4nar.h:112).

## 5. When it looks wrong

Five common problems. Each gives the likely cause from the code and the pages, what to try, and what to check if that fails. Terms are in the [glossary](../glossary.md).

### Symptom: Shadow tiles pop on and off as the car moves

**What you see:** A car's shadow square appears or vanishes in one frame as the car drives past.

**Likely cause:** In the map build without hills, a car gets a tile while it is nearer than `TILE_NEAR` 50.0f ([demo.c:42](../../demo/pl4nar/src/demo.c), ADVISED), with no fade, so it pops at 50 m. The fade code (`TILE_FADE_NEAR` 40.0f, `TILE_FADE_FAR` 50.0f, [demo.c:52-53](../../demo/pl4nar/src/demo.c), ADVISED) runs only in the hills build ([demo.c:44](../../demo/pl4nar/src/demo.c)). Second lead, from code reading: a tile's grey can jump where `road_ao_at` reads open (1) more than 2.5 m from a sample ([demo.c:1917](../../demo/pl4nar/src/demo.c), ADVISED; [demo.c:2053-2054](../../demo/pl4nar/src/demo.c)).

**Try this:**
1. Build the hills map with `build.py --hills` ([build.py:19](../../demo/pl4nar/build.py)). It keeps each car's tile until its shadow has faded ([demo.c:1940-1988](../../demo/pl4nar/src/demo.c)).
2. If you must stay on the map build, port that keep-and-fade rule into the loop at [demo.c:2022-2030](../../demo/pl4nar/src/demo.c). Not tested.
3. Change the fade distances at [demo.c:52-54](../../demo/pl4nar/src/demo.c) if needed. They were chosen, not measured (frame budget, section 6, the "Shadow tiles" row, [frame-budget.md:178](../limits/frame-budget.md), ADVISED).

**If that is not it:** not known yet. Test: drive past the two fade marks, logging the tile count and each tile's grey every frame. A step in either is the pop.

### Symptom: Shadows are too dark, or too faint

**What you see:** A shadow is much darker or lighter than the light says it should be.

**Likely cause:** No single knob covers every route. On the tile route, the car is drawn in the grey `g`, 0.50f by day ([demo.c:1929](../../demo/pl4nar/src/demo.c), ADVISED). A blob takes its alpha as an argument, and [sh4der_pool.h:9](../../include/sh4der_pool.h) advises "about 0.4" (ADVISED). The shadow-map route has no strength setting: `sh4der_psm_raster_lit` returns 1 for lit and 0 for shadowed ([sh4der_psm_raster.h:372-375](../../include/sh4der_psm_raster.h)), so your code sets how dark that is.

**Try this:**
1. Tile route: lower the 0.50f at [demo.c:1929](../../demo/pl4nar/src/demo.c) for a darker shadow.
2. Blob, with `-DPL4NAR_BLOBS` ([demo README](../../demo/pl4nar/README.md), line 6): the demo uses 0.50 by day and 0.25 at night ([demo.c:2134](../../demo/pl4nar/src/demo.c), ADVISED). The header gives the same values (`sh4der_pool.h:9`), so there is no conflict to pick between.
3. Shadow-map route: change the bias, `SH4DER_PSM_BIAS_A` 0.02 m and `SH4DER_PSM_BIAS_B` 1.5 texels ([sh4der_psm.h:91-92](../../include/sh4der_psm.h), ADVISED). From the code, a smaller bias gives a darker shadow ([sh4der_psm_raster.h:369, 386](../../include/sh4der_psm_raster.h)). Not measured.

**If that is not it:** not known yet. Test: one flat caster on flat ground under one sun, the grey at the shadow's edge read at three bias values.

### Symptom: The reflection is upside down, or missing, or shows nothing

**What you see:** The mirror inset is the right way up, is empty, or the car shows no reflection.

**Likely cause:** A right-way-up picture means the plane's normal faces away from the camera ([AGENT_WORKFLOW.md:24](../../demo/pl4nar/AGENT_WORKFLOW.md)). That guide also lists a NULL plane; the build refuses one (see below), and `sh4der_pl4nar_viewproj_rows` gives an upright view for NULL only when the rows are built by hand ([sh4der_pl4nar.h:219](../../include/sh4der_pl4nar.h)). An empty picture means the cull is the wrong way round, since a mirrored world has reversed winding ([sh4der_pl4nar.h:285](../../include/sh4der_pl4nar.h)). A 0 from the target init or the view build draws no layer, with no error on screen ([sh4der_pl4nar.h:286-287, 487-488](../../include/sh4der_pl4nar.h)). `view_build` refuses a NULL plane ([sh4der_pl4nar.h:293](../../include/sh4der_pl4nar.h)), so a NULL shows as no layer.

**Try this:**
1. Check the plane. Its normal points toward the camera, and a point you know is on the ground gives dot(n, x) - d = 0 ([AGENT_WORKFLOW.md:9](../../demo/pl4nar/AGENT_WORKFLOW.md)).
2. Reverse the cull in the mirror pass ([sh4der_pl4nar.h:483-484](../../include/sh4der_pl4nar.h)).
3. Test each return: target init (487-488), view build (286-287), and `sh4der_poly_hdr_ok` on the layer header (505-509).
4. Without `SH4DER_PL4NAR_HAVE_RTT` the picture sits in the left part of a wider texture ([sh4der_pl4nar.h:474-475](../../include/sh4der_pl4nar.h)). Set the flag only where `pvr_scene_begin_rtt` exists ([README](../../README.md), "The pl4nar effect").

**If that is not it:** not known yet. Test: [AGENTS.md](../../AGENTS.md), the engine section's step 1 (line 182 at e48edf3; line 189 in the working tree). Show the raw target as a quad: horizon near the top, sky at the bottom. The demo's inset flips v on purpose ([demo.c:1780-1785](../../demo/pl4nar/src/demo.c)), so judge the raw target.

### Symptom: A headlight's light flickers, or covers the car

**What you see:** The beam shimmers as the camera moves, or it paints over the car body.

**Likely cause:** For flicker, the header asks for 2 to 4 cm of lift ([sh4der_pool.h:9](../../include/sh4der_pool.h), ADVISED); the demo uses 0.03 m for lamp pools ([demo.c:1686, 1711](../../demo/pl4nar/src/demo.c), ADVISED) and 0.035 m for the headlight apex ([demo.c:1801](../../demo/pl4nar/src/demo.c), ADVISED). A fan seen edge on has no area and drops out ([pool effect, Pitfalls](../effects/pool.md)). For the cover, the cause is not known yet. The beam is drawn after the body ([demo.c:2556-2568](../../demo/pl4nar/src/demo.c)), and the 0.55 m body halos come last ([demo.c:1815, 2573](../../demo/pl4nar/src/demo.c), ADVISED).

**Try this:**
1. Keep the lift in the 2 to 4 cm range, at [demo.c:1686](../../demo/pl4nar/src/demo.c), [1711](../../demo/pl4nar/src/demo.c) and [1801](../../demo/pl4nar/src/demo.c).
2. Turn the beam off (`draw_car_lights(0)`, [demo.c:2568](../../demo/pl4nar/src/demo.c)), then the halos (`draw_car_lights(1)`, [demo.c:2573](../../demo/pl4nar/src/demo.c)), one at a time. The one that stops the cover is the cause.
3. Check each fan's triangles with `sh4der_tri_area_ok` ([sh4der_pool.h:40-41](../../include/sh4der_pool.h)). The demo's own guard is at [gfx.c:100](../../demo/pl4nar/src/gfx.c).

**If that is not it:** not known yet. Test: freeze the view, turn the camera slowly, and count the flicker at two lifts inside the 2 to 4 cm range.

### Symptom: The frame rate drops when the road is wet

**What you see:** The frame misses its target with rain on, and holds it when the road is dry.

**Likely cause:** In rain, the wet build adds a grid of flat lookups each frame, plus a grit layer, rain streaks, and lamps out to 90 m ([demo.c:900-903, 909, 1764, 126](../../demo/pl4nar/src/demo.c), ADVISED). Which part costs most is not known: the wet pool has not run on a console ([frame-budget.md:174-175](../limits/frame-budget.md), NOT RUN). The mirror pass costs about 3.2 ms of PVR at any target size ([PVR-175](../hardware/ledger-pvr.md), MEASURED; most of its stages had a key pressed, so it is not a clean read).

**Try this:**
1. Read the frame first: is it SH-4 bound or PVR bound ([frame-budget.md:64-70](../limits/frame-budget.md)).
2. Turn the reflection off in your own build: `DEMO_BTN_REFLECT` ([demo.c:573](../../demo/pl4nar/src/demo.c)) is set by no key in the current controls (`controls.h` has no row for it). With it off, the wet grid skips its lookups ([demo.c:933](../../demo/pl4nar/src/demo.c)).
3. Start the target at 256 x 128 and go down ([sh4der_pl4nar.h:8](../../include/sh4der_pl4nar.h), ADVISED). PVR time did not change with size in PVR-175, so size is a weak lever.
4. The demo cuts the mirror pass only in the hills build ([demo.c:2512-2517](../../demo/pl4nar/src/demo.c)). A cut needs both tables ([sh4der_tilecut.h:54-56](../../include/sh4der_tilecut.h)): one table of two cost 3.05 ms ([PVR-200](../hardware/ledger-pvr.md), MEASURED).
5. Fewer lookups: `WET_COLS` 13u, `WET_BACK` 14u, `WET_FWD` 20u ([demo.c:900-902](../../demo/pl4nar/src/demo.c), ADVISED). A design change, listed in the [demo README](../../demo/pl4nar/README.md), lines 114-115.

**If that is not it:** not known yet. Test: the suite 13 pair "wet 128x64" against "dry 128x64" ([frame-budget.md:174](../limits/frame-budget.md)). No stage turns the grit off alone ([frame-budget.md:175](../limits/frame-budget.md)).

The car programs have not run on a console (suite 13 pending), so these fixes come from the code and the pages, not from a console run.


## Where the sources disagree

- **Tile layouts and the fade (conflict a).** [frame-budget.md:178](../limits/frame-budget.md) gives four 128 px tiles, faded from 40 m, as the car default; the row's practice text does not name the hills build, though its evidence comes from it. The code has those layouts and the fade only in the hills build (`demo.c:44-60`, `340-347`, `355`, `1960`, `1988`); the map build has eight tiles, no fade and a 50 m cut (`demo.c:2012-2058`). The guide follows the code. DISPUTED: the frame budget row needs "hills build".
- **Start size (conflict g).** [include/sh4der_pl4nar.h:8](../../include/sh4der_pl4nar.h) and AGENTS.md say start at 256 x 128; `demo.c:550, 559` start the wet and hills builds at 128 x 64 (the comment at 559 gives the owner's choice). The guide gives both. OPEN: no run compares the two starts.
- **Paint F0 (conflict h).** [include/sh4der_pl4nar.h:112](../../include/sh4der_pl4nar.h) and the README's pl4nar table say 0.06 (physical). `demo.c:239` uses 0.14 in the street build and `demo.c:237` 0.25 in the map build; README line 1018 says 0.14 read best in the first client, as a stylised look. The guide gives 0.06 as the preset and the demo values as the demo's. OPEN: no console run says which looks right.
- **Cut and tuning order (conflict j).** [demo/pl4nar/README.md](../../demo/pl4nar/README.md) "What to turn down" (size, rate, triangles) is a cost order; AGENTS.md "Tuning, in the order that matters" (size, world, f0, reach, field of view) is a tuning order; AGENTS.md's engine section, step 4, tunes f0, then gain, then ground_keep (the order this guide's tuning list follows). PVR-175 (size left the mirror near 3.2 ms) and PVR-200 (a one-table tile cut added 3.05 ms) do not back size first. The guide gives the README's order and says the numbers do not back it. DISPUTED.

## Related

- [What do you want to make?](../start-here/what-do-you-want.md): the starter, for a picture you have in mind.
- [Demo guide](../start-here/demo-guide.md): the street driving demo and the car programs on the suite disc.
- Genre guides: [shadows and depth](../start-here/genre-shadows.md), [light and glow](../start-here/genre-light-and-glow.md), [surfaces and materials](../start-here/genre-surfaces-and-materials.md), [tools and safety](../start-here/genre-tools-and-safety.md).
- [Frame budget](../limits/frame-budget.md) and [recommended limits](../limits/recommended-limits.md).
- [Call orders](../start-here/call-orders.md): where each effect's calls are written, in AGENTS.md.
- [Licence and credit](../start-here/license-and-credit.md): required when you ship any of this.
