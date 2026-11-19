/* SH4ZAM-DCE tests: shz_dce_vector.h, the SH-4 side

   vector_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single; -O2 and -O3; the strict flags and the -ffast-math flags) and run by vector_check.c through
   the interpreter (tests/sh4run). Each function takes one record of vector_jobs.h. Where a routine has an SH-4 loop and a plain C form,
   both run here on the same input, and the SH4ZAM routine the header replaces runs on the same input as the reference. This file is also
   the compile check of the header: it instantiates every public routine, so every asm block is assembled.
*/

#include <sh4zam_dce/shz_dce_vector.h>

#include "vector_jobs.h"

#define P(type, addr) ((type)(uintptr_t)(addr))

void t_dot_batch(job_dot_t *j) {
    const shz_vec3_t *a = P(const shz_vec3_t *, j->a);
    const shz_vec3_t *b = P(const shz_vec3_t *, j->b);
    float *one = P(float *, j->out_one);
    uint32_t i;

    shz_dce_vec3_dot_batch(P(float *, j->out_asm), a, b, j->count);
    shz_dce_vec3_dot_batch_c(P(float *, j->out_c), a, b, j->count);
    for (i = 0; i < j->count; i++)   /* one element at a time, by the single-element FIPR the batch's odd tail uses */
        one[i] = shz_dce_vec3_dot_fipr_one_sh4(a + i, b + i);
}

void t_dot4(job_dot4_t *j) {
    const shz_vec3_t *a = P(const shz_vec3_t *, j->a);
    const shz_vec3_t *b = P(const shz_vec3_t *, j->b);
    float *ref = P(float *, j->out_ref);
    shz_vec4_t r = shz_dce_vec3_dot4(a, b);
    int k;

    *P(shz_vec4_t *, j->out_c) = r;
    for (k = 0; k < 4; k++) ref[k] = shz_vec3_dot(a[k], b[k]);
}

void t_cross_batch(job_cross_t *j) {
    const shz_vec3_t *a = P(const shz_vec3_t *, j->a);
    const shz_vec3_t *b = P(const shz_vec3_t *, j->b);
    shz_vec3_t *ref = P(shz_vec3_t *, j->out_ref);
    uint32_t i;

    for (i = 0; i < j->count; i++)   /* before the batch: its out may be a (in place) */
        ref[i] = shz_vec3_cross(a[i], b[i]);
    shz_dce_vec3_cross_batch(P(shz_vec3_t *, j->out_asm), a, b, j->count);
}

void t_normalize_batch(job_norm_t *j) {
    const shz_vec3_t *in = P(const shz_vec3_t *, j->in);
    shz_vec3_t *ref = P(shz_vec3_t *, j->out_ref);
    uint32_t i;

    for (i = 0; i < j->count; i++)   /* before the batch: its out may be in (in place) */
        ref[i] = shz_vec3_normalize(in[i]);
    shz_dce_vec3_normalize_batch(P(shz_vec3_t *, j->out_c), in, j->count);
}

void t_one(job_one_t *j) {
    const shz_vec3_t *a = P(const shz_vec3_t *, j->a);
    float *om = P(float *, j->o_mag), *rm = P(float *, j->r_mag);
    uint32_t i;

    for (i = 0; i < j->count; i++) {
        om[i] = shz_dce_vec3_magnitude_fipr(a[i]);
        rm[i] = shz_vec3_magnitude(a[i]);
    }
}

void t_one4(job_one4_t *j) {
    const shz_vec4_t *in = P(const shz_vec4_t *, j->in);
    shz_vec4_t *o = P(shz_vec4_t *, j->o_v4), *r = P(shz_vec4_t *, j->r_v4);
    uint32_t i;

    for (i = 0; i < j->count; i++) {
        o[i] = shz_dce_vec4_normalize(in[i]);
        r[i] = shz_vec4_normalize(in[i]);
    }
}
