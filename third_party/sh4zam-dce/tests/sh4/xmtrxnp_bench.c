/* SH4ZAM-DCE tests: shz_dce_xmtrx.h built without its prefetches, the loops its cycle ESTIMATES are made from

   xmtrxnp_bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   The routines' rows of xmtrx_bench.c with SHZ_DCE_XMTRX_PREFETCH 0: what the two PREFs a vector cost when every line is in
   the cache (this table against that one).
*/

#define SHZ_DCE_XMTRX_PREFETCH 0
#include <sh4zam_dce/shz_dce_xmtrx.h>

#define R SHZ_RESTRICT

static const shz_mat4x4_t bench_view = { .elem = { 300, 0, 0, 0,  0, -300, 0, 0,  320, 240, 1, 1,  960, 720, 3, 3 } };

#define BENCH(name, call) void name(float *R out, const float *R in, const float *R unused, size_t n) { (void)unused; shz_xmtrx_load_4x4(&bench_view); call; }

BENCH(bench_xmtrxnp_project, shz_dce_xmtrx_project_point3_batch(out, 16, in, 16, n))
BENCH(bench_xmtrxnp_project_12, shz_dce_xmtrx_project_point3_batch(out, 12, in, 12, n))
BENCH(bench_xmtrxnp_point3, shz_dce_xmtrx_transform_point3_batch(out, 16, in, 16, n))
BENCH(bench_xmtrxnp_vec3_12, shz_dce_xmtrx_transform_vec3_batch(out, 12, in, 12, n))
BENCH(bench_xmtrxnp_vec4, shz_dce_xmtrx_transform_vec4_batch(out, 16, in, 16, n))
