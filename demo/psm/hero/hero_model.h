/* SH4DER-PVR: hero1, a skinned low-poly figure for judging the passes' cost, model and clips

   demo/psm/hero/hero_model.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The hero: an original figure built here from tubes and a sphere (a diver's suit: white cloth, rust plates, a round
   helmet with a slit visor, a tank on the back, a tool in the right hand), 33 bones, about 1,700 triangles, sized to
   a main character's budget on this console. Two clips made here from sines: IDLE (2 s) and RUN+AIM (0.8 s), 30 frames
   a second, stored as one 3 x 4 skin matrix a bone a frame. hero_file.h reads the same data from a file instead
   (README.md), so other art can be dropped in without a change here.

   A frame: hero_pose (the clip at t: a lerp of two stored frames) -> hero_palette (each bone's skin matrix times the
   camera's: one 4 x 4 product a bone) -> hero_skin (each vertex through its bone's matrix, or two blended; the vertices
   are sorted by bone pair at build, so a matrix is loaded once a group). The light is taken into each bone's own space
   once a frame (hero_bone_dirs), so a vertex's light is one dot with its rest normal, no normal transform.

   Header only. SH4ZAM for the matrix work on the Dreamcast; SH4DER_CORE_NO_SH4ZAM: the same steps in plain C (host). */

#ifndef HERO_MODEL_H
#define HERO_MODEL_H

#include <stdint.h>
#include <string.h>
#include "sh4der_core.h"

#define HERO_BONES      33u
#define HERO_V_MAX      2048u
#define HERO_IDX_MAX    8192u
#define HERO_STRIP_MAX  1024u
#define HERO_FPS        30u
#define HERO_CLIPS      2u
#define HERO_FRAMES_MAX 64u
#define HERO_TEX_MAX    (128u * 128u)        /* texels in the largest texture the model may carry */
#define HERO_DETAIL_SCALE 4.0f               /* the detail texture's repeats over a base texture's width */
#define HERO_NOBONE     255u

enum { HERO_IDLE = 0, HERO_RUN_AIM = 1 };
enum { HERO_TEX_BASE = 0, HERO_TEX_DETAIL = 1, HERO_TEX_ENV = 2, HERO_TEXS = 3 };
enum { HERO_FMT_565 = 0, HERO_FMT_4444 = 1 };

typedef struct hero {
    unsigned vn, idx_n, strips, visor_strip0;   /* strips [0, visor_strip0): armour; [visor_strip0, strips): the visor */
    unsigned tris;                               /* triangles in all strips */
    float p[HERO_V_MAX][3], n[HERO_V_MAX][3], uv[HERO_V_MAX][2];   /* rest (bind) pose, metres; unit normals */
    uint8_t b0[HERO_V_MAX], b1[HERO_V_MAX], w0[HERO_V_MAX];       /* w0 / 255 on b0, the rest on b1 (b1 == b0: rigid) */
    uint16_t idx[HERO_IDX_MAX], slen[HERO_STRIP_MAX];
    uint8_t parent[HERO_BONES];                  /* HERO_NOBONE for the root */
    float head[HERO_BONES][3];                   /* each bone's joint at rest */
    unsigned frames[HERO_CLIPS];
    float clip[HERO_CLIPS][HERO_FRAMES_MAX][HERO_BONES][12];      /* skin matrices, 3 x 4 rows: rest -> posed, metres */
    unsigned tex_w[HERO_TEXS], tex_h[HERO_TEXS], tex_fmt[HERO_TEXS];
    uint16_t tex[HERO_TEXS][HERO_TEX_MAX];
    int from_file;
} hero_t;

/* ---- small matrix helpers: 4 x 4 column-major (SH4ZAM's layout), 3 x 4 row-major for the clips ---------------------- */

static inline void hero_m4_mul_(float o[16], const float a[16], const float b[16]) {   /* o = a b (plain C) */
    unsigned c, r;

    for (c = 0; c < 4u; c++)
        for (r = 0; r < 4u; r++)
            o[c * 4u + r] = a[r] * b[c * 4u] + a[4u + r] * b[c * 4u + 1u] + a[8u + r] * b[c * 4u + 2u] + a[12u + r] * b[c * 4u + 3u];
}

static inline void hero_m4_of34_(float o[16], const float m[12]) {   /* a 3 x 4 row-major matrix as a 4 x 4 column-major one */
    o[0] = m[0]; o[4] = m[1]; o[8] = m[2];  o[12] = m[3];
    o[1] = m[4]; o[5] = m[5]; o[9] = m[6];  o[13] = m[7];
    o[2] = m[8]; o[6] = m[9]; o[10] = m[10]; o[14] = m[11];
    o[3] = 0.0f; o[7] = 0.0f; o[11] = 0.0f; o[15] = 1.0f;
}

static inline void hero_m34_mul_(float o[12], const float a[12], const float b[12]) {   /* o = a b, both 3 x 4 affine */
    unsigned r;

    for (r = 0; r < 3u; r++) {
        const float *x = a + r * 4u;

        o[r * 4u + 0] = x[0] * b[0] + x[1] * b[4] + x[2] * b[8];
        o[r * 4u + 1] = x[0] * b[1] + x[1] * b[5] + x[2] * b[9];
        o[r * 4u + 2] = x[0] * b[2] + x[1] * b[6] + x[2] * b[10];
        o[r * 4u + 3] = x[0] * b[3] + x[1] * b[7] + x[2] * b[11] + x[3];
    }
}

static inline void hero_m34_rot_(float o[12], float ax, float ay, float az, float tx, float ty, float tz) {   /* T Ry Rx Rz */
    float sx, cx, sy, cy, sz, cz;

    sh4der_sincos_(ax, &sx, &cx); sh4der_sincos_(ay, &sy, &cy); sh4der_sincos_(az, &sz, &cz);
    o[0] = cy * cz + sy * sx * sz; o[1] = -cy * sz + sy * sx * cz; o[2] = sy * cx;  o[3] = tx;
    o[4] = cx * sz;                o[5] = cx * cz;                 o[6] = -sx;      o[7] = ty;
    o[8] = -sy * cz + cy * sx * sz; o[9] = sy * sz + cy * sx * cz; o[10] = cy * cx; o[11] = tz;
}

/* ---- the build ------------------------------------------------------------------------------------------------- */

/* A bone: parent, joint, the end its tube runs to, radii at the two ends, sides (0: no tube), rings, the material (an
   atlas cell of the base texture), a width factor across the body (torso plates are wider than deep). */
typedef struct hero_bone_def {
    uint8_t parent;
    float head[3], tail[3], r0, r1;
    uint8_t sides, rings, mat;
    float wide;
} hero_bone_def_t;

enum { HERO_M_CLOTH, HERO_M_PLATE, HERO_M_JOINT, HERO_M_HELMET, HERO_M_TANK, HERO_M_BOOT, HERO_M_TOOL, HERO_M_VISOR };

#define HERO_L 0.25f
static const hero_bone_def_t hero_def[HERO_BONES] = {
    /*  0 root     */ { HERO_NOBONE, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 0, 1 },
    /*  1 pelvis   */ { 0, { 0, 0.95f, 0 }, { 0, 1.10f, 0 }, 0.17f, 0.16f, 10, 2, HERO_M_PLATE, 1.25f },
    /*  2 spine1   */ { 1, { 0, 1.10f, 0 }, { 0, 1.25f, 0 }, 0.15f, 0.17f, 10, 2, HERO_M_CLOTH, 1.2f },
    /*  3 spine2   */ { 2, { 0, 1.25f, 0 }, { 0, 1.40f, 0 }, 0.18f, 0.21f, 10, 2, HERO_M_PLATE, 1.3f },
    /*  4 chest    */ { 3, { 0, 1.40f, 0 }, { 0, 1.56f, 0 }, 0.22f, 0.17f, 10, 3, HERO_M_PLATE, 1.35f },
    /*  5 neck     */ { 4, { 0, 1.56f, 0 }, { 0, 1.63f, 0 }, 0.075f, 0.07f, 8, 1, HERO_M_JOINT, 1 },
    /*  6 head     */ { 5, { 0, 1.63f, 0 }, { 0, 1.92f, 0 }, 0.155f, 0, 12, 7, HERO_M_HELMET, 1 },   /* the helmet: a sphere */
    /*  7 tank     */ { 4, { 0, 1.16f, -0.21f }, { 0, 1.56f, -0.21f }, 0.11f, 0.11f, 10, 3, HERO_M_TANK, 1.4f },
    /*  8 L clav   */ { 4, { 0.05f, 1.50f, 0 }, { 0.21f, 1.50f, 0 }, 0.07f, 0.08f, 8, 1, HERO_M_PLATE, 1 },
    /*  9 L upper  */ { 8, { 0.23f, 1.48f, 0 }, { 0.25f, 1.20f, 0 }, 0.075f, 0.06f, 8, 3, HERO_M_CLOTH, 1 },
    /* 10 L fore   */ { 9, { 0.25f, 1.20f, 0 }, { 0.26f, 0.95f, 0.02f }, 0.065f, 0.05f, 8, 3, HERO_M_PLATE, 1 },
    /* 11 L hand   */ { 10, { 0.26f, 0.95f, 0.02f }, { 0.26f, 0.86f, 0.03f }, 0.045f, 0.04f, 6, 2, HERO_M_JOINT, 1.3f },
    /* 12 L fing1  */ { 11, { 0.26f, 0.86f, 0.05f }, { 0.26f, 0.80f, 0.06f }, 0.015f, 0.012f, 4, 1, HERO_M_JOINT, 1 },
    /* 13 L fing2  */ { 11, { 0.26f, 0.86f, 0.02f }, { 0.26f, 0.80f, 0.02f }, 0.015f, 0.012f, 4, 1, HERO_M_JOINT, 1 },
    /* 14 L thumb  */ { 11, { 0.24f, 0.90f, 0.05f }, { 0.22f, 0.86f, 0.08f }, 0.015f, 0.012f, 4, 1, HERO_M_JOINT, 1 },
    /* 15 R clav   */ { 4, { -0.05f, 1.50f, 0 }, { -0.21f, 1.50f, 0 }, 0.07f, 0.08f, 8, 1, HERO_M_PLATE, 1 },
    /* 16 R upper  */ { 15, { -0.23f, 1.48f, 0 }, { -0.25f, 1.20f, 0 }, 0.075f, 0.06f, 8, 3, HERO_M_CLOTH, 1 },
    /* 17 R fore   */ { 16, { -0.25f, 1.20f, 0 }, { -0.26f, 0.95f, 0.02f }, 0.065f, 0.05f, 8, 3, HERO_M_PLATE, 1 },
    /* 18 R hand   */ { 17, { -0.26f, 0.95f, 0.02f }, { -0.26f, 0.86f, 0.03f }, 0.045f, 0.04f, 6, 2, HERO_M_JOINT, 1.3f },
    /* 19 R fing1  */ { 18, { -0.26f, 0.86f, 0.05f }, { -0.26f, 0.80f, 0.06f }, 0.015f, 0.012f, 4, 1, HERO_M_JOINT, 1 },
    /* 20 R fing2  */ { 18, { -0.26f, 0.86f, 0.02f }, { -0.26f, 0.80f, 0.02f }, 0.015f, 0.012f, 4, 1, HERO_M_JOINT, 1 },
    /* 21 R thumb  */ { 18, { -0.24f, 0.90f, 0.05f }, { -0.22f, 0.86f, 0.08f }, 0.015f, 0.012f, 4, 1, HERO_M_JOINT, 1 },
    /* 22 L thigh  */ { 1, { 0.10f, 0.95f, 0 }, { 0.11f, 0.52f, 0 }, 0.095f, 0.07f, 10, 3, HERO_M_CLOTH, 1 },
    /* 23 L calf   */ { 22, { 0.11f, 0.52f, 0 }, { 0.11f, 0.11f, -0.01f }, 0.075f, 0.06f, 10, 3, HERO_M_PLATE, 1 },
    /* 24 L foot   */ { 23, { 0.11f, 0.11f, -0.01f }, { 0.11f, 0.05f, 0.12f }, 0.065f, 0.055f, 8, 2, HERO_M_BOOT, 1.2f },
    /* 25 L toe    */ { 24, { 0.11f, 0.05f, 0.12f }, { 0.11f, 0.04f, 0.21f }, 0.05f, 0.04f, 6, 1, HERO_M_BOOT, 1.3f },
    /* 26 R thigh  */ { 1, { -0.10f, 0.95f, 0 }, { -0.11f, 0.52f, 0 }, 0.095f, 0.07f, 10, 3, HERO_M_CLOTH, 1 },
    /* 27 R calf   */ { 26, { -0.11f, 0.52f, 0 }, { -0.11f, 0.11f, -0.01f }, 0.075f, 0.06f, 10, 3, HERO_M_PLATE, 1 },
    /* 28 R foot   */ { 27, { -0.11f, 0.11f, -0.01f }, { -0.11f, 0.05f, 0.12f }, 0.065f, 0.055f, 8, 2, HERO_M_BOOT, 1.2f },
    /* 29 R toe    */ { 28, { -0.11f, 0.05f, 0.12f }, { -0.11f, 0.04f, 0.21f }, 0.05f, 0.04f, 6, 1, HERO_M_BOOT, 1.3f },
    /* 30 tool     */ { 18, { -0.26f, 0.90f, -0.06f }, { -0.26f, 0.90f, 0.50f }, 0.05f, 0.04f, 4, 3, HERO_M_TOOL, 1.4f },
    /* 31 tool mag */ { 30, { -0.26f, 0.87f, 0.22f }, { -0.26f, 0.72f, 0.25f }, 0.03f, 0.028f, 4, 1, HERO_M_TOOL, 1.6f },
    /* 32 aerial   */ { 6, { 0.10f, 1.86f, -0.04f }, { 0.13f, 2.02f, -0.08f }, 0.012f, 0.006f, 4, 1, HERO_M_JOINT, 1 },
};

static inline unsigned hero_vtx_(hero_t *h, const float p[3], const float n[3], float u, float v, unsigned b0, unsigned b1, unsigned w0) {
    const unsigned i = h->vn;

    if (i >= HERO_V_MAX) return 0;
    memcpy(h->p[i], p, sizeof h->p[i]); memcpy(h->n[i], n, sizeof h->n[i]);
    h->uv[i][0] = u; h->uv[i][1] = v;
    h->b0[i] = (uint8_t)b0; h->b1[i] = (uint8_t)b1; h->w0[i] = (uint8_t)w0;
    h->vn++;
    return i;
}

static inline void hero_strip_(hero_t *h, const unsigned *v, unsigned n) {
    unsigned i;

    if (h->strips >= HERO_STRIP_MAX || h->idx_n + n > HERO_IDX_MAX || n < 3u) return;
    for (i = 0; i < n; i++) h->idx[h->idx_n + i] = (uint16_t)v[i];
    h->idx_n += n; h->slen[h->strips++] = (uint16_t)n; h->tris += n - 2u;
}

/* The atlas: 4 x 2 cells of the base texture, one a material; (u, v) 0..1 inside a cell to the texture's. */
static inline float hero_atlas_u_(unsigned mat, float u) { return ((float)(mat & 3u) + 0.03f + 0.94f * u) * 0.25f; }
static inline float hero_atlas_v_(unsigned mat, float v) { return ((float)(mat >> 2) + 0.03f + 0.94f * v) * 0.5f; }

/* A frame across a tube's axis: x across the body where it can be, y the other way. */
static inline void hero_axes_(const float d[3], float x[3], float y[3]) {
    const float up[3] = { 0.0f, 1.0f, 0.0f }, fw[3] = { 0.0f, 0.0f, 1.0f };

    sh4der_cross_(sh4der_abs_(d[1]) > 0.7f ? fw : up, d, x);
    sh4der_norm3_(x);
    sh4der_cross_(d, x, y);
    sh4der_norm3_(y);
}

static inline void hero_tube_(hero_t *h, unsigned b) {
    const hero_bone_def_t *d = &hero_def[b];
    const unsigned S = d->sides, R = d->rings, blend = d->parent != HERO_NOBONE && hero_def[d->parent].sides ? d->parent : b;
    float ax[3] = { d->tail[0] - d->head[0], d->tail[1] - d->head[1], d->tail[2] - d->head[2] }, x[3], y[3];
    unsigned ring[2][17], r, s, row = 0, cap[17];

    sh4der_norm3_(ax);
    hero_axes_(ax, x, y);
    for (r = 0; r <= R; r++) {
        const float t = (float)r / (float)R, rad = d->r0 + (d->r1 - d->r0) * t;

        for (s = 0; s <= S; s++) {
            const float a = 6.2831853f * (float)s / (float)S;
            float sa, ca, n[3], p[3];
            unsigned k;

            sh4der_sincos_(a, &sa, &ca);
            for (k = 0; k < 3u; k++) {
                n[k] = ca * x[k] * d->wide + sa * y[k];
                p[k] = d->head[k] + (d->tail[k] - d->head[k]) * t + rad * (ca * x[k] * d->wide + sa * y[k]);
            }
            sh4der_norm3_(n);
            ring[row][s] = hero_vtx_(h, p, n, hero_atlas_u_(d->mat, (float)s / (float)S), hero_atlas_v_(d->mat, t), b, r == 0 ? blend : b, r == 0 && blend != b ? 128u : 255u);
        }
        if (r) {
            unsigned st[34];

            for (s = 0; s <= S; s++) { st[2u * s] = ring[row ^ 1u][s]; st[2u * s + 1u] = ring[row][s]; }
            hero_strip_(h, st, 2u * (S + 1u));
        }
        if (r == 0 || r == R) {                 /* a cap: the end ring again with the axis normal, a triangle a side */
            const float sign = r ? 1.0f : -1.0f, cn[3] = { ax[0] * sign, ax[1] * sign, ax[2] * sign };
            const float *e = r ? d->tail : d->head;
            const unsigned c = hero_vtx_(h, e, cn, hero_atlas_u_(d->mat, 0.5f), hero_atlas_v_(d->mat, r ? 1.0f : 0.0f), b, b, 255u);

            for (s = 0; s < S; s++) cap[s] = hero_vtx_(h, h->p[ring[row][s]], cn, h->uv[ring[row][s]][0], h->uv[ring[row][s]][1], b, b, 255u);
            for (s = 0; s < S; s++) {
                unsigned tri[3];

                tri[0] = c; tri[1] = cap[r ? s : (s + 1u) % S]; tri[2] = cap[r ? (s + 1u) % S : s];
                hero_strip_(h, tri, 3u);
            }
        }
        row ^= 1u;
    }
}

/* The helmet: a sphere of `rings` bands about the head bone's tube centre; the visor: a band of 13 x 4 cells on its
   front, a little proud, in its own strips at the end. */
static inline void hero_helmet_(hero_t *h, unsigned b, int visor) {
    const hero_bone_def_t *d = &hero_def[b];
    const float c[3] = { d->head[0], d->head[1] + 0.13f, d->head[2] + 0.01f };
    const unsigned S = visor ? 13u : d->sides, R = visor ? 4u : d->rings;
    const float lat0 = visor ? -0.30f : -1.5707963f, lat1 = visor ? 0.12f : 1.5707963f, lon0 = visor ? -0.95f : 0.0f;
    const float lon1 = visor ? 0.95f : 6.2831853f, rad = visor ? d->r0 * 1.03f : d->r0;
    unsigned ring[2][17], r, s, row = 0;

    for (r = 0; r <= R; r++) {
        float sl, cl;

        sh4der_sincos_(lat0 + (lat1 - lat0) * (float)r / (float)R, &sl, &cl);
        for (s = 0; s <= S; s++) {
            float so, co, n[3], p[3];
            unsigned k;

            sh4der_sincos_(lon0 + (lon1 - lon0) * (float)s / (float)S, &so, &co);
            n[0] = cl * so; n[1] = sl; n[2] = cl * co;
            for (k = 0; k < 3u; k++) p[k] = c[k] + rad * n[k] * (k == 1u ? 1.08f : 1.0f);
            ring[row][s] = hero_vtx_(h, p, n, hero_atlas_u_(visor ? HERO_M_VISOR : HERO_M_HELMET, (float)s / (float)S),
                                     hero_atlas_v_(visor ? HERO_M_VISOR : HERO_M_HELMET, (float)r / (float)R), b, b, 255u);
        }
        if (r) {
            unsigned st[34];

            for (s = 0; s <= S; s++) { st[2u * s] = ring[row][s]; st[2u * s + 1u] = ring[row ^ 1u][s]; }
            hero_strip_(h, st, 2u * (S + 1u));
        }
        row ^= 1u;
    }
}

/* Vertices in order of (b0, b1), the strips' indices moved with them: hero_skin loads a matrix once a run. */
static inline void hero_sort_(hero_t *h) {
    static uint16_t order[HERO_V_MAX], where[HERO_V_MAX];
    static float p[HERO_V_MAX][3], n[HERO_V_MAX][3], uv[HERO_V_MAX][2];
    static uint8_t b0[HERO_V_MAX], b1[HERO_V_MAX], w0[HERO_V_MAX];
    unsigned i, k = 0, a, c;

    for (a = 0; a < HERO_BONES; a++)
        for (c = 0; c < HERO_BONES; c++)
            for (i = 0; i < h->vn; i++)
                if (h->b0[i] == a && h->b1[i] == c) order[k++] = (uint16_t)i;
    for (i = 0; i < h->vn; i++) {
        const unsigned o = order[i];

        where[o] = (uint16_t)i;
        memcpy(p[i], h->p[o], sizeof p[i]); memcpy(n[i], h->n[o], sizeof n[i]); memcpy(uv[i], h->uv[o], sizeof uv[i]);
        b0[i] = h->b0[o]; b1[i] = h->b1[o]; w0[i] = h->w0[o];
    }
    memcpy(h->p, p, h->vn * sizeof p[0]); memcpy(h->n, n, h->vn * sizeof n[0]); memcpy(h->uv, uv, h->vn * sizeof uv[0]);
    memcpy(h->b0, b0, h->vn); memcpy(h->b1, b1, h->vn); memcpy(h->w0, w0, h->vn);
    for (i = 0; i < h->idx_n; i++) h->idx[i] = where[h->idx[i]];
}

/* ---- the clips ------------------------------------------------------------------------------------------------- */

/* Each bone's local turn (x, y, z radians) and the root's lift at phase ph (0 .. 2 pi), for a clip. */
static inline void hero_clip_pose_(unsigned clip, float ph, float rot[HERO_BONES][3], float *lift) {
    float s1, c1, s2, c2, sh, ch;

    memset(rot, 0, HERO_BONES * sizeof rot[0]);
    sh4der_sincos_(ph, &s1, &c1);
    sh4der_sincos_(2.0f * ph, &s2, &c2);
    sh4der_sincos_(0.5f * ph, &sh, &ch);
    if (clip == HERO_IDLE) {                                   /* breathing, a look about, the tool held low */
        *lift = 0.006f * s1;
        rot[3][0] = 0.025f * s1; rot[4][0] = 0.02f * s1;
        rot[6][1] = 0.22f * s1; rot[6][0] = 0.05f * c1;
        rot[8][2] = 0.03f * s1; rot[15][2] = -0.03f * s1;
        rot[9][2] = -0.10f; rot[10][0] = -0.15f + 0.03f * s1;
        rot[16][0] = -0.35f; rot[16][2] = 0.12f; rot[17][0] = -0.95f; rot[18][0] = 0.2f;
        rot[22][2] = -0.03f; rot[26][2] = 0.03f; rot[23][0] = 0.04f; rot[27][0] = 0.04f;
        rot[30][1] = 0.1f;
        (void)s2; (void)c2; (void)sh; (void)ch;
    } else {                                                   /* a run in place, the tool up at the shoulder */
        const float sw = 0.75f;

        *lift = 0.045f * (s2 < 0.0f ? -s2 : s2) - 0.03f;
        rot[1][1] = 0.12f * s1; rot[2][0] = 0.18f; rot[3][1] = -0.10f * s1; rot[4][1] = -0.08f * s1;
        rot[6][0] = -0.12f; rot[6][1] = 0.08f * s1;
        rot[22][0] = -sw * s1; rot[26][0] = sw * s1;
        rot[23][0] = 0.25f + 0.9f * (c1 > 0.0f ? c1 : 0.0f); rot[27][0] = 0.25f + 0.9f * (c1 < 0.0f ? -c1 : 0.0f);
        rot[24][0] = -0.2f * s1; rot[28][0] = 0.2f * s1;
        rot[16][0] = -1.35f; rot[16][2] = 0.25f; rot[17][0] = -0.25f; rot[18][0] = 0.1f;
        rot[9][0] = -1.05f; rot[9][2] = -0.55f; rot[10][0] = -0.85f; rot[10][1] = 0.4f;
        rot[30][1] = 0.05f * s1;
    }
}

static inline void hero_bake_clip_(hero_t *h, unsigned clip, unsigned frames) {
    static float world[HERO_BONES][12];
    float rot[HERO_BONES][3], lift;
    unsigned f, b;

    h->frames[clip] = frames;
    for (f = 0; f < frames; f++) {
        hero_clip_pose_(clip, 6.2831853f * (float)f / (float)frames, rot, &lift);
        for (b = 0; b < HERO_BONES; b++) {
            const unsigned p = h->parent[b];
            float local[12], back[12] = { 1, 0, 0, -h->head[b][0], 0, 1, 0, -h->head[b][1], 0, 0, 1, -h->head[b][2] };

            if (p == HERO_NOBONE) hero_m34_rot_(world[b], rot[b][0], rot[b][1], rot[b][2], h->head[b][0], h->head[b][1] + lift, h->head[b][2]);
            else {
                hero_m34_rot_(local, rot[b][0], rot[b][1], rot[b][2], h->head[b][0] - h->head[p][0], h->head[b][1] - h->head[p][1], h->head[b][2] - h->head[p][2]);
                hero_m34_mul_(world[b], world[p], local);
            }
            hero_m34_mul_(h->clip[clip][f][b], world[b], back);
        }
    }
}

/* ---- the textures: made here, 565 --------------------------------------------------------------------------------- */

static inline uint32_t hero_hash_(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
static inline uint16_t hero_565_(float r, float g, float b) {
    return (uint16_t)(((unsigned)(sh4der_sat_(r) * 31.0f + 0.5f) << 11) | ((unsigned)(sh4der_sat_(g) * 63.0f + 0.5f) << 5) | (unsigned)(sh4der_sat_(b) * 31.0f + 0.5f));
}

static inline void hero_textures_(hero_t *h) {
    static const float mc[8][3] = {
        { 0.86f, 0.85f, 0.80f }, { 0.74f, 0.34f, 0.14f }, { 0.20f, 0.20f, 0.22f }, { 0.88f, 0.86f, 0.80f },
        { 0.56f, 0.60f, 0.62f }, { 0.30f, 0.22f, 0.16f }, { 0.24f, 0.25f, 0.27f }, { 0.04f, 0.30f, 0.34f } };
    unsigned x, y;

    h->tex_w[HERO_TEX_BASE] = h->tex_h[HERO_TEX_BASE] = 128u;
    h->tex_w[HERO_TEX_DETAIL] = h->tex_h[HERO_TEX_DETAIL] = 64u;
    h->tex_w[HERO_TEX_ENV] = h->tex_h[HERO_TEX_ENV] = 64u;
    h->tex_fmt[0] = h->tex_fmt[1] = h->tex_fmt[2] = HERO_FMT_565;
    for (y = 0; y < 128u; y++)
        for (x = 0; x < 128u; x++) {
            const unsigned m = (y / 64u) * 4u + x / 32u, cx = x % 32u, cy = y % 64u;
            float k = 1.0f - 0.06f * (float)(hero_hash_(x * 131u + y) & 15u) / 15.0f;
            const float *c = mc[m];

            if (m == HERO_M_PLATE && (cy % 16u == 0u || cx == 0u)) k *= 0.55f;                       /* panel seams */
            if (m == HERO_M_PLATE && cy % 16u == 3u && (cx % 8u) == 4u) k *= 1.35f;                   /* rivets */
            if (m == HERO_M_CLOTH && (cx + cy) % 6u == 0u) k *= 0.88f;                                /* the weave */
            if (m == HERO_M_JOINT && cy % 4u < 2u) k *= 0.75f;                                       /* ribs */
            if (m == HERO_M_HELMET && cy > 26u && cy < 34u) { c = mc[HERO_M_PLATE]; }                 /* a band round the helmet */
            if (m == HERO_M_TANK && cy % 20u < 2u) k *= 0.6f;
            if (m == HERO_M_TOOL && cx % 16u == 0u) k *= 1.5f;
            h->tex[HERO_TEX_BASE][y * 128u + x] = hero_565_(c[0] * k, c[1] * k, c[2] * k);
        }
    for (y = 0; y < 64u; y++)                                  /* detail: grey near 0.5 (neutral in a 2 x multiply), scuffs */
        for (x = 0; x < 64u; x++) {
            const uint32_t r = hero_hash_(x * 7919u + y * 104729u);
            float g = 0.5f + 0.05f * ((float)(r & 255u) / 255.0f - 0.5f);

            if ((r >> 8) % 23u == 0u) g -= 0.12f;
            if (((x + 2u * y) % 29u) == 0u && ((r >> 16) & 3u) == 0u) g += 0.10f;
            h->tex[HERO_TEX_DETAIL][y * 64u + x] = hero_565_(g, g, g);
        }
    for (y = 0; y < 64u; y++)                                  /* env: a sphere map of a pale sky, a dark floor, a window */
        for (x = 0; x < 64u; x++) {
            const float u = ((float)x + 0.5f) / 32.0f - 1.0f, v = ((float)y + 0.5f) / 32.0f - 1.0f, r2 = u * u + v * v;
            float l = v < 0.0f ? 0.55f - 0.35f * v : 0.25f - 0.2f * v;

            if (u > 0.15f && u < 0.55f && v > -0.6f && v < -0.2f) l = 1.0f;
            if (r2 > 1.0f) l = 0.0f;
            h->tex[HERO_TEX_ENV][y * 64u + x] = hero_565_(l * 0.80f, l * 0.95f, l);
        }
}

/* The original hero: mesh, clips, textures. Returns 0 if a table was too small (a build error, the test catches it). */
static inline int hero_build(hero_t *h) {
    unsigned b;

    memset(h, 0, sizeof *h);
    for (b = 0; b < HERO_BONES; b++) {
        h->parent[b] = hero_def[b].parent;
        memcpy(h->head[b], hero_def[b].head, sizeof h->head[b]);
    }
    for (b = 0; b < HERO_BONES; b++) {
        if (!hero_def[b].sides) continue;
        if (b == 6u) hero_helmet_(h, b, 0);
        else hero_tube_(h, b);
    }
    h->visor_strip0 = h->strips;
    hero_helmet_(h, 6u, 1);
    hero_sort_(h);
    hero_bake_clip_(h, HERO_IDLE, 60u);
    hero_bake_clip_(h, HERO_RUN_AIM, 24u);
    hero_textures_(h);
    return h->vn < HERO_V_MAX && h->idx_n < HERO_IDX_MAX && h->strips < HERO_STRIP_MAX;
}

/* ---- a frame --------------------------------------------------------------------------------------------------- */

/* The clip at t seconds (it loops): each bone's skin matrix, a lerp of the two stored frames about t. */
static inline void hero_pose(const hero_t *h, unsigned clip, float t, float out[HERO_BONES][12]) {
    const unsigned n = h->frames[clip];
    const float ft = t * (float)HERO_FPS, fl = ft - (float)(unsigned)ft;
    const unsigned f0 = (unsigned)ft % n, f1 = (f0 + 1u) % n;
    unsigned b, k;

    for (b = 0; b < HERO_BONES; b++)
        for (k = 0; k < 12u; k++) out[b][k] = sh4der_lerp_(h->clip[clip][f0][b][k], h->clip[clip][f1][b][k], fl);
}

/* Each bone's palette matrix: vp (4 x 4 column-major, world -> screen-ready: X/W, Y/W pixels, Z = 1) times its skin. */
static inline void hero_palette(const float vp[16], const float pose[HERO_BONES][12], float pal[HERO_BONES][16]) {
    unsigned b;

    for (b = 0; b < HERO_BONES; b++) {
        float m[16] __attribute__((aligned(8)));

        hero_m4_of34_(m, pose[b]);
#ifndef SH4DER_CORE_NO_SH4ZAM
        shz_xmtrx_load_4x4((const shz_mat4x4_t *)vp);   /* inline: load_apply_store is SH4ZAM's library, not linked here */
        shz_xmtrx_apply_4x4((const shz_mat4x4_t *)m);
        shz_xmtrx_store_4x4((shz_mat4x4_t *)pal[b]);
#else
        hero_m4_mul_(pal[b], vp, m);
#endif
    }
}

/* A world direction in each bone's rest space (the transpose of the pose's turn): light, camera right, camera up. */
static inline void hero_bone_dirs(const float pose[HERO_BONES][12], const float d[3], float out[HERO_BONES][3]) {
    unsigned b;

    for (b = 0; b < HERO_BONES; b++) {
        const float *m = pose[b];

        out[b][0] = m[0] * d[0] + m[4] * d[1] + m[8] * d[2];
        out[b][1] = m[1] * d[0] + m[5] * d[1] + m[9] * d[2];
        out[b][2] = m[2] * d[0] + m[6] * d[1] + m[10] * d[2];
    }
}

static inline void hero_xf_(const float m[16], const float p[3], float o[4]) {
    o[0] = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
    o[1] = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
    o[2] = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
    o[3] = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];
}

/* Every vertex through its palette: out x, y in pixels, z = 1 / W (the PVR's depth). Vertices with W under near are
   marked by z = 0. `blend` 0 takes b0 alone (the shadow's cheap pass). Returns the matrix loads. */
static inline unsigned hero_skin(const hero_t *h, const float pal[HERO_BONES][16], int blend, float near, float out[][3]) {
    static float tmp[HERO_V_MAX][4];
    unsigned i = 0, loads = 0;

    while (i < h->vn) {
        const unsigned a = h->b0[i], c = blend ? h->b1[i] : a;
        unsigned j = i, k;

        while (j < h->vn && h->b0[j] == a && (blend ? h->b1[j] : a) == c) j++;
#ifndef SH4DER_CORE_NO_SH4ZAM
        shz_xmtrx_load_4x4((const shz_mat4x4_t *)pal[a]); loads++;
        for (k = i; k < j; k++) {
            const shz_vec4_t v = shz_xmtrx_transform_vec4(shz_vec4_init(h->p[k][0], h->p[k][1], h->p[k][2], 1.0f));

            tmp[k][0] = v.x; tmp[k][1] = v.y; tmp[k][2] = v.z; tmp[k][3] = v.w;
        }
        if (c != a) {
            shz_xmtrx_load_4x4((const shz_mat4x4_t *)pal[c]); loads++;
            for (k = i; k < j; k++) {
                const shz_vec4_t v = shz_xmtrx_transform_vec4(shz_vec4_init(h->p[k][0], h->p[k][1], h->p[k][2], 1.0f));
                const float w = (float)h->w0[k] * (1.0f / 255.0f);

                tmp[k][0] = sh4der_lerp_(v.x, tmp[k][0], w); tmp[k][1] = sh4der_lerp_(v.y, tmp[k][1], w);
                tmp[k][2] = sh4der_lerp_(v.z, tmp[k][2], w); tmp[k][3] = sh4der_lerp_(v.w, tmp[k][3], w);
            }
        }
#else
        loads++;
        for (k = i; k < j; k++) hero_xf_(pal[a], h->p[k], tmp[k]);
        if (c != a) {
            loads++;
            for (k = i; k < j; k++) {
                float v[4];
                const float w = (float)h->w0[k] * (1.0f / 255.0f);
                unsigned e;

                hero_xf_(pal[c], h->p[k], v);
                for (e = 0; e < 4u; e++) tmp[k][e] = sh4der_lerp_(v[e], tmp[k][e], w);
            }
        }
#endif
        for (k = i; k < j; k++) {
            if (tmp[k][3] < near) { out[k][0] = out[k][1] = out[k][2] = 0.0f; continue; }
            {
                const float iw = sh4der_recip_pos_(tmp[k][3]);

                out[k][0] = tmp[k][0] * iw; out[k][1] = tmp[k][1] * iw; out[k][2] = iw;
            }
        }
        i = j;
    }
    return loads;
}

/* A vertex's light, 0 .. 255: ambient + sun x the rest normal against the light in its bone's space. */
static inline void hero_light(const hero_t *h, const float lb[HERO_BONES][3], float amb, float sun, uint8_t *out) {
    unsigned i;

    for (i = 0; i < h->vn; i++) {
        const float d = sh4der_dot3_(h->n[i], lb[h->b0[i]]);

        out[i] = (uint8_t)(sh4der_sat_(amb + sun * (d > 0.0f ? d : 0.0f)) * 255.0f);
    }
}

/* The visor's sphere-map lookup for the vertices of strips [s0, s1): the rest normal against the camera's right and up
   in the bone's space. */
static inline void hero_sphere_uv(const hero_t *h, const float rb[HERO_BONES][3], const float ub[HERO_BONES][3], unsigned s0, unsigned s1, float out[][2]) {
    unsigned s, i, k = 0;

    for (s = 0; s < s0; s++) k += h->slen[s];
    for (s = s0; s < s1; s++)
        for (i = 0; i < h->slen[s]; i++, k++) {
            const unsigned v = h->idx[k];

            out[v][0] = 0.5f + 0.48f * sh4der_dot3_(h->n[v], rb[h->b0[v]]);
            out[v][1] = 0.5f - 0.48f * sh4der_dot3_(h->n[v], ub[h->b0[v]]);
        }
}

#endif
