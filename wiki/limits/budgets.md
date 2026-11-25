---
title: Frame budgets
tags: [limits, budget]
updated: 2026-11-25
sources: [wiki/hardware/ledger-pvr.md, wiki/hardware/ledger-sh4.md, wiki/hardware/ledger-bus-dma.md, demo/psm/psm_send.h]
---
# Frame budgets

**What you will learn:** the measured unit costs to build a frame budget from, and how to add them up without fooling yourself.
**For:** a programmer deciding whether an effect fits in 33.3 ms (30 fps) or 16.7 ms (60 fps).

## How a frame is spent

The SH-4 and the PVR work at the same time. The SH-4 builds and sends frame N+1 while the PVR renders frame N. So a frame costs roughly the **larger** of the two, not their sum, as long as neither waits for the other. Waits do happen. In the town demo, route A read SH-4 13.6 ms and PVR 15.9 ms but ran at 18 fps, about 55 ms a frame (see [scenes](scenes.md)). Where the rest went is not yet known.

Budget each side on its own, then check the frame rate on a console.


A frame can be longer than its work. In the c4 live view most of the frame was waiting. c4b timed each wait (PVR-104, one console, one run): the ready waits took 1 to 40 us; the render-done waits took 36 of a 39 ms frame and ran 2.9 to 9.1 ms past the renders. c4g settled it (PVR-117, one run): KallistiOS's `pvr_wait_render_done` sleeps and wakes only at its 100 Hz tick, so it returned 2.9 to 9.9 ms after the render's interrupt; polled, 7 to 8 us, and the frame fell from 39.9 to 23.3 ms. **Poll the render-done event** (errata K8), and read the PVR's render time from `pvr_get_stats`, not from a timer around the wait. The cycle counter stops while the CPU sleeps (SH4-067).

![The frame waits, c4 and c4b (one console, 2026-11-25): in c4 the frame is 39.9 or 49.9 ms while the SH-4 and PVR work in it adds to 19.3 to 27.1 ms (MEASURED PVR-090, 091, 101, 094); in c4b, over 60 frames, the vblank counts 2 or 3 a frame, a 60 Hz vblank, the frames are 29.9 or 39.9 ms (mean 39.2, not whole vblanks), and the three render-done waits take 36.2 of the 39.2 ms, running 2.9 to 9.1 ms past their renders, while the ready waits take 1 to 40 us and polling them changed nothing (MEASURED); c4g found the cause (Q53 CLOSED, PVR-117, MEASURED one run): KallistiOS's sleeping pvr_wait_render_done wakes on the 100 Hz tick, 4,645 / 2,929 / 9,940 us after the render-done interrupt, so the frame is 4.00 ticks (39,909 us mean), and with both waits polled the frame is 23,264 us, 42 % shorter; still OPEN: polled, about 1 frame in 5 is 26.5 ms (Q69).](../images/frame-wait.svg)

![Where the 16.7 ms goes in the town (route A, one view, one console): the SH-4 takes 13.3 to 13.6 ms and the PVR 15.9 ms (MEASURED, HUD), both under 16.7 ms, yet the frame runs at 18 fps, about 55.6 ms; the PVR splits into the scene 7.1 ms (route B, MEASURED), a map pass about 3.2 ms and a shadow layer about 3.1 ms (ESTIMATE) and 2.5 ms OPEN; the SH-4's transform and send are about 5.6 to 6.7 ms (ESTIMATE from unit costs) and the rest is not broken down; no G2 work is in the frame; the wait that makes the frame long is not timed in the town; in spike c4b (one console, MEASURED) the same kind of wait sits in the three render-done waits, about 36 of 39.2 ms, and c4g found the cause (Q53 CLOSED, PVR-117, MEASURED one run): KallistiOS's sleeping pvr_wait_render_done wakes on the 100 Hz tick, and polled the c4g frame fell from 39.9 to 23.3 ms; the town uses the sleeping wait (FACT, the code), so it may pay up to 10 ms a pass (ESTIMATE); polled, 1 frame in 5 is still about 26.5 ms (OPEN, Q69).](../images/frame-budget-town.svg)

## Unit costs (all MEASURED, one console)

| Cost | Value | Ledger |
|---|---|---|
| PVR: empty 640 × 480 frame | 3.15 ms | PVR-001 |
| PVR: one full-screen translucent layer, 640 × 480 | +3.1 ms each | PVR-002 |
| PVR: one full-screen translucent layer, 320 × 240 target | +0.77 ms each (floor 3.2 ms a pass) | PVR-004 |
| PVR: 256 sorted translucent triangles all in one tile | 6.4 ms | PVR-009 |
| PVR: 1024² texture shrunk 16 times, no mipmaps | 23.5 ms (21 times 1:1) | PVR-008 |
| SH-4 → TA: one vertex by `pvr_prim` | 260 ns | BUS-001 |
| SH-4 → TA: one vertex in a store-queue run | 142 ns | BUS-003 |
| SH-4: `mat_transform`, a vertex in the cache | 13.16 cycles (0.066 us) | SH4-020 |
| SH-4: `mat_transform` with 1/w, out of the cache | 42.86 cycles (two runs, one console) | SH4-050, SH4-065 |
| SH-4: `shz_dce_pvr_project`, a vertex in the cache | 21.15 cycles | SH4-023 |
| SH-4: `shz_dce_pvr_project`, arrays too large for the cache, placed apart | 58.1 to 61.7 cycles | SH4-023, SH4-048 |
| SH-4: a ray through a 64 × 64 height grid in the cache | about 22 cycles a cell + 71 to 76 a ray (two runs, one console) | SH4-052, SH4-059 |
| SH-4: a moving proxy against one receiver, with early outs | 137.80 to 137.87 cycles (two runs, one console) | SH4-055, SH4-064 |
| SH-4: a ray against a triangle, in the cache, early outs | 33.16 cycles prepared for one direction; 62.49 any direction | SH4-066 |
| SH-4: one `timer_ns_gettime64` call | 92 cycles | SH4-056 |
| SH-4: KallistiOS's sleeping render-done wait, past the render | up to 10 ms a pass (polled: 7 to 8 us) | PVR-117 |
| SH-4: the town's static-on-static sun bake | 152 ms a sun | SH4-070 |
| SH-4: one sky AO direction, 1,164 caster triangles, both sides (transform, test, send) | 1,824 us (about 522 ns a vertex sent); one side about 1,278 us | SH4-071, SH4-072 |
| PVR: one sky AO direction into a 256 × 256 target, both sides, union | 1,213 us, 68 KB of TA (one side about 753 us, 34 KB) | PVR-130 |
| SH-4 + PVR: method 2 AO at K 4 in the town | +10.0 ms of frame (SH-4 +9.8, PVR +1.4 mean) | PVR-135, SH4-073 |
| SH-4: copy 1 MB, store queue | 6.98 ms (150 MB/s) | SH4-030 |
| Upload 512 KB texture, ch2 DMA | 1.05 ms | BUS-011 |
| Video memory → RAM, 32 KB, PVR-DMA | 0.87 ms | BUS-021 |
| Sound RAM → RAM, 32 KB, G2 DMA | 6.5 ms | AICA-021 |

At 200 MHz, 1 ms is about 200,000 cycles (SH4-001).

The render-target floor (PVR-004) is not fixed: a target rendered through only the tiles it covers costs far less (PVR-092).

![The tile-list cut: rendering a target through only the tiles it covers took the 128 x 130 map pass from 3,312 to 375 us (20 tiles) and the 320 x 240 mask pass from 6,130 to 3,760 us (80 tiles) with the same picture (MEASURED PVR-092, one console, 2026-11-25); the savings of 2.9 ms and 2.4 ms are arithmetic (ESTIMATE PVR-093).](../images/tile-list-cut.svg)

## Worked example (ESTIMATE)

A 30 fps frame with 20,000 TA vertices and two full-screen translucent effects:

- PVR: 3.15 + 2 × 3.1 = about 9.4 ms of fill, plus the scene's own textures and triangles.
- SH-4 send: 20,000 × 142 ns = 2.8 ms in store-queue runs, or 5.2 ms one at a time.
- SH-4 transform: 20,000 × 13 cycles = 1.3 ms in the best case.

These are sums of measured unit costs, so the total is an ESTIMATE. Only a console run of the real frame is MEASURED.

## The TA's vertex buffer

KOS gives the TA a fixed vertex buffer at `pvr_init`; 1,024 KB in the town demo. A frame that overflows it corrupts the display list (KOS PR #1375, REPORTED). The town demo counts each frame before sending it, and drops far parts ("SHED") once the estimate passes 90 % of the buffer (`demo/psm/psm_send.h:17-21, 45`). That 90 % is a design choice, not a measured limit. Town numbers are in [VID-011](../hardware/ledger-video.md).

## Video memory

8 MB in all, shared by frame buffers, the TA's buffers and textures. The town demo had 4,554 KB free after `pvr_init` and 4,010 KB after load, with two 1,024 KB vertex buffers (VID-012, MEASURED, boot logs). Another port with different settings logged about 1.8 MB (VID-004, REPORTED). The settings decide it, so print `pvr_mem_available()` after load in your own program.

Each effect page lists its own video memory. The largest in this library:

- ca's frame texture: 640 KB at 640 × 480, twice for two textures (VID-001).
- pl4nar's target: 64 KB at 256 × 128 with `pvr_scene_begin_rtt`, 256 KB without (VID-002).

## Related

- [Recommended limits](recommended-limits.md)
- [Scenes](scenes.md)
- [Benchmarking](../methods/benchmarking.md)
