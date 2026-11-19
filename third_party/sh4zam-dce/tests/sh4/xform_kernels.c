/* SH4ZAM-DCE tests: shz_dce_xform.h, the SH-4 side

   xform_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single; -O2 and -O3; the strict flags and the -ffast-math flags) and run by xform_check.c through the
   interpreter (tests/sh4run). The routine and its plain C form run on the same input: same FTRV, same compares, so the same bytes.
*/

#include <sh4zam_dce/shz_dce_xform.h>

#include "xform_jobs.h"

#define P(type, addr) ((type)(uintptr_t)(addr))

void t_xform(job_xform_t *j) {
    const shz_dce_clip_t *clip = P(const shz_dce_clip_t *, j->clip);

    shz_xmtrx_load_4x4(P(const shz_mat4x4_t *, j->matrix));
    j->ret_asm = shz_dce_xform_vertex32(P(float *, j->out_asm), P(uint8_t *, j->codes_asm), P(const float *, j->in), j->count, (int)j->mode, clip);
    j->ret_c = shz_dce_xform_vertex32_c(P(float *, j->out_c), P(uint8_t *, j->codes_c), P(const float *, j->in), j->count, (int)j->mode, clip);
}
