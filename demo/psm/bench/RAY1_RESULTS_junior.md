# ray1: results read from the photos, Junior's share

Image `psm-bench-ray1.cdi` (spec: `NOTES.md` "Ray bench (image ray1)"), run by the owner on 2026-11-25 on one console. The photos are in `I:\マイドライブ\dcdev\sh4der-pvr\logs\benchmarks\ray1`. This file covers the photos named `PXL_20261125_135301290.jpg` onward, in name order (nine photos); Cortana covers the earlier ones.

Rules: only RESULT lines are results; TEST lines are headings. DERIVED lines are marked as such. Every value is copied from the screen as printed. Where a value does not add up, it is flagged, not corrected.

## What the nine photos hold

| Photo | Page | Content |
|---|---|---|
| 135301290 | p23/24 (first run) | The tail of a group 2 page: "...200 at F = 200.006 MHz, MEASURED s2)", "(.1 cold: grid, points, bytes and vertices purged first; P2 predicts .1 - .2 <= 18 us)". No RESULT line: the page's RESULT lines are on an earlier screen, not in this share. |
| 135303039 | p24/24 (first run) | "GROUP 2 END." No RESULT line. |
| 135722917 | p1/24 (second run) | The title page: "THESIS_SH4_SHADOWS.md P1, P2, P9; h3. One run, this console." |
| 135724347 | p2/24 | CLOCK |
| 135725896, 135727320 | p3, p4 | P1.1 |
| 135728755, 135730101 | p5, p6 | P1.2 |
| 135731481 | p7 | P1.3, first part (the page continues on p8, not photographed in this share) |

**Not in this share:** the RESULT lines of P2.C and P2.D (group 2, OC-RAM), and the pages for P9 and h3. The second run's photos stop at p7. If those pages were photographed, they are among the earlier files; if not, they are not on record.

## Results

The ns figures on these pages are the cycles divided by 0.2 (200 MHz), except where flagged.

| ID | Claim (as printed) | Status | Photo | Test | Model / ESTIMATE beside it |
|---|---|---|---|---|---|
| R1J-01 | CLOCK.1: the ns timer's least step is 640 ns. | MEASURED 2026-11-25, one console | 135724347 | CLOCK | — |
| R1J-02 | CLOCK.2: 1,027,528 cycles counted in 1,000,320 ns of the ns timer. **Flag:** that is 1.027 cycles a ns. It does not agree with the 200.006 MHz printed on p23 of the first run (135301290), nor with the 5 ns a cycle used by every P1 row. The counter or the page may count something other than CPU cycles here. Do not use it as the clock until Senior explains it. | MEASURED (as printed); meaning OPEN | 135724347 | CLOCK | s2: 200.006 MHz |
| R1J-03 | P1.1 (zero grid, 4 KB in the cache, 14:00), cycles a ray, 256 rays each: n 1: 93.91 (469.51 ns); n 2: 120.40 (601.95); n 4: 160.74 (803.61); n 8: 248.40 (1,241.89); n 12: 341.36 (1,706.77); n 16: 428.65 (2,143.16); n 24: 603.87 (3,019.35); n 32: 781.01 (3,904.98); n 48: 1,133.23 (5,666.05). n 64 and n 96: 0 rays among 65,536, not timed. | MEASURED 2026-11-25, one console | 135725896, 135727320 | P1.1 | THESIS 5.3 |
| R1J-04 | P1.1 kernel against reference: the same bytes. | MEASURED | 135727320 | P1.1 | host: 0 bytes differ |
| R1J-05 | P1.1 fit over the 9 rows with 64 rays or more: C_step 22.09 cycles, C_setup 73.85 cycles (C_setup holds the loop's load and store too). | DERIVED from MEASURED | 135727320 | P1.1 | THESIS 5.3 ESTIMATE: C_step 12 to 16, C_setup about 30 |
| R1J-06 | P1.2 (zero grid, 16:00), cycles a ray: n 1: 94.70 (473.41 ns); n 2: 119.94 (599.64); n 4: 160.41 (801.99); n 8: 248.30 (1,241.38); n 12: 342.21 (2,321.85, **flag**); n 16: 429.50 (2,147.48); n 24: 604.68 (3,023.30); n 32: 778.82 (3,894.02); n 48: 1,134.02 (5,670.09); n 64: 1,491.06 (7,455.23); n 96 (55 rays): 2,188.33 (10,941.18). 256 rays each unless stated. | MEASURED 2026-11-25, one console | 135728755, 135730101 | P1.2 | THESIS 5.3 |
| R1J-07 | **Flag on R1J-06, n 12:** the screen clearly reads 2,321.85 ns. At 5 ns a cycle, 342.21 cycles is 1,711 ns. The ns and the cycles of this row disagree by 611 ns (close to the timer's 640 ns step: one tick?). The cycles fit the line; the ns does not. Use the cycles. | MEASURED (as printed); the cause is OPEN | 135728755 | P1.2 | — |
| R1J-08 | P1.2 kernel against reference: the same bytes. | MEASURED | 135730101 | P1.2 | host: 0 bytes differ |
| R1J-09 | P1.2 fit over the 10 rows with 64 rays or more: C_step 22.12 cycles, C_setup 73.57 cycles. | DERIVED from MEASURED | 135730101 | P1.2 | THESIS 5.3 ESTIMATE: C_step 12 to 16, C_setup about 30 |
| R1J-10 | P1.3 (town grid, 14:00): n 1, 2, 4, 8, 12, 16 and 64: 0 rays among 65,536, not timed. n 24 (55 rays): 603.44 cycles a ray (3,016.63 ns); n 32 (256 rays): 781.48 (3,907.32); n 48 (256 rays): 1,136.79 (5,683.88). The rest of P1.3 (n 96, the byte check, the fit) is on p8, not in this share. | MEASURED 2026-11-25, one console | 135731481 | P1.3 | the spec's host count: lit rays exist only from n 24 up |

## What it says (DERIVED, and ESTIMATEs marked)

- The grid walk costs **22.1 cycles a cell crossing and about 74 cycles to set up a ray**. That holds on both suns (R1J-05, R1J-09), and on the town grid at n 24 to 48, which falls within 0.5 % of the zero grid (R1J-10 against R1J-03). The data is in the cache in all three, so the grid's content does not change the step.
- **Against the thesis:** the step is 1.4 to 1.8 times 5.3's ESTIMATE of 12 to 16 cycles, and the setup about 2.5 times its ~30 cycles. Every P1 prediction that used 5.3's constants is low by about that factor.
- **The town bake (TOWN-063 assumed 50 cycles a ray-triangle test).** ray1 does not time a ray-triangle test, so TOWN-063's 50 cycles stays an assumption. It does give the cost of the other way to bake the ground, a walk on the height grid:
  - a ground texel's ray at 14:00 crosses up to about 37 cells before it is above the 28 m tower (OFFLOAD_PLAN OFF-03), so 74 + 37 x 22.1 = about 890 cycles a ray at most (DERIVED ESTIMATE; a ray that hits stops sooner);
  - 256² texels: up to about 58 M cycles, 0.29 s (ESTIMATE);
  - the light-space bins of `town_bake.c` test 1.0 to 1.2 triangles a ground ray (TOWN-060), about 60 cycles at the assumed 50. The bins are the cheaper route for the ground, by about 10 times, unless a ray-triangle test costs far more than 50 cycles. That is the test still missing: a ray-triangle timing on the console.
  - For the model vertices (18 tests a ray, TOWN-060), 18 x 50 = 900 cycles a ray (ESTIMATE). That is the same order as one grid walk, and the walk misses overhangs.
- **The ARM7 rebake guess (OFF-03)** used about 8 instructions a step. The SH-4's own step is 22 cycles, so a fixed-point ARM step at 8 instructions is not out of line. That remains an ESTIMATE until B1 runs.

## Unreadable or missing

- Nothing on these nine photos was unreadable.
- Missing from this share: P1.3's p8 (n 96, the bytes, the fit), P1.4, P2.A–D, P9 and h3. So P9's pair costs and 2 ms slice share, and h3's `mat_transform` (13.16 cycles, SH4-020) against the batch, cannot be filled in from these files.
