---
title: Errata
tags: [errata, pvr, sh4]
updated: 2026-11-25
sources: [demo/psm/PVR_FEATURES.md, demo/psm/POLLY2_NOTES.md, demo/psm/compare/NOTES.md, demo/psm/ROUTE_AB_NOTES.md, demo/psm/bench/NOTES.md]
---
# Errata

**What you will learn:** where Sega's documents, KallistiOS, outside sources and our own earlier notes disagree with each other or with the console, and which rule we follow.
**For:** anyone who has read a hardware fact somewhere and found the console does something else.

Each entry names the conflict, what our console did, and the rule this project follows. Ledger IDs point to the full rows.

## Documents against the console

| # | Subject | The conflict | Our console | Rule we follow | Source |
|---|---|---|---|---|---|
| E1 | Punch-through alpha test | Sega §3.4.3 says alpha ≥ reference; §3.7.9.2 says only alpha 1.0. skmp: ≥ reference, and a failing pixel is still blended. | ≥ reference (PVR-060, c3b and c4). With KallistiOS's header (SRC_ALPHA / INV_SRC_ALPHA) half a ramp drew; with blend ONE/ZERO all of it (PVR-061, c4 test 6): that was c1 and c3b's "every pixel drawn". The tested alpha is texture × vertex (PVR-064). One run. | Treat it as ≥ reference on texture × vertex alpha. Keep KallistiOS's blend; never ONE/ZERO in that list. | PVR_FEATURES.md:49, 138; POLLY2_NOTES.md (2) |
| E2 | Punch-through depth mode | Sega §3.4.3 says GreaterEq; §3.7.9.1 says LessEq. skmp: the mode does nothing in that list. | Not contradicted: depth ALWAYS and depth write off each drew the same as KallistiOS's header (PVR-062, c4 test 6, one run). One layer on an empty screen cannot tell "ignored" from "harmless"; a test with an occluder would. | Do not rely on the punch-through depth mode. | PVR_FEATURES.md:42; POLLY2_NOTES.md (2) |
| E3 | Non-twiddled bump texture (Sega CORE 26) | Sega §3.6.4 allows it; §9 CORE 26 says it hangs. skmp: it hangs only with bilinear filtering. | Point-sampled, no hang (PVR-070, one run). | Non-twiddled bump maps with point sampling only, never bilinear. | PVR_FEATURES.md:122, 137; POLLY2_NOTES.md (6) |
| E4 | Bump shading | Sega §3.7.9.1 says flat only; its Fig. 3-21 shows Gouraud. skmp: the light is never interpolated. | Not tested. | A Gouraud base with bump is unknown. | PVR_FEATURES.md:53, 140 |
| E5 | Bump level curve | Sega gives a sine and cosine formula. skmp measured tables. | Six of seven c3b levels match skmp's rule; one is a 6-bit step low (PVR-045, PVR-047). c4: the compare disagrees with a cosine table in 443 of 40,960 pairs at window 32 (PVR-096), and that is the acne c4's model missed (PVR-099). c4b measured the window at boot: differences 99 to 157, 2 codes narrower than the cosine at each end (skmp's reading gave 1 to 3); Q made from it gave 0 acne on the ground at every b0 (PVR-102, PVR-103). | Use our console's numbers: measure the window edges at boot. Never copy skmp's tables. | POLLY2_NOTES.md (1) |
| E6 | PVR-DMA rate and direction | Sega: 51.4 MB/s, and only SB_PDDIR = 0 documented. | 38 MB/s; SB_PDDIR = 1 copies video memory to RAM (BUS-020, BUS-021). | Plan 0.87 ms for 32 KB. | PVR_FEATURES.md:141 |
| E7 | G2 / AICA DMA rate | Sega: 11.3 and 14.4 MB/s. | 5.7 and 5.0 MB/s (AICA-020, AICA-021). | Plan with the console's rate. | OFFLOAD_PLAN.md:45 |
| E8 | Held pixels (Sega CORE 10/11/15) | Sega lists missing pixel writes. | The last 32 bytes of a render land at the next render (PVR-021); the symptoms differ from Sega's. | Render 2 rows taller. | PVR_FEATURES.md:124 |
| E9 | Render target height | The TA addresses 2,048 × 480 (REPORTED, a KOS thread). | A KOS 512-row target renders only to row 479 (PVR-026). | Maps at most 480 rows. | PVR_FEATURES.md:143 |
| E10 | Register read-back (Sega "Register 1") | Registers, palette RAM and the fog table cannot be read back correctly. | The punch-through reference read c0 after 192 was written (PVR-063). | Keep a RAM copy; never read back. | PVR_FEATURES.md:130 |
| E11 | Sound RAM → texture memory by G2 DMA | A KOS comment says the protection register admits system memory only. | It worked: 32 KB, every byte right (AICA-023). | Allowed, at the G2 rate. | bench/NOTES.md h2 group 4 |

## KallistiOS

| # | Subject | What to know | Source |
|---|---|---|---|
| K1 | "PVR DMA" event | KOS's `ASIC_EVT_PVR_DMA` is the ch2 DMA's end. PVR-DMA's own end is event 0x000b, which KOS does not name (BUS-026). | ROUTE_AB_NOTES.md:46-54 |
| K2 | `pvr_dma.c` | Its header says "to/from VRAM"; the code has no download (BUS-027). | ROUTE_AB_NOTES.md:31-36 |
| K3 | Render-done wait | It is quantised by the 100 Hz scheduler tick. Use `pvr_get_stats` (PVR-011). In c4b the render-done waits ran 2.9 to 9.1 ms past the renders and took 36 of a 39 ms frame (PVR-104). c4g proved it with a polled wait: the tick is the whole cause, and polling made the frame 42 % shorter (PVR-117, K8). | ROUTE_AB_NOTES.md:373-379; SUITE_RUN1_RESULTS_senior.md:103-124 |
| K4 | Render target format | A target takes the main frame's pack mode (KOS issue #1506). | PVR_FEATURES.md:162 |
| K5 | Fog scale | KOS packs the fog scale with a 7-bit mantissa; our top fog code read 251, not 255. The cut fits; it is not proven. | compare/NOTES.md:155-160 |
| K6 | Open KOS issues seen | TA overflow corrupting lists (PR #1375), list continuation (PR #1594), presort mode (PR #1366, closed). | PVR_FEATURES.md:156-167 |
| K7 | TA error events (buffer or bin overflow) | KallistiOS (568c3404) hooks the PVR's error events 0x0200 to 0x0205 only when built with `PVR_RENDER_DBG`, and then only logs them: it drops nothing and sets no flag a program can read (`pvr_init_shutdown.c:138-149`). A full parameter buffer passes silently. Count the events yourself (the town's hud1 image does). | `demo/psm/TOWN_NOTES.md` TOWN-093 |
| K8 | `pvr_wait_render_done` sleeps | The thread waiting for a render is woken only at the scheduler's next 100 Hz tick: the wait returned 2.9 to 9.9 ms after the render's interrupt, so **a sleeping render-done wait costs up to 10 ms a pass**. Polling the render-done event returned 7 to 8 us after it, and the frame fell from 39.9 to 23.3 ms (PVR-117, one run). This is KallistiOS's behaviour; nothing in KallistiOS is changed: poll in our own code. | SUITE_RUN1_RESULTS_senior.md:103-124 |

## Our own notes, corrected

| # | What was written | What is true now | Source |
|---|---|---|---|
| O1 | h1: "memcpy 43.2 MB/s", "mat_transform 38.7 cycles a vertex", "plain C 80.9" | h1's buffers were 1 MB apart and shared every cache entry. On a clean placing: memcpy 63.4 MB/s, mat_transform 13.16 cycles. Plain C not retaken. (SH4-020, SH4-021, SH4-024, SH4-030) | bench/NOTES.md:32-37, 113-120 |
| O2 | h1: "PVR-DMA's real rate is 55 MB/s or more; polling holds it down" | Withdrawn. Ended by its interrupt it is 38.1 MB/s; polling costs about 7 %. One quick run is unexplained (BUS-022, BUS-023). | bench/NOTES.md:158-162 |
| O3 | `src/sh4der_warp.c` once quoted "264 ns a vertex by pvr_prim, 144 in one store-queue run" as the warp's gain | That was h1's TA test, not the warp. In h2 test 7 the pixelate pass gained by runs (241 against 366 ns) but the mesh lost (973 against 914 ns). The source now picks per pass: `SH4DER_WARP_MESH_SEND_PRIM` 1, `SH4DER_WARP_PIXEL_SEND_PRIM` 0 (`src/sh4der_warp.c:50-71`). Why the mesh loses is OPEN (BUS-004, Q9). | bench/NOTES.md h2 "The warp" |
| O4 | `THESIS_SH4_SHADOWS.md` F20, F21 and line 498: c3 "built, not run"; h1 "has not run" | c3b and h1 have run. The thesis text is stale on those lines. | compare/NOTES.md; bench/NOTES.md |
| O5 | `THESIS_SH4_SHADOWS.md:45-65` and `TOWN_NOTES.md` TOWN-001: town a3, 28,699 TA vertices, SH-4 22.6 ms, PVR 16.0 ms, 25 fps, logged MEASURED | Kept as **MEASURED by relay**, with the photo's file name OPEN until the owner gives it (lead's ruling, 2026-11-25; Q55). It is a different build and view from the filed photos, which read route A 13.3-13.6 ms SH-4, 15.9 ms PVR, about 27,000 TA vertices, 18 fps; so the two do not contradict each other. See [scenes](../limits/scenes.md). | logs/benchmarks/older; TOWN_NOTES.md:11, 20 |
| O6 | Town triangle totals | `TOWN_STORY.md` says about 13,900; `TOWN_MOCKUP.md` about 13,200; first view 7,864, 7,823 or 7,900. All are targets; none is measured. | TOWN_STORY.md:190-221; TOWN_MOCKUP.md:39 |
| O7 | `PVR_FEATURES.md` item 26: CORE 26 "forces a twiddle" | Corrected by POLLY2_NOTES: it forbids bilinear on a non-twiddled bump map; point sampling is fine. | POLLY2_NOTES.md (6) |
| O8 | A brief said the AICA core is ARMv4T | It is ARM7DI, architecture ARMv3 (AICA-002). | ARM7_TOOLCHAIN_NOTES.md:18-27 |
| O9 | `wiki/glossary.md` first draft | It swapped routes A and B. Fixed: route A is the PVR-drawn map; route B is the SH-4 depth map. | this wiki |
| O10 | `THESIS_SH4_SHADOWS.md` 5.3 and P1: a grid step costs 12-16 cycles, a ray's set-up about 30 (ESTIMATE) | **Overturned** by ray1 (MEASURED, one console, one run): about 22 cycles a step and 71 to 74 to set up, on both suns and both grids (SH4-052). Every figure built on 5.3 is low by 1.4 to 2.5 times. ray2 repeated it: eight more fits, 22.09 to 22.20 and 70.99 to 76.04 (SH4-059). | RAY1_RESULTS_cortana.md:61-64; RAY1_RESULTS_junior.md:40-41 |
| O11 | `THESIS_SH4_SHADOWS.md` 5.9: 338 cycles a ray at 14:00 and 702 at 16:00; k ≤ 1,183 and ≤ 570 in a 2 ms slice; a cold grid costs at most 18 us; a missing step 27.7 cycles | Rest on 5.3's constants: P1.4's fit at the thesis's mean n 22 gives 560 cycles, and ray1's slice measured 554 (SH4-053). Cold start measured 46-47 us, not ≤ 18, and 47 us again in ray2 (0 with the grid in OC-RAM; SH4-062, SH4-063). 27.7 is a mixed hit and miss figure; 40.3 on a map that does not fit (SH4-047). The 16:00 figure is untested: ray1's stand-in receivers cost less at 16:00 than at 14:00 (Q63). The k figures need redoing. | RAY1_RESULTS_cortana.md:80-88, 164-169 |
| O12 | `THESIS_SH4_SHADOWS.md` 5.6: a proxy test costs 15-20 cycles (sphere, early out) to about 100 (capsule, full) | ray1 P9 measured 137.87 cycles a pair with early outs, 187.89 without (SH4-055); ray2 137.80 and 184.43 (SH4-064). P9 itself stands (312 us, under a 2 ms slice). | RAY1_RESULTS_cortana.md:101-106 |
| O13 | SH4-001: the KOS ns timer's least step is 560 ns (640 ns in s2, s3 and ray1's CLOCK.1) | That figure is the spacing of two calls in the reading loop: one call costs 92 cycles, 460 ns (ray2 CLOCK.3). The timer counts 80 ns (KallistiOS source). Separately, the ns clock jumps 2,502,321 ns at each reload, about once a second (SH4-056, SH4-057). | RAY2_RESULTS_cortana.md:39-51 |
| O14 | `TOWN_NOTES.md` TOWN-082 (the bake's apply under 0.001 ms a frame), TOWN-063 (50 cycles a ray-triangle test) and TOWN-087 (a bake of 28 to 53 ms) | **Overturned** by bake1 on a console (SH4-070, one run): the bake took 152 ms, about 179 cycles a test (DERIVED with the host's test count), and route B's receivers part rose 5.1 to 5.4 ms with the bake (the apply, or layout: Q72). | RESULTS_suite_cortana.md:181-194 |
