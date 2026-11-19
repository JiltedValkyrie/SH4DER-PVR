/* SH4ZAM-DCE tests: the records planes_check.c hands to planes_kernels.c

   planes_jobs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   One record a routine: the host fills it in the interpreter's RAM and calls the SH-4 function with its address. Every field is 32 bits
   (an SH-4 address or a number), so the host and the SH-4 lay it out alike.
*/

#ifndef PLANES_JOBS_H
#define PLANES_JOBS_H

#include <stdint.h>

typedef struct { uint32_t planes, points, stride, count, codes_asm, codes_c, ret_asm, ret_c; } job_planes4_t;
typedef struct { uint32_t planes, spheres, stride, count, out_asm, out_c, single; } job_planes6_t;

#endif /* PLANES_JOBS_H */
