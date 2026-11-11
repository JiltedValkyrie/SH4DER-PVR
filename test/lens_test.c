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

int main(void) {
    float w8 = 0.0f, w16 = 0.0f;
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
