/* SH4DER-PVR: rise test

   test/rise_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of the rise effect: the defaults; bad arguments write nothing; every light seated exactly on its
   source before time 0; lift-offs differ and are not in index order; height never falls while rising and ends
   at the sky; size and colour move monotonically; a star's position is fixed while its brightness twinkles
   inside the depth; bit-identical answers for the same (i, t); sh4der_rise_done flips at the last arrival;
   the quad's size, centre and triangle areas; the packed colour; hostile input (every float of the block NaN or
   infinite, a bad source or time, an enormous time, a bad quad size) is refused or stays finite.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_rise.h"
#include "../include/sh4der_guard.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.00002f
#endif

#define N_LIGHTS 12u   /* lights in the test field */

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR_(a, b, e) (fabsf((a) - (b)) <= (e))

/* Source position of test light i: a row low down, a little spread in x, y and z. */
static void source_of(uint32_t i, float s[3]) {
    s[0] = (float)i * 0.5f - 3.0f;
    s[1] = 1.0f + 0.1f * (float)i;
    s[2] = (float)(i % 3u) - 1.0f;
}

/* Defaults and bad arguments (a rejected call must leave the output untouched). */
static void defaults(void) {
    sh4der_rise_params_t p;
    sh4der_rise_light_t o = { 0 };
    const float src[3] = { 0.0f, 1.0f, 0.0f };
    const unsigned char *b = (const unsigned char *)&o;
    size_t k;
    int same = 1;

    sh4der_rise_init(&p);
    CHECK(sh4der_rise_valid(&p));
    CHECK(sizeof(sh4der_rise_params_t) == 84);
    CHECK(p.stagger == 0.4f && p.jitter == 3.0f && p.rise_time == 6.0f && p.sky_y == 60.0f);
    CHECK(p.size0 == 2.0f && p.size1 == 0.5f && p.seed == 0u);
    CHECK(NEAR_(p.col0[0] * 255.0f, 255.0f, 0.01f) && NEAR_(p.col0[1] * 255.0f, 176.0f, 0.01f) && NEAR_(p.col0[2] * 255.0f, 96.0f, 0.01f));
    CHECK(NEAR_(p.col1[0] * 255.0f, 220.0f, 0.01f) && NEAR_(p.col1[1] * 255.0f, 230.0f, 0.01f) && NEAR_(p.col1[2] * 255.0f, 255.0f, 0.01f));

    memset(&o, 0x55, sizeof(o));
    CHECK(!sh4der_rise_light(NULL, 0u, src, 0.0f, &o));
    CHECK(!sh4der_rise_light(&p, 0u, NULL, 0.0f, &o));
    CHECK(!sh4der_rise_light(&p, 0u, src, 0.0f, NULL));
    CHECK(!sh4der_rise_light(&p, 0u, src, 1.0e30f * 1.0e30f, &o));   /* infinity */
    p.rise_time = 0.0f;
    CHECK(!sh4der_rise_light(&p, 0u, src, 0.0f, &o));
    CHECK(!sh4der_rise_done(&p, 4u, 1.0f));
    sh4der_rise_init(&p);
    p.bright1 = 1.5f;
    CHECK(!sh4der_rise_light(&p, 0u, src, 0.0f, &o));
    sh4der_rise_init(&p);
    CHECK(!sh4der_rise_done(&p, 0u, 100.0f) && !sh4der_rise_done(NULL, 4u, 100.0f));
    for (k = 0; k < sizeof(o); k++) {
        if (b[k] != 0x55) {
            same = 0;
        }
    }
    CHECK(same);
}

/* Before time 0 every light is exactly on its source with the start look. */
static void seated(void) {
    sh4der_rise_params_t p;
    uint32_t i;

    sh4der_rise_init(&p);
    for (i = 0; i < N_LIGHTS; i++) {
        float s[3];
        sh4der_rise_light_t o = { 0 };

        source_of(i, s);
        CHECK(sh4der_rise_light(&p, i, s, -1.0f, &o));
        CHECK(o.x == s[0] && o.y == s[1] && o.z == s[2]);
        CHECK(o.phase == SH4DER_RISE_SEATED);
        CHECK(o.size == p.size0 && o.r == p.col0[0] && o.g == p.col0[1] && o.b == p.col0[2] && o.alpha == p.bright0);
    }
}

/* Lift-off times differ, are shuffled by the jitter, and fall back to index order with no jitter. */
static void order(void) {
    sh4der_rise_params_t p;
    uint32_t i;
    int distinct = 0;
    int shuffled = 0;

    sh4der_rise_init(&p);
    for (i = 1; i < N_LIGHTS; i++) {
        if (sh4der_rise_liftoff(&p, i) != sh4der_rise_liftoff(&p, 0u)) {
            distinct++;
        }
        if (sh4der_rise_liftoff(&p, i) < sh4der_rise_liftoff(&p, i - 1u)) {
            shuffled++;
        }
    }
    CHECK(distinct == (int)N_LIGHTS - 1);
    CHECK(shuffled > 0);
    p.jitter = 0.0f;
    for (i = 1; i < N_LIGHTS; i++) {
        CHECK(sh4der_rise_liftoff(&p, i) > sh4der_rise_liftoff(&p, i - 1u));
    }
}

/* Height, size and colour walk monotonically from the source look to the star look. */
static void rising(void) {
    sh4der_rise_params_t p;
    uint32_t i;

    sh4der_rise_init(&p);
    for (i = 0; i < N_LIGHTS; i++) {
        float s[3], t, py, psize, pr, pg, pb, lift;
        sh4der_rise_light_t o = { 0 };
        int saw_rising = 0;

        source_of(i, s);
        lift = sh4der_rise_liftoff(&p, i);
        py = s[1];
        psize = p.size0;
        pr = p.col0[0];
        pg = p.col0[1];
        pb = p.col0[2];
        for (t = 0.0f; t < lift + p.rise_time + 2.0f; t += 0.05f) {
            CHECK(sh4der_rise_light(&p, i, s, t, &o));
            CHECK(o.y >= py - 1.0e-4f);
            CHECK(o.size <= psize + 1.0e-5f);
            CHECK(o.r <= pr + 1.0e-5f && o.g >= pg - 1.0e-5f && o.b >= pb - 1.0e-5f);   /* warm to cool */
            if (o.phase == SH4DER_RISE_RISING) {
                saw_rising = 1;
                CHECK(o.y >= s[1] - 1.0e-4f && o.y <= p.sky_y + p.sky_band + 1.0e-4f);
            }
            py = o.y;
            psize = o.size;
            pr = o.r;
            pg = o.g;
            pb = o.b;
        }
        CHECK(saw_rising);
        CHECK(sh4der_rise_light(&p, i, s, lift + p.rise_time + 5.0f, &o));
        CHECK(o.phase == SH4DER_RISE_STAR);
        CHECK(NEAR_(o.size, p.size1, 1.0e-5f));
        CHECK(NEAR_(o.r, p.col1[0], 1.0e-5f) && NEAR_(o.g, p.col1[1], 1.0e-5f) && NEAR_(o.b, p.col1[2], 1.0e-5f));
        CHECK(o.y >= p.sky_y - 1.0e-3f && o.y <= p.sky_y + p.sky_band + 1.0e-3f);
        CHECK(fabsf(o.x - s[0]) <= p.sky_spread + 1.0e-3f && fabsf(o.z - s[2]) <= p.sky_spread + 1.0e-3f);
    }
}

/* A star never moves; its brightness stays in the twinkle band and really moves; no jump at arrival. */
static void star(void) {
    sh4der_rise_params_t p;
    uint32_t i;
    float s[3], lift;
    sh4der_rise_light_t a = { 0 }, b = { 0 };

    sh4der_rise_init(&p);
    for (i = 0; i < 4u; i++) {
        float t, lo = 2.0f, hi = -1.0f;

        source_of(i, s);
        lift = sh4der_rise_liftoff(&p, i);
        for (t = lift + p.rise_time + 2.0f; t < lift + p.rise_time + 12.0f; t += 0.03f) {
            CHECK(sh4der_rise_light(&p, i, s, t, &a));
            CHECK(sh4der_rise_light(&p, i, s, t + 10.0f, &b));
            CHECK(a.x == b.x && a.y == b.y && a.z == b.z);
            CHECK(a.alpha >= p.bright1 * (1.0f - p.twinkle_depth) - 1.0e-5f && a.alpha <= p.bright1 + 1.0e-5f);
            if (a.alpha < lo) {
                lo = a.alpha;
            }
            if (a.alpha > hi) {
                hi = a.alpha;
            }
        }
        CHECK(hi - lo > 0.5f * p.twinkle_depth * p.bright1);
        CHECK(NEAR_(lo, p.bright1 * (1.0f - p.twinkle_depth), 0.01f));
    }
    source_of(2u, s);
    lift = sh4der_rise_liftoff(&p, 2u);
    CHECK(sh4der_rise_light(&p, 2u, s, lift + p.rise_time - 0.001f, &a));
    CHECK(sh4der_rise_light(&p, 2u, s, lift + p.rise_time + 0.001f, &b));
    CHECK(NEAR_(a.alpha, b.alpha, 0.01f));
    CHECK(NEAR_(a.x, b.x, 0.01f) && NEAR_(a.y, b.y, 0.01f) && NEAR_(a.z, b.z, 0.01f));
}

/* The same (i, t) gives bit-identical records. */
static void determinism(void) {
    sh4der_rise_params_t p;
    uint32_t i;
    float t;

    sh4der_rise_init(&p);
    for (i = 0; i < N_LIGHTS; i++) {
        float s[3];

        source_of(i, s);
        for (t = -0.5f; t < 14.0f; t += 0.37f) {
            sh4der_rise_light_t a = { 0 }, b = { 0 };

            CHECK(sh4der_rise_light(&p, i, s, t, &a));
            CHECK(sh4der_rise_light(&p, i, s, t, &b));
            CHECK(memcmp(&a, &b, sizeof(a)) == 0);
        }
    }
}

/* done is 0 until the last light arrives, then 1 for ever; the last light is a star exactly then. */
static void done(void) {
    sh4der_rise_params_t p;
    uint32_t i;
    uint32_t last = 0u;
    float arrive = 0.0f;
    float t;
    float s[3];
    sh4der_rise_light_t o = { 0 };

    sh4der_rise_init(&p);
    for (i = 0; i < N_LIGHTS; i++) {
        const float a = sh4der_rise_arrival(&p, i);

        if (a > arrive) {
            arrive = a;
            last = i;
        }
    }
    CHECK(!sh4der_rise_done(&p, N_LIGHTS, -1.0f));
    CHECK(!sh4der_rise_done(&p, N_LIGHTS, arrive - 0.01f));
    CHECK(sh4der_rise_done(&p, N_LIGHTS, arrive));
    CHECK(sh4der_rise_done(&p, N_LIGHTS, arrive + 100.0f));
    for (t = 0.0f; t < arrive + 3.0f; t += 0.1f) {
        CHECK(sh4der_rise_done(&p, N_LIGHTS, t) == (t >= arrive));
    }
    source_of(last, s);
    CHECK(sh4der_rise_light(&p, last, s, arrive, &o) && o.phase == SH4DER_RISE_STAR);
    CHECK(sh4der_rise_light(&p, last, s, arrive - 0.01f, &o) && o.phase == SH4DER_RISE_RISING);
}

/* The quad: the light's size, centred, strip order, both triangles have area seen face on, packed colour. */
static void quad(void) {
    const float right[3] = { 1.0f, 0.0f, 0.0f };
    const float up[3] = { 0.0f, 1.0f, 0.0f };
    static const int idx[2][3] = { { 0, 1, 2 }, { 1, 3, 2 } };   /* strip order: triangles 0 1 2 and 1 3 2 */
    sh4der_rise_light_t l = { 0 };
    sh4der_rise_vtx_t q[4] = { 0 };
    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    float tri[2][3][3];
    int k;

    memset(&l, 0, sizeof(l));
    l.x = 3.0f;
    l.y = 4.0f;
    l.z = 20.0f;
    l.size = 0.5f;
    l.r = 1.0f;
    l.g = 0.5f;
    l.alpha = 0.5f;
    CHECK(sh4der_rise_quad(&l, right, up, q));
    CHECK(NEAR_(q[1].x - q[0].x, 0.5f, 1.0e-5f) && NEAR_(q[0].y - q[2].y, 0.5f, 1.0e-5f));
    for (k = 0; k < 4; k++) {
        cx += q[k].x;
        cy += q[k].y;
        cz += q[k].z;
    }
    CHECK(NEAR_(cx * 0.25f, 3.0f, 1.0e-5f) && NEAR_(cy * 0.25f, 4.0f, 1.0e-5f) && NEAR_(cz * 0.25f, 20.0f, 1.0e-5f));
    CHECK(q[0].u == 0.0f && q[0].v == 0.0f && q[1].u == 1.0f && q[1].v == 0.0f);
    CHECK(q[2].u == 0.0f && q[2].v == 1.0f && q[3].u == 1.0f && q[3].v == 1.0f);
    for (k = 0; k < 6; k++) {
        const sh4der_rise_vtx_t *v = &q[idx[k / 3][k % 3]];

        tri[k / 3][k % 3][0] = v->x;
        tri[k / 3][k % 3][1] = v->y;
        tri[k / 3][k % 3][2] = v->z;
    }
    CHECK(sh4der_tri_area_ok(tri[0][0], tri[0][1], tri[0][2], 1.0f, SH4DER_GUARD_MIN_AREA2));
    CHECK(sh4der_tri_area_ok(tri[1][0], tri[1][1], tri[1][2], 1.0f, SH4DER_GUARD_MIN_AREA2));
    memset(q, 0x55, sizeof(q));
    CHECK(!sh4der_rise_quad(NULL, right, up, q));
    l.size = 0.0f;
    CHECK(!sh4der_rise_quad(&l, right, up, q));
    CHECK(((const unsigned char *)q)[0] == 0x55);
    l.size = 0.5f;
    CHECK(sh4der_rise_argb(&l) == 0x80FF8000u);   /* 0.5 -> 128 (rounded), 1 -> 255, 0.5 -> 128, 0 -> 0 */
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

/* Hostile numbers: an invalid block or a non-finite input is refused and writes nothing; a huge finite time still
   gives finite outputs. */
static void hostile(void) {
    sh4der_rise_params_t p;
    sh4der_rise_light_t o = { 0 };
    sh4der_rise_light_t keep = { 0 };
    sh4der_rise_vtx_t q[4] = { 0 };
    const float right[3] = { 1.0f, 0.0f, 0.0f };
    const float up[3] = { 0.0f, 1.0f, 0.0f };
    const float src[3] = { 0.0f, 1.0f, 0.0f };
    const float bad[3] = { from_bits(BITS_INF), from_bits(BITS_NINF), from_bits(BITS_NAN) };
    unsigned f;
    unsigned k;

    CHECK(sizeof(sh4der_rise_params_t) == 21u * sizeof(float));
    printf("rise: sizeof(sh4der_rise_params_t) = %u\n", (unsigned)sizeof(sh4der_rise_params_t));
    memset(&o, 0x55, sizeof(o));
    memcpy(&keep, &o, sizeof(o));

    /* every float of the block in turn: infinity, minus infinity, NaN -> invalid and refused */
    for (f = 0; f < 20u; f++) {
        for (k = 0; k < 3u; k++) {
            float raw[21];

            sh4der_rise_init(&p);
            memcpy(raw, &p, sizeof(raw));                          /* the 20 floats come first, then the seed: view them as an array */
            raw[f] = bad[k];
            memcpy(&p, raw, sizeof(raw));
            CHECK(!sh4der_rise_valid(&p));
            CHECK(!sh4der_rise_light(&p, 0u, src, 1.0f, &o));
            CHECK(!sh4der_rise_done(&p, 4u, 100.0f));
        }
    }
    CHECK(memcmp(&o, &keep, sizeof(o)) == 0);

    /* a source or a time that is not finite */
    sh4der_rise_init(&p);
    for (k = 0; k < 3u; k++) {
        const float s0[3] = { bad[k], 1.0f, 0.0f };
        const float s1[3] = { 0.0f, bad[k], 0.0f };
        const float s2[3] = { 0.0f, 1.0f, bad[k] };

        CHECK(!sh4der_rise_light(&p, 0u, s0, 1.0f, &o) && !sh4der_rise_light(&p, 0u, s1, 1.0f, &o));
        CHECK(!sh4der_rise_light(&p, 0u, s2, 1.0f, &o) && !sh4der_rise_light(&p, 0u, src, bad[k], &o));
        CHECK(!sh4der_rise_done(&p, 4u, bad[k]));
    }
    CHECK(memcmp(&o, &keep, sizeof(o)) == 0);

    /* an enormous finite time: the light is a star and every output is finite */
    CHECK(sh4der_rise_light(&p, 3u, src, 3.0e38f, &o) == 1 && o.phase == SH4DER_RISE_STAR);
    CHECK(sh4der_finite_(o.x) && sh4der_finite_(o.y) && sh4der_finite_(o.z) && sh4der_finite_(o.size) && sh4der_finite_(o.alpha));
    CHECK(sh4der_rise_light(&p, 3u, src, -3.0e38f, &o) == 1 && o.phase == SH4DER_RISE_SEATED);
    CHECK(sh4der_rise_done(&p, 4u, 3.0e38f));

    /* a quad of a light whose size is not finite or not positive is refused */
    memset(q, 0x55, sizeof(q));
    for (k = 0; k < 3u; k++) {
        o.size = bad[k];
        CHECK(!sh4der_rise_quad(&o, right, up, q));
    }
    o.size = 0.0f;
    CHECK(!sh4der_rise_quad(&o, right, up, q));
    o.size = -1.0f;
    CHECK(!sh4der_rise_quad(&o, right, up, q));
    CHECK(((const unsigned char *)q)[0] == 0x55 && ((const unsigned char *)q)[sizeof(q) - 1u] == 0x55);
}

int main(void) {
    defaults();
    seated();
    order();
    rising();
    star();
    determinism();
    done();
    quad();
    hostile();
    if (fails) {
        fprintf(stderr, "rise_test: %d FAILED\n", fails);
        return 1;
    }
    puts("rise: PASS defaults, bad arguments, seated, lift-off order, rising (height, size, colour), star (fixed, twinkle), determinism, done, quad, packed colour, hostile input");
    return 0;
}
