/* SH4ZAM-DCE tests: shz_dce_vertex.h, the SH-4 side

   vertex_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single; -O2 and -O3; the strict flags and the -ffast-math flags) and run by vertex_check.c through
   the interpreter (tests/sh4run). The function takes one record of vertex_jobs.h. It runs the dispatching routine, the plain C form and
   the SH-4 form, each into its own output, so the three can be compared bit for bit.
   This file is also the compile check of the header: it instantiates and calls every public routine. The pieces the SH-4 form is built
   from (one record, the fused multiply-add, the two loops) are called first, each on a range it may write; the full routines then
   overwrite those records, so only the full routines' results are compared.
*/

#include <sh4zam_dce/shz_dce_vertex.h>

#include "vertex_jobs.h"

#define P(type, addr) ((type)(uintptr_t)(addr))

void t_vertex(job_vertex_t *j) {
    const shz_dce_vertex20_grid_t *grid = P(const shz_dce_vertex20_grid_t *, j->grid);
    const shz_dce_vertex20_t *in = P(const shz_dce_vertex20_t *, j->in);
    shz_dce_vertex32_t *o_sh4 = P(shz_dce_vertex32_t *, j->out_sh4);
    const size_t n = (size_t)j->count;

    if (n >= 1u)
        shz_dce_vertex20_one_c(o_sh4, in, grid);
    (void)shz_dce_vertex_fmaf(1.0f, 2.0f, 3.0f);
#if SHZ_DCE_SH4
    if (n >= 3u && SHZ_DCE_IS_ALIGNED(o_sh4, 32))
        shz_dce_vertex20_pairs_movca_sh4(o_sh4, in, 1u, grid);   /* records 0 and 1; the line of record 2 (inside the batch) */
    if (n >= 3u && SHZ_DCE_IS_ALIGNED(o_sh4, 8) && !SHZ_DCE_IS_ALIGNED(o_sh4, 32))
        shz_dce_vertex20_pairs_plain_sh4(o_sh4, in, 1u, grid);   /* records 0 and 1 */
#endif

    shz_dce_vertex20_unpack(P(shz_dce_vertex32_t *, j->out_disp), in, n, grid);
    shz_dce_vertex20_unpack_c(P(shz_dce_vertex32_t *, j->out_c), in, n, grid);
#if SHZ_DCE_SH4
    shz_dce_vertex20_unpack_sh4(o_sh4, in, n, grid);
#else
    shz_dce_vertex20_unpack_c(o_sh4, in, n, grid);
#endif
}
