/* SH4ZAM-DCE tests: shz_dce_atan2.h, the SH-4 side

   atan2_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single; -O2 and -O3; the strict flags and the -ffast-math flags) and run by atan2_check.c through
   the interpreter (tests/sh4run). The job runs the rework with each number of terms, the public routine and SH4ZAM's routine on
   the same input into separate outputs; the host compares all of them with the true angles. This file is also the compile check
   of the header: it calls every public routine.
*/

#include <sh4zam_dce/shz_dce_atan2.h>

#include "atan2_jobs.h"

#define P(type, addr) ((type)(uintptr_t)(addr))
#define LOOP(out, expr) do { float *o_ = P(float *, out); for (uint32_t i = 0; i < j->count; i++) o_[i] = (expr); } while (0)

void t_atan2(job_atan2_t *j) {
    const float *y = P(const float *, j->y), *x = P(const float *, j->x);
    LOOP(j->out_n[0], shz_dce_atan2f_n_(y[i], x[i], 2));
    LOOP(j->out_n[1], shz_dce_atan2f_n_(y[i], x[i], 5));
    LOOP(j->out_n[2], shz_dce_atan2f_n_(y[i], x[i], 6));
    LOOP(j->out_n[3], shz_dce_atan2f_n_(y[i], x[i], 7));
    LOOP(j->out_dce, shz_dce_atan2f(y[i], x[i]));
    LOOP(j->out_shz, shz_atan2f(y[i], x[i]));
    j->terms = SHZ_DCE_ATAN2_TERMS;
#ifdef __FAST_MATH__
    j->fast = 1u;
#else
    j->fast = 0u;
#endif
}
