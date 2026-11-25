/* SH4ZAM-DCE tests: the record atan2_check.c hands to atan2_kernels.c

   atan2_jobs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

#ifndef ATAN2_JOBS_H
#define ATAN2_JOBS_H

#include <stdint.h>

/* y, x: count floats each. The outputs, count floats each: out_dce shz_dce_atan2f, out_shz SH4ZAM's shz_atan2f, each from its own
   function that is never inlined. Set by the job: terms (SHZ_ATAN2F_TERMS) and fast (1: built with -ffast-math) */
typedef struct { uint32_t y, x, count, out_dce, out_shz, terms, fast; } job_atan2_t;

#endif /* ATAN2_JOBS_H */
