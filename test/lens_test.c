/* SH4DER-PVR

   test/lens_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the lens effects: the vignette metric and falloff, the
   vignette mesh (rings, rim, and the Gouraud colour across each triangle
   against the exact curve), the grain tile and its mean-neutral boost, the
   frame offsets, the hit indicators and their wedge geometry, and the flash
   envelope.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_lens.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.0002f
#endif

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int near(float a, float b, float tol) { return fabsf(a - b) <= tol; }

/* The vignette metric for 4:3, 16:9 and the circle, and the falloff from 1 to 1 - strength. */
static int metric_and_falloff(void) {
    sh4der_lens_vig_t v;
    float m_prev = 2.0f;
    int i;
    /* 4:3 and 16:9, ROUND 0: the middle of each screen edge is r = 1, the corners sqrt(2) */
    sh4der_lens_vig_default(&v, 640.0f, 480.0f);
    if (!near(sh4der_lens_vig_r(&v, 320.0f, 0.0f), 1.0f, 1e-3f) || !near(sh4der_lens_vig_r(&v, 0.0f, 240.0f), 1.0f, 1e-3f))
        return fail("4:3 edge middles at r 1");
    if (!near(sh4der_lens_vig_r(&v, 320.0f, 240.0f), 1.41421f, 2e-3f))
        return fail("4:3 corner at sqrt 2");
    sh4der_lens_vig_default(&v, 854.0f, 480.0f);
    if (!near(sh4der_lens_vig_r(&v, 427.0f, 0.0f), 1.0f, 1e-3f) || !near(sh4der_lens_vig_r(&v, 0.0f, 240.0f), 1.0f, 1e-3f))
        return fail("16:9 edge middles at r 1");
    /* ROUND 1 (circle): the top middle is r 1, the side middle is (W/2)/(H/2) = 16:9 wide */
    v.kx = 1.0f;
    if (!near(sh4der_lens_vig_r(&v, 0.0f, 240.0f), 1.0f, 1e-3f) || !near(sh4der_lens_vig_r(&v, 427.0f, 0.0f), 1.7792f, 2e-3f))
        return fail("circle metric");
    /* falloff: 1 inside INNER, 1 - STRENGTH at OUTER, monotone between, smooth ends */
    sh4der_lens_vig_default(&v, 640.0f, 480.0f);
    if (!near(sh4der_lens_vig_m(&v, 0.0f), 1.0f, 1e-6f) || !near(sh4der_lens_vig_m(&v, v.inner), 1.0f, 1e-6f))
        return fail("falloff starts at 1");
    if (!near(sh4der_lens_vig_m(&v, v.outer), 1.0f - v.strength, 1e-6f) || !near(sh4der_lens_vig_m(&v, 9.0f), 1.0f - v.strength, 1e-6f))
        return fail("falloff ends at 1 - strength");
    for (i = 0; i <= 100; i++) {
        const float r = v.inner + (v.outer - v.inner) * (float)i / 100.0f;
        const float m = sh4der_lens_vig_m(&v, r);
        if (m > m_prev + 1e-6f)
            return fail("falloff monotone");
        m_prev = m;
    }
    return 0;
}

/* Gouraud on one triangle: barycentric colour at (px, py); returns 0 if outside. */
static int bary(const sh4der_lens_vtx_t *a, const sh4der_lens_vtx_t *b, const sh4der_lens_vtx_t *c, float px, float py, float *ch) {
    const float d = (b->y - c->y) * (a->x - c->x) + (c->x - b->x) * (a->y - c->y);
    float l1, l2, l3;
    if (fabsf(d) < 1e-9f) return 0;
    l1 = ((b->y - c->y) * (px - c->x) + (c->x - b->x) * (py - c->y)) / d;
    l2 = ((c->y - a->y) * (px - c->x) + (a->x - c->x) * (py - c->y)) / d;
    l3 = 1.0f - l1 - l2;
    if (l1 < -1e-5f || l2 < -1e-5f || l3 < -1e-5f) return 0;
    *ch = l1 * (float)((a->argb >> 16) & 255u) + l2 * (float)((b->argb >> 16) & 255u) + l3 * (float)((c->argb >> 16) & 255u);
    return 1;
}

/* The mesh: counts, the screen bounds, the ring colours, and the worst Gouraud error on a grid (red channel, 8-bit steps). */
static int mesh_check(unsigned spokes, unsigned rings, float *worst_out) {
    sh4der_lens_vig_t v;
    static sh4der_lens_vtx_t m[(SH4DER_LENS_RINGS_MAX + 1) * 2 * (SH4DER_LENS_SPOKES_MAX + 1)];
    unsigned n, bands = rings + 1u, gx, gy;
    float worst = 0.0f;
    sh4der_lens_vig_default(&v, 640.0f, 480.0f);
    v.spokes = spokes;
    v.rings = rings;
    n = sh4der_lens_vig_mesh(&v, m, sizeof(m) / sizeof(m[0]));
    if (n != bands * 2u * (spokes + 1u))
        return fail("mesh vertex count");
    /* ring 0 (A of band 0) is white, the outer ring (B of the last falloff band) is 1 - strength */
    if (((m[0].argb >> 16) & 255u) != 255u)
        return fail("inner ring white");
    /* the outer ring (OUTER) is at 1 - strength; the rim past it is flat at the same value */
    if (!near((float)((m[2 * (spokes + 1u) * rings + 0].argb >> 16) & 255u), 255.0f * (1.0f - v.strength), 1.0f))
        return fail("outer ring at 1 - strength");
    if (!near((float)((m[2 * (spokes + 1u) * bands - 1u].argb >> 16) & 255u), 255.0f * (1.0f - v.strength), 1.0f))
        return fail("rim at 1 - strength");
    /* Gouraud against the curve: sample each pixel of a 160 x 120 grid, the triangle it falls in */
    for (gy = 0; gy < 120u; gy++)
        for (gx = 0; gx < 160u; gx++) {
            const float px = (gx + 0.5f) * 4.0f, py = (gy + 0.5f) * 4.0f;
            const float r = sh4der_lens_vig_r(&v, px - 320.0f, py - 240.0f);
            float want = 255.0f * sh4der_lens_vig_m(&v, r), got = -1.0f, ch;
            unsigned band, j;
            int hit = 0;
            for (band = 0; band < bands && !hit; band++)
                for (j = 0; j < 2u * spokes && !hit; j++) {
                    const sh4der_lens_vtx_t *s = &m[band * 2u * (spokes + 1u) + j];
                    if (bary(s, s + 1, s + 2, px, py, &ch)) { got = ch; hit = 1; }
                }
            if (!hit) {   /* inside the untouched centre (multiply by 1): only valid there */
                if (r > v.inner + 0.02f)
                    return fail("mesh misses the falloff");
                continue;
            }
            if (fabsf(got - want) > worst) worst = fabsf(got - want);
        }
    *worst_out = worst;
    return 0;
}

/* The grain tile: level range, seed, texel format, mean near one half, the mean-neutral boost, frame offsets. */
static int grain(void) {
    uint32_t sum = 0, i, texels = (uint32_t)SH4DER_LENS_GRAIN_TILE * SH4DER_LENS_GRAIN_TILE;
    uint32_t seen[16] = { 0 };
    float mean, boost, uv[4], prev[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
    uint32_t f;
    unsigned same = 0;
    for (i = 0; i < texels; i++) {
        const unsigned lv = sh4der_lens_grain_level(i, SH4DER_LENS_GRAIN_SEED);
        if (lv > 15u) return fail("grain level range");
        sum += lv;
        seen[lv]++;
    }
    if (sh4der_lens_grain_level(7u, 1u) == sh4der_lens_grain_level(7u, 2u) && sh4der_lens_grain_level(8u, 1u) == sh4der_lens_grain_level(8u, 2u) && sh4der_lens_grain_level(9u, 1u) == sh4der_lens_grain_level(9u, 2u))
        return fail("seed changes the tile");
    if (sh4der_lens_grain_argb4444(15u) != 0xffffu || sh4der_lens_grain_argb4444(0u) != 0xf000u)
        return fail("argb4444 texel");
    mean = sh4der_lens_grain_mean(sum, texels);
    if (!near(mean, 0.5f, 0.01f))
        return fail("grain mean near half");
    /* the boost gives the mean back: (1 - a + a mean)(1 + boost) = 1, and a frame x keeps its brightness */
    boost = sh4der_lens_grain_boost(SH4DER_LENS_GRAIN_AMP, mean);
    for (i = 0; i <= 19; i++) {
        const float x = (float)i / 20.0f;
        const float out = x * (1.0f - SH4DER_LENS_GRAIN_AMP + SH4DER_LENS_GRAIN_AMP * mean) * (1.0f + boost);
        if (fabsf(out - x) > 1e-4f) return fail("grain mean neutral");
    }
    /* the frame offset: in the tile, changing, and not a short repeat (no repeat in the first 4096 frames) */
    for (f = 0; f < 4096u; f++) {
        sh4der_lens_grain_uv(f, 320.0f, 240.0f, uv);
        if (uv[0] < 0.0f || uv[0] >= 1.0f || uv[1] < 0.0f || uv[1] >= 1.0f || !near(uv[2] - uv[0], 5.0f, 1e-4f) || !near(uv[3] - uv[1], 3.75f, 1e-4f))
            return fail("grain uv range and span");
        if (near(uv[0], prev[0], 1e-6f) && near(uv[1], prev[1], 1e-6f)) same++;
        memcpy(prev, uv, sizeof(prev));
    }
    if (same > 40u) return fail("offsets do not repeat each frame");
    return 0;
}

/* The hit indicators: merging, the slot limit, the alpha envelope and the wedge geometry on the screen edges. */
static int hits(void) {
    sh4der_lens_hit_t s[SH4DER_LENS_HIT_MAX];
    sh4der_lens_vtx_t q[4];
    float a;
    unsigned live = 0, i;
    memset(s, 0, sizeof(s));
    /* front (forward up on screen): one hit; a hit from the same bearing merges */
    if (!sh4der_lens_hit_add(s, SH4DER_LENS_HIT_MAX, 0.0f, 1.0f)) return fail("hit added");
    sh4der_lens_hit_tick(s, SH4DER_LENS_HIT_MAX, 0.5f);
    sh4der_lens_hit_add(s, SH4DER_LENS_HIT_MAX, 0.05f, 1.0f);
    for (i = 0; i < SH4DER_LENS_HIT_MAX; i++) live += s[i].on;
    if (live != 1 || s[0].age != 0.0f) return fail("same bearing merges");
    /* four distinct bearings: four slots, a fifth replaces the oldest */
    memset(s, 0, sizeof(s));
    sh4der_lens_hit_add(s, SH4DER_LENS_HIT_MAX, 1.0f, 0.0f);   /* right */
    sh4der_lens_hit_tick(s, SH4DER_LENS_HIT_MAX, 0.2f);
    sh4der_lens_hit_add(s, SH4DER_LENS_HIT_MAX, -1.0f, 0.0f);  /* left */
    sh4der_lens_hit_tick(s, SH4DER_LENS_HIT_MAX, 0.2f);
    sh4der_lens_hit_add(s, SH4DER_LENS_HIT_MAX, 0.0f, -1.0f);  /* back */
    sh4der_lens_hit_tick(s, SH4DER_LENS_HIT_MAX, 0.2f);
    sh4der_lens_hit_add(s, SH4DER_LENS_HIT_MAX, 0.0f, 1.0f);   /* front */
    live = 0;
    for (i = 0; i < SH4DER_LENS_HIT_MAX; i++) live += s[i].on;
    if (live != 4) return fail("four bearings, four slots");
    sh4der_lens_hit_add(s, SH4DER_LENS_HIT_MAX, 0.7071f, 0.7071f);   /* a fifth: the oldest (right, age 0.6) goes */
    live = 0;
    for (i = 0; i < SH4DER_LENS_HIT_MAX; i++) live += s[i].on;
    if (live != 4) return fail("fifth replaces");
    /* alpha envelope: ramp, hold, fade, gone */
    if (!near(sh4der_lens_hit_alpha(0.0f), 0.0f, 1e-6f) || !near(sh4der_lens_hit_alpha(SH4DER_LENS_HIT_ATTACK_S), 1.0f, 1e-6f)) return fail("alpha ramp");
    if (!near(sh4der_lens_hit_alpha(SH4DER_LENS_HIT_HOLD_S), 1.0f, 1e-6f)) return fail("alpha hold");
    a = sh4der_lens_hit_alpha(SH4DER_LENS_HIT_LIFE_S);
    if (!near(a, 0.0f, 1e-6f) || !(sh4der_lens_hit_alpha(1.25f) > 0.0f && sh4der_lens_hit_alpha(1.25f) < 1.0f)) return fail("alpha fade");
    /* the wedge from the front: the top edge at the middle, outer edge alpha full, inner alpha 0 */
    memset(s, 0, sizeof(s));
    sh4der_lens_hit_add(s, SH4DER_LENS_HIT_MAX, 0.0f, 1.0f);
    if (sh4der_lens_hit_quad(&s[0], 640.0f, 480.0f, 1.0f, q) != 4) return fail("wedge is a quad");
    if (!near(0.5f * (q[0].x + q[1].x), 320.0f, 1e-3f) || !near(q[0].y, 0.0f, 1e-3f) || !near(q[1].y, 0.0f, 1e-3f)) return fail("front wedge on the top edge");
    if (!near(q[2].y, SH4DER_LENS_HIT_DEPTH, 1e-3f) || (q[0].argb >> 24) != 204u || (q[2].argb >> 24) != 0u) return fail("wedge alpha: edge full, inner 0");
    /* a right hit: the right edge at the middle */
    memset(s, 0, sizeof(s));
    sh4der_lens_hit_add(s, SH4DER_LENS_HIT_MAX, 1.0f, 0.0f);
    sh4der_lens_hit_quad(&s[0], 640.0f, 480.0f, 1.0f, q);
    if (!near(0.5f * (q[0].x + q[1].x), 640.0f, 1e-3f) || !near(0.5f * (q[0].y + q[1].y), 240.0f, 1e-3f)) return fail("right wedge on the right edge");
    /* a corner-ish bearing: the edge point is on the rectangle */
    memset(s, 0, sizeof(s));
    sh4der_lens_hit_add(s, SH4DER_LENS_HIT_MAX, 1.0f, 1.0f);
    sh4der_lens_hit_quad(&s[0], 640.0f, 480.0f, 1.0f, q);
    /* right + forward: up-right, so the edge point is on the top edge at x = 320 + 240 */
    if (!(near(0.5f * (q[0].x + q[1].x), 560.0f, 1e-2f) && near(0.5f * (q[0].y + q[1].y), 0.0f, 1e-2f))) return fail("corner wedge on the rectangle");
    /* zero direction: no hit */
    if (sh4der_lens_hit_add(s, SH4DER_LENS_HIT_MAX, 0.0f, 0.0f)) return fail("zero direction ignored");
    return 0;
}

/* The flash: the target keeps the peak, the level rises to the peak and decays, the colour alpha in each mode. */
static int flash(void) {
    sh4der_lens_flash_t f;
    float t, lvl_peak = 0.0f, late;
    const float rgb[3] = { SH4DER_LENS_FLASH_R, SH4DER_LENS_FLASH_G, SH4DER_LENS_FLASH_B };
    memset(&f, 0, sizeof(f));
    sh4der_lens_flash_hit(&f, SH4DER_LENS_FLASH_PEAK);
    sh4der_lens_flash_hit(&f, SH4DER_LENS_FLASH_PEAK * 0.5f);   /* a weaker hit does not lower the target */
    if (!near(f.target, SH4DER_LENS_FLASH_PEAK, 1e-6f)) return fail("flash target keeps the peak");
    for (t = 0.0f; t < 2.0f; t += 1.0f / 60.0f) {
        sh4der_lens_flash_step(&f, 1.0f / 60.0f);
        if (f.level > lvl_peak) lvl_peak = f.level;
    }
    if (!(lvl_peak > 0.9f * SH4DER_LENS_FLASH_PEAK && lvl_peak <= SH4DER_LENS_FLASH_PEAK + 1e-4f)) return fail("flash rises to the peak");
    for (t = 0.0f; t < 3.0f; t += 1.0f / 60.0f) sh4der_lens_flash_step(&f, 1.0f / 60.0f);
    late = f.level;
    if (!(late < 0.01f)) return fail("flash decays");
    if (sh4der_lens_flash_argb(0.5f, rgb, 1) >> 24 != 0u || (sh4der_lens_flash_argb(0.5f, rgb, 0) >> 24) != 128u) return fail("flash alpha or additive");
    return 0;
}

/* The pins: what the module gave before it grew its variations (taken on that code, 2026-11-26, the plain C path).
   The first forms, with their default values, must give these bytes for ever: the 64 x 64 tile, the default vignette
   mesh at two screen sizes, the frame offsets, the boost, and the grain quads' three colours as the draw packs them.
   On the SH4ZAM path the square root and the divide round their own way: the hashes are printed there, not held. */
static uint32_t fnv_(uint32_t h, const void *p, size_t n) {
    const uint8_t *b = (const uint8_t *)p;
    size_t i;
    for (i = 0; i < n; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}
#define PIN_TILE     0xbd94a7adu
#define PIN_TILE_SUM 30330u
#define PIN_MESH_640 0x16ac8066u
#define PIN_MESH_320 0xc0939f20u
#define PIN_UV       0x6e4836a1u
#define PIN_BOOST    0xe3e632e4u
#define PIN_COLOURS  0xbec53609u
static int pins(void) {
    static sh4der_lens_vtx_t m[(SH4DER_LENS_RINGS_MAX + 1) * 2 * (SH4DER_LENS_SPOKES_MAX + 1)];
    const uint32_t texels = (uint32_t)SH4DER_LENS_GRAIN_TILE * SH4DER_LENS_GRAIN_TILE;
    uint32_t h_tile = 2166136261u, h_mesh[2] = { 2166136261u, 2166136261u }, h_uv = 2166136261u, h_boost, h_col = 2166136261u, sum = 0, i, f, col[3];
    sh4der_lens_vig_t v;
    float uv[4], boost;
    unsigned n, k;
    for (i = 0; i < texels; i++) {
        const unsigned lv = sh4der_lens_grain_level(i, SH4DER_LENS_GRAIN_SEED);
        const uint16_t t = sh4der_lens_grain_argb4444(lv);
        sum += lv;
        h_tile = fnv_(h_tile, &t, sizeof t);
    }
    for (k = 0; k < 2u; k++) {
        sh4der_lens_vig_default(&v, k ? 320.0f : 640.0f, k ? 240.0f : 480.0f);
        n = sh4der_lens_vig_mesh(&v, m, sizeof(m) / sizeof(m[0]));
        if (n != 594u) return fail("the default mesh is 594 vertices");
        for (i = 0; i < n; i++) { h_mesh[k] = fnv_(h_mesh[k], &m[i].x, 4u); h_mesh[k] = fnv_(h_mesh[k], &m[i].y, 4u); h_mesh[k] = fnv_(h_mesh[k], &m[i].argb, 4u); }
    }
    for (f = 0; f < 1000u; f++) { sh4der_lens_grain_uv(f, 640.0f, 480.0f, uv); h_uv = fnv_(h_uv, uv, sizeof uv); }
    boost = sh4der_lens_grain_boost(SH4DER_LENS_GRAIN_AMP, sh4der_lens_grain_mean(sum, texels));
    h_boost = fnv_(2166136261u, &boost, sizeof boost);
    {   /* as sh4der_lens_grain packs them: the amplitude, 1 - it, the boost */
        const uint32_t a = (uint32_t)(SH4DER_LENS_GRAIN_AMP * 255.0f + 0.5f);
        col[0] = 0xff000000u | (a << 16) | (a << 8) | a;
        col[1] = 0xff000000u | ((255u - a) << 16) | ((255u - a) << 8) | (255u - a);
        col[2] = sh4der_lens_argb_(1.0f, boost, boost, boost);
        h_col = fnv_(h_col, col, sizeof col);
    }
    printf("lens pins: tile %08x (sum %u), mesh 640 %08x, mesh 320 %08x, uv %08x, boost %08x, colours %08x (%08x %08x %08x)\n",
           (unsigned)h_tile, (unsigned)sum, (unsigned)h_mesh[0], (unsigned)h_mesh[1], (unsigned)h_uv, (unsigned)h_boost, (unsigned)h_col,
           (unsigned)col[0], (unsigned)col[1], (unsigned)col[2]);
    if (h_tile != PIN_TILE || sum != PIN_TILE_SUM) return fail("pin: the tile moved");
#ifdef SH4DER_CORE_NO_SH4ZAM
    if (h_uv != PIN_UV) return fail("pin: the frame offsets moved");
    if (h_mesh[0] != PIN_MESH_640 || h_mesh[1] != PIN_MESH_320) return fail("pin: the default vignette mesh moved");
    if (h_boost != PIN_BOOST || h_col != PIN_COLOURS) return fail("pin: the boost or the grain's colours moved");
#endif
    return 0;
}

/* The grain's variations: the tile's sizes and kinds, any tile's mean, the movements, the colours and their neutrality. */
static int grain_variations(void) {
    static uint16_t px[SH4DER_LENS_GRAIN_TILE_MAX * SH4DER_LENS_GRAIN_TILE_MAX + 1];
    static const unsigned sides[4] = { 16u, 32u, 64u, 128u }, bad_sides[5] = { 0u, 8u, 24u, 100u, 256u };
    static const float amps[4] = { 0.03f, 0.06f, 0.2f, 0.5f };
    const float means[3] = { 0.3f, 0.5f, 0.8f };
    uint32_t sum[3], h = 2166136261u, f, col[3], mono_ends = 0, soft_ends = 0;
    float mean[3], uv[4], old[4], prev[4] = { 0 };
    unsigned i, k, n, changes = 0;

    for (i = 0; i < 4u; i++) if (!sh4der_lens_grain_tile_ok(sides[i])) return fail("a tile side that should pass");
    for (i = 0; i < 5u; i++) if (sh4der_lens_grain_tile_ok(bad_sides[i])) return fail("a tile side that should not pass");

    /* MONO at the first form's size and seed is the first form's tile, texel for texel */
    px[4096] = 0x1234u;
    if (sh4der_lens_grain_fill(px, 64u, SH4DER_LENS_GRAIN_SEED, SH4DER_LENS_GRAIN_MONO, sum) != 4096u) return fail("fill count");
    for (i = 0; i < 4096u; i++) { h = fnv_(h, &px[i], sizeof px[0]); mono_ends += (px[i] & 15u) == 0u || (px[i] & 15u) == 15u; }
    if (h != PIN_TILE || sum[0] != PIN_TILE_SUM || sum[1] != sum[0] || sum[2] != sum[0] || px[4096] != 0x1234u) return fail("MONO fill is not the first form's tile");
    if (!sh4der_lens_grain_tile_mean(px, 4096u, mean) || !near(mean[0], sh4der_lens_grain_mean(sum[0], 4096u), 1e-6f) || mean[1] != mean[0] || mean[2] != mean[0])
        return fail("the tile's mean from its texels");
    /* CHROMA: three channels of their own, each near a half; SOFT: grey, near a half, fewer texels at the two ends */
    if (sh4der_lens_grain_fill(px, 64u, SH4DER_LENS_GRAIN_SEED, SH4DER_LENS_GRAIN_CHROMA, sum) != 4096u) return fail("chroma fill");
    for (n = 0, i = 0; i < 4096u; i++) n += ((px[i] >> 8) & 15u) != (px[i] & 15u) || ((px[i] >> 4) & 15u) != (px[i] & 15u);
    if (n < 3000u || sum[0] == sum[1] || sum[1] == sum[2]) return fail("chroma channels are not their own");
    for (k = 0; k < 3u; k++) if (!near(sh4der_lens_grain_mean(sum[k], 4096u), 0.5f, 0.02f)) return fail("chroma mean near half");
    if (sh4der_lens_grain_fill(px, 64u, SH4DER_LENS_GRAIN_SEED, SH4DER_LENS_GRAIN_SOFT, sum) != 4096u) return fail("soft fill");
    for (i = 0; i < 4096u; i++) {
        if (((px[i] >> 8) & 15u) != (px[i] & 15u) || ((px[i] >> 4) & 15u) != (px[i] & 15u) || (px[i] >> 12) != 15u) return fail("soft texel is grey, alpha 15");
        soft_ends += (px[i] & 15u) == 0u || (px[i] & 15u) == 15u;
    }
    if (!near(sh4der_lens_grain_mean(sum[0], 4096u), 0.5f, 0.03f) || soft_ends * 3u > mono_ends) return fail("soft: mean near half, fewer extremes");
    printf("lens grain kinds, 64 x 64: texels at level 0 or 15: mono %u, soft %u\n", (unsigned)mono_ends, (unsigned)soft_ends);
    for (i = 0; i < 4u; i++) {                                           /* every size, no texel past the tile */
        const unsigned t = sides[i];
        px[t * t] = 0x4321u;
        if (sh4der_lens_grain_fill(px, t, 7u, SH4DER_LENS_GRAIN_MONO, sum) != t * t || px[t * t] != 0x4321u) return fail("fill size");
        if (!near(sh4der_lens_grain_mean(sum[0], t * t), 0.5f, t == 16u ? 0.06f : 0.03f)) return fail("fill mean, a size");
    }
    px[0] = 0x7777u;                                                     /* bad arguments write nothing */
    if (sh4der_lens_grain_fill(px, 48u, 1u, SH4DER_LENS_GRAIN_MONO, sum) || sh4der_lens_grain_fill(px, 64u, 1u, 3u, sum) ||
        sh4der_lens_grain_fill(NULL, 64u, 1u, 0u, sum) || sh4der_lens_grain_fill(px, 64u, 1u, 0u, NULL) || px[0] != 0x7777u) return fail("fill refuses bad arguments");
    /* a tile that is not half grey: its own mean a channel */
    for (i = 0; i < 256u; i++) px[i] = 0xf8a3u;
    if (!sh4der_lens_grain_tile_mean(px, 256u, mean) || !near(mean[0], 8.0f / 15.0f, 1e-5f) || !near(mean[1], 10.0f / 15.0f, 1e-5f) || !near(mean[2], 3.0f / 15.0f, 1e-5f))
        return fail("mean of a coloured tile");
    if (sh4der_lens_grain_tile_mean(NULL, 256u, mean) || mean[0] != 0.5f || sh4der_lens_grain_tile_mean(px, 0u, mean) || sh4der_lens_grain_tile_mean(px, 4u, NULL)) return fail("mean refuses bad arguments");

    /* movement. JUMP at the first form's tile is sh4der_lens_grain_uv */
    for (f = 0; f < 1000u; f++) {
        sh4der_lens_grain_uv(f, 640.0f, 480.0f, old);
        sh4der_lens_grain_uv_ex(SH4DER_LENS_MOVE_JUMP, f, 1u, SH4DER_LENS_GRAIN_TILE, SH4DER_LENS_GRAIN_PX, 640.0f, 480.0f, uv);
        if (memcmp(old, uv, sizeof uv)) return fail("JUMP is not the first form's offsets");
        sh4der_lens_grain_uv_ex(SH4DER_LENS_MOVE_HOLD, f, 3u, 64u, 1u, 640.0f, 480.0f, uv);
        sh4der_lens_grain_uv(f / 3u, 640.0f, 480.0f, old);
        if (memcmp(old, uv, sizeof uv)) return fail("HOLD 3 is the jump of frame / 3");
        if (f % 3u != 0u && memcmp(prev, uv, sizeof uv)) return fail("HOLD moved inside its hold");
        if (f && f % 3u == 0u && memcmp(prev, uv, sizeof uv)) changes++;
        memcpy(prev, uv, sizeof prev);
        sh4der_lens_grain_uv_ex(SH4DER_LENS_MOVE_STILL, f, 9u, 64u, 1u, 640.0f, 480.0f, uv);
        if (uv[0] != 0.0f || uv[1] != 0.0f || !near(uv[2], 10.0f, 1e-4f) || !near(uv[3], 7.5f, 1e-4f)) return fail("STILL does not move");
        sh4der_lens_grain_uv_ex(SH4DER_LENS_MOVE_DRIFT, f, 2u, 64u, 1u, 640.0f, 480.0f, uv);
        if (uv[0] != 0.0f || !near(uv[1], (float)((64u - (f * 2u) % 64u) % 64u) / 64.0f, 1e-6f) || uv[1] < 0.0f || uv[1] >= 1.0f) return fail("DRIFT slides 2 texels a frame");
    }
    if (changes < 300u) return fail("HOLD changes at the end of a hold");
    sh4der_lens_grain_uv_ex(SH4DER_LENS_MOVE_HOLD, 5u, 0u, 64u, 0u, 640.0f, 480.0f, uv);         /* a step and a px of 0 count as 1 */
    sh4der_lens_grain_uv(5u, 640.0f, 480.0f, old);
    if (memcmp(old, uv, sizeof uv)) return fail("step 0 and px 0 count as 1");
    for (i = 0; i < 4u; i++)
        for (k = 1; k <= 2u; k++)
            for (f = 0; f < 200u; f++) {
                sh4der_lens_grain_uv_ex(SH4DER_LENS_MOVE_JUMP, f, 1u, sides[i], k, 640.0f, 360.0f, uv);
                if (uv[0] < 0.0f || uv[0] >= 1.0f || uv[1] < 0.0f || uv[1] >= 1.0f || !near(uv[2] - uv[0], 640.0f / (float)(k * sides[i]), 1e-3f) ||
                    !near(uv[3] - uv[1], 360.0f / (float)(k * sides[i]), 1e-3f) || uv[0] * (float)sides[i] != floorf(uv[0] * (float)sides[i])) return fail("offset in the tile, whole texels, the span by size and px");
            }

    /* the colours: the first form's at its amplitude and mean; half the amplitude; none; past the most */
    if (sh4der_lens_grain_fill(px, 64u, SH4DER_LENS_GRAIN_SEED, SH4DER_LENS_GRAIN_MONO, sum) != 4096u || !sh4der_lens_grain_tile_mean(px, 4096u, mean)) return fail("mono again");
    sh4der_lens_grain_colours(SH4DER_LENS_GRAIN_AMP, mean, &col[0], &col[1], &col[2]);
    if (col[0] != 0xff0f0f0fu || col[1] != 0xfff0f0f0u || col[2] != 0xff080808u) return fail("the first form's three colours");
#ifdef SH4DER_CORE_NO_SH4ZAM
    if (fnv_(2166136261u, col, sizeof col) != PIN_COLOURS) return fail("pin: the colours through sh4der_lens_grain_colours");
#endif
    sh4der_lens_grain_colours(0.03f, mean, &col[0], &col[1], NULL);
    if (col[0] != 0xff080808u || col[1] != 0xfff7f7f7u) return fail("half the amplitude: 8 and 247");
    sh4der_lens_grain_colours(0.0f, mean, &col[0], &col[1], &col[2]);
    if (col[0] != 0xff000000u || col[1] != 0xffffffffu || col[2] != 0xff000000u) return fail("no amplitude changes nothing");
    sh4der_lens_grain_colours(-1.0f, mean, &col[0], NULL, NULL);
    if (col[0] != 0xff000000u) return fail("a negative amplitude is none");
    sh4der_lens_grain_colours(9.0f, mean, &col[0], &col[1], NULL);
    if (col[0] != 0xff808080u || col[1] != 0xff7f7f7fu) return fail("the amplitude is held to the most");
    sh4der_lens_grain_colours(0.06f, NULL, NULL, NULL, &col[2]);
    if (col[2] != 0xff000000u) return fail("no mean: no boost");
    sh4der_lens_grain_colours(0.06f, mean, NULL, NULL, NULL);
    /* neutral for a tile with colour of its own: each channel's mean comes back, to the byte the boost is packed in */
    for (i = 0; i < 4u; i++) {
        sh4der_lens_grain_colours(amps[i], means, &col[0], &col[1], &col[2]);
        for (k = 0; k < 3u; k++) {
            const float b = (float)((col[2] >> (16u - 8u * k)) & 255u) / 255.0f, a = (float)(col[0] & 255u) / 255.0f;
            const float out = 0.5f * (1.0f - a + a * means[k]) * (1.0f + b);
            if (fabsf(out - 0.5f) > 0.5f * (1.5f / 255.0f)) return fail("neutral a channel, a coloured tile");
        }
    }
    return 0;
}

/* The vignette's variations: the round shape, the curves, the centre off the middle (the mesh still covers the screen). */
static int vig_variations(void) {
    static sh4der_lens_vtx_t m[(SH4DER_LENS_RINGS_MAX + 1) * 2 * (SH4DER_LENS_SPOKES_MAX + 1)];
    sh4der_lens_vig_t v, d;
    float mid, worst = 0.0f, cx, cy;
    unsigned n, gx, gy, c, i;

    sh4der_lens_vig_default(&v, 640.0f, 480.0f);
    if (v.cx != 0.5f || v.cy != 0.5f || v.curve != SH4DER_LENS_CURVE_SMOOTH) return fail("the default centre and curve");
    sh4der_lens_vig_round(&v, 1.0f);
    if (v.kx != 1.0f) return fail("round 1 is a circle");
    sh4der_lens_vig_round(&v, 0.0f);
    sh4der_lens_vig_default(&d, 640.0f, 480.0f);
    if (v.kx != d.kx) return fail("round 0 is the default shape");
    sh4der_lens_vig_round(&v, 7.0f);
    if (v.kx != 1.0f) return fail("round is held to 1");
    sh4der_lens_vig_centre(&v, -3.0f, 2.0f);
    if (v.cx != 0.0f || v.cy != 1.0f) return fail("the centre is held to the screen");

    /* the curves: the same ends, their own middles, none rising with the radius */
    mid = 0.5f * (d.inner + d.outer);
    for (c = 0; c <= SH4DER_LENS_CURVE_SQUARE; c++) {
        float was = 2.0f;
        d.curve = c;
        if (!near(sh4der_lens_vig_m(&d, d.inner), 1.0f, 1e-6f) || !near(sh4der_lens_vig_m(&d, d.outer), 1.0f - d.strength, 1e-6f)) return fail("a curve's ends");
        if (!near(sh4der_lens_vig_m(&d, mid), 1.0f - d.strength * (c == SH4DER_LENS_CURVE_SQUARE ? 0.25f : 0.5f), 1e-4f)) return fail("a curve's middle");
        for (i = 0; i <= 100u; i++) {
            const float mm = sh4der_lens_vig_m(&d, d.inner + (d.outer - d.inner) * (float)i / 100.0f);
            if (mm > was + 1e-6f) return fail("a curve rises");
            was = mm;
        }
    }
    d.curve = SH4DER_LENS_CURVE_LINEAR;
    if (!near(sh4der_lens_vig_m(&d, d.inner + 0.25f * (d.outer - d.inner)), 1.0f - 0.25f * d.strength, 1e-4f)) return fail("LINEAR at a quarter");

    /* the centre a quarter in from the left, a third down: ring 0 is INNER from THAT centre, and every pixel of the
       screen past INNER lies in a triangle whose colour is the curve's at its radius from that centre */
    sh4der_lens_vig_default(&v, 640.0f, 480.0f);
    sh4der_lens_vig_centre(&v, 0.25f, 1.0f / 3.0f);
    cx = v.cx * v.w; cy = v.cy * v.h;
    n = sh4der_lens_vig_mesh(&v, m, sizeof(m) / sizeof(m[0]));
    if (n != 594u) return fail("off-centre mesh count");
    for (i = 0; i < 2u * (v.spokes + 1u); i += 2u)
        if (!near(sh4der_lens_vig_r(&v, m[i].x - cx, m[i].y - cy), v.inner, 2e-3f) || ((m[i].argb >> 16) & 255u) != 255u) return fail("ring 0 round the moved centre");
    for (gy = 0; gy < 120u; gy++)
        for (gx = 0; gx < 160u; gx++) {
            const float px = ((float)gx + 0.5f) * 4.0f, py = ((float)gy + 0.5f) * 4.0f;
            const float r = sh4der_lens_vig_r(&v, px - cx, py - cy), want = 255.0f * sh4der_lens_vig_m(&v, r);
            float got = -1.0f, ch;
            unsigned band, j;
            int hit = 0;
            for (band = 0; band <= v.rings && !hit; band++)
                for (j = 0; j < 2u * v.spokes && !hit; j++) {
                    const sh4der_lens_vtx_t *s = &m[band * 2u * (v.spokes + 1u) + j];
                    if (bary(s, s + 1, s + 2, px, py, &ch)) { got = ch; hit = 1; }
                }
            if (!hit) {
                if (r > v.inner + 0.02f) return fail("the off-centre mesh misses part of the screen");
                continue;
            }
            if (fabsf(got - want) > worst) worst = fabsf(got - want);
        }
    printf("lens vignette, centre at (0.25, 0.33): worst Gouraud error %.2f / 255 over the whole screen\n", (double)worst);
    if (worst > 6.0f) return fail("off-centre vignette error");
    return 0;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    float w8 = 0.0f, w16 = 0.0f;
    if (pins()) return 1;
    if (grain_variations() || vig_variations()) return 1;
    if (metric_and_falloff()) return 1;
    if (mesh_check(SH4DER_LENS_VIG_SPOKES, SH4DER_LENS_VIG_RINGS, &w8)) return 1;
    if (grain() || hits() || flash()) return 1;
    printf("lens_test: mesh %u spokes x %u rings, worst Gouraud error %.2f / 255 (half a 5-bit step = 4)\n",
           (unsigned)SH4DER_LENS_VIG_SPOKES, (unsigned)SH4DER_LENS_VIG_RINGS, w8);
    (void)w16;
    if (w8 > 4.0f) return fail("vignette mesh error above half a 5-bit step");
    printf("lens_test: ok\n");
    return 0;
}
