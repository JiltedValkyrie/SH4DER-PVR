/* SH4DER-PVR: ao4, the split-screen AO demo: its scenes, the baked AO and the light (demo/psm/ao)

   demo/psm/ao/ao4_scene.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Shared by the Dreamcast program (ao4_spike.c) and the host test and preview (test/ao4_scene_test.c).

   SCENES. Simple shapes on a flat ground at y 0, 12 m square, tessellated a third of a metre so a colour a vertex can
   show a contact shadow:
     A  a big sphere, a small sphere beside it and a box: the classic picture
     B  two upright walls meeting at a right angle with a sphere in the corner
   A scene is: vertices with normals (the ground's first), strips of indices, caster triangles for the shadow target
   (closed shapes wound one way, so they can be drawn one side: ../ao_layer.h), and the same shapes as exact
   occluders (spheres, boxes, the ground) for the bake.

   METHOD 1, BAKED (ao4_bake). A byte a vertex, 255 open .. 0 closed: AO4_RAYS cosine-weighted rays over the vertex's
   hemisphere (a fixed Hammersley set; the rays and the weight 1 - t / reach of ../tools/ao_bake.c), each against the
   exact occluders. A start inside an occluder (the ground right under a sphere) is closed. It multiplies the ambient
   light only. Run once at load; nothing a frame but one multiply a vertex.

   METHOD 2, THE PVR MAP. ../ao_layer.h: the casters drawn into the sun's shadow target from k sky directions, plain
   blending, one side. The ground only. This file has the software model of that target for the host preview.

   THE LIGHT (ao4_light). colour = albedo x (ambient x ao + sun x max(N.L, 0)), ao from the bake for method 1, else 1.
   Looks: CLAY (one grey, the ambient strong, the sun weak: AO shows), COLOUR (a colour a shape, the sun stronger),
   AO ONLY (white x ao, no sun). EXAGGERATE doubles the occlusion (ao' = 1 - 2 (1 - ao), not under 0).

   The maths goes through include/sh4der_core.h (SH4ZAM on the Dreamcast, plain C under SH4DER_CORE_NO_SH4ZAM).
   A model and a picture, not a measurement. */

#ifndef AO4_SCENE_H
#define AO4_SCENE_H

#include <stdint.h>
#include <string.h>
#include "sh4der_core.h"
#include "../ao_layer.h"

#define AO4_SPAN      6.0f                  /* the ground: x, z in -AO4_SPAN .. AO4_SPAN */
#define AO4_GRID      36u                   /* ground cells a side: a third of a metre */
#define AO4_SIZE      256u                  /* the shadow target */
#define AO4_V_MAX     8192u
#define AO4_IDX_MAX   24576u
#define AO4_STRIP_MAX 8192u
#define AO4_CV_MAX    4096u
#define AO4_CT_MAX    4096u
#define AO4_RAYS      64u
#define AO4_REACH     3.0f                  /* metres a ray looks for an occluder */
#define AO4_PUSH      0.02f                 /* a ray starts this far off its surface */
#define AO4_SCENES    2u
#define AO4_REACHES   4u                    /* method 2: how far a sky direction's shadow slides, as a share of ao_layer.h's */

enum { AO4_OFF = 0, AO4_BAKED = 1, AO4_MAP = 2, AO4_LMAP = 3, AO4_LMAP_M = 4 };   /* LMAP: the room alone (ao4_room.h): a lightmap a flat
                                                                                        surface; LMAP_M: that and a tile a mover (ao4_hens.h) */
enum { AO4_CLAY = 0, AO4_COLOUR = 1, AO4_AO_ONLY = 2, AO4_LOOKS = 3 };

typedef struct ao4_sph { float c[3], r; } ao4_sph_t;
typedef struct ao4_box { float lo[3], hi[3]; } ao4_box_t;

typedef struct ao4_scene {
    float p[AO4_V_MAX][3], n[AO4_V_MAX][3];
    uint8_t ao[AO4_V_MAX];                  /* method 1: 255 open .. 0 closed */
    uint8_t tint[AO4_V_MAX];                /* COLOUR: which colour of ao4_tint */
    unsigned vn, ground_vn;                 /* vertices 0 .. ground_vn - 1 are the ground's */
    uint16_t idx[AO4_IDX_MAX];
    uint16_t slen[AO4_STRIP_MAX];
    unsigned idxn, strips, ground_strips;   /* strips 0 .. ground_strips - 1 are the ground's */
    unsigned surf_strips;                   /* strips 0 .. surf_strips - 1 are flat surfaces' (the ground, then the room's walls) */
    int no_sun_shadow;                      /* 1: the sun shades by N.L but casts nothing (the room) */
    unsigned tris;                          /* triangles drawn on screen */
    float cv[AO4_CV_MAX][3];                /* the casters */
    uint16_t ct[AO4_CT_MAX][3];
    uint8_t ct_open[AO4_CT_MAX];            /* 1: the triangle's caster is not closed: both its sides are drawn */
    unsigned cvn, ctn;
    ao4_sph_t sph[4];
    ao4_box_t box[4];
    unsigned nsph, nbox;
    float sun[3];                           /* unit, toward the sun */
    float sun_slope[2];                     /* the sun's slide: (sx, sz) = (x, z) / y */
    float cam_angle, cam_swing;             /* the orbit's middle, radians; its swing each way (0: all the way round) */
    float cam_radius, cam_height, cam_tx, cam_tz;   /* the eye's circle and what it looks at */
    int cam_rh;                             /* 1: +x is on the right looking along -z (a right-handed model: the room); 0: mirrored, as scenes A and B were drawn */
} ao4_scene_t;

static const float ao4_reach[AO4_REACHES] = { 0.35f, 0.6f, 0.8f, 1.0f };   /* 0.6: about 71 degrees of elevation; 1: ao_layer.h's 60 */
#define AO4_REACH_DEFAULT 1u
static const float ao4_map[16] = { 1.0f / AO4_SPAN, 0, 0, 0,   0, 0, 0, 0,   0, 1.0f / AO4_SPAN, 0, 0,   0, 0, 0, 1 };

/* ---- building ---------------------------------------------------------------------------------------------------- */

static inline unsigned ao4_vtx_(ao4_scene_t *s, float x, float y, float z, float nx, float ny, float nz, unsigned tint) {
    const unsigned i = s->vn;

    if (i >= AO4_V_MAX) return 0;
    s->p[i][0] = x; s->p[i][1] = y; s->p[i][2] = z;
    s->n[i][0] = nx; s->n[i][1] = ny; s->n[i][2] = nz;
    s->ao[i] = 255u; s->tint[i] = (uint8_t)tint;
    s->vn++;
    return i;
}

static inline void ao4_strip_begin_(ao4_scene_t *s) { if (s->strips < AO4_STRIP_MAX) s->slen[s->strips] = 0; }
static inline void ao4_strip_idx_(ao4_scene_t *s, unsigned i) {
    if (s->idxn < AO4_IDX_MAX && s->strips < AO4_STRIP_MAX) { s->idx[s->idxn++] = (uint16_t)i; s->slen[s->strips]++; }
}
static inline void ao4_strip_end_(ao4_scene_t *s) {
    if (s->strips < AO4_STRIP_MAX && s->slen[s->strips] >= 3u) { s->tris += s->slen[s->strips] - 2u; s->strips++; }
}

/* A flat grid: origin o, sides du and dv (whole lengths), nu x nv cells, normal n. One strip a row. */
static inline void ao4_grid(ao4_scene_t *s, const float o[3], const float du[3], const float dv[3], unsigned nu, unsigned nv, const float n[3], unsigned tint) {
    const unsigned v0 = s->vn;
    const float iu = 1.0f / (float)nu, iv = 1.0f / (float)nv;
    unsigned i, j;

    for (j = 0; j <= nv; j++)
        for (i = 0; i <= nu; i++) {
            const float a = (float)i * iu, b = (float)j * iv;

            ao4_vtx_(s, o[0] + du[0] * a + dv[0] * b, o[1] + du[1] * a + dv[1] * b, o[2] + du[2] * a + dv[2] * b, n[0], n[1], n[2], tint);
        }
    for (j = 0; j < nv; j++) {
        ao4_strip_begin_(s);
        for (i = 0; i <= nu; i++) { ao4_strip_idx_(s, v0 + (j + 1u) * (nu + 1u) + i); ao4_strip_idx_(s, v0 + j * (nu + 1u) + i); }
        ao4_strip_end_(s);
    }
}

static inline void ao4_ctri_(ao4_scene_t *s, unsigned a, unsigned b, unsigned c) {
    if (s->ctn < AO4_CT_MAX) { s->ct[s->ctn][0] = (uint16_t)a; s->ct[s->ctn][1] = (uint16_t)b; s->ct[s->ctn][2] = (uint16_t)c; s->ctn++; }
}

/* A sphere: seg segments round, rings bands pole to pole. On screen the two caps are single triangles (no triangle
   without area at a pole) and each band between is a strip. Also a caster and an occluder. */
static inline void ao4_sphere(ao4_scene_t *s, const float c[3], float r, unsigned seg, unsigned rings, unsigned tint) {
    const unsigned v0 = s->vn, c0 = s->cvn;
    unsigned i, j;

    for (j = 0; j <= rings; j++) {
        float sp, cp;

        sh4der_sincos_(3.14159265f * (float)j / (float)rings, &sp, &cp);   /* 0 at the top pole */
        for (i = 0; i <= seg; i++) {
            float st, ctt, nx, nz;

            sh4der_sincos_(6.2831853f * (float)(i % seg) / (float)seg, &st, &ctt);
            nx = sp * ctt; nz = sp * st;
            ao4_vtx_(s, c[0] + r * nx, c[1] + r * cp, c[2] + r * nz, nx, cp, nz, tint);
            if (s->cvn < AO4_CV_MAX) { s->cv[s->cvn][0] = c[0] + r * nx; s->cv[s->cvn][1] = c[1] + r * cp; s->cv[s->cvn][2] = c[2] + r * nz; s->cvn++; }
        }
    }
#define AO4_SV(j, i) ((j) * (seg + 1u) + (i))
    for (i = 0; i < seg; i++) {             /* the caps */
        ao4_strip_begin_(s);
        ao4_strip_idx_(s, v0 + AO4_SV(0u, i)); ao4_strip_idx_(s, v0 + AO4_SV(1u, i + 1u)); ao4_strip_idx_(s, v0 + AO4_SV(1u, i));
        ao4_strip_end_(s);
        ao4_strip_begin_(s);
        ao4_strip_idx_(s, v0 + AO4_SV(rings, i)); ao4_strip_idx_(s, v0 + AO4_SV(rings - 1u, i)); ao4_strip_idx_(s, v0 + AO4_SV(rings - 1u, i + 1u));
        ao4_strip_end_(s);
        ao4_ctri_(s, c0 + AO4_SV(0u, i), c0 + AO4_SV(1u, i + 1u), c0 + AO4_SV(1u, i));
        ao4_ctri_(s, c0 + AO4_SV(rings, i), c0 + AO4_SV(rings - 1u, i), c0 + AO4_SV(rings - 1u, i + 1u));
    }
    for (j = 1; j + 1u < rings; j++) {      /* the bands */
        ao4_strip_begin_(s);
        for (i = 0; i <= seg; i++) { ao4_strip_idx_(s, v0 + AO4_SV(j + 1u, i)); ao4_strip_idx_(s, v0 + AO4_SV(j, i)); }
        ao4_strip_end_(s);
        for (i = 0; i < seg; i++) {
            ao4_ctri_(s, c0 + AO4_SV(j, i), c0 + AO4_SV(j, i + 1u), c0 + AO4_SV(j + 1u, i));
            ao4_ctri_(s, c0 + AO4_SV(j, i + 1u), c0 + AO4_SV(j + 1u, i + 1u), c0 + AO4_SV(j + 1u, i));
        }
    }
#undef AO4_SV
    if (s->nsph < 4u) { s->sph[s->nsph].c[0] = c[0]; s->sph[s->nsph].c[1] = c[1]; s->sph[s->nsph].c[2] = c[2]; s->sph[s->nsph].r = r; s->nsph++; }
}

/* A box as a caster and an occluder (nothing on screen): 12 triangles wound outward. */
static inline void ao4_box_solid(ao4_scene_t *s, const float lo[3], const float hi[3]) {
    /* corner bit 0 x, bit 1 y, bit 2 z; each face seen from outside, counter-clockwise */
    static const uint8_t f[6][4] = { {0,4,6,2}, {1,3,7,5}, {0,1,5,4}, {2,6,7,3}, {0,2,3,1}, {4,5,7,6} };
    const unsigned c0 = s->cvn;
    unsigned i;

    if (s->cvn + 8u > AO4_CV_MAX || s->nbox >= 4u) return;
    for (i = 0; i < 8u; i++) {
        s->cv[c0 + i][0] = (i & 1u) ? hi[0] : lo[0]; s->cv[c0 + i][1] = (i & 2u) ? hi[1] : lo[1]; s->cv[c0 + i][2] = (i & 4u) ? hi[2] : lo[2];
    }
    s->cvn += 8u;
    for (i = 0; i < 6u; i++) { ao4_ctri_(s, c0 + f[i][0], c0 + f[i][1], c0 + f[i][2]); ao4_ctri_(s, c0 + f[i][0], c0 + f[i][2], c0 + f[i][3]); }
    memcpy(s->box[s->nbox].lo, lo, sizeof s->box[0].lo); memcpy(s->box[s->nbox].hi, hi, sizeof s->box[0].hi);
    s->nbox++;
}

/* The box's top and four sides on screen, cells x cells each. */
static inline void ao4_box_faces(ao4_scene_t *s, const float lo[3], const float hi[3], unsigned cells, unsigned tint) {
    const float dx[3] = { hi[0] - lo[0], 0, 0 }, dy[3] = { 0, hi[1] - lo[1], 0 }, dz[3] = { 0, 0, hi[2] - lo[2] };
    const float px[3] = { 1, 0, 0 }, nx[3] = { -1, 0, 0 }, py[3] = { 0, 1, 0 }, pz[3] = { 0, 0, 1 }, nz[3] = { 0, 0, -1 };
    const float o_top[3] = { lo[0], hi[1], lo[2] }, o_px[3] = { hi[0], lo[1], lo[2] }, o_pz[3] = { lo[0], lo[1], hi[2] };

    ao4_grid(s, o_top, dx, dz, cells, cells, py, tint);
    ao4_grid(s, lo, dz, dy, cells, cells, nx, tint);
    ao4_grid(s, o_px, dz, dy, cells, cells, px, tint);
    ao4_grid(s, lo, dx, dy, cells, cells, nz, tint);
    ao4_grid(s, o_pz, dx, dy, cells, cells, pz, tint);
}

static inline void ao4_sun_(ao4_scene_t *s, float x, float y, float z) {
    s->sun[0] = x; s->sun[1] = y; s->sun[2] = z;
    sh4der_norm3_(s->sun);
    s->sun_slope[0] = sh4der_div_(s->sun[0], s->sun[1]);
    s->sun_slope[1] = sh4der_div_(s->sun[2], s->sun[1]);
}

/* Scene `which` (0 A: the spheres; 1 B: the corner). The AO bytes are 255 until ao4_bake. */
static inline void ao4_build(ao4_scene_t *s, unsigned which) {
    const float g0[3] = { -AO4_SPAN, 0.0f, -AO4_SPAN }, gu[3] = { 2.0f * AO4_SPAN, 0, 0 }, gv[3] = { 0, 0, 2.0f * AO4_SPAN }, up[3] = { 0, 1, 0 };

    memset(s, 0, sizeof *s);
    ao4_grid(s, g0, gu, gv, AO4_GRID, AO4_GRID, up, 0u);
    s->ground_vn = s->vn; s->ground_strips = s->surf_strips = s->strips;
    if (which == 0u) {
        const float big[3] = { -1.0f, 2.2f, 0.4f }, small[3] = { 2.25f, 1.0f, -0.5f };
        const float lo[3] = { -4.6f, 0.0f, -3.4f }, hi[3] = { -3.2f, 1.4f, -2.0f };

        ao4_sphere(s, big, 2.2f, 24u, 14u, 1u);
        ao4_sphere(s, small, 1.0f, 20u, 12u, 2u);
        ao4_box_solid(s, lo, hi);
        ao4_box_faces(s, lo, hi, 4u, 3u);
        ao4_sun_(s, -0.55f, 0.75f, -0.35f);                  /* from the left and a little behind */
        s->cam_angle = -1.5708f; s->cam_swing = 0.0f;
        s->cam_radius = 12.5f; s->cam_height = 6.5f; s->cam_tx = 0.0f; s->cam_tz = 0.0f;
    } else {
        /* the corner at x = -4, z = 4: the walls 0.3 m thick behind those planes, 5 m high */
        const float w1lo[3] = { -4.3f, 0.0f, -4.0f }, w1hi[3] = { -4.0f, 5.0f, 4.3f };
        const float w2lo[3] = { -4.3f, 0.0f, 4.0f }, w2hi[3] = { 4.0f, 5.0f, 4.3f };
        const float o1[3] = { -4.0f, 0.0f, -4.0f }, o2[3] = { -4.0f, 0.0f, 4.0f };
        const float len[3] = { 0, 0, 8.0f }, lenx[3] = { 8.0f, 0, 0 }, hgt[3] = { 0, 5.0f, 0 };
        const float px[3] = { 1, 0, 0 }, nz[3] = { 0, 0, -1 }, ball[3] = { -2.3f, 1.5f, 2.3f };

        ao4_box_solid(s, w1lo, w1hi);
        ao4_box_solid(s, w2lo, w2hi);
        ao4_grid(s, o1, len, hgt, 24u, 15u, px, 3u);         /* the faces toward the room */
        ao4_grid(s, o2, lenx, hgt, 24u, 15u, nz, 3u);
        ao4_sphere(s, ball, 1.5f, 24u, 14u, 1u);
        ao4_sun_(s, 0.45f, 0.80f, -0.55f);                   /* into the corner, over the viewer's shoulder */
        s->cam_angle = -0.7854f; s->cam_swing = 0.55f;
        s->cam_radius = 14.0f; s->cam_height = 6.5f; s->cam_tx = -1.5f; s->cam_tz = 1.5f;
    }
}

/* ---- method 1: the bake -------------------------------------------------------------------------------------------- */

/* The nearest occluder along the ray o + t d (d unit), or AO4_REACH. 0 when o is inside one. */
static inline float ao4_ray(const ao4_scene_t *s, const float o[3], const float d[3]) {
    float best = AO4_REACH;
    unsigned i, k;

    if (d[1] < -1.0e-4f && o[1] > 0.0f) {                    /* the ground */
        const float t = sh4der_div_(-o[1], d[1]);

        if (t < best) best = t;
    }
    for (i = 0; i < s->nsph; i++) {
        const float oc[3] = { o[0] - s->sph[i].c[0], o[1] - s->sph[i].c[1], o[2] - s->sph[i].c[2] };
        const float b = sh4der_dot3_(oc, d), cc = sh4der_dot3_(oc, oc) - s->sph[i].r * s->sph[i].r;
        float disc, t;

        if (cc < 0.0f) return 0.0f;
        if (b >= 0.0f) continue;
        disc = b * b - cc;
        if (disc <= 0.0f) continue;
        t = -b - sh4der_sqrt_(disc);
        if (t >= 0.0f && t < best) best = t;
    }
    for (i = 0; i < s->nbox; i++) {
        const ao4_box_t *bx = &s->box[i];
        float t0 = 0.0f, t1 = best;
        int inside = 1, miss = 0;

        for (k = 0; k < 3u; k++) {
            if (!(o[k] > bx->lo[k] + 1.0e-3f && o[k] < bx->hi[k] - 1.0e-3f)) inside = 0;   /* on a face is outside */
            if (d[k] > 1.0e-6f || d[k] < -1.0e-6f) {
                const float inv = sh4der_div_(1.0f, d[k]);
                float a = (bx->lo[k] - o[k]) * inv, b = (bx->hi[k] - o[k]) * inv;

                if (a > b) { const float sw = a; a = b; b = sw; }
                if (a > t0) t0 = a;
                if (b < t1) t1 = b;
            } else if (o[k] < bx->lo[k] || o[k] > bx->hi[k]) miss = 1;
        }
        if (inside) return 0.0f;
        if (!miss && t0 + 1.0e-4f < t1 && t0 < best) best = t0;   /* a ray that only touches a face does not hit */
    }
    return best;
}

/* The fixed ray set over the hemisphere of +y: cosine-weighted, Hammersley (the set of ../tools/ao_bake.c). */
static inline void ao4_rays(float dirs[AO4_RAYS][3]) {
    unsigned k;

    for (k = 0; k < AO4_RAYS; k++) {
        const float u1 = ((float)k + 0.5f) * (1.0f / (float)AO4_RAYS);
        float u2 = 0.0f, f = 0.5f, sn, cs, rr;
        unsigned b = k;

        while (b) { if (b & 1u) u2 += f; f *= 0.5f; b >>= 1; }   /* the base-2 radical inverse */
        rr = sh4der_sqrt_(u1);
        sh4der_sincos_(6.2831853f * u2, &sn, &cs);
        dirs[k][0] = rr * cs; dirs[k][1] = sh4der_sqrt_(1.0f - u1); dirs[k][2] = rr * sn;
    }
}

/* The open share at a point with unit normal n: 1 open .. 0 closed. */
static inline float ao4_at(const ao4_scene_t *s, const float p[3], const float n[3], const float dirs[AO4_RAYS][3]) {
    const float ax[3] = { n[1] > 0.9f || n[1] < -0.9f ? 1.0f : 0.0f, n[1] > 0.9f || n[1] < -0.9f ? 0.0f : 1.0f, 0.0f };
    const float o[3] = { p[0] + n[0] * AO4_PUSH, p[1] + n[1] * AO4_PUSH, p[2] + n[2] * AO4_PUSH };
    float t[3], b[3], occl = 0.0f;
    unsigned k;

    sh4der_cross_(n, ax, t);
    sh4der_norm3_(t);
    sh4der_cross_(n, t, b);
    for (k = 0; k < AO4_RAYS; k++) {
        const float d[3] = { t[0] * dirs[k][0] + n[0] * dirs[k][1] + b[0] * dirs[k][2],
                             t[1] * dirs[k][0] + n[1] * dirs[k][1] + b[1] * dirs[k][2],
                             t[2] * dirs[k][0] + n[2] * dirs[k][1] + b[2] * dirs[k][2] };
        const float hit = ao4_ray(s, o, d);

        if (hit < AO4_REACH) occl += 1.0f - hit * (1.0f / AO4_REACH);
    }
    return 1.0f - occl * (1.0f / (float)AO4_RAYS);
}

/* Every vertex's AO byte. `tick` (may be NULL) is called every 64 vertices: a heartbeat for the fault catcher. */
static inline void ao4_bake(ao4_scene_t *s, void (*tick)(void)) {
    float dirs[AO4_RAYS][3];
    unsigned i;

    ao4_rays(dirs);
    for (i = 0; i < s->vn; i++) {
        s->ao[i] = (uint8_t)(sh4der_sat_(ao4_at(s, s->p[i], s->n[i], dirs)) * 255.0f + 0.5f);
        if (tick && (i & 63u) == 0u) tick();
    }
}

/* ---- the light ----------------------------------------------------------------------------------------------------- */

static const float ao4_amb[AO4_LOOKS] = { 0.72f, 0.50f, 1.00f }, ao4_sunc[AO4_LOOKS] = { 0.30f, 0.50f, 0.00f };
static const float ao4_tint[4][3] = { { 0.62f, 0.66f, 0.52f }, { 0.86f, 0.40f, 0.32f }, { 0.32f, 0.52f, 0.86f }, { 0.88f, 0.80f, 0.58f } };
#define AO4_CLAY_GREY 0.86f
#define AO4_BACKDROP_R 0.80f
#define AO4_BACKDROP_G 0.82f
#define AO4_BACKDROP_B 0.86f

/* What a ground texel in the sun's shadow is multiplied by: the ambient share of the lit ground. 1 for AO ONLY. */
static inline float ao4_shadow_share(const ao4_scene_t *s, unsigned look) {
    return sh4der_div_(ao4_amb[look], ao4_amb[look] + ao4_sunc[look] * s->sun[1]);
}

/* The occlusion byte as the light uses it: 0 .. 1, doubled under EXAGGERATE. */
static inline float ao4_open(unsigned ao, int exaggerate) {
    const float a = (float)ao * (1.0f / 255.0f);

    return exaggerate ? sh4der_sat_(2.0f * a - 1.0f) : a;
}

/* Every vertex's colour for one side of the screen. method: AO4_OFF, AO4_BAKED (the AO byte on the ambient), AO4_MAP
   (as OFF: the target carries it, on the ground), AO4_LMAP (as BAKED: the flat surfaces are drawn from their lightmaps
   instead, ao4_room.h). */
static inline void ao4_light(const ao4_scene_t *s, unsigned method, unsigned look, int exaggerate, uint32_t *argb) {
    const float amb = ao4_amb[look], sunc = ao4_sunc[look];
    unsigned i;

    for (i = 0; i < s->vn; i++) {
        const float ndl = sh4der_dot3_(s->n[i], s->sun), open = method == AO4_BAKED || method >= AO4_LMAP ? ao4_open(s->ao[i], exaggerate) : 1.0f;
        const float l = amb * open + sunc * (ndl > 0.0f ? ndl : 0.0f);
        const float *t = ao4_tint[s->tint[i] & 3u];
        float r, g, b;

        if (look == AO4_COLOUR) { r = t[0] * l; g = t[1] * l; b = t[2] * l; }
        else { r = g = b = (look == AO4_CLAY ? AO4_CLAY_GREY : 1.0f) * l; }
        argb[i] = 0xff000000u | ((uint32_t)(sh4der_sat_(r) * 255.0f + 0.5f) << 16) | ((uint32_t)(sh4der_sat_(g) * 255.0f + 0.5f) << 8)
                | (uint32_t)(sh4der_sat_(b) * 255.0f + 0.5f);
    }
}

/* Method 2's alpha for k directions: ao_layer.h's, doubled (not over 1) under EXAGGERATE. */
static inline uint32_t ao4_map_argb(unsigned k, int exaggerate) {
    float a = ao_alpha(k) * (exaggerate ? 2.0f : 1.0f);

    if (a > 1.0f) a = 1.0f;
    return (uint32_t)(a * 255.0f + 0.5f) << 24;
}

/* ---- the camera ---------------------------------------------------------------------------------------------------- */

typedef struct ao4_cam {
    float e[3], f[3], r[3], u[3], focal;
} ao4_cam_t;

/* An eye on a circle of `radius` at `height`, at `angle` round the scene's middle, looking at (tx, 1.2, tz). */
static inline void ao4_cam_make(ao4_cam_t *c, float angle, float radius, float height, float tx, float tz) {
    const float up[3] = { 0.0f, 1.0f, 0.0f };
    float sn, cs;

    sh4der_sincos_(angle, &sn, &cs);
    c->e[0] = tx + radius * cs; c->e[1] = height; c->e[2] = tz + radius * sn;
    c->f[0] = tx - c->e[0]; c->f[1] = 1.2f - c->e[1]; c->f[2] = tz - c->e[2];
    sh4der_norm3_(c->f);
    sh4der_cross_(up, c->f, c->r);
    sh4der_norm3_(c->r);
    sh4der_cross_(c->f, c->r, c->u);
    c->focal = 640.0f;
}

/* A world point on a 640 x 480 screen: x, y, 1 / depth. 0 when it is not 0.5 m in front of the eye. */
static inline int ao4_cam_pt(const ao4_cam_t *c, const float p[3], float s[3]) {
    const float d[3] = { p[0] - c->e[0], p[1] - c->e[1], p[2] - c->e[2] };
    const float depth = sh4der_dot3_(d, c->f);
    float iw;

    if (!(depth > 0.5f)) { s[0] = s[1] = 0.0f; s[2] = 2.0f; return 0; }
    iw = sh4der_recip_pos_(depth);
    s[0] = 320.0f + c->focal * sh4der_dot3_(d, c->r) * iw;
    s[1] = 240.0f - c->focal * sh4der_dot3_(d, c->u) * iw;
    s[2] = iw;
    return 1;
}

/* The scene's view at time t seconds, the pad's turn and lift added: the orbit, or the swing about the middle. */
static inline void ao4_cam_at(const ao4_scene_t *s, ao4_cam_t *c, float t, float turn, float lift) {
    float ang = s->cam_angle + turn, sn, cs, h = s->cam_height + lift;

    if (s->cam_swing > 0.0f) { sh4der_sincos_(t * 0.35f, &sn, &cs); ang += s->cam_swing * sn; }
    else ang += t * 0.22f;
    if (h < 2.0f) h = 2.0f;
    if (h > 14.0f) h = 14.0f;
    ao4_cam_make(c, ang, s->cam_radius, h, s->cam_tx, s->cam_tz);
    if (s->cam_rh) { c->r[0] = -c->r[0]; c->r[1] = -c->r[1]; c->r[2] = -c->r[2]; }
}

#endif /* AO4_SCENE_H */
