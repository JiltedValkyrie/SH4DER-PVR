/* SH4ZAM-DCE tests: the record upstream_check.c hands to upstream_kernels.c

   upstream_jobs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Every field is 32 bits (an SH-4 address or a number), so the host and the SH-4 lay the record out alike.
*/

#ifndef UPSTREAM_JOBS_H
#define UPSTREAM_JOBS_H

#include <stdint.h>

/* in: count floats; the four outputs: count floats each (the proposal's routines, SH4ZAM's routines); fast: set by the job, 1
   when the kernels were built with -ffast-math */
typedef struct { uint32_t in, count, asin_new, acos_new, asin_shz, acos_shz, fast; } job_upstream_t;

#endif /* UPSTREAM_JOBS_H */
