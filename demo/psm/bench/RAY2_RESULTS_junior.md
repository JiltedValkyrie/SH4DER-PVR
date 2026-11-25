Pages covered: p19/41 to p41/41 (p18/41 read for context only; it is in the other reader's range).

# Ray bench, image ray2: console results, second half (junior)

Image `demo/psm/out/cdi/psm-bench-ray2.cdi` (sha256 7e668d77…0db1), spec in `NOTES.md` "Ray bench (image ray2)".
Photos: `I:\マイドライブ\dcdev\sh4der-pvr\logs\benchmarks\psm_bench_ray2\PXL_20261125_<name>.jpg`.
Every row below is MEASURED 2026-11-25, one console, from a RESULT line, unless it says DERIVED (printed by the
console from its own measured rows) or ESTIMATE (worked out here). All photos were readable; no value was guessed.
Clock: us = cycles / 200 at F = 200.006 MHz (MEASURED s2, as each page says).

## P1.4b and P1.4c (town grid, 16:00): pages p20 to p25

n 1, 2, 4, 8, 12, 16 and 96: 0 rays among 65,536, not timed (both runs). Kernel against reference: the same bytes (both runs).

| Id | n | cycles a ray | ns | ns − cyc | gaps | sec | irq | Photo |
|---|---|---|---|---|---|---|---|---|
| R2J-01 | P1.4b n24 | 604.80 | 3023.76 | −698 | 1440 880 | 0 | 3 | 143355675 |
| R2J-02 | P1.4b n32 | 783.45 | 3917.01 | −608 | 1440 880 | 0 | 4 | 143355675 |
| R2J-03 | P1.4b n48 | 1135.36 | 5676.56 | −332 | 1360 960 | 0 | 6 | 143356641 |
| R2J-04 | P1.4b n64 | 1492.81 | 7463.82 | −182 | 1280 1040 | 0 | 8 | 143356641 |
| R2J-05 | P1.4c n24 | 603.58 | 3017.67 | −649 | 1200 880 | 0 | 3 | 143358496 |
| R2J-06 | P1.4c n32 | 783.55 | 3917.50 | −568 | 1360 880 | 0 | 4 | 143358496 |
| R2J-07 | P1.4c n48 | 1135.65 | 5678.00 | −392 | 1440 960 | 0 | 6 | 143359493 |
| R2J-08 | P1.4c n64 | 1488.00 | 7439.76 | −185 | 1280 1040 | 0 | 6 | 143359493 |

| Id | Claim | Status | Photo |
|---|---|---|---|
| R2J-09 | P1.4b fit over 4 rows: C_step 22.17, C_setup 72.76 cycles. | DERIVED (console) | 143356641 |
| R2J-10 | P1.4c fit over 4 rows: C_step 22.09, C_setup 75.05 cycles. | DERIVED (console) | 143359493 |
| R2J-11 | P1.4a (p18, context): n32 783.28, n48 1139.60, n64 1492.82; fit C_step 22.14, C_setup 76.04. Three runs of P1.4 agree within 0.4 %, and with ray1 (22.1, about 74). Thesis: 12 to 16 and about 30. | DERIVED (console) | 143353469 |
| R2J-12 | No P1.4 row in my range crossed an ns second (sec 0 in all 8); none shows the 2.5 ms step. | MEASURED | as above |

## P2.A and P2.B (grid in RAM, group 1): pages p26 to p29

| Id | Row | 14:00 (P2.A, 143402000) | 16:00 (P2.B, 143403867) |
|---|---|---|---|
| R2J-13 | .1 batched, grid cold | 563.40 cyc a ray, 2884 us | 548.81 cyc a ray, 2809 us |
| R2J-14 | .2 batched, grid warm | 554.27 cyc a ray, 2837 us | 539.57 cyc a ray, 2762 us |
| R2J-15 | .3 28,699 vertices alone | 59.04 cyc a vertex, 8472 us | 58.93 cyc a vertex, 8456 us |
| R2J-16 | .4 interleaved | 12151 us | 11895 us |
| R2J-17 | .5 DERIVED .4 − .3 − .2 | 840 us, 164 cyc a ray | 676 us, 132 cyc a ray |
| R2J-18 | .6 kernel against reference | the same bytes | the same bytes |

Least of 5 runs (3 for .3 and .4). The page predicts .1 − .2 ≤ 18 us (p27 143402984, p29 143404817).

| Id | Claim | Status |
|---|---|---|
| R2J-19 | .1 − .2 is 47 us at both suns (2884 − 2837, 2809 − 2762): the prediction (≤ 18 us) fails in RAM. | DERIVED (here, from MEASURED rows) |

## P9 (proxy pass, 16:00): pages p30 and p31 (143405719, 143406712)

| Id | Claim | Status |
|---|---|---|
| R2J-20 | P9.1: 454 receivers (people 393, hens 61); 88 shadowed. | MEASURED |
| R2J-21 | P9.2 with early outs: 137.80 cycles a pair, 312 us a pass. | MEASURED |
| R2J-22 | P9.3 reference (no early outs): 184.43 cycles a pair. | MEASURED |
| R2J-23 | P9.4 kernel against reference: the same bytes. | MEASURED |
| R2J-24 | P9.5: the pass is 11.32 % of P2's slice of 1024 rays at 16:00 (P2.2). | DERIVED (console) |
| R2J-25 | P9.6: k for a 2 ms slice at P2.2's cost a ray: 741 rays; the pass is 15.63 % of 2 ms. P9 is falsified only if the pass costs more than k × C_ray; it does not. | DERIVED (console) |
| R2J-26 | Thesis 5.6 ESTIMATE was sphere 15 to 20 early out, 40 to 50 full, capsule about twice. The measured 137.80 / 184.43 a pair (a mix of 393 capsule and 61 sphere receivers) is well above that, about 2 to 4 times even for capsules alone. The pass still fits (R2J-25). | DERIVED (here) |

## h3 (transform with 1 / w, cycles a vertex): page p32 (143408034)

256 vectors in the cache; 16,384 out of cache (input and output purged first).

| Id | Form | In cache | Not in cache | Status |
|---|---|---|---|---|
| R2J-27 | h3.1 mat_transform | 13.39 | 42.86 | MEASURED |
| R2J-28 | h3.2 batch, prefetch | 14.46 | 45.23 | MEASURED |
| R2J-29 | h3.3 batch, no prefetch | 14.20 | 47.97 | MEASURED |
| R2J-30 | h3.4 SH4ZAM C loop | 28.64 | 57.12 | MEASURED |
| R2J-31 | h3.5 batch against C loop: the same bytes; no-prefetch build: the same bytes. | | | MEASURED |
| R2J-32 | h3.6 mat_transform against C loop (x, y, 1/w): max 2 ulp. | | | MEASURED |

R2J-33 (DERIVED here): the batch kernel gains nothing over mat_transform: it is 1.07 cycles slower in cache and 2.37 slower out of it. Prefetch saves 2.74 cycles a vertex out of cache and costs 0.26 in it.

## RT (shadow ray against a triangle, 14:00): pages p33 and p34 (143409451, 143410471)

256 points × 96 triangles, all in the cache, each ray to its first hit; a test includes the loop over triangles. No divide in any form (spec).

| Id | Form | Cycles a test | Status |
|---|---|---|---|
| R2J-34 | RT.1 any direction, early outs (kernel) | 62.49 (22,823 tests a pass) | MEASURED |
| R2J-35 | RT.2 any direction, reference | 115.21 (22,823) | MEASURED |
| R2J-36 | RT.3 prepared for the sun, early outs | 33.16 (22,823) | MEASURED |
| R2J-37 | RT.4 prepared for the sun, reference | 88.60 (22,823) | MEASURED |
| R2J-38 | RT.5 any direction, kernel against reference: the same bytes. | | MEASURED |
| R2J-39 | RT.6 prepared, kernel against reference: the same bytes; 44 of 256 rays shadowed. | | MEASURED |

DERIVED here: 22,823 / 256 = 89.2 tests a ray. Prepared saves 47 % on the kernel (62.49 → 33.16). Early outs save 46 % (any direction) and 63 % (prepared).

## OC-RAM (group 2): pages p35 to p41

| Id | Claim | Status | Photo |
|---|---|---|---|
| R2J-40 | Group 1 ends at p35; START runs group 2 with OC-RAM left on. CCR before 00000105, now 00000125: the RAM cache is then 8 KB. | MEASURED (register read) | 143411884, 143413090 |

| Id | Row | 14:00 (P2.C, 143414090) | 16:00 (P2.D, 143416038) |
|---|---|---|---|
| R2J-41 | .1 batched, grid cold | 561.08 cyc a ray, 2872 us | 547.05 cyc a ray, 2800 us |
| R2J-42 | .2 batched, grid warm | 561.08 cyc a ray, 2872 us | 547.06 cyc a ray, 2800 us |
| R2J-43 | .3 vertices alone | 62.95 cyc a vertex, 9034 us | 62.94 cyc a vertex, 9032 us |
| R2J-44 | .4 interleaved | 12221 us | 12151 us |
| R2J-45 | .5 DERIVED .4 − .3 − .2 | 314 us, 61 cyc a ray | 317 us, 62 cyc a ray |
| R2J-46 | .6 kernel against reference | the same bytes | the same bytes |

Same notes as group 1 (p38 143415039, p40 143417497). p41 (143418695): "GROUP 2 END."

DERIVED here, against group 1:
- R2J-47: in OC-RAM, cold equals warm (0 us apart at both suns; 47 us in RAM). The P2 prediction (≤ 18 us) holds in OC-RAM.
- R2J-48: the warm ray costs about the same (14:00 +6.8 cycles, 16:00 +7.5 cycles), so OC-RAM does not make the walk itself faster.
- R2J-49: the vertices alone are 3.9 cycles a vertex slower (about 560 us a slice), as the RAM cache is halved to 8 KB.
- R2J-50: the interleave penalty drops from 840 to 314 us (14:00) and 676 to 317 us (16:00). Net interleaved slice: 12151 → 12221 us (+70) at 14:00, 11895 → 12151 us (+256) at 16:00. OC-RAM is a small loss for the whole slice at both suns: the vertex cost eats what the penalty gives back.
- Note: P2.A.4 and P2.D.4 both read 12151 us. Two pages, two photos; a coincidence, not a misread.

## RT against TOWN-063, and what it means for the bake (ESTIMATE)

TOWN-063 assumed 50 cycles a ray-triangle test (Möller–Trumbore, one reciprocal). The bake needs 168,965 tests a sun with the proxies, or 577,635 with the screen mesh (host counts).

| Cycles a test | Source | Proxies (168,965) | Screen mesh (577,635) |
|---|---|---|---|
| 50 | TOWN-063 assumption | 42 ms | 144 ms |
| 33.16 | RT.3, prepared for the sun (MEASURED) | 28 ms | 96 ms |
| 62.49 | RT.1, any direction (MEASURED) | 53 ms | 181 ms |

All ESTIMATE: the cycles are MEASURED, but on a 96-triangle scene in the cache, and the test counts are host counts. The bake's data is larger (town proxies and bins), so cache misses may add to it.

What it does for the bake:
- The 50-cycle guess sat between the two measured forms. The order of magnitude holds: the load bake is well under 0.2 s in every form. Nothing changes in the design.
- The bake's rays all share the sun's direction, so the prepared form (RT.3) fits it. That is 33 cycles, a third under the guess: 28 ms with the proxies.
- bake1 (`demo/psm/patches/bake1.diff`) uses plain Möller–Trumbore with one `sh4der_div_` a test. RT.1 has no divide and costs 62.49, so bake1 is likely above RT.1 (ESTIMATE: an FDIV is about 10 cycles or more of latency). Suggestion: switch bake1's test to RT's prepared form (64 B a triangle, worked out once per sun). This also makes a sliced rebake (item 4) cheaper: about 28 ms of tests split over frames.
- A model vertex's ray tests 18 triangles (TOWN-060). At 33.16 cycles that is about 600 cycles a ray, the same size as the grid walk's 547 to 561 cycles a ray (P2). A ground texel (1.0 to 1.2 tests) costs about 33 to 40 cycles in tests, so its cost is the bin lookup, not the test.
- TOWN-085 stays OPEN: the bake's own `us / tests` on HUD line 14 is the test of these numbers in the town itself.
