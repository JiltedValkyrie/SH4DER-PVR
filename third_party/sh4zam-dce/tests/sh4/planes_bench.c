/* SH4ZAM-DCE tests: shz_dce_planes.h, the loops its cycle ESTIMATES are made from

   planes_bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc with the fast-math flags and run by tests/sh4run/sh4bench: every bench_ function is
   `void f(void *out, const void *in0, const void *in1, size_t count [, float t])` on buffers of random floats in [-1, 1).
   For each routine: the routine, its plain C form on SH4ZAM's routines, and the scalar code it replaces, so the table reads
   before / after. The figures are the interpreter's pipeline model (an ESTIMATE, cache hits assumed), not a console's.
*/

#include <sh4zam_dce/shz_dce_planes.h>

#define R SHZ_RESTRICT

/* ---- four planes against points (12 B a point). XMTRX holds whatever the caller left: the timing does not depend on it. ---- */

void bench_planes4_outcode(uint8_t *R out, const float *R points, const void *unused, size_t n) {
    (void)unused;
    shz_dce_planes4_outcode_batch(out, points, 12, n);
}

void bench_planes4_outcode_c(uint8_t *R out, const float *R points, const void *unused, size_t n) {
    (void)unused;
    shz_dce_planes4_outcode_batch_c(out, points, 12, n);
}

/* the scalar form: four arbitrary planes, four multiply-add chains a point */
void bench_planes4_scalar(uint8_t *R out, const float *R points, const float *R pl, size_t n) {
    for (size_t i = 0; i < n; i++) {
        const float x = points[3 * i], y = points[3 * i + 1], z = points[3 * i + 2];
        out[i] = (uint8_t)((unsigned)(pl[0] * x + pl[1] * y + pl[2] * z + pl[3] < 0.0f) | ((unsigned)(pl[4] * x + pl[5] * y + pl[6] * z + pl[7] < 0.0f) << 1) |
                           ((unsigned)(pl[8] * x + pl[9] * y + pl[10] * z + pl[11] < 0.0f) << 2) | ((unsigned)(pl[12] * x + pl[13] * y + pl[14] * z + pl[15] < 0.0f) << 3));
    }
}

/* the same with SH4ZAM's dot product per plane */
void bench_planes4_shz_dot(uint8_t *R out, const float *R points, const shz_vec4_t *R pl, size_t n) {
    for (size_t i = 0; i < n; i++) {
        const shz_vec4_t p = shz_vec4_init(points[3 * i], points[3 * i + 1], points[3 * i + 2], 1.0f);
        out[i] = (uint8_t)((unsigned)(shz_vec4_dot(pl[0], p) < 0.0f) | ((unsigned)(shz_vec4_dot(pl[1], p) < 0.0f) << 1) |
                           ((unsigned)(shz_vec4_dot(pl[2], p) < 0.0f) << 2) | ((unsigned)(shz_vec4_dot(pl[3], p) < 0.0f) << 3));
    }
}

/* planes of a special shape need no inner product: the four side planes of a view-space frustum are x +- kx z and y +- ky z, two multiplies
   for all four. This is the scalar code the routine does NOT beat (the header says so): a figure to keep it out of the wrong place. */
void bench_planes4_view_sides_scalar(uint8_t *R out, const float *R points, const float *R k, size_t n) {
    const float kx = k[0], ky = k[1];
    for (size_t i = 0; i < n; i++) {
        const float x = points[3 * i], y = points[3 * i + 1], z = points[3 * i + 2];
        out[i] = (uint8_t)(((unsigned)(x < -kx * z) << 2) | ((unsigned)(x > kx * z) << 3) | ((unsigned)(y < -ky * z) << 4) | ((unsigned)(y > ky * z) << 5));
    }
}

/* ---- spheres against six planes (16 B a sphere: x y z radius); in1 is the six planes ---- */

void bench_planes6_cull(uint8_t *R out, const float *R spheres, const shz_dce_planes6_t *R planes, size_t n) {
    shz_dce_planes6_cull_batch(out, planes, spheres, 16, n);
}

void bench_planes6_cull_c(uint8_t *R out, const float *R spheres, const shz_dce_planes6_t *R planes, size_t n) {
    shz_dce_planes6_cull_batch_c(out, planes, spheres, 16, n);
}

void bench_planes6_scalar(uint8_t *R out, const float *R spheres, const float *R pl, size_t n) {
    for (size_t i = 0; i < n; i++) {
        const float x = spheres[4 * i], y = spheres[4 * i + 1], z = spheres[4 * i + 2], lo = -spheres[4 * i + 3];
        unsigned m = 0u;
        for (int k = 0; k < 6; k++)
            m |= (unsigned)(pl[4 * k] * x + pl[4 * k + 1] * y + pl[4 * k + 2] * z + pl[4 * k + 3] < lo) << k;
        out[i] = (uint8_t)m;
    }
}
