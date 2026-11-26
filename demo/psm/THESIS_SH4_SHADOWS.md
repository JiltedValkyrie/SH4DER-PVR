# Cached per-vertex sun visibility on the SH-4: a design thesis for self-shadowing on the Dreamcast

Draft, 2026-11-25. A design document in the form of a paper. **No code has been written for it, and nothing in it has run.** It is built on what this project has measured on one console, on Sega's documents, and on the known bugs and contradictions in them. Where a number is not known, it is marked or left out.

Revised 2026-11-25, the same day, after spikes c3b and c4 and chip benches h1 and h2 ran (one console, one run each; c4 twice). New facts F22 to F26; F10, F20, §4, §5.8, §5.9, §6 (P7 overturned), §7 and §8 brought up to date. Ledger IDs (PVR-, SH4-, BUS-) are those of `wiki/hardware/`.

Revised again 2026-11-25, after the ray bench ray1 (one console, one run; SH4-051 to SH4-055). The grid walk and the proxy pass have now run as bench kernels, not as the design. Changes:

- §5.2: new constants; the miss cost corrected to SH4-047's 27.7 to 40.3.
- §5.3: the step cost, P1 overturned, and the stand-ins' mean n.
- §5.6: the pair estimates are low.
- §5.7: the worked example.
- §5.9: the budgets redone; the cold load and OC-RAM.
- §6: P1, P2 and P9.

The real receivers' mean n at 16:00 stays OPEN.

## Status marks

Every number and every claim about hardware carries one of these marks.

| Mark | Meaning |
|---|---|
| **MEASURED** | Read on a real Dreamcast by this project. One console, one run, unless the row says otherwise. Emulator results are never counted as evidence. |
| **DOC** | Sega's own documents, read in full: *Dreamcast/Dev.Box System Architecture* (1999, "SysArc"), the *SH4 access* table (1999), the *Holly TA&CORE* notes. |
| **REP** | Reported: KallistiOS source and headers, forums, emulator source, manuals we have not read. |
| **MODEL** | The cycle count of sh4zam-dce's SH7750 pipeline interpreter (`third_party/sh4zam-dce/KERNEL_GUIDE.md` §4), with every memory access assumed a cache hit. A model, not a console. |
| **FILE** | A constant in this repo, named with its file. |
| **DERIVED** | Arithmetic on the marks above, with no new input. |
| **ESTIMATE** | Our own count or arithmetic that adds an input of its own (an instruction count, a guessed share). Its inputs are named next to it. |
| **INF** | Inference: reasoning that has not been tested. |

## Abstract

On the Dreamcast, a depth-compared shadow must come from somewhere other than the PVR's depth buffer. The PVR keeps depth per 32 x 32 tile on the chip and writes none of it to memory (DOC, SysArc §3.4.4). It renders to 16-bit targets only (DOC, §3.4.10). Our measurements so far give two ways to compare depth:

- **Route B and route AB.** A 16-bit map in RAM, compared per vertex on the SH-4. Route B rasterises the map on the SH-4. In route AB the PVR draws it and PVR-DMA copies it back, 0.93 ms for 128 x 128 (MEASURED).
- **A blend compare on the PVR (candidate C1, spike c1).** Exact to one level at 8 bits, and to 6 bits when the PVR draws the map itself (MEASURED). About 11 translucent layers, each about 0.77 ms at 320 x 240 (MEASURED slope).

All of these rebuild the whole map every frame.

This thesis proposes the opposite trade: **do not re-render the shadow every frame; cache it.**

- **What is cached.** A per-vertex sun-visibility term, one byte per receiver vertex.
- **Static geometry.** The term comes from ray casts toward the sun through a coarse height grid of the scene, walked by a DDA, with exact triangle lists in the cells that overhang. It is baked at load and re-sliced over several frames when the sun steps. On the game's clock the sun's path is fixed, so a variant stores per-vertex occlusion intervals instead and needs no rays at run time.
- **Moving casters** (people, hens) are analytic proxies (capsules, spheres, boxes). They are evaluated only on the receivers near them.
- **Writing it out.** The term goes into the vertex colour, which the PVR shades for free. Where a triangle's vertices disagree, an optional overlay sharpens the edge, by reading the term through a point-sampled step texture or through the punch-through test, which our console showed acts on vertex alpha (F22).
- **Visibility.** Vertices hidden from the camera are not sent, to free TA vertices in the scene that needs them most.

The method trades per-frame cost for memory, latency and coarse static occluders. We state what it predicts, what would prove it wrong, and what we do not know.

## 1. The problem and its constraints

### 1.1 What is asked

Shadows that a surface casts on itself and on its neighbours (self-shadowing), from a sun that moves with a clock, in a town scene of about 13,900 screen triangles (target, `TOWN_STORY.md` §4), at 30 frames a second, on a retail Dreamcast. Modifier volumes are excluded by the owner as the method. They appear below only as context and as a measured fact.

### 1.2 The frame we are in

The reference frame is the town's "a3" view. The figures come from the owner's photo of the console's own HUD:

- Build `sh4der-psm-town-a3-b512`.
- Route A, uniform map, 256 x 256.
- DIST INF, CASTERS ALL.
- Camera x -1.7, z -11.8, heading 337, pitch -10.
- One view, one console. The photo is not filed in this repo.

| Quantity | Value | Mark |
|---|---|---|
| TA vertices a frame | 28,699 | MEASURED (HUD reading) |
| SH-4 time a frame | 22.6 ms | MEASURED (HUD reading). The demo sums its own timed parts (`demo/psm/psm_dc.c`, `cost.sh4_ms`). |
| PVR time a frame | 16.0 ms | MEASURED (HUD reading). It is `pvr_get_stats`' `rnd_last_time` of one sampled frame a second (`psm_dc.c`, `render_wait` and `cost.pvr_ms`). |
| Frames a second | 25 | MEASURED (HUD reading) |
| Vertex buffer used | 74 % at the a3 view, 77 % worst walk (783–792 KB of 1 MiB) | **ESTIMATE**, Junior's byte model, unmeasured. The TA does not store a flat 32 B a vertex, so vertex count x 32 B is the wrong scale. |
| TA vertex buffer | 1,536 KB (`PSM_TA_VBUF`, `demo/psm/psm_send.h`) | repo constant |
| Frame period at 30 fps | 33.3 ms (the a3 view runs at 25, 40 ms) | arithmetic |

So: the vertex count is measured; the buffer percentage is an estimate. The first measured percentage will be the HUD's `vtx_buffer_used_max` line in a later build. No photo of it exists yet.

### 1.3 Hard limits that shape the design

| Limit | Value | Mark, source |
|---|---|---|
| CPU clock | 200,006 cycles in 1 ms of the KOS timer | MEASURED, `ROUTE_AB_NOTES.md` s2 |
| SH-4 caches | I$ 8 KB, D$ 16 KB, both direct mapped, "index/RAM functions" | DOC, SysArc §2.2.1 Table 2-4 |
| D$ as RAM (OCRAM) | 8 KB of the 16 KB D$ | REP (Hitachi SH7750 manual, not read) |
| Store queues | 2 x 32 bytes | DOC, SysArc Table 2-4 |
| DMAC | 4 channels; channel 0 on-demand (DDT) for the PVR-DMA; channel 2 for TA / ch2-DMA | DOC, SysArc Table 2-4; `ROUTE_AB_NOTES.md` §1 |
| Render-to-texture formats | 16-bit only (0555, 565, 4444, 1555) | DOC, SysArc §3.4.10 |
| Depth out of the PVR | None. Z is per tile on the chip; no register stores it | DOC (no register); INF (no output) |
| Depth into a texture through the colour path | 10-bit ramp codes exact; 16-bit within one code | MEASURED, s2 / s3 |
| Translucent fill | about 3.1 ms a full-screen layer at 640 x 480 (about 99 Mpx/s) | MEASURED, `compare/NOTES.md` c1 |
| KOS scheduler | `pvr_wait_render_done` returns on the next 100 Hz tick: up to 10 ms lost | REP (KOS `pvr_irq.c`); the pattern MEASURED in s3 |

Note: the lead's brief gave the caches as "8 KB O-cache, 16 KB I-cache". Sega's table gives them the other way round; we use Sega's figures.

## 2. Related work

Safe wording only. We claim no first.

| Work | What it did | Mark, source |
|---|---|---|
| Tony Hawk's Pro Skater 1 and 2 (Dreamcast) | Renders to video memory for its skater shadow. How the shape is built is not documented; on screen it reads as a simplified shape, not the full mesh. | REP: libretro BeetleDC post (2018-06-12); the owner's screenshot (`PRIOR_ART.md`) |
| John Brooks, World Series Baseball 2K2 (Dreamcast) | Character shadows as PVR modifier volumes. Halo: Dreamcast Evolved ships a "WORLD SERIES" shadow option built on his renderer files. | Repo: `dc-halo/README.md` (files by John Brooks, copyright Blue Shift, Inc., used with permission); `dc-halo/third_party/JBrooksBSI/NOTICE.md` (notes that his written permission is not recorded in that repository); `dc-halo/docs/archive/campaign/CAMPAIGN_PRESENTATION.md` |
| Halo: DCE's measurement of that method | Its screen-space sweep (Brooks' `Ren_ShadowStrips`) shaded 0.2 % of the floor (81 of 41,206 pixels) on a head-height cutscene camera. Its columns carry the casting triangle's constant depth, so they reach only what lies at the body's depth. What ships is a world-space extrusion of lit triangles to the floor plane. | Repo, measured in that project: `CAMPAIGN_PRESENTATION.md`, correction of 2026-10-22 |
| Modifier volumes generally | The common Dreamcast shadow (Shenmue, Toy Commander, Speed Devils 2 headlights). | REP, `PRIOR_ART.md` |
| Star Wars Rogue Leader (GameCube) | Projected shadows; per-object self-shadowing with an 8-bit Z texture. | DOC (Sauer, Factor 5, 2002) |
| SWAT (PS2) | Static shadows precalculated; characters get a black circle. | DOC (Godbolt) |
| Shadow of the Colossus (PS2) | Stencil volumes on low proxies (about 1/40 of an 18,000-polygon colossus). | DOC (Game Watch, 2005) |
| Donnelly and Demers, *Occlusion Interval Maps* (GPU Gems 1, ch. 13, §13.2–13.4) | For a static scene and a light on a fixed path, store per point the times it becomes lit and unlit. A soft shadow is then "just min, max, and subtraction" against the light's width. | Online edition, read by Scholar (`demo/psm/GPU_GEMS_NOTES.md`) |
| Pharr and Green, *Ambient Occlusion* (GPU Gems 1, ch. 17, §17.2) | Accessibility = unoccluded rays / rays, per vertex, precomputed | same |
| O'Rorke, *Managing Visibility for Per-Pixel Lighting* (GPU Gems 1, ch. 15, §15.3–15.4) | Visible, light and shadow sets: which casters can shadow what is seen | same |
| Uralsky, *Efficient Soft-Edged Shadows* (GPU Gems 2, ch. 17, §17.2.2) | Take a few samples first; refine only where they disagree | same |
| SH4DOW-DC (this project's dependency) | Directional sun visibility over a heightfield by one dominant-axis sweep, O(w x h), no per-point ray march; no overhangs. | Repo: `sh4dow-dc/include/sh4dow_heightfield.h` |
| This project's routes A, B, AB | A: the PVR draws a silhouette into a target, laid on the ground only. B: the SH-4 rasterises a 16-bit depth map and compares per vertex. AB: the PVR draws the depth code, PVR-DMA copies it back, the SH-4 compares. | `README.md` "The psm effect"; `ROUTE_AB_NOTES.md` |

Per-vertex baked lighting is the common late sixth-generation answer for static scenes (Rogue Leader, SWAT, GTA San Andreas, Sonic Adventure 2; `LATEGEN_NOTES.md`, mixed DOC and REP). The proposal here is a baked term that is re-baked while the game runs, in slices, as the sun moves.

## 3. Measured platform facts

Every row is a fact this design leans on. "Source" is the file that records it.

| # | Fact | Value | Source | Mark |
|---|---|---|---|---|
| F1 | PVR-DMA download (VRAM to RAM, `SB_PDDIR` = 1, our own register writes) | 32 KB in 910–931 us (mean 925); 128 KB 3,702 us; 512 KB 14,803 us; about 35 MB/s | `ROUTE_AB_NOTES.md` s2, s3 | MEASURED |
| F2 | That download holds the bus (CHCR0 = 0x000853C3, burst bit set) | count it as SH-4 time | same | MEASURED (register); INF (the stall) |
| F3 | 20,000 scattered 2-byte reads | from VRAM 12,903 us; from a RAM copy 2,765–2,776 us (32 KB map), 4,026 us (512 KB) | same | MEASURED |
| F4 | Texel to 16-bit target, REPLACE, point sampling, dither off | 10-bit codes exact; 16-bit within one code (one-under from code 32,768 up; pattern not explained) | same, s2 / s3 | MEASURED |
| F5 | The last 32 bytes of a render are written at the next render | fixed by rendering 2 rows taller than the map | same, s3 | MEASURED |
| F6 | Dither can be switched off for one pass and back on | target exact, screen keeps its dither | same, s3 | MEASURED |
| F7 | C1 blend compare | one clean edge per row, constant +1 level; 8-bit map exact to that bias; PVR-drawn 565 map gives about 6 bits (err −2..+4) | `compare/NOTES.md` c1 | MEASURED |
| F8 | Translucent layer cost | 3,113 us a full-screen layer at 640 x 480; 771 us at 320 x 240; linear in layers | same | MEASURED |
| F9 | Opaque-only pass floor in that setup | about 3.15 ms at both sizes | same | MEASURED |
| F10 | Punch-through test (C3): texture alpha x vertex alpha, ref 64 | every pixel drawn (c1; c3b 5b and 5c the same). **Cause found in c4 test 6:** those headers used blend ONE / ZERO, which writes the pixels that fail the test as well (F22, PVR-061) | same; `compare/NOTES.md` "Hardware result c4: the result pages (second run)" | MEASURED (c1, c3b, c4; one run each) |
| F11 | Palette | one value a scene | same | MEASURED |
| F12 | One modifier volume | exact (11,008 px, 0 differ); no render-time cost seen (3,151 us with and without) | same | MEASURED |
| F13 | Shadow compare kernel, per point | 489 cycles (2 x 2 PCF, texel bias) / 184 (plain) at -O3 | `KERNEL_GUIDE.md`, `shz_dce_shadow_compare_batch` | MODEL |
| F14 | Depth raster, per texel | perspective covered 43, rejected 12 cycles (-O3) | `KERNEL_GUIDE.md`, `tri_row` | MODEL |
| F15 | FPU latencies used by the model | FIPR 4; FTRV 5–7, FPU busy 4; FSRRA 5; FSCA 6–7 | `KERNEL_GUIDE.md` §4 | MODEL (some "assumed" there) |
| F16 | Store-queue write to VRAM | 38 bus clocks per 32 B (about 84 MB/s) | Sega *SH4 access* table | DOC |
| F17 | ch2-DMA to TA / texture memory | 702.6 MB/s (64-bit) | same | DOC |
| F18 | Route B's raster and compare on a console | **never timed** | `ROUTE_AB_NOTES.md` "Still open" | — |
| F19 | The PVR's render time for a caster pass | not measured (s3's figure is a scheduler-tick upper bound) | same | — |
| F20 | Bump unit as a subtract; 1555 write threshold; fog-table depth code (spike c3, run as image c3b, `compare/sub_spike.c`) | **The bump unit is a subtract:** level about 128 + 127 sin(M − Q), Q one value a polygon (not interpolated), the result in alpha; exact to ±1 code, so a bias of at least one code is needed (PVR-045, PVR-046). The curve is not exactly a sine (c4 test 1: 443 of 40,960 pairs off the cosine table at window 32; PVR-096). **A non-twiddled bump texture, and a render target used as one, did not hang** and gave the same numbers as a twiddled one (CORE 26 not reproduced; one run is not proof). **The 1555 frame-buffer write thresholds accumulated alpha** exactly at 1, 2 and 128. **The fog table gives a depth code per pixel**, about one 6-bit step low (PVR-049). | `compare/NOTES.md` "Hardware result c3b" and "c4 ... (second run)" | MEASURED (c3b 2026-11-25; c4 test 1; one run each) |
| F21 | Modifier volumes at town scale | spike c2 (`compare/vol_spike.c`): built, **still not run** | repo; `wiki/shadows/thesis.md` | — |
| F22 | Punch-through, reference 128, a 4444 alpha ramp (c4 test 6) | KallistiOS's header draws the half with alpha ≥ reference (16,384 of 32,768; c3b 5a the same rule at 64 / 128 / 192; PVR-060). Blend ONE / ZERO draws all 32,768 (PVR-061). **Vertex alpha 128 draws 2,048: the tested alpha is texture alpha x vertex alpha** (PVR-064); how the product rounds is OPEN. The bump level can be the tested alpha (10,200 of 20,480 drawn; PVR-065). Depth write off or depth "always" changed nothing on an empty screen (PVR-062: cannot tell "ignored" from "harmless") | `compare/NOTES.md` "c4 ... (second run)" | MEASURED (one run); rounding OPEN |
| F23 | SH-4 operand cache (bench h1) | hit 3.17, read miss 18.41, write miss 41.91 cycles a 32-byte line; allocate + write 16.24 (SH4-003, SH4-004). OC-RAM holds 8 KB, is no faster than a hit (1,401 against 1,446 us for 20,000 reads) and makes the same 8 KB of RAM data 59 % slower by halving the cache (SH4-010 to SH4-012) | `bench/NOTES.md` "Hardware result h1" | MEASURED |
| F24 | Transform and placing (bench h2) | `mat_transform` 13.16 cycles a vertex in the cache on a clean placing, 38.67 with source and output a multiple of 16 KB apart (SH4-020, SH4-021); FTRV row 16.58, FIPR 38.10, plain C 72.43 cycles a point (SH4-017). c4's projection ran 132 cycles a vertex for this reason (its arrays 256 KB apart) | `bench/NOTES.md` "Hardware result h2"; `compare/NOTES.md` c4 | MEASURED |
| F25 | Bus (h1, h2) | TA 260 ns a vertex by `pvr_prim`, 142 ns in one store-queue run (BUS-001, BUS-003); upload 512 KB: store queue 216–224 MB/s, ch2-DMA 498 MB/s (BUS-011); PVR-DMA download ended by its interrupt 37.6–38.1 MB/s (BUS-021); a PVR render of 8 layers does not slow the SH-4 (BUS-033) | `bench/NOTES.md` h1, h2 | MEASURED |
| F26 | Frame period of the c4 live view | 39.9 ms at 676 and 1,288 vertices, 49.9 ms at 2,192, for 19.3, 21.7 and 27.1 ms of measured work: the frame is set by waiting, in steps 10.0 ms apart | `compare/NOTES.md` c4 (PVR-094) | MEASURED (the times); the cause OPEN (a wait on a 10 ms clock fits; INF) |

Naming: **candidate C1 / C2 / C3** are the compare routes in `compare/NOTES.md`: blend compare, destination alpha, punch-through. **Spike c1 / c2 / c3 / c4** are console test images (c3 ran as image c3b). Candidate C3 drew every pixel in spikes c1 and c3b (F10); c4 traced that to the ONE / ZERO blend and showed the test working (F22). Spike c3 is the subtract spike (F20); spike c4 is the whole bump-compare chain. **h1, h2** are chip benches (F23 to F25).

## 4. Known bugs and contradictions that matter

### 4.1 Sega's own bug list (SysArc §9; per-revision marks lost in the text extraction)

| Sega # | What | What it does to us |
|---|---|---|
| CORE 26 | A non-twiddled bump texture hangs the CORE. Marked "will become part of the specifications". | Any compare that reads a render target as a bump texture (rank 1 in `PVR_FEATURES.md`) must first download, twiddle on the SH-4 and upload. That brings back SH-4 time. §3.6.4 of the same document allows non-twiddled bump: **the document contradicts its own bug list.** On our console (c3b, one run) a non-twiddled bump texture and a render target read as one did not hang (F20). The marks per revision are lost, so this console's CORE may be one where it was fixed (INF); one run does not clear it. |
| CORE 25 | A polygon edge just on the negative side of a pixel centre is written twice. | An additive compare layer could add twice on a shared edge. c1 saw no sign of it on a two-triangle quad (INF: not tested on a grid). An edge overlay (§5.8) with a blend is exposed to it. |
| CORE 10, 11, 15 | A pixel write is skipped and End_Of_Render goes missing, worse with the X scaler. | Possibly related to F5. INF only. |
| CORE 21, 22 | Missing tiles; a per-tile change of translucent sort mode can hang. | Set the sort mode per render, never per tile. |
| CORE 23 | Non-twiddled texture next to a palette texture: 2 wrong pixels at the edge. | Do not mix them in one compare pass. |
| Register 1 | Registers, palette RAM and fog table RAM cannot be read back correctly. | Keep a RAM copy of every table we write. |

### 4.2 Where the documents disagree with themselves or with the console

| Point | One source | Other source | Bearing |
|---|---|---|---|
| Punch-through rule | §3.4.3: texel alpha >= PT_ALPHA_REF | §3.7.9.2: only PIX[A] = 1.0 is drawn | **The console follows §3.4.3** (c3b 5a, c4 test 6: alpha ≥ reference; F22). The tested alpha is texture x vertex, not the texel alone. c1's "every pixel drawn" was the ONE / ZERO blend (F10). |
| Punch-through depth mode | §3.4.3: GreaterEq | §3.7.9.1: LessEq | A punch-through copy drawn coplanar over its own base passes the depth test under GreaterEq and fails under LessEq. Flycast and skmp report that the depth mode and Z write disable are ignored for punch-through (REP); c4 saw no change with either, on an empty screen, which cannot tell "ignored" from "harmless" (F22, PVR-062). Still open. |
| Bump shading | §3.7.9.1: flat only | Fig. 3-21: Gouraud | Q one value a polygon on the console (F20); K / Q per polygon. |
| Bump non-twiddled | §3.6.4: allowed | CORE 26: hangs | did not hang in c3b, one run (F20) |
| PVR-DMA direction | §2.6.9 documents only system to PVR | F1: VRAM to RAM works, about 35 MB/s against the table's 51.4 | measured beats the gap |
| Render-to-texture pack mode | §3.4.10: the 16-bit modes | KOS #1506: RTT takes the main frame buffer's pack mode | keep the main mode 565 |
| Target height | TA lists to 2048 x 480 (MetalliC, KOS #696, REP) | MEASURED: KOS's 512 target renders only to row 479 | maps above 480 rows are out through the TA |

### 4.3 KallistiOS issues that bear on the method (REP, read 2026-11-24)

| Item | Bearing |
|---|---|
| No PVR-DMA download in KOS; its "PVR DMA" event is the ch2-DMA end (bit 19), not the PVR-DMA end (bit 11) | Routes AB and C need our own register code and handler |
| PR #1375: a TA overflow leaves corrupt CORE lists and lockups | The vertex budget is a safety limit, not just a speed limit. That is one more reason to cull (§5.7). |
| PR #1366 (closed 2026-04): `pvr_set_presort_mode` was broken | Needs a KOS from after April 2026 |
| PR #1594 (open): list continuation, up to 8 list groups a scene | Would allow an overlay group after the opaque world in one render |
| The 100 Hz tick in `pvr_wait_render_done` | Never block on render-done in the frame's critical path |

## 5. Proposed method

The equations in this section are Senior's (sent 2026-11-25), placed and lightly edited here. Their tags: **MEASURED** and **DOC** as above; **FILE** = a constant in this repo; **DERIVED** = arithmetic on the others; **ESTIMATE** = an instruction count, not measured. Senior's cycle counts use SH-4 latencies he holds from memory of the SH7750 manual: FIPR about 4, FTRV about 7, FSRRA about 4, FSCA 3 to 5, add / sub / mul 3, divide 10 to 12. Those are "DOC, from memory, to verify". They differ slightly from the sh4zam-dce model's (F15: FTRV 5–7, FSRRA 5). **Every cycle count below is an ESTIMATE until a bench replaces it.** Benches h1 and h2 have run and measured the cache, the transform and the bus (F23 to F25); none has yet timed a ray step, a slice or a proxy.

### 5.1 The idea

Sun visibility at a point changes only when the sun moves or a caster moves. In the town the sun moves on a clock: a quarter of an hour per D-pad step, or continuously at TIME SPEED S = 1, 8 or 60 (FILE, `psm_town.h`). Nearly every caster is static. So the plan is:

1. Compute each static receiver vertex's visibility once.
2. Keep it as one byte.
3. Refresh it in slices spread over frames when the sun has moved enough to matter.
4. Handle the few moving casters apart, with analytic proxies, only on the receivers near them.
5. Write the byte into the vertex colour, which costs the PVR nothing.

### 5.2 Constants

| Symbol | Value | Tag, source |
|---|---|---|
| F | 200,006,000 cycles a second (1 cycle = 5.0 ns) | MEASURED, s2 / s3 |
| a3 frame | T_sh4 = 22.6 ms, T_pvr = 16.0 ms, 28,699 TA vertices, 25 fps | MEASURED, HUD (§1.2) |
| a3 SH-4 work a vertex | 22.6e-3 x F / 28,699 = 157.5 cycles a vertex, everything included | DERIVED |
| Sun | latitude 52.2°, declination −5.9°, clock 9:00 to 16:00; the clock advances dt x S / 3600 hours a second | FILE, `psm_town.h` |
| Hand-driven sun | up to 30° of elevation a second at a full trigger | FILE, `psm_scene.h` (`PSM_SUN_SPEED`) |
| H | 13.0 m: the receiver slab's top (`PSM_TOWN_SLAB_B`), taken as the tallest caster | FILE; the reading is Senior's ASSUMPTION. **The tower is 24 m with its roof to 28 m** (`TOWN_STORY.md` §3). See 5.3. |
| Cache miss | 27.7 to 40.3 cycles a scattered read from RAM, loop included: 27.7 (s2) and 27.8 (s3) on a 32 KB map, a mixed figure since some reads may hit; 37.6 on 128 KB; 40.3 on 512 KB, nearer a read that always misses. 645 ns from video memory | times MEASURED, s2 and s3 (F3); cycles a read by arithmetic (SH4-047); hit share ESTIMATE. The bare line miss is 18.41 cycles (SH4-003, F23) |
| Grid walk | C_step 22.07 to 22.21 cycles a cell, C_setup 70.88 to 74.09 cycles a ray (the setup holds the loop's load and store); this text uses 22.1 and 72 | MEASURED cycles a ray, fits DERIVED (ray1 P1, SH4-052) |
| Proxy pair | 137.87 cycles a pair with early outs, 187.89 without (8 capsules and 4 spheres at 16:00) | MEASURED (ray1 P9, SH4-055) |
| Translucent fill | about 99 Mpx/s | MEASURED, c1 (F8) |

### 5.3 Static casters: a height grid and a DDA toward the sun

**Symbols.**

- c: cell size (m). Nx, Nz: grid size in cells.
- h[i][j]: tallest caster in cell (i, j) (m).
- p: the vertex (y up). e: sun elevation. a′: sun azimuth from the grid's x axis.
- s = (cos e cos a′, sin e, cos e sin a′): unit vector toward the sun.
- H: tallest caster in the grid.

**The ray** is r(t) = p + t s; its height is y(t) = p_y + t sin e. It is clear once y >= H, after a horizontal run

    L = (H − p_y) / tan e                                         (m)

**Cells visited** by an exact walk, one cell a step:

    n = (L / c)(|cos a′| + |sin a′|),  capped at Nx + Nz

So L/c <= n <= 1.414 L/c, with a mean over azimuths of (4/π) L/c = 1.273 L/c.

**The walk.**

- Per frame: dtx = c / (cos e |cos a′|) and dtz = c / (cos e |sin a′|). The rays are parallel, so these are shared.
- Per ray: tx, tz = t at the first x wall and the first z wall.
- Per step: advance whichever of tx and tz is smaller.
- The test in the cell being left: shadowed if h[i][j] > p_y + t_entry sin e (dark-side conservative) or > p_y + t_exit sin e (light-side conservative).
- Stop at the first hit, at y >= H, or at the grid's edge.

**Cost.** The first draft's ESTIMATE (about 10 instructions a step, grid in cache) was C_step = 12 to 16 cycles and C_setup ≈ 30. **The console overturned it** (P1; MEASURED 2026-11-25, one console, one run; SH4-052). Bench ray1 timed rays of known n on a 4 KB grid in the cache. Its straight-line fits over four sets of rows (zero grid and town grid, 14:00 and 16:00) give:

    C_step = 22.07 to 22.21 cycles,   C_setup = 70.88 to 74.09 cycles   (fits DERIVED from MEASURED cycles a ray)
    C_ray  = C_setup + n C_step  ≈  72 + 22.1 n                        (cycles; the values used from here on)

- The step is 1.4 to 1.8 times the old estimate, and the setup about 2.4 times; C_setup holds the loop's load and store.
- The town grid's rows lie within 0.5 % of the zero grid's, so the grid's content does not change the step.
- The kernel gave the reference's bytes on every page.

A fixed-point walk along the major axis is about 8 instructions, ESTIMATE 5 to 6 cycles a step, over n_major = (L/c) max(|cos a′|, |sin a′|) steps. It must test two cells a step to stay conservative where the ray clips a corner. It has not been timed. Since the float walk came out at 1.4 to 1.8 times its own estimate, this estimate should be read as low until a console times it (INF).

**Worked** (DERIVED; sin e = sin(lat) sin(dec) + cos(lat) cos(dec) cos(15° x (hour − 12)); H − p_y = 13 m, c = 1.5 m):

| Hour | e | tan e | L | L / c | n min .. mean .. max |
|---|---|---|---|---|---|
| 9:00 | 20.5° | 0.374 | 34.8 m | 23.2 | 23 .. 30 .. 33 |
| 12:00 | 31.9° | 0.622 | 20.9 m | 13.9 | 14 .. 18 .. 20 |
| 14:00 | 26.5° | 0.499 | 26.0 m | 17.4 | 17 .. 22 .. 25 |
| 16:00 | 12.9° | 0.229 | 56.7 m | 37.8 | 38 .. 48 .. 53 |

At 14:00, with mean n = 22 (the table above, ESTIMATE) and the console's constants, C_ray = 72 + 22 x 22.1 = 558 cycles = 2.8 µs (DERIVED). The first draft had 338. All 28,699 vertices in one frame would take 16.0 million cycles, 80 ms (DERIVED from MEASURED constants on an ESTIMATE n; the first draft said 48.5 ms). That is why the term is cached.

A shadowed vertex stops at its first hit, so the table's n is a lit vertex's cost. The ray bench's 1,024 stand-in receivers show how much early exits matter (host count, `demo/psm/bench/ray_mean_n.c`; the rays are those of P2):

| Sun | Lit / shadowed | Mean n: all / lit / shadowed | Line 72 + 22.1 n (DERIVED) | Console, batched, grid warm (MEASURED, SH4-053) |
|---|---|---|---|---|
| 14:00 | 524 / 500 | 22.08 / 38.61 / 4.75 | 560 | 554.19 |
| 16:00 | 432 / 592 | 21.47 / 41.64 / 6.75 | 546 | 539.68 |

- The line meets the console within 1.5 % at both suns. So P2's slice is explained by P1's constants and the stand-ins' own n.
- At 16:00 more stand-ins end in shadow, and early, so the mean n falls although a lit ray is longer. That is why the console's 16:00 slice cost less than its 14:00 slice.
- The stand-ins are random points over the casters' box, not the town's vertices. **The real receivers' mean n at 16:00 (48 in the table above, with H = 13 m) is OPEN.**

**Three limits of the height grid, and their fixes** (ours, INF):

- **H is the tower, not 13 m.** With H = 28 m, at 14:00, L = 56 m from the ground and mean n = 48: about twice the table's cost. A global H is a weak early out. Use a coarser max-height level (for example 4 x 4 cells), and stop when the ray is above that block's max. This is a standard two-level walk; its cost is not estimated here.
- **A height column is solid from the ground up.** The grid cannot represent the arch's passage, the eaves' overhang or the stall canopies. A low sun through the arch would be wrongly blocked.
- **A wall vertex lies inside its own building's column**, so its first cell is a self-hit. Starting the walk one cell out along the normal fixes the self-hit, but skips the eave 0.5 to 1 m above a window. That eave's shadow on the wall is exactly what route B's eave bands exist for (`TOWN_MOCKUP.md`, finding 5).
  - **How large the self-hit is on the town** (host ESTIMATE, 2026-11-25, `demo/psm/bench/ray_town_n.c` on the bench's grid and walk). Of the sun-facing static model vertices (4,914 at 14:00, 4,708 at 16:00), the grid walk calls 98 to 99 % shadowed when started 0.05 m off the surface, nearly all within one or two steps (mean n 1.6 to 1.7).
  - Started 0.3 m along the normal and 0.5 m toward the sun (TOWN-065's offset), it calls 84 % / 94 % shadowed. Started one cell (1.5 m) out, 73 % / 88 %.
  - A triangle test calls 43 % / 74 % shadowed (TOWN-061).
  - So without the fix below, the grid walk is not a usable term for the model vertices, and their n must not be used for a budget. The ground's rays are not affected.

The fix for all three is the same: cells flagged "overhang" carry a short list of exact proxy triangles. The walk tests those triangles instead of the column height. This includes the vertex's own cell, with a bias along the normal (route B's a + b x texel, here in metres).

| Store | Size | Tag |
|---|---|---|
| Height grid, 1 B a cell, step H / 255 (5.1 cm at 13 m; 11 cm at 28 m) | 64 x 64 = 4,096 B covers 96 m x 96 m at 1.5 m | DERIVED |
| Overhang cells' triangle lists | about 1,130 static proxy triangles in the whole caster budget (`TOWN_STORY.md` §4); only the overhang ones are listed; 36 B a triangle | ESTIMATE, not counted |

**Back-facing vertices need no ray.** A vertex with N · s <= 0 is in its own shadow. That product is already needed for the lighting, so the test is free. In a town of boxes a large share of the wall vertices face away at any sun (INF; the share is not counted).

**The ground is not cached per vertex.** The demo's ground is a polar grid that is re-centred on the eye whenever the eye moves a lattice step (`psm_scene.h`). Its vertices are not the same world points from frame to frame. So the ground's term lives in a world-fixed map of bytes, which each ground vertex samples bilinearly:

- 0.5 m cells over 75 x 61 m: 18,300 B (ESTIMATE; the extent is from `TOWN_MOCKUP.md`).
- The map can be filled by the same rays, one a cell.
- Or it can be filled by SH4DOW-DC's heightfield sweep (`sh4dow_heightfield_sun`, O(w x h), no per-point march). The sweep has the same no-overhang limit.

### 5.4 Variant: occlusion intervals along the day's arc

This adapts Donnelly and Demers (GPU Gems 1, ch. 13) to per-vertex data.

- **The idea.** On the clock the sun's path is fixed: one declination, 9:00 to 16:00. So for each static vertex, store the times on that path when it becomes lit and unlit.
- **Evaluating it.** At clock time t, with a light of time-width dt, the lit fraction for one interval [R, F] is

      V(t) = max(0, min(F, t + dt/2) − max(R, t − dt/2)) / dt

  Several intervals add. The formula is *transcribed* from the chapter's text by Scholar (`GPU_GEMS_NOTES.md`, equation 1); check it against the page before quoting.

What follows is ours.

| Item | Value | Tag |
|---|---|---|
| Storage | 2 intervals x 2 B (quarter-hour units, 1 B each end) = 4 B a vertex; 8,700 static model vertices = 35 KB | ESTIMATE (the interval count per vertex is a guess) |
| Bake | rays at each quarter hour from 9:00 to 16:00 (29 samples) x vertices x C_ray: 29 x 8,700 x 400 cycles = 1.0 x 10^8 cycles = 0.5 s | ESTIMATE |
| Run time on the clock | min / max / subtract a vertex: no rays at all at any S | INF |
| Run time off the clock (sun set by hand: "sun free") | falls back to the sliced refresh of 5.5 | design |

**What it buys.** Along the clock it removes the sun-step refresh entirely, and with it the lag of 5.5. It also gives a soft shadow in time, which reads as a penumbra.

**What it costs.** About 4 x the memory of the 1-byte cache, and a bake that belongs at build time, not at load.

### 5.5 Amortised refresh, and the sun error it allows

**Symbols.**

- N: vertices holding a cached term. k: vertices refreshed a frame. f: frames a second.
- w: the sun vector's angular speed (rad/s).
- h: height of a caster's edge above the receiver (m).

    P = N / (k f)  seconds                 (refresh period; a term's age is uniform on [0, P))

**Sun rate.** Only the hour angle changes, so w = (dHh/dt) cos(dec), where dHh/dt = S x 7.272e-5 rad/s (DERIVED):

| S | w |
|---|---|
| 1 | 7.23e-5 rad/s = 0.00414 °/s |
| 8 | 5.79e-4 rad/s = 0.0332 °/s |
| 60 | 4.34e-3 rad/s = 0.249 °/s |
| by hand | up to 0.524 rad/s = 30 °/s in elevation (121 x the S = 60 rate) |

**Ground error.** A caster edge at height h throws its shadow a distance d = h / tan e. If the sun turns by θ (θ² = de² + cos²e da²), the shadow point moves

    h θ / sin e  <=  |shift|  <=  h θ / sin² e

**Rule for the slice**, for a largest allowed shift ε:

    P <= ε sin² e / (h w),        k >= N h w / (f ε sin² e)

**Worked** (DERIVED; h = 13 m, ε = 0.75 m (half a 1.5 m cell), S = 60, f = 60):

- 14:00 (sin² e = 0.200): P <= 2.65 s, k >= 180 vertices a frame (361 at f = 30).
- 16:00 (sin² e = 0.050): P <= 0.66 s, k >= 719 vertices a frame.

At S = 1 the same ε allows P = 159 s: any slice will do. With the sun driven by hand at 30 °/s, P must be under 0.022 s, about one frame: **a cache cannot follow it.** Either refresh in full over the frames after the trigger is released, or use a coarser term while it is held.

**Order.** Vertex i is refreshed in frame j when i mod (N/k) = j mod (N/k). The stale terms are then scattered, and no front sweeps across the town. ASSUMPTION: the vertex order has no spatial pattern with that period.

### 5.6 Moving casters: analytic proxies

**Symbols.** α = the light's angular radius (the real sun's is 0.00465 rad; an artistic value is larger). Lit = 1 lit, 0 umbra.

**Sphere** (centre C, radius R):

    v = C − p,  t = v · s          (t <= 0: behind, Lit = 1)
    d = sqrt(v · v − t²),  wp = t tan α,  u = (d − R) / wp
    Lit = clamp(1/2 + u/2, 0, 1)                      (linear)
    Lit = 1/2 + (3u − u³)/4 for |u| < 1, else 0 or 1   (smoothstep)

Early outs before the square root:

- If v · v − t² >= (R + wp)², then Lit = 1.
- If v · v − t² <= (R − wp)² and R > wp, then Lit = 0.

**Exact reference** (disc light against disc caster, small angles). Let ρ = asin(R / |v|), and φ be the angle between the two centres. Then Lit = 1 − A / (π α²), where for |α − ρ| < φ < α + ρ

    A = α² acos((φ² + α² − ρ²) / (2φα)) + ρ² acos((φ² + ρ² − α²) / (2φρ))
        − ½ sqrt((−φ + α + ρ)(φ + α − ρ)(φ − α + ρ)(φ + α + ρ))

For a straight edge across the sun (ρ ≫ α) the exact value is 1/2 + (u sqrt(1 − u²) + asin u) / π. Against it, the linear ramp is off by at most 0.058 of full light (at u = ±0.62) and the smoothstep by about 0.040. DERIVED by hand, not checked by a program.

**Capsule** (ends A, B). Let a = A − p and b = B − p, and take their parts across the ray, a′ and b′. Then

    q = clamp(−a′ · (b′ − a′) / |b′ − a′|², 0, 1)
    d = |a′ + q (b′ − a′)|
    t = (a + q (b − a)) · s

Compute u and Lit as for the sphere.

**Box.** Seen along s, a box is a convex hexagon. d_box = max over its 6 edges of the signed distance from the ray. This is exact beside an edge. Outside a corner of angle γ the penumbra is too wide by 1 / sin(γ/2), which is 1.41 at a right angle.

**Combining.** Lit_total = min(Lit_grid, Lit_proxies). Min is never darker than the darkest caster, but too light where two penumbras overlap. The product is too dark where two proxies cover the same part of the sun.

| Pair | Cycles | Tag |
|---|---|---|
| vertex, sphere: early out | 15 to 20 | ESTIMATE, **low** (see below) |
| vertex, sphere: full path (one FSRRA, one divide) | 40 to 50 | ESTIMATE, **low** |
| vertex, capsule | about 2 x sphere | ESTIMATE, **low** |
| vertex, box | 60 to 90 | ESTIMATE, not timed |
| **the pass as run:** 8 capsules (r 0.20 m) and 4 spheres (r 0.15 m), each against the 0.5 m ground points in its 16:00 run, 454 pairs (393 capsule, 61 sphere) | **137.87** with early outs; 187.89 without | MEASURED 2026-11-25, one console, one run (ray1 P9, SH4-055) |

**The pair estimates are too low.** The measured 137.87 cycles a pair is a mix of mostly capsule pairs. It lies above every row of the estimate, including a full capsule path at about twice 40 to 50 (80 to 100). The early outs save 27 % (137.87 against 187.89). The ray bench did not time sphere and capsule pairs apart, so the split between them is OPEN. The kernel gave the reference's bytes; 88 of the 454 points were shadowed, as the host test gives.

**Which vertices.** Only those in a proxy's shadow run are tested: the box swept from the caster to the end of its shadow, of length (caster height) / tan e. A 1.7 m person reaches 3.4 m at 14:00 (the water girl's shadow, `TOWN_STORY.md`) and about 7.4 m at 16:00 (DERIVED). So the cost is Σ_j V_j x C_pair, where V_j is the vertices in proxy j's run, not N x proxies.

**Proxy error.** A proxy that misses the mesh's outline by δ moves the edge by δ: δ / (2 wp) of full light inside the penumbra.

### 5.7 Not sending what the camera cannot see

**Symbols.**

- φ: the fraction not needed (out of view, hidden, or facing away from the sun).
- C_s: the refresh cost of one vertex.
- g: vertices a group; C_g: the cost to test one group's box.
- φ_g: the fraction in groups wholly not needed.

    Per vertex:  cost = N (C_t + (1 − φ) C_s),            pays when φ > C_t / C_s
    Per group:   cost = (N/g) C_g + (1 − φ_g) N C_s,       pays when φ_g > C_g / (g C_s)
    C_g ≈ 30 (1 + occluders) cycles                        (ESTIMATE: 6 view planes, 5 planes an occluder box)

**Worked** (ESTIMATE): N = 28,699, g = 64 (449 groups), 4 occluders: C_g = 150, C_s = 558 (14:00's ray at the console's constants, 5.3; the first draft used 338). Break-even is φ_g = 0.42 %. At φ_g = 0.5 a full refresh saves 8.0 million cycles (40 ms) for 67,350 (0.34 ms). C_g itself is not timed.

**Culling orders the refresh; it does not discard.** The term is in world space, so a culled vertex keeps its byte, and culling only sets the order:

    k = N_vis / (P_v f) + (N − N_vis) / (P_u f),    N_vis = (1 − φ) N

A vertex coming into view shows a value up to P_u old. A camera turning at W rad/s with a horizontal field Th brings about N_vis W / (Th f) new vertices a frame: 2.5 % of N_vis at 90 °/s, 60° and f = 60 (ASSUMPTION: an even spread).

**The same test on the TA's side.** If the a3 SH-4 time is proportional to vertices (ASSUMPTION), skipping a group's whole vertex pass saves φ_g x 157.5 cycles a vertex, that is φ_g x 22.6 ms. A precomputed visible set per 2 m cell of the walk area (built offline by rays from sample eyes) would give φ_g for a table lookup. The frame is near its TA budget, and a TA overflow corrupts the lists (KOS PR #1375), so this matters for safety as well as speed (INF).

### 5.8 Writing the term, and sharpening the edges

**Base tier.** The vertex colour is c = Kd x (amb x cool + dif x (N · s) x Lit x warm), the palette's own form (`TOWN_MOCKUP.md`). The PVR interpolates it. Writing the term costs one load and one multiply or table read a vertex, 3 to 5 cycles (ESTIMATE); it adds no fill and no TA vertices.

**What it cannot do.** Lit is interpolated across a triangle, so an edge inside a triangle comes out as a ramp, and a shadow smaller than a triangle, with every vertex lit, is lost. There are three fixes:

| Fix | How | Risk | Tag |
|---|---|---|---|
| **Cut** receivers into cells | at build: route B's eave bands (about 400 triangles, `TOWN_STORY.md` §4) and the graded ground | TA vertices | ESTIMATE |
| **Overlay, punch-through** (the owner's item d) | draw each edge triangle (vertices that disagree) again on the punch-through list, carrying the shaded colour, vertex alpha = 1 − Lit, reference τ, with KallistiOS's blend | **F22:** the test acts on texture alpha x vertex alpha at ≥ reference, so it works with a texture of constant alpha; how the product rounds is OPEN (PVR-064), which moves τ by up to a code (INF). The copy writes its own colour, it does not multiply, so it must carry the receiver's texture and shade. Whether the depth mode is used for a coplanar copy is open (§4.2, PVR-062). ONE / ZERO must not be used (F10). | test acts (F22, one run); the overlay not built |
| **Overlay, step texture** (ours) | the same copy with u = Lit into a point-sampled 8 x 1 texture (white for u >= τ, the shade colour below), multiplied over the frame (DESTCOLOR x ZERO) on the translucent list. The threshold becomes a texel lookup. The texture coordinate path is the one F4 measured exact to 10 bits. Auto-sort translucent depth is always GreaterEq (DOC §3.7.9.1), so a coplanar copy from the same vertices passes. | coplanar depth equality is INF; CORE 25 double writes on shared edges; fill = the edge triangles' pixels at F8's rate | INF |

**Edge error of either overlay** (Senior's geometry; it holds for any threshold of an interpolated value).

Inside a triangle, Lit is affine in position (ASSUMPTION: it is interpolated in the triangle's plane). So the set Lit = τ is one straight segment. It cuts edge (i, j) at q = (τ − a_i) / (a_j − a_i). With τ = ½:

- **(a) Hard vertex values (0 or 1).** The segment joins the cut edges' midpoints. The error is at most cm/2 along a cut edge, with an RMS of cm / (2 sqrt 3) = 0.29 cm for a randomly placed true edge (cm = mesh edge length).
- **(b) Soft vertex values**, Lit = clamp(½ + D / (2 wp), 0, 1), with D the signed distance to a straight edge:

      error <= max(0, cm/2 − wp)

  A straight edge is placed exactly once wp >= cm/2. The threshold then draws it crisp.
- **(c) A curved true edge** of radius rc: the drawn chord's sagitta is about lc² / (8 rc).

      error <= max(0, cm/2 − wp) + cm² / (4 rc)

  This is second order in cm / rc. It does not hold at a corner, which can be off by up to about cm/2. Example: cm = 1.5 m, wp = 0.75 m, rc = 3 m gives 0.19 m. On screen: error x focal (px) / depth (m).

**Soft tier (item e).** Use wp itself (an artistic α in 5.6), wrap lighting near the terminator, and a ring average of Lit at bake. These give soft edges for no run-time cost, but never a sharp one.

### 5.9 Frame budget and memory

    T_sh4   = T_base + T_slice + T_proxy + T_vis
    T_slice = k (C_setup + n_mean C_step) / F
    T_proxy = (Σ_j V_j) C_pair / F
    T_vis   = G C_g / F
    frame period = ceil(60 max(T_sh4, T_pvr)) / 60   (ASSUMPTION: SH-4 builds frame m while the PVR draws m − 1, locked to the display)

**The model and the a3 HUD disagree.** The model gives a3 a period of 2/60 s (30 fps). The HUD says 25 fps (MEASURED). So the frame loop is not the simple overlap assumed. Candidates: a wait on the PVR, the KOS scheduler tick (§4.3), and frames that miss vblank. Spike c4's live view points the same way: its frames took 39.9 and 49.9 ms for 19 to 27 ms of measured work, in steps 10.0 ms apart (F26, MEASURED). A wait that wakes on the 100 Hz tick fits those numbers (INF, from three frames). 25 fps is a 40 ms period, which a 10 ms step also gives (INF). This is unresolved. Until it is, "room" below is an upper bound.

**Room at 30 fps:** 33.3 − 22.6 = 10.7 ms of SH-4 time (DERIVED). For 60 fps T_sh4 must fall under 16.7 ms, and T_pvr = 16.0 ms just fits.

**Slice from a budget B:** k <= B F / C_ray, with B F = 2 ms x 200.006 MHz = 400,012 cycles.

This was redone on 2026-11-25 with the console's constants. The first draft's figures (338 and 702 cycles a ray, k <= 1,183 and 570, P = 0.81 and 1.68 s) used 5.3's overturned estimate.

The constants:

- C_ray = 72 + 22.1 n: MEASURED, ray1 P1, SH4-052.
- Interleaved, with the vertex transform evicting the grid, a ray costs 89 to 117 cycles more: MEASURED on stand-ins, ray1 P2, SH4-053.
- n: at 14:00, 5.3's table (ESTIMATE). At 16:00, a host count over the town's own ground receivers (ESTIMATE, below); 5.3's n = 48 is kept as a second row for comparison.

P = 28,699 / (k x 30).

| Sun | n | C_ray batched | k at 2 ms | P | C_ray interleaved | k at 2 ms | P | Needed P at S = 60 |
|---|---|---|---|---|---|---|---|---|
| 14:00 | 22 (5.3, ESTIMATE) | 558 | 716 | 1.34 s | 675 (+117) | 592 | 1.62 s | 2.65 s: **inside** |
| 16:00 | **21.5** (host count over the town's ground, ESTIMATE) | 548 | 730 | 1.31 s | 637 (+89) | 628 | 1.52 s | 0.66 s: **outside, by about 2 times** |
| 16:00, 5.3's table | 48 (H = 13 m; see below) | 1,133 | 353 | 2.71 s | 1,222 (+89) | 327 | 2.93 s | 0.66 s: outside, by about 4 times |
| check: P2's stand-ins, 14:00 / 16:00 | 22.08 / 21.47 (host count) | 554.19 / 539.68 (MEASURED) | 721 / 741 (741 printed by the page) | — | — | — | — | not the town's receivers |

Everything in the 14:00 and 16:00 rows is DERIVED from MEASURED constants on the n shown.

**The 16:00 count** (host ESTIMATE, 2026-11-25; `demo/psm/bench/ray_town_n.c`, results in `demo/psm/bench/RAY1_RESULTS_cortana.md`, "Host follow-up"):

- **Method.** The bench's own walk (`ray_walk_ref`, the bytes the console kernel matched) on the town grid. One ray a texel of a 192 x 192 ground map at 0.5 m over the grid's 96 m square.
- **Result.** Mean n 21.54 at 16:00 (lit 28.25, shadowed 8.77, max 98; 34.5 % shadowed), against 21.35 at 14:00. P2's stand-ins gave 22.08 / 21.47, so they were a fair proxy for the ground.
- **Why 21.5 and not 48: the grid's extent.**
  - 5.3's 48 assumes a lit ray climbs to H = 13 m before it is clear.
  - On the town grid H is the tower's 28 m (`TOWN_RAY_H`). At 12.9° a ray needs 122 m to climb that, and the grid is 96 m across.
  - So a lit ray ends at the grid's edge, not at H, and a shadowed ray ends at its first hit.
  - A larger grid, or a world past its edge, would lengthen the lit rays toward 5.3's figure. The tower's shadow already runs off the map at 16:00 (TOWN-062).
  - So 21.5 is a figure for this grid, and the 48 row is the bound for a grid that holds the whole shadow.
- **The model vertices are not in this n.** On the grid, nearly all of them hit their own building's column (5.3, third limit). The ground is the bulk of the rays.

Read from the table:

- **14:00 still fits.** Meeting 2.65 s needs k >= 361 rays a frame: 1.01 ms batched, 1.22 ms interleaved (DERIVED).
- **16:00 does not fit at 2 ms, by about 2 times on this grid.** Meeting 0.66 s needs k >= 1,450 rays a frame: 4.0 ms batched, 4.6 ms interleaved (DERIVED, on the host-ESTIMATE n = 21.5). On 5.3's n = 48 it would be 8.2 and 8.9 ms, most of the 10.7 ms of room at 30 fps.
  - At 2 ms the shift grows with P. If it grows in proportion (ASSUMPTION), the first draft's 1.9 m at 1.68 s becomes about 1.5 m at 1.31 s and 1.7 m interleaved on this grid, or 3.1 to 3.3 m on n = 48, at h = 13 m (ESTIMATE).
- **The first draft's conclusion holds:** at x60 near sunset the interval variant (5.4) or a looser ε is needed. On the town's grid the gap is about half of what n = 48 gives. A console run of the walk over these receivers is the test that would settle it.
- **The proxy pass is cheap beside the slice:** 312 µs a frame at the load places (MEASURED, ray1 P9, SH4-055), 15.6 % of a 2 ms slice. P9 stands. Other places on the walkers' routes are not timed.

| Memory | Size | Tag |
|---|---|---|
| Cached term, 1 B a vertex | 28,699 B = 28.0 KB if every TA vertex held one (upper bound); 4 bits a vertex: 14.0 KB | DERIVED |
| Height grid | 4 KB (64 x 64) | DERIVED |
| Ground map, 0.5 m | 18.3 KB | ESTIMATE |
| Interval variant | about 35 KB | ESTIMATE (5.4) |
| OC-RAM | 8 KB of the 16 KB D$. It holds the grid, not the vertex cache. No faster than a hit; halves the cache for everything else. For the interleaved slice it loses overall (below). | MEASURED (F23, h1; ray1 P2.C and P2.D, SH4-054); DOC for the 16 KB D$ |

**Cache behaviour.**

- A grid of 4 KB is 128 of the 512 D$ lines. The vertex transform streams more than a megabyte a frame and will evict it. So refresh the slice in one batch, and the grid is loaded once a frame.
  - The first draft priced that load at 128 lines x 27.7 cycles = 18 µs. That used the mixed scattered-read figure where a line miss belongs: 128 x 18.41 (SH4-003) = 12 µs for the grid alone (DERIVED).
  - The ray bench measured the whole cold start, grid, points, bytes and vertices purged: 47 µs at 14:00 and 46 µs at 16:00 (cold − warm, MEASURED, SH4-053). That is more than P2's predicted 18 µs, and is the figure to budget; the arrays other than the grid are the rest (ESTIMATE).
- Without the batch, each step that misses costs more. A scattered read from RAM costs 27.7 to 40.3 cycles, loop included (SH4-047: 27.7 is a mixed figure on a 32 KB map, measured in s2 and s3 alike; 40.3 on 512 KB is nearer a read that always misses). Bench h1 measured the bare line: 18.41 cycles a read miss against 3.17 for a hit (F23).
  - Interleaved with the 28,699 PVR vertices, the ray bench's slice cost 117 cycles a ray more at 14:00 and 89 at 16:00 (MEASURED, SH4-053), 16 to 21 % of a batched ray. At 27.7 to 40.3 cycles a miss, that is 2.9 to 4.2 and 2.2 to 3.2 missing reads a ray (ESTIMATE: all the extra taken as grid misses).
- OC-RAM keeps the grid resident at the price of half the D$. Bench h1 measured that price: OC-RAM is no faster than a hit, and 8 KB of ordinary RAM data ran 59 % slower beside it (F23). The ray bench settled it for this slice (SH4-054, MEASURED 2026-11-25, one console, one run):
  - with the grid in OC-RAM the cold start is gone (1 and 0 µs), and the interleaving extra falls to 25 and 11 cycles a ray;
  - but the vertices cost 6.8 % more, so the interleaved slice is slower: 12,027 against 11,893 µs at 14:00, and 11,903 against 11,694 µs at 16:00.
  - **OC-RAM does not pay here; batch the slice instead.**
- Placing matters as much as the cache: two arrays a multiple of 16 KB apart share every cache entry, and the same transform ran 13.16 or 38.67 cycles a vertex by placing alone (F24). The grid, the vertex term and the vertex buffers must not sit at such distances (INF from F24).

**Moving results between chips, for comparison.**

| Path | Rate | 32 KB takes | Tag |
|---|---|---|---|
| PVR-DMA, PVR to RAM | 35.5 MB/s polled; 37.6 MB/s ended by its interrupt | 0.92 ms; 0.87 ms | MEASURED (s2, s3; h2, BUS-021) |
| Store queue, RAM to video memory | 146 MB/s at 32 KB (216–224 at 512 KB) | 0.22 ms | MEASURED (h1, h2; BUS-010, BUS-011). Sega's table gives about 84 MB/s (DOC) |
| ch2-DMA, RAM to video memory | 335 MB/s at 32 KB (498 at 512 KB) | 0.10 ms | MEASURED (h1; BUS-010, BUS-011). Sega's table gives 702.6 MB/s (DOC) |
| SH-4 loop reading video memory | 3.8 MB/s (16-bit reads) | 8.6 ms | MEASURED (s2; h2 the same, 3.7) |

The cached term needs none of these: it never leaves the SH-4's side.

### 5.10 The frame's order

1. Read the pad and the clock.
   - On the clock with the interval variant: evaluate intervals; there is no refresh.
   - Otherwise: if the sun has moved more than the slice rule's ε since the cache's sun, continue the refresh.
2. Refresh one slice (k vertices, visible ones first) into a back copy, in one batch, with the grid loaded once. Swap the copy when it is whole.
3. For each mover, compute its shadow run and its proxy terms.
4. Visibility: look up the eye's cell, then test the groups marked "maybe".
5. Write the visible receivers' colours: min(static, proxy), times N · s and the palette.
6. Send the opaque world. If the overlay is on, send the edge triangles' overlay on the translucent list.

Nothing in this order waits on the PVR (§4.3).

### 5.11 Against the routes we have measured or built

| Route | Self-shadow | Per-frame cost we know | Precision | Standing |
|---|---|---|---|---|
| A: PVR silhouette into a target, multiply layer on the ground | no | the target pass (not measured, F19) + one translucent layer over the lit ground (F8: 3.1 ms a full screen) | per pixel, from a per-vertex lookup that is exact only for a uniform map | built; the a3 HUD frame is route A |
| B: SH-4 depth raster in RAM, per-vertex compare | yes | raster 12–43 cycles a texel (F14, MODEL) + compare 184–489 cycles a vertex (F13, MODEL) | 16-bit depth; edge at vertex spacing | built; **never timed on a console** (F18) |
| AB: PVR draws the depth code, PVR-DMA back, SH-4 compares | yes | 0.93 ms copy at 128 x 128 (F1) + the compare + the PVR pass (F19) | 10 bits exact, 16 within one code (F4); one frame late | spikes s2 / s3; not integrated |
| C1: blend compare in a mask target | yes | about 11 layers: 11.7 ms at 320 x 240 (ESTIMATE from F8's slope), plus laying the mask over the screen | 8 bits exact with a +1 bias; 6 bits from a PVR-drawn map (F7) | spike c1 |
| Bump unit as a subtract (spike c3b) | yes | not timed in c3b; no twiddle needed on this console (F20) | 8-bit map, ±1 code, Q one value a polygon (F20) | ran once (F20) |
| ARGB1555 write threshold (spike c3b) | — | a 1-bit compare at write-out, no extra layer | exact at 1, 2, 128 (F20) | ran once (F20) |
| Bump-compare chain (spike c4: PVR map, bump compare, 1555 mask, layer) | yes | at 320 x 240, bands 2 codes wide, 1,288 vertices: PVR map pass 3.3 ms, mask pass 6.1 ms, screen pass 9.6 ms with the layer against 3.2 ms without; SH-4 band cut 1.3 ms, send 0.44 ms (MEASURED, one run, `compare/NOTES.md` c4) | map equal to the model in 98–99 % of pixels, the rest 1 code; acne 7 % of lit pixels at b0 1.5, 0 on the ground at b0 3 (MEASURED) | ran twice; frame period OPEN (F26) |
| Modifier volumes | yes, on what the volume encloses | one volume: no cost seen (F12); at town scale not known (F21) | exact per pixel | **excluded by the owner**; context only |
| **This proposal: cached per-vertex term** | yes | refresh slice B (chosen, for example 2 ms) + proxies + a 3–5 cycle write a vertex; overlay fill on edge triangles only | edge at vertex spacing (per pixel on the overlay's line); sun lag P (5.5), none on the clock with intervals (5.4) | design only |

**Spike c4's cost by band width** (sun 150 / 26, 320 x 240 mask; MEASURED 2026-11-25, one console, one run; `compare/NOTES.md` c4, test 5; PVR-100, SH4-045). A band is a strip of receiver of one depth code, because Q is one value a polygon (F20).

| Band width (codes) | Bands | Vertices | Light fit, us | Band cut (SH-4), us | Mask send, us | PVR map pass, us | PVR mask pass, us |
|---|---|---|---|---|---|---|---|
| 1 | 616 | 2,464 | 54 | 2,483 | 820 | 3,312 | 9,054 |
| 2 | 322 | 1,288 | 53 | 1,332 | 440 | 3,311 | 6,132 |
| 4 | 173 | 692 | 53 | 744 | 251 | 3,311 | 5,015 |

The screen pass took 3,161 us alone and 9,646 us with the shadow layer. The map pass is the same at every width, near the 3.15 ms floor of a pass in this set-up (F9). The SH-4's cut and send scale with the band count. The compare itself is not exact to the cosine level table: 443 of 40,960 code pairs disagree at window 32, 784 at window 16, 240 at window 64 (F20, PVR-096); the acne in c4's scenes (about 7 % of lit pixels at b0 1.5) has the same size. A cached per-vertex term pays none of these passes; its cost is in 5.9.

## 6. Predictions and how to falsify them

The benches:

- **h1 and h2** are Senior's chip benches (`demo/psm/bench/`, `bench/NOTES.md`). Both ran on 2026-11-25, one console, one run each. They measured the cache, the transform, the TA, uploads and DMA (F23 to F25). They did not time a ray step, a slice or a proxy. That was left to the **ray bench** (ray1), which ran on 2026-11-25, one console, one run (SH4-051 to SH4-055). It timed rays and slices on the town grid with stand-in receivers, and the proxy pass at the walkers' load places. Its verdicts are in the table: P1 overturned, P2 holds in form, P9 stands.
- **Spike c3** is the subtract spike (`compare/sub_spike.c`). It ran once as image c3b (F20). **Spike c4** is the bump-compare chain; it ran twice, and its test 6 is the punch-through test (F22).
- **"Console"** means a later test image, not yet named.

| # | Prediction | Falsified if | Test | Bench | Evidence so far |
|---|---|---|---|---|---|
| P1 | C_step is 12 to 16 cycles and C_setup about 30, with the grid in cache (5.3) | the console's cycles a ray, over n, fall well outside that | rays of known n on a 4 KB grid, timed with the KOS ns timer and performance counter 1 (the s2 clock pair) | ray bench | **OVERTURNED** (MEASURED 2026-11-25, one console, one run; **SH4-052**): fits give C_step 22.07 to 22.21 and C_setup 70.88 to 74.09 on four sets of rows. The ns column is not used: it disagrees with the cycles on two rows (SH4-051) |
| P2 | One batch a frame keeps the grid resident: a slice costs k x C_ray plus at most 18 µs of loads (5.9) | the slice's cost grows with misses (27.7 to 40.3 cycles each, SH4-047) beyond that | the same slice batched, then interleaved with the vertex transform, then with the grid in OC-RAM | ray bench | **Holds in form; the load figure was low** (MEASURED 2026-11-25, one console, one run; **SH4-053**, **SH4-054**):<br>- the batched slice is k x C_ray to within 1.5 % at the stand-ins' host-counted n;<br>- the cold start was 46 to 47 µs, not at most 18;<br>- interleaving added 89 to 117 cycles a ray, so batching is right;<br>- OC-RAM made the interleaved slice slower overall |
| P3 | At 14:00 and S = 60, a 2 ms slice keeps the shadow within 0.75 m of truth; at 16:00 it does not (5.9 gives 1.9 m) | a capture at x60 shows the lag outside those bounds | the measured slice rate put into 5.5's rule, then captures at x60 at 14:00 and 16:00 | console | none |
| P4 | On the clock, the interval variant matches a fresh ray cast within a quarter hour of sun travel (5.4) | edges differ by more than one quarter hour's shift (5.5 with θ = w x 900 s) | host first (deterministic), then the console's bytes | host; console | none |
| P5 | A per-vertex term with cut receivers agrees with route B's `lit_plane` on the same vertices and sun | the share that differs exceeds a threshold fixed before the run | the two compared on the host over the town's receivers | host | none |
| P6 | The step-texture overlay draws a two-valued edge on the line Lit = ½, within 5.8's error bound, on a coplanar copy | gaps or double darkening on shared edges (CORE 25), or the copy fails the depth test | a 32 x 32 triangle grid with a known Lit field, read back, pixels counted | console (new) | none. c4's second pass at equal depth left 0 holes (`compare/NOTES.md` c4), a sign for the coplanar copy, not a test of it |
| P7 | The punch-through overlay does not threshold vertex alpha | a run where the test acts on vertex alpha at a set reference | texel alpha alone, then vertex alpha alone | spike c4 test 6 | **OVERTURNED** (MEASURED 2026-11-25, one console, one run; **PVR-064**): with vertex alpha 128 the 4444 ramp drew 2,048 of 32,768 pixels at reference 128, so the tested alpha is texture x vertex. F10, on which P7 rested, used blend ONE / ZERO, which writes the failing pixels too (PVR-061). The rounding of the product is OPEN |
| P8 | Group culling pays above φ_g = 0.7 %, and saves about φ_g x 22.6 ms of the a3 frame's vertex work (5.7) | the a3 view's visible share is near 1, or the saving is not proportional | vertices sent with and without, counted on the host and on the console's HUD | host; console | none |
| P9 | Proxies for 8 people and 4 hens cost less than one slice (5.6) | their cost a frame exceeds k x C_ray | the proxy pass timed at 16:00 (the longest runs) with every walker in view | ray bench | **Stands** (MEASURED 2026-11-25, one console, one run, load places only; **SH4-055**): 312 µs a pass, 15.6 % of a 2 ms slice. The pair cost, 137.87 cycles, is above 5.6's estimates |
| P10 | The frame model of 5.9 explains the a3 HUD | the HUD's 25 fps stays unexplained by max(T_sh4, T_pvr) | the frame's waits (render-done, vblank, the KOS tick) timed on the console | console | against, in another program: c4's frames are 39.9 / 49.9 ms for 19–27 ms of work (F26, PVR-094, cause OPEN) |

## 7. Threats to validity and what we do not know

| Threat | Why it matters |
|---|---|
| Every MEASURED row is from one console and one run (c4's PVR-DMA at 512 KB: two runs) | No spread, no second unit, no other cable or video mode |
| The a3 photo is not filed; its buffer percentage is an ESTIMATE; its 25 fps contradicts the frame model (5.9) | The budget's "room" is an upper bound |
| Every cycle count in section 5 is an ESTIMATE from latencies held from memory, or MODEL with cache hits assumed | P1, P2 and P9 exist to replace them |
| Senior's tables use H = 13 m; the tower reaches 28 m | Rays toward the tower cost about twice the table (5.3) |
| A height grid has no overhangs and self-hits wall vertices | Eave shadows on walls need the exact overhang lists (5.3); without them the method loses route B's main gain |
| Route B has never been timed on a console (F18) | 5.11's comparison lacks its baseline |
| The PVR's render time for a town caster pass is not known (F19); c4 timed only its own small passes (5.11) | The overlay's cost is an extrapolation from F8 |
| Punch-through: the rule is settled on our console (alpha ≥ reference, texture x vertex; F22), but the product's rounding and the depth mode for a coplanar copy are not | The punch-through overlay (5.8) may place its edge up to a code off τ, and may fail or pass its depth test; P6's grid test would show both |
| Spike c2 (volumes at town scale) is built and has not run; c3b ran once and c4 twice | The volumes row of 5.11 is context, not data |
| The frame is set by waiting, not work, in c4 (F26), and the cause is not known | Any frame-rate figure here (5.9) can be off by a whole 10 ms step |
| The PVR's Gouraud precision is not documented | The base tier's darkest shade and its banding are unknown |
| The interval formula (5.4) is transcribed, not checked against the page | Check it before building on it |
| Proxy fidelity | 1,130 static proxy triangles are not the drawn mesh; a shadow can disagree with the geometry it seems to fall from |
| Static means static | A door that opens or cloth that sways must be a mover, or it is wrong |
| A hand-driven sun (30 °/s) cannot be followed by any slice (5.5) | It needs a full refresh after the trigger is released, or a coarse term while it is held |

## 8. Conclusion

The routes measured so far rebuild a shadow map every frame and pay for it every frame:

- the SH-4's raster (route B);
- a copy back (route AB: 0.93 ms at 128 x 128, measured);
- about 11 translucent layers (candidate C1).

The proposal stops paying per frame for what does not change:

- a per-vertex visibility byte, from rays through a height grid with exact overhang cells;
- refreshed in slices when the sun moves, or, on the clock, read from per-vertex occlusion intervals with no rays at all;
- moving casters as analytic proxies, only where their shadows reach;
- the term written into the vertex colour at no fill cost.

The estimates in section 5 say three things:

- a whole-scene cast is about 48 ms, so the term must be cached;
- a 2 ms slice follows an x60 sun at 14:00 but not at 16:00;
- culling by groups pays above a 0.7 % hidden share.

The costs are tens of KB, lag when the sun steps off the clock, and fidelity bounded by the proxies and the receivers' vertex spacing.

For edge sharpening there are now two candidates, and one grid test (P6) can try both. The punch-through test does act, on texture alpha x vertex alpha (F22, which overturned P7); its open points are the product's rounding and the depth mode of a coplanar copy. The point-sampled step texture goes through the texture-coordinate path, which we measured exact to 10 bits (F4), and multiplies instead of writing.

None of this is a measured result. Section 6 says what would show it wrong.

## References

Repo files (this repository unless named):

- `demo/psm/ROUTE_AB_NOTES.md`: route AB, PVR-DMA, s2 / s3 results
- `demo/psm/compare/NOTES.md`: results of spikes c1, c3b and c4 (both runs); `compare/vol_spike.c` (spike c2): built, not run
- `demo/psm/bench/NOTES.md`: results of chip benches h1 and h2
- `wiki/hardware/ledger-pvr.md`, `ledger-sh4.md`, `ledger-bus-dma.md`: the ledger IDs cited (PVR-, SH4-, BUS-)
- `demo/psm/PVR_FEATURES.md`: per-pixel operations, Sega's bug list, contradictions, KOS issues
- `demo/psm/PRIOR_ART.md`, `demo/psm/LATEGEN_NOTES.md`, `demo/psm/GPU_GEMS_NOTES.md`: related work
- `demo/psm/TOWN_STORY.md`, `TOWN_MOCKUP.md`, `TOWN_LOOK.md`: the scene, its budget, its palette
- `demo/psm/psm_scene.h`, `psm_send.h`, `psm_town.h`, `psm_dc.c`: buffer sizes, the ground grid, the clock, how the HUD's times are taken
- `README.md`, "The psm effect"
- `third_party/sh4zam-dce/KERNEL_GUIDE.md`: the cycle model and the compare and raster kernels
- `sh4dow-dc/include/sh4dow_heightfield.h`: the heightfield sweep
- `dc-halo/README.md`, `dc-halo/third_party/JBrooksBSI/NOTICE.md`, `dc-halo/docs/archive/campaign/CAMPAIGN_PRESENTATION.md` (sibling repo)

External:

- Sega, *Dreamcast/Dev.Box System Architecture* (1999): https://segaretro.org/images/7/78/DreamcastDevBoxSystemArchitecture.pdf
- Sega, *SH4 access* (1999): https://www.segaretro.org/images/3/3f/SH4_access990312_e.pdf
- Sega, *Holly TA&CORE* notes: https://segaretro.org/images/8/82/Holly_TA%26CORE_e.pdf
- KallistiOS: https://github.com/KallistiOS/KallistiOS (issues and PRs #1594, #1506, #696, #1375, #1366)
- Comstedt, PowerVR notes: http://mc.pp.se/dc/pvr.html
- libretro, BeetleDC OIT post (2018): https://www.libretro.com/index.php/introducing-reicast-oit-libretro-core-updated-reicast-regular-core/
- Sauer, GameCube shading (2002): https://www.gamedeveloper.com/programming/shader-integration-merging-shading-technologies-on-the-nintendo-gamecube
- Godbolt, SWAT PS2: https://old-dog.xania.org/201003/swat-ps2-renderer
- Game Watch, Shadow of the Colossus (2005): https://game.watch.impress.co.jp/docs/20051207/3dwa.htm
- NVIDIA, GPU Gems 1–3, free online edition (cited by chapter and section): https://developer.nvidia.com/gpugems/
