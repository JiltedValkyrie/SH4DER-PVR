/* SH4ZAM-DCE tests: shz_dce_tri_row.h, the SH-4 side

   tri_row_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single; -O2 and -O3; the strict flags and the -ffast-math flags) and run by tri_row_check.c through the
   interpreter (tests/sh4run). Each function takes one record of tri_row_jobs.h. It runs the SH-4 loop, the plain C form, the dispatch
   name and the reference row (the running-add row the C form replaces, copied here) on the same row, into four separate rows. The
   SH-4 loop and the C forms differ by design (FIPR against running sums); tri_row_check.c compares each with the exact value, the
   dispatch with the SH-4 loop bit for bit, and the C form with the reference bit for bit.
   This file is also the compile check of the header: it instantiates every public routine, so every asm block is assembled.
*/

#include <sh4zam_dce/shz_dce_tri_row.h>

#include "tri_row_jobs.h"

#define P(type, addr) ((type)(uintptr_t)(addr))

/* The running-add row the C form replaces, as the shadow raster's fill loop has it: the row start from the triangle's constants, then
   five adds a texel. Kept here, unchanged, to be the reference shz_dce_tri_row_step_c must match bit for bit on the SH-4. */
static void reference_row(uint16_t *row, unsigned count, const float edge[3], const shz_dce_tri_row_t *tri, int perspective) {
    const float e0x = tri->step[0], e1x = tri->step[1], e2x = tri->step[2], lo = tri->step[3];
    const float a0 = tri->iw[0], a1 = tri->iw[1], a2 = tri->iw[2], k0 = tri->kw[0], k1 = tri->kw[1], k2 = tri->kw[2];
    const float dwx = e0x * a0 + e1x * a1 + e2x * a2, dkx = e0x * k0 + e1x * k1 + e2x * k2;
    float e0 = edge[0], e1 = edge[1], e2 = edge[2];
    float iw = e0 * a0 + e1 * a1 + e2 * a2, kw = e0 * k0 + e1 * k1 + e2 * k2;
    unsigned x;

    for (x = 0; x < count; x++) {
        if (e0 >= lo && e1 >= lo && e2 >= lo) {
            float q = kw;

            if (perspective) q = iw > tri->iw[3] ? kw * shz_invf_fsrra(iw) : 0.0f;
            q = shz_clampf(q, 1.0f, 65535.0f);
            if ((uint16_t)q > row[x]) row[x] = (uint16_t)q;
        }
        e0 += e0x; e1 += e1x; e2 += e2x; iw += dwx; kw += dkx;
    }
}

void t_tri_row(job_tri_row_t *j) {
    const float *edge = P(const float *, j->edge);
    const shz_dce_tri_row_t *tri = P(const shz_dce_tri_row_t *, j->tri);
    const int persp = (int)j->perspective;

    shz_dce_tri_row_step_sh4(P(uint16_t *, j->row_asm), j->count, edge, tri, persp);
    shz_dce_tri_row_step_c(P(uint16_t *, j->row_c), j->count, edge, tri, persp);
    shz_dce_tri_row_step(P(uint16_t *, j->row_d), j->count, edge, tri, persp);
    reference_row(P(uint16_t *, j->row_ref), j->count, edge, tri, persp);
}
