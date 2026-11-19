/* SH4ZAM-DCE tests: shz_dce_project.h, the SH-4 side

   project_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single; -O2 and -O3; the strict flags and the -ffast-math flags) and run by project_check.c through
   the interpreter (tests/sh4run). The routine, its plain C form and, in the builds that contract a product and a sum into one fused
   step, the loop written the plain way run on the same input: same FTRV, same FSRRA, same compares, same fused steps, so the same
   bytes.
*/

#include <sh4zam_dce/shz_dce_project.h>

#define PROJECT_PLAIN_LOOP
#include "project_jobs.h"

#define P(type, addr) ((type)(uintptr_t)(addr))

__attribute__((noinline)) static size_t plain_loop(float *out, uint8_t *codes, uint32_t *words, const void *in, size_t stride, size_t count,
                                                   const shz_dce_view4_t *view, int clip, int lazy) {
    return project_plain_loop(out, codes, words, in, stride, count, view, clip, lazy);
}

void t_project(job_project_t *j) {
    const shz_dce_view4_t *view = P(const shz_dce_view4_t *, j->view);

    shz_xmtrx_load_4x4(P(const shz_mat4x4_t *, j->matrix));
    j->ret_asm = (uint32_t)shz_dce_project_vertices(P(float *, j->out_asm), P(uint8_t *, j->codes_asm), P(uint32_t *, j->words_asm), P(const void *, j->in),
                                                    j->stride, j->count, view, (int)j->clip, (int)j->lazy);
    j->ret_c = (uint32_t)shz_dce_project_vertices_c(P(float *, j->out_c), P(uint8_t *, j->codes_c), P(uint32_t *, j->words_c), P(const void *, j->in),
                                                    j->stride, j->count, view, (int)j->clip, (int)j->lazy);
#if defined(__FAST_MATH__)
    j->plain = 1u;
    j->ret_p = (uint32_t)plain_loop(P(float *, j->out_p), P(uint8_t *, j->codes_p), P(uint32_t *, j->words_p), P(const void *, j->in), j->stride, j->count, view,
                                    (int)j->clip, (int)j->lazy);
#else
    j->plain = 0u;
    j->ret_p = 0u;
    (void)plain_loop;
#endif
}
