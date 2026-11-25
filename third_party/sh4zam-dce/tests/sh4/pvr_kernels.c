/* SH4ZAM-DCE tests: shz_dce_pvr.h, the SH-4 side

   pvr_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single; -O2 and -O3; the strict flags and the -ffast-math flags) and run by pvr_check.c through the
   interpreter (tests/sh4run). The routine and its plain C form run on the same input: same FTRV, same FSRRA, same products, so the
   same bytes.
*/

#include <sh4zam_dce/shz_dce_pvr.h>

#include "pvr_jobs.h"

#define P(type, addr) ((type)(uintptr_t)(addr))

void t_pvr(job_pvr_t *j) {
    shz_xmtrx_load_4x4(P(const shz_mat4x4_t *, j->matrix));
    shz_dce_pvr_project(P(void *, j->out_asm), P(const void *, j->in), j->count);
    shz_dce_pvr_project_c(P(void *, j->out_c), P(const void *, j->in), j->count);
}
