/* SH4ZAM-DCE tests: shz_dce_sqrt.h, the SH-4 side

   sqrt_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single; -O2 and -O3; the strict flags and the -ffast-math flags) and run by sqrt_check.c through the
   interpreter (tests/sh4run). Each job runs SH4ZAM's routines and the wrappers on the same input into separate outputs, each in its
   own function that is never inlined, so the compiler shares nothing between them. This file is also the compile check of the
   header: it calls every public routine.
*/

#include <sh4zam_dce/shz_dce_sqrt.h>

#include "sqrt_jobs.h"

#define P(type, addr) ((type)(uintptr_t)(addr))
#define NOINLINE __attribute__((noinline))

static NOINLINE void scalar_shz(float *SHZ_RESTRICT out, const float *SHZ_RESTRICT a, const float *SHZ_RESTRICT b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        out[3 * i] = shz_sqrtf(a[4 * i]);
        out[3 * i + 1] = shz_inv_sqrtf(a[4 * i]);
        out[3 * i + 2] = shz_hypotf(a[4 * i], b[4 * i]);
    }
}

static NOINLINE void scalar_dce(float *SHZ_RESTRICT out, const float *SHZ_RESTRICT a, const float *SHZ_RESTRICT b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        out[3 * i] = shz_dce_sqrtf(a[4 * i]);
        out[3 * i + 1] = shz_dce_inv_sqrtf(a[4 * i]);
        out[3 * i + 2] = shz_dce_hypotf(a[4 * i], b[4 * i]);
    }
}

static NOINLINE void vector_shz(float *SHZ_RESTRICT out, const float *SHZ_RESTRICT a, const float *SHZ_RESTRICT b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++, a += 4, b += 4, out += SQRT_VECTOR_OUT) {
        const shz_vec4_t u = shz_vec4_init(a[0], a[1], a[2], a[3]), v = shz_vec4_init(b[0], b[1], b[2], b[3]);
        const shz_vec2_t n2 = shz_vec2_normalize_safe(u.xy);
        const shz_vec3_t n3 = shz_vec3_normalize_safe(u.xyz);
        const shz_vec4_t n4 = shz_vec4_normalize_safe(u);
        out[0] = shz_vec2_magnitude(u.xy); out[1] = shz_vec3_magnitude(u.xyz); out[2] = shz_vec4_magnitude(u);
        out[3] = shz_vec2_magnitude_inv(u.xy); out[4] = shz_vec3_magnitude_inv(u.xyz); out[5] = shz_vec4_magnitude_inv(u);
        out[6] = shz_vec2_distance(u.xy, v.xy); out[7] = shz_vec3_distance(u.xyz, v.xyz); out[8] = shz_vec4_distance(u, v);
        out[9] = n2.x; out[10] = n2.y;
        out[11] = n3.x; out[12] = n3.y; out[13] = n3.z;
        out[14] = n4.x; out[15] = n4.y; out[16] = n4.z; out[17] = n4.w;
    }
}

static NOINLINE void vector_dce(float *SHZ_RESTRICT out, const float *SHZ_RESTRICT a, const float *SHZ_RESTRICT b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++, a += 4, b += 4, out += SQRT_VECTOR_OUT) {
        const shz_vec4_t u = shz_vec4_init(a[0], a[1], a[2], a[3]), v = shz_vec4_init(b[0], b[1], b[2], b[3]);
        const shz_vec2_t n2 = shz_dce_vec2_normalize_safe(u.xy);
        const shz_vec3_t n3 = shz_dce_vec3_normalize_safe(u.xyz);
        const shz_vec4_t n4 = shz_dce_vec4_normalize_safe(u);
        out[0] = shz_dce_vec2_magnitude(u.xy); out[1] = shz_dce_vec3_magnitude(u.xyz); out[2] = shz_dce_vec4_magnitude(u);
        out[3] = shz_dce_vec2_magnitude_inv(u.xy); out[4] = shz_dce_vec3_magnitude_inv(u.xyz); out[5] = shz_dce_vec4_magnitude_inv(u);
        out[6] = shz_dce_vec2_distance(u.xy, v.xy); out[7] = shz_dce_vec3_distance(u.xyz, v.xyz); out[8] = shz_dce_vec4_distance(u, v);
        out[9] = n2.x; out[10] = n2.y;
        out[11] = n3.x; out[12] = n3.y; out[13] = n3.z;
        out[14] = n4.x; out[15] = n4.y; out[16] = n4.z; out[17] = n4.w;
    }
}

/* count = how this file was built */
void t_sqrt_flags(job_sqrt_t *j) {
    uint32_t f = 0;
#ifdef __FAST_MATH__
    f |= SQRT_BUILT_FAST;
#endif
    j->count = f;
}

void t_sqrt_scalar(job_sqrt_t *j) {
    scalar_shz(P(float *, j->out_shz), P(const float *, j->in), P(const float *, j->in2), j->count);
    scalar_dce(P(float *, j->out_dce), P(const float *, j->in), P(const float *, j->in2), j->count);
}

void t_sqrt_vector(job_sqrt_t *j) {
    vector_shz(P(float *, j->out_shz), P(const float *, j->in), P(const float *, j->in2), j->count);
    vector_dce(P(float *, j->out_dce), P(const float *, j->in), P(const float *, j->in2), j->count);
}
