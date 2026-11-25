/* SH4DER-PVR: the ray bench, page h3's second build of SH4ZAM-DCE's projecting batch: SHZ_DCE_XMTRX_PREFETCH 0 (no PREF).
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A file of its own, so one program holds both builds of the header (bench_ray.c takes the default, 1).
*/

#define SHZ_DCE_XMTRX_PREFETCH 0
#include "sh4zam/shz_sh4zam.h"
#include "sh4zam_dce/shz_dce_xmtrx.h"

void h3_project_np(void *out, const void *in, unsigned n);

void __attribute__((noinline)) h3_project_np(void *out, const void *in, unsigned n) {
    shz_dce_xmtrx_project_point3_batch(out, 16u, in, 16u, n);
}
