---
title: "Ledger: buses, DMA and the TA send"
tags: [hardware, ledger, bus, dma]
updated: 2026-11-25
sources: [demo/psm/bench/NOTES.md, demo/psm/ROUTE_AB_NOTES.md, demo/psm/OFFLOAD_PLAN.md, demo/psm/PVR_FEATURES.md]
---
# Ledger: buses, DMA and the TA send

**What you will learn:** how fast data moves between main RAM, video memory, the tile accelerator and sound RAM, and how much of the SH-4 each route leaves free.
**For:** a programmer sending vertices, uploading textures or reading a render back.

Each row is one fact; the status legend is in [STRUCTURE.md](../STRUCTURE.md). Runs h1 and h2 and spikes s2 and s3 are described in the [SH-4 ledger](ledger-sh4.md). All are one console, one run.

## Sending vertices to the TA

The [TA](../glossary.md) takes 32-byte vertices from the SH-4.

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| BUS-001 | `pvr_prim` one vertex at a time: 260 ns a vertex, 122.8 MB/s (h2); 264 ns (h1). | MEASURED | h1, h2 | NOTES.md:64; h2 group 1, line 4 | bench h1, h2 | every draw |
| BUS-002 | KOS direct render one vertex at a time: 258 ns a vertex. | MEASURED | h1 | NOTES.md:65 | bench h1 | every draw |
| BUS-003 | One store-queue run (`pvr_sq_load` or `shz_sq_memcpy32`): 142 ns a vertex, 223.9 MB/s (h2); 144 ns (h1). The two routines match to a microsecond. | MEASURED | h1, h2 | NOTES.md:66; h2 line 4 | bench h1, h2 | warp, prepared vertex buffers |
| BUS-004 | The warp's pixelate pass (1,200 vertices) gained from runs: 241 ns a vertex against 366 (290 against 440 us). Its mesh (2,460 vertices, 82 a call) lost: 973 ns against 914 (2,394 against 2,250 us), 6 % slower; why is OPEN. `src/sh4der_warp.c:50-71` now defaults the mesh to `pvr_prim` and the pixelate pass to runs. | MEASURED | h2 test 7 | NOTES.md h2 "The warp" | bench h2 | [warp](../effects/warp.md) |
| BUS-005 | 28,699 vertices would take 4.1 ms in one store-queue run against 7.5 ms one at a time. | ESTIMATE (arithmetic on BUS-001 and BUS-003) | — | NOTES.md:138-139 | bench B0 (town view, both ways) | budgets |
| BUS-006 | The TA's own DMA vertex mode. | OPEN | never measured | NOTES.md h2 "Not measured" | — | — |

## Uploads to video memory

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| BUS-010 | 32 KB upload: store queue 146 MB/s (h1, `pvr_txr_load` 145.5 in h2; `shz_sq_memcpy32` 110.1, one sample, not explained); channel 2 DMA 335 MB/s (h1, set-up included). | MEASURED | h1, h2 | NOTES.md:37-38; h2 line 5 | bench | textures |
| BUS-011 | 512 KB upload: store queue 216-224 MB/s; channel 2 DMA 498 MB/s. | MEASURED | h1, h2 | NOTES.md:39-40; h2 line 5 | bench | textures |
| BUS-012 | Sega gives 702.6 MB/s for ch2 DMA to the TA FIFO (64-bit area) and 84 MB/s for the store queue. Our store queue beat Sega's figure. | FACT (Sega's table) | Sega SH4 access table | NOTES.md:118-119; `PVR_FEATURES.md:187` | — | — |

## PVR-DMA: video memory back to RAM

PVR-DMA is Holly's own DMA block (registers at 0x005F7C00), not KOS's channel 2 DMA.

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| BUS-020 | PVR-DMA copies video memory → RAM with SB_PDDIR = 1. It works through both the 64-bit (0x04…) and 32-bit (0x05…) areas, with 0 bytes wrong in 512 KB. | MEASURED | spike s2, 2026-11-24 | ROUTE_AB_NOTES.md:231-235 | spike s2 | route AB |
| BUS-021 | Rate ended by its interrupt: 32 KB 869 us (37.6 MB/s), 128 KB 3,461 us (37.8), 512 KB 13,742 us (38.1). | MEASURED | h2 | NOTES.md h2 line 6 | bench h2 | route AB |
| BUS-050 | In spike c4's program, 512 KB by its interrupt took 14,510 us (36.1 MB/s), copy right: 5 % slower than h2's 13,742 us. The second c4 run gave 14,516 us (BUS-051), so within c4 it repeats; the gap to h2 is between two programs. | MEASURED | c4, 2026-11-25, one console, photo `logs/benchmarks/c4e/PXL_20261125_074541020.jpg` | compare/NOTES.md:264 | repeat both in one image | route AB |
| BUS-051 | c4 test 7, second run, PVR-DMA from video memory, every copy right. 128 KB: 4,009 us with its end polled each 1 ms, 3,526 us polled each 250 us, 3,630 us by interrupt (37.2 and 36.1 MB/s for the last two). 512 KB: 14,028 us, 13,852 us, and 14,516 us by interrupt (37.4, 37.8, 36.1 MB/s). A polled time is rounded up to the next poll, so the 1 ms line (32.7 MB/s) is a bound, not a rate. The interrupt route was the slowest of the three at 512 KB, so polling is not what held h1's rate down (that reading stays withdrawn). Rates here are bytes / us, MB = 10^6 bytes. | MEASURED (one run; 512 KB by interrupt two runs with BUS-050) | c4 second run, pages 5, 6, one console, 2026-11-25 | compare/NOTES.md:318, 347 | c4 test 7 | route AB |
| BUS-052 | c4b test 7, PVR-DMA from video memory, every copy right. 128 KB: 4,011 us polled each 1 ms (32.6 MB/s), 3,537 us each 250 us (37.0), 3,388 us by interrupt (38.6). 512 KB: 14,040 us (37.3), 14,148 us (37.0), 13,478 us by interrupt (38.9). The polled times repeat BUS-051 within 0.3 % at 128 KB and 2 % at 512 KB; the interrupt route moved 7 % between runs, fastest here and slowest in c4. | MEASURED | c4b `logs/benchmarks/c4b/PXL_20261125_134813979.jpg`, `134817460` | `demo/psm/compare/NOTES.md:448` | — | route AB |
| BUS-022 | Polling its status all the time costs about 7 % (14,775 us for 512 KB, h1). s2/s3's 35.4-35.9 MB/s were polled. | MEASURED | h1, s2, s3 | NOTES.md:72, 158-162 | — | — |
| BUS-023 | One h1 run beside a loop that missed the cache took 9,595 us (54.6 MB/s). Seen once; why is not known. The earlier reading "the real rate is 55 MB/s" is withdrawn. | MEASURED once; cause OPEN | h1 | NOTES.md:79-80, 158-162 | repeat that run | — |
| BUS-024 | Sega gives 51.4 MB/s ("simulation optimum") and documents only SB_PDDIR = 0. | DISPUTED (Sega's document against our console) | Sega SH4 access table p. 2; SysArc §2.6.9 | `PVR_FEATURES.md:141`; ROUTE_AB_NOTES.md:25 | — | — |
| BUS-025 | Against the best SH-4 reads: the uncached SH-4 is 5.0 times slower; the 16-bit loop 10.1 times; the cached `shz_memcpy32` the same within 2 % (with the invalidate). | MEASURED | h2 | NOTES.md h2 "Video memory to RAM" | bench h2 | [SH-4 ledger](ledger-sh4.md) |
| BUS-026 | PVR-DMA's end interrupt is ISTNRM bit 11 (event 0x000b). KOS has no name for it; KOS's `ASIC_EVT_PVR_DMA` (0x0013) is the ch2 DMA's. | FACT (KOS `asic.h`) + REPORTED (emulator, not evidence, for the bit) | our spike c4 uses 0x000b | ROUTE_AB_NOTES.md:46-54 | — | route AB |
| BUS-027 | KOS's `pvr_dma.c` has no download; SB_PDDIR is never written. | FACT | KOS source | ROUTE_AB_NOTES.md:31-36 | — | — |
| BUS-028 | DMAOR reads 0x8201 as the boot ROM left it; CHCR0 reads 0x000853C3 (burst bit set). | MEASURED | s2, h1 | ROUTE_AB_NOTES.md:233; NOTES.md:73 | — | — |
| BUS-029 | Can PVR-DMA and ch2 DMA run at the same time? | OPEN | s2 worked with the boot DMAOR and no channel set-up of ours | ROUTE_AB_NOTES.md:70-71 | run both at once | — |

## SH-4 DMAC channels 1 and 3 (RAM to RAM)

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| BUS-030 | Burst, 512 KB: ch1 4,339 us (120.8 MB/s), ch3 4,328 us (121.1). Cycle steal: ch1 5,445 us (96.2), ch3 5,428 us (96.5). | MEASURED | h1 | NOTES.md:74-77 | bench h1 | big copies |
| BUS-031 | **A burst holds the SH-4 for the whole copy.** The call that starts it came back after 4,325 us, with the copy all but done. | MEASURED | h2 | NOTES.md h2 "The DMAC" | bench h2 | — |
| BUS-032 | Cycle steal gave the call back after 537 us, finished 4,236 us later, and left a register loop 84 % of its speed (h2). In h1, 90 % for a register loop and 45 % for a loop that missed the cache. | MEASURED | h1, h2 | NOTES.md:83-84; h2 | bench | background copies |
| BUS-033 | A PVR render of 8 layers does not slow the SH-4 (99-100 % of both loops). | MEASURED | h1 | NOTES.md:85-86 | bench h1 | — |

## G2 bus and sound RAM

Details are in the [AICA ledger](ledger-aica.md).

![The sound chip as an offload unit: the SH-4 reaches sound RAM, the ARM7DI, the DSP and the 64 voices only over the G2 bus, measured on one console at 5.7 MB/s in and 4.9 to 5.0 MB/s back (also 5.0 MB/s straight to texture memory), so a 32 KB map costs about 6.5 ms back (ESTIMATE); only rare, small jobs fit: the ARM's timer A as a pacing clock (S2), ADPCM storage for smooth bulk tables (S3), a muted DSP blur in the background (S1, rests on Q59) and the plan's ground-shadow rebake, while ARM wait states (Q28), multiply timing (Q29), the muted DSP (Q59) and the ring base step (Q60) are OPEN.](../images/aica-offload.svg)

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| BUS-040 | G2 DMA: 5.7 MB/s to sound RAM, 5.0 MB/s back, from 4 KB to 512 KB, with the SH-4 at 98-100 %. | MEASURED | h2 | NOTES.md h2 group 3 | bench h2 | sound RAM as storage |
| BUS-041 | Sega gives 11.3 and 14.4 MB/s for AICA DMA ("simulation optimums"), about twice what our console did. | DISPUTED (Sega against our console) | Sega SH4 access table | OFFLOAD_PLAN.md:45 | — | — |
| BUS-042 | The G2 bus is 16 bits at 25 MHz, a 50 MB/s ceiling. | FACT | SysArc §1.3 | OFFLOAD_PLAN.md:46 | — | — |
| BUS-043 | An SH-4 read of the AICA can take 16 us with the CPU bus, root bus and G2 bus all stopped. At most 8 writes in a row, then wait. | FACT | SysArc §4.2.2.3 | OFFLOAD_PLAN.md:49-50 | — | drivers |
| BUS-044 | All DMAs steal cycles. | FACT | SysArc §2.6.1 | OFFLOAD_PLAN.md:53 | — | — |
| BUS-045 | Some hardware revisions lock up when a G2 DMA is requested too soon after the last; Maple and AICA share G2 and can time each other out. | REPORTED (a 2007 kernel mailing-list thread), DISPUTED by another developer | — | OFFLOAD_PLAN.md:54-56 | — | — |

## Open

Also in [open-questions.md](../open-questions.md): BUS-006, BUS-023, BUS-029, and the warp mesh's slower run (BUS-004).
