/* SH4DER-PVR: route AB spike, host test of the model

   demo/psm/spike/test/ab_model_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   Host check of ../ab_model.h (run by ../build.py, not by the Makefile). It does not test the PVR: it tests that the
   check the console program runs is sound. A software stand-in draws what the program sends (the strip corners of
   ab_rect_corner, the ramp of ab_ramp_fill, in double precision, sampled at the pixel centre, nearest depth wins) and
   the model must agree with it; then known faults are put in and the counts must say so:

     - the stand-in's picture against the model: nothing wrong, for both ramps and all three tests
     - the ramps: each texel is its own index; the 16-bit ramp's texel splits back into u and v
     - FLAT: every value sits mid-step; TILT: the code falls as the depth rises, by whole steps a pixel;
       OVER: the near quad's code in the overlap, whichever was sent first
     - faults: one pixel off by one, a low-bits fault, a high-bits fault, a lost pixel, a picture moved half a
       pixel (the best-offset search finds it), a bad input
*/

#include <math.h>
#include <stdio.h>
#include "../ab_model.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint16_t ramp[AB_RAMP_W * AB_RAMP16_H], img[AB_SIZE * AB_SIZE];
static float zbuf[AB_SIZE * AB_SIZE];

/* What the program sends, drawn the way the idea says the PVR would: two triangles a strip, the depth k and the
   products u k, v k linear over the screen, u and v by the division at the pixel, the texel point sampled, u clamped
   and v wrapped, the nearest depth kept. (sx, sy): where in the pixel the sample is taken. */
static void stand_in(const ab_test_t *t, int bits, double sx, double sy) {
    const unsigned h = ab_ramp_fill(bits, ramp);
    unsigned r, tri, x, y;

    memset(img, 0, sizeof img);
    memset(zbuf, 0, sizeof zbuf);
    for (r = 0; r < t->nrect; r++) {
        float p[4][3], uv[4][2];

        for (x = 0; x < 4u; x++) ab_rect_corner(&t->rect[r], x, bits, p[x], uv[x]);
        for (tri = 0; tri < 2u; tri++) {
            const unsigned a = tri ? 2u : 0u, b = 1u, c = tri ? 3u : 2u;
            const double den = (double)(p[b][0] - p[a][0]) * (p[c][1] - p[a][1]) - (double)(p[c][0] - p[a][0]) * (p[b][1] - p[a][1]);

            for (y = 0; y < AB_SIZE; y++) {
                for (x = 0; x < AB_SIZE; x++) {
                    const double cx = x + 0.5, cy = y + 0.5, px = x + sx, py = y + sy;
                    /* coverage by the pixel centre; the values at the sample point */
                    const double c1 = ((cx - p[a][0]) * (p[c][1] - p[a][1]) - (double)(p[c][0] - p[a][0]) * (cy - p[a][1])) / den;
                    const double c2 = ((double)(p[b][0] - p[a][0]) * (cy - p[a][1]) - (cx - p[a][0]) * (p[b][1] - p[a][1])) / den;
                    const double w1 = ((px - p[a][0]) * (p[c][1] - p[a][1]) - (double)(p[c][0] - p[a][0]) * (py - p[a][1])) / den;
                    const double w2 = ((double)(p[b][0] - p[a][0]) * (py - p[a][1]) - (px - p[a][0]) * (p[b][1] - p[a][1])) / den;
                    const double w0 = 1.0 - w1 - w2;
                    double k, u, v;
                    long ti, tj;

                    if (c1 < 0.0 || c2 < 0.0 || c1 + c2 > 1.0) continue;
                    k = w0 * p[a][2] + w1 * p[b][2] + w2 * p[c][2];
                    if (k <= zbuf[y * AB_SIZE + x]) continue;
                    u = (w0 * uv[a][0] * p[a][2] + w1 * uv[b][0] * p[b][2] + w2 * uv[c][0] * p[c][2]) / k;
                    v = (w0 * uv[a][1] * p[a][2] + w1 * uv[b][1] * p[b][2] + w2 * uv[c][1] * p[c][2]) / k;
                    ti = (long)floor(u * AB_RAMP_W);
                    if (ti < 0) ti = 0;
                    if (ti > (long)AB_RAMP_W - 1) ti = (long)AB_RAMP_W - 1;
                    tj = (long)floor((v - floor(v)) * h);
                    if (tj > (long)h - 1) tj = (long)h - 1;
                    zbuf[y * AB_SIZE + x] = (float)k;
                    img[y * AB_SIZE + x] = ramp[(unsigned)tj * AB_RAMP_W + (unsigned)ti];
                }
            }
        }
    }
}

static void ramps(void) {
    unsigned i, j;

    CHECK(ab_ramp_fill(10, ramp) == AB_RAMP10_H);
    for (j = 0; j < AB_RAMP10_H; j++) for (i = 0; i < AB_RAMP_W; i++) if (ramp[j * AB_RAMP_W + i] != i) { CHECK(ramp[j * AB_RAMP_W + i] == i); return; }
    CHECK(ab_ramp_fill(16, ramp) == AB_RAMP16_H);
    for (j = 0; j < AB_RAMP16_H; j++) {
        for (i = 0; i < AB_RAMP_W; i++) {
            const unsigned c = ramp[j * AB_RAMP_W + i];

            if ((c >> 6) != i || (c & 63u) != j) { CHECK((c >> 6) == i && (c & 63u) == j); return; }
        }
    }
    CHECK(ramp[AB_RAMP16_H * AB_RAMP_W - 1u] == 65535u);          /* every 16-bit code is in the ramp once */
}

static void agree(void) {
    static const int bits[2] = { 10, 16 };
    unsigned b, w;

    for (b = 0; b < 2u; b++) {
        for (w = 0; w < AB_TESTS; w++) {
            ab_test_t t;
            ab_stats_t st;
            float ox = 9.0f, oy = 9.0f;

            CHECK(ab_test_make(&t, w, bits[b]));
            stand_in(&t, bits[b], 0.5, 0.5);
            ab_check(&t, bits[b], img, AB_SIZE, 0.0f, 0.0f, &st);
            printf("  %2d bits %s: %u pixels, %u differ from the stand-in (%u hard), largest %u\n", bits[b], t.name, st.pixels, st.bad, st.hard, st.max_err);
            CHECK(st.pixels == AB_SIZE * AB_SIZE && st.hard == 0u && st.cover == 0u && st.max_err <= 1u);
            if (w != AB_TEST_TILT) CHECK(st.bad == 0u);          /* mid-step values: exact. The tilt's values fall anywhere: a step may round */
            CHECK(ab_best_offset(&t, bits[b], img, AB_SIZE, &ox, &oy) == st.bad && ox == 0.0f && oy == 0.0f);
        }
    }
}

static void shapes(void) {
    ab_test_t t;
    unsigned x, y, prev;
    float margin;

    /* FLAT: mid-step everywhere, every code different, inside the ramp */
    CHECK(ab_test_make(&t, AB_TEST_FLAT, 10));
    for (y = 8; y < 120u; y++) {
        for (x = 8; x < 120u; x++) {
            const unsigned c = ab_predict(&t, 10, x, y, 0.0f, 0.0f, &margin);

            if (c != 100u + 2u * (x - 8u) + 4u * (y - 8u) || margin != 0.5f) { CHECK(c == 100u + 2u * (x - 8u) + 4u * (y - 8u) && margin == 0.5f); return; }
        }
    }
    CHECK(ab_predict(&t, 10, 7u, 60u, 0.0f, 0.0f, NULL) == 0u && ab_predict(&t, 10, 120u, 60u, 0.0f, 0.0f, NULL) == 0u);   /* outside: background */
    CHECK(ab_predict(&t, 10, 60u, 7u, 0.0f, 0.0f, NULL) == 0u && ab_predict(&t, 10, 60u, 120u, 0.0f, 0.0f, NULL) == 0u);
    CHECK(ab_predict(&t, 10, 20u, 20u, 0.5f, 0.0f, NULL) == ab_predict(&t, 10, 20u, 20u, 0.0f, 0.0f, NULL) + 1u);        /* half a pixel is a whole code */
    CHECK(ab_test_make(&t, AB_TEST_FLAT, 16));
    CHECK(ab_predict(&t, 16, 8u, 8u, 0.0f, 0.0f, &margin) == 4096u && margin == 0.5f);
    CHECK(ab_predict(&t, 16, 119u, 119u, 0.0f, 0.0f, &margin) == 4096u + 38u * 111u + 302u * 111u && margin == 0.5f);

    /* TILT: the code is a / k, so it falls from left (far, k 0.5) to right (near, k 1), the same down every column */
    CHECK(ab_test_make(&t, AB_TEST_TILT, 16));
    prev = 65536u;
    for (x = 8; x < 120u; x++) {
        const unsigned c = ab_predict(&t, 16, x, 8u, 0.0f, 0.0f, NULL);

        if (!(c < prev) || c != ab_predict(&t, 16, x, 119u, 0.0f, 0.0f, NULL)) { CHECK(c < prev && c == ab_predict(&t, 16, x, 119u, 0.0f, 0.0f, NULL)); return; }
        prev = c;
    }
    CHECK(ab_predict(&t, 16, 8u, 8u, 0.0f, 0.0f, NULL) > 39000u && ab_predict(&t, 16, 119u, 8u, 0.0f, 0.0f, NULL) < 19800u);
    printf("  16 bits TILT: codes %u .. %u across 112 pixels (about %u a pixel at the far edge)\n", ab_predict(&t, 16, 119u, 8u, 0.0f, 0.0f, NULL),
           ab_predict(&t, 16, 8u, 8u, 0.0f, 0.0f, NULL), ab_predict(&t, 16, 8u, 8u, 0.0f, 0.0f, NULL) - ab_predict(&t, 16, 9u, 8u, 0.0f, 0.0f, NULL));

    /* OVER: the near quad in the overlap, on both halves (the two send orders) */
    CHECK(ab_test_make(&t, AB_TEST_OVER, 10));
    CHECK(ab_predict(&t, 10, 20u, 20u, 0.0f, 0.0f, NULL) == 700u && ab_predict(&t, 10, 100u, 20u, 0.0f, 0.0f, NULL) == 300u);
    CHECK(ab_predict(&t, 10, 60u, 20u, 0.0f, 0.0f, NULL) == 700u && ab_predict(&t, 10, 60u, 100u, 0.0f, 0.0f, NULL) == 700u);
    CHECK(t.rect[0].k0 > t.rect[1].k0 && t.rect[2].k0 < t.rect[3].k0);       /* near first above, far first below */
    CHECK(ab_test_make(&t, AB_TEST_OVER, 16));
    CHECK(ab_predict(&t, 16, 60u, 20u, 0.0f, 0.0f, NULL) == 45227u && ab_predict(&t, 16, 100u, 100u, 0.0f, 0.0f, NULL) == 19285u);
}

static void faults(void) {
    ab_test_t t;
    ab_stats_t st;
    float ox = 0.0f, oy = 0.0f;

    CHECK(ab_test_make(&t, AB_TEST_FLAT, 16));
    stand_in(&t, 16, 0.5, 0.5);
    img[20u * AB_SIZE + 30u] += 1u;                               /* one pixel off by one, mid-step: hard */
    ab_check(&t, 16, img, AB_SIZE, 0.0f, 0.0f, &st);
    CHECK(st.bad == 1u && st.hard == 1u && st.max_err == 1u && st.cover == 0u && st.lo_bad == 1u && st.hi_bad == 0u);
    CHECK(st.first_x == 30u && st.first_y == 20u && st.first_got == st.first_want + 1u);
    img[21u * AB_SIZE + 30u] += 64u;                              /* a fault in the high bits alone */
    img[22u * AB_SIZE + 30u] = 0u;                                /* a pixel of the quad lost */
    img[2u * AB_SIZE + 2u] = 77u;                                 /* a pixel of the background drawn */
    ab_check(&t, 16, img, AB_SIZE, 0.0f, 0.0f, &st);
    CHECK(st.bad == 4u && st.hard == 4u && st.cover == 2u && st.max_err == 64u && st.hi_bad == 3u && st.lo_bad == 3u);
    CHECK(st.first_x == 2u && st.first_y == 2u && st.first_got == 77u && st.first_want == 0u);

    stand_in(&t, 16, 1.0, 0.5);                                   /* the sample taken half a pixel to the right */
    ab_check(&t, 16, img, AB_SIZE, 0.0f, 0.0f, &st);
    CHECK(st.bad == 112u * 112u && st.max_err == 19u && st.cover == 0u);
    CHECK(ab_best_offset(&t, 16, img, AB_SIZE, &ox, &oy) == 0u && ox == 0.5f && oy == 0.0f);
    stand_in(&t, 16, 0.5, 0.0);                                   /* half a pixel up */
    CHECK(ab_best_offset(&t, 16, img, AB_SIZE, &ox, &oy) == 0u && ox == 0.0f && oy == -0.5f);

    CHECK(ab_test_make(&t, AB_TEST_TILT, 10));                    /* a rounding step is counted, and is not hard */
    stand_in(&t, 10, 0.5, 0.5);
    {
        unsigned x, found = 0;
        float margin;

        for (x = 8; x < 120u && !found; x++) {
            const unsigned c = ab_predict(&t, 10, x, 50u, 0.0f, 0.0f, &margin);

            if (margin < 0.1f) { img[50u * AB_SIZE + x] = (uint16_t)(c + 1u); found = 1; }
        }
        CHECK(found);
        ab_check(&t, 10, img, AB_SIZE, 0.0f, 0.0f, &st);
        CHECK(st.bad >= 1u && st.hard == 0u && st.max_err == 1u);
    }

    CHECK(!ab_test_make(&t, AB_TESTS, 10) && !ab_test_make(&t, AB_TEST_FLAT, 12) && !ab_test_make(NULL, AB_TEST_FLAT, 10));
}

int main(void) {
    ramps();
    agree();
    shapes();
    faults();
    if (fails) { fprintf(stderr, "ab_model_test: %d failure(s)\n", fails); return 1; }
    printf("ab_model_test: ok\n");
    return 0;
}
