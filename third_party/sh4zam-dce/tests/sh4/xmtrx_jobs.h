/* SH4ZAM-DCE tests: the records xmtrx_check.c hands to xmtrx_kernels.c

   xmtrx_jobs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

#ifndef XMTRX_JOBS_H
#define XMTRX_JOBS_H

#include <stdint.h>

enum { XMTRX_VEC4 = 0, XMTRX_POINT3 = 1, XMTRX_VEC3 = 2, XMTRX_PROJECT = 3, XMTRX_KINDS = 4 };

/* matrix: a shz_mat4x4_t to load into XMTRX first. in_a, in_c: the same input twice, for the routine and for its C form (two
   copies so that each can be transformed in place: out_a == in_a and out_c == in_c). Strides in bytes. */
typedef struct { uint32_t matrix, kind, count, in_a, in_c, in_stride, out_a, out_c, out_stride; } job_xmtrx_t;

#endif /* XMTRX_JOBS_H */
