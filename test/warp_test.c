/* SH4DER-PVR

   test/warp_test.c
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).

   Host check of the warps: the ring (band, push outward, zero at the centre
   and outside the band), the haze column (rect, period, climb), the wobble
   (period, moves on), the clamp at the frame edge, the sum of sources, the
   zero-strength identity, the grid's positions and UVs, the blur's scales
   and integer weights (sum 255), the pixelate UVs and the dither tile's
   twiddle order (the feed mask's: alpha on even columns).
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4der_warp.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.0002f
#endif

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int near(float a, float b) { return fabsf(a - b) <= SH4DER_TEST_TOL; }

static sh4der_warp_src_t ring(float cx, float cy, float age, float strength) {
    sh4der_warp_src_t s = { 0 };
    s.kind = SH4DER_WARP_RING; s.cx = cx; s.cy = cy; s.age = age; s.strength = strength;
    return s;
}

static sh4der_warp_src_t haze(float x0, float y0, float x1, float y1, float age, float strength) {
    sh4der_warp_src_t s = { 0 };
    s.kind = SH4DER_WARP_HAZE; s.x0 = x0; s.y0 = y0; s.x1 = x1; s.y1 = y1; s.age = age; s.strength = strength;
    return s;
}

static sh4der_warp_src_t wobble(float age, float strength) {
    sh4der_warp_src_t s = { 0 };
    s.kind = SH4DER_WARP_WOBBLE; s.age = age; s.strength = strength;
    return s;
}

/* The sample offset of one source at (x, y), by the frame's own path (row term, then the point). */
static void offset_of(const sh4der_warp_src_t *s, float x, float y, float out[2]) {
    sh4der_warp_frame_t f = { 0 };
    float wave[SH4DER_WARP_SOURCES] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float p[2];
    f.n = 1;
    f.src[0] = *s;
    wave[0] = sh4der_warp_wave_(s, y);
    sh4der_warp_point(&f, wave, x, y, p);
    out[0] = p[0] - x;
    out[1] = p[1] - y;
}

static int ring_test(void) {
    const float r = SH4DER_WARP_RING_SPEED * 0.5f;   /* radius at 0.5 s */
    float o[2];
    /* At the band's middle the push is the full amplitude, pointing into the centre (the sample comes from inside). */
    {
        sh4der_warp_src_t s = ring(100.0f, 100.0f, 0.5f, 1.0f);
        offset_of(&s, 100.0f + r, 100.0f, o);
        if (!near(o[0], -SH4DER_WARP_RING_AMP) || !near(o[1], 0.0f)) return fail("ring: full push at the band middle, radial");
        offset_of(&s, 100.0f, 100.0f - r, o);
        if (!near(o[0], 0.0f) || !near(o[1], SH4DER_WARP_RING_AMP)) return fail("ring: push along the other axis");
        offset_of(&s, 100.0f + r + 3.0f * SH4DER_WARP_RING_WIDTH, 100.0f, o);
        if (!near(o[0], 0.0f) || !near(o[1], 0.0f)) return fail("ring: zero outside the band");
        offset_of(&s, 100.0f, 100.0f, o);
        if (!near(o[0], 0.0f) || !near(o[1], 0.0f)) return fail("ring: zero at the centre");
    }
    {
        sh4der_warp_src_t s = ring(100.0f, 100.0f, 0.5f, 0.0f);   /* no strength: no push */
        offset_of(&s, 100.0f + r, 100.0f, o);
        if (!near(o[0], 0.0f)) return fail("ring: zero strength, no push");
    }
    return 0;
}

static int haze_test(void) {
    float o[2];
    sh4der_warp_src_t s = haze(100.0f, 50.0f, 140.0f, 200.0f, 0.3f, 1.0f);
    float w0, w1;
    /* Far outside the rect: no push. */
    offset_of(&s, 10.0f, 120.0f, o);
    if (!near(o[0], 0.0f)) return fail("haze: zero outside the rect");
    /* A wave at a row: the same at one period up (the climb moves the phase, so compare at the same age). */
    w0 = sh4der_warp_wave_(&s, 120.0f);
    w1 = sh4der_warp_wave_(&s, 120.0f + SH4DER_WARP_HAZE_PERIOD);
    if (!near(w0, w1)) return fail("haze: period in rows");
    /* The climb: the wave at a row later in age matches an earlier row at an earlier age. */
    {
        sh4der_warp_src_t a = haze(0, 0, 320, 240, 0.0f, 1.0f), b = haze(0, 0, 320, 240, 0.25f, 1.0f);
        const float dy = SH4DER_WARP_HAZE_RISE * 0.25f;
        if (!near(sh4der_warp_wave_(&a, 100.0f + dy), sh4der_warp_wave_(&b, 100.0f))) return fail("haze: climbs at RISE");
    }
    /* Inside the rect, the mask is 1: the push equals the wave. */
    offset_of(&s, 120.0f, 120.0f, o);
    if (!near(o[0], w0 * 1.0f)) return fail("haze: full inside");
    return 0;
}

static int wobble_test(void) {
    float o[2];
    sh4der_warp_src_t s = wobble(1.0f, 1.0f);
    const float a = sh4der_warp_wave_(&s, 40.0f);
    sh4der_warp_src_t t = wobble(1.0f + SH4DER_WARP_WOBBLE_TURN, 1.0f);
    sh4der_warp_src_t h = wobble(1.0f, 0.5f);
    if (!near(a, sh4der_warp_wave_(&s, 40.0f + SH4DER_WARP_WOBBLE_PERIOD))) return fail("wobble: period in rows");
    if (!near(a, sh4der_warp_wave_(&t, 40.0f))) return fail("wobble: moves on by TURN");
    if (!near(2.0f * sh4der_warp_wave_(&h, 40.0f), a)) return fail("wobble: strength scales");
    offset_of(&s, 77.0f, 40.0f, o);
    if (!near(o[0], a) || !near(o[1], 0.0f)) return fail("wobble: sideways only, the row's wave");
    return 0;
}

static int clamp_test(void) {
    /* A ring with a huge push cannot sample outside the frame. */
    sh4der_warp_frame_t f = { 0 };
    float wave[SH4DER_WARP_SOURCES] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float p[2];
    f.n = 1;
    f.src[0] = ring(0.0f, 120.0f, 1.0f / SH4DER_WARP_RING_SPEED, 1.0f);   /* band middle at d = 1: a full push left, past the edge */
    sh4der_warp_point(&f, wave, 1.0f, 120.0f, p);
    if (!(p[0] <= 0.5f + 1e-6f)) return fail("clamp: a push past the edge is held at the edge");
    if (p[0] < 0.5f - 1e-6f || p[0] > (float)SH4DER_WARP_FRAME_W - 0.5f + 1e-6f) return fail("clamp: x inside the frame");
    f.n = 1;
    f.src[0] = wobble(0.0f, 1.0f);
    f.src[0].strength = 1.0f;
    sh4der_warp_point(&f, (float[]){ 0.0f, 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, p);
    if (p[0] < 0.5f - 1e-6f) return fail("clamp: no black pulled in");
    return 0;
}

static int sum_test(void) {
    /* Two sources sum: a wobble at strength 1 and another at strength 0.5 sample the sum of their row terms. */
    sh4der_warp_frame_t f = { 0 };
    float wave[SH4DER_WARP_SOURCES] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float p[2];
    sh4der_warp_src_t a = wobble(0.7f, 1.0f), b = wobble(0.7f, 0.5f);
    const float ya = 90.0f;
    f.n = 2;
    f.src[0] = a;
    f.src[1] = b;
    wave[0] = sh4der_warp_wave_(&a, ya);
    wave[1] = sh4der_warp_wave_(&b, ya);
    sh4der_warp_point(&f, wave, 150.0f, ya, p);
    if (!near(p[0], 150.0f + wave[0] + wave[1])) return fail("sum: sources add");
    return 0;
}

static int identity_test(void) {
    /* No sources, and zero strength: the sample is the pixel itself, the UVs the texel centres. */
    sh4der_warp_frame_t f = { 0 };
    sh4der_warp_vtx_t row[SH4DER_WARP_FINE_X + 1];
    unsigned i;
    sh4der_warp_row(&f, 20u, 15u, 3u, row);
    for (i = 0; i <= 20u; i++) {
        const float x = (float)(i * 16u);
        if (!near(row[i].x, x) || !near(row[i].y, 48.0f)) return fail("grid: positions");
        if (!near(row[i].u, (x < 0.5f ? 0.5f : x > 319.5f ? 319.5f : x) / 512.0f)) return fail("grid: identity u");
        if (!near(row[i].v, 48.0f / 256.0f)) return fail("grid: identity v");
    }
    f.n = 1;
    f.src[0] = ring(100.0f, 100.0f, 0.5f, 0.0f);
    sh4der_warp_row(&f, 40u, 30u, 0u, row);
    if (!near(row[40].x, 320.0f) || !near(row[40].y, 0.0f)) return fail("grid: fine last column");
    return 0;
}

static int blur_test(void) {
    unsigned k, i, sum;
    for (k = 1; k <= SH4DER_WARP_BLUR_COPIES; k++) {
        sum = 0;
        for (i = 0; i < k; i++) {
            sum += sh4der_warp_blur_weight(i, k);
            if (sh4der_warp_blur_scale(i, k, 1.0f) < 1.0f) return fail("blur: copies never below 1");
        }
        if (sum != 255u) return fail("blur: weights sum to 255");
    }
    if (sh4der_warp_blur_scale(0, 4, 1.0f) != 1.0f) return fail("blur: sharp copy is 1");
    if (!near(sh4der_warp_blur_scale(3, 4, 1.0f), 1.0f + SH4DER_WARP_BLUR_ZOOM)) return fail("blur: outer copy zoom");
    if (sh4der_warp_blur_scale(3, 4, 0.0f) != 1.0f) return fail("blur: zero strength, every copy 1");
    return 0;
}

static int pixel_test(void) {
    float uv[2];
    const unsigned cell = SH4DER_WARP_PIXEL_CELL;
    if (SH4DER_WARP_FRAME_W % cell || SH4DER_WARP_FRAME_H % cell) return fail("pixel: cell divides the frame");
    sh4der_warp_pixel_uv(0u, 0u, uv);
    if (!near(uv[0], ((float)(cell / 2u) + 0.5f) / 512.0f) || !near(uv[1], ((float)(cell / 2u) + 0.5f) / 256.0f)) return fail("pixel: block centre texel");
    sh4der_warp_pixel_uv((SH4DER_WARP_FRAME_W / cell) - 1u, (SH4DER_WARP_FRAME_H / cell) - 1u, uv);
    if (uv[0] > 320.0f / 512.0f || uv[1] > 240.0f / 256.0f) return fail("pixel: last block inside the frame");
    return 0;
}

static int dither_test(void) {
    uint16_t tile[64];
    unsigned x, y;
    float lo = 2.0f, hi = -1.0f;
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++) {
            const float m = sh4der_warp_dither_mul(x, y);
            if (m < lo) lo = m;
            if (m > hi) hi = m;
        }
    if (!near(lo, 1.0f - SH4DER_WARP_DITHER * (15.5f / 16.0f)) || !near(hi, 1.0f - SH4DER_WARP_DITHER * (0.5f / 16.0f))) return fail("dither: range");
    /* The feed mask's order: alpha on even columns, whatever the row. */
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
            if ((sh4der_warp_twiddle8(x, y) < 64u) == 0) return fail("dither: index range");
    sh4der_warp_dither_tile(tile);
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++) {
            const uint16_t t = tile[sh4der_warp_twiddle8(x, y)];
            const unsigned l = (unsigned)(t & 15u);
            if ((t >> 12) != 15u || ((t >> 8) & 15u) != l || ((t >> 4) & 15u) != l) return fail("dither: argb4444 grey, alpha 15");
        }
    {
        unsigned idx0 = sh4der_warp_twiddle8(0, 0), idx1 = sh4der_warp_twiddle8(1, 0), idx2 = sh4der_warp_twiddle8(2, 0);
        if (idx0 != 0u || idx1 != 2u || idx2 != 8u) return fail("dither: twiddle x at odd bits");
    }
    return 0;
}

static int rsqrt_test(void) {
    const float v[4] = { 0.25f, 2.0f, 9.0f, 1000.0f };
    unsigned i;
    for (i = 0; i < 4; i++)
        if (fabsf(sh4der_warp_rsqrt_(v[i]) - 1.0f / sqrtf(v[i])) > SH4DER_TEST_TOL * (1.0f / sqrtf(v[i])) + 1e-5f) return fail("rsqrt");
    return 0;
}

int main(void) {
    if (ring_test() || haze_test() || wobble_test() || clamp_test() || sum_test() || identity_test() || blur_test() || pixel_test() || dither_test() || rsqrt_test())
        return 1;
    printf("warp_test: ok\n");
    return 0;
}
