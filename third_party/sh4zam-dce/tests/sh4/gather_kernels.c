/* SH4ZAM-DCE tests: shz_dce_gather.h, the SH-4 side

   gather_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single; -O2 and -O3; the strict flags and the fast-math flags) and run by gather_check.c through
   the interpreter (tests/sh4run). Each function takes one record of gather_jobs.h. This file is also the compile check of the header: it
   instantiates every public routine, and the no-prefetch core the bench compares against.
*/

#include <sh4zam_dce/shz_dce_gather.h>

#include "gather_jobs.h"

#define P(type, addr) ((type)(uintptr_t)(addr))

void t_unpack3(job_unpack3_t *j) {
    const uint32_t *bits = P(const uint32_t *, j->bits);
    const float *ax = P(const float *, j->ax);
    const float *ay = P(const float *, j->ay);
    const float *az = P(const float *, j->az);
    const uint32_t *idx = P(const uint32_t *, j->idx);

    shz_dce_unpack3_gather(P(float (*)[3], j->out_pf), bits, j->bx, j->by, j->bz, ax, ay, az, idx, j->count);
    shz_dce_gather_unpack3_core(P(float (*)[3], j->out_nopf), bits, j->bx, j->by, j->bz, ax, ay, az, idx, j->count, 0);
}

void t_copy3(job_copy3_t *j) {
    shz_dce_copy3_gather(P(float (*)[3], j->out), P(const float *, j->table), P(const uint32_t *, j->idx), j->count);
}

void t_prefetch(job_prefetch_t *j) {
    shz_dce_prefetch_gather(P(const uint32_t *, j->bits), j->width, P(const uint32_t *, j->idx), j->count);
    shz_dce_prefetch_copy3_gather(P(const float *, j->table), P(const uint32_t *, j->idx), j->count);
}

/* the negative control of the top-of-RAM runs: one load at the address the host gives. The last word of the simulated RAM must read;
   the first byte past it must stop the run. */
void t_probe(job_probe_t *j) {
    volatile uint32_t *p = P(volatile uint32_t *, j->addr);
    (void)*p;
}
