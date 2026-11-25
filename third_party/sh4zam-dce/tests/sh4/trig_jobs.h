/* SH4ZAM-DCE tests: the record trig_check.c hands to trig_kernels.c

   trig_jobs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Every field is 32 bits (an SH-4 address or a number), so the host and the SH-4 lay the record out alike.
*/

#ifndef TRIG_JOBS_H
#define TRIG_JOBS_H

#include <stdint.h>

#define TRIG_BUILT_FAST   0x01u   /* the kernels were built with -ffast-math */

/* in: count floats; the outputs: count floats each. dce: the wrappers; shz: SH4ZAM's routines. built: set by the job */
typedef struct { uint32_t in, count, asin_dce, acos_dce, asin_shz, acos_shz, built; } job_trig_t;

#endif /* TRIG_JOBS_H */
