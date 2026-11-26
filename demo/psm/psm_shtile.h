/* SH4DER-PVR: shadow tiles: a small PVR-drawn shadow a moving caster, packed in one render target

   demo/psm/psm_shtile.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   A test's helper, not yet the library's. NOTHING HERE HAS RUN ON A CONSOLE. Maths only: no KallistiOS or PVR calls, the
   caller draws.

   THE IDEA. With a sun that stands still, what does not move is baked. What moves (people, hens, cars) gets a tile each:
   a rectangle of one render target that the caster is drawn into flat, slid along the sun onto the ground, and one
   ground quad that multiplies the tile into the frame. No full shadow map, no second ground layer, no compare.

   AN ENTRY (sht_tile_t) is a rectangle in the target, x, y, w, h in pixels, each a multiple of SHT_UNIT (32: the PVR's
   tile, so the user clip and the tile cut land on its borders), and the rectangle of ground it covers: a centre and two
   half-axes in the ground plane. The half-axes are free, so a tile can be a STRIP ALONG THE SUN: a person's shadow is
   long and thin, and 128 x 32 over 4 m x 1 m has the texel of a 128 x 128 square for a quarter of the pixels. A 128 x 128
   car tile is the same entry with square axes.

   THE SIZE follows the caster's kind (sht_kind_t: pixels, the longest ground length, the ground width), so every kind has
   about the same ground texel. Fixed a kind: no nearest-N, no size steps.

   THE FADE. strength = fade x grow. fade is 1 up to SHT_FADE_START metres from the camera, linear to 0 at SHT_FADE_END
   (psm_ramp.h). grow rises 0 .. 1 over SHT_GROW_S seconds from the frame a tile is given, so a tile given late (when
   another was freed) does not pop in. A caster takes a tile only while its fade is above 0 and gives it back only when
   its fade is 0 (or the caster is gone, or the sun is too low): a visible shadow is never taken away.
   The strength goes into the caster's DRAW COLOUR in the pass, lerp(white, shadow, weight): the ground quad multiplies
   the frame by the tile's texels (blend DESTCOLOR x ZERO), and there a vertex colour could only darken, never fade a
   shadow out. It costs nothing in the pass: the triangles are the same.

   THE FEATHER. A caster vertex's weight also falls to 0 over SHT_FEATHER_PX pixels toward the tile's edge (sht_weight),
   so a shadow longer than its tile (a low sun: the length is capped a kind) thins out toward the cut instead of ending
   at a hard edge. The weight is linear and not clamped below, so after the clip (sht_clip) it is 0 or less exactly on
   the edge. Only a caster whose shadow is longer than its tile needs the clip (sht_tile_t.cut).

   USE, a frame:
     1. sht_assign(tiles, n, kinds, casters, nc, eye, sun, dt)
     2. the pass, into the target: a white quad over sht_cut's rectangle, then for each tile with strength > 0 its
        caster's triangles: sht_matrix once, then a vertex (X, Y) = m x (p, 1), weight sht_weight, a triangle through
        sht_clip when tile.cut, colour sht_argb. Flat on one depth; the tile cut (sh4der_tilecut.h) round sht_cut's tiles.
     3. on the screen, in the translucent list after the ground: for each such tile the quad of sht_quad, texture the
        target, blend DESTCOLOR x ZERO, depth test on, depth write off.
   Once: sht_layout packs the entries.

   NOT DONE HERE: two tiles that overlap on the ground multiply twice (a darker patch where two shadows cross); a shadow
   on a wall (the quad is ground only); a caster standing in a baked shadow still casts (scale its strength by the bake
   at its feet). The ground is a plane y = ground_y under each caster (sht_caster_t.base_y above it for a caster on a
   hill: the car demo); the tile's own tilt on a slope stays with the caller.

   Plain C on a host (SH4DER_CORE_NO_SH4ZAM); host test demo/psm/test/psm_shtile_test.c. */

#ifndef PSM_SHTILE_H
#define PSM_SHTILE_H

#include <stdint.h>
#include <string.h>

#include "sh4der_core.h"
#include "psm_ramp.h"

#define SHT_UNIT        32u         /* pixels: a rectangle's x, y, w, h are multiples of it */
#define SHT_FADE_START  40.0f       /* metres from the camera: full strength up to here */
#define SHT_FADE_END    50.0f       /* and none from here on */
#define SHT_GROW_S      0.5f        /* seconds a newly given tile takes to full strength */
#define SHT_FEATHER_PX  8.0f        /* pixels from a tile's edge over which a caster's weight falls to 0 */
#define SHT_SUN_MIN     0.10f       /* the sun's y under which no tile is held (a shadow longer than 10 x the height) */
#define SHT_CUT_MAX     96u         /* tiles of 32 the tile cut can take (sh4der_tilecut.h) */

/* a caster kind's tile: its pixels, the longest ground it may cover along the sun, the ground it covers across */
typedef struct sht_kind {
    uint16_t w, h;
    float along, across;            /* metres */
} sht_kind_t;

/* a caster this frame */
typedef struct sht_caster {
    int id;                         /* its own, never negative; a tile keeps it */
    unsigned kind;
    float foot[3];                  /* where it stands, world */
    float height, radius;           /* its top above its feet; its footprint's half width (both metres) */
    float base_y;                   /* its feet above the ground plane the shadow falls on (0 on flat ground) */
} sht_caster_t;

typedef struct sht_tile {
    uint16_t x, y, w, h;            /* the rectangle in the target, pixels */
    float c[2];                     /* the ground rectangle: its centre (world x, z) */
    float ax[2], ay[2];             /* its half-axes (x, z): ax along the tile's width (away from the sun), ay along its height */
    float base_y;                   /* the caster's feet above the ground plane */
    float fade, grow, strength;     /* strength = fade x grow */
    int caster;                     /* the caster's id; -1 free */
    uint8_t kind, cut;              /* cut: the shadow is longer than the tile, clip its triangles (sht_clip) */
} sht_tile_t;

/* a caster's vertex in the pass, and a corner of the ground quad */
typedef struct sht_pv { float x, y, w; } sht_pv_t;      /* target pixels, the weight */

/* Entry k takes kind kind_of[k]'s size; the rectangles are packed in rows from the target's origin, in the order given
   (put the most used first: sht_cut's rectangle then stays small). Returns 1 when all fit in tw x th. */
static inline int sht_layout(sht_tile_t *t, unsigned n, const uint8_t *kind_of, const sht_kind_t *kinds, unsigned tw, unsigned th) {
    unsigned k, x = 0, y = 0, row = 0;

    for (k = 0; k < n; k++) {
        const sht_kind_t *kd = &kinds[kind_of[k]];

        if (!kd->w || !kd->h || kd->w % SHT_UNIT || kd->h % SHT_UNIT || kd->w > tw) return 0;
        if (x + kd->w > tw) { x = 0; y += row; row = 0; }
        if (y + kd->h > th) return 0;
        memset(&t[k], 0, sizeof t[k]);
        t[k].x = (uint16_t)x; t[k].y = (uint16_t)y; t[k].w = kd->w; t[k].h = kd->h;
        t[k].kind = kind_of[k]; t[k].caster = -1;
        x += kd->w;
        if (kd->h > row) row = kd->h;
    }
    return 1;
}

/* the ground rectangle of a tile for its caster: from the footprint's edge on the sun's side to the shadow's tip, or to the
   kind's longest length (then cut is set). sun: unit, toward the sun, its y above SHT_SUN_MIN. */
static inline void sht_place_(sht_tile_t *t, const sht_kind_t *kd, const sht_caster_t *c, const float sun[3]) {
    const float h2 = sun[0] * sun[0] + sun[2] * sun[2];
    const float hl = sh4der_sqrt_(h2), isy = sh4der_recip_pos_(sun[1]);
    const float ihl = hl > 1e-4f ? sh4der_recip_pos_(hl) : 0.0f;
    const float g[2] = { hl > 1e-4f ? -sun[0] * ihl : 1.0f, hl > 1e-4f ? -sun[2] * ihl : 0.0f };   /* on the ground, away from the sun */
    const float run = (c->height + c->base_y) * hl * isy;          /* the top's shadow, from the point under the feet */
    const float lift = c->base_y * hl * isy;                       /* the feet's own shadow is this far along already */
    float len = 2.0f * c->radius + run - lift, half;

    t->cut = (uint8_t)(len > kd->along);
    if (t->cut) len = kd->along;
    if (len < 4.0f * c->radius) len = 4.0f * c->radius;            /* a high sun: still the footprint and a margin */
    half = 0.5f * len;
    t->c[0] = c->foot[0] + g[0] * (lift + half - c->radius);
    t->c[1] = c->foot[2] + g[1] * (lift + half - c->radius);
    t->ax[0] = g[0] * half; t->ax[1] = g[1] * half;
    t->ay[0] = -g[1] * 0.5f * kd->across; t->ay[1] = g[0] * 0.5f * kd->across;
    t->base_y = c->base_y;
}

/* A frame's tiles. Each held tile follows its caster; a tile whose caster is gone, or whose fade is 0, is freed; a caster
   with a fade above 0 and no tile takes a free one of its kind's size, at grow 0. eye: the camera, world. sun: unit, toward
   the sun. dt: seconds since the last call (the grow-in). Returns the tiles with strength above 0 (those to draw). */
static inline unsigned sht_assign(sht_tile_t *t, unsigned n, const sht_kind_t *kinds, const sht_caster_t *c, unsigned nc,
                                  const float eye[3], const float sun[3], float dt) {
    const int sun_ok = sun[1] > SHT_SUN_MIN;
    psm_ramp_t ramp;
    unsigned k, j, live = 0;

    psm_ramp_make(&ramp, SHT_FADE_START, SHT_FADE_END);
    for (j = 0; j < nc; j++) {
        const float d[3] = { c[j].foot[0] - eye[0], c[j].foot[1] - eye[1], c[j].foot[2] - eye[2] };
        const float fade = sun_ok ? psm_ramp_down(&ramp, sh4der_sqrt_(sh4der_dot3_(d, d))) : 0.0f;
        sht_tile_t *mine = NULL, *spare = NULL;

        for (k = 0; k < n; k++) {
            if (t[k].caster == c[j].id) mine = &t[k];
            else if (t[k].caster < 0 && !spare && t[k].w == kinds[c[j].kind].w && t[k].h == kinds[c[j].kind].h) spare = &t[k];
        }
        if (mine && !(fade > 0.0f)) { mine->caster = -1; mine->strength = mine->fade = mine->grow = 0.0f; continue; }
        if (!mine && fade > 0.0f && spare) { mine = spare; mine->caster = c[j].id; mine->grow = 0.0f; }
        if (!mine) continue;
        mine->grow = sh4der_min1_(mine->grow + dt * (1.0f / SHT_GROW_S));
        mine->fade = fade;
        mine->strength = fade * mine->grow;
        sht_place_(mine, &kinds[c[j].kind], &c[j], sun);
    }
    for (k = 0; k < n; k++) {                   /* a tile whose caster was not in the list this frame */
        if (t[k].caster < 0) continue;
        for (j = 0; j < nc && c[j].id != t[k].caster; j++) { }
        if (j == nc || !sun_ok) { t[k].caster = -1; t[k].strength = t[k].fade = t[k].grow = 0.0f; }
        else if (t[k].strength > 0.0f) live++;
    }
    return live;
}

/* The pass's transform of a tile (two rows of four in m): target pixels (X, Y) = m x (px, py, pz, 1) for a caster's world point p, slid along the
   sun onto the plane y = ground_y. Affine: no divide a vertex. The ground rectangle maps to the tile less a 1 pixel border,
   which stays the white the pass clears to. */
static inline void sht_matrix(const sht_tile_t *t, const float sun[3], float ground_y, float m[8]) {
    const float isy = sh4der_recip_pos_(sun[1]), sx = sun[0] * isy, sz = sun[2] * isy;
    const float *ax[2] = { t->ax, t->ay };
    const float half[2] = { 0.5f * (float)t->w, 0.5f * (float)t->h }, org[2] = { (float)t->x, (float)t->y };
    unsigned r;

    for (r = 0; r < 2u; r++) {
        const float l2 = ax[r][0] * ax[r][0] + ax[r][1] * ax[r][1];
        const float k = l2 > 1e-12f ? (half[r] - 1.0f) * sh4der_recip_pos_(l2) : 0.0f;
        const float a = k * ax[r][0], b = k * ax[r][1];

        /* u = ((px - sx (py - gy) - cx) ax + (pz - sz (py - gy) - cz) az) / |a|^2 ; pixel = origin + half + u (half - 1) */
        m[4u * r] = a; m[4u * r + 2u] = b;
        m[4u * r + 1u] = -(a * sx + b * sz);
        m[4u * r + 3u] = org[r] + half[r] - a * t->c[0] - b * t->c[1] + (a * sx + b * sz) * ground_y;
    }
}

static inline void sht_apply(const float m[8], const float p[3], sht_pv_t *o) {
    o->x = m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3];
    o->y = m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7];
}

/* A pass vertex's weight from its place in the tile: 1 well inside, falling over SHT_FEATHER_PX to 0 at the 1 pixel border,
   under 0 outside (not clamped below: sht_clip needs the slope; sht_argb saturates). */
static inline float sht_weight(const sht_tile_t *t, float x, float y) {
    const float l = x - (float)t->x, r = (float)(t->x + t->w) - x, u = y - (float)t->y, d = (float)(t->y + t->h) - y;
    float e = l < r ? l : r;

    if (u < e) e = u;
    if (d < e) e = d;
    e = (e - 1.0f) * (1.0f / SHT_FEATHER_PX);
    return e > 1.0f ? 1.0f : e;
}

/* The colour a pass vertex is drawn in: white at weight 0, the shadow's colour (what a shadowed ground texel is multiplied
   by, each 0 .. 1) at weight x strength 1. */
static inline uint32_t sht_argb(const sht_tile_t *t, const float shadow[3], float weight) {
    const float s = sh4der_sat_(weight) * t->strength;
    const unsigned r = (unsigned)((1.0f - s * (1.0f - shadow[0])) * 255.0f + 0.5f);
    const unsigned g = (unsigned)((1.0f - s * (1.0f - shadow[1])) * 255.0f + 0.5f);
    const unsigned b = (unsigned)((1.0f - s * (1.0f - shadow[2])) * 255.0f + 0.5f);

    return 0xff000000u | (r << 16) | (g << 8) | b;
}

/* 1 when the pass vertex is inside the tile's drawable rectangle (the tile less its border) */
static inline int sht_inside(const sht_tile_t *t, const sht_pv_t *v) {
    return v->x >= (float)t->x + 1.0f && v->x <= (float)(t->x + t->w) - 1.0f && v->y >= (float)t->y + 1.0f && v->y <= (float)(t->y + t->h) - 1.0f;
}

/* A pass triangle clipped to the tile's drawable rectangle: a convex fan of up to 7 vertices in out, the weights carried
   across each cut. Returns the vertices (0: nothing of it is inside; under 3 is never returned otherwise). Only for a tile
   whose cut is set, and then only for a triangle with a vertex outside (sht_inside). */
static inline unsigned sht_clip(const sht_tile_t *t, const sht_pv_t in[3], sht_pv_t out[7]) {
    const float lim[4] = { (float)t->x + 1.0f, (float)(t->x + t->w) - 1.0f, (float)t->y + 1.0f, (float)(t->y + t->h) - 1.0f };
    sht_pv_t a[8], b[8];
    sht_pv_t *src = a, *dst = b;
    unsigned n = 3u, e, i;

    a[0] = in[0]; a[1] = in[1]; a[2] = in[2];
    for (e = 0; e < 4u && n; e++) {
        const int axis_y = e >= 2u, upper = e & 1u;
        unsigned m = 0;

        for (i = 0; i < n; i++) {
            const sht_pv_t *p = &src[i], *q = &src[(i + 1u) % n];
            const float dp = upper ? lim[e] - (axis_y ? p->y : p->x) : (axis_y ? p->y : p->x) - lim[e];
            const float dq = upper ? lim[e] - (axis_y ? q->y : q->x) : (axis_y ? q->y : q->x) - lim[e];

            if (dp >= 0.0f) dst[m++] = *p;
            if ((dp >= 0.0f) != (dq >= 0.0f)) {
                const float s = dp * sh4der_recip_pos_(sh4der_abs_(dp - dq));     /* |dp| / |dp - dq|, signs handled below */
                const float u = dp >= 0.0f ? s : -s;

                dst[m].x = p->x + (q->x - p->x) * u; dst[m].y = p->y + (q->y - p->y) * u; dst[m].w = p->w + (q->w - p->w) * u;
                if (axis_y) dst[m].y = lim[e]; else dst[m].x = lim[e];
                m++;
            }
        }
        n = m;
        { sht_pv_t *sw = src; src = dst; dst = sw; }
    }
    if (n < 3u) return 0u;
    for (i = 0; i < n && i < 7u; i++) out[i] = src[i];
    return n < 7u ? n : 7u;
}

/* The ground quad of a tile on the screen's side: four world corners (lifted `lift` above the plane, 2 to 4 cm) and their
   texture coordinates in a target tw x th. Corner order: (-ax, -ay), (+ax, -ay), (+ax, +ay), (-ax, +ay). */
static inline void sht_quad(const sht_tile_t *t, float ground_y, float lift, unsigned tw, unsigned th, float pos[4][3], float uv[4][2]) {
    static const float sa[4] = { -1.0f, 1.0f, 1.0f, -1.0f }, sb[4] = { -1.0f, -1.0f, 1.0f, 1.0f };
    const float iw = sh4der_recip_pos_((float)tw), ih = sh4der_recip_pos_((float)th);
    const float cx = (float)t->x + 0.5f * (float)t->w, cy = (float)t->y + 0.5f * (float)t->h;
    unsigned k;

    for (k = 0; k < 4u; k++) {
        pos[k][0] = t->c[0] + sa[k] * t->ax[0] + sb[k] * t->ay[0];
        pos[k][1] = ground_y + lift;
        pos[k][2] = t->c[1] + sa[k] * t->ax[1] + sb[k] * t->ay[1];
        uv[k][0] = (cx + sa[k] * (0.5f * (float)t->w - 1.0f)) * iw;
        uv[k][1] = (cy + sb[k] * (0.5f * (float)t->h - 1.0f)) * ih;
    }
}

/* The rectangle from the target's origin that covers every tile with strength above 0, in tiles of 32: what the pass clears
   and what the tile cut is given. 0 x 0: no pass this frame. Returns 1 when it is within the tile cut's limit. */
static inline int sht_cut(const sht_tile_t *t, unsigned n, unsigned *cw, unsigned *ch) {
    unsigned k, w = 0, h = 0;

    for (k = 0; k < n; k++) {
        if (t[k].caster < 0 || !(t[k].strength > 0.0f)) continue;
        if ((unsigned)(t[k].x + t[k].w) > w) w = (unsigned)(t[k].x + t[k].w);
        if ((unsigned)(t[k].y + t[k].h) > h) h = (unsigned)(t[k].y + t[k].h);
    }
    *cw = w / SHT_UNIT; *ch = h / SHT_UNIT;
    return *cw * *ch <= SHT_CUT_MAX;
}

/* the town's first budget: 8 people as strips, 4 hens as squares, in a 256 x 256 target (131,072 bytes at 16 bits) */
enum { SHT_KIND_PERSON, SHT_KIND_HEN, SHT_KIND_CAR, SHT_KINDS };
static const sht_kind_t sht_kinds_default[SHT_KINDS] = {
    { 128u, 32u, 4.0f, 1.0f },      /* a person: 3.2 cm a texel along and across */
    { 32u, 32u, 1.25f, 1.25f },     /* a hen: 4.2 cm */
    { 128u, 128u, 10.0f, 10.0f },   /* a car, as the car demo's 10 m square: 7.9 cm */
};

#endif /* PSM_SHTILE_H */
