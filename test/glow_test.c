/* SH4DER-PVR

   test/glow_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the glow maths: the threshold table, the texel pack, the
   4 x 4 shrink, the halo split, the darken balance (a neutral scene keeps
   its mean), the ray scales and weights, the ray mapping (the sun maps to
   itself), the distance and sun-share fades.
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4der_glow.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.0002f
#endif

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int near(float a, float b, float tol) { return fabsf(a - b) <= tol; }

static int threshold_table(void) {
    uint8_t lut[256];
    int i;
    sh4der_glow_lut(lut);
    if (lut[0] != 0 || lut[255] != 255)
        return fail("lut ends");
    /* at the threshold (62 / 255 = 0.243 luma? no: threshold 0.62 = 158 / 255) the weight is 0 */
    if (lut[158] != 0 || lut[157] != 0)
        return fail("lut below threshold is not zero");
    /* a half-way luma above the threshold: t = 0.5, weight 0.25 (power 2) */
    {
        const float y = SH4DER_GLOW_THRESHOLD + 0.5f * (1.0f - SH4DER_GLOW_THRESHOLD);
        const float w = sh4der_glow_bright_(y);
        if (!near(w, 0.25f, SH4DER_TEST_TOL))
            return fail("bright: half-way is 0.25 at power 2");
        if (!near((float)lut[(int)(y * 255.0f + 0.5f)] / 255.0f, 0.25f, 0.01f))
            return fail("lut matches bright");
    }
    for (i = 1; i < 256; i++)
        if (lut[i] < lut[i - 1])
            return fail("lut is not monotonic");
    return 0;
}

static int texel_pack(void) {
    uint8_t lut[256];
    uint16_t white = 0xffffu, black = 0x0000u, dim = 0x2104u;   /* dim: about luma 0.13: below threshold */
    sh4der_glow_lut(lut);
    if (sh4der_glow_texel(white, lut) != 0xffffu)
        return fail("white stays white");
    if (sh4der_glow_texel(black, lut) != 0)
        return fail("black stays black");
    if (sh4der_glow_texel(dim, lut) != 0)
        return fail("below threshold: no glow");
    if (sh4der_glow_pack565(255, 255, 255) != 0xffffu || sh4der_glow_pack565(0, 0, 0) != 0)
        return fail("pack565 ends");
    if (sh4der_glow_pack565(0xf8, 0, 0) != 0xf800u || sh4der_glow_pack565(0, 0xfc, 0) != 0x07e0u || sh4der_glow_pack565(0, 0, 0xf8) != 0x001fu)
        return fail("pack565 channels");
    /* a red pixel at full brightness: luma 77/255 is 0.30 (below threshold): no glow; a yellow one: luma 0.89, weight 0.68 */
    if (sh4der_glow_texel(0xffe0u, lut) == 0)
        return fail("bright yellow glows");
    return 0;
}

static int shrink(void) {
    uint16_t src[SH4DER_GLOW_GRID_X * SH4DER_GLOW_GRID_Y];
    uint16_t dst[SH4DER_GLOW_SMALL_X * SH4DER_GLOW_SMALL_Y];
    uint16_t out[SH4DER_GLOW_SMALL_TEX_W * SH4DER_GLOW_SMALL_TEX_H];
    unsigned i;
    for (i = 0; i < sizeof(src) / sizeof(src[0]); i++) src[i] = 0x7bef;   /* grey: 15, 31, 15 */
    sh4der_glow_shrink4(src, SH4DER_GLOW_GRID_X, dst, SH4DER_GLOW_SMALL_X);
    if (dst[0] != 0x7bef || dst[SH4DER_GLOW_SMALL_X * SH4DER_GLOW_SMALL_Y - 1] != 0x7bef)
        return fail("shrink of a flat frame is flat");
    /* one white texel in the first 4 x 4 block: 1 / 16 of its value in that cell */
    for (i = 0; i < sizeof(src) / sizeof(src[0]); i++) src[i] = 0;
    src[0] = 0xffff;
    sh4der_glow_shrink4(src, SH4DER_GLOW_GRID_X, dst, SH4DER_GLOW_SMALL_X);
    if (dst[0] != 0x1082u)   /* one white texel in 16: r 31 / 16 -> 2, g 63 / 16 -> 4, b 31 / 16 -> 2 */
        return fail("shrink of one hot texel is its mean");
    if (dst[1] != 0)
        return fail("shrink leaks past its block");
    (void)out;
    return 0;
}

static int halo_split(void) {
    float big, small;
    sh4der_glow_halo(0.5f, 0.5f, &big, &small);
    if (!near(big, 0.25f, 1e-6f) || !near(small, 0.25f, 1e-6f))
        return fail("halo split at half radius");
    sh4der_glow_halo(0.6f, 0.0f, &big, &small);
    if (!near(big, 0.6f, 1e-6f) || small != 0.0f)
        return fail("halo radius 0: all on the map");
    sh4der_glow_halo(0.6f, 1.0f, &big, &small);
    if (big != 0.0f || !near(small, 0.6f, 1e-6f))
        return fail("halo radius 1: all on the shrink");
    sh4der_glow_halo(0.8f, 0.3f, &big, &small);
    if (!near(big + small, 0.8f, 1e-6f))
        return fail("halo sums to the strength");
    return 0;
}

static int darken_balance(void) {
    /* neutral scene: frame mean F, map mean Mg. (1 - D) F + S Mg == F exactly when D is not capped. */
    const float F = 0.3f, Mg = 0.1f, S = 0.5f;
    const float D = sh4der_glow_darken(F, Mg, S);
    const float out = (1.0f - D) * F + S * Mg;
    if (!near(out, F, 1e-5f))
        return fail("darken keeps the mean");
    if (!near(D, 0.1666667f, 1e-5f))
        return fail("darken value");
    /* a frame with no bright texel: no darken */
    if (sh4der_glow_darken(F, 0.0f, S) != 0.0f)
        return fail("no map, no darken");
    /* a black frame: no darken (no divide) */
    if (sh4der_glow_darken(0.0f, Mg, S) != 0.0f)
        return fail("black frame");
    /* a very bright map over a dark frame: capped */
    if (sh4der_glow_darken(0.05f, 0.5f, 0.9f) != SH4DER_GLOW_DARKEN_MAX)
        return fail("darken cap");
    return 0;
}

static int mean_luma(void) {
    uint16_t px[4] = { 0xffff, 0xffff, 0, 0 };
    if (!near(sh4der_glow_mean_luma(px, 4), 0.5f, 0.01f))
        return fail("mean luma of half white");
    return 0;
}

static int rays(void) {
    int i;
    float prev = 1e9f;
    /* scales: copy 1 at 0.88 with step 0.12, decreasing */
    if (!near(sh4der_glow_ray_scale(1), 0.88f, 1e-6f) || !near(sh4der_glow_ray_scale(4), 0.52f, 1e-6f))
        return fail("ray scales");
    for (i = 1; i <= 6; i++) {
        const float w = sh4der_glow_ray_weight(i);
        if (!(w > 0.0f && w < prev))
            return fail("ray weights decay");
        prev = w;
    }
    if (!near(sh4der_glow_ray_weight(1), SH4DER_GLOW_RAY_WEIGHT, 1e-6f))
        return fail("first ray weight");
    /* the sun maps to itself for any scale: a corner on the sun reads the sun's texel */
    {
        float u, v, u0, v0;
        sh4der_glow_ray_uv(0.6f, 0.4f, 0.3f, 0.4f, 0.3f, &u, &v);
        sh4der_glow_ray_uv(1.0f, 0.4f, 0.3f, 0.4f, 0.3f, &u0, &v0);
        if (!near(u, u0, 1e-5f) || !near(v, v0, 1e-5f))
            return fail("the sun maps to itself");
        /* at the map's edge: the texel centre, never past the last cell */
        sh4der_glow_ray_uv(1.0f, 0.5f, 0.5f, 1.0f, 1.0f, &u, &v);
        if (!near(u, 79.5f / 128.0f, 1e-5f) || !near(v, 59.5f / 64.0f, 1e-5f))
            return fail("map edge texel centre");
        /* a scaled copy samples nearer the sun: a corner at the right edge, sun at the centre, s = 0.5 reads x = 0.75 */
        sh4der_glow_ray_uv(0.5f, 0.5f, 0.5f, 1.0f, 0.5f, &u, &v);
        if (!near(u, sh4der_glow_tex_(0.75f, SH4DER_GLOW_GRID_X, SH4DER_GLOW_TEX_W), 1e-5f))
            return fail("ray scale about the sun");
    }
    return 0;
}

static int fades(void) {
    float c;
    if (!near(sh4der_glow_sun_fade(0.5f, 0.5f), 1.0f, 1e-6f))
        return fail("sun on screen: no fade");
    if (!near(sh4der_glow_sun_fade(0.0f, 1.0f), 1.0f, 1e-6f))
        return fail("sun on the edge: no fade");
    if (sh4der_glow_sun_fade(0.5f, 1.0f + SH4DER_GLOW_SUN_EDGE + 0.01f) != 0.0f)
        return fail("sun past the edge: gone");
    c = sh4der_glow_sun_fade(0.5f, 1.0f + 0.5f * SH4DER_GLOW_SUN_EDGE);
    if (!(c > 0.0f && c < 1.0f))
        return fail("sun off screen: smooth");
    /* distance: full at the sun, gone past REACH */
    c = sh4der_glow_ray_colour(1, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f);
    if (!near(c, SH4DER_GLOW_RAY_WEIGHT, 1e-5f))
        return fail("ray colour at the sun");
    c = sh4der_glow_ray_colour(1, 0.5f, 0.5f, 0.5f, 1.0f + SH4DER_GLOW_RAY_REACH, 1.0f, 1.0f);
    if (c != 0.0f)
        return fail("ray colour past reach");
    /* share: half lit halves the colour; over 1 clamps */
    c = sh4der_glow_ray_colour(1, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 0.5f);
    if (!near(c, 0.5f * SH4DER_GLOW_RAY_WEIGHT, 1e-5f))
        return fail("ray colour by share");
    if (!near(sh4der_glow_ray_colour(1, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 3.0f), SH4DER_GLOW_RAY_WEIGHT, 1e-5f))
        return fail("share clamps");
    return 0;
}

int main(void) {
    int bad = 0;
    bad |= threshold_table();
    bad |= texel_pack();
    bad |= shrink();
    bad |= halo_split();
    bad |= darken_balance();
    bad |= mean_luma();
    bad |= rays();
    bad |= fades();
    if (bad)
        return 1;
    printf("glow_test: ok\n");
    return 0;
}
