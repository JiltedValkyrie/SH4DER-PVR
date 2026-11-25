---
title: Open questions
tags: [open]
updated: 2026-11-25
sources: [every wiki page's "Open" section]
---
# Open questions

**What you will learn:** everything this project does not know yet, in the order we would settle it, and the test that would settle each.
**For:** whoever plans the next console run, person or agent.

Each item names the page it comes from. When a test settles one, move the answer into the ledger, mark the page, and delete the line here.

## 1. Next console runs (highest value)

| # | Question | Test | From |
|---|---|---|---|
| Q1 | sh4dow-dc: how many characters (N) within what distance (R) fit 3 ms of SH-4 time? | Bench request in [recommended limits](limits/recommended-limits.md#bench-request-sh4dow-dc-n-casters--r-metres) | limits |
| Q2 | Town route A: why 18 fps when the SH-4 reads 13.6 ms and the PVR 15.9 ms? In c4b's live chain the frame was the render-done waits (PVR-104); the same may hold here | Log `pvr_get_stats` render time, both waits, and frame time each frame in that view | [scenes](limits/scenes.md) |
| Q3 | Town route A: what are the other ~5.7 ms of PVR time beyond the shadow layer? | Route A with the map pass off, then the layer off, PVR time by `pvr_get_stats` | [scenes](limits/scenes.md) |
| Q4 | How far must casters and draw distance be cut for 30 fps in the town? | Sweep CASTERS and DRAW DISTANCE; log SH-4, PVR, fps | [recommended limits](limits/recommended-limits.md) |
| Q5 | The PVR render time of a caster pass into a render target | `pvr_get_stats` around a caster-only target pass, at 128 and 256 | [PSM routes](shadows/psm-routes.md) |
| Q6 | Route B split: raster time against compare time | Cycle counter around each, town view | [PSM routes](shadows/psm-routes.md) |
| Q8 | The c4 picture-only photos (ten in `c4e`, `070703610` to `071118010`; five in `c4_c4`, `082228971` to `082329488`): which settings each shows. `compare/NOTES.md:256-261` describes six of the `c4e` ones as seen | Rerun with the settings on screen | [bump compare chain](shadows/bump-compare-chain.md) |
| Q9 | Warp mesh: why it loses by store-queue runs (973 against 914 ns a vertex) when the pixelate pass gains (241 against 366). The source now defaults the mesh to `pvr_prim` | Vary the block size and alignment in bench h2's test 7 | BUS-004, [warp](effects/warp.md) |
| Q53 | c4b narrowed it (PVR-104): the frame (29.9 or 39.9 ms, vblank 60 Hz) is the render-done waits, 36 of 39 ms; the ready waits are tens of microseconds and polling them changed nothing. The waits run 2.9 to 9.1 ms past the renders, which fits KallistiOS's 100 Hz wake (PVR-011, errata K3). Still OPEN: is the tick the whole cause? | Time a render done polled on `render_busy` in place of `pvr_wait_render_done`, same live chain | PVR-094, PVR-104 |
| Q57 | Bias after c4b: with Q from the measured edge, the ground alone has 0 acne at every b0 from 0 to 3, bands 1 and 4 (PVR-103), and both scenes have 0 acne at b0 3.0, bands 2. Still OPEN: how low b0 can go in the scenes (0 to 1.5) with the measured edge, at more suns; the owner's live view used 5.25 | c4b test 4 at b0 0, 0.5, 1, 1.5 with the measured edge, three suns | PVR-098, PVR-103 |
| Q58 | Joining several compares into one mask (T4's three): a write that never clears a shadow bit an earlier pass set. c4's bands write with ONE/ZERO, which overwrites. **In part (c4b test 9):** the punch-through route gives the same single mask as the 1555 route to the pixel at two suns, for 14-18 % more PVR time, with a green depth pass first (PVR-105). Still OPEN: whether it joins several compares without clearing bits. Other candidate: a destination-alpha mask, as the library's trail pass already uses (`src/sh4der_trail.c:127-151`; [post-PVR tricks](research/post-pvr-tricks.md) H1) | Two overlapping compares into one target, by ONE/ZERO and by punch-through; count bits against the union on the host model | Designer, T4, PVR-065, PVR-105 |
| Q64 | c4b's band cut is 22 to 28 % slower than c4's (3,183 / 1,691 / 908 against 2,483 / 1,332 / 744 us), where by design only `c4_q_set` and `set_init` changed | Time `c4_q_set` against `c4_q` alone; check the cut's own arrays' placing against `vpool` | SH4-049 |
| Q54 | Use a tile list cut to a target's covered tiles in the library's target passes (saves 2.9 ms on a 128 × 128 map in c4) | Build it into a target pass and time it | PVR-092, PVR-093 |
| Q55 | Town TOWN-001 (a3 HUD): the photo's file name | Owner gives it; until then it is MEASURED by relay | errata O5 |
| Q10 | Town TA line: why KOS's "last frame" reads 56 K to 966 K for the same view | Log `pvr_get_stats` beside the estimate each frame | VID-011 |


Q53 in one picture:

![The frame waits, c4 and c4b (one console, 2026-11-25): in c4 the frame is 39.9 or 49.9 ms while the SH-4 and PVR work in it adds to 19.3 to 27.1 ms (MEASURED PVR-090, 091, 101, 094); in c4b, over 60 frames, the vblank counts 2 or 3 a frame, a 60 Hz vblank, the frames are 29.9 or 39.9 ms (mean 39.2, not whole vblanks), and the three render-done waits take 36.2 of the 39.2 ms, running 2.9 to 9.1 ms past their renders, while the ready waits take 1 to 40 us and polling them changed nothing (MEASURED); a wait that wakes on a 10 ms clock fits (INFERENCE), the cause is OPEN (Q53, P10), and the next test is a render done polled on render_busy.](images/frame-wait.svg)

## 2. PVR behaviour

| # | Question | Test | From |
|---|---|---|---|
| Q11 | Why a render target pass costs a 3.2 ms floor whatever its size. c4's tile stage points at KOS walking the screen's whole tile set (PVR-092) | Time an empty target through a full and a cut tile list | PVR-004 |
| Q12 | Why a 565 bilinear texture costs 2,081 us opaque but about 877 us on a translucent layer; format against layout in 4444 | Separate runs per format and layout | PVR-007 |
| Q13 | Translucent sorting off against on | Bench line | PVR-012 |
| Q14 | Why 16-bit ramp codes from 32,768 are one under | Sweep codes 16,384-32,768 finely | PVR-024 |
| Q15 | Why equal depths (M = Q) are undecided in the bump compare | Texel choice test at M = Q | PVR-046 |
| Q16 | Answered in part (c4 test 6, one run): KallistiOS's header (SRC_ALPHA / INV_SRC_ALPHA) thresholds at ≥ reference; ONE/ZERO writes every pixel; the tested alpha is texture × vertex (PVR-060, PVR-061, PVR-064). Still open: whether a failing pixel is blended with alpha 0 or not drawn; how texture × vertex rounds; a second run | A ONE / ONE punch-through pass; a vertex-alpha sweep around the reference | PVR-061, PVR-064 |
| Q17 | c3b's level 28 where the outside rule gives 32 | Bump levels at a = 101, 102 | bump compare chain |
| Q18 | A 129-row target: is one extra row enough? | s3's held-pixel test at 129 | PVR-021 |
| Q19 | Modifier volumes at town scale (spike c2) | Run `psm-vol-c2.cdi` | bump compare chain |
| Q20 | A Gouraud base under a bump polygon | Console test | errata E4 |
| Q21 | Punch-through depth mode. Answered in part (c4 test 6, one run): depth write off and depth ALWAYS drew the same as KallistiOS's header on an empty screen, so skmp's "does nothing" is not contradicted. Still open: "ignored" against "harmless here"; a second run | The punch-through quad behind an opaque quad with depth ALWAYS | PVR-062, errata E2 |

## 3. SH-4, buses and DMA

| # | Question | Test | From |
|---|---|---|---|
| Q22 | The TA's own DMA vertex mode | Bench line | BUS-006 |
| Q23 | The one PVR-DMA run at 54.6 MB/s beside a cache-missing loop | Repeat that run | BUS-023 |
| Q24 | Can PVR-DMA and ch2 DMA run at the same time? | Run both at once | BUS-029 |
| Q25 | h1's 43.2 MB/s memcpy | Rerun h1's exact placing | SH4-034 |
| Q26 | Plain C 4 × 4 against `mat_transform` on a clean placing | Bench line | SH4-024 |
| Q27 | SH4ZAM trig accuracy on the console's own back end | The trig probe on a console | SH4-019 |
| Q61 | ray1's CLOCK.2 printed about 1.03 GHz (1,027,528 cycles in 1,000,320 ns) on a stock 200 MHz console: a bench reading artefact, not a hardware fact. Which read is wrong, and why? Do not use the figure | Read `bench_ray.c`'s CLOCK.2 against SH4-001's method; rerun CLOCK | SH4-051 |
| Q62 | ray1's ns column disagrees with its cycles on P1.4 n 48 (+10.8 %) and P1.2 n 12 (+611 ns); every other row agrees within 0.01 %. No ray1 ns figure is ledgered until this is settled | Repeat P1.4 (and P1.2) | SH4-051 |
| Q63 | ray1's slice of 1,024 stand-in receivers cost less at 16:00 than at 14:00 (539.68 against 554.19 cycles a ray), where the thesis expects about twice. What is the stand-ins' mean n at each sun, and what is the town's real receivers' mean n at 16:00? | Host count of the mean n for the same 1,024 stand-ins, then for the town's vertices | SH4-053, errata O11 |

## 4. Sound chip

| # | Question | Test | From |
|---|---|---|---|
| Q28 | The ARM7DI's wait states on sound RAM | Console test 1 in `ARM7_TOOLCHAIN_NOTES.md` | AICA-007 |
| Q29 | Multiply cost on the ARM7DI | Time MUL and MLA | AICA-008 |
| Q30 | The ARM's clock | Time a known loop with the AICA timer | AICA-006 |
| Q31 | No 16-bit loads, no long multiply on our own console | Console test 3 | AICA-003 |
| Q32 | A Yamaha AICA datasheet with timing. The AICA manual was found and read 2026-11-25 (AICA-026 to AICA-031): it gives the memory cycle (4 clocks) but no ARM wait-state count, so Q28 stays | Console test 1 (Q28) | AICA ledger |
| Q59 | Does a muted DSP (send level 0, master volume 0) still compute and write to sound RAM as normal? | Run a known DSP program muted; read EFREG and its writes | AICA-029, [sound chip](research/sound-chip-precompute.md) |
| Q60 | The DSP ring buffer base step: 1 K or 4 K words (the manual disagrees with itself) | Write at two bases one step apart; read back | AICA-031 |

## 5. Video memory and the frame

| # | Question | Test | From |
|---|---|---|---|
| Q33 | Free video memory after `pvr_init` for this library's own demo disc (the town and field builds have it: VID-012, VID-013) | Print `pvr_mem_available()` in `demo/main.c` | VID-004 |
| Q33b | The town crash while turning (TOWN-004) | Turn from that view with `psm_fault` and the vertex high-water mark on screen | `demo/psm/TOWN_NOTES.md` |
| Q34 | The frame read's time on our console | Time `sh4der_frame` | VID-005 |

## 6. Everything

- **Q35**: a second console and a second run for every MEASURED row. Until then no number has a spread.

## TODO from the owner (2026-11-25)

| # | Item | Tag | Status |
|---|---|---|---|
| T1 | Soft shadows: several offset compares of the c4 mask, averaged. | SHELVED | The owner stopped it: each mask pass measured 6.1-10.2 ms (PVR-090, PVR-091), so several are too dear. Reopen if a cut tile list brings a mask pass under about 2 ms (PVR-092 measured 3.76 ms for 80 tiles; the cheapest whole-list mask so far is 4.79-5.02 ms with bands 4 wide, PVR-100, PVR-101), or if one pass can give a soft edge. |
| T2 | Material functions on the PVR: depth tests from popular engine material functions (outlines, cel, depth fade, soft intersections, contact edges, shells, rim, darkening, dither, depth colour) mapped to PVR mechanisms, with costs from the ledger. | RESEARCH TASK | Draft written, awaiting review: [depth shading ideas](research/depth-shading-ideas.md). Every idea is ESTIMATE or OPEN; first test is a camera-view depth map. |
| T3 | Depth fog and glow: film and TV effects that use depth fog or light; can fog fill the space between c4's bands, and is it cheaper than the mask pass? | RESEARCH TASK | Draft written, awaiting review: [depth fog ideas](research/depth-fog-ideas.md). Answer: fog on surfaces reuses the mask; fog in the air adds about one mask pass per 8 slices (ESTIMATE). |
| T4 | Static sun: cache the static casters' map and bake static-on-static at load; give movers their own small map each frame, cut to their tiles; shadow where either map says so. | TODO | ESTIMATE / OPEN; written up in [bump compare chain](shadows/bump-compare-chain.md#idea-a-cached-static-map-plus-small-mover-maps-t4-estimate--open). Catches: the depth buffer cannot be preloaded (PVR-083), extra passes add waits (PVR-094), the break-even in movers is unknown. Test: now that c4b works (PVR-103), and after the polled wait test (Q53), time one static map plus one mover map against the single full map. |


Why T4 bakes the static part (band counts, ESTIMATE, host, TOWN-051 to TOWN-055):

![Band counts under c4's rules: the whole static town needs 44,577 / 26,969 / 18,097 band vertices at bands 1 / 2 / 4 codes wide, the boot view 11,146 / 7,090 / 5,174, and the movers' patches only 864 / 460 / 260, against 2,464 / 1,288 / 692 in c4's measured scene; the town counts and every cost on top are host ESTIMATEs (TOWN-051 to 055), which is why static shadow goes to a bake and c4 runs for movers only.](images/town-band-counts.svg)

The split itself:

T4 in one picture:

![Idea T4, not built: with a fixed sun, draw the static casters' map once and bake static-on-static shadow at load, then each frame draw only a small mover map cut to its tiles and run three compares (static on mover, mover on static, mover on mover) into one mask; how the bits join, and the break-even number of movers, are OPEN, and the PVR's depth buffer cannot be preloaded from a cached map (FACT PVR-083).](images/static-sun-split.svg)

## 7. From the effect pages

Items already above (warp mesh Q9, psm Q2/Q5/Q6, sh4dow-dc Q1) are not repeated.

| # | Question | Test | From |
|---|---|---|---|
| Q36 | Is `src/sh4der_bloom.c` PVR drawing (`AGENTS.md`) or maths only (its comments, bloom.c:35, 125, 212, 370)? | Lead's grep: 0 `pvr_` calls in `src/sh4der_bloom.c`, so the `AGENTS.md` line is probably wrong. Owner to fix `AGENTS.md` | [bloom](effects/bloom.md) |
| Q37 | RAM a water wave: 24 B (README:1155) or 28 B (water.h:11)? | `sizeof` on the host | [water](effects/water.md) |
| Q38 | The ocean demo: a steady 33.3 ms frame, and the cost of its BLOOM row | Console run with the readout, preset by preset | [water](effects/water.md), [scenes](limits/scenes.md) |
| Q39 | Glow: is the "GPU path" (render small, multiply with itself) built or tested? And its console cost | Read glow.h:~30 and the tests; time it | [glow](effects/glow.md) |
| Q40 | Trail: its "measured constants" (trail.c:143-144) have no data in the repo; and its console cost (about 19 ms PVR for two copies, ESTIMATE) | Find or redo the measurement; time it | [trail](effects/trail.md) |
| Q41 | Visor: the thermal ramp's fit error | Read the host test's print | [visor](effects/visor.md) |
| Q42 | Bump: the azimuth's turn against v, the axis signs, 565 palette banding | Console checks (README:1200) | [bump](effects/bump.md) |
| Q43 | pl4nar: every console time, and the render-target path on a console | Fill `demo/pl4nar/AGENT_WORKFLOW.md:60-71` | [pl4nar](effects/pl4nar.md) |
| Q44 | pl4nar: the cause of the matrix-chain wrong-camera bug | Isolate on a console (README:860) | [pl4nar](effects/pl4nar.md) |
| Q45 | Pool: PVR fill of a fan | `pvr_get_stats` with pools on and off | [pool](effects/pool.md) |
| Q46 | psm: route A's target pass and layer as library code | Write them | [psm](effects/psm.md) |
| Q47 | ca: the second scene's cost on a console | Time it | [ca](effects/ca.md) |
| Q48 | sh4dow-dc: packed lists, foot cones and shade cost on our console | Part of Q1's bench | [sh4dow-dc](effects/sh4dow-dc.md) |
| Q49 | Guard: photos of the three hardware faults for the ledger | Photograph each on a console | [guard](effects/guard.md), PVR-080, PVR-081, VID-009 |
| Q50 | Look and frame: the 80 × 60 frame read's time; whether a cached read pays for a sparse grid | Time both | [look](effects/look.md), [frame](effects/frame.md), Q34 |
| Q51 | mist, rise, film, deform, cloudvolume: any console time | Time each in a demo | the five pages |
| Q52 | `AGENTS.md`'s file list lacks deform, mist, rise, film and cloudvolume | Owner adds them | [effects index](effects/README.md) |
