---
title: Ledger: SH-4 processor
tags: [hardware, ledger, sh4]
updated: 2026-11-25
sources: [demo/psm/bench/NOTES.md, demo/psm/SHZ_TRIG_PROBE.md, third_party/sh4zam-dce/README.md]
---
# Ledger: SH-4 processor

**What you will learn:** what our console runs measured about the SH-4's cache, transforms and copies, and which earlier numbers were withdrawn.
**For:** a programmer deciding how to write a hot loop, and an agent checking a speed claim.

Each row is one fact; the status legend is in [STRUCTURE.md](../STRUCTURE.md).

**Where the numbers come from.**

- **h1**: chip bench `bench_h1.c`. One run on one console, 2026-11-25. Read from photos `logs/benchmarks/PXL_20261125_054414414.jpg` to `..054608113.jpg`. Write-up: `demo/psm/bench/NOTES.md` "Hardware result h1".
- **h2**: chip bench `bench_h2.c`. One run on the same console, 2026-11-25. Read from 14 photos, `logs/benchmarks/psm-bench-h2/PXL_20261125_072052571.jpg` to `..072218093.jpg`. Write-up: `demo/psm/bench/NOTES.md` "Hardware result h2".

Neither run was repeated, so no number has a spread. A *line* is 32 bytes. *MB* is a million bytes.

> **The placing trap.** The SH-4's operand cache is direct-mapped and 16 KB. Two buffers a multiple of 16 KB apart share every cache entry, and a copy or a transform between them slows by up to 8.5 times. h1's buffers were exactly 1 MB apart, so six of its lines measured the placing, not the routine. h2 corrected them (SH4-020 to SH4-024).

![The placing trap: the SH-4 operand cache is 16 KB, direct-mapped, indexed by address bits 13..5, so two arrays a multiple of 16 KB apart put line i of each in the same entry and every access misses; measured on one console, mat_transform 13.16 against 38.67 cycles a vertex (1 MB apart), pvr_project 21.15 against 132.5 (256 KB apart in c4), memcpy 398.0 against 46.6 MB/s; the fix is one pool with the second array 8 KB off modulo 16 KB (SH4-002, 003, 020-023, 030-032, 046).](../images/cache-placing-trap.svg)

The ray bench, which times the grid with and without the vertex transform evicting it:

![The ray bench (image psm-bench-ray1 built and host-tested, not run on a console; every cycle count ESTIMATE): a ray walks a 64 x 64, 4 KB height grid toward the sun at a predicted 12 to 16 cycles a step plus about 30 (P1, timed against n with the ns timer and performance counter 1); the same slice is run batched (grid loaded once, 18 to 26 us), interleaved with the vertex transform (27.7 to 40.3 cycles a missing step, SH4-047; the miss share ESTIMATE) and with the grid in OC-RAM, which h1 measured as no faster than a hit while halving the cache (P2, SH4-010 to 012); and proxies for 8 people and 4 hens are timed at 16:00 against one slice (P9). No ray, slice or proxy has been timed on a console yet.](../images/ray-bench-plan.svg)


## Clock and cache

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| SH4-001 | The KOS ns timer's least step is 560 ns (640 ns in spikes s2 and s3). 1 ms is 200,176 to 200,186 CPU cycles. | MEASURED | h1, h2; s2, s3 (2026-11-24), one console | NOTES.md:15; h2 table; `demo/psm/ROUTE_AB_NOTES.md:202` | bench h1, h2 | every timing |
| SH4-002 | The I-cache is 8 KB and the operand cache 16 KB. | FACT | Sega SysArc §2.2.1, Table 2-4 | `demo/psm/OFFLOAD_PLAN.md:9` | — | budgets |
| SH4-003 | An operand-cache hit costs 3.17 cycles a line. A read miss costs 18.41. A write miss costs 41.91. | MEASURED | h1, one console | NOTES.md:16-18 | bench h1 | hot loops |
| SH4-004 | Allocating a line and then writing it (MOVCA) costs 16.24 cycles a line, against 41.91 for a write miss. | MEASURED | h1 | NOTES.md:19 | bench h1 | psm raster clear |
| SH4-005 | Writing back 8 KB of dirty lines costs 12.91 cycles a line. Purging 8 KB of clean lines costs 3.85. | MEASURED | h1 | NOTES.md:20-21 | bench h1 | — |
| SH4-006 | After the store queue rewrote 8 KB of video memory behind the cache, 238 of 256 lines read back stale. After `dcache_inval_range`, none did. | MEASURED | h2, one console | NOTES.md h2 group 2; photo `..072149994` | bench h2 | cached video memory reads |
| SH4-007 | `dcache_inval_range` over 512 KB takes 492 us. | MEASURED | h2 | same | bench h2 | same |

## Operand cache as RAM

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| SH4-010 | OC-RAM mode at 0x7c001000 holds 8,192 bytes. CCR read 00000105 before and 00000125 after. The area repeats every 8 KB. | MEASURED | h1 | NOTES.md:94-98 | bench h1 | — |
| SH4-011 | OC-RAM is no faster than a cache hit: 20,000 random reads took 1,401 us, against 1,446 us for 8 KB of RAM in the cache. | MEASURED | h1 | NOTES.md:93, 96 | bench h1 | — |
| SH4-012 | With OC-RAM on, the same 8 KB of ordinary RAM took 2,299 us (59 % more), because the cache for RAM halves to 8 KB. | MEASURED | h1 | NOTES.md:96 | bench h1 | — |

## Maths units

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| SH4-015 | FIPR and FTRV gave bit-identical results for all 16,384 four-term sums. | MEASURED | h2, one console | NOTES.md h2 "Rounding" | bench h2 | psm headers |
| SH4-016 | FIPR and FTRV were nearer the exact sum than the compiler's plain sum: 2,421 results not the nearest float against 6,511. Worst error 1.24 units against 1.71 (a unit is 2^-24 of the sum of the four absolute products). | MEASURED | h2 | same | bench h2 | psm headers |
| SH4-017 | Cycles a point, 256 points in the cache: plain C 72.43, FIPR 38.10, FTRV 16.58 (matrix already in XMTRX). | MEASURED | h2 | same | bench h2 | [SH4ZAM rule](../methods/sh4zam-rule.md) |
| SH4-018 | Loading XMTRX: 15.01 cycles by `shz_xmtrx_load_4x4`, 18.05 by KOS `mat_load`. | MEASURED | h2 | NOTES.md h2 "The transform" | bench h2 | — |
| SH4-019 | SH4ZAM's software back end on the host: `shz_atan2f` worst 0.58°, `shz_asinf` 0.28°, `shz_dce_atan2f` 0.0007°. The console's own back end is unchecked. | MEASURED (host only, not a console) | 2,001 points | `demo/psm/SHZ_TRIG_PROBE.md:29-38` | the probe on a console | sun angles |

## Transforms

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| SH4-020 | KOS `mat_transform`, 256 vertices in the cache, clean placing: 13.16 cycles a vertex. | MEASURED | h2 | NOTES.md h2 "The transform" | bench h2 | vertex paths |
| SH4-021 | The same on h1's placing (buffers 1 MB apart): 38.67 cycles. h1's own "38.70" was this placing. | MEASURED | h2; h1 | NOTES.md:32-37, 118-120 | bench h2 | — |
| SH4-022 | A C loop of SH4ZAM's `shz_xmtrx_transform_vec4` and `shz_invf_fsrra`: 28.67 cycles a vertex in the cache, 57.30 out of it. `mat_transform` is 2.18 times quicker in the cache (43.77 out of it). The loop waits out each FTRV and FSRRA. Results agree to 2 ulp. | MEASURED | h2 | NOTES.md h2 "The transform" | bench h2 | [SH4ZAM rule](../methods/sh4zam-rule.md) |
| SH4-023 | `shz_dce_pvr_project`: 21.15 cycles a vertex against 43.19 for its C form, bytes the same. Out of the cache: 58.09 against 74.07. The pipeline model had said 19.5 and 39. | MEASURED | h2 | same | bench h2 | sh4zam-dce |
| SH4-024 | Plain C 4×4 with a divide: 80.93 cycles a vertex. Taken on h1's bad placing; not taken again. | MEASURED (on the bad placing; withdrawn as a routine's cost) | h1 | NOTES.md:26, 118-120 | rerun on a clean placing | — |
| SH4-025 | The town demo's SH-4 frame is 157.5 cycles a vertex. | REPORTED (lead's figures, not on the h1 photos) | — | NOTES.md:121 | — | budgets |
| SH4-045 | Spike c4's SH-4 work by band width (sun 150°/26°), bands 1 / 2 / 4 codes wide (2,464 / 1,288 / 692 vertices): light fit 54 / 53 / 53 us; band cut 2,483 / 1,332 / 744 us; sending the mask's two passes 820 / 440 / 251 us; the map's 68 vertices 13 to 15 us. The PVR side is PVR-100. | MEASURED | c4 second run, pages 3, 4, one console, 2026-11-25 | compare/NOTES.md:310 | c4 test 5 | bump compare chain |
| SH4-046 | **A placing case, not the routine's cost.** In the c4 program `shz_dce_pvr_project` took 132.5 / 132.3 / 115.7 cycles a vertex against 138.8 / 138.3 / 137.5 for its C form (bytes the same), because its source array (8c205220) and output (8c1c5220) are exactly 256 KB apart, a multiple of the 16 KB operand cache: every source line and its output line share a cache entry. Cite SH4-023 (21.15 in the cache, 58.09 out) for the routine. c4b must place the arrays apart. | MEASURED (times); FACT (addresses, `sh-elf-nm` on `build/psm_cmp/psm-c4-c4.elf`) | c4 second run, pages 3, 4 | compare/NOTES.md:312-313, 340, 367-371 | rerun c4b with the arrays placed apart | the placing trap (SH4-031) |
| SH4-047 | **A scattered read from RAM, loop included,** by arithmetic on SH4-042 and SH4-043: 2,765 us / 20,000 = 138 ns = 27.7 cycles a read on a 32 KB map (2,776 us in s3: 27.8); 3,760 us = 188 ns = 37.6 cycles on a 128 KB map; 4,026 us = 201 ns = 40.3 cycles on a 512 KB map. The bare line miss is 18.41 cycles (SH4-003). The 32 KB map is twice the operand cache, so some of its reads may hit (ESTIMATE): 27.7 is a mixed figure, and 40.3 is nearer a read that always misses. The thesis's "27.7 cycles a missing step" (5.9) uses the mixed figure, and cites s3 at one place and s2 at another; both give 27.7-27.8. | MEASURED (times; spikes s2 and s3, 2026-11-24, one console); cycles a read by arithmetic (SH4-001: 1 ms = 200,176-200,186 cycles); hit share ESTIMATE | SH4-042, SH4-043 | `demo/psm/ROUTE_AB_NOTES.md:244-245, 365-369`; `demo/psm/THESIS_SH4_SHADOWS.md:116, 202, 461` | ray bench P2: the slice interleaved with the vertex transform, and the same reads on a map placed in the cache | the ray bench, thesis 5.9 |

## Copies, RAM to RAM

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| SH4-030 | 1 MB copy, clean placing (destination 8 KB off): `memcpy` 63.4 MB/s; 32-bit loop 63.4; `shz_memcpy` 131.6; `shz_memcpy8` 86.9; `shz_memcpy32` 130.6; `sq_cpy` 149.7; `shz_sq_memcpy32` 150.1. | MEASURED | h2 | NOTES.md h2 group 1, line 3 | bench h2 | every copy |
| SH4-031 | The same with the destination exactly 1 MB off: `memcpy` 12.8 MB/s; 32-bit loop 13.2; `shz_memcpy32` 130.9. | MEASURED | h2 | same | bench h2 | the placing trap |
| SH4-032 | 4 KB in the cache: `memcpy` 398.0 MB/s at 8 KB off, 46.6 at 1 MB off (8.5 times). `shz_memcpy` 589.3 and 129.8. | MEASURED | h2 | same | bench h2 | the placing trap |
| SH4-033 | `shz_memcpy32` does not care about the placing: it reads a whole line into registers and allocates the destination line without reading it. | ESTIMATE (reading of its source and SH4-030/031) | — | NOTES.md h2 "Copies" | — | — |
| SH4-034 | h1's "memcpy 1 MB at 43.2 MB/s" matches neither h2 placing. | OPEN | h1: 43.2; h2: 63.4 and 12.8 | NOTES.md h2 "Copies" | rerun h1's exact placing | — |
| SH4-035 | Twiddling a 128 × 128 16-bit texture (32 KB): 12,580 us by a bit loop, 1,504 us by a table, same result. | MEASURED | h1 | NOTES.md:28 | bench h1 | texture builds |

## Reading video memory with the SH-4

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| SH4-040 | From the uncached address (0xA4…): 32-bit loop 7.5 MB/s; `memcpy` 7.2-7.5; `shz_memcpy32` 7.0-7.1; `shz_memcpy8` 6.7-6.8; 16-bit loop 3.7. Same at 32 KB, 128 KB and 512 KB. | MEASURED | h2 | NOTES.md h2 group 1, line 6 | bench h2 | frame read |
| SH4-041 | Through the operand cache (0x84…): `shz_memcpy32` 38.6-38.7 MB/s; 32-bit loop 29.6-30.3; with prefetch 30.2-30.9. All read back right. | MEASURED | h2 | NOTES.md h2 group 2 | bench h2 | route AB, frame read |
| SH4-042 | 20,000 scattered 2-byte reads: 12,903 us from video memory, 2,765 us from a RAM copy. | MEASURED | spike s2, 2026-11-24, one console | `demo/psm/ROUTE_AB_NOTES.md:244-245` | spike s2 | route AB |
| SH4-043 | 20,000 random 2-byte reads from a RAM copy: 2,776 us (32 KB map), 3,760 (128 KB), 4,026 (512 KB). Every map is larger than the operand cache. | MEASURED | spike s3, 2026-11-24 | ROUTE_AB_NOTES.md:365-368 | spike s3 | route AB compare |
| SH4-044 | Sega's access table: an SH-4 read of texture memory takes 41 bus clocks for 2 or 4 bytes, 61 for 32. A write takes 12 or 38. | FACT | Sega SH4 access table, 1999, p. 1 | ROUTE_AB_NOTES.md:105-111 | — | — |

## Open

Also in [open-questions.md](../open-questions.md):

- SH4-019: run the trig probe on a console.
- SH4-024: plain C against `mat_transform` on a clean placing.
- SH4-034: h1's 43.2 MB/s memcpy.
- A second console and a second run for every row.
