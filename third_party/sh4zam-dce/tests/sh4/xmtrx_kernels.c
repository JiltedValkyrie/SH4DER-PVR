/* SH4ZAM-DCE tests: shz_dce_xmtrx.h, the SH-4 side

   xmtrx_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single; -O2 and -O3; the strict flags and the -ffast-math flags) and run by xmtrx_check.c through
   the interpreter (tests/sh4run). Each routine and its plain C form run on the same input: the same FTRV, the same FSRRA, the
   same products, so the same bytes.
*/

#include <sh4zam_dce/shz_dce_xmtrx.h>

#include "xmtrx_jobs.h"

#define P(type, addr) ((type)(uintptr_t)(addr))

void t_xmtrx(job_xmtrx_t *j) {
    void *oa = P(void *, j->out_a), *oc = P(void *, j->out_c);
    const void *ia = P(const void *, j->in_a), *ic = P(const void *, j->in_c);

    shz_xmtrx_load_4x4(P(const shz_mat4x4_t *, j->matrix));
    switch (j->kind) {
    case XMTRX_VEC4:
        shz_dce_xmtrx_transform_vec4_batch(oa, j->out_stride, ia, j->in_stride, j->count);
        shz_dce_xmtrx_transform_vec4_batch_c(oc, j->out_stride, ic, j->in_stride, j->count);
        break;
    case XMTRX_POINT3:
        shz_dce_xmtrx_transform_point3_batch(oa, j->out_stride, ia, j->in_stride, j->count);
        shz_dce_xmtrx_transform_point3_batch_c(oc, j->out_stride, ic, j->in_stride, j->count);
        break;
    case XMTRX_VEC3:
        shz_dce_xmtrx_transform_vec3_batch(oa, j->out_stride, ia, j->in_stride, j->count);
        shz_dce_xmtrx_transform_vec3_batch_c(oc, j->out_stride, ic, j->in_stride, j->count);
        break;
    default:
        shz_dce_xmtrx_project_point3_batch(oa, j->out_stride, ia, j->in_stride, j->count);
        shz_dce_xmtrx_project_point3_batch_c(oc, j->out_stride, ic, j->in_stride, j->count);
        break;
    }
}
