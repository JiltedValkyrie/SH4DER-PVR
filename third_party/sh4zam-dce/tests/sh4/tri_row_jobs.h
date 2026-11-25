/* SH4ZAM-DCE tests: the records tri_row_check.c hands to tri_row_kernels.c

   tri_row_jobs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   One record a routine: the host fills it in the interpreter's RAM and calls the SH-4 function with its address. Every field is 32 bits
   (an SH-4 address or a number), so the host and the SH-4 lay it out alike.
*/

#ifndef TRI_ROW_JOBS_H
#define TRI_ROW_JOBS_H

#include <stdint.h>

typedef struct { uint32_t row_asm, row_c, row_d, row_ref, count, edge, tri, perspective; } job_tri_row_t;

#endif /* TRI_ROW_JOBS_H */
