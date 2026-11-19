/* SH4ZAM-DCE: transform, clip code and screen point of a vertex stream

   shz_dce_project.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

/** \file    shz_dce_project.h
    \brief   A stream of vertices through XMTRX into 32-byte records with a near-first clip code
             against four slopes and the projected screen point, with vertices that may be
             skipped.

    The whole per-vertex front end of a software renderer in one pass: position times the
    matrix, two words carried along, a third word copied to an array of its own, an outcode
    against a view volume that need not be centred, and for every vertex in front of the near
    plane its screen point. A vertex can be marked beforehand as not needed (a lazy transform:
    a pass over the faces decides which vertices any visible face uses): it is left alone.

    The source: records of `stride` bytes (24 or more, a multiple of 4) on a multiple of 4:
    floats x y z, then three 32-bit words. The rest of a record is not read.
    The record out, 32 bytes on a multiple of 32 (one cache line):
        floats 0..2   (x', y', z') = XMTRX (x, y, z, 1)
        words  3..4   the source's words 3 and 4, as they are (texture coordinates, say; a
                      caller that wants something else there writes it afterwards)
        floats 5..7   the screen point (sx, sy, sz), ONLY for a vertex whose near bit is clear.
                      With the near bit set these 12 bytes are undefined (the line was allocated
                      without a read): nothing may read them.
    words[i] gets the source's word 5 (a colour, say). codes[i] gets the clip code.

    The clip code, near first, with z' the depth:
        z' < near_z                 code = SHZ_DCE_PROJECT_NEAR, and nothing else
        else                        X_LO when x' < x_lo z',  X_HI when x' > x_hi z',
                                    Y_LO when y' < y_lo z',  Y_HI when y' > y_hi z'
    each product rounded once, each compare strict. With `clip` 0 (a batch known to be inside)
    no test is made, every code is 0 and every vertex gets its screen point.

    The screen point, with inv = FSRRA(z' z') (SH4ZAM's shz_invf_fsrra):
        sx = centre_x + (focal x') inv        the product focal x' rounded, then ONE fused
        sy = centre_y - (focal y') inv        multiply-add each (the SH-4's FMAC)
        sz = depth_base + depth_scale inv
    That is the arithmetic sh-elf-gcc makes of `cx + f * x * inv` with -ffp-contract=fast (the
    products left to right, the sum fused), so the routine gives the bits of the plain loop
    built that way (the tests compare them); a build without contraction rounds the sum
    separately and can differ from it in the last bit of a screen coordinate.

    Lazy: with `lazy` not 0, a vertex whose code byte is SHZ_DCE_PROJECT_SKIP (0x80) on entry
    is not touched: its record, its code and its word stay as they are. With `lazy` 0 every
    vertex is processed, whatever its code byte holds. The routine returns the number of
    vertices it processed.

    Order: load XMTRX first (shz_xmtrx_load_3x4, for instance); it is read, not changed.

    Alignment: source and view on a multiple of 4; words on a multiple of 4; out on a multiple
    of 32 for the SH-4 loop (on a multiple of 4 only the plain form runs, without the line
    allocation). The SH-4 loop makes 4-byte accesses only.

    Bit-identical to the plain loop described above: YES (tested on the SH-4 against the plain
    C form of this header, which fixes that arithmetic, and against a loop written the plain
    way and built with -ffast-math -ffp-contract=fast).
    Error bound: none added (FTRV and FSRRA are the plain loop's own).
    FPSCR: the SH-4 loop issues FSRRA before it knows the near bit, so a vertex at depth
    exactly 0 runs FSRRA on 0 with or without the clip tests (flag Z, quiet with the exception
    enables off, as KallistiOS has them); the plain loop does so only without them.

    Where the cycles go (E: estimates from the pipeline model, cache hits). A vertex with the
    clip tests is 27 loads, stores and register moves (one a cycle), 17 instructions of the
    FPU's execute stage (one a cycle, FTRV and FSRRA holding it 4 and 3) and 63 instructions
    in all, two a cycle at best; the four side compares share the one T bit, two cycles each,
    and a cycle that reads T beside the next compare has no room for a load or store. That
    makes 27 + 5 + the loop branch = 33 cycles, which the loop reaches. The compiled plain
    loop takes 80: it waits where the in-order pipeline makes it, for the transform, for each
    compare in turn, for the reciprocal root. The loop here loads the position before it knows
    whether the vertex is marked, fills the transform's latency with the carried words and
    the first slopes, issues the reciprocal root ahead of the compares, and multiplies x' and
    y' by the focal length in place once their compares are behind. One vertex at a time:
    with every load and store slot taken, two vertices in flight would gain nothing.
    Without the clip tests: 45 -> 26. Behind the near plane: 38 -> 24. A marked vertex: 11 -> 7.
    The input is prefetched 96 bytes ahead of each processed record, also past the end of the
    input (PREF raises no exception and reads nothing into the program).

    Version: v0.10
    The effect: nothing on screen. The same records in fewer SH-4 cycles.
    Use it for: world geometry and unskinned models that go through one matrix and are clipped
      and projected by the CPU.
*/

#ifndef SHZ_DCE_PROJECT_H
#define SHZ_DCE_PROJECT_H

#include "shz_dce_cdefs.h"
#include "shz_dce_vertex.h"   /* shz_dce_vertex_fmaf: one fused multiply-add on every back end */

#ifdef __cplusplus
extern "C" {
#endif

/** \name Clip code bits of shz_dce_project_vertices()
    @{ */
#define SHZ_DCE_PROJECT_NEAR 0x01u   /**< \brief z' < near_z: no other bit, no screen point. */
#define SHZ_DCE_PROJECT_X_LO 0x02u   /**< \brief x' < x_lo z'. */
#define SHZ_DCE_PROJECT_X_HI 0x04u   /**< \brief x' > x_hi z'. */
#define SHZ_DCE_PROJECT_Y_LO 0x08u   /**< \brief y' < y_lo z'. */
#define SHZ_DCE_PROJECT_Y_HI 0x10u   /**< \brief y' > y_hi z'. */
#define SHZ_DCE_PROJECT_SKIP 0x80u   /**< \brief On entry, with `lazy`: leave this vertex alone. Never written. */
/** @} */

/** \brief A view volume by four slopes and a near depth, and the projection behind it. 40 bytes
    on a multiple of 4. The order of the fields is the order the SH-4 loop reads them in. */
typedef struct shz_dce_view4 {
    float y_hi, y_lo;      /**< \brief Inside when y_lo z <= y <= y_hi z. */
    float x_hi, x_lo;      /**< \brief Inside when x_lo z <= x <= x_hi z. */
    float depth_base;      /**< \brief sz = depth_base + depth_scale / z. */
    float centre_y;        /**< \brief sy = centre_y - focal y / z. */
    float centre_x;        /**< \brief sx = centre_x + focal x / z. */
    float near_z;          /**< \brief A vertex with z < near_z gets the near code alone. */
    float focal;
    float depth_scale;
} shz_dce_view4_t;

typedef char shz_dce_view4_size_ok[(sizeof(shz_dce_view4_t) == 40u) ? 1 : -1];

/** \brief shz_dce_project_vertices() in plain C on SH4ZAM's routines: the software back end, the
    path for an output that is not on a multiple of 32, and the reference the SH-4 loops are
    tested against (the same FTRV, FSRRA and fused steps, so the same bits). */
SHZ_INLINE size_t shz_dce_project_vertices_c(float *SHZ_RESTRICT out, uint8_t *SHZ_RESTRICT codes, uint32_t *SHZ_RESTRICT words, const void *SHZ_RESTRICT in,
                                             size_t stride, size_t count, const shz_dce_view4_t *view, int clip, int lazy) SHZ_NOEXCEPT {
    const shz_dce_view4_t v = *view;
    const int alloc = SHZ_DCE_IS_ALIGNED(out, 32);
    size_t done = 0, i;

    for (i = 0; i < count; i++) {
        const float *s = (const float *)((uintptr_t)in + i * stride);
        const shz_alias_uint32_t *sw = (const shz_alias_uint32_t *)s;
        float *o = out + 8 * i;
        shz_alias_uint32_t *ow = (shz_alias_uint32_t *)o;
        shz_vec3_t p;
        unsigned code = 0u;

        if (lazy && codes[i] == SHZ_DCE_PROJECT_SKIP)
            continue;
        done++;
        {
            const uint32_t w3 = sw[3], w4 = sw[4], w5 = sw[5];
            if (alloc) shz_dcache_alloc_line(o);   /* the whole line is this record; its last 12 bytes stay undefined on the near path */
            p = shz_xmtrx_transform_point3(shz_vec3_init(s[0], s[1], s[2]));
            o[0] = p.x; o[1] = p.y; o[2] = p.z;
            ow[3] = w3; ow[4] = w4;
            words[i] = w5;
        }
        if (clip) {
            if (p.z < v.near_z)
                code = SHZ_DCE_PROJECT_NEAR;
            else
                code = ((unsigned)(p.x < v.x_lo * p.z) << 1) | ((unsigned)(p.x > v.x_hi * p.z) << 2) |
                       ((unsigned)(p.y < v.y_lo * p.z) << 3) | ((unsigned)(p.y > v.y_hi * p.z) << 4);
        }
        if (!(code & SHZ_DCE_PROJECT_NEAR)) {
            const float inv = shz_invf_fsrra(p.z);
            const float fx = shz_dce_opaquef(v.focal * p.x), fy = shz_dce_opaquef(v.focal * p.y);
            o[5] = shz_dce_vertex_fmaf(fx, inv, v.centre_x);
            o[6] = shz_dce_vertex_fmaf(-fy, inv, v.centre_y);
            o[7] = shz_dce_vertex_fmaf(v.depth_scale, inv, v.depth_base);
        }
        codes[i] = (uint8_t)code;
    }
    return done;
}

#if SHZ_DCE_SH4
/* One vertex at a time, ordered for the in-order pipeline: two instructions a cycle, of which one may be a load, store or
   register move (marked LS) and one an FPU instruction (FE). Written so that nothing waits: 33 cycles a vertex with the clip
   tests, 26 without, in the model.
   Registers. s: the source, three floats into the slot's record once its position is loaded; q: the slot's record + 12 (the
   carried word that MOVCA.L writes), the walk of the position's stores, then the next slot's record + 12; c: one past the slot's
   code byte; wp: the slot's word; k: the walk through the view's first floats, rewound when a vertex starts; dn: the vertices
   processed; ce: the end of the codes; sk: the mark (0x80 sign extended, or a value no code byte has); st12, st96: the stride
   less 12 and less 96. r0: the code byte, the carried words, the code; r1: the third word; r2: the walk of the screen point's
   stores. fr4..fr7 the vertex, fr0 z'^2 then its reciprocal root, fr12 near_z, fr13 focal, fr14 -focal, fr15 depth_scale.
   A marked vertex has its position loaded and thrown away (the loads sit where the pipeline waits for its code byte). */
#define SHZ_DCE_PJ_LOAD_(rewind) \
    "0:\n\t"                                     /* a marked vertex: its slot is stepped over */ \
    "add     %[st12], %[s]\n\t" \
    "cmp/hs  %[ce], %[c]\n\t" \
    "bt/s    9f\n\t" \
    "add     #32, %[q]\n" \
    "1:\n\t"                                     /* a slot */ \
    "mov.b   @%[c]+, r0\n\t"                     /* LS  the slot's code byte */ \
    "add     #4, %[wp]\n\t" \
    "fmov.s  @%[s]+, fr4\n\t"                    /* LS */ \
    "fmov.s  @%[s]+, fr5\n\t"                    /* LS */ \
    "cmp/eq  %[sk], r0\n\t" \
    "bt/s    0b\n\t" \
    "fmov.s  @%[s]+, fr6\n\t"                    /* LS */ \
    "fldi1   fr7\n\t"                            /* LS */ \
    "add     #" rewind ", %[k]\n\t" \
    "mov.l   @%[s], r0\n\t"                      /* LS  the first carried word */ \
    "ftrv    xmtrx, fv4\n\t"                     /* FE  x' y' after 5 cycles, z' after 6 */

/* With the clip tests. fr1 fr2 fr3 fr8 the four slope products, fr11 fr10 fr9 the three sums. The four side bits are collected
   by movt and three rotcl in the order y_hi, y_lo, x_hi, x_lo, then doubled: 16, 8, 4, 2. FSRRA is issued before the near
   branch is (its 3 cycles of the FPU are the ones the branch and the next loads need anyway). */
#define SHZ_DCE_PJ_CLIP_ \
    "bra     1f\n\t" \
    "nop\n" \
    "7:\n\t"                                     /* behind the near plane: the position and the two words are stored; the third word, the code 1 */ \
    "mov.l   r1, @%[wp]\n\t" \
    "mov     #1, r0\n\t" \
    "mov.b   r0, @-%[c]\n\t" \
    "add     #20, %[k]\n\t" \
    "add     #1, %[c]\n\t" \
    "cmp/hs  %[ce], %[c]\n\t" \
    "bf      1f\n\t" \
    "bra     9f\n\t" \
    "nop\n" \
    SHZ_DCE_PJ_LOAD_("-28") \
    "fmov.s  @%[k]+, fr1\n\t"                    /* LS  y_hi */ \
    "add     #1, %[dn]\n\t" \
    "movca.l r0, @%[q]\n\t"                      /* LS  the line, and its word 3 */ \
    "mov     %[q], r2\n\t" \
    "mov.l   @(4, %[s]), r0\n\t"                 /* LS  the second carried word */ \
    "add     #20, r2\n\t" \
    "mov.l   @(8, %[s]), r1\n\t"                 /* LS  the third word */ \
    "add     #84, %[s]\n\t" \
    "pref    @%[s]\n\t"                          /* LS  96 bytes ahead */ \
    "add     %[st96], %[s]\n\t" \
    "fmov    fr6, fr0\n\t"                       /* LS */ \
    "fmul    fr0, fr0\n\t"                       /* FE  z'^2 */ \
    "fmov.s  @%[k]+, fr2\n\t"                    /* LS  y_lo */ \
    "fcmp/gt fr6, fr12\n\t"                      /* FE  T = near_z > z' */ \
    "fmov.s  fr6, @-%[q]\n\t"                    /* LS */ \
    "fmul    fr6, fr1\n\t"                       /* FE  y_hi z' */ \
    "fmov.s  fr5, @-%[q]\n\t"                    /* LS */ \
    "fmul    fr6, fr2\n\t"                       /* FE  y_lo z' */ \
    "fmov.s  fr4, @-%[q]\n\t"                    /* LS */ \
    "fsrra   fr0\n\t"                            /* FE  3 cycles */ \
    "mov.l   r0, @(16, %[q])\n\t"                /* LS */ \
    "add     #44, %[q]\n\t" \
    "bt      7b\n\t" \
    "fmov.s  @%[k]+, fr3\n\t"                    /* LS  x_hi */ \
    "fmov.s  @%[k]+, fr8\n\t"                    /* LS  x_lo */ \
    "fcmp/gt fr1, fr5\n\t"                       /* FE  T = y' > y_hi z' */ \
    "mov.l   r1, @%[wp]\n\t"                     /* LS */ \
    "fmul    fr6, fr3\n\t"                       /* FE  x_hi z' */ \
    "movt    r0\n\t" \
    "fcmp/gt fr5, fr2\n\t"                       /* FE  T = y_lo z' > y' */ \
    "fmov.s  @%[k]+, fr11\n\t"                   /* LS  depth_base */ \
    "fmul    fr6, fr8\n\t"                       /* FE  x_lo z' */ \
    "rotcl   r0\n\t" \
    "fcmp/gt fr3, fr4\n\t"                       /* FE  T = x' > x_hi z' */ \
    "fmov.s  @%[k]+, fr10\n\t"                   /* LS  centre_y */ \
    "fmul    fr14, fr5\n\t"                      /* FE  y' := -focal y' (its compares are behind) */ \
    "rotcl   r0\n\t" \
    "fcmp/gt fr4, fr8\n\t"                       /* FE  T = x_lo z' > x' */ \
    "fmov.s  @%[k]+, fr9\n\t"                    /* LS  centre_x */ \
    "fmul    fr13, fr4\n\t"                      /* FE  x' := focal x' */ \
    "rotcl   r0\n\t" \
    "fmac    fr0, fr15, fr11\n\t"                /* FE  sz = depth_base + inv depth_scale */ \
    "add     r0, r0\n\t" \
    "fmac    fr0, fr5, fr10\n\t"                 /* FE  sy = centre_y - inv focal y' */ \
    "mov.b   r0, @-%[c]\n\t"                     /* LS */ \
    "fmac    fr0, fr4, fr9\n\t"                  /* FE  sx = centre_x + inv focal x' */ \
    "fmov.s  fr11, @-r2\n\t"                     /* LS */ \
    "add     #1, %[c]\n\t" \
    "fmov.s  fr10, @-r2\n\t"                     /* LS */ \
    "cmp/hs  %[ce], %[c]\n\t" \
    "bf/s    1b\n\t" \
    "fmov.s  fr9, @-r2\n"                        /* LS */ \
    "9:\n"

/* Without the clip tests. fr1 = focal x' and fr2 = -focal y' are made beside the transform's results (nothing else needs the
   FPU then), fr11 fr10 fr9 the three sums. */
#define SHZ_DCE_PJ_INSIDE_ \
    "bra     1f\n\t" \
    "nop\n" \
    SHZ_DCE_PJ_LOAD_("-12") \
    "fmov.s  @%[k]+, fr11\n\t"                   /* LS  depth_base */ \
    "add     #1, %[dn]\n\t" \
    "movca.l r0, @%[q]\n\t"                      /* LS */ \
    "mov     %[q], r2\n\t" \
    "mov.l   @(4, %[s]), r0\n\t"                 /* LS */ \
    "add     #20, r2\n\t" \
    "mov.l   @(8, %[s]), r1\n\t"                 /* LS */ \
    "add     #84, %[s]\n\t" \
    "fmov    fr13, fr1\n\t"                      /* LS */ \
    "fmul    fr4, fr1\n\t"                       /* FE  focal x' */ \
    "fmov    fr6, fr0\n\t"                       /* LS */ \
    "fmul    fr0, fr0\n\t"                       /* FE  z'^2 */ \
    "fmov    fr14, fr2\n\t"                      /* LS */ \
    "fmul    fr5, fr2\n\t"                       /* FE  -focal y' */ \
    "fmov.s  fr6, @-%[q]\n\t"                    /* LS */ \
    "fmov.s  fr5, @-%[q]\n\t"                    /* LS */ \
    "fsrra   fr0\n\t"                            /* FE */ \
    "fmov.s  fr4, @-%[q]\n\t"                    /* LS */ \
    "mov.l   r0, @(16, %[q])\n\t"                /* LS */ \
    "add     #44, %[q]\n\t" \
    "pref    @%[s]\n\t"                          /* LS */ \
    "add     %[st96], %[s]\n\t" \
    "fmov.s  @%[k]+, fr10\n\t"                   /* LS  centre_y */ \
    "mov     #0, r0\n\t" \
    "fmov.s  @%[k]+, fr9\n\t"                    /* LS  centre_x */ \
    "fmac    fr0, fr15, fr11\n\t"                /* FE */ \
    "mov.l   r1, @%[wp]\n\t"                     /* LS */ \
    "fmac    fr0, fr2, fr10\n\t"                 /* FE */ \
    "mov.b   r0, @-%[c]\n\t"                     /* LS */ \
    "fmac    fr0, fr1, fr9\n\t"                  /* FE */ \
    "fmov.s  fr11, @-r2\n\t"                     /* LS */ \
    "add     #1, %[c]\n\t" \
    "fmov.s  fr10, @-r2\n\t"                     /* LS */ \
    "cmp/hs  %[ce], %[c]\n\t" \
    "bf/s    1b\n\t" \
    "fmov.s  fr9, @-r2\n"                        /* LS */ \
    "9:\n"

/** \brief The SH-4 loop with the clip tests. out on a multiple of 32, count at least 1. Returns the vertices processed. */
SHZ_INLINE size_t shz_dce_project_vertices_clip_sh4(float *SHZ_RESTRICT out, uint8_t *SHZ_RESTRICT codes, uint32_t *SHZ_RESTRICT words, const void *SHZ_RESTRICT in,
                                                    size_t stride, size_t count, const shz_dce_view4_t *view, int lazy) SHZ_NOEXCEPT {
    const float *s = (const float *)in, *k = (const float *)(const void *)view + 7;   /* k: past the seven floats a vertex reads through it */
    float *q = out + 3;
    uint32_t *wp = (uint32_t *)((uintptr_t)words - 4u);
    const uint8_t *ce = codes + count;
    const size_t st12 = stride - 12u, st96 = stride - 96u;
    const int sk = lazy ? -128 : 256;
    size_t dn = 0;
    register float nz asm("fr12") = view->near_z;   /* what a vertex does not destroy stays in registers for the whole batch */
    register float fo asm("fr13") = view->focal;
    register float nf asm("fr14") = -view->focal;
    register float zs asm("fr15") = view->depth_scale;

    asm volatile(SHZ_DCE_PJ_CLIP_
        : [s] "+&r" (s), [q] "+&r" (q), [c] "+&r" (codes), [wp] "+&r" (wp), [k] "+&r" (k), [dn] "+&r" (dn)
        : [ce] "r" (ce), [sk] "r" (sk), [st12] "r" (st12), [st96] "r" (st96), "f" (nz), "f" (fo), "f" (nf), "f" (zs)
        : "r0", "r1", "r2", "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "t", "memory");
    return dn;
}

/** \brief The SH-4 loop without the clip tests (a batch known to be inside the view volume): every code 0, every vertex its
    screen point. Same contract as shz_dce_project_vertices_clip_sh4(). */
SHZ_INLINE size_t shz_dce_project_vertices_inside_sh4(float *SHZ_RESTRICT out, uint8_t *SHZ_RESTRICT codes, uint32_t *SHZ_RESTRICT words, const void *SHZ_RESTRICT in,
                                                      size_t stride, size_t count, const shz_dce_view4_t *view, int lazy) SHZ_NOEXCEPT {
    const float *s = (const float *)in, *k = (const float *)(const void *)view + 7;   /* k: past depth_base, centre_y, centre_x */
    float *q = out + 3;
    uint32_t *wp = (uint32_t *)((uintptr_t)words - 4u);
    const uint8_t *ce = codes + count;
    const size_t st12 = stride - 12u, st96 = stride - 96u;
    const int sk = lazy ? -128 : 256;
    size_t dn = 0;
    register float fo asm("fr13") = view->focal;
    register float nf asm("fr14") = -view->focal;
    register float zs asm("fr15") = view->depth_scale;

    asm volatile(SHZ_DCE_PJ_INSIDE_
        : [s] "+&r" (s), [q] "+&r" (q), [c] "+&r" (codes), [wp] "+&r" (wp), [k] "+&r" (k), [dn] "+&r" (dn)
        : [ce] "r" (ce), [sk] "r" (sk), [st12] "r" (st12), [st96] "r" (st96), "f" (fo), "f" (nf), "f" (zs)
        : "r0", "r1", "r2", "fr0", "fr1", "fr2", "fr4", "fr5", "fr6", "fr7", "fr9", "fr10", "fr11", "t", "memory");
    return dn;
}

#undef SHZ_DCE_PJ_LOAD_
#undef SHZ_DCE_PJ_CLIP_
#undef SHZ_DCE_PJ_INSIDE_
#endif /* SHZ_DCE_SH4 */

/** \brief Transform, clip-code and project a stream of vertices (see the file comment for the records and the arithmetic).
    out: count 32-byte records on a multiple of 32 (on a multiple of 4 only: the plain form, no line allocation). codes: count
    bytes; what they hold on entry matters only when `lazy` is not 0. words: count 32-bit words on a multiple of 4. in: records
    of `stride` bytes (24 or more, a multiple of 4) on a multiple of 4. view: on a multiple of 4. clip: not 0 the clip tests, 0
    none (codes 0). lazy: not 0 leaves a vertex whose code is SHZ_DCE_PROJECT_SKIP on entry untouched. in, out, codes and words
    must not overlap. XMTRX is read, not changed. Returns the number of vertices processed. Bit-identical to the plain loop: YES.
    Cycles E a vertex, the plain loop -> this: with the clip tests 80 -> 33 (86 at -O3), without 45 -> 26, behind the near
    plane 38 -> 24, a marked vertex 11 -> 7. The plain C form of this header (out off a multiple of 32) is no faster than the
    plain loop. */
SHZ_INLINE size_t shz_dce_project_vertices(float *SHZ_RESTRICT out, uint8_t *SHZ_RESTRICT codes, uint32_t *SHZ_RESTRICT words, const void *SHZ_RESTRICT in,
                                           size_t stride, size_t count, const shz_dce_view4_t *view, int clip, int lazy) SHZ_NOEXCEPT {
    if (!count)
        return 0;
    SHZ_DCE_ASSERT_ALIGNED(out, 4);
    SHZ_DCE_ASSERT_ALIGNED(words, 4);
    SHZ_DCE_ASSERT_ALIGNED(in, 4);
    SHZ_DCE_ASSERT_ALIGNED(view, 4);
    assert(stride >= 24u && !(stride & 3u));
#if SHZ_DCE_SH4
    if (SHZ_DCE_IS_ALIGNED(out, 32))
        return clip ? shz_dce_project_vertices_clip_sh4(out, codes, words, in, stride, count, view, lazy)
                    : shz_dce_project_vertices_inside_sh4(out, codes, words, in, stride, count, view, lazy);
#endif
    return shz_dce_project_vertices_c(out, codes, words, in, stride, count, view, clip, lazy);
}

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_PROJECT_H */
