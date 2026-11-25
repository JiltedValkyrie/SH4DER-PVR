# SH4ZAM-DCE

Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam). Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors, MIT License (`LICENSE`).

A header-only sister include of SH4ZAM for the Dreamcast's SH-4: batch routines SH4ZAM does not have, and reworked versions of some of its routines under a `shz_dce_` name with the same signature. On the SH-4 they use FTRV, FIPR and FSRRA; anywhere else they are plain C on SH4ZAM's software back end. SH4ZAM is used as its own library: put its `include` on the path next to `include/` here, then `#include <sh4zam_dce/shz_dce.h>`. `shz_dce_override.h` (not part of that include) puts SH4ZAM's own names on the reworks, one compile-time switch a group of routines (below). `KERNEL_GUIDE.md` says how a routine is written, tested and costed. Thesis: `doc/THESIS.md` (the mathematics, the method, findings about SH4ZAM, a console's results, suggestions for upstream). Handover: `doc/HANDOVER.md`. For upstream: `doc/upstream/shz_asin_acos.h`, the arcsine and arccosine as one file that needs SH4ZAM only.

Checks, all hidden and one at a time (host tests, SH-4 compile checks, the SH-4 code run instruction by instruction in `tests/sh4run`, a cycle ESTIMATE table): `python tests/run_checks.py` with the repository's Python; `--only <topic>` for one header, `--hw-model off` to run without the third party's bit-exact FIPR / FTRV / FSRRA models (`tests/third_party/sh4_fpu_approx`, its own MIT notice, tests only). `tests/sh4run` stops a run on an unaligned load or store, a register a call must keep, a cache line allocated and not filled: a pass is the "alignment: checked" of every instruction a routine executed.

| Header | Routine | Bit-identical to the scalar form / to SH4ZAM's routine | Cycles E (before -> after) |
|---|---|---|---|
| `shz_dce_planes.h` | `shz_dce_planes4_outcode_batch`, `_planes4_dist`, `_planes4_outcode` (four planes against points, one FTRV) | NO: FTRV rounds once; error <= (P + abs R) x 2^-21 | 57 -> 18 a point |
| `shz_dce_planes.h` | `shz_dce_planes6_cull_batch`, `_planes6_sphere` (a sphere against six planes, FTRV + 2 FIPR) | NO: the same bound | 89 -> 28 a sphere |
| `shz_dce_xform.h` | `shz_dce_xform_vertex32` (32-byte vertices through XMTRX with their view-space clip codes, two in flight) | YES: the same FTRV, compares against one product each | a vertex: 19 -> 14, + side bits 36 -> 22, + all seven bits 58 -> 33 |
| `shz_dce_project.h` | `shz_dce_project_vertices` (records of any stride through XMTRX into 32-byte records in one pass: near-first clip code against four independent slopes, screen point, vertices marked "not needed" left alone) | YES against the plain loop built with -ffp-contract=fast: the same FTRV, FSRRA, compares and fused sums | a vertex: with the clip tests 80 -> 33, without 45 -> 26, behind the near plane 38 -> 24; a marked vertex 11 -> 7 |
| `shz_dce_vertex.h` | `shz_dce_vertex20_unpack` (20-byte quantised vertices to 32-byte float records) | YES for position and normal; u, v one fused rounding | 67 -> 29 a vertex |
| `shz_dce_gather.h` | `shz_dce_unpack3_gather`, `_copy3_gather`, `_prefetch_gather` (scattered entries of a bit-packed or float table) | YES: integer work and 32-bit copies | 44 -> 24 a vertex (36.5 with its prefetch passes); copy 16 -> 12.6 |
| `shz_dce_quat.h` | `shz_dce_quat_nlerp_batch`, `_quat_mult_batch`, `_quat_normalize_batch` | NO: FIPR and FSRRA; 2^-19 a component, (P + abs R) x 2^-20 for the product | 71 -> 28, 55 -> 27, 31 -> 11 a quaternion |
| `shz_dce_vector.h` | `shz_dce_vec3_dot_batch`, `_vec3_normalize_batch` (FIPR, pair loads) | NO: (P + abs R) x 2^-21; about 2e-7 relative | 14 -> 7, 24 -> 12.75 a vector |
| `shz_dce_vector.h` | `shz_dce_vec3_magnitude_fipr`, `_vec4_normalize` (reworks by FIPR), `_vec3_dot4`, `_vec3_cross_batch` | NO, within 2 and 3 ulp; dot4 and cross YES (dot4: but the sign of an exact zero) | 25 -> 18, 31 -> 24; 61 -> 50 a quad; cross 20 -> 20 (no gain) |
| `shz_dce_sqrt.h` | `shz_dce_sqrtf`, `_inv_sqrtf`, `_hypotf`, `_vec2/3/4_magnitude`, `_magnitude_inv`, `_distance`, `_normalize_safe` (reworks of SH4ZAM 0.10: the zero test in FSRRA's shadow) | YES; on the zero path of a -ffast-math build: the operand, as SH4ZAM's, or +0 with the group's barrier on | model: 16 -> 14, 12 -> 11 / 9, vec3 length 25 -> 23, distance 31 -> 29. A CONSOLE SAYS NO GAIN on single calls (below): off by default |
| `shz_dce_trig.h` | `shz_dce_asinf`, `shz_dce_acosf` (reworks: one root, one quartic) | NO: within 8e-6 rad of the true angle (SH4ZAM's: 4.95e-3), no NaN past +-1 | a console: about equal in range, 57 -> 46 and 65 -> 54 over [-pi, pi]. Model: asin 31.7 -> 34.5, acos 31.8 -> 31.5 |
| `shz_dce_atan2.h` | `shz_dce_atan2f` (rework: SH4ZAM's quotient, an arctangent polynomial of 2, 5, 6 or 7 terms; `SHZ_DCE_ATAN2_TERMS`, default 5) | NO: within 1.4e-5 rad of the true angle (SH4ZAM's: 1.02e-2); 6.2e-3, 2.5e-6 or 1.0e-6 with 2, 6 or 7 terms | model: 37.5 -> 45.4 (36.5, 46.4, 49.4). Accuracy bought with cycles, not a faster routine. A console (Falco Girgis): 5 terms end the jitter of an interpolated rotation; his cycle counts are not here |
| `shz_dce_shadow.h` | `shz_dce_shadow_compare_batch` (N points against a depth map: one FTRV a point, the planar compare of one texel or four blended, the surface's two axes given per batch; reads XMTRX, does not change it) | YES to the planar compare it replaces, byte for byte on the software back end, in strict builds and with `-ffast-math -fno-unsafe-math-optimizations`; NOT claimed under plain `-ffast-math` (KERNEL_GUIDE.md section 8). SH-4: the same C; FTRV bound; identical axes take the fallback | a point, `-O3 -ffast-math`: 634 -> 489 (pcf 1, texel bias); `-O2`: 677 -> 647; pcf 0 and bias 0: 270 -> 184 at `-O3`, 281 -> 282 at `-O2` |
| `shz_dce_tri_row.h` | `shz_dce_tri_row_step` (one triangle row into a 16-bit depth row: the edges by running adds, a covered texel's depth by one FIPR; `_c` the running-add row, `_sh4` the SH-4 loop) | `_c`: YES under strict IEEE flags (the running-add row it replaces); `_sh4`: NO for the depth (a dot product against running sums), the coverage is the running sums' own, the FIPR bound (largest product + result) x 2^-23 | E a texel, -O3: covered perspective 43 -> 44, rejected 12 -> 10; covered flat 27 -> 30, rejected 11 -> 10. A gain on rejected texels only; not measured on a console |

Cycles E are estimates from a model of the SH7750 pipeline on the compiled code, every memory access a cache hit: for comparing two routines, not a console measurement. Code that must reproduce a result bit for bit takes only the routines that say YES.

## A console's verdict, and the switches

Falco Girgis ran the reworks on a console (single calls, cycles cached / uncached; `doc/THESIS.md`, section 7, has his table). The arcsine and arccosine won: 850 times closer to the true angle for the same cycles or fewer. The reordered roots, lengths and safe normalisations did not: a cycle or two slower on single calls where the model had promised a gain, and current SH4ZAM's `shz_sqrtf` is FSQRT, which has no zero test to move. The batch routines above have not been on a console yet.

So `shz_dce_override.h` renames a routine only when its group's switch is on. Each is 1 or 0, defined before the include:

| Switch | Default | SH4ZAM names it puts on the reworks |
|---|---|---|
| `SHZ_DCE_USE_ASIN_ACOS` | 1 | `shz_asinf`, `shz_acosf` (results differ from SH4ZAM's: closer, and defined past +-1) |
| `SHZ_DCE_USE_ATAN2` | 1 | `shz_atan2f` (results differ from SH4ZAM's: about 800 times closer; it costs about 8 cycles a call in the model, so a caller that does not need the accuracy sets it to 0) |
| `SHZ_DCE_USE_SQRT` | 0 | `shz_sqrtf`, `shz_hypotf` |
| `SHZ_DCE_USE_INV_SQRT` | 0 | `shz_inv_sqrtf` |
| `SHZ_DCE_USE_MAGNITUDE` | 0 | `shz_vec2/3/4_magnitude`, `shz_vec2/3/4_magnitude_inv` |
| `SHZ_DCE_USE_DISTANCE` | 0 | `shz_vec2/3/4_distance` |
| `SHZ_DCE_USE_NORMALIZE` | 0 | `shz_vec2/3/4_normalize_safe` |
| `SHZ_DCE_USE_FIPR` | 0 | `shz_vec4_normalize`; `shz_vec3_magnitude` becomes the FIPR length (results differ in the last bits) |
| `SHZ_DCE_OVERRIDE_ALL` | 0 | 1 turns on every switch above that is not set by name |

A plain `#include <sh4zam_dce/shz_dce_override.h>` therefore renames `shz_asinf`, `shz_acosf` and `shz_atan2f` and nothing else. The arctangent is on since the console's second verdict: with 5 terms an interpolated rotation that jittered with SH4ZAM's routine is steady (Falco Girgis; `doc/THESIS.md`, 2.5). (Before the switches it renamed the root family and left those two alone; the old `SHZ_DCE_OVERRIDE_APPROX` is still read.)

A second set says where a rework puts the value barrier `shz_dce_opaquef()` (an asm statement with an empty template: no instruction; what it does and costs is `doc/THESIS.md`, appendix B). On: a guarded routine returns +0 on its zero path in every build. Off: the plain constant, which a -ffast-math build swaps for the operand, as it does in SH4ZAM's own routines. `SHZ_DCE_OPAQUE` (1 or 0) sets every group not set by name; the defaults are the console's winners:

| Switch | Default | Routines |
|---|---|---|
| `SHZ_DCE_OPAQUE_INV_SQRT` | 1 | `shz_dce_inv_sqrtf` (console: 26 / 88 without, 24 / 84 with) |
| `SHZ_DCE_OPAQUE_NORMALIZE2` | 1 | `shz_dce_vec2_normalize_safe` (40 / 138 without, 40 / 133 with) |
| `SHZ_DCE_OPAQUE_SQRT` | 0 | `shz_dce_sqrtf`, `shz_dce_hypotf` (four roots in a row: 59 / 138 without, 66 / 156 with) |
| `SHZ_DCE_OPAQUE_NORMALIZE3`, `_NORMALIZE4` | 0 | `shz_dce_vec3/4_normalize_safe` (60 / 212 and 65 / 250 without, 63 / 220 and 66 / 260 with) |
| `SHZ_DCE_OPAQUE_MAGNITUDE`, `_DISTANCE` | 0 | the lengths and distances (not measured on the console; they follow the root) |
| `SHZ_DCE_OPAQUE_ASIN_ACOS` | 0 | `shz_dce_asinf`, `shz_dce_acosf` (in range a tie; over [-pi, pi] 46 / 202 and 54 / 256 without, 65 / 210 and 68 / 270 with) |

## Acknowledgments

`shz_dce_shadow.h` (`shz_dce_shadow_compare_batch`) is the batch form of the planar shadow-map compare of SH4DER-PVR, Copyright (c) 2026 Jilted Valkyrie (see the LICENSE of that project). The steps are its steps; the code is written here against SH4ZAM's public API and says so in its comments. Credit to SH4DER-PVR and its author is given here, as that project's terms ask.

`shz_dce_shadow.h` and `shz_dce_tri_row.h` are stop-gaps: when SH4ZAM has its own versions of `shz_dce_shadow_compare_batch` and `shz_dce_tri_row_step`, callers move to those and these two headers are removed.
