---
title: Benchmarking on a real console
tags: [method, bench]
updated: 2026-11-26
sources: [demo/psm/bench/bench_h1.c, demo/psm/bench/build.py, demo/psm/bench/NOTES.md, AGENTS.md, third_party/dc-forerunner/DREAMCAST_MAGIC.md, third_party/dc-forerunner/docs/PERFORMANCE.md]
---
# Benchmarking on a real console

**What you will learn:** how this project measures, what counts as evidence, and the traps that have already given wrong numbers.
**For:** anyone timing code or hardware for this library, person or agent. Read it before you build a bench image.

## What counts as evidence

- **A real Dreamcast only.** An emulator number is not evidence (`AGENTS.md`). It may show that a build boots, nothing more.
- **Every number comes from the run that prints it.** One run on one console is evidence of that run. Say so: "MEASURED, one console, one run, date, photo file".
- **Mark every number.** MEASURED, ESTIMATE, FACT, REPORTED or OPEN (see [STRUCTURE.md](../STRUCTURE.md)). Never mix measured and estimated numbers in one row.
- **Keep what the console printed apart from what you read into it.** `demo/psm/bench/NOTES.md` has a table for each, and its readings carry their own marks and doubts.
- **A finding that is not in the [hardware ledger](../hardware/README.md) is not kept** (`AGENTS.md`, "Wiki and the hardware ledger").

## How a bench image is built and run

- Build: `python demo/psm/bench/build.py --src bench_h2.c --tag h2`, in a shell with the KallistiOS environment sourced. Step 1 is the ELF (kos-cc), step 2 the disc image (mkdcdisc). The script fetches nothing (build.py:4-19).
- Images go to `demo/psm/out/cdi/` as `psm-bench-<tag>.cdi`. The script refuses to overwrite one; use a new tag each time (build.py:8-9, 16, 81, 98).
- Each test runs in its own stage, so a hang names the test it was in, through the [fault catcher](fault-catcher.md) (bench_h1.c:5-16).
- Results go to serial ("h1:") and to result pages on screen. The pages come in groups, with START to go on, so finished work can be photographed before the riskier tests run.
- The bench writes only RAM, video memory and registers. Nothing outlives a power cycle, and every wait has a limit (bench_h1.c:18-19).
- Times come from `timer_ns_gettime64` and the SH-4's cycle counter. The first lines print the timer's resolution (560 ns step in h1 and h2) (NOTES.md:15).
- Results are read from photos of the pages. Note the photo file names. Since nothing is repeated, there is no spread to give (NOTES.md:7-9).

The ray bench, laid out test by test as it was planned. It ran twice on 2026-11-25; its results are SH4-051 to SH4-055 ([ray1](../hardware/ledger-sh4.md#ray-bench-run-one-thesis-p1-p2-p9)) and SH4-056 to SH4-066 ([ray2](../hardware/ledger-sh4.md#ray-bench-repeat-and-clock-repeat-of-run-one-the-clock-the-ray-triangle-test)) in the SH-4 ledger:

![The ray bench, run on one console on 2026-11-25 (image psm-bench-ray1, one run, MEASURED; photos in bench/RAY1_RESULTS_cortana.md and _junior.md): a ray walking the 4 KB height grid costs 22.07 to 22.21 cycles a step and 70.88 to 74.09 to set up (the page's fit, P1.1 to P1.4), against the thesis ESTIMATE of 12 to 16 and about 30, so P1 is overturned; the slice of 1,024 stand-in rays costs 563.36 / 548.78 cycles a ray cold and 554.19 / 539.68 warm (14:00 / 16:00) batched, interleaving with the vertex transform adds 117 / 89 cycles a ray (DERIVED; 27.7 to 40.3 cycles a miss, SH4-047, so the miss count is an ESTIMATE), and the grid in OC-RAM makes the interleaved slice slower overall (P2); the movers' proxy pass takes 312 us, under a 2 ms slice, so P9 stands. OPEN: CLOCK.2's cycles a ns, the P1.4 n 48 ns gap, why 16:00 costs less than 14:00 with these stand-ins.](../images/ray-bench-plan.svg)

## Result pages: fewer photos

A bench prints its results as text pages on the console's screen, and the owner photographs each page. A 24-page run is 24 photos and more, and every number is typed in again from a photo. The owner wants smaller text and fewer photos. Designer drew three page layouts for that. They are **mockups with no data: ESTIMATE / OPEN** until a phone-photo test passes. Suite 1 [s1] tested two of their parts on the console (one run): the CRC-32 line works (10 pages typed from photos agree, VID-015), and QR codes decode from a phone photo of the television, all 16 of them, 8 px safe, 6 px safe, 4 px needing a second photo (VID-014).

**Today's pages (FACT, our code).** Every bench and spike draws its page at x 20, y 16, in KallistiOS's 12 × 24 font: 50 columns and 17 rows a page, a row every 24 px (`bench/bench_ray.c:58-59, 104-106`; the same in `bench_h1.c:70-72`, `bench_h2.c:95-97`, `compare/cmp_spike.c:66-68`, `sub_spike.c`, `vol_spike.c`). So the text runs from x 20 to 620 and from y 16 to 472. It starts outside title-safe (64 / 48 px in) and outside action-safe (32 / 24 px in), and it ends past both on the right and at the bottom. The two margins are the usual SD overscan margins (REPORTED, a broadcast convention). On a television that overscans, the first and last columns and the last rows can be lost, and no photo shows it.

**The proposed pages:**

- **A graph carries the shape; a few exact numbers carry the values.** Bars, a strip of frame times or a cost-against-n chart show steps and outliers at a glance. The figures that go into the ledger are printed as text beside the graph.
- **A CRC32 line** over the page's numbers, so a number typed in from a photo can be checked.
- **Everything inside title-safe**, in the same 12 × 24 font, so nothing is lost to overscan.
- **QR codes as an extra**, never instead: a page of codes repeats a text page, with an index and a CRC32 over all of them. The module size, the version, the payload format and the encoder are OPEN.

![Mockup, no data: a 640 x 480 console result page for a bench (here a frame-wait test): a header with image, page and test, a bar a pass with the render and the wait past it on one millisecond scale and the exact values beside, a strip of 60 frame times with lines at 16.7, 33.3 and 50 ms, a histogram of vblanks a frame with counts, key numbers as text, MEASURED / ESTIMATE legend slots and a CRC32 line; all inside the title-safe frame (64 / 48 px in) in the 12 x 24 console font; whether a phone photo reads every digit is OPEN.](../images/console-page-bench.svg)

![Mockup, no data: a 640 x 480 console result page for a cost against n (like Ray bench, run one [ray1] P1): measured points as 8 x 8 squares, the fit as a 2 px line and the prediction as a dashed outline only, the plotted values as a table beside the chart, the fit constants and the reference-bytes check as text, a ! on any row whose two clocks disagree, MEASURED / ESTIMATE legend slots and a CRC32 line; all inside the title-safe frame in the 12 x 24 console font; whether a phone photo reads every digit is OPEN.](../images/console-page-curve.svg)

![Mockup, no data: a 640 x 480 console result page of QR codes: a header with image, page and part of the set, six codes dark on a white panel (drawn as version 3, 29 x 29 modules at 4 px, each with a 4-module quiet zone), a text index of which code holds which bytes, the version, error correction and byte counts, and a CRC32 over all payloads in order; the codes are placeholder patterns, the module size, version, payload format and encoder are OPEN, and the page is an extra beside the text page it repeats.](../images/console-page-qr.svg)

**The test that would settle the layouts:** draw one page of each kind with known numbers on the console. Photograph each with the phone as the owner does today, on the owner's television. Then check three things: every digit reads; the CRC32 of the typed-in numbers matches the screen's; and a phone app decodes every QR code. Until then, a layout is not a rule.

## Traps that have given wrong numbers here

1. **Two buffers a multiple of 16 KB apart.** The SH-4's data cache is 16 KB and direct-mapped. h1's source and destination were exactly 1 MB apart, so every line fought for the same cache entry. Its memcpy and transform lines measured the placing, not the routines. h2 moved the destination 8 KB off: memcpy of 4 KB in the cache went from 46.6 to 398.0 MB/s, and `mat_transform` from 38.67 to 13.16 cycles a vertex (MEASURED, one console, 2026-11-25; NOTES.md h1 correction and h2). Put copy and transform buffers half a cache apart, or at an offset that shares no entry.

   ![The placing trap: the SH-4 operand cache is 16 KB, direct-mapped, indexed by address bits 13..5, so two arrays a multiple of 16 KB apart put line i of each in the same entry and every access misses; measured on one console, mat_transform 13.16 against 38.67 cycles a vertex (1 MB apart), pvr_project 21.15 against 132.5 (256 KB apart in Full compare chain [c4]), memcpy 398.0 against 46.6 MB/s; the fix is one pool with the second array 8 KB off modulo 16 KB (SH4-002, 003, 020-023, 030-032, 046).](../images/cache-placing-trap.svg)

2. **Starting the clock after a DMA starts.** h1's "beside a burst DMA" lines timed one batch after the copy was already over, and measured nothing. Start the clock before the start call. h2 did, and showed that a burst holds the SH-4 for the whole copy (NOTES.md h1 readings, h2 "The DMAC").
3. **Polling a status register in a tight loop.** It shares the bus with what you are timing. Polled all the time, a 512 KB PVR-DMA took 14,775 us; ended by its interrupt, 13,742 us, about 7 % less (MEASURED, one console, h1 and h2).
4. **A timer round a whole PVR pass reads the PVR's queue, not the pass.** KallistiOS wakes the waiting thread on a 100 Hz scheduler tick, so a wait is quantised to 10 ms steps (ROUTE_AB_NOTES.md:373-378). Report the render time from `pvr_get_stats` and your two waits apart (`AGENTS.md`, pl4nar cost table).
5. **Work inside the timed region that is not the thing timed.** Route AB spike s3's "send 3,111 us" included a sine, a cosine and a divide per vertex. It is not a send cost (ROUTE_AB_NOTES.md:380). Build the data outside the timing.
6. **Two clocks round the same work that disagree.** Ray bench, run one read the cycle counter and the ns timer round the same runs. Its CLOCK.2 line printed about 1.03 GHz on a stock 200 MHz console, and its ns column disagreed with the cycles on two rows; every other row agreed within 0.01 % (SH4-051). Ray bench, repeat and clock [ray2] found both causes. CLOCK.2 read the cycles after a print (SH4-056). And KallistiOS's ns clock jumps 2,502,321 ns at each reload of its timer, about once a second, so any interval that spans a reload reads 2.5 ms long (SH4-057); both odd rows were 2.4 to 2.5 ms long. **Time with the cycle counter.** When two clocks disagree, trust the one that fits the rest of the page, and repeat the run before using the other.

![Why KallistiOS's ns clock jumps: its timer TMU2 reloads every 12,468,720 counts of 80 ns, 997,497,600 ns, and KallistiOS then adds a whole second, so the ns clock steps ahead 2,502,400 ns once a reload (FACT, the KallistiOS source); Ray bench, repeat and clock's CLOCK.4 measured the step at 2,502,321 ns, one 80 ns count short, and 5,070 us gained over 2.2 s, two steps plus 30 ppm (MEASURED, one console, SH4-057, SH4-058); any interval timed with timer_ns_gettime64 across a reload reads 2.5 ms long, which fits Ray bench, run one's two odd rows, so benches time with the cycle counter.](../images/ns-clock-step.svg)

## Lessons from the porting starter

`third_party/dc-forerunner` (the owner's port starter) gives these rules for measuring (DREAMCAST_MAGIC.md:33-44; docs/PERFORMANCE.md):

- Measure before you fix. The first guess about a slow frame was wrong more often than right.
- Every change states its cost in numbers first: RAM, video memory and sound RAM in KB, frame time in ms.
- Only a log from a real boot is proof. The emulator reads the disc instantly and does not model fill rate.
- Never print to serial every frame. At 115,200 baud, 1 KB costs about 94 ms. Print once per load, scene or event, with a cap.
- Free memory after load is a gate. Log the largest contiguous free block, not just the total.

## Related

- [The SH4ZAM rule](sh4zam-rule.md), [host test first](host-test-first.md), [fault catcher](fault-catcher.md)
- [SH-4 ledger](../hardware/ledger-sh4.md), [Bus and DMA ledger](../hardware/ledger-bus-dma.md), [PVR ledger](../hardware/ledger-pvr.md)
