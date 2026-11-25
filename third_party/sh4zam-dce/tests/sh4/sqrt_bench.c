/* SH4ZAM-DCE tests: shz_dce_sqrt.h, the loops its cycle ESTIMATES are made from

   sqrt_bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc with the fast-math flags and run by tests/sh4run/sh4bench: every bench_ function is
   `void f(void *out, const void *in0, const void *in1, size_t count)` on buffers of random floats in [-1, 1).
   For each routine: SH4ZAM's (the _shz rows, before) and the rework (after). The figures are the interpreter's pipeline model
   (an ESTIMATE, cache hits assumed), not a console's.
*/

/* the wrappers are SH4ZAM's routines (master ff39547): each pair of rows must cost the same */
#include <sh4zam_dce/shz_dce_sqrt.h>

#define R SHZ_RESTRICT
#define F1(name, expr) void name(float *R out, const float *R a, const float *R b, size_t n) { (void)b; for (size_t i = 0; i < n; i++) { const float x = a[i]; out[i] = (expr); } }
#define F2(name, expr) void name(float *R out, const float *R a, const float *R b, size_t n) { for (size_t i = 0; i < n; i++) { const float x = a[i], y = b[i]; out[i] = (expr); } }
#define V3F(name, expr) void name(float *R out, const shz_vec3_t *R a, const shz_vec3_t *R b, size_t n) { (void)b; for (size_t i = 0; i < n; i++) { const shz_vec3_t u = a[i]; out[i] = (expr); } }
#define V3V3F(name, expr) void name(float *R out, const shz_vec3_t *R a, const shz_vec3_t *R b, size_t n) { for (size_t i = 0; i < n; i++) { const shz_vec3_t u = a[i], v = b[i]; out[i] = (expr); } }
#define V3V(name, expr) void name(shz_vec3_t *R out, const shz_vec3_t *R a, const shz_vec3_t *R b, size_t n) { (void)b; for (size_t i = 0; i < n; i++) { const shz_vec3_t u = a[i]; out[i] = (expr); } }
#define V4F(name, expr) void name(float *R out, const shz_vec4_t *R a, const shz_vec4_t *R b, size_t n) { (void)b; for (size_t i = 0; i < n; i++) { const shz_vec4_t u = a[i]; out[i] = (expr); } }

F1(bench_sqrtf_shz, shz_sqrtf(x))
F1(bench_sqrtf, shz_dce_sqrtf(x))
F1(bench_inv_sqrtf_shz, shz_inv_sqrtf(x))
F1(bench_inv_sqrtf, shz_dce_inv_sqrtf(x))
F2(bench_hypotf_shz, shz_hypotf(x, y))
F2(bench_hypotf, shz_dce_hypotf(x, y))
V3F(bench_vec3_magnitude_shz, shz_vec3_magnitude(u))
V3F(bench_vec3_magnitude, shz_dce_vec3_magnitude(u))
V3F(bench_vec3_magnitude_inv_shz, shz_vec3_magnitude_inv(u))
V3F(bench_vec3_magnitude_inv, shz_dce_vec3_magnitude_inv(u))
V3V3F(bench_vec3_distance_shz, shz_vec3_distance(u, v))
V3V3F(bench_vec3_distance, shz_dce_vec3_distance(u, v))
V3V(bench_vec3_normalize_safe_shz, shz_vec3_normalize_safe(u))
V3V(bench_vec3_normalize_safe, shz_dce_vec3_normalize_safe(u))
V4F(bench_vec4_magnitude_shz, shz_vec4_magnitude(u))
V4F(bench_vec4_magnitude, shz_dce_vec4_magnitude(u))

/* the root of a sum the compiler can prove positive: it removes the zero test from both forms, the code is the same */
F2(bench_sqrtf_nonzero_shz, shz_sqrtf(x * x + 0.25f) + y)
F2(bench_sqrtf_nonzero, shz_dce_sqrtf(x * x + 0.25f) + y)

/* four independent roots an element (the shape of one console measurement) */
F2(bench_sqrtf4_shz, shz_sqrtf(x) + shz_sqrtf(y) + shz_sqrtf(x + 2.0f) + shz_sqrtf(y + 2.0f))
F2(bench_sqrtf4, shz_dce_sqrtf(x) + shz_dce_sqrtf(y) + shz_dce_sqrtf(x + 2.0f) + shz_dce_sqrtf(y + 2.0f))
