/* SH4ZAM-DCE tests: shz_dce_xform.h, the loops its cycle ESTIMATES are made from

   xform_bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc with the fast-math flags and run by tests/sh4run/sh4bench: every bench_ function is
   `void f(void *out, const void *in0, const void *in1, size_t count)` on buffers of random floats in [-1, 1) (32 bytes a vertex; in1 takes
   the clip codes). The routine, its plain C form (the loop it replaces), for each of the three modes.
*/

#include <sh4zam_dce/shz_dce_xform.h>

#define R SHZ_RESTRICT

static const shz_dce_clip_t bench_clip = { 1.2f, 0.9f, 2.4f, 1.8f, 0.1f, 400.0f };
unsigned bench_sink;

void bench_xform_none(float *R out, const float *R in, uint8_t *R codes, size_t n) { bench_sink = shz_dce_xform_vertex32(out, codes, in, n, SHZ_DCE_XFORM_NONE, &bench_clip); }
void bench_xform_none_c(float *R out, const float *R in, uint8_t *R codes, size_t n) { bench_sink = shz_dce_xform_vertex32_c(out, codes, in, n, SHZ_DCE_XFORM_NONE, &bench_clip); }
void bench_xform_sides(float *R out, const float *R in, uint8_t *R codes, size_t n) { bench_sink = shz_dce_xform_vertex32(out, codes, in, n, SHZ_DCE_XFORM_SIDES, &bench_clip); }
void bench_xform_sides_c(float *R out, const float *R in, uint8_t *R codes, size_t n) { bench_sink = shz_dce_xform_vertex32_c(out, codes, in, n, SHZ_DCE_XFORM_SIDES, &bench_clip); }
void bench_xform_full(float *R out, const float *R in, uint8_t *R codes, size_t n) { bench_sink = shz_dce_xform_vertex32(out, codes, in, n, SHZ_DCE_XFORM_FULL, &bench_clip); }
void bench_xform_full_c(float *R out, const float *R in, uint8_t *R codes, size_t n) { bench_sink = shz_dce_xform_vertex32_c(out, codes, in, n, SHZ_DCE_XFORM_FULL, &bench_clip); }
