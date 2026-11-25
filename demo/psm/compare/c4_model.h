/* SH4DER-PVR: the chain spike (c4), the scene and the model of the chain

   demo/psm/compare/c4_model.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Spike c3b showed, on a console: the PVR's bump unit gives 128 + 127 cos(texel - Q) in alpha, Q one value a
   polygon; a render target can be the bump texture; a target written as ARGB1555 turns alpha into one bit by a
   threshold. This file is the chain built from those: the scene, the light's fit, the depth code, the window, the
   bands, the bias, and a MODEL of what each pass should hold. A model, not a measurement.

     MAP    every face turned toward the light, drawn from the light (parallel) into a 128 x 128 picture; a pixel
            holds the depth CODE of the face nearest the light: 0 .. 255 over the scene's depth range along the
            light. Each face is drawn `grow` map pixels larger all round, in its own plane, so a pixel whose centre
            is just off a face's edge still holds that face and not what lies far behind it.
     MASK   the receivers as the camera sees them. A receiver polygon carries the map as a bump texture and one
            byte Q made from its own depth code R. The pixel's alpha is 128 + 127 cos(2 pi (M - Q) / 256), M the map's
            code there. With Q = R + (w - b) + 128 that is 128 - 127 cos(2 pi (M - R - (w - b)) / 256): low while
            M - R is inside a WINDOW of half-width w codes that starts b codes under 0, high everywhere else on the
            circle. The target write keeps one bit: alpha >= threshold = shadow.
     BANDS  Q is one value a polygon, so a receiver is cut along lines of equal depth into bands 1, 2, 4 .. codes
            wide; each band has one R.
     BIAS   b = b0 + half a band + what half a map texel is worth in depth on that surface, in codes.

   The compare is cyclic: a caster more than 256 - 2 w + b codes nearer the light than the receiver reads lit again.

   Maths: every root, reciprocal, sine, dot and cross goes through sh4der_core.h's helpers: SH4ZAM on the Dreamcast,
   plain C with SH4DER_CORE_NO_SH4ZAM (the host test). No libm call and no divide instruction is written here.
*/

#ifndef C4_MODEL_H
#define C4_MODEL_H

#include <stdint.h>
#include <string.h>

#include "sh4der_core.h"

#define C4_MAP 128u                        /* the map's side, pixels */
#define C4_MASK_W 320u
#define C4_MASK_H 240u
#define C4_GROUND 8.0f                     /* the ground: x -8 .. 8, z 0 .. 16 */
#define C4_SKIRT 0.5f                      /* the map pass draws the ground this much larger, so its edge has a neighbour */
#define C4_BOXES 5u
#define C4_FACES (1u + 5u * C4_BOXES)      /* the ground and five faces a box (no undersides) */
#define C4_BAND_MAX 8u                     /* the most corners a band has */
#define C4_K1 128u                         /* the bump unit's constant and its gain: level = K1 + K3 cos */
#define C4_K3 127u
#define C4_FACING 0.02f                    /* a face whose normal . sun is not over this is drawn as shadow, with no compare */

/* A house, a wall with an eave over it, a hen, a post. Metres: lo then hi. */
static const float c4_box[C4_BOXES][6] = {
    { -5.0f, 0.0f, 6.0f, -2.0f, 3.0f, 10.0f },
    { 1.0f, 0.0f, 9.0f, 5.0f, 2.5f, 9.3f },
    { 0.8f, 2.5f, 8.5f, 5.2f, 2.7f, 9.5f },
    { 2.0f, 0.0f, 5.0f, 2.3f, 0.3f, 5.3f },
    { 5.5f, 0.0f, 4.0f, 5.8f, 4.0f, 4.3f },
};

static inline float c4_dot(const float a[3], const float b[3]) { return sh4der_dot3_(a, b); }

/* The whole number at or under x (|x| well under 2^31): one conversion, one compare. */
static inline int c4_ifloor(float x) {
    const int i = (int)x;

    return i - ((float)i > x);
}

typedef struct c4_face {
    float p[4][3];                         /* corners, in order round the face */
    float n[3];                            /* outward unit normal */
} c4_face_t;

/* The scene's faces: [0] the ground (made `skirt` larger on every side), then five a box. */
static inline void c4_faces(c4_face_t *f, float skirt) {
    static const uint8_t corner[5][4] = { {2,3,7,6}, {0,2,6,4}, {1,5,7,3}, {0,1,3,2}, {4,6,7,5} };   /* top, -x, +x, -z, +z; bit 0 x, 1 y, 2 z */
    static const float normal[5][3] = { {0,1,0}, {-1,0,0}, {1,0,0}, {0,0,-1}, {0,0,1} };
    const float g = C4_GROUND + skirt;
    unsigned b, k, i;

    memset(f, 0, sizeof *f * C4_FACES);
    f[0].p[0][0] = -g; f[0].p[0][2] = -skirt;
    f[0].p[1][0] = -g; f[0].p[1][2] = 2.0f * C4_GROUND + skirt;
    f[0].p[2][0] = g;  f[0].p[2][2] = 2.0f * C4_GROUND + skirt;
    f[0].p[3][0] = g;  f[0].p[3][2] = -skirt;
    f[0].n[1] = 1.0f;
    for (b = 0; b < C4_BOXES; b++) {
        for (k = 0; k < 5u; k++) {
            c4_face_t *q = &f[1u + 5u * b + k];

            for (i = 0; i < 4u; i++) {
                const unsigned c = corner[k][i];

                q->p[i][0] = c4_box[b][(c & 1u) ? 3 : 0]; q->p[i][1] = c4_box[b][(c & 2u) ? 4 : 1]; q->p[i][2] = c4_box[b][(c & 4u) ? 5 : 2];
            }
            memcpy(q->n, normal[k], sizeof q->n);
        }
    }
}

/* ---- the light ---------------------------------------------------------------------- */

typedef struct c4_light {
    float s[3];                            /* unit, from the scene toward the sun */
    float lx[3], ly[3], ld[3];             /* the map's two axes and the way the light travels */
    float u0, v0, d0;                      /* the least of each over the scene */
    float su, sv, sd;                      /* map pixels a metre along lx and ly; codes a metre along ld */
    float iu, iv;                          /* metres a map pixel along lx and ly */
} c4_light_t;

/* The light for a sun at azimuth az (0 = from +z, 90 = from +x) and elevation el, degrees, fitted round the scene:
   the map covers the scene's box in the light's frame, the codes 0 .. 255 its depth. */
static inline void c4_light_fit(c4_light_t *L, float az, float el) {
    const float d2r = 0.01745329f;
    static const float up[3] = { 0.0f, 1.0f, 0.0f };
    float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f }, sa, ca, se, ce;
    c4_face_t f[C4_FACES];
    unsigned i, k, a;

    sh4der_sincos_(az * d2r, &sa, &ca);
    sh4der_sincos_(el * d2r, &se, &ce);
    L->s[0] = ce * sa; L->s[1] = se; L->s[2] = ce * ca;
    sh4der_norm3_(L->s);
    L->ld[0] = -L->s[0]; L->ld[1] = -L->s[1]; L->ld[2] = -L->s[2];
    sh4der_cross_(up, L->ld, L->lx);
    sh4der_norm3_(L->lx);
    sh4der_cross_(L->ld, L->lx, L->ly);
    c4_faces(f, C4_SKIRT);
    for (i = 0; i < C4_FACES; i++) {
        for (k = 0; k < 4u; k++) {
            const float c[3] = { c4_dot(f[i].p[k], L->lx), c4_dot(f[i].p[k], L->ly), c4_dot(f[i].p[k], L->ld) };

            for (a = 0; a < 3u; a++) { if (c[a] < lo[a]) lo[a] = c[a]; if (c[a] > hi[a]) hi[a] = c[a]; }
        }
    }
    L->u0 = lo[0]; L->v0 = lo[1]; L->d0 = lo[2];
    L->iu = (hi[0] - lo[0]) * (1.0f / (float)C4_MAP); L->iv = (hi[1] - lo[1]) * (1.0f / (float)C4_MAP);
    L->su = sh4der_recip_pos_(L->iu); L->sv = sh4der_recip_pos_(L->iv);
    L->sd = 255.0f * sh4der_recip_pos_(hi[2] - lo[2]);
}

/* A world point in the map: out[0], out[1] the map pixel (not rounded), out[2] the depth code (not rounded). */
static inline void c4_to_map(const c4_light_t *L, const float p[3], float out[3]) {
    out[0] = (c4_dot(p, L->lx) - L->u0) * L->su;
    out[1] = (c4_dot(p, L->ly) - L->v0) * L->sv;
    out[2] = (c4_dot(p, L->ld) - L->d0) * L->sd;
}

/* ---- the window, the bands, the bias --------------------------------------------------- */

/* The bump unit's level for (M - Q) & 255: K1 + K3 cos(2 pi d / 256) in real arithmetic, rounded to a byte. A table,
   so no cosine is taken at run time and the host and the console hold the same bytes (the test checks it against the
   cosine). What the PVR's own arithmetic gives for a level is the console's to say. */
static const uint8_t c4_level[256] = {
    255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 251, 250, 250, 249, 248, 246,
    245, 244, 243, 241, 240, 239, 237, 235, 234, 232, 230, 228, 226, 224, 222, 220,
    218, 216, 213, 211, 209, 206, 204, 201, 199, 196, 193, 191, 188, 185, 182, 179,
    177, 174, 171, 168, 165, 162, 159, 156, 153, 150, 147, 144, 140, 137, 134, 131,
    128, 125, 122, 119, 116, 112, 109, 106, 103, 100,  97,  94,  91,  88,  85,  82,
     79,  77,  74,  71,  68,  65,  63,  60,  57,  55,  52,  50,  47,  45,  43,  40,
     38,  36,  34,  32,  30,  28,  26,  24,  22,  21,  19,  17,  16,  15,  13,  12,
     11,  10,   8,   7,   6,   6,   5,   4,   3,   3,   2,   2,   2,   1,   1,   1,
      1,   1,   1,   1,   2,   2,   2,   3,   3,   4,   5,   6,   6,   7,   8,  10,
     11,  12,  13,  15,  16,  17,  19,  21,  22,  24,  26,  28,  30,  32,  34,  36,
     38,  40,  43,  45,  47,  50,  52,  55,  57,  60,  63,  65,  68,  71,  74,  77,
     79,  82,  85,  88,  91,  94,  97, 100, 103, 106, 109, 112, 116, 119, 122, 125,
    128, 131, 134, 137, 140, 144, 147, 150, 153, 156, 159, 162, 165, 168, 171, 174,
    177, 179, 182, 185, 188, 191, 193, 196, 199, 201, 204, 206, 209, 211, 213, 216,
    218, 220, 222, 224, 226, 228, 230, 232, 234, 235, 237, 239, 240, 241, 243, 244,
    245, 246, 248, 249, 250, 250, 251, 252, 253, 253, 254, 254, 254, 255, 255, 255,
};

/* The write threshold for a window of half-width w codes (0 .. 128): the whole part of K1 - K3 cos(2 pi w / 256). A
   level at or over it is shadow. */
static const uint8_t c4_threshold[129] = {
      1,   1,   1,   1,   1,   1,   2,   2,   3,   4,   4,   5,   6,   7,   8,   9,
     10,  11,  13,  14,  15,  17,  19,  20,  22,  24,  25,  27,  29,  31,  33,  36,
     38,  40,  42,  45,  47,  49,  52,  54,  57,  60,  62,  65,  68,  70,  73,  76,
     79,  82,  85,  88,  91,  94,  97, 100, 103, 106, 109, 112, 115, 118, 121, 124,
    128, 131, 134, 137, 140, 143, 146, 149, 152, 155, 158, 161, 164, 167, 170, 173,
    176, 179, 182, 185, 187, 190, 193, 195, 198, 201, 203, 206, 208, 210, 213, 215,
    217, 219, 222, 224, 226, 228, 230, 231, 233, 235, 236, 238, 240, 241, 242, 244,
    245, 246, 247, 248, 249, 250, 251, 251, 252, 253, 253, 254, 254, 254, 254, 254,
    255,
};

/* The settings of the mask pass. Fill with c4_set_init. */
typedef struct c4_set {
    unsigned w, gs;                        /* the window's half-width, codes (128 at most); a band is 1 << gs codes wide */
    float b0;                              /* the constant part of the bias, codes */
    float grow;                            /* map pixels each face is made larger by in the map pass */
    unsigned thr;                          /* the write threshold for w */
} c4_set_t;

static inline void c4_set_init(c4_set_t *st, unsigned w, unsigned gs, float b0, float grow) {
    st->w = w > 128u ? 128u : w; st->gs = gs; st->b0 = b0; st->grow = grow;
    st->thr = c4_threshold[st->w];
}

/* The Q byte of a receiver whose depth code is r, for a window of half-width w that starts b codes under r. */
static inline unsigned c4_q(float r, unsigned w, float b) { return (unsigned)c4_ifloor(r + (float)w - b + 128.5f) & 255u; }

/* 1 = shadow: the bump level for map code m and Q byte q, against the threshold. */
static inline int c4_shadow(const c4_set_t *st, unsigned m, unsigned q) { return c4_level[(m - q) & 255u] >= st->thr; }

/* The band a depth code falls in, bands 1 << gs codes wide, and that band's own code (its middle). */
static inline int c4_band(float code, unsigned gs) {
    const int i = c4_ifloor(code + 0.5f);

    return i >= 0 ? i >> gs : -((-i + (1 << gs) - 1) >> gs);
}

static inline float c4_band_code(int band, unsigned gs) { return (float)(band * (1 << gs)) + 0.5f * (float)((1 << gs) - 1); }

/* The bias for a face of normal n, codes: b0, half a band, and half a map texel's worth of depth on that face. */
static inline float c4_bias(const c4_light_t *L, const float n[3], float b0, unsigned gs) {
    const float nd = c4_dot(n, L->ld), flat = b0 + 0.5f * (float)(1 << gs);

    if (nd > -1e-4f) return flat;                                /* edge-on or facing away: the caller treats it as shadow */
    /* metres of depth across one texel, as codes */
    return flat + 0.5f * (sh4der_abs_(c4_dot(n, L->lx)) * L->iu + sh4der_abs_(c4_dot(n, L->ly)) * L->iv) * sh4der_recip_pos_(-nd) * L->sd;
}

/* One corner of a polygon being cut: where it is, and where that is in the map (pixel x, y and the depth code). */
typedef struct c4_corner {
    float p[3], m[3];
} c4_corner_t;

/* A face's four corners, and for each edge i (corner i to i + 1) one over its change of code (0 where it has none). */
static inline void c4_face_corners(const c4_light_t *L, const c4_face_t *f, c4_corner_t out[4], float inv[4]) {
    unsigned i;

    for (i = 0; i < 4u; i++) {
        memcpy(out[i].p, f->p[i], sizeof out[i].p);
        c4_to_map(L, f->p[i], out[i].m);
    }
    for (i = 0; i < 4u; i++) {
        const float d = out[(i + 1u) & 3u].m[2] - out[i].m[2];

        inv[i] = sh4der_abs_(d) > 1e-6f ? sh4der_div_(1.0f, d) : 0.0f;
    }
}

/* Where edge a -> b of a face meets the level `code`: always worked out from the face's own two corners in the
   face's own order, so two bands that share a level get the same bits for the corner they share. */
static inline void c4_cross_edge(const c4_corner_t *a, const c4_corner_t *b, float inv, float code, c4_corner_t *out) {
    const float t = (code - a->m[2]) * inv;

    out->p[0] = sh4der_lerp_(a->p[0], b->p[0], t); out->p[1] = sh4der_lerp_(a->p[1], b->p[1], t); out->p[2] = sh4der_lerp_(a->p[2], b->p[2], t);
    out->m[0] = sh4der_lerp_(a->m[0], b->m[0], t); out->m[1] = sh4der_lerp_(a->m[1], b->m[1], t);
    out->m[2] = code;
}

/* The part of a face (c4_face_corners) that lies in band `band`: its corners in order round it. Returns how many,
   0 if the band misses the face. */
static inline unsigned c4_band_poly(const c4_corner_t face[4], const float inv[4], int band, unsigned gs, c4_corner_t *out) {
    const float lo = (float)(band * (1 << gs)) - 0.5f, hi = lo + (float)(1 << gs);
    unsigned i, k, m = 0;

    for (i = 0; i < 4u; i++) {
        const c4_corner_t *a = &face[i], *b = &face[(i + 1u) & 3u];
        const float ca = a->m[2], cb = b->m[2];
        const float level[2] = { ca < cb ? lo : hi, ca < cb ? hi : lo };   /* in the order the edge meets them */

        if (ca >= lo && ca <= hi && m < C4_BAND_MAX) out[m++] = *a;
        for (k = 0; k < 2u; k++) {
            const float v = level[k];

            if (((ca < v && v < cb) || (cb < v && v < ca)) && m < C4_BAND_MAX) c4_cross_edge(a, b, inv[i], v, &out[m++]);
        }
    }
    return m >= 3u ? m : 0u;
}

/* ---- the camera ---------------------------------------------------------------------- */

typedef struct c4_cam {
    float eye[3], right[3], up[3], fwd[3];
    float focal, cx, cy;                   /* focal length and centre, pixels */
} c4_cam_t;

static inline void c4_cam_look(c4_cam_t *c, const float eye[3], const float target[3], float focal, float cx, float cy) {
    static const float world_up[3] = { 0.0f, 1.0f, 0.0f };

    memcpy(c->eye, eye, sizeof c->eye);
    c->fwd[0] = target[0] - eye[0]; c->fwd[1] = target[1] - eye[1]; c->fwd[2] = target[2] - eye[2];
    sh4der_norm3_(c->fwd);
    sh4der_cross_(world_up, c->fwd, c->right);
    sh4der_norm3_(c->right);
    sh4der_cross_(c->fwd, c->right, c->up);
    c->focal = focal; c->cx = cx; c->cy = cy;
}

/* The camera of the spike, for a picture w x h. */
static inline void c4_camera(c4_cam_t *cam, unsigned w, unsigned h) {
    static const float eye[3] = { 0.0f, 5.0f, -5.0f }, target[3] = { 0.0f, 0.5f, 8.0f };

    c4_cam_look(cam, eye, target, 420.0f * (float)w * (1.0f / 640.0f), 0.5f * (float)w, 0.5f * (float)h);
}

/* The camera as one matrix, column major (m[4 c + r]): (X, Y, Z, W) = m (x, y, z, 1) with W the depth along the
   view, X / W and Y / W the pixel. Z is not used (0). */
static inline void c4_cam_matrix(const c4_cam_t *c, float m[16]) {
    unsigned k;

    for (k = 0; k < 3u; k++) {
        m[4u * k + 0u] = c->focal * c->right[k] + c->cx * c->fwd[k];
        m[4u * k + 1u] = -c->focal * c->up[k] + c->cy * c->fwd[k];
        m[4u * k + 2u] = 0.0f;
        m[4u * k + 3u] = c->fwd[k];
    }
    m[12] = -(c->focal * c4_dot(c->right, c->eye) + c->cx * c4_dot(c->fwd, c->eye));
    m[13] = c->focal * c4_dot(c->up, c->eye) - c->cy * c4_dot(c->fwd, c->eye);
    m[14] = 0.0f;
    m[15] = -c4_dot(c->fwd, c->eye);
}

/* ---- rays, for the model ------------------------------------------------------------------ */

static inline int c4_ray_box(const float o[3], const float d[3], const float *bx, float *t, unsigned *axis) {
    float t0 = 1e-4f, t1 = 1e30f;
    unsigned k, ax = 0;

    for (k = 0; k < 3u; k++) {
        float a, b, inv;

        if (sh4der_abs_(d[k]) < 1e-9f) { if (o[k] < bx[k] || o[k] > bx[k + 3u]) return 0; continue; }
        inv = sh4der_div_(1.0f, d[k]);
        a = (bx[k] - o[k]) * inv; b = (bx[k + 3u] - o[k]) * inv;
        if (a > b) { const float s = a; a = b; b = s; }
        if (a > t0) { t0 = a; ax = k; }
        if (b < t1) t1 = b;
        if (t0 > t1) return 0;
    }
    *t = t0; if (axis) *axis = ax;
    return 1;
}

/* What a ray from o along d meets first: 0 nothing, 1 the ground (within `skirt` of its edge), 2 a box. *t where;
   n the surface's outward normal; *face its index in c4_faces (a box's underside, which is no face, gives C4_FACES). */
static inline int c4_ray(const float o[3], const float d[3], float skirt, float *t, float n[3], unsigned *face) {
    static const uint8_t side[3][2] = { { 1, 2 }, { 0xff, 0 }, { 3, 4 } };     /* by axis, then the low or the high side */
    float best = 1e30f, tb;
    unsigned b, ax;
    int what = 0;

    *face = 0;
    if (d[1] < -1e-9f) {
        const float tg = sh4der_div_(-o[1], d[1]), x = o[0] + tg * d[0], z = o[2] + tg * d[2];

        if (tg > 1e-4f && sh4der_abs_(x) <= C4_GROUND + skirt && z >= -skirt && z <= 2.0f * C4_GROUND + skirt) { best = tg; what = 1; n[0] = 0.0f; n[1] = 1.0f; n[2] = 0.0f; }
    }
    for (b = 0; b < C4_BOXES; b++) {
        if (c4_ray_box(o, d, c4_box[b], &tb, &ax) && tb < best) {
            const unsigned k = side[ax][d[ax] > 0.0f ? 0 : 1];

            best = tb; what = 2;
            n[0] = n[1] = n[2] = 0.0f; n[ax] = d[ax] > 0.0f ? -1.0f : 1.0f;
            *face = k == 0xffu ? C4_FACES : 1u + 5u * b + k;
        }
    }
    *t = best;
    return what;
}

/* 1 when a box stands between p (on a surface of normal n) and the sun. */
static inline int c4_truth(const c4_light_t *L, const float p[3], const float n[3]) {
    const float o[3] = { p[0] + 1e-3f * n[0], p[1] + 1e-3f * n[1], p[2] + 1e-3f * n[2] };
    float t;
    unsigned b;

    if (c4_dot(n, L->s) <= 0.0f) return 1;
    for (b = 0; b < C4_BOXES; b++) if (c4_ray_box(o, L->s, c4_box[b], &t, NULL)) return 1;
    return 0;
}

/* A face as the map pass draws it: its four corners in the map (x, y in pixels, the depth code), the face made
   `grow` pixels larger all round in its own plane. 0 when the face is turned from the light or nearly edge-on to it
   (it is then not drawn). */
static inline int c4_map_quad(const c4_light_t *L, const c4_face_t *f, float grow, float out[4][3]) {
    float c[3], e1[3], e2[3], m[4][3], l1, l2, cr, k1, k2, icr;
    unsigned i, k;

    if (c4_dot(f->n, L->s) <= C4_FACING) return 0;
    for (i = 0; i < 4u; i++) c4_to_map(L, f->p[i], m[i]);
    for (k = 0; k < 3u; k++) {
        c[k] = 0.25f * (m[0][k] + m[1][k] + m[2][k] + m[3][k]);
        e1[k] = 0.5f * (m[1][k] - m[0][k]);                      /* half an edge along 0 -> 1 */
        e2[k] = 0.5f * (m[3][k] - m[0][k]);                      /* half an edge along 0 -> 3 */
    }
    l1 = sh4der_sqrt_(sh4der_len2_(e1[0], e1[1])); l2 = sh4der_sqrt_(sh4der_len2_(e2[0], e2[1]));
    cr = sh4der_abs_(e1[0] * e2[1] - e1[1] * e2[0]);
    if (l1 < 1e-6f || l2 < 1e-6f || cr < 1e-6f) return 0;
    icr = sh4der_recip_pos_(cr);
    k1 = 1.0f + grow * l2 * icr;                                 /* cr / l2 is the centre's distance to the edges that run along e2 */
    k2 = 1.0f + grow * l1 * icr;
    for (k = 0; k < 3u; k++) { e1[k] *= k1; e2[k] *= k2; }
    for (k = 0; k < 3u; k++) {
        out[0][k] = c[k] - e1[k] - e2[k]; out[1][k] = c[k] + e1[k] - e2[k];
        out[2][k] = c[k] + e1[k] + e2[k]; out[3][k] = c[k] - e1[k] + e2[k];
    }
    return 1;
}

/* The model of the map pass: each face that c4_map_quad gives, filled where a map pixel's centre is inside it; a
   pixel keeps the nearest face's code, rounded; 255 where there is none. `ids` (may be NULL) gets the face's index,
   C4_FACES where there is none. */
static inline void c4_model_map(const c4_light_t *L, float grow, uint8_t *map, uint8_t *ids) {
    static float best[C4_MAP * C4_MAP];
    c4_face_t f[C4_FACES];
    unsigned i, k, x, y;

    c4_faces(f, C4_SKIRT);
    for (i = 0; i < C4_MAP * C4_MAP; i++) { best[i] = 1e30f; map[i] = 255u; if (ids) ids[i] = (uint8_t)C4_FACES; }
    for (i = 0; i < C4_FACES; i++) {
        float q[4][3], ex[3], ey[3], idet;

        if (!c4_map_quad(L, &f[i], grow, q)) continue;
        for (k = 0; k < 3u; k++) { ex[k] = q[1][k] - q[0][k]; ey[k] = q[3][k] - q[0][k]; }
        idet = sh4der_div_(1.0f, ex[0] * ey[1] - ex[1] * ey[0]);
        for (y = 0; y < C4_MAP; y++) {
            for (x = 0; x < C4_MAP; x++) {
                const float px = (float)x + 0.5f - q[0][0], py = (float)y + 0.5f - q[0][1];
                const float a = (px * ey[1] - py * ey[0]) * idet, b = (ex[0] * py - ex[1] * px) * idet;   /* the pixel in the quad's own two edges */
                float code;
                int ic;

                if (a < 0.0f || a > 1.0f || b < 0.0f || b > 1.0f) continue;
                code = q[0][2] + a * ex[2] + b * ey[2];
                if (code >= best[y * C4_MAP + x]) continue;
                best[y * C4_MAP + x] = code;
                ic = c4_ifloor(code + 0.5f);
                map[y * C4_MAP + x] = (uint8_t)(ic < 0 ? 0 : ic > 255 ? 255 : ic);
                if (ids) ids[y * C4_MAP + x] = (uint8_t)i;
            }
        }
    }
}

enum { C4_SKY = 0, C4_LIT = 1, C4_SHADOW = 2 };

/* What the model knows of one mask pixel besides the chain's answer. */
typedef struct c4_pixel {
    int truth;                             /* what a ray to the sun says: C4_SKY, C4_LIT, C4_SHADOW */
    unsigned face;                         /* the face the camera sees there */
    unsigned mx, my;                       /* the map pixel it reads */
    float sep;                             /* how many codes nearer the light the map's surface is than the pixel */
} c4_pixel_t;

/* The model of one mask pixel: what the chain should put there (C4_SKY, C4_LIT, C4_SHADOW), from `map` (the model's
   or the console's own). */
static inline int c4_model_pixel(const c4_light_t *L, const c4_cam_t *cam, const uint8_t *map, const c4_set_t *st, unsigned px, unsigned py, c4_pixel_t *out) {
    const float ifo = sh4der_recip_pos_(cam->focal);
    const float sx = ((float)px + 0.5f - cam->cx) * ifo, sy = -((float)py + 0.5f - cam->cy) * ifo;
    const float d[3] = { cam->fwd[0] + sx * cam->right[0] + sy * cam->up[0], cam->fwd[1] + sx * cam->right[1] + sy * cam->up[1],
                         cam->fwd[2] + sx * cam->right[2] + sy * cam->up[2] };
    float t, n[3], p[3], m[3];
    int mx, my;
    unsigned mc;

    memset(out, 0, sizeof *out);
    if (!c4_ray(cam->eye, d, 0.0f, &t, n, &out->face)) { out->truth = C4_SKY; return C4_SKY; }
    p[0] = cam->eye[0] + t * d[0]; p[1] = cam->eye[1] + t * d[1]; p[2] = cam->eye[2] + t * d[2];
    out->truth = c4_truth(L, p, n) ? C4_SHADOW : C4_LIT;
    if (c4_dot(n, L->s) <= C4_FACING) return C4_SHADOW;          /* turned from the sun: drawn as shadow, no compare */
    c4_to_map(L, p, m);
    mx = c4_ifloor(m[0]); my = c4_ifloor(m[1]);
    mx = mx < 0 ? 0 : mx >= (int)C4_MAP ? (int)C4_MAP - 1 : mx; my = my < 0 ? 0 : my >= (int)C4_MAP ? (int)C4_MAP - 1 : my;
    mc = map[(unsigned)my * C4_MAP + (unsigned)mx];
    out->mx = (unsigned)mx; out->my = (unsigned)my;
    out->sep = m[2] - (float)mc;
    return c4_shadow(st, mc, c4_q(c4_band_code(c4_band(m[2], st->gs), st->gs), st->w, c4_bias(L, n, st->b0, st->gs))) ? C4_SHADOW : C4_LIT;
}

#endif /* C4_MODEL_H */
