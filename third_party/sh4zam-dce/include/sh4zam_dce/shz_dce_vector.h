/* SH4ZAM-DCE: vectors in batches, and the vec3 routines on the SH-4's inner product unit
   shz_dce_vector.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

/** \file    shz_dce_vector.h
    \brief   Dot products of two arrays of 3-vectors, the cross product of two arrays, the normalisation of an array,
             the length of one 3-vector and the normalisation of one 4-vector: the FIPR forms where they pay.

    The SH-4 has no three-lane inner product. FIPR takes two four-float vectors (fv registers: four consecutive
    floats, fv0 / fv4 / fv8 / fv12) and returns the sum of their four products in one instruction, latency 4.
    A 3-vector is an fv register with a zero fourth lane. FIPR writes its result over the fourth lane of its
    SECOND operand, so that lane is zeroed again before each use (one fldi0 each time).

    Dot products of arrays: with FPSCR.SZ = 1 a float load moves a PAIR of floats in one load slot. A 12-byte
    element then costs 1.5 loads (the pairs straddle two elements), and the two elements of a pair go into two
    four-lane groups. The loop does two elements an iteration, software pipelined: 14 LS slots for the two
    (8 pair loads, 4 zeroings, 2 stores), seven an element. The two lanes that would hold the other element's values
    (fr7 and fr12) are zeroed too, so a NaN or an infinity in one element cannot reach the other one (0 times an
    infinity is a NaN). The odd element of an odd count is done alone with single loads, so the routine never
    reads past the array.

    Normalisation of arrays: four elements an iteration, single loads and stores, one FIPR for each squared
    length, one FSRRA for each reciprocal length, three multiplies per element. Two chains of FSRRA and FMUL run
    interleaved so the in-order pipeline's latencies overlap.

    Cross product: plain C, through the pointers. It does not pay (see below).

    Order and state: nothing needs loading first. Every routine here assumes FPSCR.FR = 0 and FPSCR.SZ = 0 on entry
    (the console's default, and what the compiler assumes: a single load under SZ = 1 would move a pair). The batch
    dot product sets SZ = 1 for its pair loads and puts it back to 0; the others change no FPSCR bit. The FPU
    registers fr0-fr15 and r0 are used, and the compiler saves what it must.

    Alignment: the batch dot product needs both element arrays on a multiple of 8 (the pair loads require it; a
    debug build asserts it), and its output on a multiple of 4. The other routines read and write single floats:
    a multiple of 4 is enough. The SH-4 check places every other buffer on 4 mod 8, so an 8-byte access would stop it.

    Error bound of a FIPR inner product: with P the largest product and R the result, the SH-4 manual bounds the
    error by P x 2^-23 + |R| x 2^-23; the software back end (the products summed in float) stays inside the same
    (P + |R|) x 2^-21. That holds for products in the normal float range: a product below 2^-126 is outside it (with
    FPSCR.DN = 1, as on the console, such a product reads as zero), and an overflow gives an infinity. FSRRA is an
    approximation as well (see shz_dce_cdefs.h). The tests measure every routine against the exact value (the dot and
    normalising batches at three scales: normal, tiny and huge); the worst relative error is printed there (about 2e-7 for the normalisations).

    Special values: a NaN or an infinity in one element gives a NaN or an infinite result for that element only, as
    the single-element form does (tested on the SH-4 with a value planted in an element of a batch). A zero vector
    gives NaN from the normalisations and 0 from the length, and an infinite or NaN component gives the same NaN and
    zero pattern as SH4ZAM's routines (tested on the SH-4 and the host).

    Bit-identical to the SH4ZAM routine each one replaces: NO for every routine that uses FIPR (the sum is rounded
    once, in a different order). The tests print the largest distance in units in the last place for the length and
    the normalisations (2 and 3 ulp for the one-call length and vec4 normalise on the SH-4; the normalising batch
    differs in many components); the FIPR dot batch is compared with the single-element FIPR form (zero differences),
    not with shz_vec3_dot(). YES for shz_dce_vec3_cross_batch(), which keeps shz_vec3_cross()'s expressions (measured:
    zero differences on the SH-4 and the host). shz_dce_vec3_dot4() keeps shz_vec3_dot()'s expressions: the same bits
    on the host, and on the SH-4 the same bits except the sign of an exactly zero result (see its comment).

    Measured and NOT shipped (cycles E per element, -O3, the interpreter's model; the SH4ZAM row in brackets): the
    one-call normalise 22 [24], the one-call safe normalise 25 [27], the one-call inverse length 18 [21] (a gain of
    14%, under the bar), the one-call distance 27 [31] (13%). The cross product in plain C: 20 [20]. The pointer form
    of the batch dot product in plain C: 14 [14]: the gain of the batch dot product is the FIPR pair loop, not the
    removed struct copies. The cross product does not pay: each component is a difference of two products, and the
    FPU's two-operand instructions are destructive and fmac takes its first factor from fr0, so the compiled loop
    spends 16 load, store, move and negate slots an element (6 loads, 3 stores, 4 register moves, 3 negations; the
    SH-4 assembly of the bench loop) against 20 E: the slots, not the arithmetic, set the pace.

    Version: v0.10
    The effect: nothing on screen. Fewer SH-4 cycles for the array work of culling, lighting and particles, and for
      the length of one vector and the direction of one 4-vector.
    Use it for: a batch of directions against one direction (dot products), normalising a batch of directions,
      the length of one vector, normalising one 4-vector.
*/

#ifndef SHZ_DCE_VECTOR_H
#define SHZ_DCE_VECTOR_H

#include "shz_dce_cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Dot products of two arrays of 3-vectors, plain C: out[i] = a[i] . b[i]. Reads the floats through the pointers (no struct copies).
    Inputs: a and b, count elements each (any alignment of 4). out: count floats (a multiple of 4); out may not overlap a or b.
    Error bound: (P + |R|) x 2^-21, P the largest product. Bit-identical to shz_vec3_dot(): YES under strict IEEE flags on the host (the host test
    counts 0 differences); under -ffast-math the compiler may reassociate the same sum differently at each call site, so NO is the safe answer
    there. Cycles E: 14, the same as the SH4ZAM loop (bench row dot_c): the struct copies are not what costs the SH4ZAM loop. */
SHZ_INLINE void shz_dce_vec3_dot_batch_c(float *out, const shz_vec3_t *a, const shz_vec3_t *b, size_t count) SHZ_NOEXCEPT {
    size_t i;

    for (i = 0; i < count; i++)
        out[i] = a[i].x * b[i].x + a[i].y * b[i].y + a[i].z * b[i].z;
}

#if SHZ_DCE_SH4
/** \brief The inner product of one pair of 3-vectors by FIPR, with the lanes of the batch's even elements: the batch's odd tail and the
    single-element reference of the tests. Reads with single loads (any multiple of 4). Returns the value. */
SHZ_FORCE_INLINE float shz_dce_vec3_dot_fipr_one_sh4(const shz_vec3_t *a, const shz_vec3_t *b) SHZ_NOEXCEPT {
    register float x0 asm("fr4") = a->x;
    register float y0 asm("fr5") = a->y;
    register float z0 asm("fr6") = a->z;
    register float w0 asm("fr7") = 0.0f;
    register float x1 asm("fr8") = b->x;
    register float y1 asm("fr9") = b->y;
    register float z1 asm("fr10") = b->z;
    register float w1 asm("fr11") = 0.0f;

    asm("fipr   fv4, fv8"
        : "+f" (w1)
        : "f" (x0), "f" (y0), "f" (z0), "f" (w0), "f" (x1), "f" (y1), "f" (z1));

    return w1;
}

/** \brief shz_dce_vec3_dot_batch() on the SH-4: pairs of elements by FIPR, software pipelined, the odd tail alone.
    The pair loop (see the file comment) runs with FPSCR.SZ = 1 for its loads and SZ = 0 for its stores, one FE op to switch each way.
    Registers: fv4 / fv8 hold element 2k, fv12 / fv0 hold element 2k+1. Element 2k+1's first lane (fr12) and element 2k's fourth lane (fr7)
    hold the other element's values: both are zeroed (fldi0), and so are the second operands' fourth lane (fr11) and first lane (fr0), so no
    product of a neighbour's value reaches an element (a zero times an infinity is a NaN). FIPR writes the results into fr11 and fr3.
    One body: 14 LS slots for two elements (8 pair loads, 4 zeroings, 2 stores). Pointers are advanced by the post-increment loads; r0 holds
    -8 for the one reload of a pair. */
SHZ_INLINE void shz_dce_vec3_dot_batch_sh4(float *out, const shz_vec3_t *a, const shz_vec3_t *b, size_t count) SHZ_NOEXCEPT {
    const size_t pairs = count >> 1;
    const float *pa = (const float *)a;
    const float *pb = (const float *)b;
    float *po = out;
    size_t np = pairs, cnt;

    if (pairs) {
        asm volatile(R"(
            fschg
            mov     #-8, r0
            fmov.s  @%[pa]+, fr4
            fmov.s  @%[pa]+, fr6
            fmov.s  @%[pb]+, fr8
            fmov.s  @(r0, %[pa]), fr12
            fmov.s  @%[pa]+, fr14
            fmov.s  @%[pb]+, fr10
            fmov.s  @(r0, %[pb]), fr0
            fmov.s  @%[pb]+, fr2
            fldi0   fr7
            fldi0   fr12
            fldi0   fr11
            fldi0   fr0
            mov     %[np], %[cnt]
            add     #-1, %[cnt]
            tst     %[cnt], %[cnt]
            bt      2f
        1:
            fipr    fv4, fv8
            fmov.s  @%[pa]+, fr4
            fipr    fv12, fv0
            fmov.s  @%[pa]+, fr6
            fmov.s  @%[pb]+, fr8
            fmov.s  @(r0, %[pa]), fr12
            fschg
            fmov.s  fr11, @%[po]
            add     #4, %[po]
            fmov.s  fr3, @%[po]
            fschg
            add     #4, %[po]
            fmov.s  @%[pb]+, fr10
            fmov.s  @(r0, %[pb]), fr0
            fmov.s  @%[pa]+, fr14
            dt      %[cnt]
            fmov.s  @%[pb]+, fr2
            fldi0   fr7
            fldi0   fr12
            fldi0   fr11
            bf/s    1b
            fldi0   fr0
        2:
            fipr    fv4, fv8
            fipr    fv12, fv0
            fschg
            fmov.s  fr11, @%[po]
            add     #4, %[po]
            fmov.s  fr3, @%[po]
        )"
        : [pa] "+r" (pa), [pb] "+r" (pb), [po] "+r" (po), [np] "+r" (np), [cnt] "=&r" (cnt)
        :
        : "r0", "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15",
          "t", "memory");
    }
    if (count & 1u)
        out[count - 1] = shz_dce_vec3_dot_fipr_one_sh4(a + count - 1, b + count - 1);
}
#endif

/** \brief Dot products of two arrays of 3-vectors: out[i] = a[i] . b[i]. The SH-4 path uses FIPR (see the file comment); elsewhere the plain C
    form. a, b: on a multiple of 8 (the SH-4 pair loads need it; a debug build asserts it). out: on a multiple of 4. count 0: nothing. Overwrites
    out[0 .. count-1] only. Error bound: (P + |R|) x 2^-21, P the largest product. Bit-identical to shz_vec3_dot(): NO. A NaN or an infinity in one
    element does not change the other elements (tested). Cycles E: 7 (bench row dot_batch) against 14 for the SH4ZAM loop: a gain of 50%. */
SHZ_INLINE void shz_dce_vec3_dot_batch(float *SHZ_RESTRICT out, const shz_vec3_t *SHZ_RESTRICT a, const shz_vec3_t *SHZ_RESTRICT b, size_t count) SHZ_NOEXCEPT {
    if (!count)
        return;
    SHZ_DCE_ASSERT_ALIGNED(a, 8);
    SHZ_DCE_ASSERT_ALIGNED(b, 8);
    SHZ_DCE_ASSERT_ALIGNED(out, 4);
#if SHZ_DCE_SH4
    shz_dce_vec3_dot_batch_sh4(out, a, b, count);
#else
    shz_dce_vec3_dot_batch_c(out, a, b, count);
#endif
}

/** \brief Four inner products of four pairs of 3-vectors, plain C: the result's x is a[0] . b[0], y is a[1] . b[1], z is a[2] . b[2], w is a[3] . b[3].
    Inputs: a and b, four elements each (any alignment of 4). Error bound: (P + |R|) x 2^-21 for each lane, P the largest product of that lane.
    Bit-identical to four shz_vec3_dot() calls: YES on the host (0 of 800 lanes differ in the tests, signed zeros included). On the SH-4 the bits
    are the same except the sign of an exactly zero result: SH4ZAM's dot product on the SH-4 adds a zero term (its w.w lane), which turns -0 into
    +0; this routine does not. Every differing lane is such a zero (tested), and 0 of 1200 random lanes differ. Cycles E per quad: 50 (bench row
    dot4_dce) against 61 for four SH4ZAM calls: a gain of 18%. */
SHZ_INLINE shz_vec4_t shz_dce_vec3_dot4(const shz_vec3_t a[4], const shz_vec3_t b[4]) SHZ_NOEXCEPT {
    return shz_vec4_init(a[0].x * b[0].x + a[0].y * b[0].y + a[0].z * b[0].z,
                         a[1].x * b[1].x + a[1].y * b[1].y + a[1].z * b[1].z,
                         a[2].x * b[2].x + a[2].y * b[2].y + a[2].z * b[2].z,
                         a[3].x * b[3].x + a[3].y * b[3].y + a[3].z * b[3].z);
}

/** \brief Cross products of two arrays of 3-vectors: out[i] = a[i] x b[i], the same expression as shz_vec3_cross() (same bits under the same flags:
    YES, measured 0 differences on the SH-4 and the host). Plain C through the pointers. out may be a or b (element by element: each element is read
    before it is written), otherwise out and the inputs must not overlap. Alignment: any multiple of 4. Error bound: as shz_vec3_cross(), one rounding
    per product and per difference. DOES NOT PAY: cycles E 20, against 20 for the SH4ZAM loop (see the file comment). Kept as the simplest correct
    form. */
SHZ_INLINE void shz_dce_vec3_cross_batch(shz_vec3_t *out, const shz_vec3_t *a, const shz_vec3_t *b, size_t count) SHZ_NOEXCEPT {
    size_t i;

    for (i = 0; i < count; i++) {
        const float ax = a[i].x, ay = a[i].y, az = a[i].z;
        const float bx = b[i].x, by = b[i].y, bz = b[i].z;

        out[i].x = ay * bz - az * by;
        out[i].y = az * bx - ax * bz;
        out[i].z = ax * by - ay * bx;
    }
}

#if SHZ_DCE_SH4
/** \brief Normalisation of one 3-vector by FIPR and FSRRA (the batch's odd tail). The vector is in fr4..fr6, the zero in fr7: FIPR puts the
    squared length in fr7, FSRRA the reciprocal length; three multiplies. Same special cases as shz_vec3_normalize(): a zero vector gives NaN. */
SHZ_FORCE_INLINE shz_vec3_t shz_dce_vec3_normalize_one_sh4(shz_vec3_t vec) SHZ_NOEXCEPT {
    register float x asm("fr4") = vec.x;
    register float y asm("fr5") = vec.y;
    register float z asm("fr6") = vec.z;
    register float s asm("fr7") = 0.0f;

    asm("fipr   fv4, fv4"
        : "+f" (s)
        : "f" (x), "f" (y), "f" (z));

    s = shz_inv_sqrtf_fsrra(s);
    return shz_vec3_init(x * s, y * s, z * s);
}

/** \brief shz_dce_vec3_normalize_batch() on the SH-4: four elements an iteration (two pairs, A and B of each, in fv4 / fv8 and fv12 / fv0), then
    a pair if the count has a 2 in it, then the one-vector tail. Each element: its three components loaded, the fourth lane zeroed (fldi0), FIPR
    for the squared length (in the fourth lane), FSRRA of it in place, three FMULs in place, three stores. The FSRRA and FMUL chains of the two
    pairs are interleaved so that their latencies overlap. Single loads and stores: FPSCR is not changed (SZ must be 0 on entry). */
SHZ_INLINE void shz_dce_vec3_normalize_batch_sh4(shz_vec3_t *out, const shz_vec3_t *in, size_t count) SHZ_NOEXCEPT {
    const float *p = (const float *)in;
    float *o = (float *)out;
    size_t nq = count >> 2, np = (count >> 1) & 1u;

    if (nq) {
        asm volatile(R"(
        1:
            fmov.s  @%[p]+, fr4
            fmov.s  @%[p]+, fr5
            fmov.s  @%[p]+, fr6
            fldi0   fr7
            fipr    fv4, fv4
            fmov.s  @%[p]+, fr8
            fmov.s  @%[p]+, fr9
            fmov.s  @%[p]+, fr10
            fldi0   fr11
            fipr    fv8, fv8
            fmov.s  @%[p]+, fr12
            fmov.s  @%[p]+, fr13
            fmov.s  @%[p]+, fr14
            fldi0   fr15
            fipr    fv12, fv12
            fmov.s  @%[p]+, fr0
            fmov.s  @%[p]+, fr1
            fmov.s  @%[p]+, fr2
            fldi0   fr3
            fipr    fv0, fv0
            fsrra   fr7
            fsrra   fr11
            fmul    fr7, fr4
            fmul    fr7, fr5
            fmul    fr7, fr6
            fsrra   fr15
            fmov.s  fr4, @%[o]
            add     #4, %[o]
            fmov.s  fr5, @%[o]
            add     #4, %[o]
            fmov.s  fr6, @%[o]
            add     #4, %[o]
            fmul    fr11, fr8
            fmul    fr11, fr9
            fmul    fr11, fr10
            fsrra   fr3
            fmov.s  fr8, @%[o]
            add     #4, %[o]
            fmov.s  fr9, @%[o]
            add     #4, %[o]
            fmov.s  fr10, @%[o]
            add     #4, %[o]
            fmul    fr15, fr12
            fmul    fr15, fr13
            fmul    fr15, fr14
            fmov.s  fr12, @%[o]
            add     #4, %[o]
            fmov.s  fr13, @%[o]
            add     #4, %[o]
            fmov.s  fr14, @%[o]
            add     #4, %[o]
            fmul    fr3, fr0
            fmul    fr3, fr1
            fmul    fr3, fr2
            fmov.s  fr0, @%[o]
            add     #4, %[o]
            fmov.s  fr1, @%[o]
            add     #4, %[o]
            dt      %[nq]
            fmov.s  fr2, @%[o]
            bf/s    1b
            add     #4, %[o]
        )"
        : [p] "+r" (p), [o] "+r" (o), [nq] "+r" (nq)
        :
        : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15", "t", "memory");
    }
    if (np) {
        asm volatile(R"(
            fmov.s  @%[p]+, fr4
            fmov.s  @%[p]+, fr5
            fmov.s  @%[p]+, fr6
            fldi0   fr7
            fmov.s  @%[p]+, fr8
            fmov.s  @%[p]+, fr9
            fmov.s  @%[p]+, fr10
            fldi0   fr11
            fipr    fv4, fv4
            fipr    fv8, fv8
            fsrra   fr7
            fsrra   fr11
            fmul    fr7, fr4
            fmul    fr7, fr5
            fmul    fr7, fr6
            fmul    fr11, fr8
            fmul    fr11, fr9
            fmul    fr11, fr10
            fmov.s  fr4, @%[o]
            add     #4, %[o]
            fmov.s  fr5, @%[o]
            add     #4, %[o]
            fmov.s  fr6, @%[o]
            add     #4, %[o]
            fmov.s  fr8, @%[o]
            add     #4, %[o]
            fmov.s  fr9, @%[o]
            add     #4, %[o]
            fmov.s  fr10, @%[o]
            add     #4, %[o]
        )"
        : [p] "+r" (p), [o] "+r" (o)
        :
        : "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "t", "memory");
    }
    if (count & 1u)
        out[count - 1] = shz_dce_vec3_normalize_one_sh4(in[count - 1]);
}
#endif

/** \brief Normalisation of an array of 3-vectors: out[i] = in[i] / |in[i]|, by FIPR and FSRRA on the SH-4 (see the file comment); the plain
    expression elsewhere. A rework of SH4ZAM's shz_vec3_normalize (Falco Girgis), in batch form, with the same special cases: no zero check, a zero
    vector gives NaN, an infinite or NaN component gives NaN in that component (zeros in the others), and a squared length that overflows gives
    zeros (all tested against the original). out may be in (element by element). Alignment: any multiple of 4. FPSCR.SZ and FR must be 0 on entry.
    Bit-identical to shz_vec3_normalize(): NO. Error bound: the FIPR bound for the squared length, the FSRRA approximation for the reciprocal: the
    tests measure the worst relative error (about 2e-7). Cycles E: 12.75 against 24 for the SH4ZAM loop (bench rows normalize_batch and
    normalize_shz, at -O3 and at -O2): a gain of 47%. */
SHZ_INLINE void shz_dce_vec3_normalize_batch(shz_vec3_t *out, const shz_vec3_t *in, size_t count) SHZ_NOEXCEPT {
#if SHZ_DCE_SH4
    shz_dce_vec3_normalize_batch_sh4(out, in, count);
#else
    size_t i;

    for (i = 0; i < count; i++) {
        const float x = in[i].x, y = in[i].y, z = in[i].z;
        const float inv = shz_inv_sqrtf_fsrra(x * x + y * y + z * z);

        out[i].x = x * inv;
        out[i].y = y * inv;
        out[i].z = z * inv;
    }
#endif
}

/* The FIPR lengths. Against SH4ZAM 0.10 these two had FIPR bodies (SHZ_DCE_REWORK_FIPR 1): the squared length by one FIPR, the root by
   FSRRA. SH4ZAM already has that squared length behind its own switch SHZ_SH4_DOT_FIPR (shz_mag_sqr3f / shz_mag_sqr4f, present in 0.10.0
   and in master ff39547), and master's root is FSQRT: with the FSRRA root the FIPR body gave NaN for an infinite component where master
   gives +inf. So the bodies were removed (the last tree with them is 691aa21); the gain the pipeline model gave them (25 -> 20 E) was
   never measured on a console. A build that wants the FIPR squared length defines SHZ_SH4_DOT_FIPR for SH4ZAM. */

/** \brief SH4ZAM's shz_vec3_magnitude (Falco Girgis), called underneath. The name is kept for code written against it.
    Bit-identical to shz_vec3_magnitude: YES (tests/host/wrap_test.c, tests/sh4/vector_check.c). */
SHZ_FORCE_INLINE float shz_dce_vec3_magnitude_fipr(shz_vec3_t vec) SHZ_NOEXCEPT {
    return shz_vec3_magnitude(vec);
}

/** \brief SH4ZAM's shz_vec4_normalize (Falco Girgis), called underneath. The name is kept for code written against it.
    Bit-identical to shz_vec4_normalize: YES (tests/host/wrap_test.c, tests/sh4/vector_check.c). */
SHZ_FORCE_INLINE shz_vec4_t shz_dce_vec4_normalize(shz_vec4_t vec) SHZ_NOEXCEPT {
    return shz_vec4_normalize(vec);
}

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_VECTOR_H */
