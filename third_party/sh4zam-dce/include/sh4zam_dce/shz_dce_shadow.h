/* SH4ZAM-DCE: shadow-map compare, a batch of points against a depth map

   shz_dce_shadow.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

/** \file    shz_dce_shadow.h
    \brief   For N world points, how lit each one is by a shadow map: 255 lit, 0 shadowed,
             from one FTRV per point and the depth compare of the point's texel (or four texels,
             blended).

    The map is a size x size grid of 16-bit depths (0 is empty; a bigger depth is nearer the light)
    and a 4 x 4 matrix M that takes a world point (x, y, z, 1) to (X, Y, K, W): the map's texel
    coordinates are (X / W + 1) size / 2 on each axis, and K is the depth. The caller loads M into
    XMTRX (the matrix's 16 floats in the layout shz_xmtrx_load_unaligned_4x4 reads), so one FTRV gives
    (X, Y, K, W) for a point.

    Per point, in this order (the surface is a plane with two unit axes t1, t2, ground in x and z):
    1. (X, Y, K, W) = FTRV of (x, y, z, 1). W not finite, or |W| at or under w_eps (the light's own
       plane): the point is lit.
    2. iw = 1 / W, by a divide that keeps the sign of W (W may be negative: the point is on the far
       side of the light's singularity, and the map still holds it).
    3. Bias: bias = bias_a, or with bias_b != 0, bias_a + bias_b x texel, where texel is the size in
       metres of one texel on the surface (a forward difference of step texel_step along each axis;
       a larger measure counts as texel_max). The two axes come in already transformed (g1, g2:
       FTRV of t1 and t2 with w = 0), once per batch, because a surface's axes are the same for
       every point of a mesh.
    4. Texel coordinates tx, ty of the point and the depth gradient across one texel, kx and ky,
       from the 2 x 2 solve of the point's and the axes' transforms (the depth and the texel
       coordinates are each affine in the surface, so the solve is exact in exact arithmetic). A
       degenerate solve (the surface edge-on to the map: |det| at or under 1e-12, or not finite)
       falls back to the point-only compare, with no slope. Identical axes (the same surface
       twice) fall back too, tested on the axes: their determinant is zero, and the SH-4's
       contracted products would leave a rounding residual that the 1e-12 test misses.
    5. A point whose texel coordinates are at or below -1, or at or above size + 1 on either axis, is lit
       (a point between -1 and 0 still reads texel -1, which is outside the map: lit as well).
    6. Reference depth k0 = K + bias x key_scale. Each texel the point reads (one, or the four round
       it with pcf = 1) gets its own reference: k0 plus the texel centre's offset from the point
       times the gradient (kx, ky), clamped to +- slope_max x key_scale, then clamped to 0 .. 65535
       and truncated. The texel is lit (1) when its stored depth is not nearer the light than that
       reference (stored <= reference); a texel outside the map is lit.
    7. pcf = 1 blends the four taps as lerp(lerp(t0, t1, fx), lerp(t2, t3, fx), fy), so four equal
       taps give exactly that tap's value.
    8. The byte is value x 255 + 0.5, truncated.

    The map's matrix is in XMTRX on entry and is still there on exit: the routine reads XMTRX, it
    does not change it. A caller that keeps its own matrix there loads it again before it transforms
    anything else. XMTRX must hold the map's matrix (the caller's load), or the points are transformed
    by whatever it holds. A count of 0 touches nothing and does not read XMTRX.

    Alignment: points are floats on a multiple of 4, their stride a multiple of 4 and at least 12
    bytes (x y z at the start of every record). out is one byte a point at out_stride bytes (any
    address, any stride of at least 1). g1 and g2 are float[4] (a multiple of 4). depth is uint16_t
    on a multiple of 2, size x size entries, and the map struct is read, not written. A debug build
    asserts the points' and depth's alignment and the stride.

    Error bound: none stated against the exact value. The arithmetic is float. The divides are SH4ZAM's
    shz_divf, which is num x shz_invf(denom): an FSRRA reciprocal on the SH-4, 1 / sqrt(x x) on the
    software back end; neither is an exact quotient. The square root is SH4ZAM's. On the SH-4 the
    point's transform is FTRV, whose bound is (P + |R|) x 2^-21 for each result (P the largest product,
    R the result; SH-4 manual). A point whose texel coordinate, depth level or W lies within errors of
    that size of a decision may take either side; the host test measures these margins. The stored
    depth is an integer, so a reference within an ulp of a level can fall either way too.

    Bit-identical: YES to the same compare written point by point with the same operations in the
    same order on the same back end (the software back end: the shadow-map library's planar batch
    path, with the same FTRV, divides, clamps and lerps, and the same bias expression). Checked byte
    for byte (shadow_test: 0 of 96,000 points; the scene harness: 72 configurations) in strict
    builds and with -ffast-math -fno-unsafe-math-optimizations. Under plain -ffast-math it is NOT
    claimed: the unsafe transforms fold SH4ZAM's software reciprocal (1 / sqrt(x x)) differently in
    each inline context, and 30 of 96,000 host points differ (KERNEL_GUIDE.md section 8; not fixed).
    ON THE SH-4: the `_sh4` form is the same C as `_c` (see below), so the two agree there; the FTRV
    is the same instruction the per-point code uses, but the software back end's sums and the FTRV's
    last bits differ, so a host result and an SH-4 result may differ inside the bounds above. Against
    the planar loop on the SH-4, identical axes differ by design (the planar loop's determinant is a
    rounding residual there; the routine takes the fallback).

    The two surface axes are the caller's: pass them already transformed (FTRV of (t, 0) with the map's
    matrix in XMTRX). A surface whose axes are not known per batch cannot use this routine; the
    non-planar (one texel, no slope) compare is a point routine the caller keeps.

    Where it pays, and where it does not (cycles E a point, see the bench in tests/sh4/shadow_bench.c
    and KERNEL_GUIDE.md section 8): the per-point code makes three transforms a point (the point and
    its two axes) and copies each of the results through memory. This routine makes one FTRV a point
    and reads the two axes from a block the caller fills once a batch. The gain is in those transforms
    and copies: at -O3 the loads and stores a point fall from 104 and 41 to 64 and 11; at -O2 the gain
    is small (4%), and for pcf 0 with no bias there is none at -O2. See KERNEL_GUIDE.md section 8 for
    the numbers and what did not pay.

    Origin and credit: the batch form of a planar shadow-map compare from another project; its credit
    is in README.md (Acknowledgments). SH4ZAM's routines are called through their public API only;
    none of its code is copied.

    Version: v0.10
    The effect: nothing of its own; the compare gives the lit value of a shadowed surface's vertex.
    Use it for: shadow maps from a light (a sun, a spot) in a world, for a mesh of many vertices
      over a surface whose two axes are known per batch.
*/

#ifndef SHZ_DCE_SHADOW_H
#define SHZ_DCE_SHADOW_H

#include "shz_dce_cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief A depth map and the tuning its compare takes. All lengths are metres unless marked; the caller owns the depth
    buffer and keeps it for the batch. The routines read it and do not change it. */
typedef struct shz_dce_shadow_map {
    const uint16_t *depth;   /**< \brief size x size texels, row by row (texel (x, y) at y x size + x). 0 is empty; a bigger depth is nearer the light. */
    unsigned size;           /**< \brief Texels a side (at least 2). */
    float key_scale;         /**< \brief Depth units a metre: the reference depth is K + bias x key_scale, and the slope limit is multiplied by it. */
    float slope_max;         /**< \brief Metres: the most depth a surface's slope may add across a texel is slope_max x key_scale. */
    float bias_a;            /**< \brief Metres of bias at a texel size of 0. */
    float bias_b;            /**< \brief Metres of bias per metre of texel size. 0: the texel size is not measured and the bias is bias_a. */
    float texel_max;         /**< \brief Metres: the largest texel size the bias follows (a larger measure counts as this). */
    float texel_step;        /**< \brief Metres: the forward-difference step of the texel size measure. */
    float w_eps;             /**< \brief |W| at or under this is the light's singularity: the point is lit. */
    int pcf;                 /**< \brief 0: the texel the point is in. 1: the four texels round it, each compared, then blended. */
} shz_dce_shadow_map_t;

/* The shared body of the three forms: the planar compare of one point at a time, in the order the algorithm above gives. Each form calls it. */
static inline int shz_dce_shadow_finite_(float x) {   /* a bit test: finite unless the exponent is all ones */
    union { float f; uint32_t u; } v;

    v.f = x;
    return (v.u & 0x7f800000u) != 0x7f800000u;
}

/* 1 lit, 0 shadowed, for the texel (x, y): a texel outside the map is lit. ref: the reference depth, already clamped to 0 .. 65535. */
static inline float shz_dce_shadow_tap_(const shz_dce_shadow_map_t *m, int x, int y, unsigned ref) {
    if (x < 0 || y < 0 || x >= (int)m->size || y >= (int)m->size) return 1.0f;
    return m->depth[(size_t)y * m->size + (unsigned)x] <= ref ? 1.0f : 0.0f;
}

/* The point-only compare (no slope): what the degenerate solve falls back to. h: the point's transform (X, Y, K, W), iw = 1 / W. */
static inline float shz_dce_shadow_lit_(const shz_dce_shadow_map_t *m, const float h[4], float iw, float bias) {
    float tx, ty, fx, fy, k;
    unsigned ref;
    int ix, iy;

    tx = (h[0] * iw + 1.0f) * 0.5f * (float)m->size;
    ty = (h[1] * iw + 1.0f) * 0.5f * (float)m->size;
    if (!(tx > -1.0f) || !(ty > -1.0f) || !(tx < (float)m->size + 1.0f) || !(ty < (float)m->size + 1.0f)) return 1.0f;
    k = shz_clampf(h[2] + bias * m->key_scale, 0.0f, 65535.0f);
    ref = (unsigned)(int)k;
    if (!m->pcf) return shz_dce_shadow_tap_(m, (int)(tx + 1.0f) - 1, (int)(ty + 1.0f) - 1, ref);
    tx += 1.5f; ty += 1.5f;                                 /* texel centres, moved up 2 so the truncation is a floor */
    ix = (int)tx; iy = (int)ty;
    fx = tx - (float)ix; fy = ty - (float)iy;
    ix -= 2; iy -= 2;
    return shz_lerpf(shz_lerpf(shz_dce_shadow_tap_(m, ix, iy, ref), shz_dce_shadow_tap_(m, ix + 1, iy, ref), fx),
                     shz_lerpf(shz_dce_shadow_tap_(m, ix, iy + 1, ref), shz_dce_shadow_tap_(m, ix + 1, iy + 1, ref), fx), fy);
}

/* The surface's texel size in metres at a point: h the point's transform, iw = 1 / W, g1, g2 the axes' transforms (w = 0),
   delta the forward-difference step. The neighbours x + delta t1 and x + delta t2 transform to h + delta g1 and h + delta g2.
   1e30 when the map is degenerate there. */
static inline float shz_dce_shadow_texel_(const shz_dce_shadow_map_t *m, const float h[4], float iw, const float g1[4], const float g2[4],
                                          float delta) {
    const float half = 0.5f * (float)m->size, u0 = h[0] * iw, v0 = h[1] * iw;
    const float a3 = h[3] + delta * g1[3], b3 = h[3] + delta * g2[3];
    float ia, ib, u1, v1, u2, v2, area;

    if (!shz_dce_shadow_finite_(h[3]) || !(h[3] > m->w_eps || h[3] < -m->w_eps)) return 1e30f;
    if (!shz_dce_shadow_finite_(a3) || !(a3 > m->w_eps || a3 < -m->w_eps)) return 1e30f;
    if (!shz_dce_shadow_finite_(b3) || !(b3 > m->w_eps || b3 < -m->w_eps)) return 1e30f;
    ia = shz_divf(1.0f, a3);
    ib = shz_divf(1.0f, b3);
    u1 = ((h[0] + delta * g1[0]) * ia - u0) * half; v1 = ((h[1] + delta * g1[1]) * ia - v0) * half;
    u2 = ((h[0] + delta * g2[0]) * ib - u0) * half; v2 = ((h[1] + delta * g2[1]) * ib - v0) * half;
    area = u1 * v2 - v1 * u2;
    if (area < 0.0f) area = -area;
    if (!(area > 1e-12f)) return 1e30f;
    return shz_sqrtf(shz_divf(delta * delta, area));        /* area > 1e-12: the root's argument is positive */
}

/* The planar compare of one point: h its transform, iw = 1 / W, g1, g2 the axes' transforms, bias the bias in metres. */
static inline float shz_dce_shadow_plane_(const shz_dce_shadow_map_t *m, const float h[4], float iw, const float g1[4], const float g2[4],
                                          float bias) {
    const float half = 0.5f * (float)m->size, cap = m->slope_max * m->key_scale;
    float s[3][3], det, kx, ky, tx, ty, fx, fy, k0, tap[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    int i, ix, iy, taps;

    s[0][0] = (h[0] * iw + 1.0f) * half; s[0][1] = (h[1] * iw + 1.0f) * half; s[0][2] = h[2];
    for (i = 1; i < 3; i++) {                                /* texels and depth a metre along each axis: d(X / W) = (dX - (X / W) dW) / W */
        const float *g = i == 1 ? g1 : g2;

        s[i][0] = (g[0] - h[0] * iw * g[3]) * iw * half;
        s[i][1] = (g[1] - h[1] * iw * g[3]) * iw * half;
        s[i][2] = g[2];
    }
    /* identical axes (the same surface twice) have a zero determinant in exact arithmetic; the SH-4's contracted products leave a
       rounding residual there that the 1e-12 test below does not catch, so the axes themselves are tested first. Software: no change. */
    if (g1[0] == g2[0] && g1[1] == g2[1] && g1[2] == g2[2] && g1[3] == g2[3]) return shz_dce_shadow_lit_(m, h, iw, bias);
    det = s[1][0] * s[2][1] - s[1][1] * s[2][0];
    if (!shz_dce_shadow_finite_(det) || !(det > 1e-12f || det < -1e-12f)) return shz_dce_shadow_lit_(m, h, iw, bias);
    det = shz_divf(1.0f, det);
    kx = (s[1][2] * s[2][1] - s[2][2] * s[1][1]) * det;      /* depth units a texel, along the map's x and y */
    ky = (s[2][2] * s[1][0] - s[1][2] * s[2][0]) * det;
    tx = s[0][0]; ty = s[0][1];
    if (!(tx > -1.0f) || !(ty > -1.0f) || !(tx < (float)m->size + 1.0f) || !(ty < (float)m->size + 1.0f)) return 1.0f;
    k0 = s[0][2] + bias * m->key_scale;
    if (m->pcf) {
        tx += 1.5f; ty += 1.5f;
        ix = (int)tx; iy = (int)ty;
        fx = tx - (float)ix; fy = ty - (float)iy;
        ix -= 2; iy -= 2;
        tx -= 1.5f; ty -= 1.5f;
        taps = 4;
    } else {
        ix = (int)(tx + 1.0f) - 1; iy = (int)(ty + 1.0f) - 1;
        fx = fy = 0.0f;
        taps = 1;
    }
    for (i = 0; i < taps; i++) {
        const int cx = ix + (i & 1), cy = iy + (i >> 1);
        const float slope = shz_clampf(kx * ((float)cx + 0.5f - tx) + ky * ((float)cy + 0.5f - ty), -cap, cap);

        tap[i] = shz_dce_shadow_tap_(m, cx, cy, (unsigned)(int)shz_clampf(k0 + slope, 0.0f, 65535.0f));
    }
    if (!m->pcf) return tap[0];
    return shz_lerpf(shz_lerpf(tap[0], tap[1], fx), shz_lerpf(tap[2], tap[3], fx), fy);   /* four equal taps give that tap exactly */
}

/* The whole batch, one point at a time. The three forms below all call it: on the SH-4 the transform is FTRV through SH4ZAM. */
static inline void shz_dce_shadow_body_(uint8_t *SHZ_RESTRICT out, size_t out_stride, const float *SHZ_RESTRICT points, size_t stride,
                                        size_t count, const shz_dce_shadow_map_t *SHZ_RESTRICT m, const float g1[4], const float g2[4]) {
    size_t i;

    for (i = 0; i < count; i++) {
        const float *x = (const float *)((const char *)points + i * stride);
        const shz_vec4_t t = shz_xmtrx_transform_vec4(shz_vec4_init(x[0], x[1], x[2], 1.0f));
        const float h[4] = { t.x, t.y, t.z, t.w };
        float v;

        if (!shz_dce_shadow_finite_(h[3]) || !(h[3] > m->w_eps || h[3] < -m->w_eps)) {
            v = 1.0f;
        } else {
            const float iw = shz_divf(1.0f, h[3]);
            float bias = m->bias_a;

            if (m->bias_b != 0.0f) {
                float texel = shz_dce_shadow_texel_(m, h, iw, g1, g2, m->texel_step);

                if (!(texel < m->texel_max)) texel = m->texel_max;
                bias = m->bias_a + m->bias_b * texel;
            }
            v = shz_dce_shadow_plane_(m, h, iw, g1, g2, bias);
        }
        out[i * out_stride] = (uint8_t)(int)(v * 255.0f + 0.5f);
    }
}

/** \brief shz_dce_shadow_compare_batch() in plain C on SH4ZAM's public API: the software back end, and the reference the host test checks against.
    Inputs: XMTRX holds the map's matrix (the caller loaded it); points, stride, count, the map, g1 and g2 (the two surface axes already transformed:
    FTRV of (t, 0) with the same matrix). Overwrites: out[0 .. count - 1] (one byte each at out_stride); XMTRX is read, not changed.
    Alignment: points on a multiple of 4, stride a multiple of 4 and at least 12; map->depth on a multiple of 2; g1, g2 on a multiple of 4.
    Error bound: see the file comment (the software back end is plain float arithmetic; no bound against an exact value is stated beyond
    the decision margins there). Bit-identical to the planar compare of the shadow-map library, point by point, on the software back end:
    YES in strict builds and with -ffast-math -fno-unsafe-math-optimizations; NOT claimed under plain -ffast-math (file comment).
    Cycles E a point (interpreter model, pcf 1, bias 1.5): 489 at -O3 -ffast-math against 634 for the planar loop; KERNEL_GUIDE.md section 8. */
static inline void shz_dce_shadow_compare_batch_c(uint8_t *SHZ_RESTRICT out, size_t out_stride, const float *SHZ_RESTRICT points, size_t stride,
                                                  size_t count, const shz_dce_shadow_map_t *SHZ_RESTRICT map, const float g1[4], const float g2[4]) SHZ_NOEXCEPT {
    shz_dce_shadow_body_(out, out_stride, points, stride, count, map, g1, g2);
}

#if SHZ_DCE_SH4
/** \brief shz_dce_shadow_compare_batch() on the SH-4: the same C as the plain form (no asm). The point's transform is FTRV: SH4ZAM's
    shz_xmtrx_transform_vec4 is the FTRV instruction in this build, so the body needs no hand asm. Same contract as the plain form: XMTRX read,
    not changed. Not bit-identical to the software back end's sums (FTRV's last bits; the file comment has the bound). The divides stay sign-safe
    (shz_divf: W, the texel measure, the solve's determinant may be negative); no fast reciprocal is used, because each denominator's sign is not
    proven positive, except the texel area, which is positive and still divided. Cycles E a point: the same as the plain form (the same
    code): 489 at -O3 -ffast-math, 647 at -O2 (KERNEL_GUIDE.md section 8). */
static inline void shz_dce_shadow_compare_batch_sh4(uint8_t *SHZ_RESTRICT out, size_t out_stride, const float *SHZ_RESTRICT points, size_t stride,
                                                    size_t count, const shz_dce_shadow_map_t *SHZ_RESTRICT map, const float g1[4], const float g2[4]) SHZ_NOEXCEPT {
    shz_dce_shadow_body_(out, out_stride, points, stride, count, map, g1, g2);
}
#endif

/** \brief The shadow compare of a batch of points: out[i] gets point i's lit value, 255 lit .. 0 shadowed, the algorithm in the file comment.
    XMTRX must hold the map's matrix (the caller loaded it before the call); it is read, not changed, so the caller may keep its own matrix there
    only by loading it again. g1, g2: the surface's two axes, FTRV of (t1, 0) and (t2, 0) with that matrix, computed once per batch.
    points: floats on a multiple of 4; stride: bytes from one record to the next, a multiple of 4, at least 12. out: one byte a point at out_stride
    bytes. map->depth: uint16_t on a multiple of 2, size x size. count 0 touches nothing.
    Bit-identical to the shadow-map library's planar compare point by point on the software back end (YES in strict builds and with
    -ffast-math -fno-unsafe-math-optimizations; not under plain -ffast-math, file comment); on the SH-4 the same C, see the file comment for the FTRV bound. Cycles E a point: 489 at -O3 -ffast-math,
    647 at -O2, against the planar loop's 634 and 677 (KERNEL_GUIDE.md section 8). */
static inline void shz_dce_shadow_compare_batch(uint8_t *SHZ_RESTRICT out, size_t out_stride, const float *SHZ_RESTRICT points, size_t stride,
                                                size_t count, const shz_dce_shadow_map_t *SHZ_RESTRICT map, const float g1[4], const float g2[4]) SHZ_NOEXCEPT {
    if (!count)
        return;
    SHZ_DCE_ASSERT_ALIGNED(points, 4);
    SHZ_DCE_ASSERT_ALIGNED(map->depth, 2);
    assert(stride >= 12u && !(stride & 3u));
    assert(map->size >= 2u && out_stride >= 1u);
#if SHZ_DCE_SH4
    shz_dce_shadow_compare_batch_sh4(out, out_stride, points, stride, count, map, g1, g2);
#else
    shz_dce_shadow_compare_batch_c(out, out_stride, points, stride, count, map, g1, g2);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_SHADOW_H */
