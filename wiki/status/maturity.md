---
title: "Maturity: how far along each part is"
tags: [status, maturity, index, start]
updated: 2026-11-25
sources: [wiki/hardware/ledger-pvr.md, wiki/hardware/ledger-sh4.md, wiki/hardware/ledger-bus-dma.md, wiki/hardware/ledger-video.md, wiki/effects/README.md, README.md effect table, include/*.h version lines, AGENTS.md Files, third_party/sh4zam-dce/README.md]
---
# Maturity: how far along each part is

**What you will learn:** for every part of SH4DER-PVR (each effect, the SH4ZAM-DCE kernels, the demos and the suite's test tools), how mature it is, what that rests on, and what would move it up.
**For:** anyone deciding what to build on, and agents checking how much proof stands behind a part before they use or change it.

A label here comes from the [hardware ledger](../hardware/README.md) alone: what has run on a real Dreamcast and been written down as a ledger row. Host tests, an emulator, or a claim in a source comment do not raise a label. Old version numbers in `README.md` and the headers are not repeated here; they are listed under [Old version claims, undecided](#old-version-claims-undecided).

## The scale

| Label | Versions | Rule (proposed; the owner can change it) |
|---|---|---|
| **PROTOTYPE** | v0.1 | Host-tested, or run on a console without a MEASURED ledger row of its own. Behaviour or API may change at any time. A part with no console run at all is PROTOTYPE whatever its test count. |
| **ALPHA** | v0.2 to v0.5 | Run on a console with a MEASURED result and a ledger row. The API may still change. Inside ALPHA: **v0.2** one run, or the part's cost not isolated from a larger program; **v0.3** one run with rows of its own; **v0.4** repeated (two or more runs or programs, the same console); **v0.5** repeated and used by a client, held back only by an OPEN item or a known defect. |
| **BETA** | v0.6 to v0.9 | Measured on a console, used by a client, no known open defect in its call order, tests cover it. |
| **STABLE** | v1.0 and up | Repeated on more than one console, tuning documented, nothing OPEN in its ledger rows. |

Two caps: a part is never marked above its weakest claim, so one OPEN step caps it at ALPHA; and every console result so far is from **one console** (Holly 2.42, PVR-123), so nothing can be STABLE yet. "A client" is a program other than the part's own test that calls it as a library (a demo or a suite program). All dates are 2026-11-25 unless a row says otherwise.

## Counts

| Label | Rows |
|---|---|
| STABLE | 0 |
| BETA | 1 |
| ALPHA | 26 |
| PROTOTYPE | 36 |

After the re-rank from s8, mip-a and corr-a2 with Parity's second read (below). Two rows keep two versions inside ALPHA; no label is disputed. Five rows rest on the written client rule (a demo or a suite program counts), which the owner may change (below).

## The library: effects and shared parts

Every header and source in `AGENTS.md`'s Files list. Where one header holds parts with different evidence, it has a row per part.

| Feature | What it is | Label | Version | Evidence | What would promote it | What it needs from a console |
|---|---|---|---|---|---|---|
| core | Shared parts: the SH4ZAM switch and helpers, the feather, tube and blocker shapes, the breath tracker, the glint | PROTOTYPE | v0.1 | Host tests (`core_test`). Its helpers run inside every console spike since c4, but no ledger row times or checks core itself. | A row of its own | The breath tracker and glint timed in a spike; the feather on screen |
| blade | Light tubes, a blocker and a pulse in vertex colours for a glowing blade | PROTOTYPE | v0.1 | Host tests (`blade_test`, `example/blade.c` on the host). No console row. | A console run of the example with a measured cost | `sh4der_blade_prepare` and the per-vertex colour, us a vertex |
| holo | An inner light in a translucent body that follows its breathing | PROTOTYPE | v0.1 | Host tests (`holo_test`). No program on the console uses it. | A client on the console | The per-vertex colour cost; the look |
| bloom: blade glow | The feathered ribbon and tip disc round a blade (`sh4der_bloom_blade`) | PROTOTYPE | v0.1 | Host only. No console row. | A console run beside the blade | Its 24 triangles' fill cost |
| bloom: rings | A round glow with a level per ring (`sh4der_bloom_rings`) | ALPHA | v0.3 | ao-lamp's halo, suite s7: SH-4 205 us for 129 vertices, timed alone (SH4-083), 240 triangles a half (PVR-157); the look reported good (PVR-158). MEASURED, 1 console, 1 run. | A second run; a client outside the suite | The rings' PVR time alone |
| water | Waves, ripple normals, Fresnel, reflected direction, crest transmission | PROTOTYPE | v0.1 | The ocean demo runs on a real Dreamcast (`demo/OCEAN.md:5`, 2026-11-18): REPORTED, no ledger row. Its scene ratios are host plain C. | A MEASURED row of the ocean's frame | The ocean's frame time; the BLOOM row's cost |
| look | Eye adaptation, a grade as full-screen layers, fog glare | PROTOTYPE | v0.1 | On the demo disc; no ledger row. | A console row | The eye step and each layer's fill |
| frame | The frame read back on a grid; a stamp on every frame | PROTOTYPE | v0.1 | The bus rates it rests on are MEASURED (SH4-040, SH4-041). The 80 × 60 read's 1,196 us is from an earlier port's console log: REPORTED (VID-005). Not timed in our runs. | The read timed on our console | The 80 × 60 read, us |
| taa | Jitter, history weight, pan shift for temporal AA | PROTOTYPE | v0.1 | Host tests; on the demo disc; no row. | A console row | The blend's cost and the look |
| glow | Screen bloom from a small bright map, halo, darken layer, god rays | PROTOTYPE | v0.1 | Host tests; on the demo disc and in the ocean; no row. | A console row | The map read and each layer's cost |
| warp | The frame through a mesh: shockwave, haze, wobble, blur, pixelate, dither | ALPHA | v0.3 | h2, the draw calls timed alone: the pixelate pass 241 ns a vertex in store-queue runs against 366; the mesh 6 % slower in runs, why OPEN (BUS-004). MEASURED, 1 console, 1 run. | The mesh's slowdown explained; the effect on screen in a client | A whole warp frame's cost |
| trail | The frame before over this one: motion blur and the TAA blend | PROTOTYPE | v0.1 | Host tests. The demo disc includes its header but never calls it. The stride-register limit is FACT from the code (VID-003). No row. | A console row | The two passes' cost; the front-buffer copy |
| lens | Vignette, film grain, hit wedges, flash | PROTOTYPE | v0.1 | Host tests; in matchlight and the torch walk; no row. | A console row | Fill cost of each overlay |
| focus | Fake depth of field through a vertex-alpha mask | PROTOTYPE | v0.1 | Host tests. No program uses it. | A client and a console row | The blurred map and mask cost |
| visor | Night, amber and thermal visor looks | PROTOTYPE | v0.1 | Host tests. No program uses it. (ledger "visor" rows are the hero's visor pass, not this effect.) | A client and a console row | Each look's layers' cost |
| torch | A flashlight carved on screen, fog bent to black | PROTOTYPE | v0.1 | On the demo disc and in the torch walk; AA 2X scaling is a design FACT (VID-010). No row. | A console row | The radial mesh's cost; the fog table load |
| pvrx | A layer over KallistiOS's PVR driver: polled waits, PVR-DMA download, TA error counts, Holly revision, user clip with its dummy | PROTOTYPE | v0.1 | Host test (`pvrx_test`). The sequences it lifts are MEASURED in their own programs (PVR-117, PVR-136, BUS-020 to BUS-053), but no function has run on a console as this layer. | The pvrx-check suite entry | Each function once, as the layer |
| guard | Drops zero-area triangles and never-compiled headers | ALPHA | v0.3 | **corr-a2 counts it acting: `sh4der_tri_area_ok` dropped 21 and 38 zero-area quads in two stages, 0 in nine (PVR-182, MEASURED, 1 run; the two stages TOUCHED).** Both faults seen on a real console (PVR-080, PVR-081: FACT from `README.md:1304-1305`, no run of ours). In every console program since the town and the street demo; pl4m c5 "headers refused 0" (PVR-161, MEASURED, s7): a zero on a run with the guard live, which does not show it acting. 1 console. | A second run that counts its drops; the header guard seen refusing | A header refused on the console |
| bump | A normal map lit by blend passes, no bump unit (`sh4der_bump.h`) | PROTOTYPE | v0.1 | The torch walk shows a cost label on the console (`effects/bump.md`), but no ledger row. No host test in `test/` either. OPEN (AGENTS.md): the azimuth's turn and the 565 palette's banding. (The many "bump" ledger rows are the c4 bump-unit chain, not this header.) | A ledger row from the torch walk | The passes' cost; the two OPEN looks |
| sss | Back-lit transmission per vertex | ALPHA | v0.3 | ao-lamp's shade, s7: SH-4 187 us for 65 vertices, timed alone (SH4-083), the look (PVR-158). MEASURED, 1 console, 1 run. In the ocean: REPORTED. | A second run; the shade's cost alone in PVR | Per-vertex cost at a larger count |
| uvwarp | A per-vertex texture offset from a small scrolling table | PROTOTYPE | v0.1 | Host tests. No program uses it. | A client and a console row | us a vertex |
| spark | A pool of 24 additive camera-facing quads | PROTOTYPE | v0.1 | The street demo's halos call `sh4der_spark_quad` and ran in pl4m c5's night stages (s7), but no row names it. | A row that isolates it | The pool's draw cost |
| bolt | Branching lightning strands that re-roll | PROTOTYPE | v0.1 | Host tests. No program uses it. | A client and a console row | 240 vertices' cost |
| motes | Stateless drifting specks | PROTOTYPE | v0.1 | Host tests; in matchlight (host). It rests on the punch-through rule, MEASURED (PVR-060). | A console row | The 96 sprites' cost |
| deform | Ground that keeps a dent | PROTOTYPE | v0.1 | Host tests; in matchlight (host). | A console row | The stamp's cost a frame |
| mist | A fog bank as upright translucent sheets | PROTOTYPE | v0.1 | Host tests; in matchlight (host). Fill not measured (AGENTS.md). | A console row | Fill cost of 12 sheets |
| rise | Lights that lift off and become stars | PROTOTYPE | v0.1 | Host tests; in matchlight (host). | A console row | Count against frame time |
| film | Old film wear: flicker, scratches, dust, weave | PROTOTYPE | v0.1 | Host tests; in matchlight (host). | A console row | The overlay's fill |
| cloudvolume | A ray-marched cloud deck into a small buffer | PROTOTYPE | v0.1 | Host tests; CPU numbers ESTIMATE (AGENTS.md). | A console row | The march's SH-4 ms |
| pl4nar | Planar reflection through a small render target | ALPHA | v0.5 | **pl4m c7, s8, a second run: lookups 0.59 to 0.65 ms; the mirror pass's PVR 3.2 to 3.3 ms, now MEASURED (the sampling fixed); the targets and video memory repeat c5's exactly (PVR-174, PVR-175).** pl4m c5, s7: lookups 0.92 to 0.97 ms; targets 128 × 64 to 512 × 256 (1,024 wide) with no NOVRAM; the split at the middle (PVR-160, PVR-161, MEASURED, 1 run). Its PVR columns are ESTIMATE, our bug (PVR-162). A 128 × 128 target renders right (PVR-020, s2). OPEN: the moving divider; the missing "largest scene" line again (Q93); the mirror pass's PVR the same at every target size (Q97). | Q97 and Q93 closed; a run with no key pressed | The mirror pass cut to its tiles |
| pool | Light pools and blob shadows as fading fans | ALPHA | v0.2 | pl4m c5's blob stage 114.71 ms against tiles 110.17 (PVR-160); its own cost not isolated. 1 run. | Its cost alone | Fill of one disc and one wedge |
| psm | Shadow map placement: uniform and perspective | ALPHA | v0.5 | The town in suite s1, s3, s6b and s7 (PVR-108 onward, PVR-148, PVR-166) and the earlier town builds; many runs, 1 console. The owner's own benchmarks: perspective runs slower than uniform. OPEN: the OPB events' cause (PVR-121, Q77). | The OPEN rows closed | A second console |
| psm_raster | Route B: the shadow map in RAM on the SH-4 | ALPHA | v0.5 | The town's route B: 40 to 44 ms of SH-4 in the town view (limits), PVR-112, PVR-113; the bake (SH4-070). `AGENTS.md` says nothing has run on a Dreamcast: out of date, the town calls `sh4der_psm_raster_begin`, `_tri_h`, `_lit_n` and `_lit` (`demo/psm/psm_scene.h:1138-1210`, `psm_mapsrc.h:96`). | As psm | As psm |
| psm_pvr | Route A: the PVR draws the map | ALPHA | v0.5 | Route A in every town run (PVR-108 onward); PARAM is a full vertex buffer (PVR-149). The same out-of-date line in `AGENTS.md`; the town calls the header's clip, `sh4der_psm_pvr_part_` (`demo/psm/psm_scene.h:1286-1287`), the step inside `sh4der_psm_pvr_tris`; `sh4der_psm_pvr_uv` runs only in the host demo. OPEN: PVR-121, Q77. | As psm | As psm |
| ao | Crease darkening for a bake, a screen cavity cue (`sh4der_ao.h`) | PROTOTYPE | v0.1 | Host tests. No program uses the header: the console AO work is the suite's own code (the "baked AO" row below). | A client and a console row | The cavity map's cost |
| ca | The frame texture and chromatic aberration | PROTOTYPE | v0.1 | On the demo disc and in matchlight; a render target renders right (PVR-020); the stride limit (VID-003, FACT). No row of its own. | A console row | The 640 KB target and the three copies' cost |
| shafts | Dark shapes cast away from a light | PROTOTYPE | v0.1 | On the demo disc; no row. | A console row | The map read and the mesh's cost |

## SH4ZAM-DCE (our SH-4 kernel layer)

| Feature | What it is | Label | Version | Evidence | What would promote it | What it needs from a console |
|---|---|---|---|---|---|---|
| SH4ZAM-DCE, as a whole | Asm kernels in SH4ZAM's style, with C references and host tests | PROTOTYPE | v0.1 | Two kernels timed on the console (below); the rest host-tested only, so the layer as a whole is capped by its weakest parts. (Scholar's draft had ALPHA v0.3; Parity's reading of the weakest-claim rule is taken.) | Each kernel timed | One bench a kernel |
| `shz_dce_pvr_project` | 32-byte PVR vertices through XMTRX and the divide | **BETA** | v0.6 | h2: 21.15 cycles a vertex against 43.19 for its C form (SH4-023); c4: the placing case and its fix (SH4-046, SH4-048); c4g: 60.0 to 61.2 cycles in two runs, to the tenth (SH4-074). Bytes the same as the C form. Used by c4. 1 console. | A second console | The same bench on another console |
| `shz_dce_xmtrx_project_point3_batch` | Points through XMTRX and the divide, three in flight | ALPHA (demoted) | **v0.3 (Scholar) / v0.4 (Parity)** | Split: Parity, two runs (SH4-065: "over two runs") meet the v0.4 rule; Scholar, a kernel measured slower than the call it was meant to replace should not read as further along than a kernel with one run. Slower than KallistiOS's `mat_transform` in both runs: 14.15 against 13.38 cycles in the cache, 45.99 against 42.86 out (SH4-050, SH4-065). Kept on record; use `mat_transform`. | Only a rework that beats `mat_transform` | — |
| `shz_dce_atan2f`, `_asinf`, `_acosf` | Now SH4ZAM's own routines underneath | PROTOTYPE | v0.1 | Host only: 0.0007° worst for atan2 (SH4-019, host, not a console). | A console check of the back end | Worst error on the console |
| The other kernels | planes, `xform_vertex32`, `project_vertices`, `vertex20_unpack`, gather, quat, vector, sqrt, `shadow_compare_batch`, `tri_row_step` | PROTOTYPE | v0.1 | Host tests against their C forms and SH4ZAM; no console timing. | A console bench each (AGENTS.md: a speed claim needs a real console) | Cycles a vertex, in and out of the cache |

## Demos

| Feature | What it is | Label | Version | Evidence | What would promote it | What it needs from a console |
|---|---|---|---|---|---|---|
| ocean | The sunset ocean demo | PROTOTYPE | v0.1 | Runs on a real Dreamcast (`demo/OCEAN.md:5`, 2026-11-18): REPORTED; a steady 33.3 ms not measured. | A MEASURED frame row | Frame time per row setting |
| matchlight | The Match Light short film | PROTOTYPE | v0.1 | Host test; the Dreamcast build compiles; its costs are counted from tables, not measured (`demo/matchlight/README.md:5-6`). | A console run | Each shot's frame time |
| cloudvolume demo | The cloud deck fly-through | PROTOTYPE | v0.1 | Host preview; no console row. | A console run | The march's ms |
| demo disc | One program, every effect as a row | PROTOTYPE | v0.1 | `README.md:119` says its effects run on screen on it; no run is in the ledger. It calls look, glow, torch, shafts, taa and ca (and SH4DOW-DC); trail is included and never called; the other effects are not on it, so "every effect" in `AGENTS.md` and README is out of date. | A recorded run with a row per effect | Each row's frame time |
| torch walk | First-person walk with the torch and the normal map | PROTOTYPE | v0.1 | Shows a cost label on the console; no row; no tests, by design. | Its label read into ledger rows | Draws, lit cells, TA, ms |
| pl4nar street demo | The street driving demo (new map) | ALPHA | v0.4 | **pl4m c7, s8, a second run: 18.3 to 25.9 ms a frame, about 4.4 times c5; the PVR columns MEASURED (PVR-174, PVR-175).** pl4m c5, s7 (PVR-160 to PVR-162, SH4-086). Known defects: still SH-4 bound, not 60 (Q93); ten of 13 stages TOUCHED in c7; a van drives through the player's car (Junior, video). | 60 Hz, or a run with no key pressed | c8 with no key pressed |

## The suite discs' tools

| Feature | What it is | Label | Version | Evidence | What would promote it | What it needs from a console |
|---|---|---|---|---|---|---|
| suite menu | The disc's menu: lists, start, the four-button return | ALPHA | v0.4 | A start from the menu repeats c4b's results (PVR-116, s1); every suite run s1 to s7 started from it. | Tests of the menu's own logic in CI (`suite_nav_test` exists) and a second console | — |
| page kit | Result pages with a CRC, QR codes, graphs | ALPHA | v0.5 | **s8, mip-a and corr-a2: 21 text pages and 4 graphs agree with their CRCs; the graph-title fix seen on the console (VID-016).** A host test exists (`demo/psm/compare/test/psm_page_test.c`) but no make target or script runs it (Parity), and it does not judge the BIOS font, so BETA's "tests cover it" does not hold. The CRC line works: 10 pages in s1 (VID-015), 19 pages typed from the s7 video, all agree; QR codes read from a photo (VID-014). Used by every spike. No known defect left. | `psm_page_test` run by `make test` (then BETA v0.6) | — |
| autorun | A test that boots into itself and ends on result pages | ALPHA | v0.4 | Town segments (PVR-121), ao1c twice with means within 0.1 ms (PVR-148), every s6b and s7 entry. OPEN: hud1b's hang and crawl (PVR-151, Q87). | Q87 closed | hud1c-2 |
| PVR-DMA download | Video memory back to RAM by Holly's own DMA | ALPHA | v0.5 | Every copy right in h1, h2, s2, c4, c4b and s7 (BUS-020, BUS-021, BUS-050 to BUS-053); used by route AB and d1's fetch (PVR-163). OPEN: concurrency with ch2 (BUS-029); the polled rate at 25 MB/s in s7 (Q92). | Q92 explained; pvrx as the layer | A second console |
| tile cut | A render pass through a tile table cut to the tiles drawn | ALPHA | v0.4 | c4: the map 3,312 → 375 us, twice (PVR-092); s7: target M 3,256 → 344 us, the same texel sum (PVR-156). Two programs, 1 console. Not read: the rewrite cost (ao4 p3). | Other target sizes; the rewrite cost | A run that shows ao4 p3 |
| c4 depth chain | The PVR-side depth compare through the bump unit: bands, bias, 1555 mask | ALPHA | v0.4 | c4, c4b, c4g (PVR-028 to PVR-100, PVR-116, PVR-133); many rows, repeated. OPEN: equal depths (PVR-046); the depth union fails (PVR-127, Q76). | Its OPEN rows closed | — |
| baked AO | The suite's AO byte a vertex, baked at load | ALPHA | v0.5 | s3 (PVR-135), s6b (PVR-138, PVR-148 twice), s7 (PVR-152, PVR-166: it shows in the town's colour look); the bake 252 to 254 ms (SH4-076, SH4-082). OPEN: its placing cost tripled in ao4-h (Q91); the owner's eye (Q86). | Q91 explained | ao4 with the divider code out |
| hen rays | The hens' eyes and sweeps (hens-eyes) | ALPHA | v0.4 | Two runs within 5 % (SH4-080, SH4-085). OPEN: the cost model reads 3 to 4 times low for the sweeps (Q88). | Q88 | Each test timed alone |
| D1 depth fade | A halo that fades where it meets the scene, by a camera depth map | ALPHA | v0.3 | **d1-b, s8: the fade ran and costs +3.7 ms of frame, nearly all the depth map (PVR-172); T1 and T2 repeat code for code (PVR-171, two runs). Its look is OPEN: the glow sat left of the divider (PVR-173, Q90).** The split (Scholar v0.2, Parity v0.3) is settled at v0.3: Scholar's reason, that the fade had not run, is gone. s7: the level into destination alpha works (PVR-163); the camera depth code reads back right (PVR-164). The fade itself not run in s7 (PVR-165). | The look read with the glow across the divider; the map pass cut to its tiles | T3 placed right |
| user clip | The screen split by the PVR's user tile clip | ALPHA | v0.4 | s6b (PVR-144) and s7 in three programs (PVR-155, PVR-158, PVR-161); no hang. OPEN: CORE 18 in other orders (Q95). | Q95 | The guard test with the divider moving |
| effect labels | The label rows naming each effect on pages and live views | ALPHA | **v0.4 (Scholar) / v0.5 (Parity)** | Drawn on every page of eight programs on three discs, off in every measured stage, clear of graph titles (VID-016, MEASURED); also in s7, where the graph-title defect was seen. | Tests of the label table on the host | — |
| split divider | The movable divider (`psm_split.h`) | ALPHA | v0.2 | The middle position works (PVR-144, PVR-161). Moving it has not been exercised (OPEN, Q95). | A run that moves it | The divider stepped a tile a frame |
| trilinear pair | Trilinear as two passes: TRILINEAR1 opaque, TRILINEAR2 translucent ONE × ONE, depth GEQUAL, the same vertices | ALPHA | v0.4 | Two programs, 1 console. mip-a: the pair gives a ramp of 48 codes with no step over 2 on a flat-level texture, +0.48 ms over bilinear at 320 × 240 (PVR-168, PVR-170). corr-a2: +3.27 ms PVR and +0.74 ms SH-4 at 640 × 480, the corridor drawn with no doubled or black half (PVR-180, PVR-182). OPEN: the blend read off the corridor's screen (PVR-182). | The corridor split read with no light change across it | — |
| mip level probe | A flat-level mipmapped texture read back to see which level each pixel took (mip-a) | ALPHA | v0.3 | mip-a, 1 run: per pixel; footprint, not depth (2 levels a doubling on a floor, 1 on a wall); costs (PVR-167 to PVR-170). OPEN: the floor reads up to one level above the MAX guess (Q96). | Q96; a second run | mip-a with the D adjust stepped |
| fakeaf bias code | `demo/psm/fakeaf.h`'s `faf_bias_code`: a mip D adjust a polygon from its slope | ALPHA | v0.2 | Host test (`fakeaf_test`). corr-a2 ran it per polygon on the console: bias by slope 7.20 and 7.21 ms PVR, a fixed bias 7.71, bilinear 6.17 (PVR-180, 1 run). The level it picks was not read back, so what is measured is its cost, not its effect. OPEN: batched headers; why a biased level costs +1 ms. | The level it picks read back (as mip-a) | corr with a mip-a read-back |

## Re-rank after s8, mip-a and corr-a2

Scholar, 2026-11-25, from PVR-167 to PVR-182 and VID-016 (`2026-11-25 17-03-40.mkv`, `filtering-bench.mkv`; one console); Parity's second read the same day (blind first, then compared). Where the two differ, both are shown.

**The client rule decides five rows.** The scale says a client is "a program other than the part's own test that calls it as a library (a demo or a suite program)". Read as written, the town counts for psm, psm_raster and psm_pvr, the street demo for pl4nar, and two suite programs for the baked AO: all five are v0.5. If the owner counts only clients outside our demos and suite, those five go back to v0.4, and `shz_dce_pvr_project` (its client is c4g, a suite program) drops from BETA to ALPHA v0.4, leaving BETA 0.

| Row | Was | Now | Backed by |
|---|---|---|---|
| guard | ALPHA v0.2 | ALPHA v0.3 | PVR-182: its drops counted on the console (21 and 38) |
| pl4nar | ALPHA v0.3 | ALPHA v0.5 | PVR-174, PVR-175: a second run (c7); the PVR columns MEASURED; the targets repeat. v0.5 by the written client rule: the street demo calls it (`demo/pl4nar/src/demo.c`); held by OPEN only (Q93, Q97) |
| pl4nar street demo | ALPHA v0.3 | ALPHA v0.4 | PVR-174, PVR-175: a second run |
| D1 depth fade | v0.2 / v0.3 (split) | ALPHA v0.3 | PVR-171, PVR-172: the fade ran, its parts repeated; the look OPEN (PVR-173) caps it |
| page kit | ALPHA v0.5 | ALPHA v0.5 (Scholar's draft BETA v0.6 withdrawn) | VID-016 shows the defect fixed, but no target runs `psm_page_test` (Parity) |
| effect labels | PROTOTYPE v0.1 | ALPHA v0.4 (Scholar) / v0.5 (Parity) | VID-016: a row of its own, eight programs. Split: v0.5's wording names an OPEN item or a known defect as the hold; the hold here is a missing test. Scholar reads that strictly (v0.4); Parity takes v0.5 if a missing test counts. The page kit's row must take the same reading |
| trilinear pair (new) | — | ALPHA v0.4 | PVR-168, PVR-170, PVR-180, PVR-182: two programs |
| mip level probe (new) | — | ALPHA v0.3 | PVR-167 to PVR-170: one run |
| fakeaf bias code (new) | — | ALPHA v0.2 | PVR-180: its cost only |
| baked AO | ALPHA v0.4 | ALPHA v0.5 | PVR-176 (ao4-i repeats ao4-h), PVR-179 (the town walk, AO 1 BAKED): repeated in s3, s6b, s7 and s8, called by two suite programs, held by OPEN only (Q91's cause, Q86). Parity's read; Scholar's draft kept v0.4 |
| psm, psm_raster, psm_pvr | ALPHA v0.4 | ALPHA v0.5 | Repeated in every town run (PVR-108 onward), PVR-179 again; the town is a demo that calls the headers (`demo/psm/psm_scene.h`); held by OPEN only (PVR-121, Q77). The written rule, applied as Parity read it; the earlier "a client outside the town" was not in the rule |

Not moved, and why:

| Row | Stays | Why |
|---|---|---|
| sss, bloom: rings | ALPHA v0.3 | ao-lamp d was stopped by the pad at stage 2, before the lamp's stages (PVR-177): no second run of either |
| pool | ALPHA v0.2 | pl4m c7's blob stage was TOUCHED (PVR-175); its cost is still not isolated |
| spark | PROTOTYPE v0.1 | The halos ran again in c7's night stages; still no row names them |
| tile cut | ALPHA v0.4 | ao4-i's cut stages did not run (PVR-176) |
| user clip, split divider | ALPHA v0.4, v0.2 | ao4-i's guard marks were not read; the divider was not moved (PVR-176) |
| autorun | ALPHA v0.4 | hud1c-2 did not run again (PVR-179): Q87 open |
| PVR-DMA download | ALPHA v0.5 | Two more right copies, mip-a's and d1-b's fetch (PVR-167, PVR-171); Q92 still open |
| hen rays | ALPHA v0.4 | Not run in s8 |
| SH4ZAM-DCE rows | as they were | No new rows |

## Parity's re-rank

Parity re-ranked every row from the same ledger, 2026-11-25. Parity's own note: not fully blind (an earlier review file and one search showed two of the draft's ranks), so every row where the two differed was checked against the ledger before it was settled.

| Row | Scholar's draft | Parity | Settled | Why |
|---|---|---|---|---|
| SH4ZAM-DCE, as a whole | ALPHA v0.3 | PROTOTYPE v0.1 | PROTOTYPE v0.1 | The weakest-claim rule: most of its kernels have no console row |
| guard | ALPHA v0.3 | ALPHA v0.2 | ALPHA v0.2 | PVR-080 and PVR-081 are FACT from README; PVR-161's zero does not show it acting |
| bloom: rings | ALPHA v0.2 | ALPHA v0.3 | ALPHA v0.3 | SH4-083 times the halo alone: a row of its own |
| sss | ALPHA v0.2 | ALPHA v0.3 | ALPHA v0.3 | SH4-083 times the shade alone |
| warp | ALPHA v0.2 | ALPHA v0.3 | ALPHA v0.3 | BUS-004 times the draw calls alone |
| `shz_dce_xmtrx_project_point3_batch` | ALPHA v0.3 | ALPHA v0.4 | **both shown** | Two runs (rule) against "measured slower than what it replaces" |
| D1 depth fade | ALPHA v0.2 | ALPHA v0.3 | **both shown** | Its parts have rows; the fade itself has not run |

Every other row agreed, after Parity rechecked pl4nar, pool, bloom rings, sss, `shz_dce_pvr_project`, the three psm rows, user clip, PVR-DMA, page kit, tile cut and baked AO against the ledger. Parity's host check the same day: `make -k test` exit 0, all 34 test programs passed (binaries already up to date, not a fresh build).

## Old version claims, undecided

These version numbers predate this scale and await the owner. They are not used above.

| Claim | Applies to | Where |
|---|---|---|
| Version scale: v1.00 used in a released game, v0.50 runs on screen on the demo disc, v0.10 passes its maths tests | every effect | `README.md:119`; repeated in `wiki/effects/README.md` |
| v1.00 ("used in a released game") | core, blade, holo, bloom | `README.md:93-95, 119`; `include/sh4der_core.h:49`, `sh4der_blade.h:34`, `sh4der_holo.h:31`, `sh4der_bloom.h:31`; `src/` of the same |
| v0.50 ("runs on screen on the demo disc"): the nine | look, frame, taa, glow, torch, ca, shafts, water, sss | `README.md:96-99, 105, 107-110`; their headers (`sh4der_look.h:36`, `_frame.h:37`, `_taa.h:39`, `_glow.h:46`, `_torch.h:48`, `_ca.h:34`, `_shafts.h:34`, `_sss.h:39`) |
| v0.50 | cloudvolume | `include/sh4der_cloudvolume.h:60` |
| v0.14 | bump | `README.md:112`; `include/sh4der_bump.h:43` |
| v0.10 | warp, trail, lens, focus, visor, ao, guard, uvwarp, spark, pl4nar, pool, psm, psm_raster, psm_pvr, bolt, motes, deform, mist, rise | `README.md:100-117`; each header |
| v0.1 | film | `include/sh4der_film.h:49` |
| v0.05 (until the pvrx-check suite entry) | pvrx | `include/sh4der_pvrx.h:22, 44`; `AGENTS.md` Files |

## Out-of-date lines found while ranking

Facts in other documents that the ledger or the code contradicts, found by Scholar and Parity. The wiki's own were fixed the same day; the rest are listed for whoever owns the file (`AGENTS.md` is the owner's).

Fixed in the wiki:

- `wiki/effects/README.md` gave pl4nar "no (one emulator figure only)" for a console cost; pl4m c5 measured its lookups (PVR-160).
- `wiki/effects/pl4nar.md` said no time was measured on a console; it now has the lookups and the targets (PVR-160, PVR-161) and the mirror pass's SH-4 share (DERIVED).
- VID-001 gave 320 × 240 as 160 KB without the whole tile rows (320 × 256), as `include/sh4der_ca.h:12` has them.
- Struct sizes on the effect pages, from Parity's host `sizeof` probe: `effects/blade.md` (156 and 160 B), `effects/water.md` (28 B a wave, its open item answered), `effects/mist.md` (68 B), `effects/psm.md` (80 B).
- `effects/bloom.md`'s open item (is `src/sh4der_bloom.c` PVR drawing?): answered, maths only.

Outside the wiki, not edited:

- `AGENTS.md` (the `sh4der_psm_raster.h` and `sh4der_psm_pvr.h` lines) says nothing has run on a Dreamcast; the town calls both and has run on the console many times (rows above).
- `AGENTS.md` and README: the demo disc "shows every effect as a row"; it shows six (the demo disc row).
- `AGENTS.md`: `src/sh4der_bloom.c` is described as KallistiOS PVR drawing; the file has no KallistiOS or PVR include and calls itself maths only.
- `demo/pl4nar/README.md:13`: the status line predates pl4m c5 (PVR-160, PVR-161).
- README struct sizes, by Parity's size probe on the host (no pointers in these structs, so the SH-4 sizes match; not a console result): the blade frame 156 B and params 160 B (README:256 says 144 and 136), a water wave 28 B (README:1155 says 24), mist 68 B (README says 64), the psm map 80 B (README says 76).
- pvrx is in `AGENTS.md` and its header (v0.05) but not in README's effect table.
- bump has a version (v0.14) and no host test in `test/`.

## Related

- [Hardware ledger](../hardware/README.md): where every label's evidence lives
- [Effects index](../effects/README.md): one line an effect
- [Recommended limits](../limits/recommended-limits.md)
- [Open questions](../open-questions.md): the OPEN items that cap the ALPHA rows
