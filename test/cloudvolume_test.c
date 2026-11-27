/* SH4DER-PVR: cloud volume test

   test/cloudvolume_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of the cloud volume: the default numbers and the struct sizes; the validity check (every float field NaN /
   infinite / out of range); the maps (filled, wide range, tile on every edge, same seed same bytes, the coarse map is
   never under a texel it covers); the bake (a shadow height is never under the point's own top, a higher sun gives
   shorter shadows, rows can be baked a few at a time); the frame (step starts rise from 0 to 1 and grow, tables fall or
   rise the right way, bad input refused); the march (clear sky above the slab is alpha 0 and costs no sample, low in a
   full deck most of the picture is solid, the same input gives the same bytes, more steps keep the picture: opacity is corrected for the step
   length, a spread marches its share and leaves the rest, counters count); the smooth (flat stays flat, clear pixels
   lend no shade); the palette (every index decodes near the shade and alpha packed, solid cloud gets 96 shades); the
   flight path (finite, moves forward, banks no more than stated).
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/sh4der_cloudvolume.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static sh4der_cloudvolume_maps_t maps, maps2;
#define W 64u
#define H 48u
static uint8_t shade[W * H], alpha[W * H], shade2[W * H], alpha2[W * H];

static float from_bits(uint32_t bits) {
    union { uint32_t u; float f; } v;

    v.u = bits;
    return v.f;
}

static void view_default(sh4der_cloudvolume_view_t *v, float y, float pitch) {
    v->eye[0] = 100.0f; v->eye[1] = y; v->eye[2] = 200.0f;
    v->yaw = 0.0f; v->pitch = pitch; v->roll = 0.0f;
    v->tan_x = 0.78f; v->tan_y = 0.59f;
}

static void defaults_and_valid(void) {
    sh4der_cloudvolume_t c;
    const float bad[3] = { from_bits(0x7f800000u), from_bits(0xff800000u), from_bits(0x7fc00000u) };
    unsigned f, k;

    memset(&c, 0xff, sizeof(c));
    sh4der_cloudvolume_init(&c);
    sh4der_cloudvolume_init(NULL);
    printf("cloudvolume: sizeof maps %u, clouds %u, frame %u\n", (unsigned)sizeof(sh4der_cloudvolume_maps_t), (unsigned)sizeof(sh4der_cloudvolume_t), (unsigned)sizeof(sh4der_cloudvolume_frame_t));
    CHECK(sizeof(sh4der_cloudvolume_maps_t) == 66560u);
    CHECK(sizeof(sh4der_cloudvolume_frame_t) <= 2048u);
    CHECK(c.steps == 16u && c.light_samples == 0u && c.detail == 1u && c.jitter == 1u && c.thickness == 1100.0f && c.tile == 8192.0f);
    CHECK(sh4der_cloudvolume_valid(&c) == 1 && sh4der_cloudvolume_valid(NULL) == 0);
    for (f = 0; f < 15u; f++) {
        for (k = 0; k < 3u; k++) {
            float *field[15];

            sh4der_cloudvolume_init(&c);
            field[0] = &c.tile; field[1] = &c.base_y; field[2] = &c.thickness; field[3] = &c.density; field[4] = &c.coverage;
            field[5] = &c.erosion; field[6] = &c.shadow; field[7] = &c.haze; field[8] = &c.far_m; field[9] = &c.sun[0];
            field[10] = &c.sun[1]; field[11] = &c.sun[2]; field[12] = &c.wind[0]; field[13] = &c.wind[1]; field[14] = &c.churn;
            *field[f] = bad[k];
            CHECK(sh4der_cloudvolume_valid(&c) == 0);
        }
    }
    sh4der_cloudvolume_init(&c); c.steps = 3u; CHECK(!sh4der_cloudvolume_valid(&c));
    sh4der_cloudvolume_init(&c); c.steps = 65u; CHECK(!sh4der_cloudvolume_valid(&c));
    sh4der_cloudvolume_init(&c); c.light_samples = 5u; CHECK(!sh4der_cloudvolume_valid(&c));
    sh4der_cloudvolume_init(&c); c.detail = 3u; CHECK(!sh4der_cloudvolume_valid(&c));
    sh4der_cloudvolume_init(&c); c.coverage = 0.0f; CHECK(!sh4der_cloudvolume_valid(&c));
    sh4der_cloudvolume_init(&c); c.density = 0.0f; CHECK(!sh4der_cloudvolume_valid(&c));
    sh4der_cloudvolume_init(&c); c.sun[0] = c.sun[1] = c.sun[2] = 0.0f; CHECK(!sh4der_cloudvolume_valid(&c));
    sh4der_cloudvolume_init(&c); c.thickness = 1.0f; CHECK(!sh4der_cloudvolume_valid(&c));
}

static void map_checks(void) {
    const unsigned n = SH4DER_CLOUDVOLUME_MAP, nn = SH4DER_CLOUDVOLUME_NOISE;
    unsigned x, y, lo = 255u, hi = 0u, nlo = 255u, nhi = 0u;
    long edge = 0, inner = 0, nedge = 0, ninner = 0;

    CHECK(sh4der_cloudvolume_generate(NULL, 1u) == 0);
    CHECK(sh4der_cloudvolume_generate(&maps, 7u) == 1 && sh4der_cloudvolume_generate(&maps2, 7u) == 1);
    CHECK(memcmp(&maps, &maps2, sizeof(maps)) == 0);
    CHECK(sh4der_cloudvolume_generate(&maps2, 8u) == 1 && memcmp(&maps, &maps2, sizeof(maps)) != 0);
    for (y = 0; y < n; y++) {
        for (x = 0; x < n; x++) {
            const unsigned h = maps.cover[y * n + x] & 255u;

            CHECK((maps.cover[y * n + x] >> 8) == 0u);             /* the shadow byte waits for the bake */
            lo = h < lo ? h : lo;
            hi = h > hi ? h : hi;
            CHECK(maps.reach[(y / 4u) * SH4DER_CLOUDVOLUME_COARSE + x / 4u] >= h);
        }
        edge += labs((long)(maps.cover[y * n + n - 1u] & 255) - (long)(maps.cover[y * n] & 255)) + labs((long)(maps.cover[(n - 1u) * n + y] & 255) - (long)(maps.cover[y] & 255));
        inner += labs((long)(maps.cover[y * n + 63u] & 255) - (long)(maps.cover[y * n + 64u] & 255)) + labs((long)(maps.cover[63u * n + y] & 255) - (long)(maps.cover[64u * n + y] & 255));
    }
    CHECK(hi - lo > 150u);
    CHECK(edge <= 3 * inner + 128);                                /* the wrap-round step is no bigger than a step inside */
    for (x = 0; x < nn * nn * nn; x++) {
        nlo = maps.noise[x] < nlo ? maps.noise[x] : nlo;
        nhi = maps.noise[x] > nhi ? maps.noise[x] : nhi;
    }
    CHECK(nhi - nlo > 150u);
    for (y = 0; y < nn; y++) {
        for (x = 0; x < nn; x++) {
            nedge += labs((long)maps.noise[(y * nn + x) * nn + nn - 1u] - (long)maps.noise[(y * nn + x) * nn]);
            ninner += labs((long)maps.noise[(y * nn + x) * nn + 15u] - (long)maps.noise[(y * nn + x) * nn + 16u]);
        }
    }
    CHECK(nedge <= 3 * ninner + 1024);
}

static void bake_checks(void) {
    const unsigned n = SH4DER_CLOUDVOLUME_MAP;
    sh4der_cloudvolume_t c;
    int32_t thr, gain;
    unsigned i;
    unsigned long low_sum = 0, high_sum = 0;

    sh4der_cloudvolume_init(&c);
    sh4der_cloudvolume_generate(&maps, 7u);
    CHECK(sh4der_cloudvolume_bake(NULL, &c, 0u, n) == 0 && sh4der_cloudvolume_bake(&maps, NULL, 0u, n) == 0 && sh4der_cloudvolume_bake(&maps, &c, 5u, 4u) == 0 && sh4der_cloudvolume_bake(&maps, &c, 0u, n + 1u) == 0);
    CHECK(sh4der_cloudvolume_bake(&maps, &c, 0u, n) == 1);
    sh4der_cloudvolume_cover_law_(c.coverage, &thr, &gain);
    for (i = 0; i < n * n; i++) {
        CHECK((int32_t)(maps.cover[i] >> 8) >= sh4der_cloudvolume_top_(maps.cover[i] & 255, thr, gain));
        low_sum += maps.cover[i] >> 8;
    }
    /* rows a few at a time give the same map */
    sh4der_cloudvolume_generate(&maps2, 7u);
    for (i = 0; i < n; i += 16u) CHECK(sh4der_cloudvolume_bake(&maps2, &c, i, i + 16u) == 1);
    CHECK(memcmp(&maps, &maps2, sizeof(maps)) == 0);
    /* baking twice changes nothing (the height bytes are kept) */
    CHECK(sh4der_cloudvolume_bake(&maps2, &c, 0u, n) == 1 && memcmp(&maps, &maps2, sizeof(maps)) == 0);
    /* a higher sun: shorter shadows */
    c.sun[0] = 0.0f; c.sun[1] = 0.7071f; c.sun[2] = 0.7071f;
    CHECK(sh4der_cloudvolume_bake(&maps2, &c, 0u, n) == 1);
    for (i = 0; i < n * n; i++) high_sum += maps2.cover[i] >> 8;
    CHECK(high_sum < low_sum);
    c.sun[0] = 0.0f; c.sun[1] = 1.0f; c.sun[2] = 0.0f;             /* overhead: no crash, every point its own top */
    CHECK(sh4der_cloudvolume_bake(&maps2, &c, 0u, n) == 1);
    for (i = 0; i < n * n; i++) CHECK((int32_t)(maps2.cover[i] >> 8) == sh4der_cloudvolume_top_(maps2.cover[i] & 255, thr, gain));
}

static void frame_checks(void) {
    sh4der_cloudvolume_t c;
    sh4der_cloudvolume_view_t v;
    sh4der_cloudvolume_frame_t f;
    unsigned i;

    sh4der_cloudvolume_init(&c);
    view_default(&v, 800.0f, -0.1f);
    CHECK(sh4der_cloudvolume_prepare(NULL, &v, 0.0f, 0u, &f) == 0 && sh4der_cloudvolume_prepare(&c, NULL, 0.0f, 0u, &f) == 0 && sh4der_cloudvolume_prepare(&c, &v, 0.0f, 0u, NULL) == 0);
    CHECK(sh4der_cloudvolume_prepare(&c, &v, from_bits(0x7fc00000u), 0u, &f) == 0);
    v.eye[1] = from_bits(0x7f800000u);
    CHECK(sh4der_cloudvolume_prepare(&c, &v, 0.0f, 0u, &f) == 0);
    view_default(&v, 800.0f, -0.1f);
    v.tan_x = 0.0f;
    CHECK(sh4der_cloudvolume_prepare(&c, &v, 0.0f, 0u, &f) == 0);
    view_default(&v, 800.0f, 2.0f);
    CHECK(sh4der_cloudvolume_prepare(&c, &v, 0.0f, 0u, &f) == 0);
    view_default(&v, 800.0f, -0.1f);
    for (c.steps = 4u; c.steps <= 64u; c.steps += 4u) {
        CHECK(sh4der_cloudvolume_prepare(&c, &v, 12.0f, 3u, &f) == 1);
        CHECK(f.u[0] == 0.0f && fabsf(f.u[c.steps] - 1.0f) < 1.0e-4f);
        for (i = 1u; i < c.steps; i++) CHECK(f.u[i + 1u] - f.u[i] > f.u[i] - f.u[i - 1u]);   /* each step longer than the one before */
        CHECK(fabsf((f.u[c.steps] - f.u[c.steps - 1u]) / f.u[1] - SH4DER_CLOUDVOLUME_GROWTH) < 0.05f * SH4DER_CLOUDVOLUME_GROWTH);
    }
    printf("cloudvolume: tables alpha %u %u beer %u %u haze %u\n", f.alpha_tab[0], f.alpha_tab[255], f.beer_tab[0], f.beer_tab[255], f.haze_tab[0]);
    CHECK(f.alpha_tab[0] == 0u && f.alpha_tab[255] > 250u && f.beer_tab[0] == 255u && f.beer_tab[255] < 8u && f.haze_tab[0] == 255u);
    for (i = 1u; i < 256u; i++) CHECK(f.alpha_tab[i] >= f.alpha_tab[i - 1u] && f.beer_tab[i] <= f.beer_tab[i - 1u]);
    for (i = 1u; i < 64u; i++) CHECK(f.phase_tab[i] >= f.phase_tab[i - 1u] && f.haze_tab[i] <= f.haze_tab[i - 1u]);
    CHECK(f.o[0] >= 0.0f && f.o[0] < 128.0f && f.o[2] >= 0.0f && f.o[2] < 128.0f);
    /* a far eye lands in the same tile place as a near one a whole number of tiles away */
    {
        sh4der_cloudvolume_frame_t g;

        c.steps = 16u;
        CHECK(sh4der_cloudvolume_prepare(&c, &v, 0.0f, 0u, &f) == 1);
        v.eye[0] += 8192.0f * 3.0f;
        CHECK(sh4der_cloudvolume_prepare(&c, &v, 0.0f, 0u, &g) == 1 && fabsf(f.o[0] - g.o[0]) < 0.01f);
    }
}

static unsigned long sum(const uint8_t *p) {
    unsigned long s = 0;
    unsigned i;

    for (i = 0; i < W * H; i++) s += p[i];
    return s;
}

static void march_checks(void) {
    sh4der_cloudvolume_t c;
    sh4der_cloudvolume_view_t v;
    sh4der_cloudvolume_frame_t f;
    unsigned i, n, solid = 0;
    unsigned long a16, a48;

    sh4der_cloudvolume_init(&c);
    sh4der_cloudvolume_generate(&maps, 7u);
    sh4der_cloudvolume_bake(&maps, &c, 0u, SH4DER_CLOUDVOLUME_MAP);

    /* above the slab, looking up: nothing, and no sample taken */
    view_default(&v, 3000.0f, 0.9f);
    CHECK(sh4der_cloudvolume_prepare(&c, &v, 0.0f, 0u, &f) == 1);
    memset(alpha, 0x55, sizeof(alpha));
    CHECK(sh4der_cloudvolume_march(&maps, &f, W, H, 1u, 0u, shade, alpha) == W * H);
    CHECK(sum(alpha) == 0u && f.rays == 0u && f.samples == 0u && f.skips == 0u);

    /* bad arguments */
    CHECK(sh4der_cloudvolume_march(NULL, &f, W, H, 1u, 0u, shade, alpha) == 0 && sh4der_cloudvolume_march(&maps, &f, W, H, 3u, 0u, shade, alpha) == 0 &&
          sh4der_cloudvolume_march(&maps, &f, 4u, H, 1u, 0u, shade, alpha) == 0 && sh4der_cloudvolume_march(&maps, &f, W, H, 1u, 0u, NULL, alpha) == 0);

    /* low in a full deck, looking level: solid cloud over most of the picture */
    c.coverage = 0.95f;
    c.detail = 0u;
    sh4der_cloudvolume_bake(&maps, &c, 0u, SH4DER_CLOUDVOLUME_MAP);
    view_default(&v, 120.0f, 0.0f);
    CHECK(sh4der_cloudvolume_prepare(&c, &v, 0.0f, 0u, &f) == 1);
    CHECK(sh4der_cloudvolume_march(&maps, &f, W, H, 1u, 0u, shade, alpha) == W * H);
    for (i = 0; i < W * H; i++) solid += alpha[i] >= 250u;
    printf("cloudvolume: solid %u of %u, rays %u\n", solid, (unsigned)(W * H), (unsigned)f.rays);
    CHECK(solid > W * H / 2u);                                     /* the deck still has lanes and low tops: 68 % here */
    CHECK(f.rays == W * H && f.hits > 0u && f.hits <= f.samples);

    /* the same input: the same bytes */
    CHECK(sh4der_cloudvolume_prepare(&c, &v, 0.0f, 0u, &f) == 1);
    sh4der_cloudvolume_march(&maps, &f, W, H, 1u, 0u, shade2, alpha2);
    CHECK(memcmp(shade, shade2, sizeof(shade)) == 0 && memcmp(alpha, alpha2, sizeof(alpha)) == 0);

    /* over the default deck: 16 steps and 48 steps hide about the same share of the sky (the step length is in the opacity) */
    sh4der_cloudvolume_init(&c);
    sh4der_cloudvolume_bake(&maps, &c, 0u, SH4DER_CLOUDVOLUME_MAP);
    view_default(&v, 1300.0f, -0.25f);
    c.steps = 16u;
    CHECK(sh4der_cloudvolume_prepare(&c, &v, 5.0f, 0u, &f) == 1);
    sh4der_cloudvolume_march(&maps, &f, W, H, 1u, 0u, shade, alpha);
    a16 = sum(alpha);
    c.steps = 48u;
    CHECK(sh4der_cloudvolume_prepare(&c, &v, 5.0f, 0u, &f) == 1);
    sh4der_cloudvolume_march(&maps, &f, W, H, 1u, 0u, shade2, alpha2);
    a48 = sum(alpha2);
    CHECK(a16 > 0u && a48 > 0u);
    CHECK(labs((long)a16 - (long)a48) < (long)(a48 / 8u));
    printf("cloudvolume: alpha sum 16 steps %lu, 48 steps %lu\n", a16, a48);

    /* light samples and a second octave run and count */
    c.steps = 16u; c.light_samples = 2u; c.detail = 2u;
    CHECK(sh4der_cloudvolume_prepare(&c, &v, 5.0f, 0u, &f) == 1);
    sh4der_cloudvolume_march(&maps, &f, W, H, 1u, 0u, shade2, alpha2);
    CHECK(f.samples + f.skips > f.hits * 2u && f.noise_reads > 0u);

    /* a spread marches its share and leaves the rest alone */
    c.light_samples = 0u; c.detail = 1u;
    CHECK(sh4der_cloudvolume_prepare(&c, &v, 5.0f, 0u, &f) == 1);
    memset(alpha2, 0x77, sizeof(alpha2));
    n = sh4der_cloudvolume_march(&maps, &f, W, H, 2u, 0u, shade2, alpha2);
    CHECK(n == W * H / 2u && alpha2[1] == 0x77u && alpha2[W] == 0x77u);
    n = sh4der_cloudvolume_march(&maps, &f, W, H, 4u, 0u, shade2, alpha2);
    CHECK(n == W * H / 4u);
    n = 0;
    for (i = 0; i < 4u; i++) n += sh4der_cloudvolume_march(&maps, &f, W, H, 4u, i, shade2, alpha2);
    CHECK(n == W * H);                                             /* four turns reach every pixel */
}

static void smooth_and_palette(void) {
    uint32_t pal[256];
    unsigned i, s, a;

    memset(shade, 90, sizeof(shade));
    memset(alpha, 255, sizeof(alpha));
    CHECK(sh4der_cloudvolume_smooth(shade, alpha, W, H, shade2, alpha2) == 1);
    for (i = 0; i < W * H; i++) CHECK(shade2[i] == 90u && alpha2[i] == 255u);
    CHECK(sh4der_cloudvolume_smooth(NULL, alpha, W, H, shade2, alpha2) == 0);
    /* one solid pixel among clear ones of another shade: its neighbours take ITS shade, not the clear ones' */
    memset(shade, 10, sizeof(shade));
    memset(alpha, 0, sizeof(alpha));
    shade[20u * W + 20u] = 200u;
    alpha[20u * W + 20u] = 255u;
    sh4der_cloudvolume_smooth(shade, alpha, W, H, shade2, alpha2);
    CHECK(shade2[20u * W + 21u] == 200u && alpha2[20u * W + 21u] > 0u && alpha2[20u * W + 21u] < 64u && alpha2[20u * W + 20u] > 48u);

    sh4der_cloudvolume_palette(pal);
    sh4der_cloudvolume_palette(NULL);
    CHECK((pal[0] >> 24) == 0u && (pal[160] >> 24) == 255u && (pal[255] >> 24) == 255u);
    CHECK(sh4der_cloudvolume_index(123u, 0u, 3u, 5u) == 0u);
    for (s = 0; s < 256u; s += 5u) {
        for (a = 5u; a < 256u; a += 10u) {
            unsigned x;

            for (x = 0; x < 16u; x++) {
                const uint8_t idx = sh4der_cloudvolume_index(s, a, x & 3u, x >> 2);
                const unsigned pa = pal[idx] >> 24;
                float want[3];

                CHECK(idx != 157u && idx != 158u && idx != 159u);
                CHECK(abs((int)pa - (int)a) <= 20);                /* one alpha level is 18 */
                sh4der_cloudvolume_rgb(s, want);
                if (pa) CHECK(fabsf((float)((pal[idx] >> 16) & 255u) / 255.0f - want[0]) < (pa == 255u ? 0.02f : 0.09f));
            }
        }
    }
    CHECK(sh4der_cloudvolume_index(0u, 255u, 0u, 0u) == 160u && sh4der_cloudvolume_index(255u, 255u, 0u, 0u) == 255u);
}

static void path(void) {
    sh4der_cloudvolume_t c;
    sh4der_cloudvolume_view_t v, w;
    float t;

    sh4der_cloudvolume_init(&c);
    memset(&v, 0, sizeof(v));
    for (t = 0.0f; t < 600.0f; t += 0.37f) {
        sh4der_cloudvolume_path(&c, t, 150.0f, 1, &v);
        CHECK(sh4der_finite_(v.eye[0]) && sh4der_finite_(v.eye[1]) && sh4der_finite_(v.eye[2]) && sh4der_finite_(v.yaw) && sh4der_finite_(v.pitch) && sh4der_finite_(v.roll));
        CHECK(fabsf(v.roll) <= 0.106f);                            /* about 6 degrees */
        CHECK(v.eye[1] > c.base_y + 0.4f * c.thickness && v.eye[1] < c.base_y + 1.0f * c.thickness);
        CHECK(fabsf(v.pitch) < 0.2f);
    }
    sh4der_cloudvolume_path(&c, 10.0f, 150.0f, 0, &w);
    CHECK(w.roll == 0.0f && fabsf(w.eye[2] - 1500.0f) < 0.01f);
    sh4der_cloudvolume_path(NULL, 1.0f, 1.0f, 1, &w);              /* no crash */
    sh4der_cloudvolume_path(&c, 1.0f, 1.0f, 1, NULL);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    defaults_and_valid();
    map_checks();
    bake_checks();
    frame_checks();
    march_checks();
    smooth_and_palette();
    path();
    if (fails) { fprintf(stderr, "cloudvolume_test: %d FAILED\n", fails); return 1; }
    puts("cloudvolume: PASS defaults, validity, maps (range, tiling, seed, coarse map), bake (own top, rows, sun height), frame (steps, tables, refusals), march (clear sky, solid, repeatable, step count, light samples, spread, counters), smooth, palette, path");
    return 0;
}
