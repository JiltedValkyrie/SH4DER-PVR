/* SH4ZAM-DCE tests: the record atan2_check.c hands to atan2_kernels.c

   atan2_jobs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Every field is 32 bits (an SH-4 address or a number), so the host and the SH-4 lay the record out alike.
*/

#ifndef ATAN2_JOBS_H
#define ATAN2_JOBS_H

#include <stdint.h>

/* y, x: count floats each. The outputs, count floats each: out_n[0..3] the rework with 2, 5, 6 and 7 terms, out_dce the public
   routine, out_shz SH4ZAM's. Set by the job: terms (SHZ_DCE_ATAN2_TERMS) and fast (1: built with -ffast-math) */
typedef struct { uint32_t y, x, count, out_n[4], out_dce, out_shz, terms, fast; } job_atan2_t;

#endif /* ATAN2_JOBS_H */
