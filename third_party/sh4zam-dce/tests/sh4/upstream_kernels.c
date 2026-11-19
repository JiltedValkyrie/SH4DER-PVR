/* SH4ZAM-DCE tests: doc/upstream/shz_asin_acos.h, the SH-4 side

   upstream_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single; -O2 and -O3; the strict flags and the -ffast-math flags) and run by upstream_check.c through
   the interpreter (tests/sh4run). The file under doc/upstream is offered to SH4ZAM as it is, so it is compiled as it is: this file
   includes it and SH4ZAM, nothing of SH4ZAM-DCE.
*/

#include "../../doc/upstream/shz_asin_acos.h"

#include "upstream_jobs.h"

#define P(type, addr) ((type)(uintptr_t)(addr))

void t_upstream(job_upstream_t *j) {
    const float *in = P(const float *, j->in);
    float *as = P(float *, j->asin_new), *ac = P(float *, j->acos_new), *os = P(float *, j->asin_shz), *oc = P(float *, j->acos_shz);
    for (uint32_t i = 0; i < j->count; i++) {
        as[i] = shz_asinf_quartic(in[i]);
        ac[i] = shz_acosf_quartic(in[i]);
    }
    for (uint32_t i = 0; i < j->count; i++) {
        os[i] = shz_asinf(in[i]);
        oc[i] = shz_acosf(in[i]);
    }
#ifdef __FAST_MATH__
    j->fast = 1u;
#else
    j->fast = 0u;
#endif
}
