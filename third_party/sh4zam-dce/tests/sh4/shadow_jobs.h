/* SH4ZAM-DCE tests: the records shadow_check.c hands to shadow_kernels.c

   shadow_jobs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   One record a routine: the host fills it in the interpreter's RAM and calls the SH-4 function with its address. Every field is 32 bits
   (an SH-4 address or a number), so the host and the SH-4 lay it out alike. The map itself is a shz_dce_shadow_map_t at the address
   `map` (its layout is the SH-4's: a pointer, an unsigned, floats, an int: 40 bytes, 4-byte aligned).
*/

#ifndef SHADOW_JOBS_H
#define SHADOW_JOBS_H

#include <stdint.h>

typedef struct {
    uint32_t map, matrix, t1, t2;               /* the map record, the map's 16 floats, the two axes (float[3] each) */
    uint32_t points, stride, count, out_stride; /* the batch */
    uint32_t out_asm, out_sh4, out_c, out_before;   /* the dispatch, the _sh4 form, the _c form, the planar path the kernel replaces */
} job_shadow_t;

#endif /* SHADOW_JOBS_H */
