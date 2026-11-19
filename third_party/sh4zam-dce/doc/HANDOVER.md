# SH4ZAM-DCE handover (2026-11-18, v0.10)

Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam). One page: what is done, what is in use, what is left, how to run the checks. The mathematics, the method and the findings about SH4ZAM are in `doc/THESIS.md`; how a routine is written and tested is in `KERNEL_GUIDE.md`; the table of routines with their cycle estimates is in `README.md`.

## Done (all in this folder, all passing)

| Header | Routines | Bit-identical |
|---|---|---|
| `shz_dce_planes.h` | four planes against points (FTRV), a sphere against six planes (FTRV + 2 FIPR) | NO |
| `shz_dce_xform.h` | 32-byte vertex stream through XMTRX with clip codes (NONE / SIDES / FULL), two vertices in flight | YES |
| `shz_dce_project.h` | a vertex stream of any stride through XMTRX in one pass: near-first clip code against four slopes, screen point (FSRRA, fused sums), marked vertices left alone | YES (the plain loop built with -ffp-contract=fast) |
| `shz_dce_vertex.h` | 20-byte packed vertices to 32-byte float records; the output type is may_alias | YES (u, v: one fused rounding) |
| `shz_dce_gather.h` | scattered reads of a bit-packed table (widths 1..16 each, 32 bits in all at most), float-table copy, prefetch helpers | YES |
| `shz_dce_quat.h` | nlerp, product, normalise over arrays | NO |
| `shz_dce_vector.h` | vec3 dot and normalise over arrays, dot4, cross (reference), FIPR length, FIPR vec4 normalise | NO but dot4, cross |
| `shz_dce_sqrt.h` | reworks of SH4ZAM 0.10's guarded roots and lengths (zero test in FSRRA's shadow); a barrier switch a group | YES (zero path of a fast-math build: the operand, or +0 behind the barrier) |
| `shz_dce_trig.h` | reworks of asinf, acosf (8e-6 rad) | NO |
| `shz_dce_atan2.h` | rework of atan2f for accuracy: 1.4e-5 rad with the default 5 terms (SH4ZAM's: 1.02e-2), about 8 cycles more in the model; `SHZ_DCE_ATAN2_TERMS` 2, 5, 6, 7 (5 is the console's choice); switch `SHZ_DCE_USE_ATAN2`, on | NO |
| `shz_dce_override.h` | macros: SH4ZAM's names on the reworks, one switch a group (`SHZ_DCE_USE_...`); a plain include renames asinf, acosf and atan2f only | |
| `doc/upstream/shz_asin_acos.h` | the arcsine and arccosine as one file on SH4ZAM alone, for upstream; tested as it is | NO |

A console (single calls, reported by Falco Girgis; THESIS.md, section 7): asinf / acosf win; the reordered roots, lengths and normalisations lose a cycle or two, so they are off by default. The batch routines have not been on a console.

Tests: `tests/host` (exact references), `tests/sh4` (kernels run in the interpreter, four flag sets), `tests/sh4run` (the interpreter, its self-test, `sh4bench`, `sh4run_cli`), `tests/third_party/sh4_fpu_approx` (bit-exact FIPR / FTRV / FSRRA / FSCA models, MIT, its own notice, tests only).

## In use in the host project (its owners wire call sites; this folder is never edited for them)

- Wired: `shz_dce_xform_vertex32` (the world vertex transform; the project keeps a switch back to its old loop). Checked by execution: the project's old object code and this routine give the same bytes on 172,596 vertices, classes 0 / 1 / 2.
- Being wired by the project: the gather's two-word block in a collision ring read, behind a loader flag (stream on a multiple of 4, widths below 32 bits in all). Checked by execution on real tables: the same bytes as the project's byte reader, every ring and every vertex.
- Specified, not wired: `shz_dce_vertex20_unpack` (67 -> 29 E a vertex), the quaternion and vector batches, the override include.
- A second project copies this folder whole and wires its own call sites. `shz_dce_project_vertices` was built to the shape of its two vertex loops (24-byte records, an off-centre view, a lazy skip): 80 -> 33 E a vertex with the clip tests, 45 -> 26 without. Its compiled loops were read: the same rounded products and fused sums, so the same bits.

## Left

- Console checks nobody here could make (THESIS.md, section 6): FMAC as one fused operation, the order of an 8-byte pair move, FSRRA / FSCA latency and busy time. Every cycle figure is a model ESTIMATE until a console confirms the model; for single calls of the root family it has already said no (section 7), and why the model was wrong there is not known.
- Speed that is known and not built: the four-plane compare chain interleaved across points (18 -> about 12 E); a deeper pipeline for the vec3 normalise batch (12.75 -> about 7); asin / acos and atan2 with their polynomial in FSRRA's shadow, which needs asm (28.5 -> about 22, 37 -> about 28); the gather's general path in asm and for more than 32 bits a vertex (real tables run from 23 to 39 bits; today a wider table keeps the caller's reader); the seven-bit clip code by rotate instead of shifts (32.5 -> about 26).
- A character (skinned) vertex batch in the manner of `shz_dce_xform.h` was offered and not started.
- `shz_dce_project_vertices`, known and not built: texture coordinates from 16-bit integers (scale and offset an axis) inside the pass; loops of their own for a batch without marks (about 1.5 E a vertex); the plain C form is as slow as the plain loop (78 E), so an output off a multiple of 32 gains nothing.

## Rules that must survive

Never copy SH4ZAM code; call its API. The banner of every file is SH4ZAM's credit and copyright, nothing else. A routine says YES for bit identity only when a test compares bytes. No test takes the host C library's `fmaf` for a fused reference (the one here is a float off in a quarter of all cases; THESIS.md, section 3). A record type that a caller may view through its own type carries may_alias on its DEFINITION. A rework gets a line in `shz_dce_override.h` and in `tests/host/override_test.c`. Only passing states are published.

## Running the checks

    python tests/run_checks.py                         everything (selftest, host, compile, sh4, cycles); about two minutes
    python tests/run_checks.py --only xform sh4        one topic, one step
    python tests/run_checks.py --hw-model off          without the third party's bit-exact models
    <build>/sh4run_cli --trace <build>/<topic>_bench_O3.elf _bench_<name> 0x8c900000 0x8c800000 0x8c880000 6

It needs the repository's Python, a host gcc and sh-elf-gcc (found through the repository's configuration, or `KOS_CC_BASE` in the environment). `--repo` and `--build` default to the repository this folder sits in and its `build/sh4zam-dce`; logs are in `<build>/logs`, cycle tables in `<build>/cycles_<topic>.txt`. All runs are hidden and below normal priority, one at a time. A new topic is six files (KERNEL_GUIDE.md, sections 5 and 6); the driver finds them by name.
