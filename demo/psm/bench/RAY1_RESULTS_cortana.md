# Ray bench ray1: results read from photos (Cortana's share)

For: lead and the ledger owner. Read 2026-11-25 from `I:\マイドライブ\dcdev\sh4der-pvr\logs\benchmarks\ray1`, the 15 files from `PXL_20261125_135219324.jpg` to `PXL_20261125_135258097.jpg` in name order (the range lead named; it holds 15 files, not 17). Image `psm-bench-ray1.cdi`, spec in `NOTES.md` "Ray bench (image ray1)".

**Rules used.**

- Every RESULT line below is MEASURED on 2026-11-25, on one console, in one run, unless it says DERIVED. DERIVED is what the page itself works out from its other lines.
- TEST lines and notes in brackets are headings, not results.
- Where I add arithmetic of my own, the row says "my arithmetic" and gives the lines it came from. A prediction or explanation that is not on the page is tagged ESTIMATE.
- Every value was readable. No value is marked unreadable.
- Line wraps on the screen are joined back together. Each value is typed as it shows in its photo.

## Which page is in which photo

| Photo | Page | Content |
|---|---|---|
| 135219324 | p8/24 | the end of P1.3: n 96, bytes, fit |
| 135220573 | p9/24 | TEST P1.4; P1.4 n 1 to n 64 (the cycles of n 64; its ns is carried onto p10) |
| 135221930 | p10/24 | the ns of n 64, P1.4 n 96, bytes, fit |
| 135223181 | p11/24 | TEST P2.A, P2.A.1 to .6 |
| 135224756 | p12/24 | the P2.A note |
| 135226153 | p13/24 | TEST P2.B, P2.B.1 to .6 |
| 135234384 | p14/24 | the P2.B note |
| 135237020 | p15/24 | TEST P9, P9.1 to the start of P9.6 |
| 135239412 | p16/24 | the end of P9.6 |
| 135242974 | p17/24 | TEST h3, h3.1 to .6 |
| 135246433 | p18/24 | "GROUP 1 END. START runs group 2: OC-RAM, left on." |
| 135250150 | p19/24 | TEST OCRAM, the CCR lines |
| 135252032 | p20/24 | TEST P2.C, P2.C.1 to .6 |
| 135256283 | p21/24 | the P2.C note |
| 135258097 | p22/24 | TEST P2.D, P2.D.1 to .6 (its note is on p23, not in my range) |

**Not in my range:**

- p1 to p7: CLOCK, P1.1, P1.2, and P1.3 from n 1 up to the row before n 96.
- p23 and p24.

No file in my range comes before `135219324` by name. So those pages are in Junior's files, or were not photographed. My files also hold group 2 (OC-RAM, p19 to p22). The note that put group 2 at "1357xx" does not match: see the end of this file.

## P1.3: rays of known n, town grid, 14:00 (end only, p8)

| Claim | Status | Photo | Test | Prediction |
|---|---|---|---|---|
| P1.3 n 96: 0 rays among 65,536, not timed | MEASURED | 135219324 | P1.3 | the host count says lit rays exist only from n 24 up; the host's number at n 96 is not in the notes (OPEN) |
| P1.3 kernel against reference: the same bytes | MEASURED | 135219324 | P1.3 | the host gives the same bytes |
| P1.3 fit over the 2 rows with 64 rays or more: C_step 22.21 cycles, C_setup 70.88 cycles | DERIVED (by the page) from MEASURED rows that are not in my photos | 135219324 | P1.3 | thesis 5.3 ESTIMATE: C_step 12 to 16, C_setup about 30 |

## P1.4: rays of known n, town grid, 16:00 (p9, p10)

| Claim | Status | Photo | Test | Prediction |
|---|---|---|---|---|
| n 1, 2, 4, 8, 12, 16: 0 rays among 65,536 each, not timed | MEASURED | 135220573 | P1.4 | matches the host: on the town grid lit rays only exist from n 24 up (NOTES.md) |
| n 24 (256 rays): 602.25 cycles a ray, 3011.15 ns | MEASURED | 135220573 | P1.4 | 5.3 ESTIMATE 30 + 24 x (12 to 16) = 318 to 414 |
| n 32 (256 rays): 781.55 cycles a ray, 3907.73 ns | MEASURED | 135220573 | P1.4 | 414 to 542 |
| n 48 (256 rays): 1135.22 cycles a ray, 6286.93 ns | MEASURED | 135220573 | P1.4 | 606 to 798; **see "looks wrong" 1** |
| n 64 (256 rays): 1485.50 cycles a ray, 7427.38 ns | MEASURED | 135220573 (cycles), 135221930 (ns) | P1.4 | 798 to 1,054 |
| n 96: 0 rays among 65,536, not timed | MEASURED | 135221930 | P1.4 | — |
| kernel against reference: the same bytes | MEASURED | 135221930 | P1.4 | the host gives the same bytes |
| fit over the 4 rows with 64 rays or more: C_step 22.07 cycles, C_setup 74.09 cycles (C_setup holds the loop's load and store too) | DERIVED (by the page) | 135221930 | P1.4 | 5.3 ESTIMATE: C_step 12 to 16, C_setup about 30 |

**P1 verdict.** C_step 22.07 to 22.21 is 1.38 to 1.85 times the thesis's 12 to 16. C_setup 70.88 to 74.09 is about 2.4 times its 30. P1's falsifier ("the console's cycles a ray, over n, fall well outside that") is met. **P1 is overturned** (MEASURED, one console, one run). Two caveats:

- C_setup includes the loop's own load and store.
- The thesis's 5.9 budget figures (338 cycles at 14:00, 702 at 16:00, k ≤ 1,183 and k ≤ 570) rest on the old constants and need redoing.

My arithmetic, the slope between rows: 22.41 (n 24 to 32), 22.10 (n 32 to 48), 21.89 (n 48 to 64). The line is nearly straight, falling slightly as n grows.

## P2.A and P2.B: a slice of 1,024 stand-in receivers, grid in RAM (p11 to p14)

The page notes: least of 5 runs, 3 for .3 and .4; us = cycles / 200 at F = 200.006 MHz (MEASURED s2). ".1 cold: grid, points, bytes and vertices purged first; P2 predicts .1 − .2 ≤ 18 us".

| Claim | P2.A (14:00) | P2.B (16:00) | Status | Photo | Prediction |
|---|---|---|---|---|---|
| .1 batched, grid cold | 563.36 cycles a ray, slice 2,884 us | 548.78 cycles a ray, slice 2,809 us | MEASURED | A 135223181, B 135226153 | thesis 5.9 ESTIMATE: 338 cycles a ray at 14:00 (n 22), 702 at 16:00 (n 48) |
| .2 batched, grid warm | 554.19, slice 2,837 us | 539.68, slice 2,763 us | MEASURED | as above | as above |
| .3 the 28,699 PVR vertices alone | 58.91 cycles a vertex, 8,453 us | 59.04 cycles a vertex, 8,473 us | MEASURED | as above | `shz_dce_pvr_project` out of the cache 58.09 (SH4-023, MEASURED h2) |
| .4 interleaved | 11,893 us | 11,694 us | MEASURED | as above | .2 + .3 + interference |
| .5 extra, .4 − .3 − .2 | 602 us, 117 cycles a ray | 458 us, 89 cycles a ray | DERIVED (by the page) | as above | "27.7 to 40.3 cycles a missing step" (SH4-047), times the number of missing steps (ESTIMATE) |
| .6 kernel against reference | the same bytes | the same bytes | MEASURED | as above | the host gives the same bytes |
| cold − warm, .1 − .2 | 47 us | 46 us | my arithmetic from .1 and .2 | as above | **P2 predicts ≤ 18 us: exceeded on both** |

My arithmetic, against the predictions:

- **Batched against the thesis.** P2.A's 554 cycles is 1.64 times 5.9's 338. P1.4's fit at the thesis's n 22 gives 74.09 + 22.07 x 22 = 560 cycles, which lies within 1 % of P2.A.2. So the error is in C_step and C_setup, not in the mean n.
- **16:00 is not dearer than 14:00 here.** P2.B (539.68) costs less than P2.A (554.19). The thesis expects 702 against 338. P1.4's fit at n 48 would give 1,133. **See "looks wrong" 2.**
- **The vertices** agree with SH4-023's out-of-cache figure to 1.4 % (58.91 and 59.04 against 58.09).
- **Interleaving.** The extra is 117 cycles a ray at 14:00. At SH4-047's 27.7 to 40.3 cycles a miss, that is the cost of 2.9 to 4.2 missing reads a ray (ESTIMATE: it assumes all the extra is grid misses). At 16:00 it is 89 cycles, 2.2 to 3.2 reads. The extra is 21 % of the batched ray cost at 14:00 and 16 % at 16:00.
- **Cold start.** 47 and 46 us against a predicted 18 or less. 4 KB of grid is 128 lines; at 18.41 cycles a line (SH4-003) that is 12 us. The rest (about 35 us) would be the purged points, bytes and vertices that .1 also starts without (ESTIMATE).

## P9: the proxy pass at 16:00 (p15, p16)

| Claim | Status | Photo | Test | Prediction |
|---|---|---|---|---|
| P9.1 receivers: 454 (people 393, hens 61); 88 shadowed | MEASURED | 135237020 | P9 | host test (ray_test.c): 454 receivers, 88 shadowed. **Matches** |
| P9.2 pass with early outs: 137.87 cycles a pair, 312 us a pass | MEASURED | 135237020 | P9 | thesis 5.6 ESTIMATE: sphere 15 to 20 early out, 40 to 50 full; capsule about twice (so about 30 to 100) |
| P9.3 reference (no early outs): 187.89 cycles a pair | MEASURED | 135237020 | P9 | — |
| P9.4 kernel against reference: the same bytes | MEASURED | 135237020 | P9 | the host gives the same bytes |
| P9.5 the pass is 11.32 % of P2's slice of 1,024 rays at 16:00 (P2.2) | DERIVED (by the page) | 135237020 | P9 | — |
| P9.6 k for a 2 ms slice at P2.2's cost a ray: 741 rays; the pass is 15.64 % of 2 ms | DERIVED (by the page) | 135237020, 135239412 | P9 | "P9 is falsified if the pass costs more than k x C_ray" (that is, 2 ms) |

**P9 verdict.**

- 312 us is under 2 ms, so P9 stands (MEASURED, one console, one run, load places only).
- The cost a pair, 137.87, is above every figure in 5.6, including a full capsule test (about 80 to 100, ESTIMATE). 5.6's per-pair figures are too low.
- The early outs save 27 % (my arithmetic: 137.87 / 187.89).
- My check: 454 pairs x 137.87 / 200 = 313 us, consistent with 312. So "a pair" is one receiver against its own proxy.

## h3: the transform with 1/w (p17)

Cycles a vertex: 256 vectors in the cache, 16,384 out of it (input and output purged first).

| Claim | In cache | Out of cache | Status | Photo | Earlier figure |
|---|---|---|---|---|---|
| h3.1 `mat_transform` | 13.38 | 42.86 | MEASURED | 135242974 | SH4-020 13.16 in the cache; SH4-022 43.77 out (h2) |
| h3.2 batch, with prefetch (`shz_dce_xmtrx_project_point3_batch`) | 14.15 | 45.99 | MEASURED | 135242974 | — |
| h3.3 batch, no prefetch | 14.14 | 48.14 | MEASURED | 135242974 | — |
| h3.4 SH4ZAM C loop | 28.60 | 57.12 | MEASURED | 135242974 | SH4-022 28.67 in, 57.30 out |
| h3.5 batch against C loop: the same bytes; no-prefetch build: the same bytes | — | — | MEASURED | 135242974 | the host gives the same bytes |
| h3.6 `mat_transform` against C loop (x, y, 1/w): max 2 ulp | — | — | MEASURED | 135242974 | SH4-022: 2 ulp |

My arithmetic:

- The h2 figures repeat within 1.7 % in the cache and 2.1 % out of it.
- The prefetch saves 4.5 % out of the cache (48.14 to 45.99) and nothing in it.
- The batch is still 5.8 % slower than `mat_transform` in the cache and 7.3 % slower out of it. **See "looks wrong" 3.**

## Group 2: OC-RAM on (p18 to p22)

| Claim | Status | Photo | Test | Prediction |
|---|---|---|---|---|
| "GROUP 1 END. START runs group 2: OC-RAM, left on." | page text, not a result | 135246433 | — | — |
| CCR before 00000105, CCR now 00000125 | MEASURED (register read) | 135250150 | OCRAM | only bit 5 (ORA, OC-RAM on) is newly set, 0x20; FACT, SH7750 hardware manual, CCR. The page itself says the RAM cache is then 8 KB |

The note under P2.C and P2.D is the same as P2.A's: least of 5 runs, us = cycles / 200; P2 predicts .1 − .2 ≤ 18 us.

| Claim | P2.C (OC-RAM, 14:00) | P2.D (OC-RAM, 16:00) | Status | Photo | Same line, grid in RAM |
|---|---|---|---|---|---|
| .1 batched, grid cold | 559.44, slice 2,864 us | 545.46, slice 2,792 us | MEASURED | C 135252032, D 135258097 | 563.36 / 548.78 |
| .2 batched, grid warm | 559.27, slice 2,863 us | 545.39, slice 2,792 us | MEASURED | as above | 554.19 / 539.68 |
| .3 vertices alone | 62.94 cycles a vertex, 9,032 us | 63.08 cycles a vertex, 9,052 us | MEASURED | as above | 58.91 / 59.04 |
| .4 interleaved | 12,027 us | 11,903 us | MEASURED | as above | 11,893 / 11,694 |
| .5 extra, .4 − .3 − .2 | 131 us, 25 cycles a ray | 59 us, 11 cycles a ray | DERIVED (by the page) | as above | 117 / 89 cycles a ray |
| .6 kernel against reference | the same bytes | the same bytes | MEASURED | as above | the same bytes |
| cold − warm | 1 us | 0 us | my arithmetic | as above | 47 / 46 us; predicted ≤ 18 |

**OC-RAM verdict** (my arithmetic, from MEASURED lines; one console, one run):

- With the grid in OC-RAM, the cold start disappears (1 and 0 us).
- The interleaving extra falls 4.7 to 8 times (117 to 25 and 89 to 11 cycles a ray).
- But with the RAM cache halved to 8 KB, the vertices cost 6.8 % more (+579 and +579 us).
- The ray loop costs 0.9 to 1.1 % more warm.
- **Overall the interleaved slice is slower with OC-RAM:** 12,027 against 11,893 us (+134) at 14:00, and 11,903 against 11,694 us (+209) at 16:00.
- This agrees with h1 (OC-RAM no faster than a cache hit while halving the cache; SH4-010 to SH4-012). OC-RAM only pays where the halved cache does not hurt the rest of the frame.

## Looks wrong, or contradicts the host test

1. **P1.4 n 48: the ns does not match the cycles.**
   - 1135.22 cycles at 200 cycles a us is 5,676 ns, but the page says 6,286.93 ns, 10.8 % more.
   - Every other row agrees to 0.01 %: n 24 3,011.25 against 3,011.15; n 32 3,907.75 against 3,907.73; n 64 7,427.50 against 7,427.38.
   - The value is clear in photo 135220573, so it is not a misreading.
   - Both clocks are read round the same 16 runs (bench_ray.c:191-193, `c0 = cyc(); t0 = ns(); ... dc = cyc() - c0; dt = ns() - t0`). So an interrupt inside the runs would lengthen both alike; it cannot explain the gap.
   - One of the two reads is off by about 2.5 ms over that row's 4,096 rays. The cycles are the likelier to be right: n 48 lies on the straight line through the other three rows (slopes 22.41, 22.10, 21.89). The ns read is then the suspect (ESTIMATE; a non-atomic timer read at a counter rollover would do it, not checked).
   - The fit uses the cycles column, so P1's verdict does not depend on this row's ns. Worth a repeat run of P1.4 before any ns figure from ray1 goes in the ledger.
2. **The slice at 16:00 costs less than at 14:00 (P2.B 539.68 against P2.A 554.19; P2.D against P2.C the same way).**
   - Thesis 5.9 expects a 16:00 ray to cost about twice a 14:00 ray (mean n 48 against 22).
   - The stand-in receivers are not the town's vertices: half on the ground, half at 0 to 13 m, random over the casters' box (NOTES.md).
   - At a low sun more of them would be shadowed early and stop at the first hit. That would bring the mean n down (ESTIMATE).
   - So P2's slice numbers do not test 5.9's 16:00 figure. The real receivers' mean n at 16:00 is still OPEN.
   - Nothing on the page counts the rays' steps. A host count of the mean n of these same 1,024 stand-ins at both suns would settle it without a console.
3. **The SH4ZAM-DCE batch does not beat `mat_transform`** (h3.2 against h3.1: 14.15 against 13.38 in the cache, 45.99 against 42.86 out of it).
   - Its bytes are right, and it is 2.0 times faster than the SH4ZAM C loop in the cache.
   - But under AGENTS.md's rule ("Show a measured gain on a real console for any asm kernel"), it has no measured gain over KOS's own routine.
   - Owner or Senior to decide whether it stays.
4. **The cold-start prediction failed:** 47 us against ≤ 18 at 14:00, 46 against ≤ 18 at 16:00. P2's prediction counted the grid alone. The other purged arrays cost the rest (ESTIMATE).
5. **The photo ranges.**
   - Lead's range "135219324 through 135258097" is 15 files, not 17. It starts at p8, not at CLOCK.
   - Group 2 (OC-RAM) is p19 to p22, in my files at 1352xx, not at 1357xx.
   - Whoever holds p1 to p7 (CLOCK, P1.1, P1.2, the start of P1.3) and p23 to p24 should check that nothing is missing. I did not open files outside my range.

Kernel against reference was "the same bytes" on every page in my range (P1.3, P1.4, P2.A to D, P9, h3). The risk named in NOTES.md, bytes compared on x86 only, did not show on these pages.
