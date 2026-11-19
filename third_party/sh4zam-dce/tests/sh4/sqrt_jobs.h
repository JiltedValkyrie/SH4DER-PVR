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

/* what t_sqrt_flags reports in `count`: how the kernels were built */
#define SQRT_BUILT_FAST       0x01u   /* -ffast-math */
#define SQRT_BUILT_SQRT       0x02u   /* SHZ_DCE_OPAQUE_SQRT */
#define SQRT_BUILT_INV_SQRT   0x04u   /* SHZ_DCE_OPAQUE_INV_SQRT */
#define SQRT_BUILT_MAGNITUDE  0x08u   /* SHZ_DCE_OPAQUE_MAGNITUDE */
#define SQRT_BUILT_DISTANCE   0x10u   /* SHZ_DCE_OPAQUE_DISTANCE */
#define SQRT_BUILT_NORMALIZE2 0x20u   /* SHZ_DCE_OPAQUE_NORMALIZE2 */
#define SQRT_BUILT_NORMALIZE3 0x40u   /* SHZ_DCE_OPAQUE_NORMALIZE3 */
#define SQRT_BUILT_NORMALIZE4 0x80u   /* SHZ_DCE_OPAQUE_NORMALIZE4 */

/* in, in2: count x 4 floats (the scalar job reads the first of each four). Four outputs of count x SQRT_*_OUT floats each:
   SH4ZAM's routines, the public reworks (their barrier as the switches say), the reworks with the barrier off and with it on */
typedef struct { uint32_t in, in2, count, out_shz, out_dce, out_plain, out_opaque; } job_sqrt_t;

#endif /* SQRT_JOBS_H */
