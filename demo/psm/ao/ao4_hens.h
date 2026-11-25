/* SH4DER-PVR: ao4's hens, movers in the room (demo/psm/ao)

   demo/psm/ao/ao4_hens.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The town's four hens in scene C: what a baked AO cannot do. A hen walks over a floor whose AO was baked without
   it, so the vertex bytes and the lightmap stay as they were under it; the PVR map (method 2) draws the hen's caster
   from the sky directions every frame and darkens the floor under it.

   THE TEMPLATE is the town's hen, untouched: hero_hens.h on the include path, written by
   demo/psm/hero/tools/hero_hens.py from the town's generated mesh (140 vertices, 35 strips of four, 70 triangles;
   a caster of 16 vertices and 20 triangles, two closed boxes).

   THE WALK is the town's (../psm_town.h, psm_hen_step and the start values in psm_town_scene): peck 0.4 s, turn by a
   random angle, step 0.4 s at 0.5 m/s, turn back at the bounds; the same homes, bounds and seeds, so two hens keep
   to a pen of 1.2 x 2.7 m and two roam 2.1 x 5.6 m. What is left out: the run from a walker (the room has none).
   The town's story metres (sx, sz) are the room's (x, z) = (sz + AO4_HEN_DX, sx - AO4_HEN_DZ): the town's places
   turned a quarter and moved, so the roamers cross the rug and pass under the table and the pen is the open floor in
   front. The furniture does not stop a hen (no collision here): one can walk through an ottoman's corner.
   The walk advances in steps of 1 / 60 s whatever the frame takes, so a run repeats.

   A TILE A MOVER (AO2_DESIGN.md's movers' half; method AO4_LMAP_M). Each hen has a square of AO4_HEN_TILE pixels in
   a small atlas target, AO4_HEN_TILE_M metres a side, centred on the hen and not turning with it. Every frame the
   hen's caster is drawn into it from the K sky directions (ao4_hens_tiles: the triangles in atlas pixels), and the
   floor takes the tile as one quad, a multiply layer (ao4_hen_quad: its corners and texture coordinates). The tile
   is sized so the caster and its slides at the longest reach stay inside with a texel to spare (the host test walks
   them two minutes and checks), and the tiles stand a tile apart in the atlas, so nothing bleeds between them and
   no clip is needed. A tile is exactly one of the PVR's 32-pixel tiles.

   Needs ao4_scene.h and ao4_draw.h first. */

#ifndef AO4_HENS_H
#define AO4_HENS_H

#include "hero_hens.h"

#define AO4_HENS    4u
#define AO4_HEN_DX  0.2f
#define AO4_HEN_DZ  7.05f
#define AO4_HEN_DT  (1.0f / 60.0f)
#define AO4_HEN_TRIS (AO4_HENS * PSM_TOWN_HEN_T)
#define AO4_HEN_CTRIS (AO4_HENS * PSM_TOWN_HEN_CT)
#define AO4_HEN_ATLAS  128u                 /* the movers' atlas target, pixels a side */
#define AO4_HEN_TILE   32u
#define AO4_HEN_TILE_M 1.28f                /* 4 cm a texel */
#define AO4_HEN_LIFT   0.01f                /* the quad lies this far over the floor */

typedef struct ao4_hen {
    float x, z, bearing, tilt;              /* story metres, radians from story north clockwise, radians nose down */
    float t, b[4];                          /* the time left in its move; its bounds (story x0, x1, z0, z1) */
    int mode;                               /* 0 peck, 1 step */
    uint32_t rng;
} ao4_hen_t;

typedef struct ao4_hens {
    ao4_hen_t h[AO4_HENS];
    float owed;                             /* seconds not yet walked */
    unsigned v0, cv0;                       /* the first hen's vertices and caster vertices in the scene */
    unsigned base[6], with[6];              /* the scene's counts without and with them: vn, idxn, strips, tris, cvn, ctn */
} ao4_hens_t;

/* ../psm_town.h's start: homes, bearings, seeds, bounds */
static inline void ao4_hens_reset(ao4_hens_t *h) {
    static const float home[AO4_HENS][3] = { { 8.2f, 0.0f, 1.0f }, { 8.8f, 1.2f, 1.0f }, { 7.0f, -1.6f, 0.0f }, { 6.3f, 0.9f, 0.0f } };
    static const float pen[4] = { 7.9f, 9.1f, -0.85f, 1.85f }, loose[4] = { 5.3f, 7.4f, -3.0f, 2.6f };
    unsigned i, k;

    for (i = 0; i < AO4_HENS; i++) {
        ao4_hen_t *m = &h->h[i];

        m->x = home[i][0]; m->z = home[i][1]; m->bearing = 1.3f * (float)i; m->tilt = 0.0f;
        m->rng = 0x9e3779b9u * (i + 1u); m->t = 0.1f * (float)i; m->mode = 0;
        for (k = 0; k < 4u; k++) m->b[k] = home[i][2] > 0.0f ? pen[k] : loose[k];
    }
    h->owed = 0.0f;
}

static inline void ao4_hen_step_(ao4_hen_t *m, float dt) {
    const float pi = 3.14159265f;
    float sb, cb;

    m->t -= dt;
    if (m->mode == 0) {                     /* peck: the body tips forward and back */
        sh4der_sincos_(pi * sh4der_clamp_(1.0f - m->t * 2.5f, 0.0f, 1.0f), &sb, &cb);
        m->tilt = 0.7f * sb;
        if (m->t <= 0.0f) {
            m->rng = m->rng * 1664525u + 1013904223u;
            m->mode = 1; m->t = 0.4f; m->tilt = 0.0f;
            m->bearing += ((float)(m->rng >> 8) * (1.0f / 16777216.0f) - 0.5f) * 2.0f;
        }
    } else {                                /* a step of 0.2 m */
        sh4der_sincos_(m->bearing, &sb, &cb);
        m->x += sb * 0.5f * dt; m->z += cb * 0.5f * dt;
        m->tilt = 0.0f;
        if (m->t <= 0.0f) { m->mode = 0; m->t = 0.4f; }
    }
    if (m->x < m->b[0] || m->x > m->b[1]) { m->x = sh4der_clamp_(m->x, m->b[0], m->b[1]); m->bearing = -m->bearing; }
    if (m->z < m->b[2] || m->z > m->b[3]) { m->z = sh4der_clamp_(m->z, m->b[2], m->b[3]); m->bearing = pi - m->bearing; }
    if (m->bearing > pi) m->bearing -= 2.0f * pi;
    if (m->bearing < -pi) m->bearing += 2.0f * pi;
}

/* dt seconds of the walk, in whole steps of AO4_HEN_DT (at most 8 a call). */
static inline void ao4_hens_step(ao4_hens_t *h, float dt) {
    unsigned i, n = 0;

    h->owed += dt;
    while (h->owed >= AO4_HEN_DT && n < 8u) {
        for (i = 0; i < AO4_HENS; i++) ao4_hen_step_(&h->h[i], AO4_HEN_DT);
        h->owed -= AO4_HEN_DT; n++;
    }
    if (h->owed > AO4_HEN_DT) h->owed = 0.0f;
}

/* A hen's place in the room: x, z. */
static inline void ao4_hen_at(const ao4_hen_t *m, float *x, float *z) { *x = m->z + AO4_HEN_DX; *z = m->x - AO4_HEN_DZ; }

/* The hens' vertices and casters where they stand now. Template -z is the beak: it points along the walk. */
static inline void ao4_hens_place(ao4_scene_t *s, const ao4_hens_t *h) {
    unsigned k, i;

    for (k = 0; k < AO4_HENS; k++) {
        const ao4_hen_t *m = &h->h[k];
        float sb, cb, st, ct, hx, hz;

        sh4der_sincos_(m->bearing, &sb, &cb);
        sh4der_sincos_(m->tilt, &st, &ct);
        ao4_hen_at(m, &hx, &hz);
        for (i = 0; i < PSM_TOWN_HEN_V; i++) {      /* tipped about the hip (0.1 m up, about x), then turned: x -> (-sb, cb), z -> (-cb, -sb) */
            const float *p = psm_town_hen_v[i];
            const float y = p[1] * ct + p[2] * st + 0.1f - 0.1f * ct, z = -p[1] * st + p[2] * ct + 0.1f * st;
            const float nx = (float)psm_town_hen_n[i][0] * (1.0f / 127.0f), n1 = (float)psm_town_hen_n[i][1] * (1.0f / 127.0f), n2 = (float)psm_town_hen_n[i][2] * (1.0f / 127.0f);
            const float ny = n1 * ct + n2 * st, nz = -n1 * st + n2 * ct;
            float *o = s->p[h->v0 + k * PSM_TOWN_HEN_V + i], *on = s->n[h->v0 + k * PSM_TOWN_HEN_V + i];

            o[0] = hx - p[0] * sb - z * cb; o[1] = y; o[2] = hz + p[0] * cb - z * sb;
            on[0] = -nx * sb - nz * cb; on[1] = ny; on[2] = nx * cb - nz * sb;
        }
        for (i = 0; i < PSM_TOWN_HEN_CV; i++) {
            const float *p = psm_town_hen_cv[i];
            const float y = p[1] * ct + p[2] * st + 0.1f - 0.1f * ct, z = -p[1] * st + p[2] * ct + 0.1f * st;
            float *o = s->cv[h->cv0 + k * PSM_TOWN_HEN_CV + i];

            o[0] = hx - p[0] * sb - z * cb; o[1] = y; o[2] = hz + p[0] * cb - z * sb;
        }
    }
}

static inline void ao4_hens_counts_(const ao4_scene_t *s, unsigned c[6]) {
    c[0] = s->vn; c[1] = s->idxn; c[2] = s->strips; c[3] = s->tris; c[4] = s->cvn; c[5] = s->ctn;
}

/* The hens in or out of what the scene draws. */
static inline void ao4_hens_show(ao4_scene_t *s, const ao4_hens_t *h, int on) {
    const unsigned *c = on ? h->with : h->base;

    s->vn = c[0]; s->idxn = c[1]; s->strips = c[2]; s->tris = c[3]; s->cvn = c[4]; s->ctn = c[5];
}

/* Add the hens to a built scene (after everything else), at their start; shown. 0 when the scene has no room left. */
static inline int ao4_hens_add(ao4_scene_t *s, ao4_hens_t *h) {
    unsigned k, i, j, at;

    ao4_hens_reset(h);
    ao4_hens_counts_(s, h->base);
    if (s->vn + AO4_HENS * PSM_TOWN_HEN_V > AO4_V_MAX || s->idxn + AO4_HENS * PSM_TOWN_HEN_SV > AO4_IDX_MAX || s->strips + AO4_HENS * PSM_TOWN_HEN_S > AO4_STRIP_MAX
        || s->cvn + AO4_HENS * PSM_TOWN_HEN_CV > AO4_CV_MAX || s->ctn + AO4_HENS * PSM_TOWN_HEN_CT > AO4_CT_MAX) {
        ao4_hens_counts_(s, h->with);
        h->v0 = s->vn; h->cv0 = s->cvn;
        return 0;
    }
    h->v0 = s->vn; h->cv0 = s->cvn;
    for (k = 0; k < AO4_HENS; k++) {
        const unsigned v0 = s->vn, c0 = s->cvn;

        for (i = 0; i < PSM_TOWN_HEN_V; i++) ao4_vtx_(s, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, (unsigned)(psm_town_hen_c[i] - HERO_HEN_PAL0 + 1u) & 3u);
        for (j = 0, at = 0; j < PSM_TOWN_HEN_S; j++) {
            ao4_strip_begin_(s);
            for (i = 0; i < psm_town_hen_sl[j]; i++) ao4_strip_idx_(s, v0 + psm_town_hen_sv[at++]);
            ao4_strip_end_(s);
        }
        s->cvn += PSM_TOWN_HEN_CV;
        for (i = 0; i < PSM_TOWN_HEN_CT; i++) {
            s->ct[s->ctn][0] = (uint16_t)(c0 + psm_town_hen_ct[i][0]); s->ct[s->ctn][1] = (uint16_t)(c0 + psm_town_hen_ct[i][1]);
            s->ct[s->ctn][2] = (uint16_t)(c0 + psm_town_hen_ct[i][2]);
            s->ct_open[s->ctn] = 0u;
            s->ctn++;
        }
    }
    ao4_hens_counts_(s, h->with);
    ao4_hens_place(s, h);
    return 1;
}

/* Hen k's tile in the atlas: its first pixel. */
static inline void ao4_hen_tile(unsigned k, float *x0, float *y0) {
    *x0 = (float)((k & 1u) * 2u * AO4_HEN_TILE); *y0 = (float)(((k >> 1) & 1u) * 2u * AO4_HEN_TILE);
}

/* The hens' casters from k sky directions into their tiles, as ao4_draw_sky gives the whole scene's: atlas pixels,
   blended. The scene's hens must be placed (ao4_hens_place). Returns the triangles given to emit. */
static inline unsigned ao4_hens_tiles(const ao4_scene_t *s, const ao4_hens_t *h, unsigned k, int exaggerate, float reach, ao4_emit_t emit, void *user) {
    const uint32_t argb = ao4_map_argb(k, exaggerate);
    const float scale = (float)AO4_HEN_TILE / AO4_HEN_TILE_M;
    unsigned m, d, i, n = 0;

    for (m = 0; m < AO4_HENS; m++) {
        const float (*cv)[3] = &s->cv[h->cv0 + m * PSM_TOWN_HEN_CV];
        float hx, hz, tx, ty;

        ao4_hen_at(&h->h[m], &hx, &hz);
        ao4_hen_tile(m, &tx, &ty);
        tx += 0.5f * (float)AO4_HEN_TILE - hx * scale; ty += 0.5f * (float)AO4_HEN_TILE - hz * scale;
        for (d = 0; d < k; d++) {
            float slope[2], xy[PSM_TOWN_HEN_CV][2];

            ao_slope(k, d, slope);
            slope[0] *= reach * scale; slope[1] *= reach * scale;
            for (i = 0; i < PSM_TOWN_HEN_CV; i++) {      /* where the point's shadow lands on the floor, in the tile */
                xy[i][0] = tx + cv[i][0] * scale - cv[i][1] * slope[0];
                xy[i][1] = ty + cv[i][2] * scale - cv[i][1] * slope[1];
            }
            for (i = 0; i < PSM_TOWN_HEN_CT; i++) {
                const unsigned short *t = psm_town_hen_ct[i];

                if (!ao_tri_ok(xy[t[0]], xy[t[1]], xy[t[2]], (float)AO4_HEN_ATLAS, AO4_CULL)) continue;
                emit(user, xy[t[0]], xy[t[1]], xy[t[2]], ao_depth(d), argb, 1);
                n++;
            }
        }
    }
    return n;
}

/* Corner c (0 .. 3, round the square) of hen k's quad on the floor, and its place in the atlas as texture
   coordinates 0 .. 1. */
static inline void ao4_hen_quad(const ao4_hens_t *h, unsigned k, unsigned c, float p[3], float uv[2]) {
    const float a = (c == 1u || c == 2u) ? 1.0f : 0.0f, b = c >= 2u ? 1.0f : 0.0f;
    float hx, hz, tx, ty;

    ao4_hen_at(&h->h[k], &hx, &hz);
    ao4_hen_tile(k, &tx, &ty);
    p[0] = hx + (a - 0.5f) * AO4_HEN_TILE_M; p[1] = AO4_HEN_LIFT; p[2] = hz + (b - 0.5f) * AO4_HEN_TILE_M;
    uv[0] = (tx + a * (float)AO4_HEN_TILE) * (1.0f / (float)AO4_HEN_ATLAS); uv[1] = (ty + b * (float)AO4_HEN_TILE) * (1.0f / (float)AO4_HEN_ATLAS);
}

#endif /* AO4_HENS_H */
