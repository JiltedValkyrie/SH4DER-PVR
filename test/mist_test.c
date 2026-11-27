/* SH4DER-PVR: mist bank test

   test/mist_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of the mist bank: the default numbers; the per-slice alpha (n slices of it together stop the asked share
   of light, within the header's stated error; edge cases); a slice's six vertices (where it stands in the bank, its
   width, its base, feather line and top, alpha full on the two lower rows and 0 on the top row, the strip's four
   triangles have area); slices are spaced evenly through the depth; a slice behind the eye is not drawn and one the
   eye is close to fades; the texture slides with time, differently on each slice, and the same time gives the same
   answer; the bank can be turned (a direction other than +z); the feather is held within the bank's height; bad
   arguments write nothing; hostile input (every float field NaN / infinite / out of range, an enormous time or eye) draws nothing and writes
   nothing, and the true struct size is printed; the noise is filled, uses most of its range, changes with the seed and tiles on both edges.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/sh4der_mist.h"
#include "../include/sh4der_guard.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.00002f
#endif

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR_(a, b, e) (fabsf((a) - (b)) <= (e))

static void defaults_and_alpha(void) {
    sh4der_mist_t m;
    unsigned n;

    memset(&m, 0xff, sizeof(m));
    sh4der_mist_init(&m);
    sh4der_mist_init(NULL);                                       /* no crash */
    CHECK(SH4DER_MIST_MAX_SLICES == 16 && SH4DER_MIST_SLICES == 12 && SH4DER_MIST_SLICE_VERTS == 6);
    CHECK(m.slices == 12u && m.dir_x == 0.0f && m.dir_z == 1.0f && m.half_width == 3.0f && m.base_y == 0.0f && m.top_y == 3.0f);
    CHECK(NEAR_(m.opacity, 0.7f, 0.0f) && NEAR_(m.feather, 2.0f, 0.0f) && NEAR_(m.near_fade, 3.0f, 0.0f) && m.depth == 12.0f);

    CHECK(sh4der_mist_slice_alpha(0.7f, 0u) == 0.0f && sh4der_mist_slice_alpha(0.0f, 12u) == 0.0f);
    CHECK(NEAR_(sh4der_mist_slice_alpha(0.6f, 1u), 0.6f, SH4DER_TEST_TOL));   /* one slice: the opacity itself */
    for (n = 4u; n <= 16u; n++) {                                  /* n slices together stop the asked share */
        static const float want[4] = { 0.2f, 0.5f, 0.7f, 0.9f };
        unsigned k;

        for (k = 0; k < 4u; k++) {
            const float a = sh4der_mist_slice_alpha(want[k], n);
            const float exact = 1.0f - powf(1.0f - want[k], 1.0f / (float)n);

            CHECK(a > 0.0f && a < want[k]);
            CHECK(NEAR_(a, exact, 0.01f));                         /* the header's stated error */
        }
    }
    CHECK(sh4der_mist_slice_alpha(0.7f, 99u) == sh4der_mist_slice_alpha(0.7f, 16u));   /* held at the most slices */
    CHECK(sh4der_mist_slice_alpha(5.0f, 8u) <= 1.0f);              /* an opacity over 1 is taken as 1 */
}

static void slices(void) {
    sh4der_mist_t m;
    sh4der_mist_vtx_t v[6], w[6];
    const float eye[3] = { 0.0f, 1.5f, -20.0f };                   /* well in front of the bank, looking along +z */
    const float alpha = sh4der_mist_slice_alpha(SH4DER_MIST_OPACITY, 12u);
    unsigned i, k;

    sh4der_mist_init(&m);
    m.z = 5.0f;
    CHECK(sh4der_mist_slice(&m, 0u, 0.0f, eye, v) == 6u);
    CHECK(NEAR_(v[0].x, -3.0f, SH4DER_TEST_TOL) && NEAR_(v[1].x, 3.0f, SH4DER_TEST_TOL));   /* left then right, 6 m apart */
    for (k = 0; k < 6u; k++) CHECK(NEAR_(v[k].z, 5.0f, SH4DER_TEST_TOL));                    /* the first slice is the near face */
    CHECK(v[0].y == 0.0f && v[1].y == 0.0f && NEAR_(v[2].y, 1.0f, SH4DER_TEST_TOL) && NEAR_(v[3].y, 1.0f, SH4DER_TEST_TOL) &&
          v[4].y == 3.0f && v[5].y == 3.0f);                       /* base, feather line 2 m under the top, top */
    CHECK(NEAR_(v[0].alpha, alpha, SH4DER_TEST_TOL) && NEAR_(v[3].alpha, alpha, SH4DER_TEST_TOL) && v[4].alpha == 0.0f && v[5].alpha == 0.0f);
    CHECK(NEAR_(v[1].u - v[0].u, 6.0f / SH4DER_MIST_TILE, SH4DER_TEST_TOL));                 /* the texture repeats every `tile` metres */
    CHECK(v[0].v > v[2].v && v[2].v > v[4].v);                     /* v runs down the texture as the slice goes up */
    for (k = 0; k < 4u; k++) {                                     /* the strip's four triangles, seen face on from the eye */
        float p[3][3];
        unsigned j;

        for (j = 0; j < 3u; j++) {
            p[j][0] = v[k + j].x - eye[0];
            p[j][1] = v[k + j].y - eye[1];
            p[j][2] = v[k + j].z - eye[2];
        }
        CHECK(sh4der_tri_area_ok(p[0], p[1], p[2], 0.1f, SH4DER_GUARD_MIN_AREA2));
    }
    for (i = 0; i < 12u; i++) {                                    /* evenly through 12 m: the last on the far face */
        CHECK(sh4der_mist_slice(&m, i, 0.0f, eye, w) == 6u);
        CHECK(NEAR_(w[0].z, 5.0f + 12.0f * (float)i / 11.0f, 0.0001f));
    }
    CHECK(sh4der_mist_slice(&m, 12u, 0.0f, eye, w) == 0);          /* no such slice */

    {   /* the eye inside the bank: slices behind it are skipped, the one just ahead is faint, far ones are full */
        const float inside[3] = { 0.0f, 1.5f, 9.0f };              /* 4 m into the bank */
        unsigned drawn = 0;
        float first_alpha = -1.0f;

        for (i = 0; i < 12u; i++) {
            if (sh4der_mist_slice(&m, i, 0.0f, inside, w) == 6u) {
                CHECK(w[0].z > inside[2]);
                if (first_alpha < 0.0f) first_alpha = w[0].alpha;
                drawn++;
            }
        }
        CHECK(drawn > 0u && drawn < 12u);
        CHECK(first_alpha > 0.0f && first_alpha < alpha);          /* the nearest drawn slice is fading */
        CHECK(sh4der_mist_slice(&m, 11u, 0.0f, inside, w) == 6u && NEAR_(w[0].alpha, alpha, SH4DER_TEST_TOL));
    }

    /* the texture moves with time, each slice its own way, and the same time gives the same answer */
    CHECK(sh4der_mist_slice(&m, 0u, 10.0f, eye, w) == 6u);
    CHECK(!NEAR_(w[0].u, v[0].u, 0.001f) || !NEAR_(w[0].v, v[0].v, 0.001f));
    CHECK(NEAR_(w[0].x, v[0].x, 0.0f) && NEAR_(w[0].y, v[0].y, 0.0f));                       /* the sheet itself does not move */
    CHECK(sh4der_mist_slice(&m, 0u, 10.0f, eye, v) == 6u && memcmp(v, w, sizeof(v)) == 0);
    CHECK(sh4der_mist_slice(&m, 1u, 10.0f, eye, v) == 6u && !NEAR_(v[0].u, w[0].u, 0.0001f));
    m.seed = 77u;
    CHECK(sh4der_mist_slice(&m, 0u, 10.0f, eye, v) == 6u && !NEAR_(v[0].u, w[0].u, 0.0001f)); /* another seed, another movement */

    {   /* a bank turned to run along +x: its slices face down x and spread along z */
        const float west[3] = { -20.0f, 1.5f, 0.0f };

        sh4der_mist_init(&m);
        m.dir_x = 1.0f;
        m.dir_z = 0.0f;
        CHECK(sh4der_mist_slice(&m, 11u, 0.0f, west, v) == 6u);
        CHECK(NEAR_(v[0].x, 12.0f, 0.0001f) && NEAR_(v[1].x, 12.0f, 0.0001f));
        CHECK(NEAR_(fabsf(v[0].z - v[1].z), 6.0f, 0.0001f));
    }

    /* a feather taller than the bank is the whole height: the feather line sits on the base */
    sh4der_mist_init(&m);
    m.feather = 50.0f;
    CHECK(sh4der_mist_slice(&m, 0u, 0.0f, eye, v) == 6u && NEAR_(v[2].y, 0.0f, SH4DER_TEST_TOL));
    m.feather = -1.0f;                                             /* none: the feather line is the top (a hard top) */
    CHECK(sh4der_mist_slice(&m, 0u, 0.0f, eye, v) == 6u && NEAR_(v[2].y, 3.0f, SH4DER_TEST_TOL));
}

static void bad(void) {
    sh4der_mist_t m;
    sh4der_mist_vtx_t v[6];
    const float eye[3] = { 0.0f, 1.5f, -20.0f };
    const float nan_eye[3] = { NAN, 1.5f, -20.0f };

    memset(v, 0x55, sizeof(v));
    sh4der_mist_init(&m);
    CHECK(sh4der_mist_slice(NULL, 0u, 0.0f, eye, v) == 0 && sh4der_mist_slice(&m, 0u, 0.0f, NULL, v) == 0 && sh4der_mist_slice(&m, 0u, 0.0f, eye, NULL) == 0);
    CHECK(sh4der_mist_slice(&m, 0u, NAN, eye, v) == 0 && sh4der_mist_slice(&m, 0u, 0.0f, nan_eye, v) == 0);
    m.slices = 0u;
    CHECK(sh4der_mist_slice(&m, 0u, 0.0f, eye, v) == 0);
    m.slices = 17u;
    CHECK(sh4der_mist_slice(&m, 0u, 0.0f, eye, v) == 0);
    sh4der_mist_init(&m);
    m.half_width = 0.0f;
    CHECK(sh4der_mist_slice(&m, 0u, 0.0f, eye, v) == 0);
    sh4der_mist_init(&m);
    m.top_y = m.base_y;
    CHECK(sh4der_mist_slice(&m, 0u, 0.0f, eye, v) == 0);
    sh4der_mist_init(&m);
    m.opacity = 0.0f;                                              /* a clear bank: nothing to draw */
    CHECK(sh4der_mist_slice(&m, 0u, 0.0f, eye, v) == 0);
    {
        unsigned i;

        for (i = 0; i < sizeof(v); i++) CHECK(((const uint8_t *)v)[i] == 0x55);   /* a refused call writes nothing */
    }
}

/* A float built from raw bits, so a NaN or an infinity exists whatever -ffast-math does to arithmetic. */
static float from_bits(uint32_t bits) {
    union {
        uint32_t u;
        float f;
    } v;

    v.u = bits;
    return v.f;
}

#define BITS_INF  0x7f800000u    /* +infinity */
#define BITS_NINF 0xff800000u    /* -infinity */
#define BITS_NAN  0x7fc00000u    /* a quiet NaN */

/* Hostile blocks and times: an invalid block draws nothing and writes nothing; no output is ever not finite. */
static void hostile(void) {
    sh4der_mist_t m;
    sh4der_mist_vtx_t v[6];
    sh4der_mist_vtx_t keep[6];
    const float eye[3] = { 0.0f, 1.5f, -20.0f };
    const float inf = from_bits(BITS_INF);
    const float ninf = from_bits(BITS_NINF);
    const float nan = from_bits(BITS_NAN);
    const float bad[3] = { inf, ninf, nan };
    unsigned f;
    unsigned k;
    unsigned n;

    printf("mist: sizeof(sh4der_mist_t) = %u\n", (unsigned)sizeof(sh4der_mist_t));
    CHECK(sizeof(sh4der_mist_t) == 68u);
    memset(v, 0x55, sizeof(v));
    memcpy(keep, v, sizeof(v));
    sh4der_mist_init(&m);
    CHECK(sh4der_mist_valid(&m) == 1 && sh4der_mist_valid(NULL) == 0);
    CHECK(sh4der_mist_slice(&m, 0u, 0.0f, eye, v) == 6u);
    memcpy(v, keep, sizeof(v));

    /* every float field, set in turn to infinity, minus infinity and NaN: invalid, nothing drawn, nothing written */
    for (f = 0; f < 15u; f++) {
        for (k = 0; k < 3u; k++) {
            float *field[15];

            sh4der_mist_init(&m);
            field[0] = &m.x;
            field[1] = &m.z;
            field[2] = &m.dir_x;
            field[3] = &m.dir_z;
            field[4] = &m.half_width;
            field[5] = &m.base_y;
            field[6] = &m.top_y;
            field[7] = &m.feather;
            field[8] = &m.depth;
            field[9] = &m.opacity;
            field[10] = &m.near_fade;
            field[11] = &m.drift;
            field[12] = &m.seethe;
            field[13] = &m.seethe_rate;
            field[14] = &m.tile;
            *field[f] = bad[k];
            CHECK(sh4der_mist_valid(&m) == 0);
            CHECK(sh4der_mist_slice(&m, 0u, 0.0f, eye, v) == 0);
        }
    }
    CHECK(memcmp(v, keep, sizeof(v)) == 0);

    /* finite but out of range */
    sh4der_mist_init(&m);
    m.half_width = 1.0e30f;
    CHECK(sh4der_mist_valid(&m) == 0 && sh4der_mist_slice(&m, 0u, 0.0f, eye, v) == 0);
    sh4der_mist_init(&m);
    m.tile = 1.0e-30f;                                             /* u would be infinite */
    CHECK(sh4der_mist_valid(&m) == 0 && sh4der_mist_slice(&m, 0u, 0.0f, eye, v) == 0);
    sh4der_mist_init(&m);
    m.tile = 0.0f;
    CHECK(sh4der_mist_valid(&m) == 0);
    sh4der_mist_init(&m);
    m.dir_x = 0.0f;
    m.dir_z = 0.0f;                                                /* no direction */
    CHECK(sh4der_mist_valid(&m) == 0);
    sh4der_mist_init(&m);
    m.opacity = 2.0f;
    CHECK(sh4der_mist_valid(&m) == 0);
    sh4der_mist_init(&m);
    m.depth = -1.0f;
    CHECK(sh4der_mist_valid(&m) == 0);
    sh4der_mist_init(&m);
    m.x = 1.0e30f;
    CHECK(sh4der_mist_valid(&m) == 0);
    sh4der_mist_init(&m);
    m.slices = 0u;
    CHECK(sh4der_mist_valid(&m) == 0);
    sh4der_mist_init(&m);
    m.slices = SH4DER_MIST_MAX_SLICES + 1u;
    CHECK(sh4der_mist_valid(&m) == 0);
    CHECK(memcmp(v, keep, sizeof(v)) == 0);                        /* none of that wrote */

    /* a non-finite or enormous time, and a non-finite eye */
    sh4der_mist_init(&m);
    CHECK(sh4der_mist_slice(&m, 0u, inf, eye, v) == 0 && sh4der_mist_slice(&m, 0u, nan, eye, v) == 0);
    m.drift = 1.0e3f;
    CHECK(sh4der_mist_slice(&m, 0u, 1.0e38f, eye, v) == 0);        /* time x drift overflows: refused */
    sh4der_mist_init(&m);
    CHECK(sh4der_mist_slice(&m, 0u, 1.0e30f, eye, v) == 6u);       /* big but finite: every output still finite */
    for (k = 0; k < 6u; k++) {
        CHECK(sh4der_finite_(v[k].x) && sh4der_finite_(v[k].y) && sh4der_finite_(v[k].z));
        CHECK(sh4der_finite_(v[k].u) && sh4der_finite_(v[k].v) && sh4der_finite_(v[k].alpha));
    }
    {
        const float inf_eye[3] = { 0.0f, 1.5f, inf };
        const float huge_eye[3] = { 3.0e38f, 1.5f, -3.0e38f };

        memcpy(v, keep, sizeof(v));
        CHECK(sh4der_mist_slice(&m, 0u, 0.0f, inf_eye, v) == 0);
        n = sh4der_mist_slice(&m, 0u, 0.0f, huge_eye, v);
        CHECK(n == 0u || (sh4der_finite_(v[0].x) && sh4der_finite_(v[0].u)));
    }
    CHECK(sh4der_mist_slice_alpha(nan, 12u) == 0.0f && sh4der_mist_slice_alpha(inf, 12u) <= 1.0f);
}

static void noise(void) {
    static uint8_t a[64 * 64], b[64 * 64];
    unsigned i, lo = 255u, hi = 0u, same = 0u;
    long edge_x = 0, inner_x = 0, edge_y = 0, inner_y = 0;

    CHECK(sh4der_mist_noise(NULL, 64u, 64u, 1u) == 0 && sh4der_mist_noise(a, 2u, 64u, 1u) == 0 && sh4der_mist_noise(a, 64u, 2000u, 1u) == 0);
    CHECK(sh4der_mist_noise(a, 64u, 64u, 1u) == 1 && sh4der_mist_noise(b, 64u, 64u, 2u) == 1);
    for (i = 0; i < 64u * 64u; i++) {
        lo = a[i] < lo ? a[i] : lo;
        hi = a[i] > hi ? a[i] : hi;
        same += a[i] == b[i];
    }
    CHECK(hi - lo > 100u);                                         /* it uses a good part of the range */
    CHECK(same < 64u * 64u / 4u);                                  /* another seed is another picture */
    for (i = 0; i < 64u; i++) {                                    /* the wrap-round step is no bigger than a step inside */
        edge_x += labs((long)a[i * 64u + 63u] - (long)a[i * 64u]);
        inner_x += labs((long)a[i * 64u + 31u] - (long)a[i * 64u + 32u]);
        edge_y += labs((long)a[63u * 64u + i] - (long)a[i]);
        inner_y += labs((long)a[31u * 64u + i] - (long)a[32u * 64u + i]);
    }
    CHECK(edge_x <= 3 * inner_x + 64 && edge_y <= 3 * inner_y + 64);
    CHECK(sh4der_mist_noise(b, 64u, 64u, 1u) == 1 && memcmp(a, b, sizeof(a)) == 0);   /* the same seed is the same picture */
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    defaults_and_alpha();
    slices();
    bad();
    hostile();
    noise();
    if (fails) { fprintf(stderr, "mist_test: %d FAILED\n", fails); return 1; }
    puts("mist: PASS defaults, slice alpha, slice vertices (place, rows, alpha, areas), spacing, eye inside, drift, turned bank, feather limits, bad arguments, hostile input, noise (range, seed, tiling)");
    return 0;
}
