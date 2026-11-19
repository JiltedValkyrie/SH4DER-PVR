/* SH4ZAM-DCE tests: the records vector_check.c hands to vector_kernels.c

   vector_jobs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   One record a routine: the host fills it in the interpreter's RAM and calls the SH-4 function with its address. Every field is 32 bits
   (an SH-4 address or a number), so the host and the SH-4 lay it out alike.
*/

#ifndef VECTOR_JOBS_H
#define VECTOR_JOBS_H

#include <stdint.h>

typedef struct { uint32_t a, b, out_asm, out_c, out_one, count; } job_dot_t;
typedef struct { uint32_t a, b, out_c, out_ref; } job_dot4_t;
typedef struct { uint32_t a, b, out_asm, out_ref, count; } job_cross_t;
typedef struct { uint32_t in, out_c, out_ref, count; } job_norm_t;
/* one 3-vector an element: the length by the one-call routine and by SH4ZAM's */
typedef struct { uint32_t a, count, o_mag, r_mag; } job_one_t;
/* one 4-vector an element: the normalisation by the one-call routine and by SH4ZAM's */
typedef struct { uint32_t in, o_v4, r_v4, count; } job_one4_t;

#endif /* VECTOR_JOBS_H */
