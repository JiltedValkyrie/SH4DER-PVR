/* SH4ZAM-DCE tests: the records xform_check.c hands to xform_kernels.c

   xform_jobs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

#ifndef XFORM_JOBS_H
#define XFORM_JOBS_H

#include <stdint.h>

/* matrix: a shz_mat4x4_t to load into XMTRX first; clip: a shz_dce_clip_t; out_*: 32-byte records; codes_*: count bytes */
typedef struct { uint32_t matrix, clip, in, count, mode, out_asm, codes_asm, ret_asm, out_c, codes_c, ret_c; } job_xform_t;

#endif /* XFORM_JOBS_H */
