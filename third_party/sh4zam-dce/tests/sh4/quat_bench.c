/* SH4ZAM-DCE tests: shz_dce_quat.h, the loops its cycle ESTIMATES are made from

   quat_bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc with the fast-math flags and run by tests/sh4run/sh4bench: every bench_ function is
   `void f(void *out, const void *in0, const void *in1, size_t count [, float t])` on buffers of random floats in [-1, 1).
   For each routine: the routine, its plain C form, and the loop it replaces (SH4ZAM's own routine called once per element, the
   way a caller writes it), so the table reads before / after. The figures are the interpreter's pipeline model (an ESTIMATE,
   cache hits assumed), not a console's.
*/

#include <sh4zam_dce/shz_dce_quat.h>

#define R SHZ_RESTRICT

/* ---- normalised lerp (one element: a quaternion pair and a weight) ---- */

void bench_nlerp_batch(shz_quat_t *R out, const shz_quat_t *R a, const shz_quat_t *R b, size_t n, float t) {
    shz_dce_quat_nlerp_batch(out, a, b, t, n);
}

void bench_nlerp_batch_c(shz_quat_t *R out, const shz_quat_t *R a, const shz_quat_t *R b, size_t n, float t) {
    shz_dce_quat_nlerp_batch_c(out, a, b, t, n);
}

void bench_nlerp_shz(shz_quat_t *R out, const shz_quat_t *R a, const shz_quat_t *R b, size_t n, float t) {
    size_t i;
    for (i = 0; i < n; i++)
        out[i] = shz_quat_nlerp(a[i], b[i], t);
}

/* ---- Hamilton product ---- */

void bench_mult_batch(shz_quat_t *R out, const shz_quat_t *R a, const shz_quat_t *R b, size_t n, float t) {
    (void)t;
    shz_dce_quat_mult_batch(out, a, b, n);
}

void bench_mult_batch_c(shz_quat_t *R out, const shz_quat_t *R a, const shz_quat_t *R b, size_t n, float t) {
    (void)t;
    shz_dce_quat_mult_batch_c(out, a, b, n);
}

void bench_mult_shz(shz_quat_t *R out, const shz_quat_t *R a, const shz_quat_t *R b, size_t n, float t) {
    size_t i;
    (void)t;
    for (i = 0; i < n; i++)
        out[i] = shz_quat_mult(a[i], b[i]);
}

/* ---- normalise (the second input is unused) ---- */

void bench_normalize_batch(shz_quat_t *R out, const shz_quat_t *R in, const void *unused, size_t n, float t) {
    (void)unused;
    (void)t;
    shz_dce_quat_normalize_batch(out, in, n);
}

void bench_normalize_batch_c(shz_quat_t *R out, const shz_quat_t *R in, const void *unused, size_t n, float t) {
    (void)unused;
    (void)t;
    shz_dce_quat_normalize_batch_c(out, in, n);
}

void bench_normalize_shz(shz_quat_t *R out, const shz_quat_t *R in, const void *unused, size_t n, float t) {
    size_t i;
    (void)unused;
    (void)t;
    for (i = 0; i < n; i++)
        out[i] = shz_quat_normalize(in[i]);
}

/* ---- the batch of one, called once per element with the one-quaternion signature (by value): what a drop-in would cost.
        The header quotes these rows for the routines it did not keep. ---- */

static shz_quat_t nlerp_one(shz_quat_t a, shz_quat_t b, float t) {
    shz_quat_t o;
    shz_dce_quat_nlerp_batch(&o, &a, &b, t, 1);
    return o;
}

static shz_quat_t mult_one(shz_quat_t a, shz_quat_t b) {
    shz_quat_t o;
    shz_dce_quat_mult_batch(&o, &a, &b, 1);
    return o;
}

static shz_quat_t normalize_one(shz_quat_t a) {
    shz_quat_t o;
    shz_dce_quat_normalize_batch(&o, &a, 1);
    return o;
}

void bench_nlerp_one(shz_quat_t *R out, const shz_quat_t *R a, const shz_quat_t *R b, size_t n, float t) {
    size_t i;
    for (i = 0; i < n; i++)
        out[i] = nlerp_one(a[i], b[i], t);
}

void bench_mult_one(shz_quat_t *R out, const shz_quat_t *R a, const shz_quat_t *R b, size_t n, float t) {
    size_t i;
    (void)t;
    for (i = 0; i < n; i++)
        out[i] = mult_one(a[i], b[i]);
}

void bench_normalize_one(shz_quat_t *R out, const shz_quat_t *R in, const void *unused, size_t n, float t) {
    size_t i;
    (void)unused;
    (void)t;
    for (i = 0; i < n; i++)
        out[i] = normalize_one(in[i]);
}
