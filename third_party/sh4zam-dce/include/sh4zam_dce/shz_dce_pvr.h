/* SH4ZAM-DCE: PVR vertices through XMTRX with the perspective divide

   shz_dce_pvr.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

/** \file    shz_dce_pvr.h
    \brief   A stream of 32-byte PVR vertices whose position is still in model or world space,
             through XMTRX and the perspective divide, into the 32-byte vertices the PVR's tile
             accelerator takes.

    A mesh that is kept as ready-made PVR vertices (command word, position, texture coordinates,
    colour, offset colour) needs one thing a frame: its positions on the screen. This routine
    does that in one pass and leaves every other word as it was, so its output is a block that
    goes to the tile accelerator as it is (one store-queue copy).

    The record in, 32 bytes on a multiple of 4:
        word   0      the command word (any bits)
        floats 1..3   x, y, z
        words  4..7   any bits (texture coordinates, colour, offset colour)
    The record out, 32 bytes on a multiple of 32 (one cache line):
        word   0      the command word
        floats 1..3   sx = X inv, sy = Y inv, sz = inv, with (X, Y, Z, W) = XMTRX (x, y, z, 1) and
                      inv = FSRRA(W W) (SH4ZAM's shz_invf_fsrra): 1 / W for W > 0
        words  4..7   the source's, as they are
    So XMTRX holds the whole way to the screen: view, projection and the screen's scale and
    centre, with W the depth. Z is not used.

    A vertex with W <= 0 is not handled: FSRRA gives 1 / |W| (a mirrored point) and infinity at
    W = 0. Clip against the near plane first, or send only what is in front of it.

    Order: load XMTRX first; it is read, not changed.

    Alignment: in on a multiple of 4; out on a multiple of 32 for the SH-4 loop (on a multiple of
    4 only the plain form runs, without the line allocation). in and out must not overlap: the
    loop allocates a record's cache line before it has read the source record's last words. The
    SH-4 loop makes 4-byte accesses only.

    Placing: keep in and out from being a multiple of 16 KB apart. The SH-4's operand cache is
    16 KB and direct mapped, so two addresses a multiple of 16 KB apart share one cache entry;
    record i of the source and record i of the output then put each other out of the cache at
    every step, and so do the source lines asked for ahead. Two static arrays of 256 KB one
    after the other are exactly that. One console measured this loop at 132 cycles a vertex on
    such a placing, against 21 with both arrays in the cache and 58 with neither in it on a
    placing that shares no entry (half a cache apart at equal offsets). The same holds for any
    loop that reads one array and writes another.

    Bit-identical to the plain loop (the C form of this header): YES on the SH-4 (the same FTRV,
    the same FSRRA of the same product, one multiply each for sx and sy).
    Error bound: FTRV's ((largest product + abs result) x 2^-21 for X, Y, W) and FSRRA's; none
    added.
    FPSCR: a vertex at W = 0 runs FSRRA on 0 (flag Z, quiet with the exception enables off, as
    KallistiOS has them).

    Where the cycles go (E: estimates from the pipeline model, cache hits). A vertex is 18 loads,
    stores and register moves, one a cycle: 8 words in, 8 words out, the constant 1 and the
    prefetch. Its latency chain is longer than that (the transform 7, the square 3, the
    reciprocal root 5, the product 3), so the loop keeps two vertices in flight: the next
    vertex's loads, transform and carried words fill the wait for this one's reciprocal, with the
    two vertices in fv4 and fv8 in turn so that neither waits for a register.
    The input is prefetched 96 bytes ahead, also past its end (PREF raises no exception and reads
    nothing into the program; the address must not be in the store-queue area).

    Version: v0.10
    The effect: nothing on screen. The same vertices in fewer SH-4 cycles.
    Use it for: any mesh kept as PVR vertices and drawn through one matrix with no clipping by
      the CPU (it is all in front of the near plane; the tile accelerator clips the sides).
*/

#ifndef SHZ_DCE_PVR_H
#define SHZ_DCE_PVR_H

#include "shz_dce_cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief shz_dce_pvr_project() in plain C on SH4ZAM's routines: the software back end, the path
    for an output that is not on a multiple of 32, and the reference the SH-4 loop is tested
    against (the same FTRV, FSRRA and products, so the same bits). */
SHZ_INLINE void shz_dce_pvr_project_c(void *SHZ_RESTRICT out, const void *SHZ_RESTRICT in, size_t count) SHZ_NOEXCEPT {
    const int alloc = SHZ_DCE_IS_ALIGNED(out, 32);
    size_t i;

    for (i = 0; i < count; i++) {
        const float *s = (const float *)in + 8 * i;
        const shz_alias_uint32_t *sw = (const shz_alias_uint32_t *)s;
        float *o = (float *)out + 8 * i;
        shz_alias_uint32_t *ow = (shz_alias_uint32_t *)o;
        const uint32_t w0 = sw[0], w4 = sw[4], w5 = sw[5], w6 = sw[6], w7 = sw[7];
        const shz_vec4_t p = shz_xmtrx_transform_vec4(shz_vec4_init(s[1], s[2], s[3], 1.0f));
        const float inv = shz_invf_fsrra(p.w);

        if (alloc) shz_dcache_alloc_line(o);   /* the whole line is this record */
        ow[0] = w0;
        o[1] = p.x * inv; o[2] = p.y * inv; o[3] = inv;
        ow[4] = w4; ow[5] = w5; ow[6] = w6; ow[7] = w7;
    }
}

#if SHZ_DCE_SH4
/* Two instructions issue a cycle, of which one may be a load, store or register move (LS) and one an FPU instruction (FE).
   Registers. s: the source walk; q: the record whose line is allocated next; pf: the prefetch, 96 bytes ahead of s; n: the
   vertices left. r0: the command word, then the first carried word, then the offsets of the screen point's stores (the record
   before q's); r1..r3 the other three carried words. A vertex lives in fv4, the next in fv8, and so on in turn; W is squared
   and turned into its reciprocal in place.
   A step: finish the vertex before (W W, the reciprocal root, the two products, three stores) and begin this one (eight loads,
   the transform, the line and five stores), ordered so that nothing waits: 18 loads, stores and moves, 18 cycles in the model. */
#define SHZ_DCE_PV_FRONT_(cx, cy, cz, cw, cfv) \
    "mov.l   @%[s]+, r0\n\t"                     /* LS  the command word */ \
    "fmov.s  @%[s]+, " cx "\n\t"                 /* LS */ \
    "fmov.s  @%[s]+, " cy "\n\t"                 /* LS */ \
    "fmov.s  @%[s]+, " cz "\n\t"                 /* LS */ \
    "fldi1   " cw "\n\t"                         /* LS */ \
    "movca.l r0, @%[q]\n\t"                      /* LS  the line, and its word 0 */ \
    "ftrv    xmtrx, " cfv "\n\t"                 /* FE  X Y after 5 cycles, W after 7 */ \
    "mov.l   @%[s]+, r0\n\t"                     /* LS */ \
    "mov.l   @%[s]+, r1\n\t"                     /* LS */ \
    "mov.l   @%[s]+, r2\n\t"                     /* LS */ \
    "mov.l   @%[s]+, r3\n\t"                     /* LS */ \
    "mov.l   r0, @(16, %[q])\n\t"                /* LS */ \
    "mov.l   r1, @(20, %[q])\n\t"                /* LS */ \
    "mov.l   r2, @(24, %[q])\n\t"                /* LS */ \
    "mov.l   r3, @(28, %[q])\n\t"                /* LS */ \
    "add     #32, %[q]\n\t"

/* The last vertex: q is one record past it. */
#define SHZ_DCE_PV_BACK_(px, py, pw) \
    "fmul    " pw ", " pw "\n\t"                 /* FE  W W */ \
    "mov     #-20, r0\n\t" \
    "fsrra   " pw "\n\t"                         /* FE  1 / W */ \
    "fmul    " pw ", " px "\n\t"                 /* FE */ \
    "fmul    " pw ", " py "\n\t"                 /* FE */ \
    "fmov.s  " pw ", @(r0, %[q])\n\t"            /* LS  sz */ \
    "mov     #-24, r0\n\t" \
    "fmov.s  " py ", @(r0, %[q])\n\t"            /* LS  sy */ \
    "mov     #-28, r0\n\t" \
    "fmov.s  " px ", @(r0, %[q])\n"              /* LS  sx */

#define SHZ_DCE_PV_STEP_(cx, cy, cz, cw, cfv, px, py, pw) \
    "fmul    " pw ", " pw "\n\t"                 /* FE  the vertex before: W W (W ready since its own step) */ \
    "mov.l   @%[s]+, r0\n\t"                     /* LS  this vertex: the command word */ \
    "fmov.s  @%[s]+, " cx "\n\t"                 /* LS */ \
    "dt      %[n]\n\t"                           /*     T keeps until the branch after the step: nothing else here sets it */ \
    "fmov.s  @%[s]+, " cy "\n\t"                 /* LS */ \
    "fmov.s  @%[s]+, " cz "\n\t"                 /* LS */ \
    "fsrra   " pw "\n\t"                         /* FE  3 cycles of the FPU: over before the transform is wanted */ \
    "fldi1   " cw "\n\t"                         /* LS */ \
    "movca.l r0, @%[q]\n\t"                      /* LS */ \
    "mov.l   @%[s]+, r0\n\t"                     /* LS */ \
    "ftrv    xmtrx, " cfv "\n\t"                 /* FE  4 cycles of the FPU */ \
    "pref    @%[pf]\n\t"                         /* LS  96 bytes ahead */ \
    "add     #32, %[pf]\n\t" \
    "mov.l   @%[s]+, r1\n\t"                     /* LS */ \
    "mov.l   @%[s]+, r2\n\t"                     /* LS */ \
    "mov.l   @%[s]+, r3\n\t"                     /* LS */ \
    "fmul    " pw ", " px "\n\t"                 /* FE  the vertex before: X / W */ \
    "mov.l   r0, @(16, %[q])\n\t"                /* LS */ \
    "fmul    " pw ", " py "\n\t"                 /* FE  Y / W */ \
    "mov.l   r1, @(20, %[q])\n\t"                /* LS */ \
    "mov.l   r2, @(24, %[q])\n\t"                /* LS */ \
    "mov.l   r3, @(28, %[q])\n\t"                /* LS */ \
    "mov     #-20, r0\n\t" \
    "fmov.s  " pw ", @(r0, %[q])\n\t"            /* LS  sz, into the record before q's */ \
    "mov     #-24, r0\n\t" \
    "fmov.s  " py ", @(r0, %[q])\n\t"            /* LS  sy */ \
    "mov     #-28, r0\n\t" \
    "fmov.s  " px ", @(r0, %[q])\n\t"            /* LS  sx */ \
    "add     #32, %[q]\n\t"

/** \brief The SH-4 loop. out on a multiple of 32, in on a multiple of 4, count at least 1, no overlap. */
SHZ_INLINE void shz_dce_pvr_project_sh4(void *SHZ_RESTRICT out, const void *SHZ_RESTRICT in, size_t count) SHZ_NOEXCEPT {
    const uint32_t *s = (const uint32_t *)in;
    uint32_t *q = (uint32_t *)out;
    const uint8_t *pf = (const uint8_t *)in + 128u;   /* the first step loads record 1: 96 bytes ahead of it */
    size_t n = count;

    asm volatile(
        SHZ_DCE_PV_FRONT_("fr4", "fr5", "fr6", "fr7", "fv4")
        "dt      %[n]\n\t"
        "bt      8f\n"
        "1:\n\t"
        SHZ_DCE_PV_STEP_("fr8", "fr9", "fr10", "fr11", "fv8", "fr4", "fr5", "fr7")
        "bt      9f\n\t"
        SHZ_DCE_PV_STEP_("fr4", "fr5", "fr6", "fr7", "fv4", "fr8", "fr9", "fr11")
        "bf      1b\n"
        "8:\n\t"
        SHZ_DCE_PV_BACK_("fr4", "fr5", "fr7")
        "\tbra     7f\n\t"
        "nop\n"
        "9:\n\t"
        SHZ_DCE_PV_BACK_("fr8", "fr9", "fr11")
        "7:\n"
        : [s] "+&r" (s), [q] "+&r" (q), [pf] "+&r" (pf), [n] "+&r" (n)
        :
        : "r0", "r1", "r2", "r3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "t", "memory");
}

#undef SHZ_DCE_PV_FRONT_
#undef SHZ_DCE_PV_BACK_
#undef SHZ_DCE_PV_STEP_
#endif /* SHZ_DCE_SH4 */

/** \brief Project a stream of PVR vertices (see the file comment for the records and the arithmetic).
    out: count 32-byte records on a multiple of 32 (on a multiple of 4 only: the plain form, no line allocation). in: count
    32-byte records on a multiple of 4. in and out must not overlap. XMTRX is read, not changed. Every vertex must have W > 0.
    Bit-identical to the plain loop: YES. Cycles E a vertex: see tests/sh4/pvr_bench.c's table (the plain loop, KallistiOS's
    way of a divide, this). */
SHZ_INLINE void shz_dce_pvr_project(void *SHZ_RESTRICT out, const void *SHZ_RESTRICT in, size_t count) SHZ_NOEXCEPT {
    if (!count)
        return;
    SHZ_DCE_ASSERT_ALIGNED(out, 4);
    SHZ_DCE_ASSERT_ALIGNED(in, 4);
#if SHZ_DCE_SH4
    if (SHZ_DCE_IS_ALIGNED(out, 32)) {
        shz_dce_pvr_project_sh4(out, in, count);
        return;
    }
#endif
    shz_dce_pvr_project_c(out, in, count);
}

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_PVR_H */
