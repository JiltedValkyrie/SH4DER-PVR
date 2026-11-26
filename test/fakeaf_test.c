/* SH4DER-PVR: host test of the fake anisotropic filtering helper (include/sh4der_fakeaf.h). Arithmetic only: no PVR.

   test/fakeaf_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Compile: cc -std=c99 -Wall -Wextra -Werror -DSH4DER_CORE_NO_SH4ZAM -Iinclude -Ish4dow-dc/include
            test/fakeaf_test.c -lm (make test; make test-shz builds the same with SH4ZAM) */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sh4der_fakeaf.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    faf_range_t t[FAF_RANGES_MAX], bad[FAF_RANGES_MAX];
    float edge[FAF_EDGES_MAX], s[FAF_EDGES_MAX], d, v3, d3;
    static uint16_t src[8 * 8], dst[8 * 8];
    unsigned n, ne, k, m, x, y;
    int op, bl;

    /* the default table is a good one; each broken rule is refused */
    n = faf_table_default(t);
    CHECK(n == 4u && faf_table_ok(t, n) && faf_table_ok(t, 1u));
    CHECK(!faf_table_ok(t, 0u) && !faf_table_ok(t, 5u) && !faf_table_ok(NULL, 2u));
    memcpy(bad, t, sizeof bad); bad[0].from = 1.0f; CHECK(!faf_table_ok(bad, n));
    memcpy(bad, t, sizeof bad); bad[2].from = 7.5f; CHECK(!faf_table_ok(bad, n));          /* inside range 1's zone (6 .. 8) */
    memcpy(bad, t, sizeof bad); bad[2].from = 8.0f; CHECK(faf_table_ok(bad, n));           /* at its end: allowed */
    memcpy(bad, t, sizeof bad); bad[1].ramp = 0.0f; CHECK(!faf_table_ok(bad, n));
    memcpy(bad, t, sizeof bad); bad[3].bias = 0u; CHECK(!faf_table_ok(bad, n));
    memcpy(bad, t, sizeof bad); bad[3].bias = 16u; CHECK(!faf_table_ok(bad, n));
    memcpy(bad, t, sizeof bad); bad[1].shift = 4u; CHECK(!faf_table_ok(bad, n));

    /* the table from a floor's numbers: starts a square root of two apart, where a pixel covers 2, 4, 8 texels along the view */
    {
        faf_range_t f[FAF_RANGES_MAX];
        const float focal = 400.0f, eye = 1.6f, tpm = 128.0f;

        CHECK(faf_table_from(f, 4u, focal, eye, tpm, 0.5f) == 4u && faf_table_ok(f, 4u));
        for (k = 1; k < 4u; k++) {
            const float along = tpm * f[k].from * f[k].from / (focal * eye);

            CHECK(fabsf(along - (float)(1u << k)) < 0.01f * (float)(1u << k));
            CHECK(f[k].shift == k && f[k].bias == FAF_BIAS_NONE && f[k].ramp > 0.0f);
        }
        CHECK(fabsf(f[2].from / f[1].from - 1.41421356f) < 1e-4f && fabsf(f[1].from - 3.1623f) < 1e-3f);
        CHECK(fabsf(f[1].from + f[1].ramp - (f[1].from + 0.5f * (f[2].from - f[1].from))) < 1e-4f);
        CHECK(faf_table_from(f, 4u, focal, eye, tpm, 1.0f) == 4u && faf_table_ok(f, 4u));      /* zones that just meet are allowed */
        CHECK(faf_table_from(f, 4u, focal, eye, tpm, 0.0f) == 4u && faf_table_ok(f, 4u));      /* a share of 0 is held to a narrow zone */
        CHECK(faf_table_from(f, 1u, focal, eye, tpm, 0.5f) == 1u && faf_table_ok(f, 1u));
        CHECK(!faf_table_from(f, 5u, focal, eye, tpm, 0.5f) && !faf_table_from(f, 4u, 0.0f, eye, tpm, 0.5f) && !faf_table_from(NULL, 4u, focal, eye, tpm, 0.5f));
        printf("table from focal 400, eye 1.6 m, 128 texels a metre: starts %.2f %.2f %.2f m\n", (double)f[1].from, (double)f[2].from, (double)f[3].from);
    }

    /* the shares add to 1 at every distance; a range is alone outside the zones; the ramp is linear */
    for (d = 0.0f; d < 60.0f; d += 0.37f) {
        float sum = 0.0f;

        for (k = 0; k < n; k++) { const float w = faf_weight(t, n, k, d); CHECK(w >= 0.0f && w <= 1.0f); sum += w; }
        CHECK(fabsf(sum - 1.0f) < 1e-5f);
    }
    CHECK(faf_weight(t, n, 0, 3.0f) == 1.0f && faf_weight(t, n, 1, 3.0f) == 0.0f);
    CHECK(fabsf(faf_weight(t, n, 0, 7.0f) - 0.5f) < 1e-5f && fabsf(faf_weight(t, n, 1, 7.0f) - 0.5f) < 1e-5f);
    CHECK(faf_weight(t, n, 1, 10.0f) == 1.0f && faf_weight(t, n, 3, 100.0f) == 1.0f && faf_weight(t, n, 2, 100.0f) == 0.0f);
    CHECK(fabsf(faf_weight(t, n, 2, 15.0f) - 0.25f) < 1e-5f);

    /* 3V's alpha: 255 at a zone's start, 0 at its end, half way 128; outside held */
    CHECK(faf_alpha(&t[1], 6.0f) == 255u && faf_alpha(&t[1], 8.0f) == 0u && faf_alpha(&t[1], 7.0f) == 128u);
    CHECK(faf_alpha(&t[1], 2.0f) == 255u && faf_alpha(&t[1], 50.0f) == 0u);
    for (d = 6.0f, m = 255u; d <= 8.0f; d += 0.05f) { CHECK(faf_alpha(&t[1], d) <= m); m = faf_alpha(&t[1], d); }

    /* the edges, rising; a segment's cuts going away and coming back */
    ne = faf_edges(t, n, edge);
    CHECK(ne == 6u && edge[0] == 6.0f && edge[1] == 8.0f && edge[2] == 14.0f && edge[3] == 18.0f && edge[4] == 30.0f && edge[5] == 38.0f);
    for (k = 1; k < ne; k++) CHECK(edge[k] > edge[k - 1u]);
    CHECK(faf_edges(t, 1u, edge) == 0u);
    ne = faf_edges(t, n, edge);
    m = faf_cuts(edge, ne, 4.0f, 16.0f, s);
    CHECK(m == 3u && fabsf(s[0] - 2.0f / 12.0f) < 1e-5f && fabsf(s[1] - 4.0f / 12.0f) < 1e-5f && fabsf(s[2] - 10.0f / 12.0f) < 1e-5f);
    m = faf_cuts(edge, ne, 16.0f, 4.0f, s);                     /* the same segment the other way: the same points, in the order met */
    CHECK(m == 3u && fabsf(s[0] - 2.0f / 12.0f) < 1e-5f && fabsf(s[1] - 8.0f / 12.0f) < 1e-5f && fabsf(s[2] - 10.0f / 12.0f) < 1e-5f);
    CHECK(faf_cuts(edge, ne, 9.0f, 13.0f, s) == 0u && faf_cuts(edge, ne, 7.0f, 7.0f, s) == 0u);
    CHECK(faf_cuts(edge, ne, 6.0f, 8.0f, s) == 0u);             /* ends on two edges: no cut inside */
    CHECK(faf_cuts(edge, ne, 0.0f, 100.0f, s) == 6u);

    /* what draws a piece */
    faf_piece(t, n, 3.0f, &op, &bl); CHECK(op == 0 && bl == -1);
    faf_piece(t, n, 7.0f, &op, &bl); CHECK(op == 1 && bl == 0);
    faf_piece(t, n, 10.0f, &op, &bl); CHECK(op == 1 && bl == -1);
    faf_piece(t, n, 16.0f, &op, &bl); CHECK(op == 2 && bl == 1);
    faf_piece(t, n, 34.0f, &op, &bl); CHECK(op == 3 && bl == 2);
    faf_piece(t, n, 90.0f, &op, &bl); CHECK(op == 3 && bl == -1);
    faf_piece(t, 1u, 90.0f, &op, &bl); CHECK(op == 0 && bl == -1);

    /* the bias: none facing, sharper as it grazes, never past the limits, strength 0 is none */
    CHECK(faf_bias_code(1.0f, 1.0f, 1u) == FAF_BIAS_NONE && faf_bias_code(0.5f, 0.0f, 1u) == FAF_BIAS_NONE);
    CHECK(faf_bias_code(0.25f, 1.0f, 1u) == 2u && faf_bias_code(0.0f, 1.0f, 1u) == 1u && faf_bias_code(0.0f, 1.0f, 2u) == 2u);
    CHECK(faf_bias_code(-3.0f, 1.0f, 1u) == 1u && faf_bias_code(7.0f, 1.0f, 1u) == FAF_BIAS_NONE && faf_bias_code(0.0f, 1.0f, 0u) == 1u && faf_bias_code(0.0f, 1.0f, 9u) == FAF_BIAS_NONE);
    for (d = 0.0f, m = 1u; d <= 1.0f; d += 0.02f) { CHECK(faf_bias_code(d, 1.0f, 1u) >= m); m = faf_bias_code(d, 1.0f, 1u); }
    CHECK(faf_bias_code(0.25f, 0.5f, 1u) == 3u);

    /* a stretched copy: flat stays flat; rows that alternate come out as their mean; nothing moves along a row */
    for (k = 0; k < 64u; k++) src[k] = 0x7bef;
    CHECK(faf_stretch(src, 8u, 8u, 1u, dst) == 4u);
    for (k = 0; k < 32u; k++) CHECK(dst[k] == 0x7bef);
    for (y = 0; y < 8u; y++) for (x = 0; x < 8u; x++) src[y * 8u + x] = (uint16_t)((y & 1u) ? ((x & 31u) << 11) | (60u << 5) | 30u : ((x & 31u) << 11) | (20u << 5) | 10u);
    CHECK(faf_stretch(src, 8u, 8u, 1u, dst) == 4u);
    for (y = 0; y < 4u; y++) for (x = 0; x < 8u; x++) CHECK(dst[y * 8u + x] == (uint16_t)((x << 11) | (40u << 5) | 20u));
    CHECK(faf_stretch(src, 8u, 8u, 3u, dst) == 1u);
    for (x = 0; x < 8u; x++) CHECK(dst[x] == (uint16_t)((x << 11) | (40u << 5) | 20u));
    CHECK(faf_stretch(src, 8u, 8u, 0u, dst) == 8u && !memcmp(src, dst, sizeof src));
    CHECK(faf_stretch(src, 8u, 8u, 4u, dst) == 0u && faf_stretch(NULL, 8u, 8u, 1u, dst) == 0u && faf_stretch(src, 8u, 4u, 3u, dst) == 0u);
    CHECK(faf_stretch_bytes(256u, 256u, 3u) == 114688u);        /* 112 KB: the number in the plan */

    /* the 3D helpers: the ramp's coordinate and its 16 steps */
    CHECK(faf_ramp_u(&t[1], 6.0f) == 0.0f && fabsf(faf_ramp_u(&t[1], 8.0f) - 1.0f) < 1e-6f && fabsf(faf_ramp_u(&t[2], 15.0f) - 0.25f) < 1e-6f);
    CHECK(faf_ramp_texel(0u, 16u) == 0x0000 && faf_ramp_texel(15u, 16u) == 0xf000 && faf_ramp_texel(255u, 256u) == 0xf000 && faf_ramp_texel(128u, 256u) == 0x7000);
    for (k = 1; k < 256u; k++) CHECK(faf_ramp_texel(k, 256u) >= faf_ramp_texel(k - 1u, 256u));

    /* the fill model: a corridor 3 m wide and 3 m high from its middle, focal length 400 pixels */
    CHECK(faf_beyond(400.0f, 1.5f, 1.5f, 0.5f) == 1.0f && faf_beyond(400.0f, 1.5f, 1.5f, 1e6f) < 1e-6f);
    for (d = 1.0f; d < 50.0f; d += 1.0f) CHECK(faf_beyond(400.0f, 1.5f, 1.5f, d + 1.0f) <= faf_beyond(400.0f, 1.5f, 1.5f, d));
    v3 = faf_fill_3v(t, n, 400.0f, 1.5f, 1.5f);
    d3 = faf_fill_3d(t, n, 400.0f, 1.5f, 1.5f);
    CHECK(v3 > 0.0f && v3 < d3);
    printf("fill model, corridor 3 x 3 m, focal 400: 3V adds %.3f of a screen (translucent), 3D %.3f\n", (double)v3, (double)d3);
    CHECK(v3 < 0.15f && d3 < 0.6f);                             /* the model's own numbers for the default table, pinned loosely */

    if (fails) printf("fakeaf_test: %d FAILED\n", fails); else printf("fakeaf_test: ok\n");
    return fails ? 1 : 0;
}
