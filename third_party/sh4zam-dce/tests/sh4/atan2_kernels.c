/* SH4ZAM-DCE tests: shz_dce_atan2.h, the SH-4 side

   atan2_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single; -O2 and -O3; the strict flags and the -ffast-math flags) and run by atan2_check.c through
   the interpreter (tests/sh4run). The job runs shz_dce_atan2f and SH4ZAM's shz_atan2f on the same input into separate outputs,
   each in its own function that is never inlined, as two callers would have them. This file is also the compile check of the
   header: it calls every public routine.
*/

#include <sh4zam_dce/shz_dce_atan2.h>

#include "atan2_jobs.h"

#define P(type, addr) ((type)(uintptr_t)(addr))
#define NOINLINE __attribute__((noinline))

static NOINLINE void run_dce(float *SHZ_RESTRICT o, const float *SHZ_RESTRICT y, const float *SHZ_RESTRICT x, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) o[i] = shz_dce_atan2f(y[i], x[i]);
}

static NOINLINE void run_shz(float *SHZ_RESTRICT o, const float *SHZ_RESTRICT y, const float *SHZ_RESTRICT x, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) o[i] = shz_atan2f(y[i], x[i]);
}

void t_atan2(job_atan2_t *j) {
    run_dce(P(float *, j->out_dce), P(const float *, j->y), P(const float *, j->x), j->count);
    run_shz(P(float *, j->out_shz), P(const float *, j->y), P(const float *, j->x), j->count);
    j->terms = SHZ_ATAN2F_TERMS;
#ifdef __FAST_MATH__
    j->fast = 1u;
#else
    j->fast = 0u;
#endif
}
