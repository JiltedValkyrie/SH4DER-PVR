---
title: "When it is too slow: the cut ladder"
tags: [start, learning]
updated: 2026-11-26
sources: [wiki/limits/frame-budget.md, wiki/limits/recommended-limits.md, wiki/methods/benchmarking.md, wiki/hardware/ledger-pvr.md, wiki/hardware/ledger-bus-dma.md, wiki/hardware/ledger-video.md, wiki/effects/pool.md, wiki/effects/spark.md, wiki/effects/tilecut.md, wiki/effects/trail.md, wiki/effects/glow.md, wiki/glossary.md, include/sh4der_pvrx.h, include/sh4der_tilecut.h, include/sh4der_pool.h, wiki/guides/racing-game.md]
---
# When it is too slow: the cut ladder

**What you will learn:** how to find out what makes a frame slow before you cut anything, which setting to cut at each step and what each cut costs in the picture, and which parts to leave alone.
**For:** anyone whose frame misses 60 Hz (or 30 Hz) with this library, whatever the game.

Every number here has a status tag. **MEASURED** means a ledger row names the run. **ADVISED** means a header's or a guide's start value. **ESTIMATE** means arithmetic or a named model. **NOT RUN** and **OPEN** mean nobody has the number yet. A measured and an estimated number never share a cell. [Benchmarking](../methods/benchmarking.md) says what counts as evidence: a real Dreamcast, and no emulator.

## 1. Measure before you cut

Do not cut anything yet. The porting starter's rule is "measure before you fix": a guess about a slow frame was wrong more often than right (`wiki/methods/benchmarking.md:73`).

Take these numbers for the same stage, with no key pressed:

- **Frame time.** The whole frame on the console. Time the work with the cycle counter, not the nanosecond clock. That clock steps forward by about 2.5 ms at each reload, about once a second (`benchmarking.md:67`; [Recommended limits](../limits/recommended-limits.md), line 77). MEASURED (SH4-057).
- **PVR time.** The render time of each pass from `pvr_get_stats`, a KallistiOS call. Report it apart from your waits (`benchmarking.md:63`). This library does not wrap it; its one call is in `include/sh4der_fault.h:528`, for the fault report.
- **SH-4 time.** The frame less the PVR waits. [Frame budget](../limits/frame-budget.md), section 1, "How to read a page" (lines 64 to 70), says what each pair of numbers means.
- **Vertices and TA kilobytes.** The TA is the chip that takes your polygons ([glossary](../glossary.md)). One vertex is one 32-byte store-queue burst (glossary), so vertices × 32 bytes gives the TA bytes. ESTIMATE (arithmetic on that size). A 24-quad spark pool is 96 vertices and 3,104 bytes with its header (`wiki/effects/spark.md:43`).
- **The vertex buffer's peak.** KallistiOS's largest-scene counter, `vtx_buffer_used_max`, sits against the buffer size (`frame-budget.md:179`).

The pvrx layer has a polled render-done wait and TA error counters. Those came from test programs that ran. The owner has run the layer on a console in Copy video memory to RAM (her test; no measured numbers recorded yet): its version is v0.05 (`include/sh4der_pvrx.h:21-22`, `:44`). Use it knowing that.

Quote a segment's mean, not its largest frame. The largest frame moved by up to 10.5 ms in one test while the means agreed within 0.1 ms (`recommended-limits.md:88`, MEASURED).

60 Hz is 16.7 ms a frame, and 30 Hz is 33.3 ms (ESTIMATE, arithmetic; `frame-budget.md:46`).

## 2. The ladder: cheapest loss first

Each step names the setting or the pass, what the picture loses, and the saving with its status. Walk down the ladder until the frame holds. Re-measure after each step.

| Step | Setting or pass | What the picture loses | Saving (status) | Source |
|---|---|---|---|---|
| 1 | Poll the render-done wait where you wait every frame | Nothing | Frame 39.9 to 23.3 ms (MEASURED, PVR-117; repeated in PVR-133). No gain where the wait ran on one frame in six (MEASURED, PVR-136) | `recommended-limits.md:78`; `frame-budget.md:112` |
| 2 | Cut each render-target pass to its own tiles ([Tile-cut pass](../effects/tilecut.md)) | Nothing: the same picture | Pass 3,312 to 375 us (MEASURED, PVR-092); 3,256 to 344 us (MEASURED, PVR-156) | `include/sh4der_tilecut.h:11-17`; `frame-budget.md:100` |
| 3 | Fewer full-screen translucent layers (glow, fog, a flash) | That layer's look | +3.1 ms of PVR each at 640 × 480 (MEASURED, PVR-002) | `recommended-limits.md:63` |
| 4 | Spread translucent quads and fans; never stack them in one tile | Nothing, if spread | 256 triangles in one tile 6.4 ms, spread 82 us (MEASURED, PVR-009) | `include/sh4der_pool.h:15-16`; `frame-budget.md:109` |
| 5 | Trilinear back to bilinear; a sharper mip bias back to none | Smoothness between mip levels | Trilinear +3.27 ms of PVR (ESTIMATE: the difference of two MEASURED stage means, PVR-180) | `frame-budget.md:105-106` |
| 6 | A shorter draw distance; less detail past a distance | Far detail, and far things in a mirror | 110 to 25 ms over several changes at once (frames MEASURED, PVR-160 and PVR-174; the share of each change NOT MEASURED) | `frame-budget.md:92`, `:102` |
| 7 | Fewer per-vertex lookups: look up what needs one | Lookups on the far vertices | 2.0 to 3.0 us a looked-up vertex (ESTIMATE, arithmetic on MEASURED lookup times) | `frame-budget.md:103` |
| 8 | Shadows: a maximum distance, faded out before it | Shadows of far casters | Not measured for this cut. The 40 m and 50 m values are chosen (ADVISED) | `frame-budget.md:117` |
| 9 | Light pools and halos by distance | Lamp light far away | NOT MEASURED (OPEN). Limit by distance is ADVISED | `recommended-limits.md:66`; `include/sh4der_pool.h:100` |
| 10 | A whole effect off (the reflection, the shadow tiles) | That effect | 110 ms on, 69 ms off, an older street build (MEASURED, PVR-160) | `wiki/hardware/ledger-pvr.md:237` |

**Target size is not on this ladder for time.** The mirror pass's PVR time was 3.02 to 3.23 ms at 128 × 64, 256 × 128 and 512 × 256 (MEASURED, PVR-175, some stages with keys pressed; why it is a floor is an ESTIMATE). A smaller target saves video memory, not frame time (`frame-budget.md:78`; `recommended-limits.md:57`).

**Watch the cuts that can cost more.** A tile cut with one of two tables found made the frame 3.05 ms slower (MEASURED, PVR-200). Begin and end each cost SH-4 time: 126 and 130 us in one run (MEASURED, PVR-205). Cut when every table is found, and wait for render done after each pass ([Tile-cut pass](../effects/tilecut.md), `include/sh4der_tilecut.h:14-15`). The refusal for a missing table is in the header but has not run on a console (`include/sh4der_tilecut.h:13`).

## 3. What not to cut first

- **The send path.** Send vertices in store-queue runs: 142 ns a vertex, against 260 ns through one `pvr_prim` call at a time (MEASURED, BUS-003 and BUS-001, `wiki/hardware/ledger-bus-dma.md:20`, `:22`). Do not go back to single calls to save code. Measure your own mesh: the warp's mesh ran 6 % slower by runs (MEASURED, one console), and the reason is OPEN (`recommended-limits.md:31`).
- **Placement.** Keep a copy's source and destination apart. Two arrays a multiple of 16 KB apart share one cache entry and can cost up to 8.5 times (MEASURED; `recommended-limits.md:32`; `benchmarking.md:57`).
- **The HUD and overlays.** Send them early. A full vertex buffer drops the end of the list with no message (`frame-budget.md:115`; PVR-150, MEASURED at one view).
- **Mipmaps.** Keep them on. Turning them off cost +1.43 ms on one pass (MEASURED, PVR-170; `frame-budget.md:104`).
- **The render-done wait.** Do not block in KallistiOS's sleeping wait where it runs every frame (`recommended-limits.md:78`).

## 4. When the send is slow, not the fill

Check which bound holds the frame before you cut ([Frame budget](../limits/frame-budget.md), section 1, lines 64 to 70):

- **SH-4 within a few ms of the frame:** the SH-4 is the bound. Cut vertex work at ladder steps 6 and 7.
- **PVR within a few ms of the frame:** the PVR is the bound. Cut passes and fill (steps 2 to 5).
- **Frame near the SH-4 time plus the PVR time:** the program runs the two one after the other. Overlap them, or cut one (`frame-budget.md:69`).

Sends cost by count. 28,699 vertices take 7.5 ms one at a time and 4.1 ms in store-queue runs (ESTIMATE, arithmetic on BUS-001 and BUS-003; BUS-005, `ledger-bus-dma.md:24`). Check your count, not the example.

A full buffer can also freeze a frame on a bigger scene. On a 1,024 KB buffer the second of ten boot turns froze, after the first ran whole; on 1,536 KB all ten ran whole (MEASURED, PVR-208 and PVR-209, one pair of runs). [Frame budget](../limits/frame-budget.md), line 179, says the same ten turns froze: the sources disagree on the count, and the ledger row is the record here. The freeze's cause is OPEN. Size the buffer from your largest scene (`frame-budget.md:179`).

## Related

- [Frame budget](../limits/frame-budget.md): the practices and the numbers behind this page.
- [Recommended limits](../limits/recommended-limits.md): each limit with its status.
- [When it looks wrong](looks-wrong.md): the picture-side problems.
