Pages covered: p1/35, p2/35, p3/41 to p19/41 (no gap inside the range; p1 and p2 carry a different page total, see "Looks odd" 1).

# Ray bench ray2: results, pages p1 to p19 (cortana)

Image: `demo/psm/out/cdi/psm-bench-ray2.cdi` (spec: `demo/psm/bench/NOTES.md`, "Ray bench (image ray2)"). Run by the owner on 2026-11-25 on one console.
Photos: `I:\マイドライブ\dcdev\sh4der-pvr\logs\benchmarks\psm_bench_ray2\`. Only RESULT lines are entered as results. TEST lines are headings. Lines the page itself marks DERIVED are entered as DERIVED. Every value below was read clearly. No value in my range was unreadable.

## Photos read

| Photo (PXL_20261125_…) | Page | What it holds |
|---|---|---|
| 143317805 | p1/35 | Title only ("A/B page START: OC-RAM") |
| 143322667 | p2/35 | CLOCK.1 to CLOCK.4 |
| 143335803 | p3/41 | TEST P1.1; n 1, n 2, n 4 (its second line is on p4) |
| 143337411 | p4/41 | P1.1 n 4 second line; n 8 to n 32; n 48 cycles (its ns is on p5) |
| 143338732 | p5/41 | P1.1 n 48 ns; n 64 and n 96 not timed; bytes; fit |
| 143339918 | p6/41 | TEST P1.2a; n 1, n 2, n 4 (its second line is on p7) |
| 143341074 | p7/41 | P1.2a n 8 to n 32; n 48 cycles (its ns is on p8) |
| 143342377 | p8/41 | P1.2a n 48 ns; n 64; n 96; bytes; fit |
| 143343478 | p9/41 | TEST P1.2b; n 1, n 2, n 4 (its second line is on p10) |
| 143345413 | p10/41 | P1.2b n 8 to n 32; n 48 cycles |
| 143346552 | p11/41 | P1.2b n 48 ns; n 64; n 96; bytes; fit |
| 143347672 | p12/41 | TEST P1.2c; n 1, n 2, n 4 |
| 143348718 | p13/41 | P1.2c n 8 to n 32; n 48 cycles |
| 143349680 | p14/41 | P1.2c n 48 ns; n 64; n 96; bytes; fit |
| 143350648 | p15/41 | TEST P1.3; n 1 to n 16 not timed; n 24 |
| 143351644 | p16/41 | P1.3 n 32, n 48; n 64 and n 96 not timed; bytes; fit |
| 143352566 | p17/41 | TEST P1.4a; n 1 to n 16 not timed; n 24 |
| 143353469 | p18/41 | P1.4a n 32 to n 64; n 96 not timed; bytes; fit (its last line is on p19) |
| 143354396 | p19/41 | The fit's last line ("thesis 5.3 ESTIMATE …"), then nothing more |

The named range 143317805 to 143353469 is 18 files, not 20. In name order the 20 files include the stray 135731481 (a copy of ray1's last file, ignored as told) and 143354396. I read 143354396 because p19 continues p18. The next reader starts at 143355675.

Predictions used below:
- **Line:** C_ray = 72 + 22.1 n (SH4-052, from ray1; thesis 5.3 as reworked).
- **Old thesis 5.3 ESTIMATE:** 30 + (12 to 16) n. The page prints it under every fit.
- **Clock:** KallistiOS's TMU2 reload, +2,502,400 ns once a reload (NOTES.md, ESTIMATE until CLOCK.4).

## CLOCK (p2, photo 143322667)

| ID | Claim | Status | Photo | Test | Prediction |
|---|---|---|---|---|---|
| R2C-01 | CLOCK.1: the ns timer's least step is 640 ns | MEASURED 2026-11-25, one console | 143322667 | CLOCK.1 | — (see note a) |
| R2C-02 | CLOCK.2: 200,258 cycles in 1,000,240 ns, both clocks read together | MEASURED 2026-11-25, one console | 143322667 | CLOCK.2 | about 200,000 (NOTES.md); 200,054 at 200.006 MHz. **Holds** (+0.10 %, note b) |
| R2C-03 | CLOCK.3: one ns call costs 92.01 cycles | MEASURED 2026-11-25, one console | 143322667 | CLOCK.3 | none given |
| R2C-04 | CLOCK.4: over 2,199,940 µs of cycles the ns clock moved 2,205,010 µs. It stepped over 1 ms ahead 2 times, and the largest step was 2,502,321 ns | MEASURED 2026-11-25, one console | 143322667 | CLOCK.4 | +2,502,400 ns once a reload. **Confirmed**: the largest step is 79 ns short, which is one 80 ns count (note c) |

Notes (all ESTIMATE, my arithmetic):
- **a.** 640 ns is more than one ns call (92 cycles = 460 ns). So CLOCK.1 most likely gives the spacing of two calls in the loop, rounded to the 80 ns count, not the timer's resolution. The count itself is 80 ns (NOTES.md, KallistiOS source).
- **b.** 1,000,240 ns at 200.006 MHz is 200,054 cycles. The 204 extra cycles are about two ns calls at CLOCK.3's 92, plus the reads inside the window.
- **c.** The ns clock gained 5,070 µs on the cycles in 2.2 s. Two reload steps give 2 × 2,502.4 = 5,004.8 µs. The other 65 µs is 30 ppm of 2.2 s. The same 30 ppm shows inside every P1 row (R2C-30). Both parts fit one cause: the timer counts at 50.0015 MHz / 4 (200.006 MHz / 16, 79.998 ns a count), while KallistiOS counts 80 ns and reloads at 12,468,720 counts (0.99747 s). The 2 steps in 2.2 s are as expected: a reload comes every 0.9975 s, so a 2.2 s window holds 2 reloads, or 3 about 20 % of the time.

## P1.1: zero grid, 14:00 (p3 to p5)

Status of every row: MEASURED 2026-11-25, one console. Test P1.1. Each row is 256 rays and 16 passes. The second line is ns-cyc (ns), gaps a and b (ns), sec, irq.

| n | Cycles a ray | ns a ray | ns-cyc | Gaps | sec | irq | Photo | Line 72 + 22.1 n | ray1 (R1J-03) |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 99.75 | 498.51 | −1098 | 1360 960 | 0 | 1 | 143335803 | 94.1 | 93.91 |
| 2 | 117.33 | 586.36 | −1087 | 1360 880 | 0 | 2 | 143335803 | 116.2 | 120.40 |
| 4 | 162.08 | 810.15 | −970 | 1520 800 | 0 | 1 | 143335803, 143337411 | 160.4 | 160.74 |
| 8 | 254.61 | 1272.83 | −893 | 1440 960 | 0 | 0 | 143337411 | 248.8 | 248.40 |
| 12 | 338.15 | 1690.48 | −1002 | 1600 880 | 0 | 2 | 143337411 | 337.2 | 341.36 |
| 16 | 430.31 | 2151.30 | −795 | 1280 960 | 0 | 1 | 143337411 | 425.6 | 428.65 |
| 24 | 606.67 | 3033.06 | −857 | 1280 1120 | 0 | 4 | 143337411 | 602.4 | 603.87 |
| 32 | 782.67 | 3913.12 | −539 | 1200 960 | 0 | 3 | 143337411 | 779.2 | 781.01 |
| 48 | 1137.31 | 5686.25 | −561 | 1440 1200 | 0 | 4 | 143337411, 143338732 | 1132.8 | 1133.23 |

| ID | Claim | Status | Photo | Test | Prediction |
|---|---|---|---|---|---|
| R2C-05 | P1.1 n 64 and n 96: 0 rays among 65,536, not timed | MEASURED 2026-11-25, one console | 143338732 | P1.1 | same as ray1 |
| R2C-06 | P1.1 kernel against reference: the same bytes | MEASURED 2026-11-25, one console | 143338732 | P1.1 | host: 0 bytes differ |
| R2C-07 | P1.1 fit over the 9 rows: C_step 22.13 cycles, C_setup 75.15 cycles | DERIVED (by the page) from MEASURED | 143338732 | P1.1 | 22.1 / 72 (ray1 22.09 / 73.85). Old 5.3: 12 to 16 / about 30, still overturned |

## P1.2a, P1.2b, P1.2c: zero grid, 16:00, three runs (p6 to p14)

Status of every row: MEASURED 2026-11-25, one console. Tests P1.2a, P1.2b and P1.2c. Each row is 256 rays unless stated. Cycles a ray / ns a ray, then ns-cyc, gaps, sec, irq.

| n | P1.2a | P1.2b | P1.2c | Line | ray1 P1.2 (R1J-06) |
|---|---|---|---|---|---|
| 1 | 96.03 / 479.86; −1141, 1360 960, 0, 0 | 96.03 / 479.90; −1080, 1360 880, 0, 2 | 96.03 / 479.86; −1200, 1440 960, 0, 0 | 94.1 | 94.70 |
| 2 | 118.40 / 591.73; −1037, 1520 800, 0, 0 | 117.63 / 587.91; −1067, 1360 880, 0, 2 | 117.63 / 587.87; −1137, 1520 880, 0, 1 | 116.2 | 119.94 |
| 4 | 161.74 / 808.45; −1080, 1360 880, 0, 1 | 162.54 / 812.42; −1150, 1600 960, 0, 2 | 161.73 / 808.39; −1120, 1360 960, 0, 0 | 160.4 | 160.41 |
| 8 | 249.63 / 1247.92; −886, 1200 880, 0, 2 | 255.29 / 1276.19; −913, 1440 960, 0, 0 | 249.62 / 1247.85; −946, 1280 880, 0, 1 | 248.8 | 248.30 |
| 12 | 342.56 / 1712.57; −809, 1200 880, 0, 0 | 338.86 / 1694.08; −881, 1440 880, 0, 2 | 341.93 / 1709.39; −849, 1280 960, 0, 1 | 337.2 | 342.21 |
| 16 | 425.54 / 2127.50; −758, 1200 880, 0, 2 | 430.02 / 2149.86; −875, 1280 960, 0, 1 | 426.36 / 2131.58; −808, 1440 880, 0, 3 | 425.6 | 429.50 |
| 24 | 602.42 / 3011.87; −749, 1280 960, 0, 3 | 607.35 / 3036.48; −896, 1280 1120, 0, 4 | 606.87 / 3034.10; −657, 1360 880, 0, 3 | 602.4 | 604.68 |
| 32 | 784.22 / 3920.83; −628, 1440 960, 0, 4 | 782.56 / 3912.55; −659, 1280 960, 0, 3 | 784.17 / 3920.62; −628, 1440 960, 0, 4 | 779.2 | 778.82 |
| 48 | 1135.94 / 5679.43; −412, 1360 1040, 0, 6 | 1139.29 / 5696.17; −520, 1440 1200, 0, 4 | 1136.63 / 5682.89; −451, 1440 1040, 0, 5 | 1132.8 | 1134.02 |
| 64 | 1492.09 / 7460.21; −193, 1440 1040, 0, 7 | 1491.82 / 7458.86; −163, 1200 1040, 0, 7 | 1493.13 / 7465.39; −162, 1280 960, 0, 8 | 1486.4 | 1491.06 |
| 96 (55 rays) | 2216.77 / 11082.63; −787, 1280 1040, 0, 1 | 2194.39 / 10970.90; −640, 1280 800, 0, 3 | 2216.51 / 11081.36; −787, 1280 1040, 0, 1 | 2193.6 | 2188.33 |

Photos:
- P1.2a: 143339918 (n 1 to 4), 143341074 (n 4's second line, n 8 to 32, n 48 cycles), 143342377 (n 48 ns to the fit).
- P1.2b: 143343478, 143345413, 143346552, in the same split.
- P1.2c: 143347672, 143348718, 143349680, in the same split.

| ID | Claim | Status | Photo | Test | Prediction |
|---|---|---|---|---|---|
| R2C-08 | P1.2a kernel against reference: the same bytes | MEASURED 2026-11-25, one console | 143342377 | P1.2a | host: 0 bytes differ |
| R2C-09 | P1.2a fit over the 10 rows: C_step 22.15, C_setup 73.39 | DERIVED (by the page) from MEASURED | 143342377 | P1.2a | 22.1 / 72 (ray1 22.12 / 73.57) |
| R2C-10 | P1.2b kernel against reference: the same bytes | MEASURED 2026-11-25, one console | 143346552 | P1.2b | host: 0 bytes differ |
| R2C-11 | P1.2b fit over the 10 rows: C_step 22.16, C_setup 74.61 | DERIVED (by the page) from MEASURED | 143346552 | P1.2b | 22.1 / 72 |
| R2C-12 | P1.2c kernel against reference: the same bytes | MEASURED 2026-11-25, one console | 143349680 | P1.2c | host: 0 bytes differ |
| R2C-13 | P1.2c fit over the 10 rows: C_step 22.18, C_setup 73.47 | DERIVED (by the page) from MEASURED | 143349680 | P1.2c | 22.1 / 72 |
| R2C-14 | No P1.2 row in a, b or c has sec 1. Every ns a ray is 4.997 to 5.000 times its cycles a ray. ray1's P1.2 n 12 row (2,321.85 ns against 342.21 cycles) does not repeat: here n 12 reads 1,712.57, 1,694.08 and 1,709.39 ns | MEASURED 2026-11-25, one console (the ratio is my arithmetic) | 143341074, 143345413, 143348718 | P1.2a/b/c | NOTES.md: a row that spans a reload carries +2.5 ms. None spanned one here |

## P1.3: town grid, 14:00 (p15, p16)

| ID | Claim | Status | Photo | Test | Prediction |
|---|---|---|---|---|---|
| R2C-15 | P1.3 n 1, 2, 4, 8, 12, 16: 0 rays among 65,536 each, not timed | MEASURED 2026-11-25, one console | 143350648 | P1.3 | host count: lit rays on the town grid only from n 24 up; same as ray1 |
| R2C-16 | P1.3 n 24 (55 rays): 607.02 cycles a ray, 3,034.00 ns. Second line on p16: ns-cyc −899, gaps 1360 800, sec 0, irq 1 | MEASURED 2026-11-25, one console | 143350648, 143351644 | P1.3 | line 602.4; ray1 603.44 (also 55 rays) |
| R2C-17 | P1.3 n 32 (256 rays): 781.31 cycles a ray, 3,906.28 ns; ns-cyc −749, gaps 1280 1040, sec 0, irq 3 | MEASURED 2026-11-25, one console | 143351644 | P1.3 | line 779.2; ray1 781.48 |
| R2C-18 | P1.3 n 48 (256 rays): 1,136.47 cycles a ray, 5,682.12 ns; ns-cyc −371, gaps 1440 960, sec 0, irq 4 | MEASURED 2026-11-25, one console | 143351644 | P1.3 | line 1,132.8; ray1 1,136.79 |
| R2C-19 | P1.3 n 64 and n 96: 0 rays among 65,536, not timed | MEASURED 2026-11-25, one console | 143351644 | P1.3 | ray1 the same |
| R2C-20 | P1.3 kernel against reference: the same bytes | MEASURED 2026-11-25, one console | 143351644 | P1.3 | host: 0 bytes differ |
| R2C-21 | P1.3 fit over 2 rows: C_step 22.20, C_setup 70.99 | DERIVED (by the page) from MEASURED | 143351644 | P1.3 | 22.1 / 72 (ray1 22.21 / 70.88). Only 2 rows (n 24 has 55 rays, under the 64 the fit needs) |

## P1.4a: town grid, 16:00 (p17 to p19)

| ID | Claim | Status | Photo | Test | Prediction |
|---|---|---|---|---|---|
| R2C-22 | P1.4a n 1, 2, 4, 8, 12, 16: 0 rays among 65,536 each, not timed | MEASURED 2026-11-25, one console | 143352566 | P1.4a | same as ray1 P1.4 |
| R2C-23 | P1.4a n 24 (256 rays): 608.11 cycles a ray, 3,040.33 ns. Second line on p18: ns-cyc −716, gaps 1440 960, sec 0, irq 3 | MEASURED 2026-11-25, one console | 143352566, 143353469 | P1.4a | line 602.4; ray1 602.25 |
| R2C-24 | P1.4a n 32 (256 rays): 783.28 cycles a ray, 3,916.13 ns; ns-cyc −628, gaps 1440 960, sec 0, irq 3 | MEASURED 2026-11-25, one console | 143353469 | P1.4a | line 779.2; ray1 781.55 |
| R2C-25 | P1.4a n 48 (256 rays): 1,139.60 cycles a ray, 5,697.77 ns; ns-cyc −339, gaps 1280 960, sec 0, irq 5 | MEASURED 2026-11-25, one console | 143353469 | P1.4a | line 1,132.8; ray1 1,135.22 cycles with **6,286.93 ns** (the odd row) |
| R2C-26 | P1.4a n 64 (256 rays): 1,492.82 cycles a ray, 7,463.86 ns; ns-cyc −212, gaps 1440 960, sec 0, irq 7 | MEASURED 2026-11-25, one console | 143353469 | P1.4a | line 1,486.4; ray1 1,485.50 |
| R2C-27 | P1.4a n 96: 0 rays among 65,536, not timed | MEASURED 2026-11-25, one console | 143353469 | P1.4a | ray1 the same |
| R2C-28 | P1.4a kernel against reference: the same bytes | MEASURED 2026-11-25, one console | 143353469 | P1.4a | host: 0 bytes differ |
| R2C-29 | P1.4a fit over 4 rows: C_step 22.14, C_setup 76.04. The printed "thesis 5.3 ESTIMATE" line is on p19 | DERIVED (by the page) from MEASURED | 143353469, 143354396 | P1.4a | 22.1 / 72 (ray1 22.07 / 74.09) |

P1.4a n 48 here reads 5,697.77 ns against 1,139.60 cycles (ratio 5.000), with sec 0. ray1's 6,286.93 ns was 589 ns a ray long. Over 16 × 256 passes that is 2.41 ms, which is one reload step. This fits NOTES.md's cause. It is not proof that ray1's row spanned a reload, since ray1 printed no sec flag.

## What the second lines show (DERIVED from the rows above, my arithmetic)

| ID | Claim | Status | Source | Test | Prediction |
|---|---|---|---|---|---|
| R2C-30 | ns-cyc rises with the row's length. It is about −1,150 ns for a 2 ms row (n 1) and about −180 ns for a 30.5 ms row (n 64). From n 1 to n 64 the slope is 948 ns over 28.6 ms = **+33 ns a ms (33 ppm)**. Same in P1.2a, b and c | DERIVED from MEASURED rows | P1.2a/b/c rows | P1.2 | Note c's cause predicts 30 ppm (80 ns counted against 79.998 ns). **Holds** within reading error |
| R2C-31 | The part of ns-cyc at zero length is about −1,150 to −1,200 ns (230 to 240 cycles). The cycle window holds the two ns calls at its ends (2 × 92 = 184 cycles, CLOCK.3) and the reads; the ns window starts and ends inside those calls | ESTIMATE (cause); the size is DERIVED | rows above | P1 | — |
| R2C-32 | Gap a (ns, cyc, ns at the start) is 1,200 to 1,600 ns; gap b (at the end) is 800 to 1,200 ns. A cycle read plus one ns call costs 160 to 320 cycles, more than CLOCK.3's 92 for the ns call alone. Why gap a is about 400 ns longer than gap b is OPEN | MEASURED (the gaps); the cause is OPEN | every row | P1 | none given |
| R2C-33 | irq grows with row length: 0 to 2 at 2 ms, 7 to 8 at 30 ms, so about 230 a second. If each costs 1 to 2 µs (not measured), that is under 0.1 % of a row | ESTIMATE | every row | P1 | none given |
| R2C-34 | Spread across the three runs of the same rays: at most 5.7 cycles (P1.2b n 8, 255.29 against 249.6, +2.3 %) and 22.4 cycles at n 96 (P1.2b 2,194.39 against 2,216.6, −1.0 %). irq does not explain it: b's n 8 shows irq 0, but irq is counted in a second run, not the timed one (NOTES.md risk) | MEASURED (the values); the cause is OPEN | 143345413, 143346552 | P1.2a/b/c | — |

## Summary against the model

- **The cost line holds.** Six fits in this range give C_step 22.13 to 22.20 and C_setup 70.99 to 76.04. ray1's fits gave 22.07 to 22.21 and 70.88 to 74.09. The line 72 + 22.1 n (SH4-052) stands. The old 5.3 ESTIMATE (12 to 16, about 30) stays overturned. Every timed row sits on or above the line, by 0 % to +6 %. The largest gap is P1.1 n 1, 99.75 against 94.1 (ray1 93.91, P1.2 runs 96.03). The n 1 row is the noisiest, and the fit hardly moves.
- **The clock step is confirmed** (R2C-04): +2,502,321 ns a reload against +2,502,400 predicted. ray2's ns rows all agree with their cycles at about 5.000 ns a cycle, and no row in my range has sec 1. Every interval timed with `timer_ns_gettime64` across a reload reads 2.5 ms long, as NOTES.md says. Use the cycle counter.
- **New finding, small:** between reloads the ns clock gains about 30 to 33 ppm on cycles taken at 200.006 MHz (R2C-30; it also closes CLOCK.4's last 65 µs). Both parts fit one cause: KallistiOS's TMU2 constant is about 0.25 % low. The cause is an ESTIMATE; the rates are DERIVED from MEASURED. The size does not matter for any bench (0.003 %).
- **Nothing in p1 to p19 contradicts the model.** Kernel against reference was "the same bytes" on all six runs.

## Looks odd

1. **Page totals.** p1 and p2 say "/35" and "A/B page START: OC-RAM". p3 onward say "/41 … (end)". Most likely p1 and p2 were photographed while the bench was still running (35 pages written so far), and the rest after the end. The CLOCK values are fixed once printed, so this should not change them. Worth a check against the next reader's pages that p1 and p2 at "/41" hold the same text.
2. **Range size.** The named range is 18 files. With the stray file and p19's file, the "first 20 by name order" are exactly what I read. No page is missing between p1 and p19.
3. **CLOCK.1's 640 ns** is the loop's spacing, not the timer's step (note a). Anyone who quoted 640 ns as the timer's resolution from ray1 should use 80 ns (FACT from KallistiOS source; NOTES.md) with this reading.
