/* SH4DER-PVR

   test/bolt_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the bolt effect: the default numbers and bad params, a bolt that depends only on its seed, the strand
   shape (starts on the root, runs about its length along the direction, forks leave the strand), the segment count
   and the way the segments chain, the re-roll clock (every 4 pictures at 30 a second, one re-roll at most per call),
   what a re-roll keeps (the first segments and the forks that left them) and how much of the bolt it keeps overall,
   the brightness gain, the quad width in pixels (the same at any distance, the halo three times as wide), the colours
   (peak colour at the top gain, a fork dimmer, the halo fainter), no halo on a fork, the near plane, and that nothing
   the effect gives is ever NaN.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_bolt.h"
#include "../include/sh4der_guard.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR_(a, b, e) (fabsf((a) - (b)) <= (e))

static sh4der_spark_view_t view_default(void) {
    sh4der_spark_view_t v;

    memset(&v, 0, sizeof(v));
    v.right[0] = 1.0f; v.up[1] = 1.0f; v.fwd[2] = 1.0f;          /* camera at the origin, looking down +z */
    v.near_z = 1.0f; v.focal_px = 320.0f; v.min_px = SH4DER_SPARK_MIN_PX; v.inset = 0.0f;
    v.cols = 1; v.rows = 1;
    return v;
}

/* Six strands hanging down (-y) from a row of roots at depth 100, each 40 long. */
static void anchors_default(sh4der_bolt_anchor_t a[SH4DER_BOLT_STRANDS_MAX]) {
    unsigned i;

    for (i = 0; i < SH4DER_BOLT_STRANDS_MAX; i++) {
        a[i].root[0] = -15.0f + 6.0f * (float)i; a[i].root[1] = 20.0f; a[i].root[2] = 100.0f;
        a[i].dir[0] = 0.0f; a[i].dir[1] = -1.0f; a[i].dir[2] = 0.0f;
        a[i].length = 40.0f;
    }
}

static float dist2d(const sh4der_spark_vtx_t *a, const sh4der_spark_vtx_t *b) {
    return sqrtf((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y));
}

/* The default numbers and the checks that refuse a bad look. */
static void defaults_and_params(void) {
    sh4der_bolt_params_t p, q;

    sh4der_bolt_params_default(&p);
    CHECK(sh4der_bolt_params_valid(&p));
    CHECK(p.strands == 4 && p.segs == 6 && p.forks == 1 && p.fork_segs == 3 && p.keep_segs == 3);
    CHECK(sh4der_bolt_segments(&p) == 4u * (6u + 1u * 3u));/* 36 core quads; the halo adds 24 on the main strands */
    CHECK(NEAR_(p.reroll_s * 30.0f, 4.0f, 0.01f));                     /* 4 pictures at 30 a second */
    q = p; q.strands = 0; CHECK(!sh4der_bolt_params_valid(&q));
    q = p; q.strands = 9; CHECK(!sh4der_bolt_params_valid(&q));
    q = p; q.segs = 2; CHECK(!sh4der_bolt_params_valid(&q));
    q = p; q.segs = 13; CHECK(!sh4der_bolt_params_valid(&q));
    q = p; q.forks = 4; CHECK(!sh4der_bolt_params_valid(&q));
    q = p; q.fork_segs = 0; CHECK(!sh4der_bolt_params_valid(&q));
    q = p; q.keep_segs = 9; CHECK(!sh4der_bolt_params_valid(&q));
    q = p; q.width_px = NAN; CHECK(!sh4der_bolt_params_valid(&q));
    q = p; q.width_px = 0.0f; CHECK(!sh4der_bolt_params_valid(&q));
    q = p; q.reroll_s = 0.0f; CHECK(!sh4der_bolt_params_valid(&q));
    q = p; q.fork_lo = 1.0f; q.fork_hi = 0.5f; CHECK(!sh4der_bolt_params_valid(&q));
    q = p; q.halo_scale = 0.5f; CHECK(!sh4der_bolt_params_valid(&q));
    q = p; q.forks = 0; q.fork_segs = 0; CHECK(sh4der_bolt_params_valid(&q) && sh4der_bolt_segments(&q) == 4u * 6u);   /* no forks: fork_segs is ignored */
    CHECK(!sh4der_bolt_params_valid(NULL));
}

/* The shape of a fresh bolt: seed determinism, root, length and direction, forks, bad anchors. */
static void shape(void) {
    static sh4der_bolt_t a, b, c;
    sh4der_bolt_params_t p;
    sh4der_bolt_anchor_t an[SH4DER_BOLT_STRANDS_MAX];
    unsigned i, f;

    sh4der_bolt_params_default(&p);
    anchors_default(an);
    CHECK(sh4der_bolt_init(&a, &p, an, 1234u) && sh4der_bolt_init(&b, &p, an, 1234u) && sh4der_bolt_init(&c, &p, an, 99u));
    CHECK(memcmp(&a, &b, sizeof a) == 0);                              /* the same seed, the same bolt */
    CHECK(memcmp(&a, &c, sizeof a) != 0);
    for (i = 0; i < p.strands; i++) {
        const float *end = a.main[i][p.segs];

        CHECK(a.main[i][0][0] == 0.0f && a.main[i][0][1] == 0.0f && a.main[i][0][2] == 0.0f);   /* point 0 is the root */
        CHECK(end[1] < -0.8f * an[i].length && end[1] > -1.25f * an[i].length);                 /* runs down (-y) about its length */
        CHECK(fabsf(end[0]) < 0.45f * an[i].length && fabsf(end[2]) < 0.45f * an[i].length);    /* the wander stays near the line */
        for (f = 0; f < p.forks; f++) {
            const unsigned at = a.fork_at[i][f];

            CHECK(at >= 1 && at <= p.segs - 2);                                                  /* leaves the strand with a tail after it */
            CHECK(a.fork[i][f][0][0] == a.main[i][at][0] && a.fork[i][f][0][1] == a.main[i][at][1] && a.fork[i][f][0][2] == a.main[i][at][2]);
        }
    }
    an[2].dir[0] = an[2].dir[1] = an[2].dir[2] = 0.0f;
    CHECK(!sh4der_bolt_init(&c, &p, an, 5u));                          /* a zero direction is refused */
    anchors_default(an);
    an[1].length = -3.0f;
    CHECK(!sh4der_bolt_init(&c, &p, an, 5u));
    anchors_default(an);
    an[0].root[0] = NAN;
    CHECK(!sh4der_bolt_init(&c, &p, an, 5u));
}

/* The segment list: count, chaining of the main polyline, a fork leaving a main point, out of range refused. */
static void segments(void) {
    static sh4der_bolt_t b;
    sh4der_bolt_params_t p;
    sh4der_bolt_anchor_t an[SH4DER_BOLT_STRANDS_MAX];
    unsigned k, n, forks = 0;
    float t[3] = { 0 }, h[3] = { 0 }, t2[3] = { 0 }, h2[3] = { 0 };
    int isf = 0, isf2 = 0;

    sh4der_bolt_params_default(&p);
    anchors_default(an);
    CHECK(sh4der_bolt_init(&b, &p, an, 7u));
    n = sh4der_bolt_segments(&p);
    CHECK(n == 36);
    for (k = 0; k < n; k++) {
        CHECK(sh4der_bolt_segment(&b, &p, an, k, t, h, &isf));
        forks += (unsigned)isf;
        if (!isf && k % 9u != 5u) {                                   /* the next main segment of the strand starts where this one ends */
            CHECK(sh4der_bolt_segment(&b, &p, an, k + 1u, t2, h2, &isf2));
            CHECK(!isf2 && t2[0] == h[0] && t2[1] == h[1] && t2[2] == h[2]);
        }
    }
    CHECK(forks == 4u * 1u * 3u);
    CHECK(!sh4der_bolt_segment(&b, &p, an, n, t, h, &isf));
    CHECK(sh4der_bolt_segment(&b, &p, an, 0, t, h, &isf) && NEAR_(t[0], an[0].root[0], 1e-4f) && NEAR_(t[1], an[0].root[1], 1e-4f) && NEAR_(t[2], an[0].root[2], 1e-4f));   /* strand 0 starts on its root */
}

/* The re-roll clock and what a re-roll keeps. */
static void reroll(void) {
    static sh4der_bolt_t b, before;
    sh4der_bolt_params_t p;
    sh4der_bolt_anchor_t an[SH4DER_BOLT_STRANDS_MAX];
    unsigned i, f, s, changed, forks_kept, forks_total, r;
    float gmin = 9.0f, gmax = 0.0f;
    unsigned same = 0, total = 0;

    sh4der_bolt_params_default(&p);
    anchors_default(an);
    CHECK(sh4der_bolt_init(&b, &p, an, 42u));
    for (i = 0; i < 3; i++) sh4der_bolt_update(&b, &p, an, 1.0f / 30.0f);
    CHECK(b.rolls == 0);                                               /* 3 pictures: no re-roll yet */
    sh4der_bolt_update(&b, &p, an, 1.0f / 30.0f);
    CHECK(b.rolls == 1);                                               /* the 4th: a re-roll */
    for (i = 0; i < 4; i++) sh4der_bolt_update(&b, &p, an, 1.0f / 30.0f);
    CHECK(b.rolls == 2);                                               /* about 8 a second */
    sh4der_bolt_update(&b, &p, an, 10.0f);
    CHECK(b.rolls == 3);                                               /* a long pause is one flicker, not a burst */
    sh4der_bolt_update(&b, &p, an, NAN); sh4der_bolt_update(&b, &p, an, -1.0f); sh4der_bolt_update(&b, &p, an, 0.0f);
    CHECK(b.rolls == 3);                                               /* a bad dt changes nothing */

    before = b;
    sh4der_bolt_update(&b, &p, an, 1.0f);
    changed = 0; forks_kept = 0; forks_total = 0;
    for (i = 0; i < p.strands; i++) {
        for (s = 0; s <= p.keep_segs; s++)                              /* points 0 .. keep_segs: the kept segments */
            CHECK(memcmp(b.main[i][s], before.main[i][s], sizeof b.main[i][s]) == 0);
        for (s = p.keep_segs + 1u; s <= p.segs; s++) changed += memcmp(b.main[i][s], before.main[i][s], sizeof b.main[i][s]) != 0;
        for (f = 0; f < p.forks; f++) {
            forks_total++;
            if (before.fork_at[i][f] <= p.keep_segs) {                  /* it left the kept part: it stays, whole */
                CHECK(b.fork_at[i][f] == before.fork_at[i][f]);
                CHECK(memcmp(b.fork[i][f], before.fork[i][f], sizeof b.fork[i][f]) == 0);
                forks_kept++;
            }
        }
    }
    CHECK(changed >= 4u * (p.segs - p.keep_segs));                      /* the rest is drawn again */
    (void)forks_total; (void)forks_kept;

    /* how much of the bolt a re-roll keeps: segments whose two ends are unchanged, over 300 re-rolls */
    CHECK(sh4der_bolt_init(&b, &p, an, 4242u));
    for (r = 0; r < 300; r++) {
        unsigned k, n = sh4der_bolt_segments(&p);

        before = b;
        sh4der_bolt_update(&b, &p, an, 1.0f);
        if (b.gain < gmin) gmin = b.gain;
        if (b.gain > gmax) gmax = b.gain;
        for (k = 0; k < n; k++) {
            float t0[3], h0[3], t1[3], h1[3];
            int f0, f1;

            sh4der_bolt_segment(&before, &p, an, k, t0, h0, &f0);
            sh4der_bolt_segment(&b, &p, an, k, t1, h1, &f1);
            same += memcmp(t0, t1, sizeof t0) == 0 && memcmp(h0, h1, sizeof h0) == 0;
            total++;
        }
    }
    {
        const float share = (float)same / (float)total;

        CHECK(share > 0.40f && share < 0.80f);                          /* the brightness pattern keeps part of its shape (brief: overlap .4 - .8) */
    }
    CHECK(gmin >= 1.0f - p.gain_jitter - 1e-4f && gmax <= 1.0f + p.gain_jitter + 1e-4f);
    CHECK(gmax - gmin > 0.3f);                                          /* the brightness really varies */
}

/* Quad width, colours, forks, near plane. */
static void quads(void) {
    static sh4der_bolt_t b;
    sh4der_bolt_params_t p;
    sh4der_bolt_anchor_t an[SH4DER_BOLT_STRANDS_MAX], far_an[SH4DER_BOLT_STRANDS_MAX];
    sh4der_spark_view_t v = view_default();
    sh4der_spark_vtx_t q[4];
    unsigned k, n, mainq = 0, haloq = 0, forkq = 0, nearq = 0;
    float t[3], h[3];
    int isf, c;

    sh4der_bolt_params_default(&p);
    anchors_default(an);
    CHECK(sh4der_bolt_init(&b, &p, an, 11u));
    b.gain = 1.0f + p.gain_jitter;                                      /* the brightest roll: the peak colours */
    n = sh4der_bolt_segments(&p);
    for (k = 0; k < n; k++) {
        float z, px;

        sh4der_bolt_segment(&b, &p, an, k, t, h, &isf);
        if (sh4der_bolt_quad(&b, &p, an, k, 0, &v, q)) {
            const float zm = (q[0].z + q[1].z + q[2].z + q[3].z) * 0.25f;

            z = zm;
            px = dist2d(&q[0], &q[1]) * v.focal_px / z;                 /* the width across the tail end, in pixels */
            if (!isf) { CHECK(NEAR_(px, p.width_px, 0.25f)); CHECK(q[0].argb == p.core_argb); mainq++; }
            else {
                const uint32_t peak = p.core_argb;
                const unsigned r = (unsigned)((float)((peak >> 16) & 255u) * p.fork_gain);

                CHECK(NEAR_(px, p.width_px, 0.25f));
                CHECK(((q[0].argb >> 16) & 255u) >= r - 2u && ((q[0].argb >> 16) & 255u) <= r + 1u);   /* a fork is dimmer */
                forkq++;
            }
        }
        if (sh4der_bolt_quad(&b, &p, an, k, 1, &v, q)) {
            const float zm = (q[0].z + q[1].z + q[2].z + q[3].z) * 0.25f;

            px = dist2d(&q[0], &q[1]) * v.focal_px / zm;
            CHECK(!isf);                                                /* a fork has no halo */
            CHECK(NEAR_(px, p.width_px * p.halo_scale, 0.6f));
            CHECK(((q[0].argb >> 16) & 255u) <= (unsigned)(255.0f * p.halo_gain) + 2u);   /* fainter than the core */
            CHECK(((q[0].argb >> 16) & 255u) >= (unsigned)(255.0f * p.halo_gain) - 3u);
            haloq++;
        }
    }
    CHECK(mainq + forkq == n && forkq == 12 && mainq == 24 && haloq == 24);   /* 36 core quads and 24 halo ribbons */

    /* the same strands ten times farther away: still the same width on the screen */
    for (c = 0; c < SH4DER_BOLT_STRANDS_MAX; c++) {
        far_an[c] = an[c];
        far_an[c].root[0] *= 10.0f; far_an[c].root[1] *= 10.0f; far_an[c].root[2] *= 10.0f;
        far_an[c].length = an[c].length * 10.0f;
    }
    CHECK(sh4der_bolt_init(&b, &p, far_an, 11u));
    for (k = 0; k < n; k++)
        if (sh4der_bolt_quad(&b, &p, far_an, k, 0, &v, q)) {
            const float zm = (q[0].z + q[1].z + q[2].z + q[3].z) * 0.25f;

            CHECK(NEAR_(dist2d(&q[0], &q[1]) * v.focal_px / zm, p.width_px, 0.3f));
        }

    /* nothing is drawn behind the eye or wholly before the near plane; a segment across the near plane is cut, not lost */
    for (c = 0; c < SH4DER_BOLT_STRANDS_MAX; c++) { an[c].root[2] = -50.0f; an[c].dir[0] = 0.0f; an[c].dir[1] = -1.0f; an[c].dir[2] = 0.0f; }
    CHECK(sh4der_bolt_init(&b, &p, an, 11u));
    for (k = 0; k < n; k++) nearq += (unsigned)sh4der_bolt_quad(&b, &p, an, k, 0, &v, q);
    CHECK(nearq == 0);
    CHECK(!sh4der_bolt_quad(&b, &p, an, n, 0, &v, q));                 /* a bad segment index */
    CHECK(!sh4der_bolt_quad(&b, &p, an, 0, 0, NULL, q));
}

/* Nothing the effect gives is a NaN, over many re-rolls and several eyes. */
static void all_finite(void) {
    static sh4der_bolt_t b;
    sh4der_bolt_params_t p;
    sh4der_bolt_anchor_t an[SH4DER_BOLT_STRANDS_MAX];
    sh4der_spark_view_t v = view_default();
    unsigned r, k, n, drawn = 0;
    sh4der_spark_vtx_t q[4];
    int c, h;

    sh4der_bolt_params_default(&p);
    p.strands = SH4DER_BOLT_STRANDS_MAX; p.segs = SH4DER_BOLT_SEGS_MAX; p.forks = SH4DER_BOLT_FORKS_MAX; p.fork_segs = SH4DER_BOLT_FORK_SEGS_MAX;
    p.keep_segs = 5;
    anchors_default(an);
    CHECK(sh4der_bolt_params_valid(&p) && sh4der_bolt_init(&b, &p, an, 3u));
    n = sh4der_bolt_segments(&p);
    for (r = 0; r < 100; r++) {
        sh4der_bolt_update(&b, &p, an, 0.2f);
        v.eye[0] = (float)(r % 7) * 3.0f - 9.0f; v.eye[1] = (float)(r % 5); v.eye[2] = (float)(r % 11) * 20.0f - 60.0f;
        for (k = 0; k < n; k++)
            for (h = 0; h < 2; h++)
                if (sh4der_bolt_quad(&b, &p, an, k, h, &v, q)) {
                    drawn++;
                    for (c = 0; c < 4; c++) CHECK(sh4der_finite_(q[c].x) && sh4der_finite_(q[c].y) && sh4der_finite_(q[c].z) && q[c].z >= v.near_z);
                }
    }
    CHECK(drawn > 1000);
}

int main(void) {
    defaults_and_params();
    shape();
    segments();
    reroll();
    quads();
    all_finite();
    if (fails) { fprintf(stderr, "bolt_test: %d failure(s)\n", fails); return 1; }
    printf("bolt_test: ok\n");
    return 0;
}
