/* SH4DER-PVR: pl4nar, real-time planar reflection

   sh4der_pl4nar.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_pl4nar.h
    \brief   The world drawn a second time, mirrored about a plane, into a small
             texture; a reflective surface looks that texture up per vertex and
             blends it in as one translucent layer whose vertex alpha is Fresnel.

    The PVR renders to a texture, so a second, small picture of the world costs one
    more pass and no trick. Mirror the world about a plane (a road, a floor, water,
    a window), draw it at low detail into a 256 x 128 target, and that target is
    the reflection of every surface near the plane.

      pass 1  sh4der_pl4nar_view_build -> the mirrored view; draw the world (low
              detail, culled the other way) into the target
      pass 2  draw the scene; for each reflective vertex sh4der_pl4nar_shade (a
              body) or sh4der_pl4nar_flat (a surface in the plane) gives u, v and
              the Fresnel share; draw the surface again as ONE translucent layer
              with the target as its texture

    The lookup. Reflect the view ray off the vertex (r = 2 (n.v) n - v), follow
    it `reach` world units, and project the point it reaches with the mirror pass's
    own rows. For a vertex in the plane with the plane's normal that is the exact
    planar reflection: the texel under the vertex's own screen position, for any
    reach. For any other vertex (a car body above a road) a curved body bends the
    picture as a mirror would, from the one target. A ray that leaves the picture
    slides to its edge; a ray that ends behind the reflection camera fades to the
    "sky overhead" texel at the bottom centre of the target.

    The share. Schlick, F = F0 + (1 - F0) (1 - n.v)^5, for any F0 (at the water
    F0 it is sh4der_water_fresnel). It is the layer's vertex alpha: a face seen edge on
    reflects strongly, a face seen head on barely.

    The layer. Same vertices as the opaque surface, translucent list, depth test
    greater-or-equal, no depth write, nothing sorted by hand. Additive (source
    alpha / one) over paint: a clear coat keeps the paint's colour. Blended
    (source alpha / 1 - source alpha) for glass, water, a wet road.

    Flat surfaces. The lookup is a screen position, and the PVR maps a texture
    perspective-correct, so a large polygon in the plane swims. Cut it while the
    depths of its corners differ: sh4der_pl4nar_tri_split names the edge.

    Views. The four rows of screen x projection x view (x mirror) are filled in
    closed form (sh4der_pl4nar_viewproj_rows) and loaded with
    shz_xmtrx_load_rows_4x4; a vertex is one FTRV (shz_xmtrx_transform_vec4).
    Keep that form: it is the one that ran right in the first client. XMTRX is
    shared state: the lookups need the view in it (sh4der_pl4nar_begin, or the
    _n calls, which load it themselves) and leave it there, so the caller's own
    matrix is gone afterwards. Between sh4der_pl4nar_begin and the last single
    lookup call nothing that touches XMTRX: no shz_xmtrx_* call, no SH4ZAM
    matrix routine that uses XMTRX as scratch, no KallistiOS mat_* call.

    A large world. FTRV's error is relative to the largest product in a row,
    and a row's constant (-dot(row, eye)) cancels against row . p. Within a few
    hundred units of the origin that is under a tenth of a pixel. For a world
    of kilometres, build the rows about the eye (pass eye - eye = 0 and
    target - eye) and give the lookups positions less the eye.

    Memory: the target is w x h x 2 B of video memory (256 x 128: 64 KB), the
    only memory the effect holds. Main memory: a view (104 B) and a material
    (24 B), the caller's. Sound RAM (AICA): 0.

    No allocation, no globals in the header. Floats in arrays the caller owns
    (4-byte aligned); a stride is a multiple of 4; nothing casts a byte pointer.
    The maths is SH4ZAM through sh4der_core.h (normalise, cross, dot, lerp, fused
    multiply-add, sine and cosine, divide, reciprocal) and XMTRX (load rows,
    FTRV); define SH4DER_CORE_NO_SH4ZAM for the plain C path. By hand (no SH4ZAM
    equivalent): the four view rows and the mirror step are a few products each,
    marked "hand" below; the edge search of the split is compares.

    Version: v0.10
    The effect: a car's paint and glass show the street going by; a wet road, a
      polished floor, a pool or a shop window shows the world upside down in it.
    Developed for: a driving demo, a street reflected on a car body.
    Use it for: car paint, glass, chrome, water, wet ground, polished floors,
      mirrors: anything that lies in, or near, one plane.

    \author Jilted Valkyrie
*/

#ifndef SH4DER_PL4NAR_H
#define SH4DER_PL4NAR_H

#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_PL4NAR_F0_WATER     0.0204f   /**< \brief The F0 inside sh4der_water_fresnel: air to water. */
#define SH4DER_PL4NAR_F0_PAINT     0.06f     /**< \brief Clear-coated paint. */
#define SH4DER_PL4NAR_F0_GLASS     0.10f     /**< \brief Window glass from outside (two faces). */
#define SH4DER_PL4NAR_F0_CHROME    0.55f     /**< \brief Chromium plate. */
#define SH4DER_PL4NAR_F0_FLOOR     0.05f     /**< \brief A varnished or polished floor. */
#define SH4DER_PL4NAR_REACH        18.0f     /**< \brief World units a reflected ray is followed before the lookup (a body near the plane). */
#define SH4DER_PL4NAR_FADE_W       4.5f      /**< \brief View depth under which a body's lookup fades to the sky texel: a quarter of the reach. */
#define SH4DER_PL4NAR_FOV_SCALE    1.55f     /**< \brief Reflection field of view / main field of view for a body: wider, so rays off a curved body stay in the picture. A flat surface wants 1. */
#define SH4DER_PL4NAR_SKY_U        0.5f      /**< \brief The texel a lost ray fades to, u: the centre. */
#define SH4DER_PL4NAR_SKY_V        0.97f     /**< \brief The texel a lost ray fades to, v: the bottom row, the sky over the camera. */
#define SH4DER_PL4NAR_GROUND_KEEP  0.55f     /**< \brief Share of the layer kept where a body's reflected ray points straight down at the plane. */
#define SH4DER_PL4NAR_W_MIN        0.001f    /**< \brief View depth at or under which a point is behind the reflection camera. */
#define SH4DER_PL4NAR_SPLIT_RATIO  1.25f     /**< \brief Far corner depth / near corner depth above which a flat triangle is cut. */
/** @} */

/** \brief A plane: unit normal n toward the side the camera is on, points x with dot(n, x) = d. */
typedef struct sh4der_pl4nar_plane { float n[3]; float d; } sh4der_pl4nar_plane_t;

/** \brief One matrix row: SH4ZAM's vector, or four floats on the plain C path. */
#ifndef SH4DER_CORE_NO_SH4ZAM
typedef shz_vec4_t sh4der_pl4nar_row_t;
#else
typedef struct sh4der_pl4nar_row { float x, y, z, w; } sh4der_pl4nar_row_t;
#endif

/** \brief The reflection view of one frame. Keep it while its target is on screen (a target drawn every other frame
    is looked up with the view it was drawn with). */
typedef struct sh4der_pl4nar_view {
    sh4der_pl4nar_row_t row[4];   /**< \brief World -> (x w, y w, znear, w), x and y in target pixels (y down), w the view depth. */
    float eye[3];                 /**< \brief The real camera, world. Rows are read with 4-byte loads: 4-byte alignment is enough. */
    float plane_n[3];             /**< \brief The mirror plane's normal. */
    float inv_w, inv_h;           /**< \brief 1 / target size. */
    float inv_fade;               /**< \brief 1 / fade depth. */
    float pad_;
} sh4der_pl4nar_view_t;

/** \brief A reflective material: a row of the caller's table. */
typedef struct sh4der_pl4nar_material {
    float f0;             /**< \brief Reflection seen head on, 0 .. 1 (SH4DER_PL4NAR_F0_*). */
    float gain;           /**< \brief Scale on the layer's alpha: 1 = the Fresnel share itself. */
    float ground_keep;    /**< \brief Share kept where the ray meets the plane itself (a body); 1 = no change. */
    float reach;          /**< \brief World units the reflected ray is followed (a body); a flat surface ignores it. */
    uint32_t tint;        /**< \brief 0x00RRGGBB the target is multiplied by: 0x00FFFFFF = none. */
    uint32_t additive;    /**< \brief 1: source alpha / one (over paint). 0: source alpha / 1 - source alpha (glass, water). */
} sh4der_pl4nar_material_t;

/** \brief Starting materials. Paint and glass were judged on screen (host picture); the rest are physical F0 values
    with untested gains. */
enum {
    SH4DER_PL4NAR_PAINT = 0,   /**< \brief Clear coat over paint: additive. */
    SH4DER_PL4NAR_GLASS,       /**< \brief A window on a body: blended. */
    SH4DER_PL4NAR_CHROME,      /**< \brief Bumpers, trim: blended, strong head on. */
    SH4DER_PL4NAR_WATER,       /**< \brief A pool or a puddle in the plane: blended. */
    SH4DER_PL4NAR_WET_ROAD,    /**< \brief A film of water on rough ground in the plane: blended, dimmed. */
    SH4DER_PL4NAR_FLOOR,       /**< \brief A polished floor in the plane: additive, the floor keeps its colour. */
    SH4DER_PL4NAR_PRESETS
};

/** \brief One vertex's answer. */
typedef struct sh4der_pl4nar_sample {
    float u, v;       /**< \brief Lookup in the reflection texture, 0 .. 1. */
    float fresnel;    /**< \brief Share of the reflection, f0 .. 1. */
    float down;       /**< \brief 0 .. 1: how far the reflected ray points below the plane's horizon (it meets the plane itself, not the world). */
} sh4der_pl4nar_sample_t;

/** \brief A starting material by name (SH4DER_PL4NAR_PAINT ...); an unknown name gives paint. */
static inline sh4der_pl4nar_material_t sh4der_pl4nar_preset(unsigned kind) {
    sh4der_pl4nar_material_t m;

    m.f0 = SH4DER_PL4NAR_F0_PAINT; m.gain = 1.0f; m.ground_keep = SH4DER_PL4NAR_GROUND_KEEP; m.reach = SH4DER_PL4NAR_REACH;
    m.tint = 0x00FFFFFFu; m.additive = 1u;
    switch (kind) {
    case SH4DER_PL4NAR_GLASS:    m.f0 = SH4DER_PL4NAR_F0_GLASS; m.additive = 0u; break;
    case SH4DER_PL4NAR_CHROME:   m.f0 = SH4DER_PL4NAR_F0_CHROME; m.additive = 0u; break;
    case SH4DER_PL4NAR_WATER:    m.f0 = SH4DER_PL4NAR_F0_WATER; m.ground_keep = 1.0f; m.reach = 0.0f; m.additive = 0u; break;
    case SH4DER_PL4NAR_WET_ROAD: m.f0 = SH4DER_PL4NAR_F0_WATER; m.gain = 0.7f; m.ground_keep = 1.0f; m.reach = 0.0f; m.additive = 0u; break;
    case SH4DER_PL4NAR_FLOOR:    m.f0 = SH4DER_PL4NAR_F0_FLOOR; m.gain = 0.8f; m.ground_keep = 1.0f; m.reach = 0.0f; break;
    default: break;
    }
    return m;
}

/** \brief A point mirrored about the plane: x' = x - 2 (dot(n, x) - d) n. out may be p. */
static inline void sh4der_pl4nar_mirror_point(const sh4der_pl4nar_plane_t *plane, const float p[3], float out[3]) {
    const float k = -2.0f * (sh4der_dot3_(plane->n, p) - plane->d);   /* hand: one scale */

    out[0] = sh4der_fma_(plane->n[0], k, p[0]);
    out[1] = sh4der_fma_(plane->n[1], k, p[1]);
    out[2] = sh4der_fma_(plane->n[2], k, p[2]);
}

/** \brief 1: the sphere is wholly on the camera's side of the plane; -1: wholly on the far side; 0: it crosses.
    The PVR has no clip plane, so the mirror pass must not draw what lies under the plane: skip a -1, draw a 1,
    and for a 0 draw only if the part below is hidden (a building's foundations) or cut the mesh at the plane. */
static inline int sh4der_pl4nar_sphere_side(const sh4der_pl4nar_plane_t *plane, const float centre[3], float radius) {
    const float h = sh4der_dot3_(plane->n, centre) - plane->d;

    return h > radius ? 1 : h < -radius ? -1 : 0;
}

/** \brief 1 on the frames the target is drawn, for a target refreshed one frame in `every` (0 and 1: every frame).
    Half rate halves the mirror pass and leaves the reflection one frame late on the others. */
static inline int sh4der_pl4nar_due(unsigned frame, unsigned every) {
    return every < 2u || frame % every == 0u;
}

/** \brief The rows of screen x projection x view (x mirror), in closed form: a world point p gives
    (x w, y w, znear, w) = (dot(row0, (p, 1)), ...), x and y in pixels of a w x h target (y down), w the view depth.
    Right-handed, the camera looks from eye to target. plane NULL: no mirror (a main view). Returns 1, or 0 with
    the rows untouched when the view has no direction (eye on target, up along the view), the field of view is not
    inside 0 .. pi or a size is not positive.

      back b = normalize(eye - target), side s = normalize(up x b), up u = b x s, c = cot(fov / 2)
      row x = (w / 2) (c h / w) s - (w / 2) b      row y = -(h / 2) c u - (h / 2) b      row w = -b
      each row's constant = -dot(row, eye) (the eye maps to 0); row z = (0, 0, 0, znear)
      mirror about dot(n, p) = d: row' = row - 2 dot(row, n) n, constant' = constant + 2 d dot(row, n)

    By hand: the row sums below (a few products a row; the vector steps are SH4ZAM through sh4der_core.h). */
static inline int sh4der_pl4nar_viewproj_rows(sh4der_pl4nar_row_t row[4], const sh4der_pl4nar_plane_t *plane, const float eye[3],
                                              const float target[3], const float up[3], float fov, float znear, float w, float h) {
    float b[3] = { eye[0] - target[0], eye[1] - target[1], eye[2] - target[2] }, s[3], u[3], r[3][3], sn, cs, cot;
    const float hw = w * 0.5f, hh = h * 0.5f;
    unsigned i, k;

    if (!sh4der_finite_(fov) || !(fov > 0.001f) || !(fov < 3.14f) || !(w > 0.0f) || !(h > 0.0f) || !sh4der_finite_(znear)) return 0;
    if (!(sh4der_len3_(b[0], b[1], b[2]) > 1e-12f)) return 0;
    sh4der_norm3_(b);
    sh4der_cross_(up, b, s);
    if (!(sh4der_len3_(s[0], s[1], s[2]) > 1e-12f)) return 0;
    sh4der_norm3_(s);
    sh4der_cross_(b, s, u);
    sh4der_sincos_(fov * 0.5f, &sn, &cs);
    cot = sh4der_div_(cs, sn);
    {
        const float kx = hw * cot * sh4der_div_(h, w), ky = -hh * cot;   /* hand: constant scales */

        for (k = 0; k < 3u; k++) {
            r[0][k] = sh4der_fma_(s[k], kx, -hw * b[k]);
            r[1][k] = sh4der_fma_(u[k], ky, -hh * b[k]);
            r[2][k] = -b[k];
        }
    }
    for (i = 0; i < 3u; i++) {
        sh4der_pl4nar_row_t *o = &row[i == 2u ? 3 : i];
        float c = -sh4der_dot3_(r[i], eye);

        if (plane) {
            const float m = 2.0f * sh4der_dot3_(r[i], plane->n);

            for (k = 0; k < 3u; k++) r[i][k] = sh4der_fma_(plane->n[k], -m, r[i][k]);
            c = sh4der_fma_(plane->d, m, c);
        }
        o->x = r[i][0]; o->y = r[i][1]; o->z = r[i][2]; o->w = c;
    }
    row[2].x = 0.0f; row[2].y = 0.0f; row[2].z = 0.0f; row[2].w = znear;
    return 1;
}

/** \brief Put the view in XMTRX: before the mirrored world is drawn with it, and before a run of single
    sh4der_pl4nar_shade or sh4der_pl4nar_flat calls. From here to the last of them, nothing else may touch XMTRX;
    afterwards the caller loads its own matrix again. Nothing on the plain C path. */
static inline void sh4der_pl4nar_begin(const sh4der_pl4nar_view_t *v) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    shz_xmtrx_load_rows_4x4(&v->row[0], &v->row[1], &v->row[2], &v->row[3]);
#else
    (void)v;
#endif
}

/** \brief Build the reflection view and leave it in XMTRX.

    The camera is the main camera (eye, target, up); the world is mirrored in front of it. fov is the reflection
    pass's vertical field of view in radians: the main one for a flat surface, the main one x
    SH4DER_PL4NAR_FOV_SCALE for a body. tex_w, tex_h: the target in pixels. fade_w: SH4DER_PL4NAR_FADE_W, or a
    quarter of the longest reach in use. A mirrored world has its winding reversed: cull the other way in this
    pass. Returns 1, or 0 for a view sh4der_pl4nar_viewproj_rows refuses or a fade that is not positive (the view
    is then not to be used: draw no layer this frame). */
static inline int sh4der_pl4nar_view_build(sh4der_pl4nar_view_t *v, const sh4der_pl4nar_plane_t *plane, const float eye[3],
                                           const float target[3], const float up[3], float fov, float znear,
                                           float tex_w, float tex_h, float fade_w) {
    unsigned k;

    if (!v || !plane || !(fade_w > 0.0f) || !sh4der_pl4nar_viewproj_rows(v->row, plane, eye, target, up, fov, znear, tex_w, tex_h)) return 0;
    for (k = 0; k < 3u; k++) { v->eye[k] = eye[k]; v->plane_n[k] = plane->n[k]; }
    v->inv_w = sh4der_recip_pos_(tex_w);
    v->inv_h = sh4der_recip_pos_(tex_h);
    v->inv_fade = sh4der_recip_pos_(fade_w);
    v->pad_ = 0.0f;
    sh4der_pl4nar_begin(v);
    return 1;
}

/* A world point through the view: FTRV on XMTRX (the view is loaded), or the four row sums in plain C. */
static inline __attribute__((always_inline)) sh4der_pl4nar_row_t sh4der_pl4nar_xform_(const sh4der_pl4nar_view_t *v, float x, float y, float z) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    (void)v;
    return shz_xmtrx_transform_vec4(shz_vec4_init(x, y, z, 1.0f));
#else
    sh4der_pl4nar_row_t o;

    o.x = v->row[0].x * x + v->row[0].y * y + v->row[0].z * z + v->row[0].w;
    o.y = v->row[1].x * x + v->row[1].y * y + v->row[1].z * z + v->row[1].w;
    o.z = v->row[2].x * x + v->row[2].y * y + v->row[2].z * z + v->row[2].w;
    o.w = v->row[3].x * x + v->row[3].y * y + v->row[3].z * z + v->row[3].w;
    return o;
#endif
}

/** \brief Schlick Fresnel with any F0: F0 + (1 - F0) (1 - n.v)^5. At F0 = SH4DER_PL4NAR_F0_WATER it is
    sh4der_water_fresnel. n.v is clamped to 0 .. 1: a face turned away reflects fully. */
static inline __attribute__((always_inline)) float sh4der_pl4nar_fresnel(float ndotv, float f0) {
    const float t = 1.0f - sh4der_sat_(ndotv), t2 = t * t;   /* hand: the fifth power is three products */

    return sh4der_fma_(1.0f - f0, t2 * t2 * t, f0);
}

/* The body lookup, one copy for the single and the batch call. The reflected ray is 2 (n.v) n - v (what
   sh4der_water_reflect of sh4der_water.h gives) with the n.v the Fresnel term already has: one dot less. */
static inline __attribute__((always_inline)) void sh4der_pl4nar_shade_(const sh4der_pl4nar_view_t *v, const float pos[3], const float n[3],
                                                                       const sh4der_pl4nar_material_t *m, sh4der_pl4nar_sample_t *o) {
    float view[3] = { v->eye[0] - pos[0], v->eye[1] - pos[1], v->eye[2] - pos[2] }, r[3], ndv2;

    sh4der_norm3_(view);
    ndv2 = sh4der_dot3_(n, view);
    o->fresnel = sh4der_pl4nar_fresnel(ndv2, m->f0);
    ndv2 += ndv2;
    r[0] = sh4der_fma_(ndv2, n[0], -view[0]);
    r[1] = sh4der_fma_(ndv2, n[1], -view[1]);
    r[2] = sh4der_fma_(ndv2, n[2], -view[2]);
    o->down = sh4der_sat_(-sh4der_dot3_(r, v->plane_n));
    {
        const sh4der_pl4nar_row_t c = sh4der_pl4nar_xform_(v, sh4der_fma_(r[0], m->reach, pos[0]), sh4der_fma_(r[1], m->reach, pos[1]),
                                                           sh4der_fma_(r[2], m->reach, pos[2]));
        float x = 0.0f, y = 0.0f, t = 0.0f;

        if (c.w > SH4DER_PL4NAR_W_MIN) {
            const float iw = sh4der_recip_pos_(c.w);

            x = sh4der_sat_(c.x * iw * v->inv_w);            /* hand: scalar products */
            y = sh4der_sat_(c.y * iw * v->inv_h);
            t = sh4der_sat_(c.w * v->inv_fade);
        }
        o->u = sh4der_lerp_(SH4DER_PL4NAR_SKY_U, x, t);
        o->v = sh4der_lerp_(SH4DER_PL4NAR_SKY_V, y, t);
    }
}

/** \brief One vertex of a body near the plane. pos, n: world, n unit (a smoothed normal reads best: see
    tools/pl4nar_normals.py). XMTRX holds the view (sh4der_pl4nar_begin).
    Cost: one normalise (FSRRA), two dots, one FTRV, one reciprocal. */
static inline void sh4der_pl4nar_shade(const sh4der_pl4nar_view_t *v, const float pos[3], const float n[3],
                                       const sh4der_pl4nar_material_t *m, sh4der_pl4nar_sample_t *o) {
    sh4der_pl4nar_shade_(v, pos, n, m, o);
}

/** \brief count vertices of a body: sh4der_pl4nar_shade over two strided arrays (float[3] every pos_stride and
    n_stride bytes, both multiples of 4, both arrays 4-byte aligned), the view loaded here (and left in XMTRX), the
    next vertex's cache line asked for ahead. Returns the vertices done: count, or 0 for a NULL pointer or a stride
    that is not a multiple of 4 (nothing is written). */
static inline unsigned sh4der_pl4nar_shade_n(const sh4der_pl4nar_view_t *v, const float *pos, size_t pos_stride, const float *n,
                                             size_t n_stride, const sh4der_pl4nar_material_t *m, sh4der_pl4nar_sample_t *out,
                                             unsigned count) {
    const size_t ps = pos_stride / sizeof(float), ns = n_stride / sizeof(float);
    unsigned i;

    if (!v || !pos || !n || !m || !out || !pos_stride || !n_stride || (pos_stride & 3u) || (n_stride & 3u)) return 0;
    sh4der_pl4nar_begin(v);
    for (i = 0; i < count; i++) {
        const float *p = pos + (size_t)i * ps, *q = n + (size_t)i * ns;

        __builtin_prefetch(p + ps);                          /* a hint: past the end it reads nothing */
        __builtin_prefetch(q + ns);
        sh4der_pl4nar_shade_(v, p, q, m, &out[i]);
    }
    return count;
}

/** \brief One vertex of a surface that lies IN the plane (water, a wet road, a floor, a window): the exact planar
    reflection, the texel under the vertex's own position in the mirrored picture. The normal is the plane's; the
    reach, the fade and ground_keep play no part. XMTRX holds the view. Returns the vertex's view depth (for
    sh4der_pl4nar_tri_split); at or under SH4DER_PL4NAR_W_MIN the vertex is behind the camera, its share is 0 and
    the caller's clipper should have cut it. Cost: one normalise, one dot, one FTRV, one reciprocal. */
static inline float sh4der_pl4nar_flat(const sh4der_pl4nar_view_t *v, const float pos[3], const sh4der_pl4nar_material_t *m,
                                       sh4der_pl4nar_sample_t *o) {
    const sh4der_pl4nar_row_t c = sh4der_pl4nar_xform_(v, pos[0], pos[1], pos[2]);
    float view[3] = { v->eye[0] - pos[0], v->eye[1] - pos[1], v->eye[2] - pos[2] };

    o->down = 0.0f;
    if (!(c.w > SH4DER_PL4NAR_W_MIN)) {
        o->u = SH4DER_PL4NAR_SKY_U; o->v = SH4DER_PL4NAR_SKY_V; o->fresnel = 0.0f;
        return c.w;
    }
    sh4der_norm3_(view);
    o->fresnel = sh4der_pl4nar_fresnel(sh4der_dot3_(v->plane_n, view), m->f0);
    {
        const float iw = sh4der_recip_pos_(c.w);

        o->u = sh4der_sat_(c.x * iw * v->inv_w);             /* hand: scalar products */
        o->v = sh4der_sat_(c.y * iw * v->inv_h);
    }
    return c.w;
}

/** \brief count vertices of a surface in the plane: sh4der_pl4nar_flat over a strided array (float[3] every
    pos_stride bytes, a multiple of 4), the view loaded here (and left in XMTRX). w: count view depths out, or NULL.
    Returns count, or 0 for a NULL pointer or a bad stride (nothing is written). */
static inline unsigned sh4der_pl4nar_flat_n(const sh4der_pl4nar_view_t *v, const float *pos, size_t pos_stride,
                                            const sh4der_pl4nar_material_t *m, sh4der_pl4nar_sample_t *out, float *w, unsigned count) {
    const size_t ps = pos_stride / sizeof(float);
    unsigned i;

    if (!v || !pos || !m || !out || !pos_stride || (pos_stride & 3u)) return 0;
    sh4der_pl4nar_begin(v);
    for (i = 0; i < count; i++) {
        const float d = sh4der_pl4nar_flat(v, pos + (size_t)i * ps, m, &out[i]);

        if (w) w[i] = d;
    }
    return count;
}

/** \brief The edge of a flat triangle to cut, or -1 for none. w: the three view depths sh4der_pl4nar_flat returned.
    Edge k joins corner k and corner (k + 1) % 3; the answer is the edge whose far / near depth is largest, when
    that is over ratio (SH4DER_PL4NAR_SPLIT_RATIO). Cut it at its middle in world space, shade the new vertex, and
    ask again for both halves; stop at a depth of your own (4 is 16 triangles from one). A corner at or behind the
    camera gives -1: clip first.

    Why: the texture coordinate is a screen position, a straight line on screen, but the PVR walks it straight in
    the world. Along an edge whose depths are in the ratio q the two differ by at most (sqrt(q) - 1) / (sqrt(q) + 1)
    of the edge's length on screen: 5.6 % at 1.25, 1.2 % at 1.05. */
static inline int sh4der_pl4nar_tri_split(const float w[3], float ratio) {
    float best_hi = 0.0f, best_lo = 1.0f;
    int k, edge = -1;

    for (k = 0; k < 3; k++)
        if (!sh4der_finite_(w[k]) || !(w[k] > SH4DER_PL4NAR_W_MIN)) return -1;
    for (k = 0; k < 3; k++) {
        const float a = w[k], b = w[(k + 1) % 3], hi = a > b ? a : b, lo = a > b ? b : a;

        if (hi > ratio * lo && hi * best_lo > best_hi * lo) { best_hi = hi; best_lo = lo; edge = k; }   /* hand: compares, no divide */
    }
    return edge;
}

/** \brief The layer's vertex alpha, 0 .. 255: the Fresnel share x gain, less where a body's ray meets the plane
    itself (ground_keep). */
static inline unsigned sh4der_pl4nar_alpha(const sh4der_pl4nar_sample_t *s, const sh4der_pl4nar_material_t *m) {
    const float a = sh4der_sat_(s->fresnel * m->gain * sh4der_lerp_(1.0f, m->ground_keep, s->down));   /* hand: scalar products */

    return (unsigned)(a * 255.0f + 0.5f);
}

/** \brief The layer's packed vertex colour: sh4der_pl4nar_alpha in the top byte, the material's tint below. */
static inline uint32_t sh4der_pl4nar_argb(const sh4der_pl4nar_sample_t *s, const sh4der_pl4nar_material_t *m) {
    return ((uint32_t)sh4der_pl4nar_alpha(s, m) << 24) | (m->tint & 0x00FFFFFFu);
}

/* ---- the KallistiOS part (src/sh4der_pl4nar.c; nothing off the Dreamcast) ---- */

/** \brief The render target: RGB565, not twiddled, a picture of w x h pixels in a texture pitch pixels wide:
    pitch x h x 2 B of video memory. Built with SH4DER_PL4NAR_HAVE_RTT (a KallistiOS that has pvr_scene_begin_rtt,
    a sized render-to-texture scene) pitch is w. Without it the scene is pvr_scene_begin_txr, whose rows are 1024
    pixels apart: pitch is 1024, the picture is the left w pixels, and 256 x 128 costs 256 KB, not 64 KB. */
typedef struct sh4der_pl4nar_target {
    void *tex;                /**< \brief The texture (a pvr_ptr_t), or NULL. */
    uint32_t w, h, bytes;
    uint32_t pitch;           /**< \brief The texture's width in pixels: w, or 1024. */
    float u_scale;            /**< \brief w / pitch: multiply a sample's u by it when the layer's vertex is written (1 with HAVE_RTT). */
} sh4der_pl4nar_target_t;

/** \brief Called inside the mirror pass with the view loaded in XMTRX: draw the world, low detail, culled the other
    way, nothing that lies under the plane (sh4der_pl4nar_sphere_side), not the reflecting surface itself. */
typedef void (*sh4der_pl4nar_draw_fn)(const sh4der_pl4nar_view_t *view, void *user);

/** \brief Take the target from video memory (pvr_mem_malloc). w, h: powers of two, 8 .. 1024. Returns 1, or 0 for a
    bad size or no memory (tex is NULL, bytes says 0; every call below then does nothing). */
int sh4der_pl4nar_target_init(sh4der_pl4nar_target_t *t, unsigned w, unsigned h);

/** \brief Wait for the PVR to finish with the target, then free it. */
void sh4der_pl4nar_target_free(sh4der_pl4nar_target_t *t);

/** \brief Open a scene on the target (pvr_scene_begin_rtt, or pvr_scene_begin_txr). Returns 1, or 0 when there is
    no target or the scene was refused (no scene is open: do not draw, do not call sh4der_pl4nar_target_end). */
int sh4der_pl4nar_target_begin(const sh4der_pl4nar_target_t *t);

/** \brief Close the scene (pvr_scene_finish). The next scene may sample the target at once. */
void sh4der_pl4nar_target_end(void);

/** \brief The whole mirror pass: begin, load the view, call draw (which opens and closes its own lists), end.
    Call it before the screen's scene. Returns 1, or 0 with nothing drawn for a missing target, view or draw. */
int sh4der_pl4nar_pass(const sh4der_pl4nar_target_t *t, const sh4der_pl4nar_view_t *view, sh4der_pl4nar_draw_fn draw, void *user);

/** \brief Compile the layer's polygon header (hdr: a pvr_poly_hdr_t, 32 B, 4-byte aligned) for the translucent list:
    the target as a bilinear, clamped texture, colour = texel x vertex colour, alpha = vertex alpha, depth test
    greater-or-equal (the layer has the opaque surface's own depth), no depth write, no culling; the blend by
    additive (a material's field). Returns 1, or 0 with hdr zeroed for a missing target: test it with
    sh4der_poly_hdr_ok before a vertex is sent. */
int sh4der_pl4nar_layer_header(const sh4der_pl4nar_target_t *t, void *hdr, unsigned additive);

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_PL4NAR_H */
