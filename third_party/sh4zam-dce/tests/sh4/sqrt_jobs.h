/* SH4ZAM-DCE tests: the records sqrt_check.c hands to sqrt_kernels.c

   sqrt_jobs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Every field is 32 bits (an SH-4 address or a number), so the host and the SH-4 lay the record out alike.
*/

#ifndef SQRT_JOBS_H
#define SQRT_JOBS_H

#include <stdint.h>

#define SQRT_SCALAR_OUT 3    /* floats an element: sqrtf, inv_sqrtf, hypotf */
#define SQRT_VECTOR_OUT 18   /* floats an element: magnitude 2 3 4, magnitude_inv 2 3 4, distance 2 3 4, normalize_safe 2 + 3 + 4 */

#define SQRT_BUILT_FAST       0x01u   /* what t_sqrt_flags reports in `count`: built with -ffast-math */

/* in, in2: count x 4 floats (the scalar job reads the first of each four). Two outputs of count x SQRT_*_OUT floats each:
   SH4ZAM's routines and the wrappers */
typedef struct { uint32_t in, in2, count, out_shz, out_dce; } job_sqrt_t;

#endif /* SQRT_JOBS_H */
