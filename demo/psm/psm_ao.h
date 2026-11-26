/* SH4DER-PVR: baked ambient occlusion for a psm scene (the walled square): a byte a static model vertex and a byte grid
   over the ground, made once at load on the SH-4, then one table read a vertex

   demo/psm/psm_ao.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   RECOMMENDED (no console run: nothing here is measured; wiki/limits/header-recommended.md)
   Cost:       at load, once a scene. On the town's mesh, counted on the host (exact): 190,884 rays, 1,594,130
               ray-triangle tests, 69 ms on a desktop; the SH-4's time is not measured (the AO row shows it). RAM: a
               byte a model vertex (12 KB) and the ground's grid (70 x 103 bytes there). A frame: one more table
               read and a saturating add a vertex: ESTIMATE
   Start with: PSM_AO_RAYS = 12, PSM_AO_REACH = 1.6f, PSM_AO_GCELL = 0.5f; psm_ao_bake at load, psm_ao_frame a frame
   Status:     ADVISED: host tested (test/psm_ao_test.c); the bake's time and the look on a console are OPEN
   Do:         multiply the ambient light only; give psm_ao_bake a tick so a watchdog stays quiet over the bake
   Do not:     bake in a frame; bake the movers (people and hens stay open, 255)
   Maturity:   PROTOTYPE v0.1

   The method of ao/ao4_scene.h (ao4_bake) on a triangle mesh: from each static model vertex PSM_AO_RAYS cosine-weighted
   rays over its hemisphere, each against the scene's static triangles and the ground (y 0); a hit at t metres closes
   the ray by 1 - t / PSM_AO_REACH; the byte is 255 open .. 0 closed. The ground has no vertices of its own (its mesh
   moves with the camera), so it gets a grid of bytes, PSM_AO_GCELL a cell, over the scene's walk area and a margin;
   psm_ao_ground_fill gives each ground vertex its byte when the ground's positions change, not every frame. The movers
   (people, hens) are open: 255.

   The occlusion multiplies the AMBIENT light only: colour = albedo x (ambient x ao + sun x direct). psm_colour
   (psm_scene.h) does that when the frame has the bytes (psm_frame_t.mao, gao_on); without them it is as it always was.

   Cost. At load: the rays below, each against the triangles of the 1 to 4 cells of a PSM_AO_CELL grid its reach
   touches (psm_ao_t.rays, .tests count them; the program times the bake and shows it on the AO row). RAM: a byte a
   model vertex (12 KB), the ground's grid (a byte a 0.5 m cell), and, during the bake only, the triangles' boxes and
   cell lists. A frame: one more table read and a saturating add a vertex. ESTIMATE until a console runs it.

   The maths goes through include/sh4der_core.h. Host tested: test/psm_ao_test.c. */

#ifndef PSM_AO_H
#define PSM_AO_H

#include <stdlib.h>
#include "psm_scene.h"

#define PSM_AO_RAYS   12u
#define PSM_AO_REACH  1.6f               /* metres a ray looks for an occluder */
#define PSM_AO_PUSH   0.03f              /* a ray starts this far off its surface */
#define PSM_AO_CELL   2.0f               /* the triangles' grid, metres: not under the reach, so a ray touches 4 cells at most */
#define PSM_AO_GCELL  0.5f               /* the ground's grid of bytes, metres */
#define PSM_AO_MARGIN 8.0f               /* the ground's grid goes this far past the walk area */
#define PSM_AO_GMAX   (256u * 256u)      /* its most cells */

typedef struct psm_ao {
    int ok;                              /* the bake ran; 0: the bytes are all open */
    uint8_t mao[PSM_MV_MAX];             /* a model vertex: 255 open .. 0 closed */
    uint8_t *g;                          /* the ground's grid, gw x gh, row by row along x; NULL: none (open) */
    unsigned gw, gh;
    float gx0, gz0;
    uint32_t rays, tests;                /* rays cast, ray-triangle tests made */
    /* the bake's scratch: gone when it returns */
    float (*box)[6];
    uint32_t *cell0;
    uint16_t *list;
    float cx0, cz0;
    unsigned cw, ch;
    float dir[PSM_AO_RAYS][3];
} psm_ao_t;

static inline void psm_ao_free(psm_ao_t *a) { free(a->g); a->g = NULL; a->gw = a->gh = 0u; a->ok = 0; memset(a->mao, 255, sizeof a->mao); }

/* A ray against a triangle, both sides: the distance, or a negative number for none within (1e-4, reach). */
static inline float psm_ao_tri_(const float o[3], const float d[3], const float a[3], const float b[3], const float c[3], float reach) {
    const float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
    float h[3], s[3], q[3], det, inv, u, v, t;

    sh4der_cross_(d, e2, h);
    det = sh4der_dot3_(e1, h);
    if (det > -1e-9f && det < 1e-9f) return -1.0f;
    inv = det > 0.0f ? sh4der_recip_pos_(det) : -sh4der_recip_pos_(-det);
    s[0] = o[0] - a[0]; s[1] = o[1] - a[1]; s[2] = o[2] - a[2];
    u = sh4der_dot3_(s, h) * inv;
    if (u < 0.0f || u > 1.0f) return -1.0f;
    sh4der_cross_(s, e1, q);
    v = sh4der_dot3_(d, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return -1.0f;
    t = sh4der_dot3_(e2, q) * inv;
    return t > 1e-4f && t < reach ? t : -1.0f;
}

/* The nearest hit of a ray within the reach, against the ground and the triangles of the cells its box touches: the
   ray's closed share, 0 (nothing) .. 1 (a hit at its start). */
static inline float psm_ao_ray_(psm_ao_t *a, const psm_scene_t *s, const float o[3], const float d[3], int ground) {
    const float e[3] = { o[0] + d[0] * PSM_AO_REACH, o[1] + d[1] * PSM_AO_REACH, o[2] + d[2] * PSM_AO_REACH };
    const float lo[3] = { o[0] < e[0] ? o[0] : e[0], o[1] < e[1] ? o[1] : e[1], o[2] < e[2] ? o[2] : e[2] };
    const float hi[3] = { o[0] > e[0] ? o[0] : e[0], o[1] > e[1] ? o[1] : e[1], o[2] > e[2] ? o[2] : e[2] };
    const float ic = 1.0f / PSM_AO_CELL;
    int x0 = (int)((lo[0] - a->cx0) * ic), x1 = (int)((hi[0] - a->cx0) * ic), z0 = (int)((lo[2] - a->cz0) * ic), z1 = (int)((hi[2] - a->cz0) * ic), x, z;
    float best = PSM_AO_REACH;

    a->rays++;
    if (ground && d[1] < -1e-6f) {
        const float t = o[1] * sh4der_recip_pos_(-d[1]);

        if (t < best) best = t > 0.0f ? t : 0.0f;
    }
    if (lo[0] < a->cx0) x0 = 0;
    if (lo[2] < a->cz0) z0 = 0;
    if (x1 >= (int)a->cw) x1 = (int)a->cw - 1;
    if (z1 >= (int)a->ch) z1 = (int)a->ch - 1;
    for (z = z0; z <= z1; z++)
        for (x = x0; x <= x1; x++) {
            const unsigned c = (unsigned)z * a->cw + (unsigned)x;
            uint32_t k;

            for (k = a->cell0[c]; k < a->cell0[c + 1u]; k++) {
                const unsigned t = a->list[k];
                const float *b = a->box[t];
                float hit;

                if (b[0] > hi[0] || b[3] < lo[0] || b[1] > hi[1] || b[4] < lo[1] || b[2] > hi[2] || b[5] < lo[2]) continue;
                a->tests++;
                hit = psm_ao_tri_(o, d, s->mv[s->mt[t][0]], s->mv[s->mt[t][1]], s->mv[s->mt[t][2]], best);
                if (hit >= 0.0f) best = hit;
            }
        }
    return 1.0f - best * (1.0f / PSM_AO_REACH);
}

/* The byte at point p with unit normal n. */
static inline uint8_t psm_ao_at_(psm_ao_t *a, const psm_scene_t *s, const float p[3], const float n[3], int ground) {
    const float up[3] = { 0.0f, 1.0f, 0.0f }, side[3] = { 1.0f, 0.0f, 0.0f };
    const float o[3] = { p[0] + n[0] * PSM_AO_PUSH, p[1] + n[1] * PSM_AO_PUSH, p[2] + n[2] * PSM_AO_PUSH };
    float t[3], b[3], shut = 0.0f;
    unsigned i;

    sh4der_cross_(n, n[1] > 0.9f || n[1] < -0.9f ? side : up, t);
    sh4der_norm3_(t);
    sh4der_cross_(n, t, b);
    for (i = 0; i < PSM_AO_RAYS; i++) {
        const float *l = a->dir[i];
        const float d[3] = { t[0] * l[0] + b[0] * l[1] + n[0] * l[2], t[1] * l[0] + b[1] * l[1] + n[1] * l[2], t[2] * l[0] + b[2] * l[1] + n[2] * l[2] };

        shut += psm_ao_ray_(a, s, o, d, ground);
    }
    return (uint8_t)(sh4der_sat_(1.0f - shut * (1.0f / (float)PSM_AO_RAYS)) * 255.0f + 0.5f);
}

/* The bake. `tick` (NULL: none) is called now and then, for a program that must show it is alive. 1: it ran; 0: no
   memory or nothing static, and the bytes are all open. A second call bakes again (another scene). */
static inline int psm_ao_bake(psm_ao_t *a, const psm_scene_t *s, void (*tick)(void)) {
    const unsigned vn = s->mdyn0 < s->mvn ? s->mdyn0 : s->mvn, tn = s->tdyn0 < s->mtn ? s->tdyn0 : s->mtn;
    const float ic = 1.0f / PSM_AO_CELL;
    float lo[2] = { 1e9f, 1e9f }, hi[2] = { -1e9f, -1e9f };
    unsigned i, k, pass;
    uint32_t refs = 0;

    psm_ao_free(a);
    a->rays = a->tests = 0u;
    if (!vn || !tn) return 0;
    for (i = 0; i < PSM_AO_RAYS; i++) {                     /* cosine-weighted, the same for every vertex: a spiral over the disc */
        const float r2 = ((float)i + 0.5f) * (1.0f / (float)PSM_AO_RAYS);
        float sn, cs;

        sh4der_sincos_(6.2831853f * sh4der_frac_((float)i * 0.61803399f), &sn, &cs);
        a->dir[i][0] = sh4der_sqrt_(r2) * cs; a->dir[i][1] = sh4der_sqrt_(r2) * sn; a->dir[i][2] = sh4der_sqrt_(1.0f - r2);
    }
    a->box = (float (*)[6])malloc((size_t)tn * sizeof a->box[0]);
    if (!a->box) return 0;
    for (i = 0; i < tn; i++) {
        float *b = a->box[i];

        for (k = 0; k < 3u; k++) {
            const float p0 = s->mv[s->mt[i][0]][k], p1 = s->mv[s->mt[i][1]][k], p2 = s->mv[s->mt[i][2]][k];

            b[k] = p0 < p1 ? (p0 < p2 ? p0 : p2) : (p1 < p2 ? p1 : p2);
            b[3u + k] = p0 > p1 ? (p0 > p2 ? p0 : p2) : (p1 > p2 ? p1 : p2);
        }
        if (b[0] < lo[0]) lo[0] = b[0];
        if (b[2] < lo[1]) lo[1] = b[2];
        if (b[3] > hi[0]) hi[0] = b[3];
        if (b[5] > hi[1]) hi[1] = b[5];
    }
    a->cx0 = lo[0]; a->cz0 = lo[1];
    a->cw = (unsigned)((hi[0] - lo[0]) * ic) + 1u; a->ch = (unsigned)((hi[1] - lo[1]) * ic) + 1u;
    a->cell0 = (uint32_t *)calloc((size_t)a->cw * a->ch + 1u, sizeof a->cell0[0]);
    a->list = NULL;
    if (!a->cell0 || a->cw * a->ch > 65536u) { free(a->box); free(a->cell0); a->box = NULL; a->cell0 = NULL; return 0; }
    for (pass = 0; pass < 2u; pass++) {                     /* count, then fill: a triangle into every cell its box touches */
        if (pass) {
            uint32_t sum = 0;

            for (i = 0; i < a->cw * a->ch; i++) { const uint32_t c = a->cell0[i]; a->cell0[i] = sum; sum += c; }
            a->cell0[a->cw * a->ch] = refs = sum;
            a->list = (uint16_t *)malloc((size_t)(refs ? refs : 1u) * sizeof a->list[0]);
            if (!a->list) { free(a->box); free(a->cell0); a->box = NULL; a->cell0 = NULL; return 0; }
        }
        for (i = 0; i < tn; i++) {
            const float *b = a->box[i];
            const unsigned x0 = (unsigned)((b[0] - lo[0]) * ic), x1 = (unsigned)((b[3] - lo[0]) * ic), z0 = (unsigned)((b[2] - lo[1]) * ic), z1 = (unsigned)((b[5] - lo[1]) * ic);
            unsigned x, z;

            for (z = z0; z <= z1 && z < a->ch; z++)
                for (x = x0; x <= x1 && x < a->cw; x++) {
                    if (pass) a->list[a->cell0[z * a->cw + x]++] = (uint16_t)i;
                    else a->cell0[z * a->cw + x]++;
                }
        }
    }
    for (i = a->cw * a->ch; i > 0u; i--) a->cell0[i] = a->cell0[i - 1u];   /* the fill moved each start to its end: back */
    a->cell0[0] = 0u;

    for (i = 0; i < vn; i++) {
        a->mao[i] = psm_ao_at_(a, s, s->mv[i], s->mn[i], 1);
        if (tick && (i & 255u) == 255u) tick();
    }
    {                                                       /* the ground's grid: the walk area and a margin, the normal up */
        const float up[3] = { 0.0f, 1.0f, 0.0f }, ig = 1.0f / PSM_AO_GCELL;
        unsigned x, z;

        a->gx0 = s->x0 - PSM_AO_MARGIN; a->gz0 = s->z0 - PSM_AO_MARGIN;
        a->gw = (unsigned)((s->x1 - s->x0 + 2.0f * PSM_AO_MARGIN) * ig) + 1u; a->gh = (unsigned)((s->z1 - s->z0 + 2.0f * PSM_AO_MARGIN) * ig) + 1u;
        if (a->gw * a->gh <= PSM_AO_GMAX && s->x1 > s->x0 && s->z1 > s->z0) a->g = (uint8_t *)malloc((size_t)a->gw * a->gh);
        if (!a->g) a->gw = a->gh = 0u;
        for (z = 0; z < a->gh; z++) {
            for (x = 0; x < a->gw; x++) {
                const float p[3] = { a->gx0 + ((float)x + 0.5f) * PSM_AO_GCELL, 0.0f, a->gz0 + ((float)z + 0.5f) * PSM_AO_GCELL };

                a->g[z * a->gw + x] = psm_ao_at_(a, s, p, up, 0);
            }
            if (tick && (z & 7u) == 7u) tick();
        }
    }
    free(a->box); free(a->cell0); free(a->list);
    a->box = NULL; a->cell0 = NULL; a->list = NULL;
    a->ok = 1;
    return 1;
}

/* The ground's byte at (x, z): the cell's, 255 outside the grid. */
static inline uint8_t psm_ao_ground(const psm_ao_t *a, float x, float z) {
    const float ig = 1.0f / PSM_AO_GCELL, fx = (x - a->gx0) * ig, fz = (z - a->gz0) * ig;

    if (!a->g || !(fx >= 0.0f) || !(fz >= 0.0f) || !(fx < (float)a->gw) || !(fz < (float)a->gh)) return 255u;
    return a->g[(unsigned)fz * a->gw + (unsigned)fx];
}

/* The frame's AO for psm_colour: the model's bytes, and each ground vertex's byte, read again only when the ground's
   positions changed (gpos_gen). a NULL (or not baked): off, the colours as they always were. Call before psm_colour. */
static inline void psm_ao_frame(psm_frame_t *f, const psm_ao_t *a) {
    unsigned i;

    if (!a || !a->ok) { f->mao = NULL; f->gao_on = 0; return; }
    f->mao = a->mao;
    if (!f->gpos_ok || !a->g) { f->gao_on = 0; return; }
    if (!f->gao_on || f->gao_gen != f->gpos_gen || f->gao_n != f->gvn) {
        for (i = 0; i < f->gvn; i++) f->gao[i] = psm_ao_ground(a, f->gpos[i][0], f->gpos[i][2]);
        f->gao_gen = f->gpos_gen; f->gao_n = f->gvn;
    }
    f->gao_on = 1;
}

#endif /* PSM_AO_H */
