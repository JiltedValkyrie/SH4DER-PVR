/* SH4ZAM-DCE: a stream of vertices through XMTRX

   shz_dce_xform.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

/** \file    shz_dce_xform.h
    \brief   Transform a stream of 32-byte vertices by XMTRX, with their view-space clip codes,
             two vertices in flight.

    A renderer without a vertex shader does this for every vertex: position times the matrix,
    a few floats carried along untouched, an outcode against the view volume. Written the plain
    way (load, FTRV, store, per vertex) the SH-4 waits out FTRV's seven cycles on every vertex,
    because it runs in order: the store of a result holds up everything behind it. Here the
    loads and the FTRV of vertex k + 1 are issued before the stores of vertex k, in two sets of
    registers, so nothing waits: the loop runs at the speed of the memory port.

    The vertex in: 32 bytes on a multiple of 4: floats x, y, z, then two 32-bit words that are
    copied through (texture coordinates, say), then 12 bytes that are not read.
    The vertex out: 32 bytes on a multiple of 32 (one cache line): floats x', y', z' = XMTRX times
    (x, y, z, 1), then the two words. Its last 12 bytes are NOT written and hold nothing defined
    afterwards (the line is allocated without a read): they are for a later pass to fill.

    Clip codes (one byte a vertex), for a view volume given by its slopes: a point is inside when
    near_z <= z <= far_z, |x| <= kx z and |y| <= ky z; the guard band is |x| <= gkx z and
    |y| <= gky z. SHZ_DCE_XFORM_NONE writes zeros (for a batch known to be inside),
    SHZ_DCE_XFORM_SIDES the four side bits, SHZ_DCE_XFORM_FULL all seven.

    XMTRX is read, not changed: load the matrix first (shz_xmtrx_load_3x4, for instance).

    Bit-identical to the plain loop (shz_xmtrx_transform_point3 a vertex, the compares written
    out): YES. The FTRV is the same instruction on the same operands; each compare is against one
    product (kx z, ...), rounded once either way; the copies are integer moves. The test puts
    points on every plane of the view volume and one unit in the last place to either side.
    (Two things FTRV itself does, here as in the plain loop: a denormal coordinate is read as 0,
    and a result that is exactly zero is +0.)

    Cycles E a vertex (see shz_dce_cdefs.h), the plain loop -> this: positions only 19 -> 14;
    with the four side bits 36 -> 22; with all seven bits 58 -> 33. A plain loop that also
    tests its prefetch and stores a code per vertex costs more (24, 45, 59). Where the cycles
    go: 13 loads and stores a vertex, one a cycle, is the floor of the positions; the side bits
    add four compares, two cycles each as there is one T bit, and they ride beside the loads and
    stores; the full code adds four more compares, four products and three shifts.

    The input is prefetched four records ahead and never past its end. An even batch's last
    vertex goes through the plain form when codes are asked for (the loops take two vertices a
    turn after the first).

    Version: v0.10
    The effect: nothing on screen. The same vertices in fewer SH-4 cycles.
    Use it for: world geometry and any other vertex stream that goes through one matrix.
*/

#ifndef SHZ_DCE_XFORM_H
#define SHZ_DCE_XFORM_H

#include <string.h>

#include "shz_dce_cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Clip code bits
    @{ */
#define SHZ_DCE_CLIP_NEAR   0x01u   /**< \brief z < near_z. */
#define SHZ_DCE_CLIP_FAR    0x02u   /**< \brief z > far_z. */
#define SHZ_DCE_CLIP_LEFT   0x04u   /**< \brief x < -kx z. */
#define SHZ_DCE_CLIP_RIGHT  0x08u   /**< \brief x > kx z. */
#define SHZ_DCE_CLIP_BOTTOM 0x10u   /**< \brief y < -ky z. */
#define SHZ_DCE_CLIP_TOP    0x20u   /**< \brief y > ky z. */
#define SHZ_DCE_CLIP_GUARD  0x40u   /**< \brief |x| > gkx z or |y| > gky z: outside the guard band. */
/** @} */

/** \name What shz_dce_xform_vertex32() computes besides the positions
    @{ */
#define SHZ_DCE_XFORM_NONE  0   /**< \brief Clip codes all 0 (the batch is known to be inside). */
#define SHZ_DCE_XFORM_SIDES 1   /**< \brief LEFT, RIGHT, BOTTOM, TOP. */
#define SHZ_DCE_XFORM_FULL  2   /**< \brief All seven bits. */
/** @} */

/** \brief A view volume by its slopes, for the clip codes. */
typedef struct shz_dce_clip {
    float kx, ky;       /**< \brief Side slopes: inside when |x| <= kx z, |y| <= ky z. */
    float gkx, gky;     /**< \brief Guard band slopes. */
    float near_z;       /**< \brief Near plane depth. */
    float far_z;        /**< \brief Far plane depth. */
} shz_dce_clip_t;

/** \brief The four side bits of a view-space point. */
SHZ_FORCE_INLINE unsigned shz_dce_clip_sides(float x, float y, float z, float kx, float ky) SHZ_NOEXCEPT {
    return ((unsigned)(x < -kx * z) << 2) | ((unsigned)(x > kx * z) << 3) | ((unsigned)(y < -ky * z) << 4) | ((unsigned)(y > ky * z) << 5);
}

/** \brief All seven bits of a view-space point. */
SHZ_FORCE_INLINE unsigned shz_dce_clip_full(float x, float y, float z, const shz_dce_clip_t *clip) SHZ_NOEXCEPT {
    return (unsigned)(z < clip->near_z) | ((unsigned)(z > clip->far_z) << 1) | shz_dce_clip_sides(x, y, z, clip->kx, clip->ky) |
           ((unsigned)(shz_fabsf(x) > clip->gkx * z || shz_fabsf(y) > clip->gky * z) << 6);
}

/** \brief shz_dce_xform_vertex32() in plain C on SH4ZAM's routines: the software back end, and the reference the SH-4 loop is tested
    against (the same FTRV, so the same bits). */
SHZ_INLINE unsigned shz_dce_xform_vertex32_c(float *SHZ_RESTRICT out, uint8_t *SHZ_RESTRICT codes, const float *SHZ_RESTRICT in, size_t count, int mode,
                                             const shz_dce_clip_t *clip) SHZ_NOEXCEPT {
    const int alloc = SHZ_DCE_IS_ALIGNED(out, 32);
    unsigned any = 0u;
    size_t i;

    for (i = 0; i < count; i++) {
        const float *s = in + 8 * i;
        const shz_alias_uint32_t *sw = (const shz_alias_uint32_t *)s;
        float *o = out + 8 * i;
        shz_alias_uint32_t *ow = (shz_alias_uint32_t *)o;
        const uint32_t w3 = sw[3], w4 = sw[4];
        shz_vec3_t p;
        unsigned code;

        if (alloc) shz_dcache_alloc_line(o);   /* the whole line is this record: allocated, not fetched; its last 12 bytes stay undefined */
        p = shz_xmtrx_transform_point3(shz_vec3_init(s[0], s[1], s[2]));
        o[0] = p.x; o[1] = p.y; o[2] = p.z;
        ow[3] = w3; ow[4] = w4;
        code = mode == SHZ_DCE_XFORM_NONE ? 0u : mode == SHZ_DCE_XFORM_SIDES ? shz_dce_clip_sides(p.x, p.y, p.z, clip->kx, clip->ky) : shz_dce_clip_full(p.x, p.y, p.z, clip);
        codes[i] = (uint8_t)code;
        any |= code;
    }
    return any;
}

#if SHZ_DCE_SH4
/* The steps of a vertex as text, for the two sets of registers the loop alternates. s: input pointer; dm: the output record being
   written (for the line allocation and the two carried words); db: dm + 12, made during the loads of the other vertex (the three
   floats are stored downwards from it, so no store waits on an address add); pp: the record to prefetch (four ahead); pe: the end of
   the input. The order inside each step is the pipeline's: one load or store a cycle, every add and compare beside one of them.
   LOAD: the next vertex in, its FTRV issued. STORE: the vertex before it out (its FTRV was issued one step ago, so its results wait
   for no one). STORE_TAIL is STORE with the loop's count and branch woven in: the last store rides in the delay slot. */
#define SHZ_DCE_XF_LOAD_(fx, fy, fz, fw, ru, rv, fv) \
    "cmp/hs  %[pe], %[pp]\n\t" \
    "bt      9f\n\t" \
    "pref    @%[pp]\n" \
    "9:\n\t" \
    "add     #32, %[pp]\n\t" \
    "fmov.s  @%[s]+, " fx "\n\t" \
    "mov     %[dm], %[db]\n\t" \
    "fmov.s  @%[s]+, " fy "\n\t" \
    "add     #12, %[db]\n\t" \
    "fmov.s  @%[s]+, " fz "\n\t" \
    "mov.l   @%[s]+, " ru "\n\t" \
    "mov.l   @%[s]+, " rv "\n\t" \
    "fldi1   " fw "\n\t" \
    "add     #12, %[s]\n\t" \
    "ftrv    xmtrx, " fv "\n\t"
#define SHZ_DCE_XF_STORE_(fx, fy, fz, ru, rv) \
    "movca.l r0, @%[dm]\n\t" \
    "mov.l   " ru ", @(12, %[dm])\n\t" \
    "mov.l   " rv ", @(16, %[dm])\n\t" \
    "add     #32, %[dm]\n\t" \
    "fmov.s  " fz ", @-%[db]\n\t" \
    "fmov.s  " fy ", @-%[db]\n\t" \
    "fmov.s  " fx ", @-%[db]\n\t"
#define SHZ_DCE_XF_STORE_TAIL_(fx, fy, fz, ru, rv) \
    "movca.l r0, @%[dm]\n\t" \
    "mov.l   " ru ", @(12, %[dm])\n\t" \
    "mov.l   " rv ", @(16, %[dm])\n\t" \
    "add     #32, %[dm]\n\t" \
    "fmov.s  " fz ", @-%[db]\n\t" \
    "dt      %[pairs]\n\t" \
    "fmov.s  " fy ", @-%[db]\n\t" \
    "bf/s    1b\n\t" \
    "fmov.s  " fx ", @-%[db]\n"
#define SHZ_DCE_XF_LOAD_A_ SHZ_DCE_XF_LOAD_("fr4", "fr5", "fr6", "fr7", "%[ua]", "%[va]", "fv4")
#define SHZ_DCE_XF_LOAD_B_ SHZ_DCE_XF_LOAD_("fr8", "fr9", "fr10", "fr11", "%[ub]", "%[vb]", "fv8")
#define SHZ_DCE_XF_STORE_A_ SHZ_DCE_XF_STORE_("fr4", "fr5", "fr6", "%[ua]", "%[va]")
#define SHZ_DCE_XF_STORE_B_ SHZ_DCE_XF_STORE_("fr8", "fr9", "fr10", "%[ub]", "%[vb]")
#define SHZ_DCE_XF_STORE_B_TAIL_ SHZ_DCE_XF_STORE_TAIL_("fr8", "fr9", "fr10", "%[ub]", "%[vb]")

/** \brief Positions only (SHZ_DCE_XFORM_NONE) on the SH-4: vertex k + 1 is loaded and put through FTRV before vertex k is stored. out on a
    multiple of 32; count >= 1. */
SHZ_INLINE void shz_dce_xform_vertex32_none_sh4(float *SHZ_RESTRICT out, const float *SHZ_RESTRICT in, size_t count) SHZ_NOEXCEPT {
    const float *pp = in + 8 * 4, *pe = in + 8 * count;   /* prefetch four records ahead, never past the end */
    float *db;
    size_t pairs = (count - 1u) >> 1;                      /* steps after the first load come in pairs: (load B, store A) (load A, store B) */
    const size_t odd = (count - 1u) & 1u;
    uint32_t ua, va, ub, vb;

    asm volatile(
        SHZ_DCE_XF_LOAD_A_
        "tst     %[pairs], %[pairs]\n\t"
        "bt      2f\n"
    "1:\n\t"
        SHZ_DCE_XF_LOAD_B_
        SHZ_DCE_XF_STORE_A_
        SHZ_DCE_XF_LOAD_A_
        SHZ_DCE_XF_STORE_B_TAIL_
    "2:\n\t"
        "tst     %[odd], %[odd]\n\t"
        "bt      3f\n\t"
        SHZ_DCE_XF_LOAD_B_
        SHZ_DCE_XF_STORE_A_
        "mov     %[dm], %[db]\n\t"
        "add     #12, %[db]\n\t"
        SHZ_DCE_XF_STORE_B_
        "bra     4f\n\t"
        "nop\n"
    "3:\n\t"
        "mov     %[dm], %[db]\n\t"
        "add     #12, %[db]\n\t"
        SHZ_DCE_XF_STORE_A_
    "4:\n"
        : [s] "+r" (in), [dm] "+r" (out), [db] "=&r" (db), [pp] "+r" (pp), [pairs] "+r" (pairs), [ua] "=&r" (ua), [va] "=&r" (va), [ub] "=&r" (ub),
          [vb] "=&r" (vb)
        : [pe] "r" (pe), [odd] "r" (odd)
        : "r0", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "t", "memory");
}

/* The side bits woven into the same loop. One step (STEP): the vertex Y, whose FTRV was issued a step ago, is stored and gets its four
   compares, while the next vertex X is loaded and put through FTRV. Every cycle carries one load or store; the compares (FE) and the
   collecting of T (EX) ride beside them, so the chain of four compares, two cycles each as there is one T bit, costs no cycle of its
   own. fr0 = kx, fr1 = ky, fr2 = ky z (then -ky z); kx z is made in place of z once z is stored. r0 collects top, bottom, right, left
   and is shifted to the documented bits; it is also the word MOVCA.L writes (any value: the store of x overwrites it).
   cp: the code's address, kept one behind and advanced early so the store waits on nothing. The prefetch test uses T: it sits after
   the chain. FIN is STEP without a next vertex. */
#define SHZ_DCE_XF_SIDES_STEP_(Xx, Xy, Xz, Xw, fvX, uX, vX, Yx, Yy, Yz, uY, vY) \
    "movca.l r0, @%[dm]\n\t" \
    "fmov    " Yz ", fr2\n\t" \
    "fmul    fr1, fr2\n\t" \
    "fmov.s  " Yz ", @-%[db]\n\t" \
    "fmul    fr0, " Yz "\n\t" \
    "fmov.s  @%[s]+, " Xx "\n\t" \
    "add     #1, %[cp]\n\t" \
    "fmov.s  " Yy ", @-%[db]\n\t" \
    "fcmp/gt fr2, " Yy "\n\t" \
    "fmov.s  " Yx ", @-%[db]\n\t" \
    "fneg    fr2\n\t" \
    "movt    r0\n\t" \
    "fcmp/gt " Yy ", fr2\n\t" \
    "fmov.s  @%[s]+, " Xy "\n\t" \
    "add     #44, %[db]\n\t" \
    "fmov.s  @%[s]+, " Xz "\n\t" \
    "rotcl   r0\n\t" \
    "fcmp/gt " Yz ", " Yx "\n\t" \
    "fneg    " Yz "\n\t" \
    "mov.l   @%[s]+, " uX "\n\t" \
    "rotcl   r0\n\t" \
    "fcmp/gt " Yx ", " Yz "\n\t" \
    "mov.l   @%[s]+, " vX "\n\t" \
    "fldi1   " Xw "\n\t" \
    "add     #12, %[s]\n\t" \
    "ftrv    xmtrx, " fvX "\n\t" \
    "rotcl   r0\n\t" \
    "mov.l   " uY ", @(12, %[dm])\n\t" \
    "shll2   r0\n\t" \
    "mov.l   " vY ", @(16, %[dm])\n\t" \
    "add     #32, %[dm]\n\t" \
    "mov.b   r0, @%[cp]\n\t" \
    "or      r0, %[any]\n\t" \
    "cmp/hs  %[pe], %[pp]\n\t" \
    "bt      9f\n\t" \
    "pref    @%[pp]\n" \
    "9:\n\t" \
    "add     #32, %[pp]\n\t"
#define SHZ_DCE_XF_SIDES_FIN_(Yx, Yy, Yz, uY, vY) \
    "movca.l r0, @%[dm]\n\t" \
    "fmov    " Yz ", fr2\n\t" \
    "fmul    fr1, fr2\n\t" \
    "fmov.s  " Yz ", @-%[db]\n\t" \
    "fmul    fr0, " Yz "\n\t" \
    "add     #1, %[cp]\n\t" \
    "fmov.s  " Yy ", @-%[db]\n\t" \
    "fcmp/gt fr2, " Yy "\n\t" \
    "fmov.s  " Yx ", @-%[db]\n\t" \
    "fneg    fr2\n\t" \
    "movt    r0\n\t" \
    "fcmp/gt " Yy ", fr2\n\t" \
    "mov.l   " uY ", @(12, %[dm])\n\t" \
    "mov.l   " vY ", @(16, %[dm])\n\t" \
    "rotcl   r0\n\t" \
    "fcmp/gt " Yz ", " Yx "\n\t" \
    "fneg    " Yz "\n\t" \
    "rotcl   r0\n\t" \
    "fcmp/gt " Yx ", " Yz "\n\t" \
    "rotcl   r0\n\t" \
    "shll2   r0\n\t" \
    "mov.b   r0, @%[cp]\n\t" \
    "or      r0, %[any]\n\t"

/** \brief SHZ_DCE_XFORM_SIDES on the SH-4. out on a multiple of 32; count odd (the caller does an even batch's last vertex apart), so the
    steps after the first load come in pairs. Returns the OR of the codes. */
SHZ_INLINE unsigned shz_dce_xform_vertex32_sides_sh4(float *SHZ_RESTRICT out, uint8_t *SHZ_RESTRICT codes, const float *SHZ_RESTRICT in, size_t count,
                                                     float kx_, float ky_) SHZ_NOEXCEPT {
    const float *pp = in + 8 * 4, *pe = in + 8 * count;
    float *db = out + 3;
    uint8_t *cp = codes - 1;
    size_t pairs = (count - 1u) >> 1;
    unsigned any = 0u;
    uint32_t ua, va, ub, vb;
    register float kx asm("fr0") = kx_;   /* the two slopes stay in fr0, fr1 for the whole batch */
    register float ky asm("fr1") = ky_;

    asm volatile(
        SHZ_DCE_XF_LOAD_A_
        "tst     %[pairs], %[pairs]\n\t"
        "bt      2f\n"
    "1:\n\t"
        SHZ_DCE_XF_SIDES_STEP_("fr8", "fr9", "fr10", "fr11", "fv8", "%[ub]", "%[vb]", "fr4", "fr5", "fr6", "%[ua]", "%[va]")
        SHZ_DCE_XF_SIDES_STEP_("fr4", "fr5", "fr6", "fr7", "fv4", "%[ua]", "%[va]", "fr8", "fr9", "fr10", "%[ub]", "%[vb]")
        "dt      %[pairs]\n\t"
        "bf      1b\n"
    "2:\n\t"
        SHZ_DCE_XF_SIDES_FIN_("fr4", "fr5", "fr6", "%[ua]", "%[va]")
        : [s] "+r" (in), [dm] "+r" (out), [db] "+r" (db), [pp] "+r" (pp), [cp] "+r" (cp), [any] "+r" (any), [pairs] "+r" (pairs), [ua] "=&r" (ua),
          [va] "=&r" (va), [ub] "=&r" (ub), [vb] "=&r" (vb)
        : [pe] "r" (pe), "f" (kx), "f" (ky)
        : "r0", "fr2", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "t", "memory");
    return any;
}

/* All seven bits, the same way. fr0 = -kx, fr1 = -ky, fr12 = gkx, fr13 = gky, fr14 = near_z, fr15 = far_z; fr2 and fr3 hold the
   products. The compares come in the order near, far, left, right, bottom, top, guard and ROTCR collects each from the top of r0, so
   after seven of them and a shift right by 25 the bits sit in the documented order, near lowest. z stays whole until the last product
   (gky z, made in its place); x and y become |x| and |y| in place once they are stored and their side compares are issued. The guard
   bit is one of two compares: the second is skipped when the first already set T. */
#define SHZ_DCE_XF_FULL_CHAIN_A_(Yx, Yy, Yz) \
    "movca.l r0, @%[dm]\n\t" \
    "fcmp/gt " Yz ", fr14\n\t" \
    "fmov.s  " Yz ", @-%[db]\n\t" \
    "fmov    fr0, fr2\n\t" \
    "fmul    " Yz ", fr2\n\t" \
    "rotcr   r0\n\t" \
    "fcmp/gt fr15, " Yz "\n\t"
#define SHZ_DCE_XF_FULL_CHAIN_B_(Yx, Yy, Yz) \
    "add     #1, %[cp]\n\t" \
    "fmov    fr1, fr3\n\t" \
    "fmul    " Yz ", fr3\n\t" \
    "rotcr   r0\n\t" \
    "fcmp/gt " Yx ", fr2\n\t" \
    "fmov.s  " Yy ", @-%[db]\n\t" \
    "fneg    fr2\n\t" \
    "rotcr   r0\n\t" \
    "fcmp/gt fr2, " Yx "\n\t" \
    "fmov.s  " Yx ", @-%[db]\n\t"
#define SHZ_DCE_XF_FULL_CHAIN_C_(Yx, Yy, Yz) \
    "rotcr   r0\n\t" \
    "fcmp/gt " Yy ", fr3\n\t" \
    "fneg    fr3\n\t" \
    "add     #44, %[db]\n\t" \
    "fmov    fr12, fr2\n\t" \
    "fmul    " Yz ", fr2\n\t" \
    "rotcr   r0\n\t" \
    "fcmp/gt fr3, " Yy "\n\t" \
    "fabs    " Yx "\n\t"
#define SHZ_DCE_XF_FULL_CHAIN_D_(Yx, Yy, Yz) \
    "fmul    fr13, " Yz "\n\t" \
    "rotcr   r0\n\t" \
    "fcmp/gt fr2, " Yx "\n\t" \
    "fabs    " Yy "\n\t"
#define SHZ_DCE_XF_FULL_CHAIN_E_(Yx, Yy, Yz) \
    "bt      8f\n\t" \
    "fcmp/gt " Yz ", " Yy "\n" \
    "8:\n\t"
#define SHZ_DCE_XF_FULL_CODE_ \
    "shlr16  r0\n\t" \
    "shlr8   r0\n\t" \
    "shlr    r0\n\t" \
    "mov.b   r0, @%[cp]\n\t" \
    "or      r0, %[any]\n\t"
#define SHZ_DCE_XF_FULL_STEP_(Xx, Xy, Xz, Xw, fvX, uX, vX, Yx, Yy, Yz, uY, vY) \
    SHZ_DCE_XF_FULL_CHAIN_A_(Yx, Yy, Yz) \
    "fmov.s  @%[s]+, " Xx "\n\t" \
    SHZ_DCE_XF_FULL_CHAIN_B_(Yx, Yy, Yz) \
    "fmov.s  @%[s]+, " Xy "\n\t" \
    SHZ_DCE_XF_FULL_CHAIN_C_(Yx, Yy, Yz) \
    "fmov.s  @%[s]+, " Xz "\n\t" \
    SHZ_DCE_XF_FULL_CHAIN_D_(Yx, Yy, Yz) \
    "mov.l   @%[s]+, " uX "\n\t" \
    SHZ_DCE_XF_FULL_CHAIN_E_(Yx, Yy, Yz) \
    "mov.l   @%[s]+, " vX "\n\t" \
    "fldi1   " Xw "\n\t" \
    "add     #12, %[s]\n\t" \
    "ftrv    xmtrx, " fvX "\n\t" \
    "rotcr   r0\n\t" \
    "mov.l   " uY ", @(12, %[dm])\n\t" \
    "mov.l   " vY ", @(16, %[dm])\n\t" \
    "add     #32, %[dm]\n\t" \
    SHZ_DCE_XF_FULL_CODE_ \
    "cmp/hs  %[pe], %[pp]\n\t" \
    "bt      9f\n\t" \
    "pref    @%[pp]\n" \
    "9:\n\t" \
    "add     #32, %[pp]\n\t"
#define SHZ_DCE_XF_FULL_FIN_(Yx, Yy, Yz, uY, vY) \
    SHZ_DCE_XF_FULL_CHAIN_A_(Yx, Yy, Yz) \
    SHZ_DCE_XF_FULL_CHAIN_B_(Yx, Yy, Yz) \
    SHZ_DCE_XF_FULL_CHAIN_C_(Yx, Yy, Yz) \
    SHZ_DCE_XF_FULL_CHAIN_D_(Yx, Yy, Yz) \
    "mov.l   " uY ", @(12, %[dm])\n\t" \
    SHZ_DCE_XF_FULL_CHAIN_E_(Yx, Yy, Yz) \
    "mov.l   " vY ", @(16, %[dm])\n\t" \
    "rotcr   r0\n\t" \
    SHZ_DCE_XF_FULL_CODE_

/** \brief SHZ_DCE_XFORM_FULL on the SH-4. out on a multiple of 32; count odd. Returns the OR of the codes. */
SHZ_INLINE unsigned shz_dce_xform_vertex32_full_sh4(float *SHZ_RESTRICT out, uint8_t *SHZ_RESTRICT codes, const float *SHZ_RESTRICT in, size_t count,
                                                    const shz_dce_clip_t *clip) SHZ_NOEXCEPT {
    const float *pp = in + 8 * 4, *pe = in + 8 * count;
    float *db = out + 3;
    uint8_t *cp = codes - 1;
    size_t pairs = (count - 1u) >> 1;
    unsigned any = 0u;
    uint32_t ua, va, ub, vb;
    register float nkx asm("fr0") = -clip->kx;   /* the six numbers of the view volume stay in registers for the whole batch */
    register float nky asm("fr1") = -clip->ky;
    register float gkx asm("fr12") = clip->gkx;
    register float gky asm("fr13") = clip->gky;
    register float nz asm("fr14") = clip->near_z;
    register float fz asm("fr15") = clip->far_z;

    asm volatile(
        SHZ_DCE_XF_LOAD_A_
        "tst     %[pairs], %[pairs]\n\t"
        "bt      2f\n"
    "1:\n\t"
        SHZ_DCE_XF_FULL_STEP_("fr8", "fr9", "fr10", "fr11", "fv8", "%[ub]", "%[vb]", "fr4", "fr5", "fr6", "%[ua]", "%[va]")
        SHZ_DCE_XF_FULL_STEP_("fr4", "fr5", "fr6", "fr7", "fv4", "%[ua]", "%[va]", "fr8", "fr9", "fr10", "%[ub]", "%[vb]")
        "dt      %[pairs]\n\t"
        "bf      1b\n"
    "2:\n\t"
        SHZ_DCE_XF_FULL_FIN_("fr4", "fr5", "fr6", "%[ua]", "%[va]")
        : [s] "+r" (in), [dm] "+r" (out), [db] "+r" (db), [pp] "+r" (pp), [cp] "+r" (cp), [any] "+r" (any), [pairs] "+r" (pairs), [ua] "=&r" (ua),
          [va] "=&r" (va), [ub] "=&r" (ub), [vb] "=&r" (vb)
        : [pe] "r" (pe), "f" (nkx), "f" (nky), "f" (gkx), "f" (gky), "f" (nz), "f" (fz)
        : "r0", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "t", "memory");
    return any;
}

#undef SHZ_DCE_XF_FULL_CHAIN_A_
#undef SHZ_DCE_XF_FULL_CHAIN_B_
#undef SHZ_DCE_XF_FULL_CHAIN_C_
#undef SHZ_DCE_XF_FULL_CHAIN_D_
#undef SHZ_DCE_XF_FULL_CHAIN_E_
#undef SHZ_DCE_XF_FULL_CODE_
#undef SHZ_DCE_XF_FULL_STEP_
#undef SHZ_DCE_XF_FULL_FIN_
#undef SHZ_DCE_XF_SIDES_STEP_
#undef SHZ_DCE_XF_SIDES_FIN_
#undef SHZ_DCE_XF_LOAD_
#undef SHZ_DCE_XF_STORE_
#undef SHZ_DCE_XF_STORE_TAIL_
#undef SHZ_DCE_XF_LOAD_A_
#undef SHZ_DCE_XF_LOAD_B_
#undef SHZ_DCE_XF_STORE_A_
#undef SHZ_DCE_XF_STORE_B_
#undef SHZ_DCE_XF_STORE_B_TAIL_
#endif

/** \brief Transform count vertices by XMTRX and give their clip codes. Returns the OR of the codes.
    out: 32-byte records on a multiple of 32 (on a multiple of 4 only, the plain C form runs instead: no cache line is allocated, about
    the plain loop's speed); in: 32-byte records on a multiple of 4; codes: count bytes; clip: read for SHZ_DCE_XFORM_SIDES and
    SHZ_DCE_XFORM_FULL, may be NULL for SHZ_DCE_XFORM_NONE. in and out must not overlap. XMTRX is read, not changed.
    Bytes 20..31 of every output record are not written and are undefined afterwards.
    Bit-identical to the plain loop: yes. Cycles E a vertex: 14 (NONE), 22 (SIDES), 33 (FULL); the plain loop: 19, 36, 58. */
SHZ_INLINE unsigned shz_dce_xform_vertex32(float *SHZ_RESTRICT out, uint8_t *SHZ_RESTRICT codes, const float *SHZ_RESTRICT in, size_t count, int mode,
                                           const shz_dce_clip_t *clip) SHZ_NOEXCEPT {
    SHZ_DCE_ASSERT_ALIGNED(out, 4);
    SHZ_DCE_ASSERT_ALIGNED(in, 4);
#if SHZ_DCE_SH4
    if (count && SHZ_DCE_IS_ALIGNED(out, 32)) {
        if (mode == SHZ_DCE_XFORM_NONE) {
            shz_dce_xform_vertex32_none_sh4(out, in, count);
            memset(codes, 0, count);
            return 0u;
        }
        if (mode == SHZ_DCE_XFORM_SIDES || mode == SHZ_DCE_XFORM_FULL) {
            const size_t paired = (count - 1u) | 1u;   /* the loops take an odd count: an even batch's last vertex goes through the plain form */
            unsigned any = mode == SHZ_DCE_XFORM_SIDES ? shz_dce_xform_vertex32_sides_sh4(out, codes, in, paired, clip->kx, clip->ky)
                                                       : shz_dce_xform_vertex32_full_sh4(out, codes, in, paired, clip);

            if (paired != count)
                any |= shz_dce_xform_vertex32_c(out + 8 * paired, codes + paired, in + 8 * paired, 1u, mode, clip);
            return any;
        }
    }
#endif
    return shz_dce_xform_vertex32_c(out, codes, in, count, mode, clip);
}

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_XFORM_H */
