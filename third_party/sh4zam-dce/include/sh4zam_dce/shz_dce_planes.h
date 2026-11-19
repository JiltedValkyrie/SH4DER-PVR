/* SH4ZAM-DCE: points and spheres against planes

   shz_dce_planes.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

/** \file    shz_dce_planes.h
    \brief   Four planes against a point in one FTRV, and a sphere against six planes in one
             FTRV and two FIPRs.

    The idea is Falco Girgis's: the SH-4 multiplies a vector by the 4 x 4 matrix it keeps in its
    second register bank (XMTRX) in one instruction, FTRV, and each of the four results is the
    inner product of one matrix ROW with the vector. Put four plane equations in the four rows
    and transform the point (x, y, z, 1): the four results are the point's signed distances to
    the four planes. A fifth and sixth plane take one FIPR each.

    A plane here is four floats (a, b, c, d): a point p is in front of it when
    a p.x + b p.y + c p.z + d >= 0, behind it when that is negative. With (a, b, c) of length 1
    the value is the distance in the caller's units; the sphere test needs that, the outcode of
    a point does not (only the sign is read).

    Order: shz_dce_planes4_set (or shz_dce_planes6_set) once for a set of planes, then
    shz_dce_planes4_load (shz_dce_planes6_load) before a run of tests: it overwrites XMTRX, and
    the tests read XMTRX, so nothing else may load XMTRX in between. Reload the caller's own
    matrix afterwards.

    Alignment: a shz_dce_planes4_t holds a shz_mat4x4_t, loaded by 8-byte moves: it must sit on
    a multiple of 8 (the type asks for it; a block from malloc or a member of an aligned struct
    is fine, a cast of a byte buffer is not). Points and spheres are arrays of floats on a
    multiple of 4, their stride a multiple of 4.

    Error bound: each distance is one inner product of the SH-4's vector unit; with P the largest
    of |a x|, |b y|, |c z|, |d| and R the result, the SH-4 manual bounds its error by
    P x 2^-23 + |R| x 2^-23, and the software back end (four float multiplies, three adds, each
    rounded) stays under 3.5 P x 2^-23 + 0.5 |R| x 2^-23. Both are inside (P + |R|) x 2^-21: a
    point or sphere closer to a plane than about P x 2^-21 may fall on either side.

    Bit-identical to the scalar test (a x + b y + c z + d < 0 written out, or
    x < -k z for a view-space side plane): NO. The inner product rounds once where the scalar
    form rounds four to seven times, so points within the bound above of a plane can get the
    other outcode bit. Never put this in code that must reproduce a result bit for bit.

    Where it pays, and where it does not (cycles E a point or sphere, see shz_dce_cdefs.h): four
    ARBITRARY planes cost four inner products of three multiplies and three adds each in scalar
    code: 57 E a point with its outcode, 50 E with SH4ZAM's shz_vec4_dot per plane; here one
    FTRV: 18 E (25 E for the plain C form). Six planes against a sphere: 89 E scalar, 28 E here
    (50 E for the plain C form). Planes of a special shape do not gain: a view-space frustum's
    side planes are x +- k z and y +- k z, two multiplies for all four (26 E as a loop of its
    own, less inside a loop that already holds the point), and the compare chain is the same
    length either way: four compares, two cycles each, as there is one T bit.

    Version: v0.10
    The effect: nothing on screen. Fewer SH-4 cycles for culling and clip classification against
      planes that are not axis-aligned in the space of the data.
    Use it for: world- or model-space frustum planes, portal planes, a convex volume of up to six
      planes, against many points or spheres.
*/

#ifndef SHZ_DCE_PLANES_H
#define SHZ_DCE_PLANES_H

#include "shz_dce_cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Four planes, laid out as XMTRX wants them (the planes are the matrix rows). 64 bytes on a multiple of 8. */
typedef struct shz_dce_planes4 {
    shz_mat4x4_t rows;   /**< \brief Column j holds component j (a, b, c, d) of the four planes. */
} shz_dce_planes4_t;

/** \brief Six planes: four for FTRV, two for FIPR. */
typedef struct shz_dce_planes6 {
    shz_dce_planes4_t side;   /**< \brief Planes 0..3. */
    shz_vec4_t cap[2];        /**< \brief Planes 4 and 5, as (a, b, c, d). */
} shz_dce_planes6_t;

/** \name Outcode of a batch (shz_dce_planes4_outcode_batch())
    @{ */
#define SHZ_DCE_OUTCODE_ANY(r) ((r) & 0xfu)          /**< \brief The planes at least one point of the batch is behind. */
#define SHZ_DCE_OUTCODE_ALL(r) (((r) >> 8) & 0xfu)   /**< \brief The planes every point of the batch is behind: any bit set, and the batch is wholly outside. */
/** @} */

/** \name Sphere against six planes (shz_dce_planes6_sphere())
    @{ */
#define SHZ_DCE_SPHERE_OUT(r) ((r) & 0x3fu)            /**< \brief Bit i: the whole sphere is behind plane i. Not 0: the sphere is outside the volume. */
#define SHZ_DCE_SPHERE_CROSS(r) (((r) >> 8) & 0x3fu)   /**< \brief Bit i: the sphere crosses or touches plane i. OUT and CROSS both 0: wholly inside. */
/** @} */

/** \brief Store four planes (each a, b, c, d) for shz_dce_planes4_load(). */
SHZ_INLINE void shz_dce_planes4_set(shz_dce_planes4_t *planes, shz_vec4_t p0, shz_vec4_t p1, shz_vec4_t p2, shz_vec4_t p3) SHZ_NOEXCEPT {
    planes->rows.col[0] = shz_vec4_init(p0.x, p1.x, p2.x, p3.x);
    planes->rows.col[1] = shz_vec4_init(p0.y, p1.y, p2.y, p3.y);
    planes->rows.col[2] = shz_vec4_init(p0.z, p1.z, p2.z, p3.z);
    planes->rows.col[3] = shz_vec4_init(p0.w, p1.w, p2.w, p3.w);
}

/** \brief Put the four planes in XMTRX (its old contents are gone). planes: on a multiple of 8. About 10 cycles E. */
SHZ_FORCE_INLINE void shz_dce_planes4_load(const shz_dce_planes4_t *planes) SHZ_NOEXCEPT {
    SHZ_DCE_ASSERT_ALIGNED(planes, 8);
    shz_xmtrx_load_4x4(&planes->rows);
}

/** \brief The point's four signed distances (x: plane 0 ... w: plane 3). Needs shz_dce_planes4_load().
    One FTRV. Error bound and "not bit-identical": see the file comment. */
SHZ_FORCE_INLINE shz_vec4_t shz_dce_planes4_dist(shz_vec3_t point) SHZ_NOEXCEPT {
    return shz_xmtrx_transform_vec4(shz_vec3_vec4(point, 1.0f));
}

/** \brief Outcode of four distances: bit i set when distance i is negative (a zero of either sign and a NaN leave the bit clear). */
SHZ_FORCE_INLINE unsigned shz_dce_outcode4(shz_vec4_t dist) SHZ_NOEXCEPT {
    return (unsigned)(dist.x < 0.0f) | ((unsigned)(dist.y < 0.0f) << 1) | ((unsigned)(dist.z < 0.0f) << 2) | ((unsigned)(dist.w < 0.0f) << 3);
}

/** \brief The point's outcode: bit i set when the point is behind plane i. Needs shz_dce_planes4_load(). */
SHZ_FORCE_INLINE unsigned shz_dce_planes4_outcode(shz_vec3_t point) SHZ_NOEXCEPT {
    return shz_dce_outcode4(shz_dce_planes4_dist(point));
}

/** \brief shz_dce_planes4_outcode_batch() in plain C on SH4ZAM's routines: the software back end, and the reference the SH-4 loop is tested against (same FTRV, same bits). */
SHZ_INLINE unsigned shz_dce_planes4_outcode_batch_c(uint8_t *SHZ_RESTRICT codes, const float *SHZ_RESTRICT points, size_t stride, size_t count) SHZ_NOEXCEPT {
    unsigned any = 0u, all = 0xfu;
    size_t i;

    if (!count)
        return 0u;
    for (i = 0; i < count; i++) {
        const float *p = (const float *)((const char *)points + i * stride);
        const unsigned code = shz_dce_planes4_outcode(shz_vec3_init(p[0], p[1], p[2]));

        codes[i] = (uint8_t)code;
        any |= code;
        all &= code;
    }
    return any | (all << 8);
}

#if SHZ_DCE_SH4
/** \brief shz_dce_planes4_outcode_batch() on the SH-4: two points an iteration, so the second point's loads and FTRV run while the first
    point's four compares wait on the T bit (an FCMP sets it two cycles later, and there is one T). */
SHZ_INLINE unsigned shz_dce_planes4_outcode_batch_sh4(uint8_t *SHZ_RESTRICT codes, const float *SHZ_RESTRICT points, size_t stride, size_t count) SHZ_NOEXCEPT {
    unsigned any = 0u, all = 0xfu, code;
    size_t pairs = count >> 1;
    const ptrdiff_t step = (ptrdiff_t)stride - 8;   /* two of the three loads move the pointer themselves */

    if (!count)
        return 0u;
    if (pairs) {
        asm volatile(R"(
            fldi0   fr0
        1:
            fmov.s  @%[p]+, fr4
            fmov.s  @%[p]+, fr5
            fmov.s  @%[p], fr6
            fldi1   fr7
            add     %[step], %[p]
            ftrv    xmtrx, fv4
            fmov.s  @%[p]+, fr8
            fmov.s  @%[p]+, fr9
            fmov.s  @%[p], fr10
            fldi1   fr11
            add     %[step], %[p]
            ftrv    xmtrx, fv8
            fcmp/gt fr7, fr0
            movt    %[a]
            fcmp/gt fr6, fr0
            rotcl   %[a]
            fcmp/gt fr5, fr0
            rotcl   %[a]
            fcmp/gt fr4, fr0
            rotcl   %[a]
            fcmp/gt fr11, fr0
            movt    r0
            fcmp/gt fr10, fr0
            rotcl   r0
            fcmp/gt fr9, fr0
            rotcl   r0
            fcmp/gt fr8, fr0
            rotcl   r0
            mov.b   %[a], @%[c]
            or      %[a], %[any]
            mov.b   r0, @(1, %[c])
            and     %[a], %[all]
            or      r0, %[any]
            and     r0, %[all]
            dt      %[n]
            bf/s    1b
            add     #2, %[c]
        )"
        : [c] "+r" (codes), [p] "+r" (points), [n] "+r" (pairs), [any] "+r" (any), [all] "+r" (all), [a] "=&r" (code)
        : [step] "r" (step)
        : "r0", "fr0", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "t", "memory");
    }
    if (count & 1u) {   /* the odd one out: codes and points were moved past the pairs by the loop */
        code = shz_dce_planes4_outcode(shz_vec3_init(points[0], points[1], points[2]));
        codes[0] = (uint8_t)code;
        any |= code;
        all &= code;
    }
    return any | (all << 8);
}
#endif

/** \brief Outcodes of a batch of points: codes[i] gets point i's outcode (bit k: behind plane k). Needs shz_dce_planes4_load().
    points: floats on a multiple of 4, x y z at the start of every record; stride: bytes from one record to the next, a multiple of 4,
    at least 12. Returns the OR of all the outcodes in bits 0..3 and their AND in bits 8..11 (SHZ_DCE_OUTCODE_ANY, SHZ_DCE_OUTCODE_ALL);
    0 for an empty batch.
    Error bound and "not bit-identical": see the file comment. Same bits as calling shz_dce_planes4_outcode() per point. */
SHZ_INLINE unsigned shz_dce_planes4_outcode_batch(uint8_t *SHZ_RESTRICT codes, const float *SHZ_RESTRICT points, size_t stride, size_t count) SHZ_NOEXCEPT {
    SHZ_DCE_ASSERT_ALIGNED(points, 4);
    assert(stride >= 12u && !(stride & 3u));
#if SHZ_DCE_SH4
    return shz_dce_planes4_outcode_batch_sh4(codes, points, stride, count);
#else
    return shz_dce_planes4_outcode_batch_c(codes, points, stride, count);
#endif
}

/** \brief Store six planes (each a, b, c, d; unit normals for a metric radius) for shz_dce_planes6_load(). */
SHZ_INLINE void shz_dce_planes6_set(shz_dce_planes6_t *planes, const shz_vec4_t p[6]) SHZ_NOEXCEPT {
    shz_dce_planes4_set(&planes->side, p[0], p[1], p[2], p[3]);
    planes->cap[0] = p[4];
    planes->cap[1] = p[5];
}

/** \brief Put planes 0..3 in XMTRX (its old contents are gone). planes: on a multiple of 8. */
SHZ_FORCE_INLINE void shz_dce_planes6_load(const shz_dce_planes6_t *planes) SHZ_NOEXCEPT {
    shz_dce_planes4_load(&planes->side);
}

/** \brief The six signed distances of a point: planes 0..3 by one FTRV, planes 4 and 5 by one FIPR each (the point is the multiplicand
    of all six inner products, as it is of FTRV's, so the six are computed alike). Needs shz_dce_planes6_load(). */
SHZ_FORCE_INLINE void shz_dce_planes6_dist(const shz_dce_planes6_t *planes, shz_vec3_t point, float dist[6]) SHZ_NOEXCEPT {
    const shz_vec4_t p = shz_vec3_vec4(point, 1.0f);
    const shz_vec2_t e = shz_vec4_dot2(p, planes->cap[0], planes->cap[1]);
    const shz_vec4_t d = shz_xmtrx_transform_vec4(p);

    dist[0] = d.x; dist[1] = d.y; dist[2] = d.z; dist[3] = d.w; dist[4] = e.x; dist[5] = e.y;
}

/** \brief A sphere against the six planes. Needs shz_dce_planes6_load(). Returns, per plane i, bit i when the whole sphere is behind the
    plane (distance < -radius) and bit 8 + i when it crosses or touches it (-radius <= distance <= radius): SHZ_DCE_SPHERE_OUT,
    SHZ_DCE_SPHERE_CROSS. A NaN distance counts as crossing (not culled, not trusted). radius >= 0; unit plane normals.
    Error bound and "not bit-identical": see the file comment (a sphere within the bound of touching a plane may fall either way). */
SHZ_INLINE unsigned shz_dce_planes6_sphere(const shz_dce_planes6_t *planes, shz_vec3_t centre, float radius) SHZ_NOEXCEPT {
    const float lo = -radius;
    float d[6];
    unsigned out = 0u, in = 0u;
    int i;

    shz_dce_planes6_dist(planes, centre, d);
    for (i = 0; i < 6; i++) {
        out |= (unsigned)(d[i] < lo) << i;
        in |= (unsigned)(d[i] > radius) << i;
    }
    return out | ((~(out | in) & 0x3fu) << 8);
}

/** \brief shz_dce_planes6_cull_batch() in plain C: the software back end, and the reference the SH-4 loop is tested against. */
SHZ_INLINE void shz_dce_planes6_cull_batch_c(uint8_t *SHZ_RESTRICT out, const shz_dce_planes6_t *planes, const float *SHZ_RESTRICT spheres, size_t stride, size_t count) SHZ_NOEXCEPT {
    size_t i;

    for (i = 0; i < count; i++) {
        const float *s = (const float *)((const char *)spheres + i * stride);

        out[i] = (uint8_t)SHZ_DCE_SPHERE_OUT(shz_dce_planes6_sphere(planes, shz_vec3_init(s[0], s[1], s[2]), s[3]));
    }
}

#if SHZ_DCE_SH4
/** \brief shz_dce_planes6_cull_batch() on the SH-4. The two FIPR planes stay in fv8 and fv12 for the whole batch (FIPR writes its result
    over the fourth element of its second operand: that one is put back from fr1, fr2 before each use); the six compares are against -radius. */
SHZ_INLINE void shz_dce_planes6_cull_batch_sh4(uint8_t *SHZ_RESTRICT out, const shz_dce_planes6_t *planes, const float *SHZ_RESTRICT spheres, size_t stride, size_t count) SHZ_NOEXCEPT {
    const float *cap = &planes->cap[0].x;
    const ptrdiff_t step = (ptrdiff_t)stride - 12;   /* three of the four loads move the pointer themselves */

    if (!count)
        return;
    asm volatile(R"(
        fmov.s  @%[cap]+, fr8
        fmov.s  @%[cap]+, fr9
        fmov.s  @%[cap]+, fr10
        fmov.s  @%[cap]+, fr1
        fmov.s  @%[cap]+, fr12
        fmov.s  @%[cap]+, fr13
        fmov.s  @%[cap]+, fr14
        fmov.s  @%[cap], fr2
    1:
        fmov.s  @%[s]+, fr4
        fmov.s  @%[s]+, fr5
        fmov.s  @%[s]+, fr6
        fmov.s  @%[s], fr0
        fldi1   fr7
        fmov    fr1, fr11
        fmov    fr2, fr15
        fipr    fv4, fv8
        fneg    fr0
        fipr    fv4, fv12
        add     %[step], %[s]
        ftrv    xmtrx, fv4
        fcmp/gt fr15, fr0
        movt    r0
        fcmp/gt fr11, fr0
        rotcl   r0
        fcmp/gt fr7, fr0
        rotcl   r0
        fcmp/gt fr6, fr0
        rotcl   r0
        fcmp/gt fr5, fr0
        rotcl   r0
        fcmp/gt fr4, fr0
        rotcl   r0
        dt      %[n]
        mov.b   r0, @%[o]
        bf/s    1b
        add     #1, %[o]
    )"
    : [o] "+r" (out), [s] "+r" (spheres), [n] "+r" (count), [cap] "+r" (cap)
    : [step] "r" (step)
    : "r0", "fr0", "fr1", "fr2", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15", "t", "memory");
}
#endif

/** \brief Cull a batch of spheres: out[i] gets, per plane k, bit k when sphere i is wholly behind plane k; 0: not culled.
    Needs shz_dce_planes6_load(). spheres: floats on a multiple of 4, x y z radius at the start of every record; stride: bytes from one
    record to the next, a multiple of 4, at least 16. radius >= 0; unit plane normals.
    Same bits as SHZ_DCE_SPHERE_OUT(shz_dce_planes6_sphere()) per sphere. Error bound and "not bit-identical": see the file comment. */
SHZ_INLINE void shz_dce_planes6_cull_batch(uint8_t *SHZ_RESTRICT out, const shz_dce_planes6_t *planes, const float *SHZ_RESTRICT spheres, size_t stride, size_t count) SHZ_NOEXCEPT {
    SHZ_DCE_ASSERT_ALIGNED(spheres, 4);
    assert(stride >= 16u && !(stride & 3u));
#if SHZ_DCE_SH4
    shz_dce_planes6_cull_batch_sh4(out, planes, spheres, stride, count);
#else
    shz_dce_planes6_cull_batch_c(out, planes, spheres, stride, count);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_PLANES_H */
