/* SH4ZAM-DCE tests: shz_dce_xmtrx.h, the loops its cycle ESTIMATES are made from

   xmtrx_bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc with the fast-math flags and run by tests/sh4run/sh4bench: every bench_ function is
   `void f(void *out, const void *in0, const void *in1, size_t count)` on buffers of random floats in [-1, 1).
   For each routine: the routine; its plain C form (the loop of SH4ZAM's single call that it replaces). For the projection also
   the same loop with a divide for 1 / W. Records of 16 bytes in both arrays (a vector of four floats, the fourth not used by
   the three-float routines), and of 12 bytes for the rows named _12.
*/

#include <sh4zam_dce/shz_dce_xmtrx.h>

#define R SHZ_RESTRICT

/* a view from 3 units back: W = z + 3, in 2 .. 4 for the bench's inputs */
static const shz_mat4x4_t bench_view = { .elem = { 300, 0, 0, 0,  0, -300, 0, 0,  320, 240, 1, 1,  960, 720, 3, 3 } };

static void project_divide_loop(float *R out, const float *R in, size_t n) {
    size_t i;

    for (i = 0; i < n; i++) {
        const float *s = in + 4 * i;
        float *o = out + 4 * i;
        const shz_vec4_t p = shz_xmtrx_transform_vec4(shz_vec4_init(s[0], s[1], s[2], 1.0f));
        const float inv = 1.0f / p.w;

        o[0] = p.x * inv; o[1] = p.y * inv; o[2] = inv;
    }
}

#define BENCH(name, call) void name(float *R out, const float *R in, const float *R unused, size_t n) { (void)unused; shz_xmtrx_load_4x4(&bench_view); call; }

BENCH(bench_xmtrx_project, shz_dce_xmtrx_project_point3_batch(out, 16, in, 16, n))
BENCH(bench_xmtrx_project_c, shz_dce_xmtrx_project_point3_batch_c(out, 16, in, 16, n))
BENCH(bench_xmtrx_project_divide, project_divide_loop(out, in, n))
BENCH(bench_xmtrx_project_12, shz_dce_xmtrx_project_point3_batch(out, 12, in, 12, n))
BENCH(bench_xmtrx_project_12_c, shz_dce_xmtrx_project_point3_batch_c(out, 12, in, 12, n))
BENCH(bench_xmtrx_point3, shz_dce_xmtrx_transform_point3_batch(out, 16, in, 16, n))
BENCH(bench_xmtrx_point3_c, shz_dce_xmtrx_transform_point3_batch_c(out, 16, in, 16, n))
BENCH(bench_xmtrx_vec3_12, shz_dce_xmtrx_transform_vec3_batch(out, 12, in, 12, n))
BENCH(bench_xmtrx_vec3_12_c, shz_dce_xmtrx_transform_vec3_batch_c(out, 12, in, 12, n))
BENCH(bench_xmtrx_vec4, shz_dce_xmtrx_transform_vec4_batch(out, 16, in, 16, n))
BENCH(bench_xmtrx_vec4_c, shz_dce_xmtrx_transform_vec4_batch_c(out, 16, in, 16, n))
