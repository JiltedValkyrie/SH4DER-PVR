/* SH4ZAM-DCE tests: shz_dce_vertex.h, the loops its cycle ESTIMATES are made from

   vertex_bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc with the -ffast-math flags and run by tests/sh4run/sh4bench: every bench_ function is
   `void f(void *out, const void *in0, const void *in1, size_t count)`, on buffers of random floats in [-1, 1). The grid of a batch is
   read from in1 at run time (the values are masked into their ranges), so the compiler cannot fold it into constants.
   For each form: the dispatching routine on the cache-line output, its plain C form, the dispatching routine on outputs that are
   only 8- or only 4-byte aligned, and the scalar code it replaces (the obvious loop, one 16-bit field at a time, plain expression).
   The figures are the interpreter's pipeline model (an ESTIMATE, cache hits assumed), not a console's.
*/

#include <sh4zam_dce/shz_dce_vertex.h>

/* the grid from the input block: origins masked into +-2^22, shift 0..3, the scales as they come */
static shz_dce_vertex20_grid_t grid_from(const void *in1) {
    const uint32_t *w = (const uint32_t *)in1;
    const float *f = (const float *)in1;
    shz_dce_vertex20_grid_t g;
    g.origin[0] = (int32_t)(w[0] & 0x7fffffu) - 4194304;
    g.origin[1] = (int32_t)(w[1] & 0x7fffffu) - 4194304;
    g.origin[2] = (int32_t)(w[2] & 0x7fffffu) - 4194304;
    g.shift = w[3] & 3u;
    g.pos_scale = f[4];
    g.u_scale = f[5];
    g.u_base = f[6];
    g.v_scale = f[7];
    g.v_base = f[8];
    g.n_scale = f[9];
    return g;
}

/* ---- the dispatching routine: on the cache-line output (32-aligned), and on outputs of 8 and of 4 mod 32 ---- */

void bench_vertex20_unpack(void *out, const void *in0, const void *in1, size_t n) {
    const shz_dce_vertex20_grid_t g = grid_from(in1);
    shz_dce_vertex20_unpack((shz_dce_vertex32_t *)out, (const shz_dce_vertex20_t *)in0, n, &g);
}

void bench_vertex20_unpack_out8(void *out, const void *in0, const void *in1, size_t n) {
    const shz_dce_vertex20_grid_t g = grid_from(in1);
    shz_dce_vertex20_unpack((shz_dce_vertex32_t *)((char *)out + 8), (const shz_dce_vertex20_t *)in0, n, &g);
}

void bench_vertex20_unpack_out4(void *out, const void *in0, const void *in1, size_t n) {
    const shz_dce_vertex20_grid_t g = grid_from(in1);
    shz_dce_vertex20_unpack((shz_dce_vertex32_t *)((char *)out + 4), (const shz_dce_vertex20_t *)in0, n, &g);
}

/* ---- the plain C form on the cache-line output ---- */

void bench_vertex20_unpack_c(void *out, const void *in0, const void *in1, size_t n) {
    const shz_dce_vertex20_grid_t g = grid_from(in1);
    shz_dce_vertex20_unpack_c((shz_dce_vertex32_t *)out, (const shz_dce_vertex20_t *)in0, n, &g);
}

/* ---- the scalar code it replaces: the obvious loop, each 16-bit field read on its own, the plain expression for u and v ---- */

void bench_vertex20_scalar(void *out, const void *in0, const void *in1, size_t n) {
    const shz_dce_vertex20_grid_t g = grid_from(in1);
    shz_dce_vertex32_t *o = (shz_dce_vertex32_t *)out;
    const uint16_t *r = (const uint16_t *)in0;
    size_t i;

    for (i = 0; i < n; i++, r += 10) {
        const uint32_t q0 = r[0], q1 = r[1], q2 = r[2], q3 = r[3], q4 = r[4];
        const int32_t n0 = (int16_t)r[5], n1 = (int16_t)r[6];
        const uint32_t argb = (uint32_t)r[8] | ((uint32_t)r[9] << 16);

        o[i].x = (float)(g.origin[0] + (int32_t)(q0 << g.shift)) * g.pos_scale;
        o[i].y = (float)(g.origin[1] + (int32_t)(q1 << g.shift)) * g.pos_scale;
        o[i].z = (float)(g.origin[2] + (int32_t)(q2 << g.shift)) * g.pos_scale;
        o[i].u = (float)q3 * g.u_scale + g.u_base;
        o[i].v = (float)q4 * g.v_scale + g.v_base;
        o[i].nu = (float)n0 * g.n_scale;
        o[i].nv = (float)n1 * g.n_scale;
        o[i].argb = argb;
    }
}
