/* SH4ZAM-DCE tests: shz_dce_xmtrx.h built without its prefetches, as the SH-4 runs it

   xmtrxnp_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   xmtrxnp_check <xmtrxnp_kernels.elf>: every check of xmtrx_check.c on the kernels built with SHZ_DCE_XMTRX_PREFETCH 0.
*/

#define XMTRX_CHECK_NAME "xmtrxnp_check"
#include "xmtrx_check.c"
