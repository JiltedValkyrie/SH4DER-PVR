/* SH4ZAM-DCE tests: the records pvr_check.c hands to pvr_kernels.c

   pvr_jobs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

#ifndef PVR_JOBS_H
#define PVR_JOBS_H

#include <stdint.h>

/* matrix: a shz_mat4x4_t to load into XMTRX first; in, out_*: 32-byte records */
typedef struct { uint32_t matrix, in, count, out_asm, out_c; } job_pvr_t;

#endif /* PVR_JOBS_H */
