/* SH4ZAM-DCE tests: shz_dce_trig.h, the SH-4 side

   trig_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single; -O2 and -O3; the strict flags and the -ffast-math flags) and run by trig_check.c through the
   interpreter (tests/sh4run). The job runs the public reworks, the reworks with the barrier off and on, and SH4ZAM's routines on
   the same input into separate outputs; the host compares all of them with the true angles. This file is also the compile check of
   the header: it calls every public routine.
*/

#include <sh4zam_dce/shz_dce_trig.h>

#include "trig_jobs.h"

#define P(type, addr) ((type)(uintptr_t)(addr))

void t_trig(job_trig_t *j) {
    const float *in = P(const float *, j->in);
    float *as = P(float *, j->asin_dce), *ac = P(float *, j->acos_dce), *os = P(float *, j->asin_shz), *oc = P(float *, j->acos_shz);
    float *ps = P(float *, j->asin_plain), *pc = P(float *, j->acos_plain), *qs = P(float *, j->asin_opaque), *qc = P(float *, j->acos_opaque);
    for (uint32_t i = 0; i < j->count; i++) {
        as[i] = shz_dce_asinf(in[i]);
        ac[i] = shz_dce_acosf(in[i]);
    }
    for (uint32_t i = 0; i < j->count; i++) {
        ps[i] = shz_dce_asinf_o_(in[i], 0);
        pc[i] = shz_dce_acosf_o_(in[i], 0);
    }
    for (uint32_t i = 0; i < j->count; i++) {
        qs[i] = shz_dce_asinf_o_(in[i], 1);
        qc[i] = shz_dce_acosf_o_(in[i], 1);
    }
    for (uint32_t i = 0; i < j->count; i++) {
        os[i] = shz_asinf(in[i]);
        oc[i] = shz_acosf(in[i]);
    }
    j->built = (SHZ_DCE_OPAQUE_ASIN_ACOS ? TRIG_BUILT_OPAQUE : 0u)
#ifdef __FAST_MATH__
               | TRIG_BUILT_FAST
#endif
               ;
}
