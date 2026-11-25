---
title: Benchmarking on a real console
tags: [method, bench]
updated: 2026-11-25
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

## Traps that have given wrong numbers here

1. **Two buffers a multiple of 16 KB apart.** The SH-4's data cache is 16 KB and direct-mapped. h1's source and destination were exactly 1 MB apart, so every line fought for the same cache entry. Its memcpy and transform lines measured the placing, not the routines. h2 moved the destination 8 KB off: memcpy of 4 KB in the cache went from 46.6 to 398.0 MB/s, and `mat_transform` from 38.67 to 13.16 cycles a vertex (MEASURED, one console, 2026-11-25; NOTES.md h1 correction and h2). Put copy and transform buffers half a cache apart, or at an offset that shares no entry.

   ![The placing trap: the SH-4 operand cache is 16 KB, direct-mapped, indexed by address bits 13..5, so two arrays a multiple of 16 KB apart put line i of each in the same entry and every access misses; measured on one console, mat_transform 13.16 against 38.67 cycles a vertex (1 MB apart), pvr_project 21.15 against 132.5 (256 KB apart in c4), memcpy 398.0 against 46.6 MB/s; the fix is one pool with the second array 8 KB off modulo 16 KB (SH4-002, 003, 020-023, 030-032, 046).](../images/cache-placing-trap.svg)

2. **Starting the clock after a DMA starts.** h1's "beside a burst DMA" lines timed one batch after the copy was already over, and measured nothing. Start the clock before the start call. h2 did, and showed that a burst holds the SH-4 for the whole copy (NOTES.md h1 readings, h2 "The DMAC").
3. **Polling a status register in a tight loop.** It shares the bus with what you are timing. Polled all the time, a 512 KB PVR-DMA took 14,775 us; ended by its interrupt, 13,742 us, about 7 % less (MEASURED, one console, h1 and h2).
4. **A timer round a whole PVR pass reads the PVR's queue, not the pass.** KallistiOS wakes the waiting thread on a 100 Hz scheduler tick, so a wait is quantised to 10 ms steps (ROUTE_AB_NOTES.md:373-378). Report the render time from `pvr_get_stats` and your two waits apart (`AGENTS.md`, pl4nar cost table).
5. **Work inside the timed region that is not the thing timed.** Route AB spike s3's "send 3,111 us" included a sine, a cosine and a divide per vertex. It is not a send cost (ROUTE_AB_NOTES.md:380). Build the data outside the timing.

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
