/* SH4ZAM-DCE: quaternion arrays on the SH-4's vector unit

   shz_dce_quat.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

/** \file    shz_dce_quat.h
    \brief   Quaternion arrays: a normalised lerp, a Hamilton product and a normalise, each over a
             whole array, with the SH-4's inner product (FIPR) and reciprocal square root (FSRRA).

    The idea. SH4ZAM's quaternion routines take one quaternion at a time, by value, so a loop over
    them copies every quaternion through the stack and waits for each result before the next starts.
    The routines here take arrays and read the floats where they sit, and each asm loop runs several
    elements at once: the elements' loads, inner products and stores are interleaved in one fixed
    order, so the vector unit's latencies are hidden behind the next element's work.

    A quaternion is four floats in memory, w, x, y, z (shz_quat_t). Every batch takes arrays of
    them: count elements, no stride.

    Order and state: no call needs another. None loads XMTRX and none changes FPSCR: a caller's
    matrix survives every call. Each asm block lists every register it writes (fr0 to fr15, r0, T).

    Alignment: every array pointer is a multiple of 4 (the type's own alignment). A pointer that
    is not a multiple of 4 is an address error on the console; the software back end does not
    care, the debug build asserts. An output array must not overlap an input array (SHZ_RESTRICT).

    Error bounds, and "not bit-identical": every routine here is APPROXIMATE on the SH-4, because
    FIPR and FSRRA are (see shz_dce_cdefs.h). None of them is bit-identical to the SH4ZAM routine
    it reworks. Code that must reproduce a result bit for bit keeps the SH4ZAM routines. The bound
    of each routine is in its \brief.

    The software back end (any target but the SH-4) runs the same arithmetic in plain C with
    SH4ZAM's 1 / sqrt, inside the same bound, and never uses FIPR.

    Input range (nlerp and normalise). The range tested is lengths from 1e-15 to 1e15, and both routines are inside
    their bounds there on every path. Below it the console matters: it runs with FPSCR.DN = 1, so a product or a sum
    below 2^-126 is a zero. A component c of a length L (ratio r = c / L) whose square falls below that range is lost
    from the squared length, which is short by about r^2 of its value. A plain C form on the console (FMUL and FMAC,
    no FIPR) is therefore outside the bound at small lengths: measured with the normalise on the SH-4 interpreter
    (direction (1, 0.01, 0.01, 0.01)), its error is about 1e-4 at length 1e-17 (both flag sets), and SH4ZAM's own
    routine is the same. The loop's squared length is a FIPR, which the bit-exact model keeps whole: 6e-8 off at
    1e-18 (measured, not promised: the console's FIPR is not checked here). Above 1e15 nothing overflows, but nothing
    is measured there.

    The loops (the order of the asm is a schedule, produced by a search over the in-order pipeline
    model of the interpreter, see tests/sh4run): each routine's loop body is printed in the order
    that model found fastest, one element's ops on each line with a comment naming the element and
    the value. The order keeps every write-after-read and write-after-write landing rule the model
    can see (a write never lands before an earlier read or an earlier write of the same register).

    Where it pays, and where it does not (cycles E, from the interpreter's pipeline model: an
    ESTIMATE, see shz_dce_cdefs.h; the figures are with the routines, per element, at -O3).

    Tried and not kept: one-quaternion drop-ins (shz_dce_quat_nlerp, shz_dce_quat_mult, shz_dce_quat_normalize: the batch
    of one element, the same signatures as SH4ZAM's routines). A loop over them costs more than SH4ZAM's own loop at -O3:
    nlerp 88 E against 71.44 E, mult 81 E against 55.00 E, normalise 53 E against 31.00 E (cycles E per element, the
    interpreter's model; rows _bench_nlerp_one, _bench_mult_one, _bench_normalize_one at -O3; at -O2 they are 115, 92
    and 70 E). The batch wins; the single call pays a whole element's latency with nothing to overlap it.

    Version: v0.10
    The effect: nothing on screen by itself. Fewer SH-4 cycles for the quaternion work of an
      animation or a camera, per element of an array.
    Use it for: a pose blend over many bones, a set of joint orientations composed in one pass,
      re-normalising a whole array after integration.

*/

#ifndef SHZ_DCE_QUAT_H
#define SHZ_DCE_QUAT_H

#include "shz_dce_cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------------------------- */
/* Normalised lerp.                                                                               */
/* ---------------------------------------------------------------------------------------------- */

/** \brief Normalised lerp of two arrays: out[i] is the normalised blend of a[i] towards b[i] by t
    along the shorter way (b[i] is negated when a[i] . b[i] < 0), as SH4ZAM's shz_quat_nlerp() does.
    Plain C form: the software back end and the reference of the SH-4 loop.

    Inputs: a and b hold unit quaternions, or both hold quaternions of one common length L between 1e-15 and 1e15
    (see the file comment), and t in [0, 1]. Overwrites nothing (no XMTRX, no FPSCR). Alignment: out, a, b on
    multiples of 4; out must not overlap a or b. The squared length of the result is at least L^2 / 2: with the sign
    rule the cross term 2 t (1 - t) (a . b) is not negative (up to the sign band), so FSRRA never sees a zero.
    Error bound (absolute, per component, for the inputs above): 2^-19 against the exact nlerp computed in
    double. Measured: 1.4e-7 on the software back end (1.8e-7 with -ffast-math), 1.4e-7 on the SH-4
    interpreter with its bit-exact FIPR and FSRRA. Bit-identical to shz_quat_nlerp(): NO. The sign is decided by an
    approximate dot product: where |a . b| is within about 2^-20 of zero, either sign is a shortest
    way and the result may take the other one. */
SHZ_INLINE void shz_dce_quat_nlerp_batch_c(shz_quat_t *SHZ_RESTRICT out, const shz_quat_t *SHZ_RESTRICT a,
                                           const shz_quat_t *SHZ_RESTRICT b, float t, size_t count) SHZ_NOEXCEPT {
    const float wa = 1.0f - t;
    size_t i;

    for (i = 0; i < count; i++) {
        const float *p = a[i].e;
        const float *q = b[i].e;
        const float wb = ((p[0] * q[0] + p[1] * q[1]) + (p[2] * q[2] + p[3] * q[3]) < 0.0f) ? -t : t;
        const float w = wa * p[0] + wb * q[0];
        const float x = wa * p[1] + wb * q[1];
        const float y = wa * p[2] + wb * q[2];
        const float z = wa * p[3] + wb * q[3];
        const float r = shz_inv_sqrtf_fsrra((w * w + x * x) + (y * y + z * z));

        out[i] = shz_quat_init(w * r, x * r, y * r, z * r);
    }
}

#if SHZ_DCE_SH4
/** \brief shz_dce_quat_nlerp_batch() on the SH-4: three elements an iteration (a loop body of three interleaved
    elements, which use two register sets in turn: set A (a in fv4, copy in fr3) and set B (a in fv12, copy in fr3);
    both take b in fv8), then the remaining elements one at a time. Per element: the two quaternions load into
    fv4 (or fv12) and fv8;
    FIPR gives a . b in the last register of b; FCMP and a table of +t / -t (indexed by T) give the
    weight of b; the blend runs in place in a; FIPR gives |q|^2; FSRRA; the four scaled results store
    backwards (pre-decrement). Uses fr0 to fr15 and r0; XMTRX and FPSCR untouched.
    Cycles E per element, from the model: 28.20 (SH4ZAM's loop: 71.44). */
SHZ_INLINE void shz_dce_quat_nlerp_batch_sh4(shz_quat_t *SHZ_RESTRICT out, const shz_quat_t *SHZ_RESTRICT a,
                                             const shz_quat_t *SHZ_RESTRICT b, float t, size_t count) SHZ_NOEXCEPT {
    float k[3] = { 1.0f - t, t, -t };       /* k[0]: 1 - t; k[1]: +t; k[2]: -t. The index 4 * T from k + 1 picks the sign. */
    const char *pk = (const char *)(k + 1);
    const char *pa = (const char *)a;
    const char *pb = (const char *)b;
    char *po = (char *)out + 16;             /* stores run backwards from the end of each element */
    size_t groups = count / 3u;
    size_t rem = count % 3u;

    if (!count)
        return;

    asm volatile(R"(
        mov     #-4, r0
        fmov.s  @(r0, %[pk]), fr1          ! wa = 1 - t
        fldi0   fr2                        ! 0.0, for the compare
        tst     %[groups], %[groups]
        bt      3f
    1:
        fmov.s  @%[pa]+, fr4           ! e0 ld a.w
        fmov.s  @%[pb]+, fr8           ! e0 ld b.w
        fmov.s  @%[pa]+, fr5           ! e0 ld a.x
        fmov.s  @%[pa]+, fr6           ! e0 ld a.y
        fmov.s  @%[pa]+, fr7           ! e0 ld a.z
        fmov.s  @%[pb]+, fr9           ! e0 ld b.x
        fmov.s  @%[pb]+, fr10           ! e0 ld b.y
        fmov.s  @%[pb]+, fr11           ! e0 ld b.z
        fmov.s  @%[pa]+, fr12           ! e1 ld a.w
        fipr    fv4, fv8             ! e0 d = a.b (in b.z)
        fmov.s  @%[pa]+, fr13           ! e1 ld a.x
        fmul    fr1, fr4            ! e0 wa * a.w
        fmul    fr1, fr5            ! e0 wa * a.x
        fmov.s  @%[pa]+, fr14           ! e1 ld a.y
        fmov.s  @%[pa]+, fr15           ! e1 ld a.z
        fcmp/gt fr11, fr2            ! e0 T = d < 0
        fmul    fr1, fr6            ! e0 wa * a.y
        movt    r0                     ! e0 0 or 1
        shll2   r0                     ! e0 0 or 4
        fmov.s  @(r0, %[pk]), fr0         ! e0 wb = +t or -t
        fmul    fr1, fr7            ! e0 wa * a.z
        mov     #-4, r0                ! e0 address of b.z
        fmov.s  @(r0, %[pb]), fr11      ! e0 reload b.z
        fmac    fr0, fr8, fr4    ! e0 q.w
        fmov.s  @%[pb]+, fr8           ! e1 ld b.w
        fmac    fr0, fr9, fr5    ! e0 q.x
        fmov.s  @%[pb]+, fr9           ! e1 ld b.x
        fmac    fr0, fr10, fr6    ! e0 q.y
        fmov.s  @%[pb]+, fr10           ! e1 ld b.y
        fmac    fr0, fr11, fr7    ! e0 q.z
        fmov.s  @%[pb]+, fr11           ! e1 ld b.z
        fipr    fv12, fv8             ! e1 d = a.b (in b.z)
        fmov    fr7, fr3            ! e0 copy q.z
        fipr    fv4, fv4           ! e0 |q|^2
        fmul    fr1, fr14            ! e1 wa * a.y
        fmul    fr1, fr12            ! e1 wa * a.w
        fcmp/gt fr11, fr2            ! e1 T = d < 0
        fsrra   fr7                 ! e0 1 / |q|
        fmul    fr1, fr15            ! e1 wa * a.z
        movt    r0                     ! e1 0 or 1
        fmul    fr1, fr13            ! e1 wa * a.x
        shll2   r0                     ! e1 0 or 4
        fmul    fr7, fr3            ! e0 scale z
        fmov.s  @(r0, %[pk]), fr0         ! e1 wb = +t or -t
        fmul    fr7, fr6         ! e0 scale y
        mov     #-4, r0                ! e1 address of b.z
        fmul    fr7, fr5         ! e0 scale x
        fmov.s  @(r0, %[pb]), fr11      ! e1 reload b.z
        fmul    fr7, fr4         ! e0 scale w
        fmov.s  fr3, @-%[po]              ! e0 st z
        fmac    fr0, fr9, fr13    ! e1 q.x
        fmov.s  fr6, @-%[po]           ! e0 st y
        fmov.s  fr5, @-%[po]           ! e0 st x
        fmac    fr0, fr11, fr15    ! e1 q.z
        fmac    fr0, fr8, fr12    ! e1 q.w
        fmov.s  fr4, @-%[po]           ! e0 st w
        fmac    fr0, fr10, fr14    ! e1 q.y
        fmov.s  @%[pb]+, fr8           ! e2 ld b.w
        fmov    fr15, fr3            ! e1 copy q.z
        fmov.s  @%[pb]+, fr9           ! e2 ld b.x
        fmov.s  @%[pa]+, fr4           ! e2 ld a.w
        fipr    fv12, fv12           ! e1 |q|^2
        fmov.s  @%[pa]+, fr5           ! e2 ld a.x
        fmov.s  @%[pb]+, fr10           ! e2 ld b.y
        fmov.s  @%[pa]+, fr6           ! e2 ld a.y
        fmov.s  @%[pb]+, fr11           ! e2 ld b.z
        fsrra   fr15                 ! e1 1 / |q|
        fmov.s  @%[pa]+, fr7           ! e2 ld a.z
        fipr    fv4, fv8             ! e2 d = a.b (in b.z)
        fmul    fr1, fr4            ! e2 wa * a.w
        fmul    fr15, fr3            ! e1 scale z
        fmul    fr1, fr7            ! e2 wa * a.z
        fcmp/gt fr11, fr2            ! e2 T = d < 0
        fmul    fr1, fr6            ! e2 wa * a.y
        movt    r0                     ! e2 0 or 1
        fmul    fr15, fr13         ! e1 scale x
        shll2   r0                     ! e2 0 or 4
        fmul    fr15, fr14         ! e1 scale y
        fmul    fr1, fr5            ! e2 wa * a.x
        fmov.s  @(r0, %[pk]), fr0         ! e2 wb = +t or -t
        mov     #-4, r0                ! e2 address of b.z
        fmov.s  @(r0, %[pb]), fr11      ! e2 reload b.z
        fmac    fr0, fr10, fr6    ! e2 q.y
        fmac    fr0, fr9, fr5    ! e2 q.x
        add     #32, %[po]                ! e0 next out element
        fmac    fr0, fr11, fr7    ! e2 q.z
        fmov.s  fr3, @-%[po]              ! e1 st z
        fmac    fr0, fr8, fr4    ! e2 q.w
        fmov    fr7, fr3            ! e2 copy q.z
        fmul    fr15, fr12         ! e1 scale w
        fmov.s  fr14, @-%[po]           ! e1 st y
        fipr    fv4, fv4           ! e2 |q|^2
        fsrra   fr7                 ! e2 1 / |q|
        fmov.s  fr13, @-%[po]           ! e1 st x
        fmul    fr7, fr3            ! e2 scale z
        fmul    fr7, fr6         ! e2 scale y
        fmov.s  fr12, @-%[po]           ! e1 st w
        add     #32, %[po]                ! e1 next out element
        fmul    fr7, fr5         ! e2 scale x
        fmov.s  fr3, @-%[po]              ! e2 st z
        fmul    fr7, fr4         ! e2 scale w
        fmov.s  fr6, @-%[po]           ! e2 st y
        fmov.s  fr5, @-%[po]           ! e2 st x
        dt      %[groups]
        fmov.s  fr4, @-%[po]           ! e2 st w
        add     #32, %[po]                ! e2 next out element
        bf/s    1b
        nop
    3:
        tst     %[rem], %[rem]
        bt      4f
    2:
        fmov.s  @%[pa]+, fr4           ! e0 ld a.w
        fmov.s  @%[pa]+, fr5           ! e0 ld a.x
        fmov.s  @%[pa]+, fr6           ! e0 ld a.y
        fmov.s  @%[pa]+, fr7           ! e0 ld a.z
        fmov.s  @%[pb]+, fr8           ! e0 ld b.w
        fmov.s  @%[pb]+, fr9           ! e0 ld b.x
        fmov.s  @%[pb]+, fr10           ! e0 ld b.y
        fmov.s  @%[pb]+, fr11           ! e0 ld b.z
        fipr    fv4, fv8             ! e0 d = a.b (in b.z)
        fmul    fr1, fr4            ! e0 wa * a.w
        fmul    fr1, fr5            ! e0 wa * a.x
        fmul    fr1, fr6            ! e0 wa * a.y
        fcmp/gt fr11, fr2            ! e0 T = d < 0
        fmul    fr1, fr7            ! e0 wa * a.z (the gap: T is ready two cycles after the compare)
        movt    r0                     ! e0 0 or 1
        shll2   r0                     ! e0 0 or 4
        fmov.s  @(r0, %[pk]), fr0         ! e0 wb = +t or -t
        mov     #-4, r0                ! e0 address of b.z
        fmov.s  @(r0, %[pb]), fr11      ! e0 reload b.z
        fmac    fr0, fr8, fr4    ! e0 q.w
        fmac    fr0, fr9, fr5    ! e0 q.x
        fmac    fr0, fr10, fr6    ! e0 q.y
        fmac    fr0, fr11, fr7    ! e0 q.z
        fmov    fr7, fr3            ! e0 copy q.z
        fipr    fv4, fv4           ! e0 |q|^2
        fsrra   fr7                 ! e0 1 / |q|
        fmul    fr7, fr4         ! e0 scale w
        fmul    fr7, fr5         ! e0 scale x
        fmul    fr7, fr6         ! e0 scale y
        fmul    fr7, fr3            ! e0 scale z
        fmov.s  fr3, @-%[po]              ! e0 st z
        fmov.s  fr6, @-%[po]           ! e0 st y
        fmov.s  fr5, @-%[po]           ! e0 st x
        fmov.s  fr4, @-%[po]           ! e0 st w
        add     #32, %[po]                ! e0 next out element
        dt      %[rem]
        bf/s    2b
        nop
    4:
    )"
    : [pa] "+r" (pa), [pb] "+r" (pb), [po] "+r" (po), [groups] "+r" (groups), [rem] "+r" (rem)
    : [pk] "r" (pk)
    : "r0", "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15",
      "t", "memory");
}
#endif

/** \brief Normalised lerp of two arrays, out[i] = nlerp(a[i], b[i], t): see shz_dce_quat_nlerp_batch_c() for the contract,
    the bound and the sign rule. On the SH-4 this is the asm loop; elsewhere the plain C form. count 0 returns at once.
    Alignment: out, a, b on multiples of 4. */
SHZ_INLINE void shz_dce_quat_nlerp_batch(shz_quat_t *SHZ_RESTRICT out, const shz_quat_t *SHZ_RESTRICT a,
                                         const shz_quat_t *SHZ_RESTRICT b, float t, size_t count) SHZ_NOEXCEPT {
    SHZ_DCE_ASSERT_ALIGNED(out, 4);
    SHZ_DCE_ASSERT_ALIGNED(a, 4);
    SHZ_DCE_ASSERT_ALIGNED(b, 4);
    if (!count)
        return;
#if SHZ_DCE_SH4
    shz_dce_quat_nlerp_batch_sh4(out, a, b, t, count);
#else
    shz_dce_quat_nlerp_batch_c(out, a, b, t, count);
#endif
}

/* ---------------------------------------------------------------------------------------------- */
/* Hamilton product.                                                                              */
/* ---------------------------------------------------------------------------------------------- */

/** \brief Hamilton product of two arrays, out[i] = a[i] * b[i] in SH4ZAM's convention (shz_quat_mult(a, b)).
    Plain C form: the software back end and the reference of the SH-4 loop.

    Inputs: any quaternions whose products stay above about 1e-30. The console flushes denormals to zero
    (FPSCR.DN), which costs at most 4 x 2^-126 per component: inside the bound once the largest product P is above
    about 5e-32. Overwrites nothing. Alignment: out, a, b on multiples of 4; out must not overlap a or b.
    Error bound (absolute, per component): (P + |R|) x 2^-20, P the largest of the four products that make
    one component and R that component, against the product computed in double. Measured: 1.6e-7 x (P + |R|)
    on the software back end, 1.2e-7 to 1.7e-7 x (P + |R|) on the SH-4 interpreter. Bit-identical to shz_quat_mult():
    on the software back end, yes for the same expression under strict IEEE flags (the host test asserts it; the order of the sums is SH4ZAM's;
    with -ffast-math the compiler may contract the products differently); on the SH-4, NOT promised (an inner
    product rounds once, see shz_dce_cdefs.h). Measured: the same bits as SH4ZAM's routine in
    every element the check tested (5818 of 5818 per flag set, with the interpreter's bit-exact FIPR model); a console
    may differ in the last bit. */
SHZ_INLINE void shz_dce_quat_mult_batch_c(shz_quat_t *SHZ_RESTRICT out, const shz_quat_t *SHZ_RESTRICT a,
                                          const shz_quat_t *SHZ_RESTRICT b, size_t count) SHZ_NOEXCEPT {
    size_t i;

    for (i = 0; i < count; i++) {
        const float *p = a[i].e;
        const float *q = b[i].e;

        out[i] = shz_quat_init(p[0] * q[0] - p[1] * q[1] - p[2] * q[2] - p[3] * q[3],
                               p[0] * q[1] + p[1] * q[0] + p[2] * q[3] - p[3] * q[2],
                               p[0] * q[2] - p[1] * q[3] + p[2] * q[0] + p[3] * q[1],
                               p[0] * q[3] + p[1] * q[2] - p[2] * q[1] + p[3] * q[0]);
    }
}

#if SHZ_DCE_SH4
/** \brief shz_dce_quat_mult_batch() on the SH-4: one element an iteration, software-pipelined: the loop body finishes
    the previous element (its stores and last two products) and starts the next (its loads and first two products),
    so the end of the body does not wait for a result. A prologue starts the first element; an epilogue finishes the
    last. The first quaternion
    a[i] sits in fv4 (its four components, the multiplicand of all four inner products). Each result component is
    one FIPR against a signed, reordered copy of b[i] built in fv0 or fv8, loaded straight from memory (twenty
    loads per element: four of a, sixteen of b; the signs are multiplications by -1 kept in fr12). The two operand groups are reused, and
    each group's result is stored before the group is loaded again. Uses fr0 to fr12 and no r0; XMTRX and FPSCR
    untouched.
    Cycles E per element, from the model: 27.00 (SH4ZAM's loop: 55.00). */
SHZ_INLINE void shz_dce_quat_mult_batch_sh4(shz_quat_t *SHZ_RESTRICT out, const shz_quat_t *SHZ_RESTRICT a,
                                            const shz_quat_t *SHZ_RESTRICT b, size_t count) SHZ_NOEXCEPT {
    const char *pa = (const char *)a;
    const char *p0 = (const char *)b;          /* w of b[i] */
    const char *p1 = p0 + 4;                   /* x */
    const char *p2 = p0 + 8;                   /* y */
    const char *p3 = p0 + 12;                  /* z */
    char *po = (char *)out + 16;               /* stores run backwards from the end of each element */
    size_t n = count ? count - 1u : 0u;        /* loop turns: the first element's first half is in the prologue */

    if (!count)
        return;
    asm volatile(R"(
        fldi1   fr12
        fneg    fr12                           ! -1.0
        fmov.s  @%[pa]+, fr4      ! q1.w
        fmov.s  @%[pa]+, fr5      ! q1.x
        fmov.s  @%[pa]+, fr6      ! q1.y
        fmov.s  @%[pa]+, fr7      ! q1.z
        fmov.s  @%[p3], fr0       ! O_z: z2
        fmov.s  @%[p2], fr1       ! O_z: y2
        fmov.s  @%[p1], fr2       ! O_z: x2
        fmul    fr12, fr2      ! O_z: -x2
        fmov.s  @%[p0], fr3       ! O_z: w2
        fipr    fv4, fv0       ! out z
        fmov.s  @%[p2], fr8       ! O_y: y2
        fmov.s  @%[p3], fr9       ! O_y: z2
        fmul    fr12, fr9      ! O_y: -z2
        fmov.s  @%[p0], fr10      ! O_y: w2
        fmov.s  @%[p1], fr11      ! O_y: x2
        fipr    fv4, fv8       ! out y
        tst     %[n], %[n]
        bt      3f
    1:
        fmov.s  @%[p1], fr9       ! O_w: x2
        fmov.s  @%[p2], fr10      ! O_w: y2
        fmul    fr12, fr9      ! O_w: -x2
        fmov.s  fr3, @-%[po]      ! st z
        fmov.s  @%[p2], fr3       ! O_x: y2
        add     #16, %[p2]
        fmov.s  fr11, @-%[po]     ! st y
        fmov.s  @%[p3], fr11      ! O_w: z2
        dt      %[n]
        fmov.s  @%[p0], fr1       ! O_x: w2
        fmul    fr12, fr10     ! O_w: -y2
        fmov.s  @%[p0], fr8       ! O_w: w2
        fmul    fr12, fr11     ! O_w: -z2
        fmul    fr12, fr3      ! O_x: -y2
        fmov.s  @%[p1], fr0       ! O_x: x2
        fmov.s  @%[p3], fr2       ! O_x: z2
        add     #16, %[p3]
        fipr    fv4, fv8       ! out w
        fmov.s  @%[p3], fr9       ! O_y: z2
        fipr    fv4, fv0       ! out x
        fmov.s  @%[pa]+, fr4      ! q1.w
        add     #16, %[p1]
        fmov.s  @%[p2], fr1       ! O_z: y2
        fmov.s  @%[pa]+, fr5      ! q1.x
        fmov.s  @%[pa]+, fr6      ! q1.y
        fmul    fr12, fr9      ! O_y: -z2
        fmov.s  @%[p1], fr2       ! O_z: x2
        add     #16, %[p0]        ! next b element
        fmov.s  fr3, @-%[po]      ! st x
        fmul    fr12, fr2      ! O_z: -x2
        fmov.s  fr11, @-%[po]     ! st w
        add     #32, %[po]        ! next out element
        fmov.s  @%[p2], fr8       ! O_y: y2
        fmov.s  @%[pa]+, fr7      ! q1.z
        fmov.s  @%[p3], fr0       ! O_z: z2
        fmov.s  @%[p0], fr10      ! O_y: w2
        fmov.s  @%[p0], fr3       ! O_z: w2
        fmov.s  @%[p1], fr11      ! O_y: x2
        fipr    fv4, fv0       ! out z
        fipr    fv4, fv8       ! out y
        bf/s    1b
        nop
    3:
        fmov.s  fr3, @-%[po]      ! st z
        fmov.s  @%[p1], fr0       ! O_x: x2
        fmov.s  @%[p0], fr1       ! O_x: w2
        fmov.s  @%[p3], fr2       ! O_x: z2
        fmov.s  @%[p2], fr3       ! O_x: y2
        fmul    fr12, fr3      ! O_x: -y2
        fipr    fv4, fv0       ! out x
        fmov.s  fr11, @-%[po]     ! st y
        fmov.s  @%[p0], fr8       ! O_w: w2
        fmov.s  @%[p1], fr9       ! O_w: x2
        fmul    fr12, fr9      ! O_w: -x2
        fmov.s  @%[p2], fr10      ! O_w: y2
        fmul    fr12, fr10     ! O_w: -y2
        fmov.s  @%[p3], fr11      ! O_w: z2
        fmul    fr12, fr11     ! O_w: -z2
        fipr    fv4, fv8       ! out w
        fmov.s  fr3, @-%[po]      ! st x
        fmov.s  fr11, @-%[po]     ! st w
        add     #16, %[p0]        ! next b element
        add     #16, %[p1]
        add     #16, %[p2]
        add     #16, %[p3]
        add     #32, %[po]        ! next out element
    )"
    : [pa] "+r" (pa), [p0] "+r" (p0), [p1] "+r" (p1), [p2] "+r" (p2), [p3] "+r" (p3), [po] "+r" (po), [n] "+r" (n)
    :
    : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "t", "memory");
}
#endif

/** \brief Hamilton product of two arrays, out[i] = a[i] * b[i]: see shz_dce_quat_mult_batch_c() for the contract,
    the bound and the convention. On the SH-4 this is the asm loop; elsewhere the plain C form. count 0 returns
    at once. Alignment: out, a, b on multiples of 4. */
SHZ_INLINE void shz_dce_quat_mult_batch(shz_quat_t *SHZ_RESTRICT out, const shz_quat_t *SHZ_RESTRICT a,
                                        const shz_quat_t *SHZ_RESTRICT b, size_t count) SHZ_NOEXCEPT {
    SHZ_DCE_ASSERT_ALIGNED(out, 4);
    SHZ_DCE_ASSERT_ALIGNED(a, 4);
    SHZ_DCE_ASSERT_ALIGNED(b, 4);
    if (!count)
        return;
#if SHZ_DCE_SH4
    shz_dce_quat_mult_batch_sh4(out, a, b, count);
#else
    shz_dce_quat_mult_batch_c(out, a, b, count);
#endif
}

/* ---------------------------------------------------------------------------------------------- */
/* Normalise.                                                                                     */
/* ---------------------------------------------------------------------------------------------- */

/** \brief Normalise an array: out[i] is in[i] scaled to unit length (SH4ZAM's shz_quat_normalize(), per element).
    Plain C form: the software back end and the reference of the SH-4 loop.

    Inputs: non-zero quaternions (a zero one divides by zero, as shz_quat_normalize() does). Overwrites nothing.
    Alignment: out, in on multiples of 4; out must not overlap in. Error bound (absolute, per component, for an input
    length between 1e-15 and 1e15, the range of the file comment): 2^-19.
    Measured: 1.4e-7 to 1.9e-7 on the software back end and on the SH-4 interpreter. Bit-identical to shz_quat_normalize():
    NO on the SH-4 (the squared length is a FIPR); on the software back end the expression is SH4ZAM's up to
    the order of the sum of squares. */
SHZ_INLINE void shz_dce_quat_normalize_batch_c(shz_quat_t *SHZ_RESTRICT out, const shz_quat_t *SHZ_RESTRICT in,
                                               size_t count) SHZ_NOEXCEPT {
    size_t i;

    for (i = 0; i < count; i++) {
        const float *p = in[i].e;
        const float r = shz_inv_sqrtf_fsrra((p[0] * p[0] + p[1] * p[1]) + (p[2] * p[2] + p[3] * p[3]));

        out[i] = shz_quat_init(p[0] * r, p[1] * r, p[2] * r, p[3] * r);
    }
}

#if SHZ_DCE_SH4
/** \brief shz_dce_quat_normalize_batch() on the SH-4: six elements an iteration, a loop body of six interleaved
    elements that use three register sets in turn (set 0: fv4 and fr12; set 1: fv8 and fr13; set 2: fv0 and fr14),
    then the remaining elements one at a time. Per element: the four loads into an aligned group, the copy of the
    fourth component (the FIPR overwrites it), FIPR of the group with itself (|q|^2 over the fourth register), FSRRA,
    the four scales and four backwards stores. Uses fr0 to fr14 and no r0; XMTRX and FPSCR untouched.
    Cycles E per element, from the model: 10.66 (SH4ZAM's loop: 31.00). */
SHZ_INLINE void shz_dce_quat_normalize_batch_sh4(shz_quat_t *SHZ_RESTRICT out, const shz_quat_t *SHZ_RESTRICT in,
                                                 size_t count) SHZ_NOEXCEPT {
    const char *pa = (const char *)in;
    char *po = (char *)out + 16;               /* stores run backwards from the end of each element */
    size_t groups = count / 6u;
    size_t rem = count % 6u;

    if (!count)
        return;
    asm volatile(R"(
        tst     %[groups], %[groups]
        bt      3f
    1:
        fmov.s  @%[pa]+, fr4      ! e0 ld q.w
        fmov.s  @%[pa]+, fr5      ! e0 ld q.x
        fmov.s  @%[pa]+, fr6      ! e0 ld q.y
        fmov.s  @%[pa]+, fr7      ! e0 ld q.z
        fmov    fr7, fr12       ! e0 copy q.z (FIPR overwrites it)
        fipr    fv4, fv4      ! e0 |q|^2
        fmov.s  @%[pa]+, fr8      ! e1 ld q.w
        fmov.s  @%[pa]+, fr9      ! e1 ld q.x
        fmov.s  @%[pa]+, fr10      ! e1 ld q.y
        fmov.s  @%[pa]+, fr11      ! e1 ld q.z
        fsrra   fr7            ! e0 1 / |q|
        fmov    fr11, fr13       ! e1 copy q.z (FIPR overwrites it)
        fmov.s  @%[pa]+, fr0      ! e2 ld q.w
        fipr    fv8, fv8      ! e1 |q|^2
        fmov.s  @%[pa]+, fr1      ! e2 ld q.x
        fmul    fr7, fr6      ! e0 scale y
        fmul    fr7, fr5      ! e0 scale x
        fmov.s  @%[pa]+, fr2      ! e2 ld q.y
        fmul    fr7, fr12       ! e0 scale z
        fsrra   fr11            ! e1 1 / |q|
        fmov.s  @%[pa]+, fr3      ! e2 ld q.z
        fmov    fr3, fr14       ! e2 copy q.z (FIPR overwrites it)
        fmov.s  fr12, @-%[po]       ! e0 st z
        fmul    fr7, fr4      ! e0 scale w
        fipr    fv0, fv0      ! e2 |q|^2
        fmov.s  fr6, @-%[po]      ! e0 st y
        fmul    fr11, fr13       ! e1 scale z
        fmov.s  fr5, @-%[po]      ! e0 st x
        fmov.s  fr4, @-%[po]      ! e0 st w
        fmul    fr11, fr10      ! e1 scale y
        fmul    fr11, fr9      ! e1 scale x
        fmov.s  @%[pa]+, fr4      ! e0 ld q.w
        fmov.s  @%[pa]+, fr5      ! e0 ld q.x
        fsrra   fr3            ! e2 1 / |q|
        fmov.s  @%[pa]+, fr6      ! e0 ld q.y
        add     #32, %[po]         ! e0 next element out
        fmov.s  @%[pa]+, fr7      ! e0 ld q.z
        fmov.s  fr13, @-%[po]       ! e1 st z
        fmul    fr11, fr8      ! e1 scale w
        fmov    fr7, fr12       ! e0 copy q.z (FIPR overwrites it)
        fipr    fv4, fv4      ! e0 |q|^2
        fmul    fr3, fr14       ! e2 scale z
        fmov.s  fr10, @-%[po]      ! e1 st y
        fmov.s  fr9, @-%[po]      ! e1 st x
        fmov.s  fr8, @-%[po]      ! e1 st w
        fmul    fr3, fr2      ! e2 scale y
        fsrra   fr7            ! e0 1 / |q|
        fmov.s  @%[pa]+, fr8      ! e1 ld q.w
        fmov.s  @%[pa]+, fr9      ! e1 ld q.x
        add     #32, %[po]         ! e1 next element out
        dt      %[groups]
        fmov.s  fr14, @-%[po]       ! e2 st z
        fmov.s  @%[pa]+, fr10      ! e1 ld q.y
        fmul    fr3, fr0      ! e2 scale w
        fmul    fr3, fr1      ! e2 scale x
        fmov.s  @%[pa]+, fr11      ! e1 ld q.z
        fmov.s  fr2, @-%[po]      ! e2 st y
        fmul    fr7, fr12       ! e0 scale z
        fmov    fr11, fr13       ! e1 copy q.z (FIPR overwrites it)
        fipr    fv8, fv8      ! e1 |q|^2
        fmov.s  fr1, @-%[po]      ! e2 st x
        fmul    fr7, fr6      ! e0 scale y
        fmul    fr7, fr4      ! e0 scale w
        fmov.s  fr0, @-%[po]      ! e2 st w
        fmul    fr7, fr5      ! e0 scale x
        add     #32, %[po]         ! e2 next element out
        fmov.s  @%[pa]+, fr0      ! e2 ld q.w
        fsrra   fr11            ! e1 1 / |q|
        fmov.s  @%[pa]+, fr1      ! e2 ld q.x
        fmov.s  @%[pa]+, fr2      ! e2 ld q.y
        fmov.s  @%[pa]+, fr3      ! e2 ld q.z
        fmov.s  fr12, @-%[po]       ! e0 st z
        fmov    fr3, fr14       ! e2 copy q.z (FIPR overwrites it)
        fipr    fv0, fv0      ! e2 |q|^2
        fmov.s  fr6, @-%[po]      ! e0 st y
        fmul    fr11, fr10      ! e1 scale y
        fmov.s  fr5, @-%[po]      ! e0 st x
        fmul    fr11, fr8      ! e1 scale w
        fmov.s  fr4, @-%[po]      ! e0 st w
        fmul    fr11, fr13       ! e1 scale z
        fsrra   fr3            ! e2 1 / |q|
        add     #32, %[po]         ! e0 next element out
        fmov.s  fr13, @-%[po]       ! e1 st z
        fmul    fr11, fr9      ! e1 scale x
        fmul    fr3, fr2      ! e2 scale y
        fmov.s  fr10, @-%[po]      ! e1 st y
        fmov.s  fr9, @-%[po]      ! e1 st x
        fmul    fr3, fr14       ! e2 scale z
        fmul    fr3, fr0      ! e2 scale w
        fmov.s  fr8, @-%[po]      ! e1 st w
        add     #32, %[po]         ! e1 next element out
        fmul    fr3, fr1      ! e2 scale x
        fmov.s  fr14, @-%[po]       ! e2 st z
        fmov.s  fr2, @-%[po]      ! e2 st y
        fmov.s  fr1, @-%[po]      ! e2 st x
        fmov.s  fr0, @-%[po]      ! e2 st w
        add     #32, %[po]         ! e2 next element out
        bf/s    1b
        nop
    3:
        tst     %[rem], %[rem]
        bt      4f
    2:
        fmov.s  @%[pa]+, fr4      ! e0 ld q.w
        fmov.s  @%[pa]+, fr5      ! e0 ld q.x
        fmov.s  @%[pa]+, fr6      ! e0 ld q.y
        fmov.s  @%[pa]+, fr7      ! e0 ld q.z
        fmov    fr7, fr12       ! e0 copy q.z (FIPR overwrites it)
        fipr    fv4, fv4      ! e0 |q|^2
        fsrra   fr7            ! e0 1 / |q|
        fmul    fr7, fr4      ! e0 scale w
        fmul    fr7, fr5      ! e0 scale x
        fmul    fr7, fr6      ! e0 scale y
        fmul    fr7, fr12       ! e0 scale z
        fmov.s  fr12, @-%[po]       ! e0 st z
        fmov.s  fr6, @-%[po]      ! e0 st y
        fmov.s  fr5, @-%[po]      ! e0 st x
        fmov.s  fr4, @-%[po]      ! e0 st w
        add     #32, %[po]         ! e0 next element out
        dt      %[rem]
        bf/s    2b
        nop
    4:
    )"
    : [pa] "+r" (pa), [po] "+r" (po), [groups] "+r" (groups), [rem] "+r" (rem)
    :
    : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "t", "memory");
}
#endif

/** \brief Normalise an array, out[i] = in[i] / |in[i]|: see shz_dce_quat_normalize_batch_c() for the contract, the bound
    and the input rule. On the SH-4 this is the asm loop; elsewhere the plain C form. count 0 returns at once. */
SHZ_INLINE void shz_dce_quat_normalize_batch(shz_quat_t *SHZ_RESTRICT out, const shz_quat_t *SHZ_RESTRICT in,
                                             size_t count) SHZ_NOEXCEPT {
    SHZ_DCE_ASSERT_ALIGNED(out, 4);
    SHZ_DCE_ASSERT_ALIGNED(in, 4);
    if (!count)
        return;
#if SHZ_DCE_SH4
    shz_dce_quat_normalize_batch_sh4(out, in, count);
#else
    shz_dce_quat_normalize_batch_c(out, in, count);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_QUAT_H */
