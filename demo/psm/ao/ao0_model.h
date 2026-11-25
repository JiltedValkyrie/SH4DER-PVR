/* SH4DER-PVR: the ao0 image's scenes and its model of the target (demo/psm/ao)

   demo/psm/ao/ao0_model.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Shared by the Dreamcast program (ao0_spike.c) and the host test (test/ao0_model_test.c):

     the scenes   boxes standing on a flat ground at y 0. SMALL: a house, a lean-to that stands half inside it, a post and a mover (48 triangles), for the
                  union test and the live view. LOAD: 97 boxes (1,164 triangles, the town's caster count is about
                  1,162), for the cost rows.
     the map      the ground's x, z in -AO0_SPAN .. AO0_SPAN as the target's -1 .. 1, w 1: a uniform map.
     the model    which directions' slid triangles cover each texel's centre (ao0_cover): `bits` one bit a direction,
                  `count` the triangles over the texel in all directions. With the depth union a texel is darkened once
                  for every bit; with plain blending once for every triangle. `edge` marks the texels whose 3 x 3
                  neighbours differ in bits: there the PVR's fill rule decides, and the compare leaves them out.

   A model, not a measurement. Plain C on both sides: it is not in the frame. */

#ifndef AO0_MODEL_H
#define AO0_MODEL_H

#include <stdint.h>
#include <string.h>
#include "../ao_layer.h"

#define AO0_SIZE     256u
#define AO0_SPAN     12.0f
#define AO0_BOX_MAX  100u
#define AO0_V_MAX    (AO0_BOX_MAX * 8u)
#define AO0_T_MAX    (AO0_BOX_MAX * 12u)
#define AO0_LOAD_BOXES 97u

typedef struct ao0_scene {
    float v[AO0_V_MAX][3];
    uint16_t t[AO0_T_MAX][3];
    unsigned vn, tn;
} ao0_scene_t;

static const float ao0_map[16] = { 1.0f / AO0_SPAN, 0, 0, 0,   0, 0, 0, 0,   0, 1.0f / AO0_SPAN, 0, 0,   0, 0, 0, 1 };

/* the sun of the map pass: its slide's slope (an elevation near 35 degrees) and the grey of its shadow */
static const float ao0_sun_slope[2] = { -1.20f, 0.77f };
#define AO0_SUN_GREY 0xff6b6b6bu

/* a box's 12 triangles over its 8 corners: bit 0 x, bit 1 z, bit 2 y */
static const uint8_t ao0_box_t[12][3] = { {0,1,2},{1,3,2},{4,6,5},{5,6,7},{0,4,1},{1,4,5},{2,3,6},{3,7,6},{0,2,4},{2,6,4},{1,5,3},{3,5,7} };

/* A box standing on the ground: centre (cx, cz), half sizes hw and hd, height h. */
static inline void ao0_box(ao0_scene_t *s, float cx, float cz, float hw, float hd, float h) {
    unsigned i;

    if (s->vn + 8u > AO0_V_MAX) return;
    for (i = 0; i < 8u; i++) {
        s->v[s->vn + i][0] = cx + ((i & 1u) ? hw : -hw);
        s->v[s->vn + i][1] = (i & 4u) ? h : 0.0f;
        s->v[s->vn + i][2] = cz + ((i & 2u) ? hd : -hd);
    }
    for (i = 0; i < 12u; i++) {
        s->t[s->tn + i][0] = (uint16_t)(s->vn + ao0_box_t[i][0]);
        s->t[s->tn + i][1] = (uint16_t)(s->vn + ao0_box_t[i][1]);
        s->t[s->tn + i][2] = (uint16_t)(s->vn + ao0_box_t[i][2]);
    }
    s->vn += 8u; s->tn += 12u;
}

/* SMALL: the house, the lean-to half inside it (two casters whose shadows overlap: what tells the union from plain
   blending), the post, the mover at (mx, mz). */
static inline void ao0_small(ao0_scene_t *s, float mx, float mz) {
    s->vn = s->tn = 0;
    ao0_box(s, -2.0f, -1.0f, 2.0f, 1.5f, 3.0f);
    ao0_box(s, 0.5f, -0.5f, 1.2f, 1.0f, 2.0f);
    ao0_box(s, 4.0f, 3.0f, 0.12f, 0.12f, 3.0f);
    ao0_box(s, mx, mz, 0.25f, 0.25f, 1.7f);
}

/* LOAD: AO0_LOAD_BOXES boxes on a 10 x 10 grid 2.1 m apart, sizes from the box's number. */
static inline void ao0_load(ao0_scene_t *s) {
    unsigned i;

    s->vn = s->tn = 0;
    for (i = 0; i < AO0_LOAD_BOXES; i++) {
        const uint32_t h = (i + 1u) * 2654435761u;
        const float hw = 0.35f + (float)((h >> 8) & 255u) * (0.55f / 255.0f), hd = 0.35f + (float)((h >> 16) & 255u) * (0.55f / 255.0f);

        ao0_box(s, -9.45f + 2.1f * (float)(i % 10u), -9.45f + 2.1f * (float)(i / 10u), hw, hd, 1.0f + (float)(h >> 24) * (3.0f / 255.0f));
    }
}

/* (p, 1) x m, the model's own */
static inline void ao0_xform(const float m[16], const float p[3], float h[4]) {
    unsigned c;

    for (c = 0; c < 4u; c++) h[c] = p[0] * m[c] + p[1] * m[4u + c] + p[2] * m[8u + c] + m[12u + c];
}

/* Scene vertex i of direction `slope` in target pixels. */
static inline int ao0_place(const ao0_scene_t *s, const float m[16], unsigned i, float xy[2]) {
    float h[4];

    ao0_xform(m, s->v[i], h);
    return ao_px(h, 1.0f, 0.5f * (float)AO0_SIZE, xy);
}

static inline float ao0_edge_(const float a[2], const float b[2], float px, float py) {
    return (b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0]);
}

/* The model of the target for k directions: bits and count, AO0_SIZE x AO0_SIZE each. Returns the triangles drawn
   (those ao_tri_ok keeps under `cull`), which is what the program sends. */
static inline unsigned ao0_cover(const ao0_scene_t *s, unsigned k, int cull, uint8_t *bits, uint8_t *count) {
    static float xy[AO0_V_MAX][2];
    unsigned d, i, t, drawn = 0;

    memset(bits, 0, AO0_SIZE * AO0_SIZE);
    memset(count, 0, AO0_SIZE * AO0_SIZE);
    for (d = 0; d < k; d++) {
        float slope[2], m[16];

        ao_slope(k, d, slope);
        ao_slide_map(ao0_map, slope, 0.0f, m);
        for (i = 0; i < s->vn; i++) ao0_place(s, m, i, xy[i]);
        for (t = 0; t < s->tn; t++) {
            const float *a = xy[s->t[t][0]], *b = xy[s->t[t][1]], *c = xy[s->t[t][2]];
            float x0 = a[0], x1 = a[0], y0 = a[1], y1 = a[1];
            int ix0, ix1, iy0, iy1, x, y;

            if (!ao_tri_ok(a, b, c, (float)AO0_SIZE, cull)) continue;
            drawn++;
            if (b[0] < x0) x0 = b[0];
            if (c[0] < x0) x0 = c[0];
            if (b[0] > x1) x1 = b[0];
            if (c[0] > x1) x1 = c[0];
            if (b[1] < y0) y0 = b[1];
            if (c[1] < y0) y0 = c[1];
            if (b[1] > y1) y1 = b[1];
            if (c[1] > y1) y1 = c[1];
            ix0 = x0 < 0.0f ? 0 : (int)x0; ix1 = x1 >= (float)AO0_SIZE ? (int)AO0_SIZE - 1 : (int)x1;
            iy0 = y0 < 0.0f ? 0 : (int)y0; iy1 = y1 >= (float)AO0_SIZE ? (int)AO0_SIZE - 1 : (int)y1;
            for (y = iy0; y <= iy1; y++)
                for (x = ix0; x <= ix1; x++) {
                    const float px = (float)x + 0.5f, py = (float)y + 0.5f;
                    const float e0 = ao0_edge_(a, b, px, py), e1 = ao0_edge_(b, c, px, py), e2 = ao0_edge_(c, a, px, py);

                    if (!((e0 >= 0.0f && e1 >= 0.0f && e2 >= 0.0f) || (e0 <= 0.0f && e1 <= 0.0f && e2 <= 0.0f))) continue;
                    bits[(unsigned)y * AO0_SIZE + (unsigned)x] |= (uint8_t)(1u << d);
                    if (count[(unsigned)y * AO0_SIZE + (unsigned)x] < 255u) count[(unsigned)y * AO0_SIZE + (unsigned)x]++;
                }
        }
    }
    return drawn;
}

/* 1 where a texel's 3 x 3 neighbours do not all hold its bits (and on the target's rim). Returns the texels marked. */
static inline unsigned ao0_edges(const uint8_t *bits, uint8_t *edge) {
    unsigned x, y, n = 0;

    for (y = 0; y < AO0_SIZE; y++)
        for (x = 0; x < AO0_SIZE; x++) {
            const unsigned i = y * AO0_SIZE + x;
            int e = x == 0u || y == 0u || x == AO0_SIZE - 1u || y == AO0_SIZE - 1u, dx, dy;

            for (dy = -1; !e && dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++)
                    if (bits[i + (unsigned)(dy * (int)AO0_SIZE + dx)] != bits[i]) e = 1;
            edge[i] = (uint8_t)e;
            n += (unsigned)e;
        }
    return n;
}

static inline unsigned ao0_popcount(unsigned b) { unsigned n = 0; while (b) { n += b & 1u; b >>= 1; } return n; }

/* What the union leaves in an RGB565 white target after n of k directions, if the PVR blends at 8 bits a channel
   with the vertex alpha as sent and keeps the top bits: a guide for reading the page, not a claim about the PVR. */
static inline uint16_t ao0_guess565(unsigned k, unsigned n) {
    const unsigned a = ao_argb(k, 1.0f) >> 24;
    unsigned c = 255u;

    while (n--) c = (c * (255u - a) + 127u) / 255u;
    return (uint16_t)(((c >> 3) << 11) | ((c >> 2) << 5) | (c >> 3));
}

/* The reading of a target against the model. `px` is the target as read back, row `r` of the picture at row
   (flip ? AO0_SIZE - 1 - r : r). The distinct values of the texels that are not edges are ranked from the lightest,
   and so are the blocked counts the model has among those texels; with the union a texel holds the value whose rank
   is its count's rank.
     colours   distinct values among those texels (at most 32 kept)
     levels    distinct blocked counts the model has among them
     bad       texels whose value's rank is not their count's rank
     tested    texels compared */
typedef struct ao0_read {
    unsigned colours, levels, bad, tested;
    uint16_t value[32];
} ao0_read_t;

static inline unsigned ao0_luma565(uint16_t p) { return ((p >> 11) & 31u) * 2u + ((p >> 5) & 63u) + (p & 31u) * 2u; }

static inline void ao0_read(const uint16_t *px, const uint8_t *bits, const uint8_t *edge, int flip, ao0_read_t *r) {
    unsigned x, y, i, j;

    memset(r, 0, sizeof *r);
    for (y = 0; y < AO0_SIZE; y++)
        for (x = 0; x < AO0_SIZE; x++) {
            const uint16_t p = px[(flip ? AO0_SIZE - 1u - y : y) * AO0_SIZE + x];

            if (edge[y * AO0_SIZE + x]) continue;
            for (i = 0; i < r->colours && r->value[i] != p; i++) { }
            if (i == r->colours && r->colours < 32u) r->value[r->colours++] = p;
        }
    for (i = 1; i < r->colours; i++)                              /* lightest first */
        for (j = i; j > 0u && ao0_luma565(r->value[j]) > ao0_luma565(r->value[j - 1u]); j--) {
            const uint16_t t = r->value[j];

            r->value[j] = r->value[j - 1u]; r->value[j - 1u] = t;
        }
    {
        unsigned present[AO_DIRS_MAX + 1u], rank[AO_DIRS_MAX + 1u];

        memset(present, 0, sizeof present);
        for (i = 0; i < AO0_SIZE * AO0_SIZE; i++) if (!edge[i]) present[ao0_popcount(bits[i])] = 1u;
        for (i = 0; i <= AO_DIRS_MAX; i++) { rank[i] = r->levels; r->levels += present[i]; }
        for (y = 0; y < AO0_SIZE; y++)
            for (x = 0; x < AO0_SIZE; x++) {
                const uint16_t p = px[(flip ? AO0_SIZE - 1u - y : y) * AO0_SIZE + x];

                if (edge[y * AO0_SIZE + x]) continue;
                for (i = 0; i < r->colours && r->value[i] != p; i++) { }
                r->tested++;
                if (i != rank[ao0_popcount(bits[y * AO0_SIZE + x])]) r->bad++;
            }
    }
}

#endif /* AO0_MODEL_H */
