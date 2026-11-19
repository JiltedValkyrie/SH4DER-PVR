/* SH4ZAM-DCE tests: shz_dce_planes.h, the SH-4 side

   planes_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single; -O2 and -O3; the strict flags and the -ffast-math flags) and run by planes_check.c through
   the interpreter (tests/sh4run). Each function takes one record of planes_jobs.h. Where a routine has an SH-4 loop and a plain C form,
   both run here on the same input: they use the same FTRV / FIPR, so their results must agree bit for bit.
   This file is also the compile check of the header: it instantiates every public routine, so every asm block is assembled.
*/

#include <sh4zam_dce/shz_dce_planes.h>

#include "planes_jobs.h"

#define P(type, addr) ((type)(uintptr_t)(addr))

void t_planes4(job_planes4_t *j) {
    shz_dce_planes4_load(P(const shz_dce_planes4_t *, j->planes));
    j->ret_asm = shz_dce_planes4_outcode_batch(P(uint8_t *, j->codes_asm), P(const float *, j->points), j->stride, j->count);
    j->ret_c = shz_dce_planes4_outcode_batch_c(P(uint8_t *, j->codes_c), P(const float *, j->points), j->stride, j->count);
}

void t_planes6(job_planes6_t *j) {
    const shz_dce_planes6_t *planes = P(const shz_dce_planes6_t *, j->planes);
    const float *spheres = P(const float *, j->spheres);
    uint32_t *single = P(uint32_t *, j->single);

    shz_dce_planes6_load(planes);
    shz_dce_planes6_cull_batch(P(uint8_t *, j->out_asm), planes, spheres, j->stride, j->count);
    shz_dce_planes6_cull_batch_c(P(uint8_t *, j->out_c), planes, spheres, j->stride, j->count);
    for (uint32_t i = 0; i < j->count; i++) {
        const float *s = (const float *)((const char *)spheres + i * j->stride);
        single[i] = shz_dce_planes6_sphere(planes, shz_vec3_init(s[0], s[1], s[2]), s[3]);
    }
}
