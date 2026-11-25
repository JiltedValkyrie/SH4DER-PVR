/* SH4ZAM-DCE tests: the loops the vector cycle ESTIMATES are made from

   vector_bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc with the -ffast-math flags and run by tests/sh4run/sh4bench: every bench_ function is a loop over the elements of its
   arrays (random floats in [-1, 1)). An element is one item of the batch: a 3-vector, a 4-vector, or for dot4 a quad of four pairs.
   The "_shz" loops are SH4ZAM's routines called by value per element, as a caller writes them today. The "_dce" and "_batch" loops are the
   routines of shz_dce_vector.h; "_c" is the plain C form of the same routine. The "try_" loops are one-call FIPR forms that were measured and
   NOT shipped (they did not reach a 15% gain): their code lives here, not in the header.
   The figures are the interpreter's pipeline model (an ESTIMATE, cache hits assumed), not a console's.
*/

/* the FIPR bodies of the length and of the 4-vector normalisation are tested here: by default they are SH4ZAM's routines, called underneath */
#define SHZ_DCE_REWORK_FIPR 1
#include <sh4zam_dce/shz_dce_vector.h>

#define R SHZ_RESTRICT

/* ---- dot products of arrays of 3-vectors ---- */

void bench_dot_shz(float *R out, const shz_vec3_t *R a, const shz_vec3_t *R b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        out[i] = shz_vec3_dot(a[i], b[i]);
}

void bench_dot_c(float *R out, const shz_vec3_t *R a, const shz_vec3_t *R b, size_t n) {
    shz_dce_vec3_dot_batch_c(out, a, b, n);
}

void bench_dot_batch(float *R out, const shz_vec3_t *R a, const shz_vec3_t *R b, size_t n) {
    shz_dce_vec3_dot_batch(out, a, b, n);
}

/* ---- four dot products per element (a quad of pairs) ---- */

void bench_dot4_shz(shz_vec4_t *R out, const shz_vec3_t *R a, const shz_vec3_t *R b, size_t n) {
    size_t q;
    for (q = 0; q < n; q++) {
        const shz_vec3_t *x = a + 4 * q, *y = b + 4 * q;
        out[q] = shz_vec4_init(shz_vec3_dot(x[0], y[0]), shz_vec3_dot(x[1], y[1]), shz_vec3_dot(x[2], y[2]), shz_vec3_dot(x[3], y[3]));
    }
}

void bench_dot4_dce(shz_vec4_t *R out, const shz_vec3_t *R a, const shz_vec3_t *R b, size_t n) {
    size_t q;
    for (q = 0; q < n; q++)
        out[q] = shz_dce_vec3_dot4(a + 4 * q, b + 4 * q);
}

/* ---- cross products ---- */

void bench_cross_shz(shz_vec3_t *R out, const shz_vec3_t *R a, const shz_vec3_t *R b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        out[i] = shz_vec3_cross(a[i], b[i]);
}

void bench_cross_batch(shz_vec3_t *R out, const shz_vec3_t *R a, const shz_vec3_t *R b, size_t n) {
    shz_dce_vec3_cross_batch(out, a, b, n);
}

/* experiments on the cross product's loop shape (not in the header): restrict on every pointer, and two elements an iteration */
void bench_cross_restrict(shz_vec3_t *R out, const shz_vec3_t *R a, const shz_vec3_t *R b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        const float ax = a[i].x, ay = a[i].y, az = a[i].z;
        const float bx = b[i].x, by = b[i].y, bz = b[i].z;
        out[i].x = ay * bz - az * by;
        out[i].y = az * bx - ax * bz;
        out[i].z = ax * by - ay * bx;
    }
}

void bench_cross_restrict_u2(shz_vec3_t *R out, const shz_vec3_t *R a, const shz_vec3_t *R b, size_t n) {
    size_t i = 0;
    for (; i + 1 < n; i += 2) {
        const float ax0 = a[i].x, ay0 = a[i].y, az0 = a[i].z, bx0 = b[i].x, by0 = b[i].y, bz0 = b[i].z;
        const float ax1 = a[i + 1].x, ay1 = a[i + 1].y, az1 = a[i + 1].z, bx1 = b[i + 1].x, by1 = b[i + 1].y, bz1 = b[i + 1].z;
        const float x0 = ay0 * bz0 - az0 * by0, y0 = az0 * bx0 - ax0 * bz0, z0 = ax0 * by0 - ay0 * bx0;
        const float x1 = ay1 * bz1 - az1 * by1, y1 = az1 * bx1 - ax1 * bz1, z1 = ax1 * by1 - ay1 * bx1;
        out[i] = shz_vec3_init(x0, y0, z0);
        out[i + 1] = shz_vec3_init(x1, y1, z1);
    }
    if (i < n) {
        const float ax = a[i].x, ay = a[i].y, az = a[i].z, bx = b[i].x, by = b[i].y, bz = b[i].z;
        out[i] = shz_vec3_init(ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx);
    }
}

/* ---- normalisation of arrays ---- */

void bench_normalize_shz(shz_vec3_t *R out, const shz_vec3_t *R in, const void *unused, size_t n) {
    size_t i;
    (void)unused;
    for (i = 0; i < n; i++)
        out[i] = shz_vec3_normalize(in[i]);
}

void bench_normalize_batch(shz_vec3_t *R out, const shz_vec3_t *R in, const void *unused, size_t n) {
    (void)unused;
    shz_dce_vec3_normalize_batch(out, in, n);
}

void bench_normalize_safe_shz(shz_vec3_t *R out, const shz_vec3_t *R in, const void *unused, size_t n) {
    size_t i;
    (void)unused;
    for (i = 0; i < n; i++)
        out[i] = shz_vec3_normalize_safe(in[i]);
}

void bench_vec4_normalize_shz(shz_vec4_t *R out, const shz_vec4_t *R in, const void *unused, size_t n) {
    size_t i;
    (void)unused;
    for (i = 0; i < n; i++)
        out[i] = shz_vec4_normalize(in[i]);
}

void bench_vec4_normalize_dce(shz_vec4_t *R out, const shz_vec4_t *R in, const void *unused, size_t n) {
    size_t i;
    (void)unused;
    for (i = 0; i < n; i++)
        out[i] = shz_dce_vec4_normalize(in[i]);
}

/* ---- lengths ---- */

void bench_magnitude_shz(float *R out, const shz_vec3_t *R in, const void *unused, size_t n) {
    size_t i;
    (void)unused;
    for (i = 0; i < n; i++)
        out[i] = shz_vec3_magnitude(in[i]);
}

void bench_magnitude_dce(float *R out, const shz_vec3_t *R in, const void *unused, size_t n) {
    size_t i;
    (void)unused;
    for (i = 0; i < n; i++)
        out[i] = shz_dce_vec3_magnitude_fipr(in[i]);
}

void bench_magnitude_fsrra_shz(float *R out, const shz_vec3_t *R in, const void *unused, size_t n) {
    size_t i;
    (void)unused;
    for (i = 0; i < n; i++)
        out[i] = shz_vec3_magnitude_fsrra(in[i]);
}

void bench_magnitude_inv_shz(float *R out, const shz_vec3_t *R in, const void *unused, size_t n) {
    size_t i;
    (void)unused;
    for (i = 0; i < n; i++)
        out[i] = shz_vec3_magnitude_inv(in[i]);
}

void bench_distance_shz(float *R out, const shz_vec3_t *R a, const shz_vec3_t *R b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        out[i] = shz_vec3_distance(a[i], b[i]);
}

/* ---- the other rows of the table: a 4-vector dot product, a lerp, a plain copy ---- */

void bench_vec4_dot_shz(float *R out, const shz_vec4_t *R a, const shz_vec4_t *R b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        out[i] = shz_vec4_dot(a[i], b[i]);
}

void bench_lerp_shz(shz_vec3_t *R out, const shz_vec3_t *R a, const shz_vec3_t *R b, size_t n, float t) {
    size_t i;
    for (i = 0; i < n; i++)
        out[i] = shz_vec3_lerp(a[i], b[i], t);
}

void bench_copy_shz(shz_vec3_t *R out, const shz_vec3_t *R in, const void *unused, size_t n) {
    size_t i;
    (void)unused;
    for (i = 0; i < n; i++)
        out[i] = in[i];
}

/* ---- measured and NOT shipped: one-call FIPR forms that did not reach a 15% gain (see the header's file comment) ---- */

SHZ_FORCE_INLINE shz_vec3_t try_normalize(shz_vec3_t vec) {
    register float x asm("fr4") = vec.x;
    register float y asm("fr5") = vec.y;
    register float z asm("fr6") = vec.z;
    register float s asm("fr7") = 0.0f;

    asm("fipr   fv4, fv4" : "+f" (s) : "f" (x), "f" (y), "f" (z));
    s = shz_inv_sqrtf_fsrra(s);
    return shz_vec3_init(x * s, y * s, z * s);
}

SHZ_FORCE_INLINE shz_vec3_t try_normalize_safe(shz_vec3_t vec) {
    register float x asm("fr4") = vec.x;
    register float y asm("fr5") = vec.y;
    register float z asm("fr6") = vec.z;
    register float s asm("fr7") = 0.0f;

    asm("fipr   fv4, fv4" : "+f" (s) : "f" (x), "f" (y), "f" (z));
    if (s == 0.0f)
        return shz_vec3_init(0.0f, 0.0f, 0.0f);
    s = shz_inv_sqrtf_fsrra(s);
    return shz_vec3_init(x * s, y * s, z * s);
}

SHZ_FORCE_INLINE float try_magnitude_inv(shz_vec3_t vec) {
    register float x asm("fr4") = vec.x;
    register float y asm("fr5") = vec.y;
    register float z asm("fr6") = vec.z;
    register float s asm("fr7") = 0.0f;

    asm("fipr   fv4, fv4" : "+f" (s) : "f" (x), "f" (y), "f" (z));
    return (s == 0.0f) ? 0.0f : shz_inv_sqrtf_fsrra(s);
}

SHZ_FORCE_INLINE float try_distance(shz_vec3_t a, shz_vec3_t b) {
    register float x asm("fr4") = a.x - b.x;
    register float y asm("fr5") = a.y - b.y;
    register float z asm("fr6") = a.z - b.z;
    register float s asm("fr7") = 0.0f;

    asm("fipr   fv4, fv4" : "+f" (s) : "f" (x), "f" (y), "f" (z));
    return (s == 0.0f) ? 0.0f : s * shz_inv_sqrtf_fsrra(s);
}

void bench_try_normalize(shz_vec3_t *R out, const shz_vec3_t *R in, const void *unused, size_t n) {
    size_t i;
    (void)unused;
    for (i = 0; i < n; i++)
        out[i] = try_normalize(in[i]);
}

void bench_try_normalize_safe(shz_vec3_t *R out, const shz_vec3_t *R in, const void *unused, size_t n) {
    size_t i;
    (void)unused;
    for (i = 0; i < n; i++)
        out[i] = try_normalize_safe(in[i]);
}

void bench_try_magnitude_inv(float *R out, const shz_vec3_t *R in, const void *unused, size_t n) {
    size_t i;
    (void)unused;
    for (i = 0; i < n; i++)
        out[i] = try_magnitude_inv(in[i]);
}

void bench_try_distance(float *R out, const shz_vec3_t *R a, const shz_vec3_t *R b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        out[i] = try_distance(a[i], b[i]);
}
