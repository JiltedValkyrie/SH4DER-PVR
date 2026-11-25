/* SH4ZAM-DCE tests: shz_dce_pvr.h, the loops its cycle ESTIMATES are made from

   pvr_bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc with the fast-math flags and run by tests/sh4run/sh4bench: every bench_ function is
   `void f(void *out, const void *in0, const void *in1, size_t count)` on buffers of random floats in [-1, 1) (32 bytes a vertex).
   The routine; its plain C form (the loop it replaces, with the FSRRA reciprocal); the same loop with a divide for 1 / W.
*/

#include <sh4zam_dce/shz_dce_pvr.h>

#define R SHZ_RESTRICT

/* a view from 3 units back: W = z + 3, in 2 .. 4 for the bench's inputs */
static const shz_mat4x4_t bench_view = { .elem = { 300, 0, 0, 0,  0, -300, 0, 0,  320, 240, 1, 1,  960, 720, 3, 3 } };

static void pvr_divide_loop(float *R out, const float *R in, size_t n) {
    size_t i;

    for (i = 0; i < n; i++) {
        const float *s = in + 8 * i;
        const shz_alias_uint32_t *sw = (const shz_alias_uint32_t *)s;
        float *o = out + 8 * i;
        shz_alias_uint32_t *ow = (shz_alias_uint32_t *)o;
        const shz_vec4_t p = shz_xmtrx_transform_vec4(shz_vec4_init(s[1], s[2], s[3], 1.0f));
        const float inv = 1.0f / p.w;

        ow[0] = sw[0];
        o[1] = p.x * inv; o[2] = p.y * inv; o[3] = inv;
        ow[4] = sw[4]; ow[5] = sw[5]; ow[6] = sw[6]; ow[7] = sw[7];
    }
}

void bench_pvr_project(float *R out, const float *R in, const float *R unused, size_t n) { (void)unused; shz_xmtrx_load_4x4(&bench_view); shz_dce_pvr_project(out, in, n); }
void bench_pvr_project_c(float *R out, const float *R in, const float *R unused, size_t n) { (void)unused; shz_xmtrx_load_4x4(&bench_view); shz_dce_pvr_project_c(out, in, n); }
void bench_pvr_project_divide(float *R out, const float *R in, const float *R unused, size_t n) { (void)unused; shz_xmtrx_load_4x4(&bench_view); pvr_divide_loop(out, in, n); }
