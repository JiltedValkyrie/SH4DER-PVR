# hen_eyes against a collision sweep: the host tables

Written by `build/hen_eyes/hen_eyes_test --report` (demo/psm/ao/test/hen_eyes_test.c) on the scripted room. Do not edit: run it again.
Counts are exact. Every cycle and microsecond figure is the cost model's: **ESTIMATE**, not a console time.

## The scene

- 10 objects, 372 triangles (17856 bytes at 48 a triangle).
- 104 triangles span the eye's height (0.20 m): the only ones a level ray tests, 4992 bytes.
- 274 triangles stand in the body's height (0 to 0.35 m): the ones a sweep tests. The grid: 16 x 16 cells, 630 entries.

## The cost model (cycles; ESTIMATE unless said)

| Thing counted | Cycles | Status |
|---|---|---|
| eyes: a ray against a triangle | 62.49 | MEASURED for any-hit in the cache (SH4-066); the nearest-hit form here is not timed |
| eyes: a ray against an object's box | 30 | ESTIMATE |
| eyes: a ray's set-up | 74 | ESTIMATE (the grid ray's measured set-up, SH4-052, borrowed) |
| eyes: a ray against another agent | 137.87 | ESTIMATE (SH4-055's point-against-capsule pair, borrowed; a circle is less work) |
| eyes: another agent's distance, once a tick | 20 | ESTIMATE |
| eyes: the steering, a tick | 150 | ESTIMATE |
| sweep: a capsule against a triangle | 220 | ESTIMATE |
| sweep: a triangle turned away by height or box | 20 | ESTIMATE |
| sweep: an object's footprint / a grid cell | 25 / 15 | ESTIMATE |
| sweep: the capsule against another agent | 137.87 | ESTIMATE (SH4-055 borrowed) |
| sweep: the steering, a cast | 60 | ESTIMATE |
| both: an agent's own work a frame | 40 | ESTIMATE |
| shared: the push (circle against a footprint) / the separation (a pair) | 25 / 20 | ESTIMATE |
| 1 ms | 200006 | MEASURED (200.006 MHz) |

## Delta against a collision sweep: frames of 33.3 ms, 60 s scripted run

Per agent per frame (mean): casts, triangle tests, broad-phase tests, agent-pair tests, cycles. Then all agents: us a frame (mean, worst frame), the share of the frame, and the shared push + separation.

| Mode | Agents | Casts | Triangle tests | Broad-phase tests | Pair tests | Cycles an agent a frame | us a frame | us worst frame | Share, mean | Share, worst | Shared us a frame |
|---|---|---|---|---|---|---|---|---|---|---|---|
| eyes: 5 rays, tick 0.10 s | 4 | 2.46 | 3.3 | 17.9 | 1.72 | 1134 | 23 | 61 | 0.07 % | 0.18 % | 11 |
| eyes: 9 rays, tick 0.10 s | 4 | 3.80 | 6.2 | 31.3 | 2.33 | 1900 | 38 | 111 | 0.11 % | 0.33 % | 11 |
| eyes: 13 rays, tick 0.10 s | 4 | 5.12 | 6.8 | 44.5 | 2.93 | 2511 | 50 | 143 | 0.15 % | 0.43 % | 11 |
| sweep, every triangle, every frame, look = reach | 4 | 1.00 | 8.0 | 364.0 | 3.00 | 9552 | 191 | 297 | 0.57 % | 0.89 % | 11 |
| sweep, object boxes, every frame, look = reach | 4 | 1.00 | 8.0 | 35.0 | 3.00 | 3022 | 60 | 194 | 0.18 % | 0.58 % | 11 |
| sweep, grid, every frame, look = reach | 4 | 1.00 | 8.3 | 22.4 | 3.00 | 2739 | 55 | 186 | 0.16 % | 0.56 % | 11 |
| sweep, grid, every frame, look = speed x dt | 4 | 1.00 | 1.5 | 6.2 | 3.00 | 937 | 19 | 30 | 0.06 % | 0.09 % | 11 |
| sweep, grid, tick 0.10 s, look = reach | 4 | 0.33 | 1.4 | 6.0 | 1.00 | 612 | 12 | 81 | 0.04 % | 0.24 % | 11 |
| eyes: 5 rays, tick 0.10 s | 8 | 2.46 | 2.8 | 17.9 | 4.12 | 1280 | 51 | 97 | 0.15 % | 0.29 % | 23 |
| eyes: 9 rays, tick 0.10 s | 8 | 3.78 | 14.2 | 31.0 | 5.12 | 2618 | 105 | 260 | 0.31 % | 0.78 % | 26 |
| eyes: 13 rays, tick 0.10 s | 8 | 5.12 | 19.2 | 44.4 | 6.40 | 3610 | 144 | 400 | 0.43 % | 1.20 % | 24 |
| sweep, every triangle, every frame, look = reach | 8 | 1.00 | 4.7 | 367.3 | 7.00 | 9451 | 378 | 462 | 1.13 % | 1.39 % | 23 |
| sweep, object boxes, every frame, look = reach | 8 | 1.00 | 4.7 | 21.0 | 7.00 | 2576 | 103 | 212 | 0.31 % | 0.64 % | 23 |
| sweep, grid, every frame, look = reach | 8 | 1.00 | 9.1 | 22.7 | 7.00 | 3467 | 139 | 246 | 0.42 % | 0.74 % | 23 |
| sweep, grid, every frame, look = speed x dt | 8 | 1.00 | 1.0 | 7.0 | 7.00 | 1402 | 56 | 73 | 0.17 % | 0.22 % | 23 |
| sweep, grid, tick 0.10 s, look = reach | 8 | 0.33 | 1.3 | 6.2 | 2.33 | 771 | 31 | 158 | 0.09 % | 0.47 % | 23 |
| eyes: 5 rays, tick 0.10 s | 16 | 2.44 | 6.9 | 17.7 | 8.95 | 1875 | 150 | 285 | 0.45 % | 0.85 % | 54 |
| eyes: 9 rays, tick 0.10 s | 16 | 3.77 | 12.6 | 31.0 | 11.84 | 3131 | 250 | 466 | 0.75 % | 1.40 % | 55 |
| eyes: 13 rays, tick 0.10 s | 16 | 5.11 | 15.0 | 44.4 | 15.79 | 4325 | 346 | 666 | 1.04 % | 2.00 % | 53 |
| sweep, every triangle, every frame, look = reach | 16 | 1.00 | 5.8 | 366.2 | 15.00 | 10760 | 861 | 971 | 2.58 % | 2.91 % | 52 |
| sweep, object boxes, every frame, look = reach | 16 | 1.00 | 5.8 | 25.2 | 15.00 | 3988 | 319 | 454 | 0.96 % | 1.36 % | 52 |
| sweep, grid, every frame, look = reach | 16 | 1.00 | 4.7 | 17.9 | 15.00 | 3512 | 281 | 443 | 0.84 % | 1.33 % | 52 |
| sweep, grid, every frame, look = speed x dt | 16 | 1.00 | 0.6 | 6.8 | 15.00 | 2426 | 194 | 217 | 0.58 % | 0.65 % | 52 |
| sweep, grid, tick 0.10 s, look = reach | 16 | 0.33 | 1.7 | 5.7 | 5.00 | 1222 | 98 | 234 | 0.29 % | 0.70 % | 52 |
| eyes: 5 rays, tick 0.10 s | 32 | 2.45 | 8.2 | 17.8 | 18.16 | 2603 | 417 | 647 | 1.25 % | 1.94 % | 139 |
| eyes: 9 rays, tick 0.10 s | 32 | 3.78 | 14.6 | 31.0 | 24.37 | 4355 | 697 | 1212 | 2.09 % | 3.64 % | 144 |
| eyes: 13 rays, tick 0.10 s | 32 | 5.11 | 21.8 | 44.3 | 31.56 | 6296 | 1007 | 1846 | 3.02 % | 5.54 % | 146 |
| sweep, every triangle, every frame, look = reach | 32 | 1.00 | 7.2 | 364.8 | 31.00 | 13254 | 2121 | 2270 | 6.36 % | 6.81 % | 130 |
| sweep, object boxes, every frame, look = reach | 32 | 1.00 | 7.2 | 26.1 | 31.00 | 6530 | 1045 | 1221 | 3.13 % | 3.66 % | 130 |
| sweep, grid, every frame, look = reach | 32 | 1.00 | 7.1 | 20.4 | 31.00 | 6294 | 1007 | 1218 | 3.02 % | 3.65 % | 130 |
| sweep, grid, every frame, look = speed x dt | 32 | 1.00 | 0.9 | 7.5 | 31.00 | 4701 | 752 | 789 | 2.26 % | 2.37 % | 130 |
| sweep, grid, tick 0.10 s, look = reach | 32 | 0.33 | 2.2 | 6.1 | 10.33 | 2083 | 333 | 566 | 1.00 % | 1.70 % | 130 |

## Delta against a collision sweep: frames of 23.0 ms, 60 s scripted run

Per agent per frame (mean): casts, triangle tests, broad-phase tests, agent-pair tests, cycles. Then all agents: us a frame (mean, worst frame), the share of the frame, and the shared push + separation.

| Mode | Agents | Casts | Triangle tests | Broad-phase tests | Pair tests | Cycles an agent a frame | us a frame | us worst frame | Share, mean | Share, worst | Shared us a frame |
|---|---|---|---|---|---|---|---|---|---|---|---|
| eyes: 5 rays, tick 0.10 s | 4 | 1.70 | 2.3 | 12.4 | 0.98 | 771 | 15 | 59 | 0.07 % | 0.26 % | 11 |
| eyes: 9 rays, tick 0.10 s | 4 | 2.62 | 2.8 | 21.6 | 1.24 | 1180 | 24 | 92 | 0.10 % | 0.40 % | 11 |
| eyes: 13 rays, tick 0.10 s | 4 | 3.53 | 14.8 | 30.7 | 1.31 | 2281 | 46 | 232 | 0.20 % | 1.01 % | 11 |
| sweep, every triangle, every frame, look = reach | 4 | 1.00 | 4.8 | 367.2 | 3.00 | 8918 | 178 | 251 | 0.78 % | 1.09 % | 11 |
| sweep, object boxes, every frame, look = reach | 4 | 1.00 | 4.8 | 18.7 | 3.00 | 1998 | 40 | 119 | 0.17 % | 0.52 % | 11 |
| sweep, grid, every frame, look = reach | 4 | 1.00 | 5.0 | 16.9 | 3.00 | 1886 | 38 | 115 | 0.16 % | 0.50 % | 11 |
| sweep, grid, every frame, look = speed x dt | 4 | 1.00 | 0.2 | 5.9 | 3.00 | 658 | 13 | 31 | 0.06 % | 0.14 % | 11 |
| sweep, grid, tick 0.10 s, look = reach | 4 | 0.23 | 1.1 | 4.4 | 0.69 | 475 | 10 | 80 | 0.04 % | 0.35 % | 11 |
| eyes: 5 rays, tick 0.10 s | 8 | 1.70 | 4.2 | 12.4 | 3.03 | 1064 | 43 | 130 | 0.19 % | 0.57 % | 23 |
| eyes: 9 rays, tick 0.10 s | 8 | 2.61 | 4.6 | 21.5 | 3.98 | 1561 | 62 | 185 | 0.27 % | 0.80 % | 23 |
| eyes: 13 rays, tick 0.10 s | 8 | 3.52 | 11.9 | 30.6 | 4.43 | 2420 | 97 | 373 | 0.42 % | 1.62 % | 24 |
| sweep, every triangle, every frame, look = reach | 8 | 1.00 | 3.6 | 368.4 | 7.00 | 9218 | 369 | 444 | 1.60 % | 1.93 % | 23 |
| sweep, object boxes, every frame, look = reach | 8 | 1.00 | 3.6 | 18.1 | 7.00 | 2262 | 90 | 189 | 0.39 % | 0.82 % | 23 |
| sweep, grid, every frame, look = reach | 8 | 1.00 | 7.0 | 21.8 | 7.00 | 2985 | 119 | 226 | 0.52 % | 0.98 % | 23 |
| sweep, grid, every frame, look = speed x dt | 8 | 1.00 | 0.5 | 8.7 | 7.00 | 1331 | 53 | 74 | 0.23 % | 0.32 % | 23 |
| sweep, grid, tick 0.10 s, look = reach | 8 | 0.23 | 1.3 | 4.3 | 1.61 | 626 | 25 | 137 | 0.11 % | 0.60 % | 23 |
| eyes: 5 rays, tick 0.10 s | 16 | 1.69 | 4.4 | 12.2 | 6.18 | 1289 | 103 | 271 | 0.45 % | 1.18 % | 57 |
| eyes: 9 rays, tick 0.10 s | 16 | 2.61 | 6.3 | 21.4 | 8.78 | 2111 | 169 | 440 | 0.73 % | 1.91 % | 53 |
| eyes: 13 rays, tick 0.10 s | 16 | 3.53 | 11.1 | 30.6 | 10.71 | 3015 | 241 | 640 | 1.05 % | 2.78 % | 53 |
| sweep, every triangle, every frame, look = reach | 16 | 1.00 | 5.3 | 366.7 | 15.00 | 10672 | 854 | 981 | 3.71 % | 4.26 % | 52 |
| sweep, object boxes, every frame, look = reach | 16 | 1.00 | 5.3 | 22.1 | 15.00 | 3830 | 306 | 462 | 1.33 % | 2.01 % | 52 |
| sweep, grid, every frame, look = reach | 16 | 1.00 | 6.0 | 18.1 | 15.00 | 3785 | 303 | 466 | 1.32 % | 2.03 % | 52 |
| sweep, grid, every frame, look = speed x dt | 16 | 1.00 | 0.7 | 7.3 | 15.00 | 2458 | 197 | 224 | 0.86 % | 0.97 % | 52 |
| sweep, grid, tick 0.10 s, look = reach | 16 | 0.23 | 1.1 | 4.0 | 3.45 | 834 | 67 | 217 | 0.29 % | 0.94 % | 52 |
| eyes: 5 rays, tick 0.10 s | 32 | 1.69 | 6.0 | 12.2 | 12.35 | 1804 | 289 | 669 | 1.25 % | 2.91 % | 141 |
| eyes: 9 rays, tick 0.10 s | 32 | 2.61 | 8.3 | 21.4 | 16.46 | 2857 | 457 | 1092 | 1.99 % | 4.75 % | 140 |
| eyes: 13 rays, tick 0.10 s | 32 | 3.53 | 13.6 | 30.6 | 21.45 | 4218 | 675 | 1489 | 2.93 % | 6.47 % | 138 |
| sweep, every triangle, every frame, look = reach | 32 | 1.00 | 7.1 | 364.9 | 31.00 | 13234 | 2117 | 2314 | 9.21 % | 10.06 % | 130 |
| sweep, object boxes, every frame, look = reach | 32 | 1.00 | 7.1 | 35.8 | 31.00 | 6703 | 1072 | 1275 | 4.66 % | 5.55 % | 130 |
| sweep, grid, every frame, look = reach | 32 | 1.00 | 9.0 | 20.3 | 31.00 | 6697 | 1071 | 1322 | 4.66 % | 5.75 % | 133 |
| sweep, grid, every frame, look = speed x dt | 32 | 1.00 | 0.8 | 7.8 | 31.00 | 4680 | 749 | 785 | 3.26 % | 3.41 % | 130 |
| sweep, grid, tick 0.10 s, look = reach | 32 | 0.23 | 1.2 | 4.2 | 7.13 | 1381 | 221 | 478 | 0.96 % | 2.08 % | 130 |

## Delta against a collision sweep: frames of 75.0 ms, 60 s scripted run

Per agent per frame (mean): casts, triangle tests, broad-phase tests, agent-pair tests, cycles. Then all agents: us a frame (mean, worst frame), the share of the frame, and the shared push + separation.

| Mode | Agents | Casts | Triangle tests | Broad-phase tests | Pair tests | Cycles an agent a frame | us a frame | us worst frame | Share, mean | Share, worst | Shared us a frame |
|---|---|---|---|---|---|---|---|---|---|---|---|
| eyes: 5 rays, tick 0.10 s | 4 | 5.54 | 6.1 | 40.4 | 3.37 | 2356 | 47 | 78 | 0.06 % | 0.10 % | 11 |
| eyes: 9 rays, tick 0.10 s | 4 | 8.56 | 10.1 | 70.6 | 5.07 | 3968 | 79 | 130 | 0.11 % | 0.17 % | 11 |
| eyes: 13 rays, tick 0.10 s | 4 | 11.52 | 14.9 | 100.2 | 7.60 | 5722 | 114 | 191 | 0.15 % | 0.25 % | 11 |
| sweep, every triangle, every frame, look = reach | 4 | 1.00 | 5.1 | 366.9 | 3.00 | 8969 | 179 | 237 | 0.24 % | 0.32 % | 11 |
| sweep, object boxes, every frame, look = reach | 4 | 1.00 | 5.1 | 21.6 | 3.00 | 2113 | 42 | 108 | 0.06 % | 0.14 % | 11 |
| sweep, grid, every frame, look = reach | 4 | 1.00 | 5.3 | 19.3 | 3.00 | 2010 | 40 | 156 | 0.05 % | 0.21 % | 11 |
| sweep, grid, every frame, look = speed x dt | 4 | 1.00 | 1.1 | 6.9 | 3.00 | 869 | 17 | 24 | 0.02 % | 0.03 % | 11 |
| sweep, grid, tick 0.10 s, look = reach | 4 | 0.75 | 3.0 | 12.0 | 2.25 | 1264 | 25 | 128 | 0.03 % | 0.17 % | 11 |
| eyes: 5 rays, tick 0.10 s | 8 | 5.50 | 7.7 | 40.0 | 9.36 | 2908 | 116 | 182 | 0.16 % | 0.24 % | 23 |
| eyes: 9 rays, tick 0.10 s | 8 | 8.52 | 15.8 | 70.2 | 12.69 | 5009 | 200 | 335 | 0.27 % | 0.45 % | 23 |
| eyes: 13 rays, tick 0.10 s | 8 | 11.52 | 26.9 | 100.1 | 13.70 | 6958 | 278 | 436 | 0.37 % | 0.58 % | 24 |
| sweep, every triangle, every frame, look = reach | 8 | 1.00 | 3.8 | 368.2 | 7.00 | 9264 | 371 | 456 | 0.49 % | 0.61 % | 23 |
| sweep, object boxes, every frame, look = reach | 8 | 1.00 | 3.8 | 19.1 | 7.00 | 2332 | 93 | 199 | 0.12 % | 0.26 % | 23 |
| sweep, grid, every frame, look = reach | 8 | 1.00 | 5.4 | 18.9 | 7.00 | 2585 | 103 | 204 | 0.14 % | 0.27 % | 23 |
| sweep, grid, every frame, look = speed x dt | 8 | 1.00 | 1.1 | 9.0 | 7.00 | 1459 | 58 | 88 | 0.08 % | 0.12 % | 23 |
| sweep, grid, tick 0.10 s, look = reach | 8 | 0.75 | 3.6 | 13.6 | 5.25 | 1830 | 73 | 201 | 0.10 % | 0.27 % | 23 |
| eyes: 5 rays, tick 0.10 s | 16 | 5.52 | 12.7 | 40.1 | 20.19 | 4017 | 321 | 485 | 0.43 % | 0.65 % | 55 |
| eyes: 9 rays, tick 0.10 s | 16 | 8.49 | 27.8 | 69.8 | 27.46 | 7072 | 566 | 869 | 0.75 % | 1.16 % | 56 |
| eyes: 13 rays, tick 0.10 s | 16 | 11.48 | 42.5 | 99.8 | 34.65 | 10105 | 808 | 1207 | 1.08 % | 1.61 % | 53 |
| sweep, every triangle, every frame, look = reach | 16 | 1.00 | 4.2 | 367.8 | 15.00 | 10440 | 835 | 971 | 1.11 % | 1.29 % | 52 |
| sweep, object boxes, every frame, look = reach | 16 | 1.00 | 4.2 | 19.9 | 15.00 | 3532 | 283 | 448 | 0.38 % | 0.60 % | 52 |
| sweep, grid, every frame, look = reach | 16 | 1.00 | 3.9 | 18.3 | 15.00 | 3331 | 266 | 447 | 0.36 % | 0.60 % | 52 |
| sweep, grid, every frame, look = speed x dt | 16 | 1.00 | 1.0 | 8.0 | 15.00 | 2517 | 201 | 228 | 0.27 % | 0.30 % | 52 |
| sweep, grid, tick 0.10 s, look = reach | 16 | 0.75 | 2.9 | 13.6 | 11.25 | 2499 | 200 | 331 | 0.27 % | 0.44 % | 52 |
| eyes: 5 rays, tick 0.10 s | 32 | 5.51 | 17.9 | 39.9 | 40.89 | 5771 | 923 | 1189 | 1.23 % | 1.59 % | 139 |
| eyes: 9 rays, tick 0.10 s | 32 | 8.52 | 36.7 | 69.9 | 56.06 | 10163 | 1626 | 2155 | 2.17 % | 2.87 % | 147 |
| eyes: 13 rays, tick 0.10 s | 32 | 11.50 | 42.9 | 99.8 | 67.70 | 13275 | 2124 | 2782 | 2.83 % | 3.71 % | 142 |
| sweep, every triangle, every frame, look = reach | 32 | 1.00 | 5.2 | 366.8 | 31.00 | 12856 | 2057 | 2242 | 2.74 % | 2.99 % | 130 |
| sweep, object boxes, every frame, look = reach | 32 | 1.00 | 5.2 | 22.1 | 31.00 | 6013 | 962 | 1185 | 1.28 % | 1.58 % | 130 |
| sweep, grid, every frame, look = reach | 32 | 1.00 | 7.5 | 19.0 | 31.00 | 6345 | 1015 | 1145 | 1.35 % | 1.53 % | 130 |
| sweep, grid, every frame, look = speed x dt | 32 | 1.00 | 0.7 | 8.6 | 31.00 | 4683 | 749 | 811 | 1.00 % | 1.08 % | 130 |
| sweep, grid, tick 0.10 s, look = reach | 32 | 0.75 | 3.7 | 12.7 | 23.25 | 4311 | 690 | 883 | 0.92 % | 1.18 % | 130 |

## The eyes alone: rays and tick (8 agents, frames of 33.3 ms)

| Mode | Agents | Casts | Triangle tests | Broad-phase tests | Pair tests | Cycles an agent a frame | us a frame | us worst frame | Share, mean | Share, worst | Shared us a frame |
|---|---|---|---|---|---|---|---|---|---|---|---|
| eyes: 5 rays, tick 0.05 s | 8 | 4.87 | 6.9 | 35.4 | 7.44 | 2469 | 99 | 186 | 0.30 % | 0.56 % | 23 |
| eyes: 5 rays, tick 0.10 s | 8 | 2.45 | 2.9 | 17.8 | 4.22 | 1293 | 52 | 97 | 0.16 % | 0.29 % | 23 |
| eyes: 5 rays, tick 0.20 s | 8 | 1.23 | 3.6 | 8.9 | 1.91 | 774 | 31 | 86 | 0.09 % | 0.26 % | 26 |
| eyes: 5 rays, tick 0.40 s | 8 | 0.61 | 2.6 | 4.5 | 1.58 | 545 | 22 | 95 | 0.07 % | 0.29 % | 25 |
| eyes: 9 rays, tick 0.05 s | 8 | 7.56 | 26.1 | 62.2 | 9.69 | 4984 | 199 | 386 | 0.60 % | 1.16 % | 24 |
| eyes: 9 rays, tick 0.10 s | 8 | 3.77 | 8.1 | 31.0 | 5.43 | 2280 | 91 | 249 | 0.27 % | 0.75 % | 23 |
| eyes: 9 rays, tick 0.20 s | 8 | 1.89 | 8.5 | 15.5 | 2.84 | 1456 | 58 | 185 | 0.17 % | 0.56 % | 26 |
| eyes: 9 rays, tick 0.40 s | 8 | 0.95 | 7.4 | 7.8 | 2.83 | 1141 | 46 | 178 | 0.14 % | 0.54 % | 29 |
| eyes: 13 rays, tick 0.05 s | 8 | 10.22 | 28.8 | 88.8 | 12.57 | 6542 | 262 | 441 | 0.79 % | 1.32 % | 23 |
| eyes: 13 rays, tick 0.10 s | 8 | 5.12 | 19.9 | 44.4 | 6.34 | 3642 | 146 | 406 | 0.44 % | 1.22 % | 24 |
| eyes: 13 rays, tick 0.20 s | 8 | 2.56 | 6.8 | 22.3 | 3.86 | 1744 | 70 | 190 | 0.21 % | 0.57 % | 23 |
| eyes: 13 rays, tick 0.40 s | 8 | 1.28 | 11.5 | 11.1 | 3.66 | 1638 | 66 | 247 | 0.20 % | 0.74 % | 28 |

## One tick of one agent (9 rays, 0.1 s; means over the 8-agent run)

- Rays a tick: 11.30 (9 in the fan, 2 down, 0.30 to the viewer).
- Box tests: 93.0. Triangle tests: 24.4. Agent distance looks: 7.0; ray-against-agent tests: 9.3.
- Cycles a tick: 6721 = 33.6 us (ESTIMATE). With the rays cast between other work, add up to 164 cycles a ray (SH4-062): 8575 cycles, 42.9 us.
- Ticks that fit in 1 ms: 30. Agents that fit in 1 ms a frame at 30 fps, mean (0.3 ticks an agent a frame): 88; if every agent ticked on one frame: 30.
- Worst frame seen with 8 agents: 4 of them ticked together.

## Outcome on the same scripted run (60 s, frames of 33.3 ms)

Through: frames in which a body's centre was inside a footprint. Into the room / a hen: the deepest overlap left after the push, m. Pushed: how far the push moved bodies in all, m (the less, the better the steering). Walked: the distance covered in all, m.

| Mode | Agents | Through | Into the room | Into a hen | Pushed | Walked |
|---|---|---|---|---|---|---|
| nothing | 4 | 307 | 0.1500 | 0.134 | 0.00 | 59.9 |
| push only | 4 | 0 | 0.0000 | 0.000 | 13.69 | 51.4 |
| eyes: 5 rays, tick 0.10 s | 4 | 0 | 0.0000 | 0.000 | 0.00 | 54.2 |
| eyes: 9 rays, tick 0.10 s | 4 | 0 | 0.0000 | 0.000 | 0.44 | 56.5 |
| eyes: 13 rays, tick 0.10 s | 4 | 0 | 0.0000 | 0.000 | 0.04 | 56.5 |
| sweep, grid, every frame, look = reach | 4 | 0 | 0.0000 | 0.000 | 0.00 | 49.7 |
| sweep, grid, every frame, look = speed x dt | 4 | 0 | 0.0000 | 0.000 | 0.09 | 21.7 |
| sweep, grid, tick 0.10 s, look = reach | 4 | 0 | 0.0000 | 0.000 | 0.00 | 49.5 |
| nothing | 8 | 768 | 0.1500 | 0.184 | 0.00 | 119.7 |
| push only | 8 | 0 | 0.0000 | 0.040 | 25.56 | 104.2 |
| eyes: 5 rays, tick 0.10 s | 8 | 0 | 0.0000 | 0.000 | 0.41 | 104.5 |
| eyes: 9 rays, tick 0.10 s | 8 | 0 | 0.0000 | 0.000 | 0.99 | 101.8 |
| eyes: 13 rays, tick 0.10 s | 8 | 0 | 0.0000 | 0.000 | 7.24 | 103.6 |
| sweep, grid, every frame, look = reach | 8 | 0 | 0.0000 | 0.000 | 0.00 | 102.2 |
| sweep, grid, every frame, look = speed x dt | 8 | 0 | 0.0000 | 0.000 | 7.17 | 76.6 |
| sweep, grid, tick 0.10 s, look = reach | 8 | 0 | 0.0000 | 0.000 | 0.00 | 102.9 |
| nothing | 16 | 1142 | 0.1500 | 0.296 | 0.00 | 239.2 |
| push only | 16 | 0 | 0.0000 | 0.005 | 27.76 | 222.6 |
| eyes: 5 rays, tick 0.10 s | 16 | 0 | 0.0000 | 0.000 | 5.47 | 188.0 |
| eyes: 9 rays, tick 0.10 s | 16 | 0 | 0.0000 | 0.000 | 13.74 | 194.4 |
| eyes: 13 rays, tick 0.10 s | 16 | 0 | 0.0000 | 0.000 | 19.93 | 194.3 |
| sweep, grid, every frame, look = reach | 16 | 0 | 0.0000 | 0.000 | 0.00 | 181.4 |
| sweep, grid, every frame, look = speed x dt | 16 | 0 | 0.0000 | 0.000 | 5.62 | 184.7 |
| sweep, grid, tick 0.10 s, look = reach | 16 | 0 | 0.0000 | 0.000 | 0.00 | 154.5 |
| nothing | 32 | 2640 | 0.1500 | 0.296 | 0.00 | 478.4 |
| push only | 32 | 0 | 0.0000 | 0.026 | 65.70 | 438.4 |
| eyes: 5 rays, tick 0.10 s | 32 | 0 | 0.0000 | 0.000 | 15.47 | 291.6 |
| eyes: 9 rays, tick 0.10 s | 32 | 0 | 0.0000 | 0.005 | 17.42 | 333.9 |
| eyes: 13 rays, tick 0.10 s | 32 | 0 | 0.0000 | 0.029 | 22.58 | 346.2 |
| sweep, grid, every frame, look = reach | 32 | 0 | 0.0000 | 0.000 | 0.00 | 196.9 |
| sweep, grid, every frame, look = speed x dt | 32 | 0 | 0.0000 | 0.000 | 20.01 | 312.9 |
| sweep, grid, tick 0.10 s, look = reach | 32 | 0 | 0.0000 | 0.000 | 0.00 | 233.2 |

## Paths: eyes against the sweep, the same hens and seed (8 agents)

- Mean distance between a hen's place under the eyes and under the sweep: 0.30 m after 5 s; 1.70 m over the run; 6.92 m at the most.
- The walk has random turns, so two steering rules part for good once one of them turns a hen: the paths are not the same paths. What is the same is the outcome table above.
- Distance walked: eyes 101.8 m, sweep 102.2 m.

## One walker straight at something (frames of 33.3 ms)

First reaction: the gap to the nearest footprint when it first turns or slows, m. Least gap: the nearest it came (0: touching). Pushed: what the push had to move it, m. Time to turn 45 degrees, s.

| Case | Mode | Speed m/s | First reaction at | Least gap | Pushed | Turned 45 degrees after |
|---|---|---|---|---|---|---|
| a wall, walking | eyes: 5 rays, tick 0.10 s | 0.5 | 0.98 | 0.155 | 0.000 | 1.47 |
| a wall, walking | eyes: 9 rays, tick 0.10 s | 0.5 | 0.98 | 0.448 | 0.000 | 1.57 |
| a wall, walking | eyes: 13 rays, tick 0.10 s | 0.5 | 0.98 | 0.422 | 0.000 | 1.60 |
| a wall, walking | eyes: 9 rays, tick 0.40 s | 0.5 | 0.69 | 0.502 | 0.000 | 0.40 |
| a wall, walking | sweep, grid, every frame, look = reach | 0.5 | 0.98 | 0.243 | 0.000 | 1.37 |
| a wall, walking | sweep, grid, every frame, look = speed x dt | 0.5 | 0.10 | 0.050 | 0.000 | 2.90 |
| a wall, walking | sweep, grid, tick 0.10 s, look = reach | 0.5 | 0.98 | 0.273 | 0.000 | 1.57 |
| a wall, running | eyes: 5 rays, tick 0.10 s | 2.0 | 0.98 | 0.278 | 0.000 | 0.87 |
| a wall, running | eyes: 9 rays, tick 0.10 s | 2.0 | 0.98 | 0.235 | 0.000 | 0.93 |
| a wall, running | eyes: 13 rays, tick 0.10 s | 2.0 | 0.98 | 0.174 | 0.000 | 0.97 |
| a wall, running | eyes: 9 rays, tick 0.40 s | 2.0 | 0.85 | -0.000 | 0.304 | 0.80 |
| a wall, running | sweep, grid, every frame, look = reach | 2.0 | 0.98 | 0.053 | 0.000 | 0.73 |
| a wall, running | sweep, grid, every frame, look = speed x dt | 2.0 | 0.05 | 0.048 | 0.000 | 1.20 |
| a wall, running | sweep, grid, tick 0.10 s, look = reach | 2.0 | 0.98 | 0.113 | 0.000 | 0.77 |
| a 4 cm post, 10 cm off the path | eyes: 5 rays, tick 0.10 s | 0.5 | 0.03 | -0.000 | 0.002 | 5.57 |
| a 4 cm post, 10 cm off the path | eyes: 9 rays, tick 0.10 s | 0.5 | 0.27 | 0.136 | 0.000 | 5.57 |
| a 4 cm post, 10 cm off the path | eyes: 13 rays, tick 0.10 s | 0.5 | 0.52 | 0.168 | 0.000 | 5.20 |
| a 4 cm post, 10 cm off the path | eyes: 9 rays, tick 0.40 s | 0.5 | 0.14 | 0.130 | 0.000 | 2.73 |
| a 4 cm post, 10 cm off the path | sweep, grid, every frame, look = reach | 0.5 | 0.36 | 0.107 | 0.000 | 4.63 |
| a 4 cm post, 10 cm off the path | sweep, grid, every frame, look = speed x dt | 0.5 | 0.11 | 0.053 | 0.278 | did not turn 45 degrees |
| a 4 cm post, 10 cm off the path | sweep, grid, tick 0.10 s, look = reach | 0.5 | 0.64 | 0.158 | 0.000 | 4.40 |
| a 4 cm post, 10 cm off the path, running | eyes: 5 rays, tick 0.10 s | 2.0 | 0.62 | 0.394 | 0.000 | 2.00 |
| a 4 cm post, 10 cm off the path, running | eyes: 9 rays, tick 0.10 s | 2.0 | 0.73 | -0.000 | 0.084 | 1.77 |
| a 4 cm post, 10 cm off the path, running | eyes: 13 rays, tick 0.10 s | 2.0 | 1.02 | -0.000 | 0.070 | 1.77 |
| a 4 cm post, 10 cm off the path, running | eyes: 9 rays, tick 0.40 s | 2.0 | 0.72 | 0.340 | 0.000 | 0.83 |
| a 4 cm post, 10 cm off the path, running | sweep, grid, every frame, look = reach | 2.0 | 0.66 | 0.130 | 0.148 | 1.73 |
| a 4 cm post, 10 cm off the path, running | sweep, grid, every frame, look = speed x dt | 2.0 | 0.11 | 0.059 | 8.371 | 1.17 |
| a 4 cm post, 10 cm off the path, running | sweep, grid, tick 0.10 s, look = reach | 2.0 | 0.64 | 0.118 | 0.000 | 1.73 |
