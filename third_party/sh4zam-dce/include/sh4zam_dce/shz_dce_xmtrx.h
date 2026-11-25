/* SH4ZAM-DCE: arrays of vectors through XMTRX

   shz_dce_xmtrx.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
   NOT affiliated with, endorsed by or maintained by Falco Girgis or the SH4ZAM
   project. Report problems with this library to SH4DER-PVR, not to them.
*/

/** \file    shz_dce_xmtrx.h
    \brief   An array of vectors through the matrix in XMTRX, one FTRV a vector, with the next
             vectors already on their way while one finishes.

    SH4ZAM transforms one vector at a time (shz_xmtrx_transform_vec4, _point3, _vec3); it has no
    routine that takes an array (0.10.0). A loop of its calls is correct and slow: the SH-4 runs
    its instructions in order, so each vector waits out its own FTRV (7 cycles to the last
    result) and, for a perspective divide, its own FSRRA (5 more) before the next vector's first
    load. The four routines here are that loop with the waits filled: while one vector's results
    are on their way, the next one or two are loaded and started. Nothing of SH4ZAM's is
    replaced: off the SH-4 each routine IS the loop of SH4ZAM's call, and on the SH-4 it is
    tested against that loop byte for byte.

        shz_dce_xmtrx_transform_vec4_batch     (x, y, z, w)  ->  (X, Y, Z, W)
        shz_dce_xmtrx_transform_point3_batch   (x, y, z), w taken as 1  ->  (X, Y, Z)
        shz_dce_xmtrx_transform_vec3_batch     (x, y, z), w taken as 0  ->  (X, Y, Z)   (a direction)
        shz_dce_xmtrx_project_point3_batch     (x, y, z), w taken as 1  ->  (X inv, Y inv, inv)
    with (X, Y, Z, W) = XMTRX (x, y, z, w) and inv = FSRRA(W W) (SH4ZAM's shz_invf_fsrra): 1 / W
    for W > 0. The last one is a whole way to the screen when XMTRX holds view, projection and
    the screen's scale and centre: screen x, screen y and the 1 / W the PVR takes as depth.

    Arrays. Every routine takes a stride for each array, in bytes: the distance from one record
    to the next, a multiple of 4 and at least the record's size (12 or 16). Bytes between
    records are not read and not written. The output may be the input array itself with the
    same stride (in place: a record is loaded before any record at or after it is stored);
    any other overlap is not allowed.

    Order: load XMTRX first; it is read, not changed. Not every SH4ZAM routine leaves it alone:
    shz_sq_memcpy32_xmtrx(), shz_sq_memcpy32_1_xmtrx() and shz_memswap32_1_xmtrx() use the back
    bank as scratch, so a copy made with one of them between the load and a call here gives a
    wrong matrix. Load XMTRX again after such a copy (SH4ZAM ff39547).

    Alignment: every pointer and every stride on a multiple of 4. The SH-4 loops make 4-byte
    accesses only.

    The projecting routine does not handle W <= 0: FSRRA gives 1 / |W| (a mirrored point) and
    infinity at W = 0. Clip against the near plane first, or send only what is in front of it.

    Bit-identical to the plain loop (the C form of this header): YES on the SH-4. The same FTRV
    on the same four numbers, the same FSRRA of the same product, one multiply each for the two
    screen coordinates.
    Error bound: FTRV's ((largest product + abs result) x 2^-21 a component) and FSRRA's; none
    added.
    FPSCR: a vector at W = 0 runs FSRRA on 0 in the projecting routine (flag Z, quiet with the
    exception enables off, as KallistiOS has them).

    Prefetch. Each step asks for the cache line 64 bytes ahead in both arrays (PREF), so that a
    mesh that is not in the cache has its lines on their way before the loads and stores reach
    them. PREF raises no exception and reads nothing into the program, so asking past the end
    of an array is harmless, with one exception: an address in the store-queue area
    (0xe0000000 and up) makes PREF send a store queue. Neither array may be there.
    SHZ_DCE_XMTRX_PREFETCH (1 or 0, default 1, set before the include) takes the PREFs out: for
    arrays known to be in the cache, and to measure what they are worth.
    MEASURED on a console (ray bench h3, 2026-11-25, one console), the projecting routine in
    cycles a vector: with the prefetch 14.15 in the cache and 45.99 out of it; without 14.14 and
    48.14. The prefetch saves 4.5 % out of the cache and nothing in it; the default stays 1. Do
    not overwrite or replace in a SH4ZAM sync pass without re-measuring on a console.

    Where the cycles go (E: estimates from the pipeline model, cache hits; not a console's).
    One FPU instruction can start a cycle, and FTRV keeps the FPU for 4, FSRRA for 3 (the 3 is
    the model's assumption). The projecting routine is FTRV, a square, FSRRA and two products a
    vector: 10 cycles of the FPU at the least, against a chain of 18 from the first load to the
    last result. So it keeps three vectors in flight (in fv0, fv4 and fv8 in turn): a step
    starts one vector, squares W and takes the reciprocal root for the one before, and
    multiplies and stores the one before that. The plain transforms have no chain after FTRV
    and keep two in flight; their pace is their loads, stores and prefetches, one a cycle.

        cycles E a vector            the loop of SH4ZAM's call   here   here, no prefetch
        project_point3_batch         26.0 (30.0 with a divide)   11.0   11.0
        transform_point3 / _vec3     13.0                         9.5    7.5
        transform_vec4               15.0                        10.5    9.0
    (build/sh4zam-dce/cycles_xmtrx.txt and cycles_xmtrxnp.txt; the same at -O2 and -O3.)
    The model's 11.0 for the projection is contradicted by the console (below); the other
    three rows are model only.

    MEASURED on a console (ray bench h3, 2026-11-25, one console), cycles a vector, in the cache
    / out of it: shz_dce_xmtrx_project_point3_batch 14.15 / 45.99; KallistiOS's mat_transform
    13.38 / 42.86; the loop of SH4ZAM's calls 28.60 / 57.12. The batch is 2.0 times the loop's
    speed in the cache, but 5.8 % and 7.3 % slower than mat_transform. Demoted: for a hot path
    use mat_transform (its x, y, 1 / W are within 2 ulp of SH4ZAM's); keep this batch only
    where the bytes must equal SH4ZAM's own calls. transform_vec4 / _point3 / _vec3_batch:
    untimed on a console, not for hot paths. Do not overwrite or replace in a SH4ZAM sync pass
    without re-measuring on a console.

    Version: v0.10
    The effect: nothing on screen. The same numbers in fewer SH-4 cycles.
    Use it for: a mesh's positions to the screen or to view space, its normals through a
      rotation, any array of points or directions through one matrix.
*/

#ifndef SHZ_DCE_XMTRX_H
#define SHZ_DCE_XMTRX_H

#include "shz_dce_cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief 1: the SH-4 loops prefetch both arrays 64 bytes ahead. 0: no PREF at all. */
#ifndef SHZ_DCE_XMTRX_PREFETCH
#define SHZ_DCE_XMTRX_PREFETCH 1
#endif

/** rief shz_dce_xmtrx_transform_vec4_batch() in plain C: SH4ZAM's shz_xmtrx_transform_vec4() a vector. The software back
    end, and the reference the SH-4 loop is tested against (the same FTRV, so the same bits). */
SHZ_INLINE void shz_dce_xmtrx_transform_vec4_batch_c(void *out, size_t out_stride, const void *in, size_t in_stride, size_t count) SHZ_NOEXCEPT {
    const uint8_t *s = (const uint8_t *)in;
    uint8_t *o = (uint8_t *)out;
    size_t i;

    for (i = 0; i < count; i++, s += in_stride, o += out_stride) {
        const float *p = (const float *)(const void *)s;
        float *d = (float *)(void *)o;
        const shz_vec4_t r = shz_xmtrx_transform_vec4(shz_vec4_init(p[0], p[1], p[2], p[3]));

        d[0] = r.x; d[1] = r.y; d[2] = r.z; d[3] = r.w;
    }
}

/** \brief shz_dce_xmtrx_transform_point3_batch() in plain C: SH4ZAM's shz_xmtrx_transform_point3() a point. */
SHZ_INLINE void shz_dce_xmtrx_transform_point3_batch_c(void *out, size_t out_stride, const void *in, size_t in_stride, size_t count) SHZ_NOEXCEPT {
    const uint8_t *s = (const uint8_t *)in;
    uint8_t *o = (uint8_t *)out;
    size_t i;

    for (i = 0; i < count; i++, s += in_stride, o += out_stride) {
        const float *p = (const float *)(const void *)s;
        float *d = (float *)(void *)o;
        const shz_vec3_t r = shz_xmtrx_transform_point3(shz_vec3_init(p[0], p[1], p[2]));

        d[0] = r.x; d[1] = r.y; d[2] = r.z;
    }
}

/** \brief shz_dce_xmtrx_transform_vec3_batch() in plain C: SH4ZAM's shz_xmtrx_transform_vec3() a vector. */
SHZ_INLINE void shz_dce_xmtrx_transform_vec3_batch_c(void *out, size_t out_stride, const void *in, size_t in_stride, size_t count) SHZ_NOEXCEPT {
    const uint8_t *s = (const uint8_t *)in;
    uint8_t *o = (uint8_t *)out;
    size_t i;

    for (i = 0; i < count; i++, s += in_stride, o += out_stride) {
        const float *p = (const float *)(const void *)s;
        float *d = (float *)(void *)o;
        const shz_vec3_t r = shz_xmtrx_transform_vec3(shz_vec3_init(p[0], p[1], p[2]));

        d[0] = r.x; d[1] = r.y; d[2] = r.z;
    }
}

/** \brief shz_dce_xmtrx_project_point3_batch() in plain C on SH4ZAM's routines (the same FTRV, FSRRA and products as the SH-4
    loop, so the same bits). */
SHZ_INLINE void shz_dce_xmtrx_project_point3_batch_c(void *out, size_t out_stride, const void *in, size_t in_stride, size_t count) SHZ_NOEXCEPT {
    const uint8_t *s = (const uint8_t *)in;
    uint8_t *o = (uint8_t *)out;
    size_t i;

    for (i = 0; i < count; i++, s += in_stride, o += out_stride) {
        const float *p = (const float *)(const void *)s;
        float *d = (float *)(void *)o;
        const shz_vec4_t r = shz_xmtrx_transform_vec4(shz_vec4_init(p[0], p[1], p[2], 1.0f));
        const float inv = shz_invf_fsrra(r.w);

        d[0] = r.x * inv; d[1] = r.y * inv; d[2] = inv;
    }
}

#if SHZ_DCE_SH4
/* Two instructions issue a cycle, of which one may be a load, store or float register move (LS), one an FPU instruction (FE)
   and one an add (EX); a compare pairs with anything, a branch with anything but a branch.

   Registers of every loop.
   s: the source walk: post-increment loads, then `is` (the source stride less the bytes loaded). The loop ends when s
      reaches `se`.
   qa: the output walk. The stores run backwards through it (@-qa: the last number of a record first), so before a record's
      stores qa is stepped by `oq` (the output stride plus the record's bytes) from the start of the record before to the end
      of this one. It starts one stride before the array.
   With SHZ_DCE_XMTRX_PREFETCH: pi and qp, 64 bytes ahead of the source record being loaded and of the output record being
      stored, stepped by `ip` and `os` (the two strides). In the plain transforms they are stepped before they are used,
      so there they are one record further ahead. */
#if SHZ_DCE_XMTRX_PREFETCH
#define SHZ_DCE_XM_PREF_(reg) "pref    @%[" reg "]\n\t"
#define SHZ_DCE_XM_PADD_(reg, step) "add     %[" step "], %[" reg "]\n\t"
#define SHZ_DCE_XM_PREF_REGS_ const uint8_t *pi = (const uint8_t *)in + 64u, *qp = (const uint8_t *)out + 64u
#define SHZ_DCE_XM_PREF_OUT_ , [pi] "+&r" (pi), [qp] "+&r" (qp)
#define SHZ_DCE_XM_PREF_IN_ , [ip] "r" (in_stride), [os] "r" (out_stride)
#else
#define SHZ_DCE_XM_PREF_(reg) ""
#define SHZ_DCE_XM_PADD_(reg, step) ""
#define SHZ_DCE_XM_PREF_REGS_ (void)0
#define SHZ_DCE_XM_PREF_OUT_
#define SHZ_DCE_XM_PREF_IN_
#endif

#define SHZ_DCE_XM_REGS_(out_size) \
    const float *s = (const float *)in; \
    const float *const se = (const float *)(const void *)((const uint8_t *)in + count * in_stride); \
    float *qa = (float *)(void *)((uintptr_t)out - out_stride); \
    const size_t oq = out_stride + (out_size); \
    SHZ_DCE_XM_PREF_REGS_

#define SHZ_DCE_XM_OPERANDS_(in_size) \
    : [s] "+&r" (s), [qa] "+&r" (qa) SHZ_DCE_XM_PREF_OUT_ \
    : [se] "r" (se), [is] "r" (in_stride - (in_size)), [oq] "r" (oq) SHZ_DCE_XM_PREF_IN_

/* Start a vector: three loads, the fourth element (a load, fldi1 or fldi0), the transform; then the test for the last. */
#define SHZ_DCE_XM_BEGIN_(cx, cy, cz, fourth, cfv) \
    "fmov.s  @%[s]+, " cx "\n\t" \
    "fmov.s  @%[s]+, " cy "\n\t" \
    "fmov.s  @%[s]+, " cz "\n\t" \
    fourth "\n\t" \
    "ftrv    xmtrx, " cfv "\n\t" \
    "add     %[is], %[s]\n\t" \
    "cmp/eq  %[se], %[s]\n\t"

/* ---- the plain transforms: two vectors in flight ---- */

/* Store a finished vector, the last number first. `top`: the store of W for a vector of four, or nothing. */
#define SHZ_DCE_XM_STORE_(px, py, pz, top) \
    "add     %[oq], %[qa]\n\t" \
    top \
    "fmov.s  " pz ", @-%[qa]\n\t" \
    "fmov.s  " py ", @-%[qa]\n\t" \
    "fmov.s  " px ", @-%[qa]\n\t"

/* A step: start this vector (c) and store the one before (p), whose results have been ready since its own step. Without
   the prefetches a step of three numbers is 7 cycles in the model, one load or store a cycle; of four numbers 8 (the
   vector of four has its source step after its fourth load, one cycle later). The compare sets T for the branch that
   follows the step. */
#define SHZ_DCE_XM_STEP2_(cx, cy, cz, fourth, cfv, px, py, pz, top) \
    "fmov.s  @%[s]+, " cx "\n\t"                 /* LS */ \
    "add     %[oq], %[qa]\n\t"                   /* EX  to the end of the record before's place */ \
    "fmov.s  @%[s]+, " cy "\n\t"                 /* LS */ \
    SHZ_DCE_XM_PADD_("pi", "ip")                 /* EX */ \
    "fmov.s  @%[s]+, " cz "\n\t"                 /* LS */ \
    SHZ_DCE_XM_PADD_("qp", "os")                 /* EX */ \
    fourth "\n\t"                                /* LS */ \
    "add     %[is], %[s]\n\t"                    /* EX */ \
    top                                          /* LS  the vector before; a fourth load is still on its way */ \
    "fmov.s  " pz ", @-%[qa]\n\t"                /* LS */ \
    "cmp/eq  %[se], %[s]\n\t" \
    "ftrv    xmtrx, " cfv "\n\t"                 /* FE */ \
    "fmov.s  " py ", @-%[qa]\n\t"                /* LS */ \
    "fmov.s  " px ", @-%[qa]\n\t"                /* LS */ \
    SHZ_DCE_XM_PREF_("pi")                       /* LS */ \
    SHZ_DCE_XM_PREF_("qp")                       /* LS */

/* The whole of a plain transform: vectors in fv0 and fv4 in turn. f0, f4: the fourth element of a vector in fv0, in fv4.
   t0, t4: the store of its W, or nothing. */
#define SHZ_DCE_XM_LOOP2_(f0, f4, t0, t4) \
    SHZ_DCE_XM_BEGIN_("fr0", "fr1", "fr2", f0, "fv0") \
    "bt      8f\n" \
    "1:\n\t" \
    SHZ_DCE_XM_STEP2_("fr4", "fr5", "fr6", f4, "fv4", "fr0", "fr1", "fr2", t0) \
    "bt      7f\n\t" \
    SHZ_DCE_XM_STEP2_("fr0", "fr1", "fr2", f0, "fv0", "fr4", "fr5", "fr6", t4) \
    "bf      1b\n" \
    "8:\n\t" \
    SHZ_DCE_XM_STORE_("fr0", "fr1", "fr2", t0) \
    "bra     9f\n\t" \
    "nop\n" \
    "7:\n\t" \
    SHZ_DCE_XM_STORE_("fr4", "fr5", "fr6", t4) \
    "9:\n"

#define SHZ_DCE_XM_TOP_(pw) "fmov.s  " pw ", @-%[qa]\n\t"

/** \brief The SH-4 loop of shz_dce_xmtrx_transform_vec4_batch(). count at least 1. */
SHZ_INLINE void shz_dce_xmtrx_transform_vec4_batch_sh4(void *out, size_t out_stride, const void *in, size_t in_stride, size_t count) SHZ_NOEXCEPT {
    SHZ_DCE_XM_REGS_(16u);

    asm volatile(
        SHZ_DCE_XM_LOOP2_("fmov.s  @%[s]+, fr3", "fmov.s  @%[s]+, fr7", SHZ_DCE_XM_TOP_("fr3"), SHZ_DCE_XM_TOP_("fr7"))
        SHZ_DCE_XM_OPERANDS_(16u)
        : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "t", "memory");
}

/** \brief The SH-4 loop of shz_dce_xmtrx_transform_point3_batch(). count at least 1. */
SHZ_INLINE void shz_dce_xmtrx_transform_point3_batch_sh4(void *out, size_t out_stride, const void *in, size_t in_stride, size_t count) SHZ_NOEXCEPT {
    SHZ_DCE_XM_REGS_(12u);

    asm volatile(
        SHZ_DCE_XM_LOOP2_("fldi1   fr3", "fldi1   fr7", "", "")
        SHZ_DCE_XM_OPERANDS_(12u)
        : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "t", "memory");
}

/** \brief The SH-4 loop of shz_dce_xmtrx_transform_vec3_batch(). count at least 1. */
SHZ_INLINE void shz_dce_xmtrx_transform_vec3_batch_sh4(void *out, size_t out_stride, const void *in, size_t in_stride, size_t count) SHZ_NOEXCEPT {
    SHZ_DCE_XM_REGS_(12u);

    asm volatile(
        SHZ_DCE_XM_LOOP2_("fldi0   fr3", "fldi0   fr7", "", "")
        SHZ_DCE_XM_OPERANDS_(12u)
        : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "t", "memory");
}

/* ---- the projection: three vectors in flight ---- */

/* The middle of a vector's way: W W, then its reciprocal root, in place. */
#define SHZ_DCE_XM_MID_(pw) \
    "fmul    " pw ", " pw "\n\t" \
    "fsrra   " pw "\n\t"

/* The end of a vector's way: the two products and the three stores, 1 / W first. */
#define SHZ_DCE_XM_END_(qx, qy, qw) \
    "add     %[oq], %[qa]\n\t" \
    "fmul    " qw ", " qy "\n\t" \
    "fmul    " qw ", " qx "\n\t" \
    "fmov.s  " qw ", @-%[qa]\n\t" \
    "fmov.s  " qy ", @-%[qa]\n\t" \
    "fmov.s  " qx ", @-%[qa]\n\t"

/* A step, 11 cycles in the model. This vector (c): three loads, the constant 1, the transform. The vector before (p): W W at
   the step's start (its W has just arrived), the reciprocal root as soon as this vector's transform lets the FPU go. The
   vector two before (q): its reciprocal root arrives in the step's third cycle; the two products, then the three stores as
   each number arrives. Every FPU cycle of the step is taken: W W, the two products, 4 of FTRV, 3 of FSRRA, and the next
   step's W W. The loads, stores and prefetches (9) fit beside them. */
#define SHZ_DCE_XM_STEP3_(cx, cy, cz, cw, cfv, pw, qx, qy, qw) \
    "fmul    " pw ", " pw "\n\t"                 /* FE  the vector before: W W */ \
    "fmov.s  @%[s]+, " cx "\n\t"                 /* LS */ \
    "add     %[oq], %[qa]\n\t"                   /* EX */ \
    "fmov.s  @%[s]+, " cy "\n\t"                 /* LS */ \
    "fmul    " qw ", " qy "\n\t"                 /* FE  two before: Y / W */ \
    "fmov.s  @%[s]+, " cz "\n\t"                 /* LS */ \
    "fmul    " qw ", " qx "\n\t"                 /* FE  X / W */ \
    "fldi1   " cw "\n\t"                         /* LS */ \
    "ftrv    xmtrx, " cfv "\n\t"                 /* FE  4 cycles of the FPU */ \
    "fmov.s  " qw ", @-%[qa]\n\t"                /* LS  1 / W */ \
    "add     %[is], %[s]\n\t"                    /* EX */ \
    SHZ_DCE_XM_PREF_("pi")                       /* LS */ \
    SHZ_DCE_XM_PADD_("pi", "ip")                 /* EX */ \
    "fmov.s  " qy ", @-%[qa]\n\t"                /* LS */ \
    "cmp/eq  %[se], %[s]\n\t" \
    "fmov.s  " qx ", @-%[qa]\n\t"                /* LS */ \
    "fsrra   " pw "\n\t"                         /* FE  3 cycles of the FPU */ \
    SHZ_DCE_XM_PREF_("qp")                       /* LS */ \
    SHZ_DCE_XM_PADD_("qp", "os")                 /* EX */

/** \brief The SH-4 loop of shz_dce_xmtrx_project_point3_batch(). count at least 1. Vectors in fv0, fv4, fv8 in turn. */
SHZ_INLINE void shz_dce_xmtrx_project_point3_batch_sh4(void *out, size_t out_stride, const void *in, size_t in_stride, size_t count) SHZ_NOEXCEPT {
    SHZ_DCE_XM_REGS_(12u);

    asm volatile(
        SHZ_DCE_XM_BEGIN_("fr0", "fr1", "fr2", "fldi1   fr3", "fv0")              /* vector 0 */
        "bt      5f\n\t"
        SHZ_DCE_XM_MID_("fr3")
        SHZ_DCE_XM_BEGIN_("fr4", "fr5", "fr6", "fldi1   fr7", "fv4")              /* vector 1 */
        "bt      6f\n"
        "1:\n\t"
        SHZ_DCE_XM_STEP3_("fr8", "fr9", "fr10", "fr11", "fv8", "fr7", "fr0", "fr1", "fr3")
        "bt      7f\n\t"
        SHZ_DCE_XM_STEP3_("fr0", "fr1", "fr2", "fr3", "fv0", "fr11", "fr4", "fr5", "fr7")
        "bt      8f\n\t"
        SHZ_DCE_XM_STEP3_("fr4", "fr5", "fr6", "fr7", "fv4", "fr3", "fr8", "fr9", "fr11")
        "bf      1b\n"
        "6:\n\t"                                                                  /* the last began in fv4, the one before in fv0 */
        SHZ_DCE_XM_END_("fr0", "fr1", "fr3")
        SHZ_DCE_XM_MID_("fr7")
        SHZ_DCE_XM_END_("fr4", "fr5", "fr7")
        "bra     9f\n\t"
        "nop\n"
        "7:\n\t"                                                                  /* the last in fv8, the one before in fv4 */
        SHZ_DCE_XM_END_("fr4", "fr5", "fr7")
        SHZ_DCE_XM_MID_("fr11")
        SHZ_DCE_XM_END_("fr8", "fr9", "fr11")
        "bra     9f\n\t"
        "nop\n"
        "8:\n\t"                                                                  /* the last in fv0, the one before in fv8 */
        SHZ_DCE_XM_END_("fr8", "fr9", "fr11")
        "5:\n\t"
        SHZ_DCE_XM_MID_("fr3")
        SHZ_DCE_XM_END_("fr0", "fr1", "fr3")
        "9:\n"
        SHZ_DCE_XM_OPERANDS_(12u)
        : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "t", "memory");
}

#undef SHZ_DCE_XM_PREF_
#undef SHZ_DCE_XM_PADD_
#undef SHZ_DCE_XM_PREF_REGS_
#undef SHZ_DCE_XM_PREF_OUT_
#undef SHZ_DCE_XM_PREF_IN_
#undef SHZ_DCE_XM_REGS_
#undef SHZ_DCE_XM_OPERANDS_
#undef SHZ_DCE_XM_BEGIN_
#undef SHZ_DCE_XM_STORE_
#undef SHZ_DCE_XM_STEP2_
#undef SHZ_DCE_XM_LOOP2_
#undef SHZ_DCE_XM_TOP_
#undef SHZ_DCE_XM_MID_
#undef SHZ_DCE_XM_END_
#undef SHZ_DCE_XM_STEP3_
#endif /* SHZ_DCE_SH4 */

/* What every routine here asks of its arguments: pointers and strides on multiples of 4, a stride that holds its record. */
#define SHZ_DCE_XM_ASSERT_(in_size, out_size) do { \
        SHZ_DCE_ASSERT_ALIGNED(out, 4); SHZ_DCE_ASSERT_ALIGNED(in, 4); \
        assert(!(in_stride & 3u) && !(out_stride & 3u) && in_stride >= (in_size) && out_stride >= (out_size)); \
    } while (0)

/** \brief out[i] = XMTRX in[i] for count vectors of four floats.
    in: count records of 4 floats (x, y, z, w) at in_stride bytes. out: count records of 4 floats (X, Y, Z, W) at out_stride
    bytes. Pointers and strides on multiples of 4, strides at least 16. out may be in with the same stride; no other overlap.
    XMTRX is read, not changed. Bit-identical to shz_xmtrx_transform_vec4() a vector: YES on the SH-4.
    Cycles E a vector: see the file comment's table. */
SHZ_INLINE void shz_dce_xmtrx_transform_vec4_batch(void *out, size_t out_stride, const void *in, size_t in_stride, size_t count) SHZ_NOEXCEPT {
    if (!count)
        return;
    SHZ_DCE_XM_ASSERT_(16u, 16u);
#if SHZ_DCE_SH4
    shz_dce_xmtrx_transform_vec4_batch_sh4(out, out_stride, in, in_stride, count);
#else
    shz_dce_xmtrx_transform_vec4_batch_c(out, out_stride, in, in_stride, count);
#endif
}

/** \brief out[i] = the first three rows of XMTRX (x, y, z, 1) for count points.
    in: count records of 3 floats at in_stride bytes. out: count records of 3 floats (X, Y, Z) at out_stride bytes; W is
    not kept. Pointers and strides on multiples of 4, strides at least 12. out may be in with the same stride; no other
    overlap. XMTRX is read, not changed. Bit-identical to shz_xmtrx_transform_point3() a point: YES on the SH-4. */
SHZ_INLINE void shz_dce_xmtrx_transform_point3_batch(void *out, size_t out_stride, const void *in, size_t in_stride, size_t count) SHZ_NOEXCEPT {
    if (!count)
        return;
    SHZ_DCE_XM_ASSERT_(12u, 12u);
#if SHZ_DCE_SH4
    shz_dce_xmtrx_transform_point3_batch_sh4(out, out_stride, in, in_stride, count);
#else
    shz_dce_xmtrx_transform_point3_batch_c(out, out_stride, in, in_stride, count);
#endif
}

/** \brief out[i] = the first three rows of XMTRX (x, y, z, 0) for count directions (normals through a rotation: the
    matrix's translation does not reach them).
    Arrays, alignment and overlap as shz_dce_xmtrx_transform_point3_batch(). Bit-identical to shz_xmtrx_transform_vec3() a
    vector: YES on the SH-4. */
SHZ_INLINE void shz_dce_xmtrx_transform_vec3_batch(void *out, size_t out_stride, const void *in, size_t in_stride, size_t count) SHZ_NOEXCEPT {
    if (!count)
        return;
    SHZ_DCE_XM_ASSERT_(12u, 12u);
#if SHZ_DCE_SH4
    shz_dce_xmtrx_transform_vec3_batch_sh4(out, out_stride, in, in_stride, count);
#else
    shz_dce_xmtrx_transform_vec3_batch_c(out, out_stride, in, in_stride, count);
#endif
}

/** \brief out[i] = (X inv, Y inv, inv) with (X, Y, Z, W) = XMTRX (x, y, z, 1) and inv = FSRRA(W W), for count points.
    in: count records of 3 floats at in_stride bytes. out: count records of 3 floats at out_stride bytes. Pointers and
    strides on multiples of 4, strides at least 12. out may be in with the same stride; no other overlap. XMTRX is read, not
    changed. Every point must have W > 0. Bit-identical to the plain loop (shz_xmtrx_transform_vec4, shz_invf_fsrra, one
    product each): YES on the SH-4.
    Demoted (MEASURED, ray bench h3, 2026-11-25, one console): 14.15 / 45.99 cycles a point in / out of the cache against
    mat_transform's 13.38 / 42.86. Use mat_transform on a hot path; this is the bytes-equal-to-SH4ZAM form. Do not
    overwrite or replace in a SH4ZAM sync pass without re-measuring on a console. */
SHZ_INLINE void shz_dce_xmtrx_project_point3_batch(void *out, size_t out_stride, const void *in, size_t in_stride, size_t count) SHZ_NOEXCEPT {
    if (!count)
        return;
    SHZ_DCE_XM_ASSERT_(12u, 12u);
#if SHZ_DCE_SH4
    shz_dce_xmtrx_project_point3_batch_sh4(out, out_stride, in, in_stride, count);
#else
    shz_dce_xmtrx_project_point3_batch_c(out, out_stride, in, in_stride, count);
#endif
}

#undef SHZ_DCE_XM_ASSERT_

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_XMTRX_H */
