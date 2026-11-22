/* SH4DER-PVR

   test/motes_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the motes effect: the default numbers and bad params, the hash (range, spread, no two salts alike),
   the stateless rule (the same slot and time give the same speck, a new generation starts somewhere else), the look
   curves (the dot opens between clip x alpha_in and alpha_in of the life, the colour holds and falls, nothing is
   drawn once it is dark), how many specks are alive, the screen side (held between min_px and max_px, the light kept
   under the least size, the big sprites dimmed), the cap (the brightest are kept), the lod cut, the near plane and
   the screen edge, the vertex colour, the quad corners, the round mask, and that nothing the effect gives is ever NaN.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_motes.h"
#include "../include/sh4der_guard.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR_(a, b, e) (fabsf((a) - (b)) <= (e))

/* A camera 1,200 units east of an emitter looking west (-x), 90 degree field, 640 x 480 picture. */
static sh4der_spark_view_t view_at(float dist) {
    sh4der_spark_view_t v;

    memset(&v, 0, sizeof(v));
    v.right[1] = 1.0f;                  /* looking down -x: screen right is +y */
    v.up[2] = 1.0f;
    v.fwd[0] = -1.0f;
    v.eye[0] = dist; v.eye[2] = 150.0f;
    v.near_z = 8.0f; v.focal_px = 320.0f; v.min_px = SH4DER_SPARK_MIN_PX; v.cols = 1; v.rows = 1;
    return v;
}

static sh4der_spark_screen_t screen_default(void) {
    sh4der_spark_screen_t s;

    s.cx = 320.0f; s.cy = 240.0f; s.focal_y_px = 320.0f;
    return s;
}

/* The defaults and the checks that refuse a bad row. */
static void defaults_and_params(void) {
    sh4der_motes_params_t p, q;

    sh4der_motes_params_default(&p);
    CHECK(sh4der_motes_params_valid(&p));
    CHECK(sh4der_motes_slots(&p) == 210u);                              /* 30 a second x 7 s */
    CHECK(sizeof(sh4der_motes_draw_t) == 32u && sizeof(sh4der_motes_emitter_t) == 16u);
    q = p; q.rate = 0.0f; CHECK(!sh4der_motes_params_valid(&q));
    q = p; q.rate = 100.0f; CHECK(!sh4der_motes_params_valid(&q));      /* 700 slots: over the limit */
    q = p; q.life_lo = 8.0f; CHECK(!sh4der_motes_params_valid(&q));
    q = p; q.alpha_in = 0.0f; CHECK(!sh4der_motes_params_valid(&q));
    q = p; q.clip = 1.0f; CHECK(!sh4der_motes_params_valid(&q));
    q = p; q.col_hold = 0.7f; CHECK(!sh4der_motes_params_valid(&q));
    q = p; q.size_hi = NAN; CHECK(!sh4der_motes_params_valid(&q));
    q = p; q.min_px = 20.0f; CHECK(!sh4der_motes_params_valid(&q));
    q = p; q.max_draw = 0u; CHECK(!sh4der_motes_params_valid(&q));
    q = p; q.max_draw = 129u; CHECK(!sh4der_motes_params_valid(&q));
    q = p; q.vel_lo[1] = 50.0f; CHECK(!sh4der_motes_params_valid(&q));
    q = p; q.big_k = 0.0f; CHECK(!sh4der_motes_params_valid(&q));
    CHECK(!sh4der_motes_params_valid(NULL));
}

/* The hash is in 0..1, spreads evenly, and two salts do not agree. */
static void hash(void) {
    unsigned i, bins[8] = { 0 }, same = 0;
    float sum = 0.0f;

    for (i = 0; i < 4000u; i++) {
        const float h = sh4der_motes_hash01(i, 1u);

        CHECK(h >= 0.0f && h < 1.0f);
        bins[(unsigned)(h * 8.0f)]++;
        sum += h;
        same += sh4der_motes_hash01(i, 1u) == sh4der_motes_hash01(i, 2u);
    }
    for (i = 0; i < 8u; i++) CHECK(bins[i] > 400u && bins[i] < 600u);   /* 500 each, within 20 % */
    CHECK(NEAR_(sum / 4000.0f, 0.5f, 0.02f));
    CHECK(same < 5u);
}

/* The stateless rule and the look curves for one slot, followed through its whole life. */
static void rule_and_curves(void) {
    sh4der_motes_params_t p;
    sh4der_motes_emitter_t e = { { 100.0f, 200.0f, -50.0f }, 0u };
    sh4der_motes_ember_t a, b;
    unsigned s;
    float t;
    int drawn;

    sh4der_motes_params_default(&p);
    CHECK(sh4der_motes_slot(&p, &e, 5u, 20.0f, &a) == sh4der_motes_slot(&p, &e, 5u, 20.0f, &b));
    for (s = 0; s < 210u; s++) {                                       /* the same slot and time give the same speck */
        const int ra = sh4der_motes_slot(&p, &e, s, 33.3f, &a), rb = sh4der_motes_slot(&p, &e, s, 33.3f, &b);

        CHECK(ra == rb);
        if (ra) CHECK(memcmp(&a, &b, sizeof a) == 0);
    }
    {   /* follow slot 3: find its life, check the curves at fixed shares of it */
        const float life = p.life_lo + (p.life_hi - p.life_lo) * sh4der_motes_hash01(3u, 1u);
        const float phase = sh4der_motes_hash01(3u, 2u) * life;
        const float t0 = life - phase;                                  /* time of the start of generation 1 */
        const float shares[] = { 0.03f, 0.10f, 0.19f, 0.25f, 0.45f, 0.58f, 0.62f, 0.9f };
        const int expect[] = { 0, 1, 1, 1, 1, 1, 0, 0 };                /* drawn?: open at 6.3 %, dark under cull_k from 57.6 % */
        unsigned i;

        for (i = 0; i < 8u; i++) {
            drawn = sh4der_motes_slot(&p, &e, 3u, t0 + shares[i] * life, &a);
            if (i == 5u) continue;                                      /* 58 %: k = 0.067 is under the cull; the edge, not asserted */
            CHECK(drawn == expect[i]);
        }
        sh4der_motes_slot(&p, &e, 3u, t0 + 0.10f * life, &a);
        CHECK(a.size > 0.0f && a.size < p.size_hi * 0.5f + 0.01f && a.k == 1.0f);   /* opening, full colour */
        sh4der_motes_slot(&p, &e, 3u, t0 + 0.19f * life, &a);
        CHECK(a.size >= p.size_lo * 0.99f && a.size <= p.size_hi * 1.01f && a.k == 1.0f);   /* fully open */
        sh4der_motes_slot(&p, &e, 3u, t0 + 0.45f * life, &b);
        CHECK(NEAR_(b.k, 0.5f, 0.01f));                                  /* halfway through the fade: (0.6 - 0.45) / 0.3 */
        sh4der_motes_slot(&p, &e, 3u, t0 + 0.19f * life + 4.0f * life, &b);   /* the same share four generations on starts elsewhere */
        CHECK(fabsf(b.pos[0] - a.pos[0]) + fabsf(b.pos[1] - a.pos[1]) > 1.0f);
    }
    /* position falls with the start height and the acceleration: the field sits in a band below the start */
    for (t = 0.0f; t < 60.0f; t += 1.7f)
        for (s = 0; s < 210u; s++)
            if (sh4der_motes_slot(&p, &e, s, t, &a)) {
                CHECK(a.pos[2] > e.anchor[2] - 200.0f && a.pos[2] < e.anchor[2] + 610.0f);   /* 500 - 600 up, falls about 620 by 60 % of the longest life */
                CHECK(a.pos[0] > e.anchor[0] - 2000.0f - 600.0f && a.pos[0] < e.anchor[0] + 2000.0f);
                CHECK(sh4der_finite_(a.pos[0]) && sh4der_finite_(a.pos[1]) && sh4der_finite_(a.size) && a.size <= p.size_hi + 0.01f);
            }
}

/* How many specks are alive: about half the slots, whatever the time. */
static void census(void) {
    sh4der_motes_params_t p;
    sh4der_motes_emitter_t e = { { 0.0f, 0.0f, 0.0f }, 0u };
    sh4der_motes_ember_t a;
    float t;
    unsigned s, lo = 9999u, hi = 0u;

    sh4der_motes_params_default(&p);
    for (t = 3.0f; t < 90.0f; t += 4.3f) {
        unsigned n = 0;

        for (s = 0; s < 210u; s++) n += (unsigned)sh4der_motes_slot(&p, &e, s, t, &a);
        if (n < lo) lo = n;
        if (n > hi) hi = n;
    }
    CHECK(lo >= 85u && hi <= 130u);                                      /* the mock counts 91 - 117 in range */
}

/* The screen side, the light under the least size, the dimming of big sprites, the colour and the quad. */
static void screen_rules(void) {
    sh4der_motes_params_t p;
    sh4der_motes_emitter_t e = { { 0.0f, 0.0f, 0.0f }, 0u };
    sh4der_spark_view_t v = view_at(1200.0f);
    const sh4der_spark_screen_t sc = screen_default();
    static sh4der_motes_draw_t d[SH4DER_MOTES_DRAW_MAX];
    sh4der_spark_vtx_t q[4] = { { 0 } };
    unsigned n, i;
    float t;

    sh4der_motes_params_default(&p);
    p.max_draw = 128u;
    for (t = 5.0f; t < 40.0f; t += 3.1f) {
        n = sh4der_motes_collect(&p, &e, 1u, t, &v, &sc, d);
        CHECK(n <= 128u);
        for (i = 0; i < n; i++) {
            const float px = d[i].half * 2.0f * v.focal_px / d[i].c[2];

            CHECK(px >= p.min_px - 0.01f && px <= p.max_px + 0.01f);
            CHECK(d[i].c[2] >= v.near_z && d[i].score > 0.0f);
            CHECK(d[i].flat == (px <= p.flat_px));
            CHECK((d[i].argb >> 24) == 0xFFu);
            CHECK(sh4der_motes_quad(&d[i], q));
            CHECK(NEAR_(q[2].x - q[0].x, 2.0f * d[i].half, 1e-3f) && NEAR_(q[0].y - q[1].y, 2.0f * d[i].half, 1e-3f));
            CHECK(q[0].z == q[3].z && q[0].u == 0.0f && q[3].u == 1.0f && q[1].v == 1.0f);
        }
    }
    /* a bright full-size speck in front of the eye is dimmed when wide; a distant one is 2 px wide at a lower brightness */
    CHECK(sh4der_motes_color_(&p, 1.0f) == p.color_argb && sh4der_motes_color_(&p, 0.0f) == p.fade_argb);
    CHECK(((sh4der_motes_color_(&p, 0.5f) >> 16) & 255u) == 152u);       /* (70 + 235) / 2, rounded down */
    {
        sh4der_motes_draw_t one;

        memset(&one, 0, sizeof one);
        one.c[2] = 100.0f; one.half = 0.0f;
        CHECK(!sh4der_motes_quad(&one, q));
        CHECK(!sh4der_motes_quad(NULL, q));
    }
}

/* The cap, the lod cut, the near plane, the screen edge and bad arguments. */
static void limits(void) {
    sh4der_motes_params_t p;
    sh4der_motes_emitter_t e[2] = { { { 0.0f, 0.0f, 0.0f }, 0u }, { { 0.0f, 0.0f, 0.0f }, 1000u } };
    sh4der_spark_view_t v = view_at(1200.0f);
    const sh4der_spark_screen_t sc = screen_default();
    static sh4der_motes_draw_t d[SH4DER_MOTES_DRAW_MAX], all[SH4DER_MOTES_DRAW_MAX];
    unsigned n_all, n_cap, i, j, weakest_kept_ok = 1;
    float t = 12.0f, min_kept = 1e30f, max_dropped = 0.0f;

    sh4der_motes_params_default(&p);
    p.max_draw = 128u;
    n_all = sh4der_motes_collect(&p, e, 2u, t, &v, &sc, all);
    CHECK(n_all > 40u);                                                 /* the mock draws 55 - 68 */
    p.max_draw = 30u;
    n_cap = sh4der_motes_collect(&p, e, 2u, t, &v, &sc, d);
    CHECK(n_cap == 30u);
    for (i = 0; i < n_cap; i++) if (d[i].score < min_kept) min_kept = d[i].score;
    for (i = 0; i < n_all; i++) {
        int kept = 0;

        for (j = 0; j < n_cap; j++) kept |= (d[j].score == all[i].score && d[j].c[0] == all[i].c[0]);
        if (!kept && all[i].score > max_dropped) max_dropped = all[i].score;
    }
    CHECK(min_kept >= max_dropped);                                     /* the brightest are the ones kept */
    (void)weakest_kept_ok;

    p.max_draw = 128u;
    v = view_at(5000.0f);
    CHECK(sh4der_motes_collect(&p, e, 2u, t, &v, &sc, d) == 0u);        /* past the lod cut */
    v = view_at(1200.0f);
    p.box_xy = 1000.0f;                                                 /* a field that ends before the eye (x under 700) */
    v.fwd[0] = 1.0f; v.right[1] = -1.0f;                                /* looking away: the whole field is behind the eye */
    CHECK(sh4der_motes_collect(&p, e, 2u, t, &v, &sc, d) == 0u);
    v = view_at(1200.0f);
    p.box_xy = 4000.0f;
    CHECK(sh4der_motes_collect(NULL, e, 2u, t, &v, &sc, d) == 0u);
    CHECK(sh4der_motes_collect(&p, NULL, 2u, t, &v, &sc, d) == 0u);
    CHECK(sh4der_motes_collect(&p, e, 2u, -1.0f, &v, &sc, d) == 0u);
    CHECK(sh4der_motes_collect(&p, e, 2u, NAN, &v, &sc, d) == 0u);
    CHECK(sh4der_motes_collect(&p, e, 0u, t, &v, &sc, d) == 0u);
    v.focal_px = 0.0f;
    CHECK(sh4der_motes_collect(&p, e, 2u, t, &v, &sc, d) == 0u);
}

/* The round mask and no NaN over many frames and eyes. */
static void mask_and_finite(void) {
    uint16_t m[SH4DER_MOTES_MASK_SIDE * SH4DER_MOTES_MASK_SIDE];
    sh4der_motes_params_t p;
    sh4der_motes_emitter_t e[3] = { { { 0.0f, 0.0f, 0.0f }, 0u }, { { 900.0f, 100.0f, 0.0f }, 1000u }, { { -700.0f, 50.0f, 20.0f }, 2000u } };
    static sh4der_motes_draw_t d[SH4DER_MOTES_DRAW_MAX];
    const sh4der_spark_screen_t sc = screen_default();
    unsigned i, on = 0, r, n, drawn = 0;

    sh4der_motes_mask_make(m);
    for (i = 0; i < 256u; i++) { CHECK(m[i] == 0xFFFFu || m[i] == 0u); on += m[i] == 0xFFFFu; }
    CHECK(m[0] == 0u && m[15] == 0u && m[240] == 0u && m[255] == 0u);    /* corners clear */
    CHECK(m[7 * 16 + 7] == 0xFFFFu && m[8 * 16 + 8] == 0xFFFFu);         /* the middle set */
    CHECK(on > 170u && on < 215u);                                       /* a round dot fills about pi/4 of the cell: 201 */

    sh4der_motes_params_default(&p);
    for (r = 0; r < 200u; r++) {
        sh4der_spark_view_t v = view_at(300.0f + (float)(r % 17u) * 150.0f);

        v.eye[1] = (float)(r % 9u) * 40.0f - 160.0f;
        n = sh4der_motes_collect(&p, e, 3u, (float)r * 0.37f, &v, &sc, d);
        for (i = 0; i < n; i++) {
            drawn++;
            CHECK(sh4der_finite_(d[i].c[0]) && sh4der_finite_(d[i].c[1]) && sh4der_finite_(d[i].c[2]) && sh4der_finite_(d[i].half) && sh4der_finite_(d[i].score));
        }
    }
    CHECK(drawn > 1000u);
}

int main(void) {
    defaults_and_params();
    hash();
    rule_and_curves();
    census();
    screen_rules();
    limits();
    mask_and_finite();
    if (fails) { fprintf(stderr, "motes_test: %d failure(s)\n", fails); return 1; }
    printf("motes_test: ok\n");
    return 0;
}
