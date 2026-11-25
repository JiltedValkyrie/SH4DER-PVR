/* SH4ZAM-DCE: a triangle's texels in one row, into a 16-bit depth row

   shz_dce_tri_row.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
   NOT affiliated with, endorsed by or maintained by Falco Girgis or the SH4ZAM
   project. Report problems with this library to SH4DER-PVR, not to them.
*/

/** \file    shz_dce_tri_row.h
    \brief   One row of a triangle, texel by texel, into a 16-bit depth row: the coverage test and the depth of each covered texel.

    A triangle is three edge functions e0, e1, e2 (each affine in the texel centre) and three vertex values: the depth over w
    (kw) and 1 / w (iw), each already divided by the triangle's area. A texel is covered when all three edges are >= -tolerance,
    and its depth is kw / iw there, clamped to 1 .. 65535 and truncated; the row keeps the larger of the new and the stored depth.

    Two routines do the same job with two different ways of getting kw and iw:
    - shz_dce_tri_row_step_c (the reference, and the SH4ZAM build's host path): kw and iw are running sums, one add a texel each,
      started from the row's first texel. Five adds a texel, covered or not.
    - shz_dce_tri_row_step_sh4 (the SH-4 loop): the edges are the same running sums, so the covered texels are the same; kw and
      iw of a covered texel are the barycentric blend of the three vertices, computed by one FIPR each (the inner product of the
      edge values with the weights). The running kw and iw sums are not needed: the covered texels are a fifth of a shadow map,
      so the FIPRs are paid there and the rejected texels pay three adds instead of five.
    shz_dce_tri_row_step picks one by target. Both take the same constants and the same row start.

    Order: the caller computes the three edge functions at the first texel centre (the expression a running-add row uses) and
    fills a shz_dce_tri_row_t. Nothing is loaded first. Neither routine touches XMTRX, and neither changes FPSCR.

    Alignment: row on a multiple of 2 (16-bit stores); edge on a multiple of 4; tri (48 bytes) on a multiple of 4.

    Constants (shz_dce_tri_row_t): step[0..2] the change of each edge per texel (the x step), step[3] minus the edge tolerance
    (<= 0); kw[0..2] and iw[0..2] the vertices' values weighted by 1 / area (the barycentric weight times the value); kw[3] is not
    read; iw[3] is the guard: at or below it a texel stores depth 1 (a plain loop's guard against an infinite depth). The caller
    must set iw[3] to at least SHZ_DCE_TRI_ROW_IW_FLOOR: the SH-4 reciprocal is FSRRA of iw squared, and iw squared must stay a
    normal float (it is flushed to zero below about 1.1e-19 when FPSCR.DN is set, as KallistiOS sets it).

    Error bound (running sums, shz_dce_tri_row_step_c): each running edge value is within count x 2^-24 x M of the exact edge
    function at its texel, M the largest partial sum (the rounding of count additions of exact steps). The depth sums obey the
    same kind of bound with their own M, plus the rounding of the two steps dwx and dkx, which are computed once per call. Those
    are the bounds the routine's own arithmetic gives; the tests measure the depth against an exact value and print the result.
    Error bound (FIPR, shz_dce_tri_row_step_sh4): the edges are the same running sums, so the coverage bound is the same. A
    covered depth is one FIPR of four products, one of them a zero: the manual's bound is (largest product + |result|) x 2^-23
    for each of kw and iw. The products are vertex values times barycentric weights in [0, 1] (a little below zero inside the
    tolerance), so the sums have no cancellation: a relative error of about 2^-22 on the depth.

    Bit-identical: shz_dce_tri_row_step_c is bit-identical to the running-add row it replaces under strict IEEE flags (the same
    expressions in the same order; the host test checks every texel of 20000 random triangles). Under -ffast-math the compiler may
    contract or reorder the same expressions differently in another inline context: measured on the same triangles, 50128 texels
    differ (none in coverage; the largest by 748 units, on slivers). shz_dce_tri_row_step_sh4 is NOT bit-identical: a dot product
    and a running sum round differently, so the depth differs in its last bits, and the truncation to 16 bits turns that into a
    one-unit step on a share of covered texels. The coverage is the same on the texels the test judges clearly (the edges are the
    same running sums in both). A NaN edge: _c counts it as not covered (its comparison is
    `e >= tolerance`); _sh4 counts it as covered. A non-finite edge needs non-finite vertices, which the caller rejects first.

    Where it pays (cycles E a texel, the interpreter's pipeline model, not a console; -O2 and -O3 of the same source; the running-add
    row is the BEFORE): a rejected texel 12 -> 10 at both levels; a covered perspective texel 50 -> 44 at -O2 and 43 -> 44 at -O3; a
    covered flat texel 32 -> 30 at -O2 and 27 -> 30 at -O3. The gain is on the rejected texels, which are most of a shadow map. On
    covered texels it is neutral or worse: the flat covered path at -O3 is about 11% slower. The row start (two dot products a row)
    is not in these per-texel figures; the SH-4 loop does not compute it. See KERNEL_GUIDE.md for the counts and what was not measured.

    Version: v0.10
    The effect: nothing on screen; a depth or shadow map filled by the CPU is produced by the same coverage and depth, one unit
      apart on the covered texels for the SH-4 loop.
    Use it for: a software raster of triangles into a 16-bit depth row, one row at a time (a depth map, a shadow map).
*/

#ifndef SHZ_DCE_TRI_ROW_H
#define SHZ_DCE_TRI_ROW_H

#include "shz_dce_cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief The smallest 1 / w a caller may guard with: iw squared stays a normal float (2^-126 and up) for every iw at or above
    it, so FSRRA of iw squared is defined on the SH-4 with FPSCR.DN set. The guard of a texel is iw[3] of the triangle. */
#define SHZ_DCE_TRI_ROW_IW_FLOOR 1.0e-18f

/** \brief A triangle's constants for one row walk: 48 bytes, on a multiple of 4. See the file comment for each field. */
typedef struct shz_dce_tri_row {
    float step[4];   /**< \brief step[0..2]: each edge's change per texel; step[3]: minus the edge tolerance (<= 0). */
    float kw[4];     /**< \brief kw[0..2]: the vertices' depth over w, each divided by the area; kw[3] not read. */
    float iw[4];     /**< \brief iw[0..2]: the vertices' 1 / w, each divided by the area; iw[3]: the guard (at or below it: depth 1). */
} shz_dce_tri_row_t;

/** \brief shz_dce_tri_row_step() in plain C on SH4ZAM's reciprocal: the running-add row, unchanged.
    Inputs: row (the first texel of the row, count texels to the right), edge[3] (the three edge functions at the first texel centre),
    tri (the constants), perspective (nonzero: kw / iw with 1 / w read; zero: flat, kw is the depth). Overwrites: row[0 .. count-1],
    and only where the new depth is larger. Alignment: row on a multiple of 2, edge and tri on a multiple of 4 (the dispatch asserts
    them). Error bound: the running sums of the file comment (edges within count x 2^-24 x M; depths not tightly bounded). Bit-identical
    to the running-add row it replaces: YES under strict IEEE flags; not promised under -ffast-math (the file comment measures it).
    Cycles E at -O3 (-O2 in brackets): persp covered 43 (43), persp rejected 12 (12), flat covered 27 (27), flat rejected 11 (11).
    The running-add row it replaces: 43 (50), 12 (12), 27 (32), 11 (12). */
SHZ_INLINE void shz_dce_tri_row_step_c(uint16_t *SHZ_RESTRICT row, unsigned count, const float edge[3],
                                       const shz_dce_tri_row_t *SHZ_RESTRICT tri, int perspective) SHZ_NOEXCEPT {
    const float lo = tri->step[3];
    const float e0x = tri->step[0], e1x = tri->step[1], e2x = tri->step[2];
    const float k0 = tri->kw[0], k1 = tri->kw[1], k2 = tri->kw[2];
    const float a0 = tri->iw[0], a1 = tri->iw[1], a2 = tri->iw[2];
    const float dwx = e0x * a0 + e1x * a1 + e2x * a2, dkx = e0x * k0 + e1x * k1 + e2x * k2;
    float e0 = edge[0], e1 = edge[1], e2 = edge[2];
    float iw = e0 * a0 + e1 * a1 + e2 * a2, kw = e0 * k0 + e1 * k1 + e2 * k2;
    unsigned i;

    for (i = 0; i < count; i++) {
        if (e0 >= lo && e1 >= lo && e2 >= lo) {
            float q = kw;

            if (perspective) q = iw > tri->iw[3] ? kw * shz_invf_fsrra(iw) : 0.0f;
            q = shz_clampf(q, 1.0f, 65535.0f);
            if ((uint16_t)q > row[i]) row[i] = (uint16_t)q;
        }
        e0 += e0x; e1 += e1x; e2 += e2x; iw += dwx; kw += dkx;
    }
}

#if SHZ_DCE_SH4
/** \brief shz_dce_tri_row_step() on the SH-4 (FIPR): the same covered texels, the depth by one FIPR each (barycentric).
    Inputs: as shz_dce_tri_row_step_c(). Overwrites: row where the new depth is larger; the FP registers fr0..fr15 and FPUL (the loop's),
    r0 and two integer temporaries. Does not touch XMTRX; FPSCR as found. Alignment: row 2, edge 4, tri 4 (asserted by the dispatch).
    Error bound: the FIPR bound of the file comment, about 2^-22 relative on the depth; the coverage is the running sums' own.
    Bit-identical to shz_dce_tri_row_step_c(): NO (a dot product against running sums; the same covered texels). A NaN edge counts as
    covered here and as not covered in _c.
    Cycles E at -O3 and -O2 (the same): persp covered 44, persp rejected 10, flat covered 30, flat rejected 10.
    The register map is fixed for the loop: fv0 = (e0, e1, e2, fr3 = 0): the running edges and the zero that multiplies the junk in
    the planes' fourth elements; fv4 = (k0, k1, k2, fr7): the depth FIPR's result goes to fr7; fv8 = (a0, a1, a2, fr11): the 1 / w
    FIPR's result goes to fr11; fr12..fr14 the edge steps; fr15 minus the tolerance, which the perspective guard borrows for one
    compare (it loads iw[3] into fr15 and puts the tolerance back). fr7 and fr11 are set to +0 on entry: a NaN left there would make
    every depth NaN, since 0 x NaN is NaN. The reciprocal is FMUL, FSRRA, FMUL (1 / |iw| from iw squared), the same value
    SH4ZAM's shz_invf_fsrra makes, so the caller's guard must keep iw squared normal (SHZ_DCE_TRI_ROW_IW_FLOOR). The clamp runs on
    the integer FTRC gives (a saturated truncation of the depth): it needs no FP constant register. Two loops, flat and
    perspective, chosen once a call. Does not touch XMTRX; FPSCR as it was. */
SHZ_INLINE void shz_dce_tri_row_step_sh4(uint16_t *SHZ_RESTRICT row, unsigned count, const float edge[3],
                                         const shz_dce_tri_row_t *SHZ_RESTRICT tri, int perspective) SHZ_NOEXCEPT {
    const float *t = &tri->step[0];
    const float *ne = &tri->step[3];
    const float *g = &tri->iw[3];
    uint32_t v, c;
    const float *e = edge;

    if (!count)
        return;
    if (perspective) {
        asm volatile(R"(
            fmov.s  @%[e]+, fr0
            fmov.s  @%[e]+, fr1
            fmov.s  @%[e], fr2
            fldi0   fr3
            fldi0   fr7
            fldi0   fr11
            fmov.s  @%[t]+, fr12
            fmov.s  @%[t]+, fr13
            fmov.s  @%[t]+, fr14
            fmov.s  @%[t]+, fr15
            fmov.s  @%[t]+, fr4
            fmov.s  @%[t]+, fr5
            fmov.s  @%[t]+, fr6
            add     #4, %[t]
            fmov.s  @%[t]+, fr8
            fmov.s  @%[t]+, fr9
            fmov.s  @%[t]+, fr10
            mov     #0, r0
        1:
            fcmp/gt fr0, fr15
            bt      3f
            fcmp/gt fr1, fr15
            bt      3f
            fcmp/gt fr2, fr15
            bt      3f
            fipr    fv0, fv4
            fipr    fv0, fv8
            fmov.s  @%[g], fr15
            fcmp/gt fr15, fr11
            fmov.s  @%[ne], fr15
            bf      4f
            fmul    fr11, fr11
            fsrra   fr11
            fmul    fr11, fr7
            ftrc    fr7, fpul
            sts     fpul, %[v]
            cmp/pl  %[v]
            bt      7f
            mov     #1, %[v]
            bra     8f
            nop
        4:
            mov     #1, %[v]
            bra     8f
            nop
        7:
            mov     #-1, %[c]
            extu.w  %[c], %[c]
            cmp/gt  %[c], %[v]
            bf      8f
            mov     %[c], %[v]
        8:
            mov.w   @(r0, %[row]), %[c]
            extu.w  %[c], %[c]
            cmp/hi  %[c], %[v]
            bf      3f
            mov.w   %[v], @(r0, %[row])
        3:
            fadd    fr12, fr0
            fadd    fr13, fr1
            fadd    fr14, fr2
            add     #2, r0
            dt      %[n]
            bf/s    1b
            nop
        )"
        : [e] "+r" (e), [t] "+r" (t), [n] "+r" (count), [v] "=&r" (v), [c] "=&r" (c)
        : [row] "r" (row), [ne] "r" (ne), [g] "r" (g)
        : "r0", "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14",
          "fr15", "fpul", "t", "memory");
    } else {
        asm volatile(R"(
            fmov.s  @%[e]+, fr0
            fmov.s  @%[e]+, fr1
            fmov.s  @%[e], fr2
            fldi0   fr3
            fldi0   fr7
            fmov.s  @%[t]+, fr12
            fmov.s  @%[t]+, fr13
            fmov.s  @%[t]+, fr14
            fmov.s  @%[t]+, fr15
            fmov.s  @%[t]+, fr4
            fmov.s  @%[t]+, fr5
            fmov.s  @%[t]+, fr6
            mov     #0, r0
        1:
            fcmp/gt fr0, fr15
            bt      3f
            fcmp/gt fr1, fr15
            bt      3f
            fcmp/gt fr2, fr15
            bt      3f
            fipr    fv0, fv4
            ftrc    fr7, fpul
            sts     fpul, %[v]
            cmp/pl  %[v]
            bt      7f
            mov     #1, %[v]
            bra     8f
            nop
        7:
            mov     #-1, %[c]
            extu.w  %[c], %[c]
            cmp/gt  %[c], %[v]
            bf      8f
            mov     %[c], %[v]
        8:
            mov.w   @(r0, %[row]), %[c]
            extu.w  %[c], %[c]
            cmp/hi  %[c], %[v]
            bf      3f
            mov.w   %[v], @(r0, %[row])
        3:
            fadd    fr12, fr0
            fadd    fr13, fr1
            fadd    fr14, fr2
            add     #2, r0
            dt      %[n]
            bf/s    1b
            nop
        )"
        : [e] "+r" (e), [t] "+r" (t), [n] "+r" (count), [v] "=&r" (v), [c] "=&r" (c)
        : [row] "r" (row)
        : "r0", "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr12", "fr13", "fr14", "fr15", "fpul", "t", "memory");
    }
}
#endif

/** \brief Walk one row of a triangle (the dispatch): the texels row[0 .. count-1] (row points at the first texel), edge the three edge
    functions at the first texel centre, tri the triangle's constants, perspective nonzero for 1 / w. Needs no load first; touches no
    XMTRX. Alignment: row 2, edge 4, tri 4, asserted in a debug build. Error bounds and bit identity: the routine named by the target
    (shz_dce_tri_row_step_c on the host, bit-identical to the running-add row; shz_dce_tri_row_step_sh4 on the SH-4, FIPR). Cycles E: the
    figures of the routine it names. */
SHZ_INLINE void shz_dce_tri_row_step(uint16_t *SHZ_RESTRICT row, unsigned count, const float edge[3],
                                     const shz_dce_tri_row_t *SHZ_RESTRICT tri, int perspective) SHZ_NOEXCEPT {
    SHZ_DCE_ASSERT_ALIGNED(row, 2);
    SHZ_DCE_ASSERT_ALIGNED(edge, 4);
    SHZ_DCE_ASSERT_ALIGNED(tri, 4);
#if SHZ_DCE_SH4
    shz_dce_tri_row_step_sh4(row, count, edge, tri, perspective);
#else
    shz_dce_tri_row_step_c(row, count, edge, tri, perspective);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_TRI_ROW_H */
