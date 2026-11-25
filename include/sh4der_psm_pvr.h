/* SH4DER-PVR: perspective shadow map, the PVR draws the map

   sh4der_psm_pvr.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_psm_pvr.h
    \brief   The SH-4's half of a shadow map the PVR draws: the casters as triangles ready
             for the PVR (clipped, in map pixels with 1 / w), and the lookup coordinates
             of a ground vertex. Nothing here fills a depth buffer or compares a depth.

    The other way to use `sh4der_psm.h` is `sh4der_psm_raster.h`: the SH-4 draws a depth
    map in RAM and compares it per receiver vertex, so a model can shadow itself. This
    file is the PVR route, to be measured against it:

    - The PVR draws the casters from the light into a small render target, flat dark on
      white. It does the filling, so the SH-4 only transforms and clips the caster
      vertices (`sh4der_psm_pvr_tris`).
    - The ground is a layer drawn after it, a multiply blend, with the target as its
      texture. Each ground vertex carries its map coordinates (`sh4der_psm_pvr_uv`),
      and the PVR looks the target up per pixel, filtered.
    - There is no depth compare on the PVR, so a model does not shadow itself on this
      route: casters and receivers are separate sets (cars on a road).
    - The PVR has no q texture coordinate, and the lookup of a perspective map is
      projective in world space. It interpolates the coordinates of the three vertices
      of a triangle, so a lookup is exact at the vertices and only close inside.
      A uniform map is affine in world space and exact. A perspective shadow map
      needs the ground cut into cells small enough:
      `sh4der_psm_pvr_uv_error` measures the error of one triangle in texels.
    - A caster triangle that crosses the light's singularity is clipped in homogeneous
      form, twice (the part with w > 0, and the part with w < 0 negated), as the
      raster does. The PVR cannot take a vertex with w <= 0.

    No allocation, no globals. The caller owns the arrays. Version: v0.10

    \author Jilted Valkyrie
*/

#ifndef SH4DER_PSM_PVR_H
#define SH4DER_PSM_PVR_H

#include "sh4der_psm.h"
#include "sh4der_psm_raster.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_PSM_PVR_MAX_TRIS   10   /**< \brief Most triangles one world triangle gives (two parts of up to 5 each). */
#define SH4DER_PSM_PVR_UV_GRID    5    /**< \brief The error probe: points a side of the barycentric grid. */
/** @} */

/** \brief A vertex for the PVR: map pixels (0 .. size, y up like the raster's) and 1 / w, always positive. */
typedef struct sh4der_psm_pvr_vtx {
    float x, y, invw;
} sh4der_psm_pvr_vtx_t;

/* The part of a triangle with W > 0 (sign 1) or W < 0 (sign -1), clipped to the map, as a fan. Returns triangles written. */
static inline unsigned sh4der_psm_pvr_part_(const float *h0, const float *h1, const float *h2, float sign, unsigned size,
                                            sh4der_psm_pvr_vtx_t out[][3]) {
    const float *h[3];
    float a[8][4], b[8][4], half = 0.5f * (float)size;
    float (*v)[4];
    sh4der_psm_pvr_vtx_t s[8];
    unsigned n = 3u, i, and_code = 15u, or_code = 0u;
    int k;

    h[0] = h0; h[1] = h1; h[2] = h2;
    for (i = 0; i < 3u; i++) {
        unsigned code;

        for (k = 0; k < 4; k++) {
            a[i][k] = h[i][k] * sign;
            if (!sh4der_finite_(a[i][k])) return 0;
        }
        code = (a[i][3] + a[i][0] < 0.0f ? 1u : 0u) | (a[i][3] - a[i][0] < 0.0f ? 2u : 0u)
             | (a[i][3] + a[i][1] < 0.0f ? 4u : 0u) | (a[i][3] - a[i][1] < 0.0f ? 8u : 0u);
        and_code &= code; or_code |= code;
    }
    if (and_code) return 0;
    v = a;
    if (or_code) v = sh4der_psm_raster_clip_(a, b, &n);
    if (n < 3u) return 0;
    for (i = 0; i < n; i++) {
        float iw;

        if (!(v[i][3] > SH4DER_PSM_TRI_EPS)) return 0;
        iw = sh4der_div_(1.0f, v[i][3]);
        s[i].x = (v[i][0] * iw + 1.0f) * half;
        s[i].y = (v[i][1] * iw + 1.0f) * half;
        s[i].invw = iw;
    }
    for (i = 1; i + 1u < n; i++) {
        out[i - 1u][0] = s[0]; out[i - 1u][1] = s[i]; out[i - 1u][2] = s[i + 1u];
    }
    return n - 2u;
}

/** \brief One world triangle into the triangles the PVR draws into the map target: clipped, in map pixels. `out` holds
    SH4DER_PSM_PVR_MAX_TRIS triangles. Returns the count (0: nothing of it lands in the map). Draw them with culling off. */
static inline unsigned sh4der_psm_pvr_tris(const sh4der_psm_t *map, unsigned size, const float a[3], const float b[3],
                                           const float c[3], sh4der_psm_pvr_vtx_t out[][3]) {
    float h[3][4];
    unsigned n = 0;
    int pos, neg;

    if (!map || !a || !b || !c || !out || size < 2u) return 0;
    sh4der_psm_xform(map->m, a, 1.0f, h[0]);
    sh4der_psm_xform(map->m, b, 1.0f, h[1]);
    sh4der_psm_xform(map->m, c, 1.0f, h[2]);
    pos = h[0][3] > 0.0f || h[1][3] > 0.0f || h[2][3] > 0.0f;
    neg = h[0][3] < 0.0f || h[1][3] < 0.0f || h[2][3] < 0.0f;
    if (pos) n += sh4der_psm_pvr_part_(h[0], h[1], h[2], 1.0f, size, out + n);
    if (neg) n += sh4der_psm_pvr_part_(h[0], h[1], h[2], -1.0f, size, out + n);
    return n;
}

/** \brief A ground vertex's texture coordinates in the map target, 0 .. 1 (v up). 0 when the vertex is at the singularity. */
static inline int sh4der_psm_pvr_uv(const sh4der_psm_t *map, const float x[3], float *u, float *v) {
    float mu, mv, w;

    if (!sh4der_psm_map(map, x, &mu, &mv, &w)) return 0;
    *u = 0.5f * mu + 0.5f;
    *v = 0.5f * mv + 0.5f;
    return 1;
}

/** \brief The most a triangle's interpolated lookup is off the exact projective one, in texels of a `size` map, over a
    grid of points inside it. 0 for a uniform map (affine); grows with the triangle's size and with the warp. Returns
    1e30 when a vertex or probe is at the singularity. */
static inline float sh4der_psm_pvr_uv_error(const sh4der_psm_t *map, unsigned size, const float a[3], const float b[3], const float c[3]) {
    const float *vtx[3];
    float uv[3][2], worst = 0.0f;
    unsigned i, j;
    int k;

    vtx[0] = a; vtx[1] = b; vtx[2] = c;
    for (k = 0; k < 3; k++) if (!sh4der_psm_pvr_uv(map, vtx[k], &uv[k][0], &uv[k][1])) return 1e30f;
    for (i = 0; i < SH4DER_PSM_PVR_UV_GRID; i++)
        for (j = 0; i + j < SH4DER_PSM_PVR_UV_GRID; j++) {
            const float l1 = (float)i / (float)(SH4DER_PSM_PVR_UV_GRID - 1u), l2 = (float)j / (float)(SH4DER_PSM_PVR_UV_GRID - 1u), l0 = 1.0f - l1 - l2;
            float p[3], u, v, lu, lv, e;

            for (k = 0; k < 3; k++) p[k] = a[k] * l0 + b[k] * l1 + c[k] * l2;
            if (!sh4der_psm_pvr_uv(map, p, &u, &v)) return 1e30f;
            lu = uv[0][0] * l0 + uv[1][0] * l1 + uv[2][0] * l2;
            lv = uv[0][1] * l0 + uv[1][1] * l1 + uv[2][1] * l2;
            e = (lu > u ? lu - u : u - lu);
            if ((lv > v ? lv - v : v - lv) > e) e = lv > v ? lv - v : v - lv;
            e *= (float)size;
            if (e > worst) worst = e;
        }
    return worst;
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_PSM_PVR_H */
