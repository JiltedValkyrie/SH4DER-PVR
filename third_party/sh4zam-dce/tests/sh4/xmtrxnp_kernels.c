/* SH4ZAM-DCE tests: shz_dce_xmtrx.h built without its prefetches, the SH-4 side

   xmtrxnp_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   The same kernels as xmtrx_kernels.c with SHZ_DCE_XMTRX_PREFETCH 0: the loops are other instruction sequences, so they are
   run and checked on their own (xmtrxnp_check.c).
*/

#define SHZ_DCE_XMTRX_PREFETCH 0
#include "xmtrx_kernels.c"
