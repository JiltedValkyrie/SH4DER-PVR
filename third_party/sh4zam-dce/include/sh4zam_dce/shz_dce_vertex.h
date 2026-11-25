/* SH4ZAM-DCE: unpacking packed vertices

   shz_dce_vertex.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

/** \file    shz_dce_vertex.h
    \brief   A batch of 20-byte packed vertices to 32-byte float vertices.

    A packed vertex is 20 bytes: five 16-bit grid values (x, y, z and the two texture coordinates
    u, v), two signed 16-bit normal components, one 16-bit field nobody reads, and a 32-bit colour
    (ARGB, copied as it is). The grid values are on a grid, so a batch carries one description of
    that grid (shz_dce_vertex20_grid_t): an integer origin and a left shift for the positions, a
    scale and a base for each texture coordinate, a scale for the normal. The output is seven floats
    (x, y, z, u, v, nu, nv) and the colour, in the order of shz_dce_vertex32_t.

    The formula, per vertex, with q0..q4 the grid values and n0, n1 the normal components:
      x  = (float)(origin[0] + (q0 << shift)) * pos_scale     (y and z alike, with q1, q2 and origin[1], origin[2])
      u  = q3 * u_scale + u_base, as ONE fused multiply-add (one rounding)
      v  = q4 * v_scale + v_base, the same
      nu = (float)n0 * n_scale,  nv = (float)n1 * n_scale
      argb copied; the pad field is not used (its word is loaded with the normal one, and its value dropped).
    The integer sum origin + (q << shift) is taken modulo 2^32 and read as a signed 32-bit number.
    That is the plain sum whenever it fits in 32 bits: the tests sample origins up to +-2^22 with
    shifts 0..3, where it always does. With a larger origin, or a shift up to 15, the sum can wrap,
    and the wrapped value is the one the routines give. A shift of 16 or more is not allowed (the
    dispatcher asserts it).

    Order: there is no state. Call shz_dce_vertex20_unpack() on a batch; it overwrites out[0 ..
    count-1] and nothing else. The input, the grid and the output must not overlap. XMTRX, the
    general registers r8 to r14 and fr12 to fr15 are as they were on return; the FPSCR mode bits
    (FR, SZ, PR, DN, RM) are as they were (the SH-4 path toggles FPSCR.SZ and puts it back). The
    arithmetic sets FPSCR's exception flags as it does for any float operation. The routines assume
    FPSCR.FR = 0 and PR = 0 (one register bank, single precision, which -m4-single assumes) and
    FPSCR.RM = 0 (round to nearest, the host's default).

    Alignment: the packed records are read by 32-bit words, so the input must sit on a multiple of
    4 (its records are 20 bytes, so the record stride is a multiple of 4, not of 8). The output
    records are 32 bytes. Output on a multiple of 32: each record's cache line is allocated (MOVCA:
    no read of the old contents) before any byte of it is written, one record ahead in the loop.
    Output on a multiple of 8 but not 32: 8-byte pair stores. Output on a multiple of 4 only: the
    plain form (4-byte stores). The grid is read as 32-bit words and must sit on a multiple of 4.

    Error bound: none. There is no approximation in these formulas: every value is one conversion
    and at most one multiply or one fused multiply-add, each rounded once (see bit-identical below).

    Bit-identical to the scalar expression: YES for x, y, z, nu, nv, on the same machine. Why: an int to
    float conversion is exact below 2^24 and rounds once above that, and a single multiply rounds once;
    the routine does those two steps in the same order as the scalar expression, so the bits match.
    For u and v the reference is the fused form: one fused multiply-add of q * scale + base, with one
    rounding. The SH-4's FMAC instruction is that operation; the tests model it as one correctly rounded
    fused operation, and the console's own FMAC rounding is not checked here. When the scale is a power
    of two the product q * scale is exact (a 16-bit integer times a power of two), so the fused form and
    the plain q * scale + base agree bit for bit as well. For a scale that is not a power of two only the
    fused form is promised: the plain expression can differ in the last bit. The host's fused form does
    not depend on the C library's fmaf (some libraries round it wrongly): it is the FMA instruction when
    the compiler has one, else a correctly rounded plain-C form. The test checks all of this
    (tests/host/vertex_test.c, tests/sh4/vertex_check.c).
    Denormal values are outside the claim: with FPSCR.DN = 1 the SH-4 flushes them to zero (the same
    flush the C expression gets on that machine), while the host does not flush. The tests keep every
    value in the normal range.

    Where it pays, and where it does not (cycles E per vertex, read from cycles_vertex.txt; -O2 and -O3
    agree within one cycle; see tests/sh4/vertex_bench.c): the scalar loop that reads the 16-bit fields one
    by one and stores each float costs 67; the plain C form (shz_dce_vertex20_unpack_c) 51; the SH-4 loop on
    output on a multiple of 32 costs 29, and on a multiple of 8 but not 32, 27. On output on a multiple of 4
    only, the SH-4 form runs the plain form (50 at -O2, 49 at -O3). The SH-4 loop does two records an
    iteration, prefetches the input about one pair ahead, and leaves the last record (or two) to the plain
    form, so neither the cache-line allocation nor the prefetch reaches past the batch.
    The MOVCA loop is 2 E per vertex slower than the plain pair loop in this model, which charges no line
    fill (every access is a cache hit). MOVCA is used on output on a multiple of 32 because that is where
    it avoids the fill of a cold line. The gain on a console is not measured here.

    Version: v0.10
    The effect: nothing on screen. Fewer SH-4 cycles to turn packed vertex data into the float vertices
      that a vertex buffer or a transform wants.
    Use it for: vertex data that stays packed in memory and is expanded to floats once per load or
      once per draw, in batches.
*/

#ifndef SHZ_DCE_VERTEX_H
#define SHZ_DCE_VERTEX_H

#include "shz_dce_cdefs.h"

#if !SHZ_DCE_SH4
#include <string.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** \brief A packed vertex: 20 bytes, little-endian, on a multiple of 4.
    w holds the record as five 32-bit words (w[0] = q0 | q1 << 16, w[1] = q2 | q3 << 16,
    w[2] = q4 | n0 << 16, w[3] = n1 | pad << 16, w[4] = argb); the routines read those words. The
    member type is marked as aliasing, so a byte buffer may be read through this type. */
typedef union shz_dce_vertex20 {
    shz_alias_uint32_t w[5];   /**< \brief The record as five 32-bit words. */
    struct {
        uint16_t q[5];         /**< \brief Grid values: x, y, z, u, v. */
        int16_t n[2];          /**< \brief Signed normal components n0, n1 (for instance an octahedral normal). */
        uint16_t pad;          /**< \brief Not used by the routines. */
        uint32_t argb;         /**< \brief Colour, copied as it is. */
    } f;
} shz_dce_vertex20_t;

/** \brief A float vertex: 32 bytes, on a multiple of 4 (a multiple of 32 for the cache-line path).
    The type is marked as aliasing: a caller may pass a buffer of its own vertex type with this
    layout (a cast of the pointer) and read the result back through that type; the compiler then
    treats the routines' stores as stores to any type (without the mark, strict aliasing would
    let it reorder a read of the caller's type across them). */
typedef struct shz_dce_vertex32 {
    float x, y, z;             /**< \brief Position. */
    float u, v;                /**< \brief Texture coordinates. */
    float nu, nv;              /**< \brief Normal components. */
    uint32_t argb;             /**< \brief Colour. */
} SHZ_ALIASING shz_dce_vertex32_t;

/** \brief The grid of one batch: 40 bytes, on a multiple of 4. */
typedef struct shz_dce_vertex20_grid {
    int32_t origin[3];         /**< \brief Position origin of x, y, z (added to the shifted grid value). */
    uint32_t shift;            /**< \brief Left shift of the x, y, z grid values: 0 to 15. */
    float pos_scale;           /**< \brief Position per unit of the shifted grid value. */
    float u_scale;             /**< \brief u = q3 * u_scale + u_base. */
    float u_base;
    float v_scale;             /**< \brief v = q4 * v_scale + v_base. */
    float v_base;
    float n_scale;             /**< \brief Normal component = n * n_scale (n0 for nu, n1 for nv). */
} shz_dce_vertex20_grid_t;

/* the sizes the routines rely on: a compile error here, not a wrong answer on the console */
typedef char shz_dce_vertex20_size_ok[(sizeof(shz_dce_vertex20_t) == 20u) ? 1 : -1];
typedef char shz_dce_vertex32_size_ok[(sizeof(shz_dce_vertex32_t) == 32u) ? 1 : -1];
typedef char shz_dce_vertex20_grid_size_ok[(sizeof(shz_dce_vertex20_grid_t) == 40u) ? 1 : -1];

#if !SHZ_DCE_SH4
/** \brief a * b + c with ONE rounding, in plain C (the software back end, on a compiler without an FMA instruction).
    The product of two floats is exact in a double (24 + 24 bits). The sum is taken in double with its error kept
    (TwoSum: hi + lo is exactly a * b + c). The double is then rounded to odd (its last bit set when the sum is inexact), and
    rounding that to float gives the correctly rounded result: rounding to odd at 53 bits and then to nearest at 24 bits is
    the same as rounding once, because 53 is at least 24 + 2. Correct for results in the normal float range (the routines'
    domain). The C library's fmaf() is not used: some C libraries round it wrongly. */
SHZ_INLINE float shz_dce_vertex_fmaf_sw(float a, float b, float c) SHZ_NOEXCEPT {
    /* The temporaries are volatile: an optimiser allowed to reorder floating point (-ffast-math) would fold the error term
       of TwoSum to zero, and the rounding would then be wrong on the exact halfway cases. */
    volatile double vp = (double)a * (double)b;
    volatile double vc = (double)c;
    volatile double vhi = vp + vc;
    volatile double vbb = vhi - vp;
    volatile double vt1 = vhi - vbb;
    volatile double vt2 = vp - vt1;
    volatile double vt3 = vc - vbb;
    const double hi = vhi;
    const double lo = vt2 + vt3;
    double h = hi;
    uint64_t bits;

    if (lo != 0.0) {
        memcpy(&bits, &hi, sizeof bits);
        if (!(bits & 1u)) {   /* even last bit: step one double toward the side the exact sum is on (magnitude up when lo has hi's sign) */
            const uint64_t mag = (bits & 0x7fffffffffffffffu) + (((lo > 0.0) == (hi > 0.0)) ? 1u : (uint64_t)-1);
            bits = (bits & 0x8000000000000000u) | mag;
            memcpy(&h, &bits, sizeof h);
        }
    }
    return (float)h;
}
#endif

/** \brief One fused multiply-add: a * b + c with one rounding. On the SH-4 it is FMAC (the multiplicand goes in FR0, where FMAC
    wants it); with a compiler's FMA instruction (-mfma) it is that instruction; otherwise shz_dce_vertex_fmaf_sw().
    Not SH4ZAM's shz_fmaf(), on purpose: that one is the expression a * b + c, which the compiler fuses or not as its flags
    say (two roundings in a strict build, one with -ffp-contract=fast). This one FORCES the single rounding in every build
    and on every target, because the routines here that promise the bits of a fused sum, and the tests that check them, need
    the same answer whatever the flags. Where one rounding or two does not matter, call shz_fmaf(). */
SHZ_FORCE_INLINE float shz_dce_vertex_fmaf(float a, float b, float c) SHZ_NOEXCEPT {
#if SHZ_DCE_SH4
    register float f0 asm("fr0") = a;
    asm("fmac fr0, %1, %0" : "+f" (c) : "f" (b), "f" (f0));
    return c;
#elif defined(__FMA__)
    return __builtin_fmaf(a, b, c);
#else
    return shz_dce_vertex_fmaf_sw(a, b, c);
#endif
}

/** \brief One record in plain C on SH4ZAM-DCE's helpers: the formula in the file comment, one vertex. The
    grid is passed by pointer to a local copy (k), so the compiler can keep its fields in registers. */
SHZ_FORCE_INLINE void shz_dce_vertex20_one_c(shz_dce_vertex32_t *o, const shz_dce_vertex20_t *v, const shz_dce_vertex20_grid_t *k) SHZ_NOEXCEPT {
    const uint32_t w0 = v->w[0], w1 = v->w[1], w2 = v->w[2], w3 = v->w[3], w4 = v->w[4];
    const uint32_t sh = k->shift;
    const uint32_t q0 = w0 & 0xffffu, q1 = w0 >> 16, q2 = w1 & 0xffffu, q3 = w1 >> 16, q4 = w2 & 0xffffu;
    const int32_t n0 = (int16_t)(w2 >> 16), n1 = (int16_t)(w3 & 0xffffu);

    o->x = (float)(int32_t)((uint32_t)k->origin[0] + (q0 << sh)) * k->pos_scale;
    o->y = (float)(int32_t)((uint32_t)k->origin[1] + (q1 << sh)) * k->pos_scale;
    o->z = (float)(int32_t)((uint32_t)k->origin[2] + (q2 << sh)) * k->pos_scale;
    o->u = shz_dce_vertex_fmaf((float)q3, k->u_scale, k->u_base);
    o->v = shz_dce_vertex_fmaf((float)q4, k->v_scale, k->v_base);
    o->nu = (float)n0 * k->n_scale;
    o->nv = (float)n1 * k->n_scale;
    o->argb = w4;
}

/** \brief shz_dce_vertex20_unpack() in plain C: the software back end, and the reference the SH-4 loops are tested against.
    out: 4-byte aligned; when out is on a multiple of 32, each record's line is allocated first (shz_dcache_alloc_line).
    in: 4-byte aligned. grid: 4-byte aligned. Overwrites out[0 .. count-1]. Same bits as the file comment's formula.
    Error bound: none. Cycles E: 51 (-O2 and -O3); the scalar loop, 67. */
SHZ_INLINE void shz_dce_vertex20_unpack_c(shz_dce_vertex32_t *SHZ_RESTRICT out, const shz_dce_vertex20_t *SHZ_RESTRICT in, size_t count, const shz_dce_vertex20_grid_t *grid) SHZ_NOEXCEPT {
    const shz_dce_vertex20_grid_t k = *grid;
    size_t i;

    if (SHZ_DCE_IS_ALIGNED(out, 32)) {
        for (i = 0; i < count; i++) {
            shz_dcache_alloc_line(&out[i]);
            shz_dce_vertex20_one_c(&out[i], &in[i], &k);
        }
    } else {
        for (i = 0; i < count; i++)
            shz_dce_vertex20_one_c(&out[i], &in[i], &k);
    }
}

#if SHZ_DCE_SH4
/* The registers the two loops below use: r0 to r3 and r8 to r14, fr0 and fr2 to fr15 (the compiler saves fr12 to fr15 and
   r8 to r14 around the call), T, FPUL. Their pointers: r4 out, r5 in, r6 the pair count, r7 the grid (then the prefetch pointer). */
#define SHZ_DCE_VERTEX_CLOBBERS "r0", "r1", "r2", "r3", "r8", "r9", "r10", "r11", "r12", "r13", "r14", \
    "fr0", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15", "t", "fpul", "memory"

/** \brief Pairs of records on the SH-4, output on a multiple of 32: the cache line of each record is allocated (MOVCA) one record
    ahead, the first before the loop, and each record is written by four 8-byte pair stores (FPSCR.SZ = 1) and one 4-byte store.
    pairs >= 1; out on a multiple of 32, in on a multiple of 4, grid on a multiple of 4 with shift 0 to 15. Writes out[0 .. 2*pairs-1]
    and allocates (does not write) the line of out[2*pairs], which must lie inside the batch. pairs = 0 returns at once.
    Error bound: none. Cycles E: 29 per vertex (-O2 and -O3), see the file comment. */
SHZ_FORCE_INLINE void shz_dce_vertex20_pairs_movca_sh4(shz_dce_vertex32_t *out, const shz_dce_vertex20_t *in, size_t pairs, const shz_dce_vertex20_grid_t *grid) SHZ_NOEXCEPT {
    if (!pairs)
        return;
    register void *po asm("r4") = out;
    register const void *pi asm("r5") = in;
    register size_t pn asm("r6") = pairs;
    register const void *pg asm("r7") = grid;

    asm volatile(R"(
    mov.l   @r7,r1
    mov.l   @(4,r7),r2
    mov.l   @(8,r7),r3
    mov.l   @(12,r7),r14
    mov     #16,r0
    fmov.s  @(r0,r7),fr10
    mov     #20,r0
    fmov.s  @(r0,r7),fr14
    mov     #24,r0
    fmov.s  @(r0,r7),fr11
    mov     #28,r0
    fmov.s  @(r0,r7),fr15
    mov     #32,r0
    fmov.s  @(r0,r7),fr12
    mov     #36,r0
    fmov.s  @(r0,r7),fr13
    mov     #0,r0
    movca.l r0,@r4
    add     #32,r4
    mov     r5,r7
    add     #56,r7
    fschg
1:
    dt r6
    mov.l @r5+,r8
    movca.l r0,@r4
    mov.l @r5+,r9
    extu.w r8,r0
    shld r14,r0
    mov.l @r5+,r10
    shlr16 r8
    extu.w r9,r13
    add r1,r0
    fmov dr10,dr4
    shld r14,r8
    lds r0,fpul
    add r2,r8
    float fpul,fr2
    shld r14,r13
    lds r8,fpul
    add r3,r13
    float fpul,fr3
    lds r13,fpul
    shlr16 r9
    float fpul,fr4
    lds r9,fpul
    float fpul,fr0
    mov.l @r5+,r11
    extu.w r10,r13
    shlr16 r10
    lds r13,fpul
    exts.w r10,r10
    fmac fr0,fr14,fr5
    exts.w r11,r11
    float fpul,fr0
    lds r10,fpul
    fmov dr12,dr6
    float fpul,fr7
    lds r11,fpul
    fmul fr10,fr4
    float fpul,fr8
    mov.l @r5+,r12
    fmac fr0,fr15,fr6
    mov.l @r5+,r8
    fmul fr13,fr7
    mov.l @r5+,r9
    fmul fr13,fr8
    mov.l @r5+,r10
    fmul fr10,fr2
    extu.w r9,r13
    fmul fr10,fr3
    shld r14,r13
    fmov dr8,@-r4
    add r3,r13
    mov.l r12,@(4,r4)
    shlr16 r9
    fmov dr6,@-r4
    fmov dr4,@-r4
    fmov dr2,@-r4
    add #64,r4
    fmov dr10,dr4
    movca.l r0,@r4
    extu.w r8,r0
    shld r14,r0
    mov.l @r5+,r11
    shlr16 r8
    fmov dr12,dr6
    add r1,r0
    exts.w r11,r11
    shld r14,r8
    lds r0,fpul
    add r2,r8
    float fpul,fr2
    lds r8,fpul
    float fpul,fr3
    lds r13,fpul
    float fpul,fr4
    lds r9,fpul
    float fpul,fr0
    extu.w r10,r13
    shlr16 r10
    lds r13,fpul
    exts.w r10,r10
    mov.l @r5+,r12
    fmac fr0,fr14,fr5
    pref @r7
    float fpul,fr0
    lds r10,fpul
    float fpul,fr7
    lds r11,fpul
    float fpul,fr8
    add #40,r7
    fmac fr0,fr15,fr6
    fmul fr13,fr7
    fmul fr13,fr8
    fmul fr10,fr4
    fmul fr10,fr2
    fmul fr10,fr3
    fmov dr8,@-r4
    mov.l r12,@(4,r4)
    fmov dr6,@-r4
    fmov dr4,@-r4
    fmov dr2,@-r4
    bf/s 1b
    add #64,r4
    fschg
    )" : "+r" (po), "+r" (pi), "+r" (pn), "+r" (pg) : : SHZ_DCE_VERTEX_CLOBBERS);
}

/** \brief Pairs of records on the SH-4, output on a multiple of 8 (not 32): the same loop without the cache-line allocation.
    pairs >= 1; out on a multiple of 8, in on a multiple of 4, grid on a multiple of 4 with shift 0 to 15. Writes out[0 .. 2*pairs-1].
    pairs = 0 returns at once. Error bound: none. Cycles E: 27 per vertex (-O2 and -O3). */
SHZ_FORCE_INLINE void shz_dce_vertex20_pairs_plain_sh4(shz_dce_vertex32_t *out, const shz_dce_vertex20_t *in, size_t pairs, const shz_dce_vertex20_grid_t *grid) SHZ_NOEXCEPT {
    if (!pairs)
        return;
    register void *po asm("r4") = out;
    register const void *pi asm("r5") = in;
    register size_t pn asm("r6") = pairs;
    register const void *pg asm("r7") = grid;

    asm volatile(R"(
    mov.l   @r7,r1
    mov.l   @(4,r7),r2
    mov.l   @(8,r7),r3
    mov.l   @(12,r7),r14
    mov     #16,r0
    fmov.s  @(r0,r7),fr10
    mov     #20,r0
    fmov.s  @(r0,r7),fr14
    mov     #24,r0
    fmov.s  @(r0,r7),fr11
    mov     #28,r0
    fmov.s  @(r0,r7),fr15
    mov     #32,r0
    fmov.s  @(r0,r7),fr12
    mov     #36,r0
    fmov.s  @(r0,r7),fr13
    add     #32,r4
    mov     r5,r7
    add     #56,r7
    fschg
1:
    dt r6
    mov.l @r5+,r8
    mov.l @r5+,r9
    extu.w r8,r0
    shlr16 r8
    shld r14,r0
    extu.w r9,r13
    add r1,r0
    mov.l @r5+,r10
    shld r14,r8
    lds r0,fpul
    add r2,r8
    float fpul,fr2
    shld r14,r13
    lds r8,fpul
    add r3,r13
    float fpul,fr3
    lds r13,fpul
    shlr16 r9
    fmov dr10,dr4
    float fpul,fr4
    lds r9,fpul
    extu.w r10,r13
    float fpul,fr0
    mov.l @r5+,r11
    shlr16 r10
    lds r13,fpul
    exts.w r10,r10
    exts.w r11,r11
    fmac fr0,fr14,fr5
    fmov dr12,dr6
    float fpul,fr0
    lds r10,fpul
    float fpul,fr7
    lds r11,fpul
    float fpul,fr8
    mov.l @r5+,r12
    mov.l @r5+,r8
    fmac fr0,fr15,fr6
    fmul fr13,fr7
    mov.l @r5+,r9
    fmul fr13,fr8
    extu.w r8,r0
    fmul fr10,fr4
    shld r14,r0
    fmul fr10,fr2
    shlr16 r8
    fmul fr10,fr3
    fmov dr8,@-r4
    mov.l r12,@(4,r4)
    add r1,r0
    fmov dr6,@-r4
    shld r14,r8
    extu.w r9,r13
    fmov dr4,@-r4
    lds r0,fpul
    add r2,r8
    shld r14,r13
    fmov dr2,@-r4
    float fpul,fr2
    lds r8,fpul
    add r3,r13
    float fpul,fr3
    lds r13,fpul
    shlr16 r9
    mov.l @r5+,r10
    add #64,r4
    fmov dr10,dr4
    float fpul,fr4
    lds r9,fpul
    extu.w r10,r13
    float fpul,fr0
    mov.l @r5+,r11
    shlr16 r10
    lds r13,fpul
    exts.w r10,r10
    exts.w r11,r11
    fmac fr0,fr14,fr5
    fmov dr12,dr6
    float fpul,fr0
    lds r10,fpul
    float fpul,fr7
    lds r11,fpul
    float fpul,fr8
    mov.l @r5+,r12
    fmac fr0,fr15,fr6
    pref @r7
    fmul fr13,fr7
    add #40,r7
    fmul fr13,fr8
    fmul fr10,fr4
    fmul fr10,fr2
    fmul fr10,fr3
    fmov dr8,@-r4
    mov.l r12,@(4,r4)
    fmov dr6,@-r4
    fmov dr4,@-r4
    fmov dr2,@-r4
    bf/s 1b
    add #64,r4
    fschg
    )" : "+r" (po), "+r" (pi), "+r" (pn), "+r" (pg) : : SHZ_DCE_VERTEX_CLOBBERS);
}
#undef SHZ_DCE_VERTEX_CLOBBERS

/** \brief shz_dce_vertex20_unpack() on the SH-4: the pairs of records in the loops above (all but the last one or two records), and the
    rest in the plain form; the whole batch in the plain form when it holds fewer than three records or the output is not on a multiple
    of 8. Same bits as the plain form. */
SHZ_INLINE void shz_dce_vertex20_unpack_sh4(shz_dce_vertex32_t *SHZ_RESTRICT out, const shz_dce_vertex20_t *SHZ_RESTRICT in, size_t count, const shz_dce_vertex20_grid_t *grid) SHZ_NOEXCEPT {
    size_t pairs, done;

    /* pairs end one record before the batch's last: the line allocated last, and the input prefetched, both stay inside the batch */
    if (count < 3u || !SHZ_DCE_IS_ALIGNED(out, 8)) {
        shz_dce_vertex20_unpack_c(out, in, count, grid);
        return;
    }
    pairs = (count - 1u) >> 1;
    if (SHZ_DCE_IS_ALIGNED(out, 32))
        shz_dce_vertex20_pairs_movca_sh4(out, in, pairs, grid);
    else
        shz_dce_vertex20_pairs_plain_sh4(out, in, pairs, grid);
    done = 2u * pairs;
    shz_dce_vertex20_unpack_c(out + done, in + done, count - done, grid);
}
#endif

/** \brief Unpack a batch of packed vertices into float vertices (the formula in the file comment).
    out: 4-byte aligned, count 32-byte records; in: 4-byte aligned, count 20-byte records; grid: 4-byte aligned, its shift 0 to 15.
    Overwrites out[0 .. count-1] and nothing else. Bit-identical to the scalar expression for x, y, z, nu, nv; the fused form for
    u and v (see the file comment). count 0 touches nothing. Error bound: none. Cycles E per vertex: 29 (output on a multiple of 32),
    27 (on a multiple of 8, not 32), 50 at -O2 and 49 at -O3 (on a multiple of 4 only); the scalar loop, 67. */
SHZ_INLINE void shz_dce_vertex20_unpack(shz_dce_vertex32_t *SHZ_RESTRICT out, const shz_dce_vertex20_t *SHZ_RESTRICT in, size_t count, const shz_dce_vertex20_grid_t *grid) SHZ_NOEXCEPT {
    if (!count)
        return;
    SHZ_DCE_ASSERT_ALIGNED(out, 4);
    SHZ_DCE_ASSERT_ALIGNED(in, 4);
    SHZ_DCE_ASSERT_ALIGNED(grid, 4);
    assert(grid->shift <= 15u);
#if SHZ_DCE_SH4
    shz_dce_vertex20_unpack_sh4(out, in, count, grid);
#else
    shz_dce_vertex20_unpack_c(out, in, count, grid);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_VERTEX_H */
