/* SH4ZAM-DCE tests: the records quat_check.c hands to quat_kernels.c

   quat_jobs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   One record a routine: the host fills it in the interpreter's RAM and calls the SH-4 function with its address. Every field is 32 bits
   (an SH-4 address or a number; a float is passed as its bits), so the host and the SH-4 lay it out alike.
*/

#ifndef QUAT_JOBS_H
#define QUAT_JOBS_H

#include <stdint.h>

typedef struct { uint32_t a, b, out_asm, out_c, out_orig, count, t; } job_nlerp_t;
typedef struct { uint32_t a, b, out_asm, out_c, out_orig, count; } job_mult_t;
typedef struct { uint32_t in, out_asm, out_c, out_orig, count; } job_norm_t;

#endif /* QUAT_JOBS_H */
