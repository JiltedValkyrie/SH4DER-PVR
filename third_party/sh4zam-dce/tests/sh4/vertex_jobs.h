/* SH4ZAM-DCE tests: the records vertex_check.c hands to vertex_kernels.c

   vertex_jobs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   One record a routine: the host fills it in the interpreter's RAM and calls the SH-4 function with its address. Every field is 32 bits
   (an SH-4 address or a number), so the host and the SH-4 lay it out alike.
*/

#ifndef VERTEX_JOBS_H
#define VERTEX_JOBS_H

#include <stdint.h>

typedef struct { uint32_t in, grid, out_disp, out_c, out_sh4, count; } job_vertex_t;

#endif /* VERTEX_JOBS_H */
