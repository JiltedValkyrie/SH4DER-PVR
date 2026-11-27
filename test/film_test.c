/* SH4DER-PVR: film test

   test/film_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of the film wear effect: the defaults and their size; a block out of range is refused; the flicker stays
   inside its depth, never steps hard between two ticks, changes direction fewer than three times a second and does
   not repeat within a minute; the tint mixes by its strength; the weave stays within its pixels; a slot's scratch
   lives between its shortest and longest life, stays in the picture and out of the keep-out span, and no more than
   the slots are alive at once; the specks of a tick are within their count, inside the picture, at most the cap in
   the keep-out band, and differ from the next tick's; the same time gives the same answer; hostile input (every
   float of the block NaN or infinite, a bad or enormous time, null pointers) gives "no wear".
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_film.h"

#define W 640.0f     /* test picture width, pixels */
#define H 360.0f     /* and height */
#define RUN_S 60.0f  /* seconds of film the sweeps cover */

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* A float from raw bits: an infinity or a NaN that the compiler's fast-math cannot fold away. */
static float bits(uint32_t u) {
    float f;

    memcpy(&f, &u, sizeof(f));
    return f;
}

/* The defaults are valid and as the header says; a struct of the stated size. */
static void defaults(void) {
    sh4der_film_t f;

    memset(&f, 0, sizeof(f));
    CHECK(!sh4der_film_valid(&f) && !sh4der_film_valid(NULL));
    sh4der_film_init(NULL, W, H);                        /* must not crash */
    sh4der_film_init(&f, W, H);
    CHECK(sh4der_film_valid(&f));
    CHECK(f.width == W && f.height == H && f.tick_hz == 60.0f && f.flicker == 0.025f && f.weave_px == 1.0f);
    CHECK(f.scratch_min == 6u && f.scratch_max == 20u && f.specks_min == 6u && f.specks_max == 12u && f.seed == 1u);
    CHECK(sizeof(sh4der_film_t) == 92u);
    printf("  sizeof(sh4der_film_t) = %u\n", (unsigned)sizeof(sh4der_film_t));
}

/* Each field out of its range is refused, one at a time. */
static void ranges(void) {
    sh4der_film_t f;

    sh4der_film_init(&f, W, H);
    f.flicker = SH4DER_FILM_FLICKER_MAX + 0.001f;
    CHECK(!sh4der_film_valid(&f));                       /* too deep: a strobe */
    sh4der_film_init(&f, W, H);
    f.flicker_hz_a = SH4DER_FILM_RATE_MAX_HZ + 0.1f;
    CHECK(!sh4der_film_valid(&f));                       /* too fast: a strobe */
    sh4der_film_init(&f, W, H);
    f.weave_px = SH4DER_FILM_WEAVE_MAX_PX + 0.5f;
    CHECK(!sh4der_film_valid(&f));
    sh4der_film_init(&f, W, H);
    f.scratch_min = 0u;
    CHECK(!sh4der_film_valid(&f));
    sh4der_film_init(&f, W, H);
    f.scratch_max = 256u;
    CHECK(!sh4der_film_valid(&f));
    sh4der_film_init(&f, W, H);
    f.specks_max = SH4DER_FILM_SPECKS_MAX + 1u;
    CHECK(!sh4der_film_valid(&f));
    sh4der_film_init(&f, W, H);
    f.specks_min = f.specks_max + 1u;
    CHECK(!sh4der_film_valid(&f));
    sh4der_film_init(&f, W, H);
    f.scratch_alpha_hi = f.scratch_alpha_lo - 0.01f;
    CHECK(!sh4der_film_valid(&f));
    sh4der_film_init(&f, W, H);
    f.keep_x0 = 0.0f;
    f.keep_x1 = W;                                       /* a keep-out span over the whole picture */
    CHECK(!sh4der_film_valid(&f));
    sh4der_film_init(&f, W, H);
    f.width = 0.0f;
    CHECK(!sh4der_film_valid(&f));
    sh4der_film_init(&f, W, H);
    f.height = 3.0f;                                     /* a picture a speck (3 pixels a side) does not fit */
    CHECK(!sh4der_film_valid(&f));
    {
        /* A keep-out span against the picture's edge: every scratch lies on its one free side, inside the picture.
         * A span against both edges (narrower than the picture, so valid): no scratch at all. */
        sh4der_film_scratch_t s;
        unsigned tick, slot, seen = 0;

        for (tick = 0; tick < 3600u; tick++) {
            for (slot = 0; slot < SH4DER_FILM_SCRATCH_SLOTS; slot++) {
                const float t = ((float)tick + 0.5f) / 60.0f;

                sh4der_film_init(&f, W, H);
                f.keep_x0 = -10.0f;
                f.keep_x1 = 100.0f;                          /* against the left edge */
                if (sh4der_film_scratch(&f, slot, t, &s)) {
                    seen++;
                    CHECK(s.x >= 100.0f && s.x <= W - 1.0f);
                }
                f.keep_x0 = W - 100.0f;
                f.keep_x1 = W + 10.0f;                       /* against the right edge */
                if (sh4der_film_scratch(&f, slot, t, &s)) {
                    CHECK(s.x >= 0.0f && s.x <= W - 101.0f);
                }
                f.keep_x0 = 0.5f;
                f.keep_x1 = W - 0.25f;                       /* against both */
                CHECK(sh4der_film_valid(&f) && !sh4der_film_scratch(&f, slot, t, &s));
            }
        }
        CHECK(seen > 50u);
    }
    sh4der_film_init(&f, W, H);
    f.tick_hz = 1.0f;
    CHECK(!sh4der_film_valid(&f));
}

/* The flicker: inside 1 - 2 x depth .. 1, gentle between ticks, slow, not periodic; the tint by its strength. */
static void flicker(void) {
    static const float warm[3] = { 1.00f, 0.96f, 0.88f };
    sh4der_film_t f;
    float lo = 2.0f, hi = 0.0f, prev, prev_d = 0.0f, worst_step = 0.0f;
    unsigned turns = 0, same = 0, n = 0;
    uint32_t c;
    float t;

    sh4der_film_init(&f, W, H);
    prev = sh4der_film_exposure(&f, 0.0f);
    for (t = 1.0f / 60.0f; t < RUN_S; t += 1.0f / 60.0f) {
        const float e = sh4der_film_exposure(&f, t);
        const float d = e - prev;

        lo = e < lo ? e : lo;
        hi = e > hi ? e : hi;
        worst_step = fabsf(d) > worst_step ? fabsf(d) : worst_step;
        if (d * prev_d < 0.0f) {
            turns++;                                     /* the brightness changed direction */
        }
        if (fabsf(e - sh4der_film_exposure(&f, t + 7.0f)) < 1.0e-4f) {
            same++;                                      /* equal to itself 7 s later: chance now and then, not a period */
        }
        prev_d = d != 0.0f ? d : prev_d;
        prev = e;
        n++;
    }
    CHECK(lo >= 1.0f - 2.0f * f.flicker - 1.0e-5f && hi <= 1.0f + 1.0e-6f);
    CHECK(hi - lo >= 0.03f && hi - lo <= 0.08f);         /* a swing of 1.5 to 4 % either side of its middle */
    CHECK(worst_step <= 0.005f);                         /* no step over half a per cent between two 60 Hz frames */
    CHECK((float)turns / RUN_S < 2.0f * SH4DER_FILM_RATE_MAX_HZ);   /* under 3 full flickers (6 turns) a second */
    CHECK(same < n / 20u);
    printf("  flicker: %.3f .. %.3f, worst step %.4f a frame, %.1f turns a second\n", (double)lo, (double)hi, (double)worst_step, (double)((float)turns / RUN_S));

    f.flicker = 0.0f;                                    /* no flicker: the rectangle is the tint alone */
    CHECK(sh4der_film_exposure(&f, 3.0f) == 1.0f);
    CHECK(sh4der_film_exposure_argb(&f, 3.0f, NULL, 1.0f) == 0xffffffffu);
    CHECK(sh4der_film_exposure_argb(&f, 3.0f, warm, 0.0f) == 0xffffffffu);
    c = sh4der_film_exposure_argb(&f, 3.0f, warm, 1.0f);
    CHECK(((c >> 16) & 255u) == 255u && ((c >> 8) & 255u) == 245u && (c & 255u) == 224u && (c >> 24) == 255u);
    c = sh4der_film_exposure_argb(&f, 3.0f, warm, 0.15f);
    CHECK(((c >> 16) & 255u) == 255u && ((c >> 8) & 255u) >= 252u && (c & 255u) >= 249u && (c & 255u) <= 251u);
    c = sh4der_film_exposure_argb(&f, 3.0f, warm, 99.0f);                /* a strength past 1 is 1 */
    CHECK((c & 255u) == 224u);
}

/* The weave: within its pixels in both axes, and really moving in both. */
static void weave(void) {
    sh4der_film_t f;
    float min_x = 9.0f, max_x = -9.0f, min_y = 9.0f, max_y = -9.0f;
    float t;

    sh4der_film_init(&f, W, H);
    for (t = 0.0f; t < RUN_S; t += 1.0f / 60.0f) {
        float dx = 9.0f, dy = 9.0f;

        sh4der_film_weave(&f, t, &dx, &dy);
        CHECK(fabsf(dx) <= f.weave_px + 1.0e-5f && fabsf(dy) <= f.weave_px + 1.0e-5f);
        min_x = dx < min_x ? dx : min_x;
        max_x = dx > max_x ? dx : max_x;
        min_y = dy < min_y ? dy : min_y;
        max_y = dy > max_y ? dy : max_y;
    }
    CHECK(max_x - min_x > 1.5f * f.weave_px && max_y - min_y > 1.5f * f.weave_px);
    f.weave_px = 0.0f;
    {
        float dx = 9.0f, dy = 9.0f;

        sh4der_film_weave(&f, 2.0f, &dx, &dy);
        CHECK(dx == 0.0f && dy == 0.0f);
        sh4der_film_weave(&f, 2.0f, NULL, &dy);          /* must not crash */
    }
}

/* Scratches: lives within the stated range, columns in the picture and out of the keep-out span, light share near its value. */
static void scratches(void) {
    sh4der_film_t f;
    unsigned alive_run[SH4DER_FILM_SCRATCH_SLOTS] = { 0u, 0u, 0u };
    int whole[SH4DER_FILM_SCRATCH_SLOTS] = { 0, 0, 0 };     /* 1: this life was seen from its first tick (one alive at tick 0 was not) */
    unsigned lives = 0, light = 0, most_at_once = 0, shortest = 999u, longest = 0;
    uint32_t tick, slot;

    sh4der_film_init(&f, W, H);
    f.keep_x0 = 280.0f;
    f.keep_x1 = 360.0f;
    CHECK(sh4der_film_valid(&f));
    for (tick = 0; tick < (uint32_t)(RUN_S * 60.0f); tick++) {
        const float t = ((float)tick + 0.5f) / 60.0f;
        unsigned at_once = 0;

        for (slot = 0; slot < SH4DER_FILM_SCRATCH_SLOTS; slot++) {
            sh4der_film_scratch_t s;

            if (sh4der_film_scratch(&f, slot, t, &s)) {
                CHECK(s.x >= 0.0f && s.x <= W - 1.0f);
                CHECK(!(s.x > f.keep_x0 - 1.0f && s.x < f.keep_x1));     /* a one-pixel line at x does not touch the span */
                CHECK(s.alpha >= f.scratch_alpha_lo && s.alpha <= f.scratch_alpha_hi && (s.light == 0 || s.light == 1));
                if (alive_run[slot] == 0u) {
                    whole[slot] = tick > 0u;
                    lives++;
                    light += (unsigned)s.light;
                }
                alive_run[slot]++;
                at_once++;
            } else if (alive_run[slot] > 0u) {           /* a life just ended: its length */
                if (whole[slot]) {
                    shortest = alive_run[slot] < shortest ? alive_run[slot] : shortest;
                    longest = alive_run[slot] > longest ? alive_run[slot] : longest;
                }
                alive_run[slot] = 0u;
            }
        }
        most_at_once = at_once > most_at_once ? at_once : most_at_once;
    }
    CHECK(lives > 100u);
    CHECK(shortest >= f.scratch_min && longest <= f.scratch_max);        /* 0.1 to 0.33 s at 60 ticks a second */
    CHECK(most_at_once >= 2u && most_at_once <= SH4DER_FILM_SCRATCH_SLOTS);
    CHECK((float)light / (float)lives > f.scratch_light - 0.12f && (float)light / (float)lives < f.scratch_light + 0.12f);
    printf("  scratches: %u in %.0f s, lives %u .. %u ticks, %u at once at the most, %.0f %% light\n", lives, (double)RUN_S, shortest, longest, most_at_once,
           (double)(100.0f * (float)light / (float)lives));
    {
        sh4der_film_scratch_t s;

        CHECK(!sh4der_film_scratch(&f, SH4DER_FILM_SCRATCH_SLOTS, 1.0f, &s) && !sh4der_film_scratch(&f, 0u, 1.0f, NULL));
    }
}

/* Specks: the count, the places, the keep-out band's cap, a new set every tick. */
static void specks(void) {
    sh4der_film_t f;
    sh4der_film_speck_t a[SH4DER_FILM_SPECKS_MAX], b[SH4DER_FILM_SPECKS_MAX];
    unsigned least = 99u, most = 0u, dark = 0u, total = 0u, moved = 0u;
    uint32_t tick;

    sh4der_film_init(&f, W, H);
    f.keep_y0 = 200.0f;
    f.keep_y1 = 330.0f;
    f.keep_specks = 2u;
    for (tick = 0; tick < 1200u; tick++) {
        const float t = ((float)tick + 0.5f) / 60.0f;
        const unsigned n = sh4der_film_specks(&f, t, a, SH4DER_FILM_SPECKS_MAX);
        unsigned i, in_band = 0;

        CHECK(n <= f.specks_max);
        for (i = 0; i < n; i++) {
            CHECK(a[i].size >= 1.0f && a[i].size <= 3.0f && a[i].x >= 0.0f && a[i].y >= 0.0f && a[i].x + a[i].size <= W && a[i].y + a[i].size <= H);
            CHECK(a[i].light ? a[i].alpha == 0.5f : (a[i].alpha >= 0.40f && a[i].alpha <= 0.60f));
            in_band += (a[i].y + a[i].size > f.keep_y0 && a[i].y < f.keep_y1) ? 1u : 0u;
            dark += a[i].light ? 0u : 1u;
        }
        CHECK(in_band <= f.keep_specks);
        total += n;
        least = n < least ? n : least;
        most = n > most ? n : most;
        if (n > 0u && sh4der_film_specks(&f, t + 1.0f / 60.0f, b, SH4DER_FILM_SPECKS_MAX) > 0u && (a[0].x != b[0].x || a[0].y != b[0].y)) {
            moved++;
        }
    }
    CHECK(most <= f.specks_max && least >= 1u && moved > 1100u);
    CHECK((float)dark / (float)total > f.speck_dark - 0.1f && (float)dark / (float)total < f.speck_dark + 0.1f);
    f.keep_y1 = 0.0f;                                    /* no band: the full count every tick */
    least = 99u;
    for (tick = 0; tick < 600u; tick++) {
        const unsigned n = sh4der_film_specks(&f, ((float)tick + 0.5f) / 60.0f, a, SH4DER_FILM_SPECKS_MAX);

        least = n < least ? n : least;
    }
    CHECK(least >= f.specks_min);
    CHECK(sh4der_film_specks(&f, 1.0f, a, 3u) <= 3u && sh4der_film_specks(&f, 1.0f, a, 0u) == 0u && sh4der_film_specks(&f, 1.0f, NULL, 4u) == 0u);
    printf("  specks: %u .. %u a tick with the band capped at 2, %.0f %% dark\n", least, most, (double)(100.0f * (float)dark / (float)total));
}

/* The same block and time give the same answer; another seed gives another wear. */
static void determinism(void) {
    sh4der_film_t f, g;
    sh4der_film_speck_t a[SH4DER_FILM_SPECKS_MAX], b[SH4DER_FILM_SPECKS_MAX];
    unsigned n;

    sh4der_film_init(&f, W, H);
    g = f;
    g.seed = 77u;
    CHECK(sh4der_film_exposure(&f, 12.34f) == sh4der_film_exposure(&f, 12.34f));
    CHECK(sh4der_film_exposure(&f, 12.34f) != sh4der_film_exposure(&g, 12.34f));
    n = sh4der_film_specks(&f, 5.0f, a, SH4DER_FILM_SPECKS_MAX);
    CHECK(n == sh4der_film_specks(&f, 5.0f, b, SH4DER_FILM_SPECKS_MAX) && memcmp(a, b, n * sizeof(a[0])) == 0);
    CHECK(sh4der_film_specks(&g, 5.0f, b, SH4DER_FILM_SPECKS_MAX) > 0u && (a[0].x != b[0].x || a[0].y != b[0].y));
}

/* Hostile input: "no wear", nothing written out of bounds, no trap. */
static void hostile(void) {
    const float bad[3] = { bits(0x7f800000u), bits(0xff800000u), bits(0x7fc00000u) };   /* +inf, -inf, NaN */
    sh4der_film_t f;
    sh4der_film_scratch_t s;
    sh4der_film_speck_t sp[SH4DER_FILM_SPECKS_MAX];
    unsigned k, field;

    for (k = 0; k < 3u; k++) {
        float dx = 9.0f, dy = 9.0f;

        for (field = 0; field < 17u; field++) {          /* the 17 floats of the block, one at a time */
            float *as_floats;

            sh4der_film_init(&f, W, H);
            as_floats = &f.width;
            as_floats[field] = bad[k];
            CHECK(!sh4der_film_valid(&f));
            CHECK(sh4der_film_exposure(&f, 1.0f) == 1.0f && sh4der_film_exposure_argb(&f, 1.0f, NULL, 0.0f) == 0xffffffffu);
            CHECK(!sh4der_film_scratch(&f, 0u, 1.0f, &s) && sh4der_film_specks(&f, 1.0f, sp, SH4DER_FILM_SPECKS_MAX) == 0u);
        }
        sh4der_film_init(&f, W, H);                      /* a good block, a bad time */
        CHECK(sh4der_film_exposure(&f, bad[k]) == 1.0f);
        sh4der_film_weave(&f, bad[k], &dx, &dy);
        CHECK(dx == 0.0f && dy == 0.0f);
        CHECK(!sh4der_film_scratch(&f, 1u, bad[k], &s) && sh4der_film_specks(&f, bad[k], sp, SH4DER_FILM_SPECKS_MAX) == 0u);
        CHECK(sh4der_film_exposure_argb(&f, 1.0f, bad, bad[k]) != 0u);   /* a bad tint and strength: still a colour, alpha set */
    }
    sh4der_film_init(&f, W, H);
    CHECK(sh4der_film_exposure(&f, -1.0f) == 1.0f && sh4der_film_exposure(&f, 3.0e38f) == 1.0f);
    CHECK(SH4DER_FILM_TIME_MAX_S == 3600.0f && sh4der_film_exposure(&f, 3600.5f) == 1.0f);   /* past the hour: no wear */
    CHECK(!sh4der_film_scratch(&f, 0u, 3.0e38f, &s) && sh4der_film_specks(&f, -5.0f, sp, SH4DER_FILM_SPECKS_MAX) == 0u);
    CHECK(sh4der_film_exposure(&f, SH4DER_FILM_TIME_MAX_S) <= 1.0f && sh4der_film_exposure(&f, SH4DER_FILM_TIME_MAX_S) >= 0.9f);
    CHECK(sh4der_film_exposure(NULL, 1.0f) == 1.0f);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    defaults();
    ranges();
    flicker();
    weave();
    scratches();
    specks();
    determinism();
    hostile();
    if (fails) {
        fprintf(stderr, "film_test: %d FAILED\n", fails);
        return 1;
    }
    puts("film: PASS defaults, ranges, flicker (depth, step, rate, no period, tint), weave, scratches (life, place, keep-out, count), specks (count, place, band cap), determinism, hostile input");
    return 0;
}
