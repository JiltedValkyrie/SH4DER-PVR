/* SH4DER-PVR: software shadow map for the SH-4

   sh4der_psm_raster.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_psm_raster.h
    \brief   The shadow map itself, in RAM: caster triangles drawn as depth into a small
             16-bit buffer the caller owns, and a compare per receiver vertex that gives
             0 .. 1 for the vertex colour. The PVR draws nothing extra and compares nothing.

    `sh4der_psm.h` says where the texels go (a uniform map or a perspective shadow map).
    This file fills that map and reads it:

    - The depth is `sh4der_psm_depth`: the distance along the light in world metres,
      scaled into 1 .. 65535 over a range the caller gives (`key_lo` .. `key_hi`, from
      the casters' bounds: `sh4der_psm_raster_key_range`). Bigger is nearer the light,
      and 0 is an empty texel. It is linear in world position, so it is carried through
      the map's projection as depth / w and w, both affine in the map, and divided per
      texel (not at all when w is the same at the three vertices: a uniform map).
    - A caster triangle is clipped to the map's square in homogeneous form, and twice:
      the part with w > 0 and the part with w < 0. Both parts land on the right texels
      (a projective line through the light does not care which side of the singularity
      a point is on), so a triangle that crosses the light's singularity or the camera's
      plane is drawn whole. Nothing is dropped and no bounding box runs away.
    - A receiver vertex is lit when no stored depth at its texel is nearer the light than
      its own depth plus a bias in metres (`sh4der_psm_bias`: a + b x the ground a texel
      covers there). `pcf` blends the four texels round the point, compare first.

    Send the result as the vertex colour (or its alpha) of the receiver: the slot
    SH4DOW-DC's visibility and `sh4der_sss_rgb`'s `visibility` take.

    Cost: RAM size x size x 2 bytes (128 x 128: 32 KB), VRAM 0. CPU: one 4 x 4 row
    transform a caster vertex (one FTRV a vertex in a batch under SH4ZAM), a clip a
    triangle that leaves the map, three adds and one reciprocal a covered texel; a
    receiver vertex takes three transforms (the point and its two axes: the texel size
    and the plane compare share them), a square root when the bias follows the texel
    size, and one to four reads. No allocation, no globals. The batch paths (the
    casters' `sh4der_psm_raster_xform_n`, the receivers' `sh4der_psm_raster_lit_n` with
    axes) load the map's matrix into XMTRX under SH4ZAM and leave it there.

    Version: v0.10
    The effect: shadows from a small depth map with self-shadowing, on the CPU.
    Developed for: the software shadow demo in demo/psm.
    Use it for: a few low-detail casters over ground whose vertices are dense enough to carry the shadow's edge.

    \author Jilted Valkyrie

    PVR terms (TA, TR/OP list, EOL, twiddled, direct render): see the PVR glossary in README.md.
*/

#ifndef SH4DER_PSM_RASTER_H
#define SH4DER_PSM_RASTER_H

#include <stdint.h>
#include "sh4der_psm.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_PSM_RASTER_SIZE_MAX   1024u     /**< \brief Largest map side, texels. */
#define SH4DER_PSM_RASTER_EDGE_EPS   1.0e-3f   /**< \brief A texel centre this far outside an edge (edge function units) is still covered: no cracks between a clipped triangle's parts. */
#define SH4DER_PSM_RASTER_DELTA      0.02f     /**< \brief Step of the texel size measure, metres. */
#define SH4DER_PSM_RASTER_TEXEL_MAX  5.0f      /**< \brief Largest texel size the bias follows, metres. */
#define SH4DER_PSM_RASTER_SLOPE_MAX  8.0f      /**< \brief Most depth a surface's slope may add across a texel in the plane compare, metres. */
/** @} */

/** \brief A shadow map being filled or read. */
typedef struct sh4der_psm_raster {
    sh4der_psm_t map;      /**< the placement (a copy) */
    float m[16];           /**< world (x, y, z, 1) -> (X, Y, K, W): the map's rows with the depth scale K in the third column */
    uint16_t *depth;       /**< size x size, the caller's; 0 is empty, bigger is nearer the light */
    unsigned size;         /**< texels a side */
    float key_scale;       /**< depth units a metre */
    unsigned tris, drawn;  /**< triangles given, and those with at least one part inside the map */
} sh4der_psm_raster_t;

/** \brief The range of `sh4der_psm_depth` over `n` points at `stride` bytes (the casters' bounds): what
    `sh4der_psm_raster_begin` takes. Returns 0 for no points. */
static inline int sh4der_psm_raster_key_range(const float light[3], const float *pts, unsigned stride, unsigned n, float *lo, float *hi) {
    float a = 1e30f, b = -1e30f;
    unsigned i;

    if (!light || !pts || !lo || !hi || n < 1u || stride < 3u * sizeof(float)) return 0;
    for (i = 0; i < n; i++) {
        const float k = sh4der_dot3_(light, (const float *)((const char *)pts + (size_t)i * stride));

        if (k < a) a = k;
        if (k > b) b = k;
    }
    *lo = a; *hi = b;
    return 1;
}

/** \brief Start a map: keep the placement, take the caller's buffer (size x size uint16_t) and clear it.
    `key_lo` .. `key_hi`: the casters' range of `sh4der_psm_depth`, metres; a caster outside it is held at its end.
    Returns 0 for a bad input (draw no shadow). */
static inline int sh4der_psm_raster_begin(sh4der_psm_raster_t *r, const sh4der_psm_t *map, uint16_t *depth, unsigned size,
                                          float key_lo, float key_hi) {
    unsigned i;
    int k;

    if (!r || !map || !depth || size < 2u || size > SH4DER_PSM_RASTER_SIZE_MAX) return 0;
    if (!sh4der_finite_(key_lo) || !sh4der_finite_(key_hi) || !(key_hi > key_lo)) return 0;
    r->map = *map;
    r->depth = depth;
    r->size = size;
    r->key_scale = sh4der_div_(65534.0f, key_hi - key_lo);
    for (i = 0; i < 16u; i++) r->m[i] = map->m[i];
    for (k = 0; k < 3; k++) r->m[k * 4 + 2] = map->light[k] * r->key_scale;     /* K = 1 + (depth - lo) x scale */
    r->m[14] = 1.0f - key_lo * r->key_scale;
    for (i = 0; i < size * size; i++) depth[i] = 0u;
    r->tris = r->drawn = 0u;
    return 1;
}

/** \brief A world point in the map's homogeneous form: (X, Y, K, W). */
static inline void sh4der_psm_raster_xform(const sh4der_psm_raster_t *r, const float x[3], float out[4]) {
    sh4der_psm_xform(r->m, x, 1.0f, out);
}

/* SH4ZAM: load the map's matrix into XMTRX. r->m is row form (p' = (x, y, z, w) x m), and the 16 floats read as the column-
   major matrix XMTRX holds are its transpose, so one FTRV per point gives p' (the sw back end and the plain path agree). */
static inline void sh4der_psm_raster_load_(const sh4der_psm_raster_t *r) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    shz_xmtrx_load_unaligned_4x4(r->m);
#else
    (void)r;
#endif
}

/* One point through the map's matrix: out = (p, w_in) x r->m. Under SH4ZAM an FTRV; the matrix must be loaded first. */
static inline void sh4der_psm_raster_fwd_(const sh4der_psm_raster_t *r, const float p[3], float w_in, float out[4]) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    const shz_vec4_t v = shz_xmtrx_transform_vec4(shz_vec4_init(p[0], p[1], p[2], w_in));

    out[0] = v.x; out[1] = v.y; out[2] = v.z; out[3] = v.w;
#else
    sh4der_psm_xform(r->m, p, w_in, out);
#endif
    (void)r;
}

/** \brief `n` world points at `stride` bytes into `out` (n float[4]): transform a mesh once, then
    `sh4der_psm_raster_tri_h` per triangle. Under SH4ZAM the matrix is loaded into XMTRX once and each point is one FTRV;
    XMTRX holds the map's matrix afterwards, so a caller that keeps its own matrix there loads it again before it
    transforms anything else. Plain C otherwise, the same arithmetic. */
static inline void sh4der_psm_raster_xform_n(const sh4der_psm_raster_t *r, const float *pos, unsigned stride, unsigned n, float out[][4]) {
    unsigned i;

    sh4der_psm_raster_load_(r);
    for (i = 0; i < n; i++) sh4der_psm_raster_fwd_(r, (const float *)((const char *)pos + (size_t)i * stride), 1.0f, out[i]);
}

/* One triangle in texels: s = (x, y, 1 / w, K / w), w > 0. Either winding. */
static inline void sh4der_psm_raster_px_(sh4der_psm_raster_t *r, const float *s0, const float *s1, const float *s2) {
    const float n = (float)r->size;
    float area = (s1[0] - s0[0]) * (s2[1] - s0[1]) - (s2[0] - s0[0]) * (s1[1] - s0[1]);
    float lo_x, hi_x, lo_y, hi_y, inv, e0x, e1x, e2x, a0, a1, a2, k0, k1, k2, dwx, dkx;
    int x0, x1, y0, y1, x, y, flat;

    if (area < 0.0f) { const float *t = s1; s1 = s2; s2 = t; area = -area; }
    if (!(area > 1e-9f) || !sh4der_finite_(area)) return;
    lo_x = s0[0] < s1[0] ? s0[0] : s1[0]; if (s2[0] < lo_x) lo_x = s2[0];
    hi_x = s0[0] > s1[0] ? s0[0] : s1[0]; if (s2[0] > hi_x) hi_x = s2[0];
    lo_y = s0[1] < s1[1] ? s0[1] : s1[1]; if (s2[1] < lo_y) lo_y = s2[1];
    hi_y = s0[1] > s1[1] ? s0[1] : s1[1]; if (s2[1] > hi_y) hi_y = s2[1];
    if (!(hi_x > 0.0f) || !(hi_y > 0.0f) || !(lo_x < n) || !(lo_y < n)) return;
    x0 = lo_x > 0.5f ? (int)(lo_x - 0.5f) : 0;                 /* texel centres are at + 0.5 */
    y0 = lo_y > 0.5f ? (int)(lo_y - 0.5f) : 0;
    x1 = hi_x < n ? (int)(hi_x + 0.5f) + 1 : (int)r->size;
    y1 = hi_y < n ? (int)(hi_y + 0.5f) + 1 : (int)r->size;
    if (x1 > (int)r->size) x1 = (int)r->size;
    if (y1 > (int)r->size) y1 = (int)r->size;

    /* the weight of vertex i at a point is e_i / area; each e is affine in the point */
    e0x = s1[1] - s2[1];
    e1x = s2[1] - s0[1];
    e2x = s0[1] - s1[1];
    inv = sh4der_div_(1.0f, area);
    flat = s0[2] == s1[2] && s1[2] == s2[2];
    if (flat) {                                                /* one w: the depth itself is affine, no divide a texel */
        const float w = sh4der_div_(1.0f, s0[2]);

        a0 = a1 = a2 = 0.0f;
        k0 = s0[3] * w * inv; k1 = s1[3] * w * inv; k2 = s2[3] * w * inv;
    } else {
        a0 = s0[2] * inv; a1 = s1[2] * inv; a2 = s2[2] * inv;
        k0 = s0[3] * inv; k1 = s1[3] * inv; k2 = s2[3] * inv;
    }
    dwx = e0x * a0 + e1x * a1 + e2x * a2;
    dkx = e0x * k0 + e1x * k1 + e2x * k2;
    for (y = y0; y < y1; y++) {
        const float px = (float)x0 + 0.5f, py = (float)y + 0.5f;
        float e0 = (s1[0] - px) * (s2[1] - py) - (s2[0] - px) * (s1[1] - py);
        float e1 = (s2[0] - px) * (s0[1] - py) - (s0[0] - px) * (s2[1] - py);
        float e2 = (s0[0] - px) * (s1[1] - py) - (s1[0] - px) * (s0[1] - py);
        float iw = e0 * a0 + e1 * a1 + e2 * a2, kw = e0 * k0 + e1 * k1 + e2 * k2;
        uint16_t *row = r->depth + (size_t)y * r->size;

        for (x = x0; x < x1; x++) {
            if (e0 >= -SH4DER_PSM_RASTER_EDGE_EPS && e1 >= -SH4DER_PSM_RASTER_EDGE_EPS && e2 >= -SH4DER_PSM_RASTER_EDGE_EPS) {
                float q = kw;

                if (!flat) q = iw > 1e-30f ? kw * sh4der_recip_pos_(iw) : 0.0f;
                q = sh4der_clamp_(q, 1.0f, 65535.0f);
                if ((uint16_t)q > row[x]) row[x] = (uint16_t)q;
            }
            e0 += e0x; e1 += e1x; e2 += e2x; iw += dwx; kw += dkx;
        }
    }
}

/* Clip a polygon (X, Y, K, W) to -W <= X <= W, -W <= Y <= W. `a` holds the input and is used as scratch; the result is
   in the returned pointer (a or b), its count in *n. A triangle gives at most 7 vertices. */
static inline float (*sh4der_psm_raster_clip_(float a[8][4], float b[8][4], unsigned *n))[4] {
    float (*in)[4] = a, (*out)[4] = b;
    unsigned cnt = *n, p, i;
    int k;

    for (p = 0; p < 4u && cnt >= 3u; p++) {
        const int ax = (int)(p >> 1);
        const float sg = (p & 1u) ? -1.0f : 1.0f;
        unsigned m = 0;

        for (i = 0; i < cnt; i++) {
            const float *c = in[i], *d = in[(i + 1u) % cnt];
            const float dc = c[3] + sg * c[ax], dd = d[3] + sg * d[ax];

            if (dc >= 0.0f) { for (k = 0; k < 4; k++) out[m][k] = c[k]; m++; }
            if ((dc >= 0.0f) != (dd >= 0.0f)) {
                const float t = sh4der_div_(dc, dc - dd);

                for (k = 0; k < 4; k++) out[m][k] = c[k] + (d[k] - c[k]) * t;
                m++;
            }
        }
        cnt = m;
        { float (*t)[4] = in; in = out; out = t; }
    }
    *n = cnt;
    return in;
}

/* The part of a triangle with W > 0 (sign 1) or W < 0 (sign -1), clipped to the map and drawn. 1 when something was. */
static inline int sh4der_psm_raster_part_(sh4der_psm_raster_t *r, const float *h0, const float *h1, const float *h2, float sign) {
    const float *h[3];
    float a[8][4], b[8][4], s[8][4], half = 0.5f * (float)r->size;
    float (*v)[4];
    unsigned n = 3u, i, and_code = 15u, or_code = 0u;
    int k;

    h[0] = h0; h[1] = h1; h[2] = h2;
    for (i = 0; i < 3u; i++) {
        unsigned code;

        a[i][0] = h[i][0] * sign; a[i][1] = h[i][1] * sign; a[i][2] = h[i][2] * sign; a[i][3] = h[i][3] * sign;
        for (k = 0; k < 4; k++) if (!sh4der_finite_(a[i][k])) return 0;
        code = (a[i][3] + a[i][0] < 0.0f ? 1u : 0u) | (a[i][3] - a[i][0] < 0.0f ? 2u : 0u)
             | (a[i][3] + a[i][1] < 0.0f ? 4u : 0u) | (a[i][3] - a[i][1] < 0.0f ? 8u : 0u);
        and_code &= code; or_code |= code;
    }
    if (and_code) return 0;                                    /* all three outside one side */
    v = a;
    if (or_code) v = sh4der_psm_raster_clip_(a, b, &n);
    if (n < 3u) return 0;
    for (i = 0; i < n; i++) {
        float iw;

        if (!(v[i][3] > SH4DER_PSM_TRI_EPS)) return 0;         /* the light's own point: not drawn */
        iw = sh4der_div_(1.0f, v[i][3]);
        s[i][0] = (v[i][0] * iw + 1.0f) * half;
        s[i][1] = (v[i][1] * iw + 1.0f) * half;
        s[i][2] = iw;
        s[i][3] = v[i][2] * iw;
    }
    for (i = 1; i + 1u < n; i++) sh4der_psm_raster_px_(r, s[0], s[i], s[i + 1u]);
    return 1;
}

/** \brief Draw one caster triangle given in the map's homogeneous form (`sh4der_psm_raster_xform`). Either winding;
    a triangle across the light's singularity is drawn in its two parts. */
static inline void sh4der_psm_raster_tri_h(sh4der_psm_raster_t *r, const float h0[4], const float h1[4], const float h2[4]) {
    const int pos = h0[3] > 0.0f || h1[3] > 0.0f || h2[3] > 0.0f;
    const int neg = h0[3] < 0.0f || h1[3] < 0.0f || h2[3] < 0.0f;
    int hit = 0;

    r->tris++;
    if (pos) hit |= sh4der_psm_raster_part_(r, h0, h1, h2, 1.0f);
    if (neg) hit |= sh4der_psm_raster_part_(r, h0, h1, h2, -1.0f);
    if (hit) r->drawn++;
}

/** \brief Draw one caster triangle from world positions. */
static inline void sh4der_psm_raster_tri(sh4der_psm_raster_t *r, const float a[3], const float b[3], const float c[3]) {
    float h[3][4];

    sh4der_psm_xform(r->m, a, 1.0f, h[0]);
    sh4der_psm_xform(r->m, b, 1.0f, h[1]);
    sh4der_psm_xform(r->m, c, 1.0f, h[2]);
    sh4der_psm_raster_tri_h(r, h[0], h[1], h[2]);
}

/* 1 when texel (x, y) holds nothing nearer the light than ref. Outside the map is lit. */
static inline float sh4der_psm_raster_tap_(const sh4der_psm_raster_t *r, int x, int y, unsigned ref) {
    if (x < 0 || y < 0 || x >= (int)r->size || y >= (int)r->size) return 1.0f;
    return r->depth[(size_t)y * r->size + (unsigned)x] <= ref ? 1.0f : 0.0f;
}

/** \brief How lit a world point is: 1 lit .. 0 shadowed. `bias_m`: metres toward the light (`sh4der_psm_bias`).
    `pcf` 0 reads the one texel the point is in; 1 blends the four round it (compare first, then the blend).
    A point outside the map, or at the light's singularity, is lit. */
static inline float sh4der_psm_raster_lit(const sh4der_psm_raster_t *r, const float x[3], float bias_m, int pcf) {
    float h[4], iw, tx, ty, fx, fy, k;
    unsigned ref;
    int ix, iy;

    sh4der_psm_xform(r->m, x, 1.0f, h);
    if (!sh4der_finite_(h[3]) || !(h[3] > SH4DER_PSM_TRI_EPS || h[3] < -SH4DER_PSM_TRI_EPS)) return 1.0f;
    iw = sh4der_div_(1.0f, h[3]);
    tx = (h[0] * iw + 1.0f) * 0.5f * (float)r->size;
    ty = (h[1] * iw + 1.0f) * 0.5f * (float)r->size;
    if (!(tx > -1.0f) || !(ty > -1.0f) || !(tx < (float)r->size + 1.0f) || !(ty < (float)r->size + 1.0f)) return 1.0f;
    k = sh4der_clamp_(h[2] + bias_m * r->key_scale, 0.0f, 65535.0f);
    ref = (unsigned)k;
    if (!pcf) return sh4der_psm_raster_tap_(r, (int)(tx + 1.0f) - 1, (int)(ty + 1.0f) - 1, ref);
    tx += 1.5f; ty += 1.5f;                                    /* texel centres, moved up 2 so the truncation is a floor */
    ix = (int)tx; iy = (int)ty;
    fx = tx - (float)ix; fy = ty - (float)iy;
    ix -= 2; iy -= 2;
    return sh4der_lerp_(sh4der_lerp_(sh4der_psm_raster_tap_(r, ix, iy, ref), sh4der_psm_raster_tap_(r, ix + 1, iy, ref), fx),
                        sh4der_lerp_(sh4der_psm_raster_tap_(r, ix, iy + 1, ref), sh4der_psm_raster_tap_(r, ix + 1, iy + 1, ref), fx), fy);
}

static inline float sh4der_psm_raster_plane_(const sh4der_psm_raster_t *r, const float x[3], const float h[4], float iw,
                                             const float g1[4], const float g2[4], float bias_m, int pcf);

/** \brief The same for a point on a surface with unit axes `t1`, `t2` (ground: x and z): each texel read is compared
    with the surface's own depth at that texel's centre, not at the point. A map texel stores one depth for all the
    surface it covers, and in a warped map that can be metres of ground: this takes the surface's slope across the
    texel out of the compare, so `bias_m` stays a few centimetres and a surface drawn into the map does not shadow
    itself. Three transforms a point. Falls back to `sh4der_psm_raster_lit` where the map is degenerate. */
static inline float sh4der_psm_raster_lit_plane(const sh4der_psm_raster_t *r, const float x[3], const float t1[3], const float t2[3],
                                                float bias_m, int pcf) {
    float h[4], g1[4], g2[4], iw;

    sh4der_psm_xform(r->m, x, 1.0f, h);
    sh4der_psm_xform(r->m, t1, 0.0f, g1);
    sh4der_psm_xform(r->m, t2, 0.0f, g2);
    if (!sh4der_finite_(h[3]) || !(h[3] > SH4DER_PSM_TRI_EPS || h[3] < -SH4DER_PSM_TRI_EPS)) return 1.0f;
    iw = sh4der_div_(1.0f, h[3]);
    return sh4der_psm_raster_plane_(r, x, h, iw, g1, g2, bias_m, pcf);
}

/* The plane compare from a point's transform h (X, Y, K, W), iw = 1 / W and its two axes' transforms g1, g2 (w = 0): the
   core of sh4der_psm_raster_lit_plane and of the receivers' batch (sh4der_psm_raster_lit_n). `x` is the point, for the
   fallback. */
static inline float sh4der_psm_raster_plane_(const sh4der_psm_raster_t *r, const float x[3], const float h[4], float iw,
                                             const float g1[4], const float g2[4], float bias_m, int pcf) {
    const float half = 0.5f * (float)r->size, cap = SH4DER_PSM_RASTER_SLOPE_MAX * r->key_scale;
    float s[3][3], det, kx, ky, tx, ty, fx, fy, k0, tap[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    int i, ix, iy, taps;

    s[0][0] = (h[0] * iw + 1.0f) * half; s[0][1] = (h[1] * iw + 1.0f) * half; s[0][2] = h[2];
    for (i = 1; i < 3; i++) {                                  /* texels and depth a metre along each axis: d(X / W) = (dX - (X / W) dW) / W */
        const float *g = i == 1 ? g1 : g2;

        s[i][0] = (g[0] - h[0] * iw * g[3]) * iw * half;
        s[i][1] = (g[1] - h[1] * iw * g[3]) * iw * half;
        s[i][2] = g[2];
    }
    det = s[1][0] * s[2][1] - s[1][1] * s[2][0];
    if (!sh4der_finite_(det) || !(det > 1e-12f || det < -1e-12f)) return sh4der_psm_raster_lit(r, x, bias_m, pcf);
    det = sh4der_div_(1.0f, det);
    kx = (s[1][2] * s[2][1] - s[2][2] * s[1][1]) * det;        /* depth units a texel, along the map's x and y */
    ky = (s[2][2] * s[1][0] - s[1][2] * s[2][0]) * det;
    tx = s[0][0]; ty = s[0][1];
    if (!(tx > -1.0f) || !(ty > -1.0f) || !(tx < (float)r->size + 1.0f) || !(ty < (float)r->size + 1.0f)) return 1.0f;
    k0 = s[0][2] + bias_m * r->key_scale;
    if (pcf) {
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
        const float slope = sh4der_clamp_(kx * ((float)cx + 0.5f - tx) + ky * ((float)cy + 0.5f - ty), -cap, cap);

        tap[i] = sh4der_psm_raster_tap_(r, cx, cy, (unsigned)sh4der_clamp_(k0 + slope, 0.0f, 65535.0f));
    }
    if (!pcf) return tap[0];
    return sh4der_lerp_(sh4der_lerp_(tap[0], tap[1], fx), sh4der_lerp_(tap[2], tap[3], fx), fy);   /* four equal taps give that tap exactly */
}

/* Metres of ground a map texel covers at a point, from its transform h, iw = 1 / W and its two axes' transforms g1, g2
   (w = 0). The neighbours x + delta t1 and x + delta t2 transform to h + delta g1 and h + delta g2 (the transform is linear
   in (x, 1)), so the axes cost no transform of their own. The number sh4der_psm_texel_m gives (a forward difference over
   delta metres), up to rounding. 1e30 when the map is degenerate there. */
static inline float sh4der_psm_raster_texel_(const sh4der_psm_raster_t *r, const float h[4], float iw, const float g1[4],
                                             const float g2[4], float delta) {
    const float half = 0.5f * (float)r->size, u0 = h[0] * iw, v0 = h[1] * iw;
    const float a3 = h[3] + delta * g1[3], b3 = h[3] + delta * g2[3];
    float ia, ib, u1, v1, u2, v2, area;

    if (!sh4der_finite_(h[3]) || !(h[3] > SH4DER_PSM_TRI_EPS || h[3] < -SH4DER_PSM_TRI_EPS)) return 1e30f;
    if (!sh4der_finite_(a3) || !(a3 > SH4DER_PSM_TRI_EPS || a3 < -SH4DER_PSM_TRI_EPS)) return 1e30f;
    if (!sh4der_finite_(b3) || !(b3 > SH4DER_PSM_TRI_EPS || b3 < -SH4DER_PSM_TRI_EPS)) return 1e30f;
    ia = sh4der_div_(1.0f, a3);
    ib = sh4der_div_(1.0f, b3);
    u1 = ((h[0] + delta * g1[0]) * ia - u0) * half; v1 = ((h[1] + delta * g1[1]) * ia - v0) * half;
    u2 = ((h[0] + delta * g2[0]) * ib - u0) * half; v2 = ((h[1] + delta * g2[1]) * ib - v0) * half;
    area = u1 * v2 - v1 * u2;
    if (area < 0.0f) area = -area;
    if (!(area > 1e-12f)) return 1e30f;
    return sqrtf(sh4der_div_(delta * delta, area));
}

/** \brief A receiver mesh: `n` world positions at `stride` bytes -> one byte each at `out_stride` bytes, 255 lit .. 0
    shadowed. With the surface's two unit axes `t1`, `t2` (ground: x and z) each vertex is the plane compare
    (`sh4der_psm_raster_lit_plane`'s, with the same three transforms) with a bias of a + b x the surface a texel covers
    there (b 0 skips that measure); with NULL axes through `sh4der_psm_raster_lit` with a bias of a. Put the byte in the
    vertex colour. Under SH4ZAM the map's matrix is loaded into XMTRX once and each transform is one FTRV, and XMTRX
    holds that matrix afterwards (a caller that keeps its own matrix there loads it again before it transforms anything
    else). Plain C otherwise, the same arithmetic. */
static inline void sh4der_psm_raster_lit_n(const sh4der_psm_raster_t *r, const float *pos, unsigned stride, unsigned n,
                                           const float t1[3], const float t2[3], float a, float b, int pcf,
                                           uint8_t *out, unsigned out_stride) {
    const int planar = t1 && t2;
    unsigned i;

    if (planar) sh4der_psm_raster_load_(r);
    for (i = 0; i < n; i++) {
        const float *x = (const float *)((const char *)pos + (size_t)i * stride);
        float v;

        if (planar) {
            float h[4], g1[4], g2[4], iw, bias = a;

            sh4der_psm_raster_fwd_(r, x, 1.0f, h);
            sh4der_psm_raster_fwd_(r, t1, 0.0f, g1);
            sh4der_psm_raster_fwd_(r, t2, 0.0f, g2);
            if (!sh4der_finite_(h[3]) || !(h[3] > SH4DER_PSM_TRI_EPS || h[3] < -SH4DER_PSM_TRI_EPS)) {
                v = 1.0f;
            } else {
                iw = sh4der_div_(1.0f, h[3]);
                if (b != 0.0f) {
                    float texel = sh4der_psm_raster_texel_(r, h, iw, g1, g2, SH4DER_PSM_RASTER_DELTA);

                    if (!(texel < SH4DER_PSM_RASTER_TEXEL_MAX)) texel = SH4DER_PSM_RASTER_TEXEL_MAX;
                    bias = sh4der_psm_bias(a, b, texel);
                }
                v = sh4der_psm_raster_plane_(r, x, h, iw, g1, g2, bias, pcf);
            }
        } else {
            v = sh4der_psm_raster_lit(r, x, a, pcf);
        }
        out[(size_t)i * out_stride] = (uint8_t)(v * 255.0f + 0.5f);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_PSM_RASTER_H */
