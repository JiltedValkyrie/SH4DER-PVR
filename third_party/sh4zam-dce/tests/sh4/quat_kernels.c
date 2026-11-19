/* SH4ZAM-DCE tests: shz_dce_quat.h, the SH-4 side

   quat_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single; -O2 and -O3; the strict flags and the -ffast-math flags) and run by quat_check.c through
   the interpreter (tests/sh4run). Each function takes one record of quat_jobs.h. Each job runs the routine (the dispatch, which on the
   SH-4 is the asm loop), its plain C form, and the SH4ZAM routine it reworks, on the same input, into three outputs. The check then
   compares them. This file is also the compile check of the header: it calls every public routine, so every asm block is assembled.
*/

#include <sh4zam_dce/shz_dce_quat.h>

#include "quat_jobs.h"

#define P(type, addr) ((type)(uintptr_t)(addr))

static float bits_to_float(uint32_t b) {
    union { uint32_t u; float f; } c;
    c.u = b;
    return c.f;
}

void t_nlerp(job_nlerp_t *j) {
    const shz_quat_t *a = P(const shz_quat_t *, j->a);
    const shz_quat_t *b = P(const shz_quat_t *, j->b);
    const float t = bits_to_float(j->t);
    const size_t n = j->count;
    shz_quat_t *oo = P(shz_quat_t *, j->out_orig);
    size_t i;

    shz_dce_quat_nlerp_batch(P(shz_quat_t *, j->out_asm), a, b, t, n);
    shz_dce_quat_nlerp_batch_c(P(shz_quat_t *, j->out_c), a, b, t, n);
    for (i = 0; i < n; i++)
        oo[i] = shz_quat_nlerp(a[i], b[i], t);
}

void t_mult(job_mult_t *j) {
    const shz_quat_t *a = P(const shz_quat_t *, j->a);
    const shz_quat_t *b = P(const shz_quat_t *, j->b);
    const size_t n = j->count;
    shz_quat_t *oo = P(shz_quat_t *, j->out_orig);
    size_t i;

    shz_dce_quat_mult_batch(P(shz_quat_t *, j->out_asm), a, b, n);
    shz_dce_quat_mult_batch_c(P(shz_quat_t *, j->out_c), a, b, n);
    for (i = 0; i < n; i++)
        oo[i] = shz_quat_mult(a[i], b[i]);
}

void t_norm(job_norm_t *j) {
    const shz_quat_t *in = P(const shz_quat_t *, j->in);
    const size_t n = j->count;
    shz_quat_t *oo = P(shz_quat_t *, j->out_orig);
    size_t i;

    shz_dce_quat_normalize_batch(P(shz_quat_t *, j->out_asm), in, n);
    shz_dce_quat_normalize_batch_c(P(shz_quat_t *, j->out_c), in, n);
    for (i = 0; i < n; i++)
        oo[i] = shz_quat_normalize(in[i]);
}
