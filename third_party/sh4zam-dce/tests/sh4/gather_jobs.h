/* SH4ZAM-DCE tests: the records gather_check.c hands to gather_kernels.c

   gather_jobs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   One record a routine: the host fills it in the interpreter's RAM and calls the SH-4 function with its address. Every field is 32 bits
   (an SH-4 address or a number), so the host and the SH-4 lay it out alike.
*/

#ifndef GATHER_JOBS_H
#define GATHER_JOBS_H

#include <stdint.h>

/* out_pf: the public routine (with its prefetch passes); out_nopf: the same code without them, for the bench's comparison */
typedef struct { uint32_t out_pf, out_nopf, bits, bx, by, bz, ax, ay, az, idx, count; } job_unpack3_t;
typedef struct { uint32_t out, table, idx, count; } job_copy3_t;
typedef struct { uint32_t bits, width, idx, count, table; } job_prefetch_t;
typedef struct { uint32_t addr; } job_probe_t;   /* the negative control of the top-of-RAM runs: one load at addr */

#endif /* GATHER_JOBS_H */
