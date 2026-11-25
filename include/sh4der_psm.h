/* SH4DER-PVR: perspective shadow map matrices

   sh4der_psm.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_psm.h
    \brief   Where the texels of a shadow map go: a uniform map for a directional
             light, and a perspective shadow map (PSM) that builds the same map
             in the camera's post-projective space so near ground gets more
             texels than far ground.

    This is the placement only: one world-to-map matrix, `u = x / w`, `v = y / w`,
    both -1 .. 1 across the fit (a rectangle, so the map's texels are not square on the ground). What is drawn into the map, and how it is
    compared, is the client's: a software renderer can store a depth and compare
    it (demo/psm does), the PVR cannot (no depth texture, no per-pixel compare),
    so there the map is a caster silhouette projected on receivers.

    The method is the one of "Perspective Shadow Maps: Care and Feeding"
    (GPU Gems, chapter 14, NVIDIA 2004), reduced to a directional light:

    - The scene is taken to post-projective space of the camera (z' = Q (1 - Zn / z)).
      Parallel light rays become rays through one point L' = (D, 0) x ViewProj
      (a direction when L'.w is 0). A map through L' is a projective map in world
      space, so one 4 x 4 matrix holds all of it.
    - The camera's near plane for this warp (`warp_near`) need not be the render's:
      raising it takes less of the unit cube for the first metre (Q is about 1 and
      the first metre after a 1 m near plane takes half the depth range), so more
      texels are left for the rest. It is the cheapest fix the chapter has.
    - The map is fitted to the receivers' points inside the view frustum (the caller
      lists them; `sh4der_psm_slab_pts` makes the list for ground), off axis, to the
      tight rectangle.
    - A sun behind the camera (L'.w < 0) is the same map: a projective line through
      L' does not care which way round the infinity plane it was reached. What changes
      is which casters land where: a caster's vertices can have either sign of w, and
      a triangle with mixed signs crosses the singularity (the chapter's "inverse
      projection" exists to draw those). Here: draw a triangle when its three w have
      one sign (`sh4der_psm_tri_ok`), and cut or drop the rest. NOT the chapter's
      matrix, which a rasteriser that handles negative w does not need; see test/psm_test.c.
    - The depth to store and compare is `sh4der_psm_depth`: distance along the light,
      in world metres. It is linear in world position, so interpolating it with
      perspective correction across a map triangle is exact, and a bias in metres
      (`sh4der_psm_bias`, the chapter's world-space bias a + b x texel) means the
      same everywhere in the map.

    The equations were checked against the chapter's printed text (NVIDIA's PDF of the
    chapter): Q = Zf / (Zf - Zn) and z' = Q (1 - Zn / z), the inverse projection (Zn = -a,
    Zf = a, z' = (1 + a / z) / 2), the receiver points as the union of (Bi intersect F), the
    world-space bias Pbiased = (P + L (a + b Ltexel)) M. Where this header differs: the fit
    axis is the mean direction (the chapter uses the minimal cone), and the bias is in
    metres (the chapter's Ltexel is a squared length). Pinned in test/psm_test.c against
    the numbers its prose gives (Q 1.0002 at Zn 1, Zf 4000; Q 1.02 at Zf / Zn 50; half
    the unit cube in the first metre).

    Convention: Direct3D-like depth, z' 0 at the near plane and 1 at the far plane (not an OpenGL -1 .. 1 cube); a light
    along the view direction lands at z' = Q, a distance Q - 1 outside the cube's far face. Directional lights only: a
    point light inside the cube has no such fit.

    Row form: every matrix is a float[16], row major, p' = (x, y, z, 1) x M. Compose
    with sh4der_psm_mul (first, then second). No allocation, no globals. A build returns
    0 (draw no map) for a bad input: a light inside the fit, a cone of 90 degrees or
    more, a point behind the camera.

    Version: v0.10
    The effect: sharper shadows near the camera for the same map size.
    Developed for: the software shadow demo in demo/psm.
    Use it for: shadows of a directional light (sun, moon) on ground seen at a low angle.

    \author Jilted Valkyrie

    PVR terms (TA, TR/OP list, EOL, twiddled, direct render): see the PVR glossary in README.md.
*/

#ifndef SH4DER_PSM_H
#define SH4DER_PSM_H

#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_PSM_MAX_PTS   64        /**< \brief Most points a fit takes (a slab clip of a frustum gives 32). */
#define SH4DER_PSM_W_EPS     1.0e-4f   /**< \brief |L'.w| under this share of |L'.xyz|: the light is a direction in post-projective space. */
#define SH4DER_PSM_MIN_COS   0.02f     /**< \brief Least cosine between the cone axis and a point's direction (about 89 degrees). */
#define SH4DER_PSM_MARGIN    1.01f     /**< \brief The fit's square, a little larger than the points. */
#define SH4DER_PSM_TRI_EPS   1.0e-6f   /**< \brief |w| under this is the singularity: a vertex there is not drawn. */
#define SH4DER_PSM_BIAS_A    0.02f     /**< \brief Bias a, metres (the chapter's a). */
#define SH4DER_PSM_BIAS_B    1.5f      /**< \brief Bias b, texels (the chapter's b: times the texel's size in metres). */
/** @} */

/** \brief Which map `sh4der_psm_build_*` made. */
enum {
    SH4DER_PSM_UNIFORM = 0,   /**< world space, parallel rays: an orthographic map */
    SH4DER_PSM_ORTHO,         /**< post-projective space, light parallel to the view plane (L'.w = 0) */
    SH4DER_PSM_POINT_FRONT,   /**< L'.w > 0: the sun is ahead of the camera */
    SH4DER_PSM_POINT_BEHIND   /**< L'.w < 0: the sun is behind the camera */
};

/** \brief A camera: unit forward and up (not parallel), vertical half-angle tangent, width over height. */
typedef struct sh4der_psm_cam {
    float eye[3], fwd[3], up[3];
    float tan_half, aspect, znear, zfar;
} sh4der_psm_cam_t;

/** \brief The result of a build: the matrix and what it was made from. */
typedef struct sh4der_psm {
    float m[16];        /**< world (x, y, z, 1) -> (X, Y, ., W); u = X / W, v = Y / W */
    float light[3];     /**< unit direction from the surface toward the light */
    int   kind;         /**< SH4DER_PSM_* */
} sh4der_psm_t;

/* ---- the chapter's depth numbers --------------------------------------- */

/** \brief Q = Zf / (Zf - Zn): the post-projective depth scale (the chapter's 1.0002 at 1, 4000). */
static inline float sh4der_psm_q(float zn, float zf) { return sh4der_div_(zf, zf - zn); }

/** \brief Post-projective depth of view depth z: Q (1 - Zn / z). 0 at the near plane, 1 at the far. */
static inline float sh4der_psm_post_z(float zn, float zf, float z) {
    return sh4der_psm_q(zn, zf) * (1.0f - sh4der_div_(zn, z));
}

/** \brief The inverse projection's depth (Zn = -a, Zf = +a): (1 + a / z) / 2. 0 at -a, 1 at +a, 1/2 at both infinities. */
static inline float sh4der_psm_inverse_post_z(float a, float z) { return 0.5f * (1.0f + sh4der_div_(a, z)); }

/** \brief Bias in metres: a + b x texel (the chapter's world-space bias; its listing prints the terms the other way round). */
static inline float sh4der_psm_bias(float a, float b, float texel_m) { return a + b * texel_m; }

/** \brief The depth key of a world point: its distance along the light, metres. Bigger is nearer the light. */
static inline float sh4der_psm_depth(const sh4der_psm_t *p, const float x[3]) { return sh4der_dot3_(p->light, x); }

/* ---- 4 x 4 row form ----------------------------------------------------- */

/** \brief out = a then b (row vectors). `out` may be a or b. */
static inline void sh4der_psm_mul(const float a[16], const float b[16], float out[16]) {
    float t[16];
    int r, c;

    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++)
            t[r * 4 + c] = a[r * 4 + 0] * b[0 + c] + a[r * 4 + 1] * b[4 + c] + a[r * 4 + 2] * b[8 + c] + a[r * 4 + 3] * b[12 + c];
    for (r = 0; r < 16; r++) out[r] = t[r];
}

/** \brief (x, y, z, w_in) x m. w_in is 1 for a point, 0 for a direction. */
static inline void sh4der_psm_xform(const float m[16], const float p[3], float w_in, float out[4]) {
    int c;

    for (c = 0; c < 4; c++)
        out[c] = p[0] * m[c] + p[1] * m[4 + c] + p[2] * m[8 + c] + w_in * m[12 + c];
}

/** \brief The camera's world-to-clip rows for a near plane `zn` (the warp's; not the render's): x / (tan a),
    y / tan, Q (z - Zn), w = z. Right = fwd x up. Returns 0 for a bad camera or zn. */
static inline int sh4der_psm_cam_rows(const sh4der_psm_cam_t *cam, float zn, float out[16]) {
    float r[3], u[3], q, sx, sy, e;
    int i;

    if (!cam || !out || !(cam->tan_half > 0.0f) || !(cam->aspect > 0.0f) || !(zn > 0.0f) || !(cam->zfar > zn)) return 0;
    sh4der_cross_(cam->fwd, cam->up, r);
    sh4der_norm3_(r);
    sh4der_cross_(r, cam->fwd, u);
    sh4der_norm3_(u);
    if (sh4der_len3_(r[0], r[1], r[2]) < 0.5f) return 0;
    q = sh4der_psm_q(zn, cam->zfar);
    sx = sh4der_recip_pos_(cam->tan_half * cam->aspect);   /* > 0: the guard above */
    sy = sh4der_recip_pos_(cam->tan_half);
    for (i = 0; i < 3; i++) {
        out[i * 4 + 0] = r[i] * sx;
        out[i * 4 + 1] = u[i] * sy;
        out[i * 4 + 2] = q * cam->fwd[i];
        out[i * 4 + 3] = cam->fwd[i];
    }
    out[12] = -sh4der_dot3_(cam->eye, r) * sx;
    out[13] = -sh4der_dot3_(cam->eye, u) * sy;
    e = sh4der_dot3_(cam->eye, cam->fwd);
    out[14] = q * (-e - zn);
    out[15] = -e;
    return 1;
}

/* ---- the fit ------------------------------------------------------------ */

/* Two unit axes across a unit direction a. */
static inline void sh4der_psm_basis_(const float a[3], float r[3], float u[3]) {
    float h[3] = { 0.0f, 1.0f, 0.0f };

    if (a[1] > 0.9f || a[1] < -0.9f) { h[0] = 1.0f; h[1] = 0.0f; }
    sh4der_cross_(h, a, r);
    sh4der_norm3_(r);
    sh4der_cross_(a, r, u);
    sh4der_norm3_(u);
}

/* Fit a map to world points seen through T (world -> homogeneous). Writes the matrix and kind. 0 on a bad fit. */
static inline int sh4der_psm_fit_(const float T[16], int warped, const float light[3], const float *pts, unsigned n, sh4der_psm_t *out) {
    float lh[4], q[SH4DER_PSM_MAX_PTS][3], a[3], r[3], u[3], lp[3] = { 0, 0, 0 };
    float n_m[16] = { 0 }, lo_s = 1e30f, hi_s = -1e30f, lo_t = 1e30f, hi_t = -1e30f, cu, cv, ext_u, ext_v;
    unsigned i;
    int kind;

    if (!out || !light || !pts || n < 1u || n > SH4DER_PSM_MAX_PTS) return 0;
    sh4der_psm_xform(T, light, 0.0f, lh);
    {
        const float len = sh4der_len3_(lh[0], lh[1], lh[2]);
        const float aw = lh[3] < 0.0f ? -lh[3] : lh[3];

        if (!(len > 1e-20f) || !sh4der_finite_(len) || !sh4der_finite_(lh[3])) return 0;
        kind = !warped ? SH4DER_PSM_UNIFORM
             : (aw * aw < SH4DER_PSM_W_EPS * SH4DER_PSM_W_EPS * len) ? SH4DER_PSM_ORTHO
             : lh[3] > 0.0f ? SH4DER_PSM_POINT_FRONT : SH4DER_PSM_POINT_BEHIND;
    }
    for (i = 0; i < n; i++) {
        float h[4];

        sh4der_psm_xform(T, pts + 3u * i, 1.0f, h);
        if (!(h[3] > 1e-12f) || !sh4der_finite_(h[0]) || !sh4der_finite_(h[1]) || !sh4der_finite_(h[2])) return 0;
        q[i][0] = sh4der_div_(h[0], h[3]); q[i][1] = sh4der_div_(h[1], h[3]); q[i][2] = sh4der_div_(h[2], h[3]);
    }
    if (kind == SH4DER_PSM_UNIFORM || kind == SH4DER_PSM_ORTHO) {
        a[0] = lh[0]; a[1] = lh[1]; a[2] = lh[2];
        sh4der_norm3_(a);
        sh4der_psm_basis_(a, r, u);
        for (i = 0; i < n; i++) {
            const float s = sh4der_dot3_(q[i], r), t = sh4der_dot3_(q[i], u);

            if (s < lo_s) lo_s = s;
            if (s > hi_s) hi_s = s;
            if (t < lo_t) lo_t = t;
            if (t > hi_t) hi_t = t;
        }
    } else {
        float mean[3] = { 0, 0, 0 };
        float e[SH4DER_PSM_MAX_PTS][3];

        lp[0] = sh4der_div_(lh[0], lh[3]); lp[1] = sh4der_div_(lh[1], lh[3]); lp[2] = sh4der_div_(lh[2], lh[3]);
        for (i = 0; i < n; i++) {
            e[i][0] = q[i][0] - lp[0]; e[i][1] = q[i][1] - lp[1]; e[i][2] = q[i][2] - lp[2];
            sh4der_norm3_(e[i]);
            mean[0] += e[i][0]; mean[1] += e[i][1]; mean[2] += e[i][2];
        }
        a[0] = mean[0]; a[1] = mean[1]; a[2] = mean[2];
        if (!(sh4der_len3_(a[0], a[1], a[2]) > 1e-12f)) return 0;
        sh4der_norm3_(a);
        sh4der_psm_basis_(a, r, u);
        for (i = 0; i < n; i++) {
            const float c = sh4der_dot3_(e[i], a);
            float s, t;

            if (!(c > SH4DER_PSM_MIN_COS)) return 0;          /* a cone of about 90 degrees: the light sits inside the fit */
            s = sh4der_div_(sh4der_dot3_(e[i], r), c);
            t = sh4der_div_(sh4der_dot3_(e[i], u), c);
            if (s < lo_s) lo_s = s;
            if (s > hi_s) hi_s = s;
            if (t < lo_t) lo_t = t;
            if (t > hi_t) hi_t = t;
        }
    }
    cu = 0.5f * (lo_s + hi_s);
    cv = 0.5f * (lo_t + hi_t);
    ext_u = 0.5f * (hi_s - lo_s) * SH4DER_PSM_MARGIN;     /* the two sides fitted apart: a long thin fit keeps its texels */
    ext_v = 0.5f * (hi_t - lo_t) * SH4DER_PSM_MARGIN;
    if (!(ext_u > 1e-9f) || !(ext_v > 1e-9f) || !sh4der_finite_(ext_u) || !sh4der_finite_(ext_v)) return 0;
    {
        const float iu = sh4der_recip_pos_(ext_u), iv = sh4der_recip_pos_(ext_v);   /* ext > 1e-9: the guard above */
        int k;

        if (kind == SH4DER_PSM_UNIFORM || kind == SH4DER_PSM_ORTHO) {
            /* X = (q.r - cu) / ext, W = q.w, from the homogeneous point (so the divide gives the post-projective q) */
            for (k = 0; k < 3; k++) { n_m[k * 4 + 0] = r[k] * iu; n_m[k * 4 + 1] = u[k] * iv; }
            n_m[12] = -cu * iu; n_m[13] = -cv * iv;
            n_m[15] = 1.0f;
        } else {
            /* d = q_h.xyz - L' q_h.w (the point seen from L'); X = (d.r - cu d.a) / ext, W = d.a */
            const float lr = sh4der_dot3_(lp, r), lu = sh4der_dot3_(lp, u), la = sh4der_dot3_(lp, a);

            for (k = 0; k < 3; k++) {
                n_m[k * 4 + 0] = (r[k] - cu * a[k]) * iu;
                n_m[k * 4 + 1] = (u[k] - cv * a[k]) * iv;
                n_m[k * 4 + 3] = a[k];
            }
            n_m[12] = (cu * la - lr) * iu;
            n_m[13] = (cv * la - lu) * iv;
            n_m[15] = -la;
        }
    }
    sh4der_psm_mul(T, n_m, out->m);
    out->light[0] = light[0]; out->light[1] = light[1]; out->light[2] = light[2];
    sh4der_norm3_(out->light);
    out->kind = kind;
    return 1;
}

/** \brief The baseline: a uniform (orthographic) map for a directional light, fitted to `pts` (n float[3]).
    `light` is the direction from the surface toward the light. Returns 0 for a bad input. */
static inline int sh4der_psm_build_uniform(sh4der_psm_t *out, const float light[3], const float *pts, unsigned n) {
    static const float id[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

    return sh4der_psm_fit_(id, 0, light, pts, n, out);
}

/** \brief The warp's near plane from the receivers' points: the nearest view depth among them, held between the camera's
    near and `limit` (a share of the far plane is a good limit). The same list feeds the fit, so one list serves both.
    0 for no usable point. */
static inline float sh4der_psm_near_from_pts(const sh4der_psm_cam_t *cam, const float *pts, unsigned n, float limit) {
    float best = 1e30f;
    unsigned i;

    if (!cam || !pts || n < 1u || !(limit > cam->znear)) return 0.0f;
    for (i = 0; i < n; i++) {
        float d[3], z;

        d[0] = pts[3u * i] - cam->eye[0]; d[1] = pts[3u * i + 1u] - cam->eye[1]; d[2] = pts[3u * i + 2u] - cam->eye[2];
        z = sh4der_dot3_(d, cam->fwd);
        if (z > 0.0f && z < best) best = z;
    }
    if (!(best < 1e29f)) return 0.0f;
    return sh4der_clamp_(best, cam->znear, limit);
}

/** \brief The perspective shadow map: the same fit in the camera's post-projective space (built with near `warp_near`).
    `pts`: the receivers' points inside the view frustum, all in front of the camera. Returns 0 for a bad input or a
    degenerate frame (draw no map, or fall back to the uniform one). */
static inline int sh4der_psm_build_warp(sh4der_psm_t *out, const sh4der_psm_cam_t *cam, float warp_near, const float light[3],
                                        const float *pts, unsigned n) {
    float t[16];

    if (!sh4der_psm_cam_rows(cam, warp_near, t)) return 0;
    return sh4der_psm_fit_(t, 1, light, pts, n, out);
}

/* ---- the lookup --------------------------------------------------------- */

/** \brief A world point's map coordinates: u, v in -1 .. 1 across the fit, w the map's own (its sign says which side
    of the light's singularity: see sh4der_psm_tri_ok). Returns 0 when |w| is at the singularity. */
static inline int sh4der_psm_map(const sh4der_psm_t *p, const float x[3], float *u, float *v, float *w) {
    float h[4];

    sh4der_psm_xform(p->m, x, 1.0f, h);
    if (!sh4der_finite_(h[3]) || !(h[3] > SH4DER_PSM_TRI_EPS || h[3] < -SH4DER_PSM_TRI_EPS)) return 0;
    *u = sh4der_div_(h[0], h[3]);
    *v = sh4der_div_(h[1], h[3]);
    *w = h[3];
    return 1;
}

/** \brief 1 when a caster triangle with these three map w can be drawn: one sign, none at the singularity.
    Mixed signs cross the light's singularity (the triangle is cut or dropped by the caller). */
static inline int sh4der_psm_tri_ok(float w0, float w1, float w2) {
    const int p0 = w0 > SH4DER_PSM_TRI_EPS, p1 = w1 > SH4DER_PSM_TRI_EPS, p2 = w2 > SH4DER_PSM_TRI_EPS;
    const int n0 = w0 < -SH4DER_PSM_TRI_EPS, n1 = w1 < -SH4DER_PSM_TRI_EPS, n2 = w2 < -SH4DER_PSM_TRI_EPS;

    return (p0 && p1 && p2) || (n0 && n1 && n2);
}

/** \brief Metres of ground a map texel covers at point x, for a map `size` texels wide. t1, t2: two unit axes in the
    surface. Finite difference over `delta` metres. 1e30 when the map is degenerate there. */
static inline float sh4der_psm_texel_m(const sh4der_psm_t *p, unsigned size, const float x[3], const float t1[3], const float t2[3],
                                       float delta) {
    float a[3], b[3], u0, v0, w0, u1, v1, w1, u2, v2, w2, area;
    int k;

    for (k = 0; k < 3; k++) { a[k] = x[k] + t1[k] * delta; b[k] = x[k] + t2[k] * delta; }
    if (!sh4der_psm_map(p, x, &u0, &v0, &w0) || !sh4der_psm_map(p, a, &u1, &v1, &w1) || !sh4der_psm_map(p, b, &u2, &v2, &w2)) return 1e30f;
    u1 = (u1 - u0) * 0.5f * (float)size; v1 = (v1 - v0) * 0.5f * (float)size;
    u2 = (u2 - u0) * 0.5f * (float)size; v2 = (v2 - v0) * 0.5f * (float)size;
    area = u1 * v2 - v1 * u2;
    if (area < 0.0f) area = -area;
    if (!(area > 1e-12f)) return 1e30f;
    return sqrtf(sh4der_div_(delta * delta, area));
}

/** \brief Metres of surface one screen pixel covers at point x with unit normal n, for a screen `h_px` pixels tall.
    0 for a point behind the camera. The shadow map magnifies where texel_m is larger than this. */
static inline float sh4der_psm_pixel_m(const sh4der_psm_cam_t *cam, unsigned h_px, const float x[3], const float n[3]) {
    float d[3], z, len2, cos_t, ndr, az, area, inv;

    d[0] = x[0] - cam->eye[0]; d[1] = x[1] - cam->eye[1]; d[2] = x[2] - cam->eye[2];
    z = sh4der_dot3_(d, cam->fwd);
    len2 = sh4der_len3_(d[0], d[1], d[2]);
    if (!(z > 0.0f) || !(len2 > 0.0f) || !(h_px > 0u)) return 0.0f;
    inv = 1.0f / sqrtf(len2);
    cos_t = z * inv;
    ndr = sh4der_dot3_(n, d) * inv;
    if (ndr < 0.0f) ndr = -ndr;
    if (!(ndr > 1e-4f)) ndr = 1e-4f;
    az = z * 2.0f * cam->tan_half / (float)h_px;
    area = az * az * cos_t / ndr;
    return sqrtf(area);
}

/* ---- the points to fit -------------------------------------------------- */

/** \brief The eight corners of the camera's frustum between `zn` and `zf`: near 0..3 (lower left, lower right,
    upper right, upper left), far 4..7 in the same order. */
static inline int sh4der_psm_frustum(const sh4der_psm_cam_t *cam, float zn, float zf, float out[8][3]) {
    float r[3], u[3];
    int i, k;

    if (!cam || !out || !(zn > 0.0f) || !(zf > zn) || !(cam->tan_half > 0.0f) || !(cam->aspect > 0.0f)) return 0;
    sh4der_cross_(cam->fwd, cam->up, r);
    sh4der_norm3_(r);
    sh4der_cross_(r, cam->fwd, u);
    sh4der_norm3_(u);
    for (i = 0; i < 8; i++) {
        const float z = i < 4 ? zn : zf;
        const float hx = z * cam->tan_half * cam->aspect * ((i & 3) == 1 || (i & 3) == 2 ? 1.0f : -1.0f);
        const float hy = z * cam->tan_half * ((i & 3) >= 2 ? 1.0f : -1.0f);

        for (k = 0; k < 3; k++) out[i][k] = cam->eye[k] + cam->fwd[k] * z + r[k] * hx + u[k] * hy;
    }
    return 1;
}

/** \brief The vertices of a convex hull's slab clip: the corners (an 8 corner box or frustum, edges as in
    sh4der_psm_frustum) with height dot(p, axis) in lo .. hi, and where its 12 edges cross the two planes.
    These are the fit's points for a receiver slab (ground: axis up, lo -0.01, hi 0.01 or the tallest receiver).
    `out` holds up to 32 float[3]. Returns the count, 0 for an empty clip. */
static inline unsigned sh4der_psm_slab_pts(const float corners[8][3], const float axis[3], float lo, float hi, float out[][3]) {
    static const unsigned char edge[12][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
                                               { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
    unsigned n = 0, i;
    int k;

    if (!corners || !axis || !out || !(hi >= lo)) return 0;
    for (i = 0; i < 8u; i++) {
        const float h = sh4der_dot3_(corners[i], axis);

        if (h >= lo && h <= hi) { for (k = 0; k < 3; k++) out[n][k] = corners[i][k]; n++; }
    }
    for (i = 0; i < 12u; i++) {
        const float *p = corners[edge[i][0]], *q = corners[edge[i][1]];
        const float hp = sh4der_dot3_(p, axis), hq = sh4der_dot3_(q, axis);
        int pl;

        for (pl = 0; pl < 2; pl++) {
            const float h = pl ? hi : lo;

            if ((hp < h && hq > h) || (hp > h && hq < h)) {
                const float t = sh4der_div_(h - hp, hq - hp);

                for (k = 0; k < 3; k++) out[n][k] = p[k] + (q[k] - p[k]) * t;
                n++;
            }
        }
    }
    return n;
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_PSM_H */
