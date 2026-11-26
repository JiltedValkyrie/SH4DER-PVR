/* SH4DER-PVR: time base test

   test/timebase_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of the tape time-base module. The reference in this file is a second, plain transcription of the model's
   formulas (NTSCRT, port/engine/src/glitch/receiver.rs, MIT, Copyright (c) 2026 Finn McKenty; see the header) in
   double precision with the model's full 53-bit random numbers and no remembered values: the module, which works in
   single precision and walks its noise positions from line to line without dividing, must give the same row shifts to a small part of a pixel,
   at the start of play and five hours in. Then the properties a client leans on: a deck in order shifts nothing; the
   head switching step sits on the bottom rows the header names and changes side every field; a tighter tape hooks the
   top rows and the hook dies away down the picture; the master amount scales it all to nothing; no shift is larger
   than half a line; the same input gives the same output; a bad block, a bad state or a missing pointer writes nothing.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_timebase.h"

#define ROWS 360u        /* rows of the test picture: a 640 x 360 picture band */
#define WIDTH 640.0f     /* and its width, pixels */
#define RUN_FIELDS 240u  /* fields a comparison run covers: four seconds */
#define LATE_FIELD 1080000u  /* a field five hours into play (59.94 a second) */
/* Largest difference from the double reference, pixels. The SH4ZAM software path's sine is a table, so it gets more:
 * the host run of 2026-11-26 (SH4ZAM ff39547, software back end) gave 0.66, 0.85 and 0.56 px over four seconds of fields
 * (the television loop carries each line's sine error on to the next). Under a pixel in a picture bent by tens of them. */
#ifdef SH4DER_TEST_TOL
#define TOL_PX 1.0
#define TOL_MAX_PX 3.0    /* every fault at its limit and the set's lock at its fastest: 1.98 px in the same run */
#else
#define TOL_PX 0.05
#define TOL_MAX_PX 0.05   /* every fault at its limit and the set's lock at its fastest */
#endif
#define TAU_D 6.283185307179586   /* 2 pi */

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* A float from raw bits: an infinity or a NaN that the compiler's fast-math cannot fold away. */
static float bits(uint32_t u) {
    float f;

    memcpy(&f, &u, sizeof(f));
    return f;
}

/* ---- the reference: the model's formulas, double precision ---- */

/* the model's mixing hash */
static uint64_t ref_mix(uint64_t z0) {
    uint64_t z = z0 + 0x9e3779b97f4a7c15ull;

    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

/* uniform(seed, purpose, a, 0): 53 bits, 0 .. 1 */
static double ref_uni(uint64_t seed, uint64_t purpose, int64_t a) {
    const uint64_t h = ref_mix(seed ^ ref_mix(purpose ^ ref_mix((uint64_t)a ^ ref_mix(0ull))));

    return (double)(h >> 11) * (1.0 / 9007199254740992.0);
}

/* gauss(seed, purpose, a, 0): Box-Muller */
static double ref_gauss(uint64_t seed, uint64_t purpose, int64_t a) {
    double u1 = ref_uni(seed, purpose, a);
    const double u2 = ref_uni(seed, purpose + 0x51ull, a);

    if (u1 < 1.0e-12) {
        u1 = 1.0e-12;
    }
    return sqrt(-2.0 * log(u1)) * cos(TAU_D * u2);
}

/* value(seed, purpose, x): smooth noise -1 .. 1 */
static double ref_value(uint64_t seed, uint64_t purpose, double x) {
    const double i = floor(x);
    const double f = x - i;
    const double a = ref_uni(seed, purpose, (int64_t)i) * 2.0 - 1.0;
    const double b = ref_uni(seed, purpose, (int64_t)i + 1) * 2.0 - 1.0;

    return a + (b - a) * f * f * (3.0 - 2.0 * f);
}

/* the deck's error of signal line j, microseconds */
static double ref_error(const sh4der_timebase_t *p, int64_t j, int64_t lines, int64_t lead) {
    const uint64_t seed = ((uint64_t)p->seed_hi << 32) | (uint64_t)p->seed_lo;
    const double field_rate = 60000.0 / 1001.0;
    const double t = (double)j / ((double)lines * field_rate);         /* the line's time, seconds */
    const int64_t fs = (int64_t)floor((double)(j + lead) / (double)lines);   /* the field whose head plays it */
    const int64_t along = j + lead - fs * lines - lead;                 /* lines from the head's track start; negative before the picture's top */
    double e = 0.0;

    if (p->head_switch_us > 0.0f) {
        e += ((fs & 1) == 0 ? 0.5 : -0.5) * (double)p->head_switch_us * (1.0 + 0.15 * ref_gauss(seed, 31ull, fs));
    }
    if (p->flutter > 0.0f) {
        e += (double)p->flutter * (0.8 * sin(TAU_D * (field_rate * 0.5) * t + TAU_D * ref_uni(seed, 40ull, 0)) +
                                   1.3 * ref_value(seed, 41ull, 4.0 * t) + 0.45 * ref_value(seed, 42ull, 180.0 * t) +
                                   (p->line_noise ? 0.07 * ref_gauss(seed, 43ull, j) : 0.0));
    }
    if (p->tension != 0.0f) {
        e += 8.0e-4 * (double)p->tension * (double)along * (262.5 / (double)lines) * 63.556;
    }
    return e * (double)p->amount;
}

/* one field of the reference: the television's loop over every line, then the row shifts */
static void ref_field(const sh4der_timebase_t *p, double *theta, uint32_t field, double *shift_px) {
    const double h = 63.556;
    const int64_t vbi = (int64_t)sh4der_timebase_vbi(p->rows);
    const int64_t lines = (int64_t)p->rows + vbi;
    const int64_t lead = (int64_t)floor(3.5 * (double)lines / 262.5 + 0.5);
    const double kp = 0.04 * pow(10.0, (double)p->afc_speed) / ((double)lines / 262.5);
    const int64_t j0 = (int64_t)field * lines;
    int64_t r;

    for (r = 0; r < lines; r++) {
        int64_t j = r;
        double d = ref_error(p, j0 + j, lines, lead) - *theta;
        double u;

        if (d < -0.5 * h) {
            j++;
            d = h + ref_error(p, j0 + j, lines, lead) - *theta;
        } else if (d > 0.5 * h) {
            j--;
            d = ref_error(p, j0 + j, lines, lead) - h - *theta;
        }
        if (r >= vbi) {
            u = *theta - ref_error(p, j0 + r, lines, lead);
            if (u > 0.5 * h) {
                u -= h;
            } else if (u < -0.5 * h) {
                u += h;
            }
            shift_px[r - vbi] = -u * (double)p->width_px / 52.656;
        }
        *theta += kp * (h / TAU_D) * sin(TAU_D * d / h);
    }
}

/* ---- the checks ---- */

/* The largest difference, pixels, between the module and the reference over a run of fields from `first`. */
static double run_diff(const sh4der_timebase_t *p, uint32_t first) {
    static float got[SH4DER_TIMEBASE_ROWS_MAX];
    static double want[SH4DER_TIMEBASE_ROWS_MAX];
    sh4der_timebase_state_t st;
    double theta = 0.0;
    double worst = 0.0;
    uint32_t f, y;

    sh4der_timebase_reset(&st);
    for (f = first; f < first + RUN_FIELDS; f++) {
        CHECK(sh4der_timebase_field(p, &st, f, got) == p->rows);
        ref_field(p, &theta, f, want);
        for (y = 0; y < p->rows; y++) {
            const double d = fabs((double)got[y] - want[y]);

            if (d > worst) {
                worst = d;
            }
        }
    }
    return worst;
}

/* The defaults are valid and shift nothing; the raster's blanking and switch lines are as the header says. */
static void defaults(void) {
    static float sh[SH4DER_TIMEBASE_ROWS_MAX];
    sh4der_timebase_t p;
    sh4der_timebase_state_t st;
    uint32_t y;

    memset(&p, 0, sizeof(p));
    CHECK(!sh4der_timebase_valid(&p) && !sh4der_timebase_valid(NULL));
    sh4der_timebase_init(NULL, ROWS, WIDTH);             /* must not crash */
    sh4der_timebase_init(&p, ROWS, WIDTH);
    CHECK(sh4der_timebase_valid(&p));
    CHECK(p.amount == 1.0f && p.head_switch_us == 0.0f && p.flutter == 0.0f && p.tension == 0.0f && p.afc_speed == 0.5f);
    CHECK(sh4der_timebase_vbi(ROWS) == 30u && sh4der_timebase_vbi(10u) == SH4DER_TIMEBASE_VBI_MIN);
    CHECK(sh4der_timebase_switch_lead(ROWS + 30u) == 5u);   /* 360 rows: the bottom five, rows 355 .. 359 */
    sh4der_timebase_reset(&st);
    CHECK(sh4der_timebase_field(&p, &st, 7u, sh) == ROWS);
    for (y = 0; y < ROWS; y++) {
        CHECK(sh[y] == 0.0f);
    }
}

/* Single precision with remembered cells against the double reference: every fault on, at the start and late. */
static void against_reference(void) {
    sh4der_timebase_t p;
    double d;

    sh4der_timebase_init(&p, ROWS, WIDTH);
    p.head_switch_us = 2.0f;
    p.flutter = 1.0f;
    p.tension = 0.5f;
    d = run_diff(&p, 0u);
    CHECK(d < TOL_PX);
    printf("timebase: worst difference from the reference %.4f px (start)", d);
    d = run_diff(&p, LATE_FIELD);
    CHECK(d < TOL_PX);
    printf(", %.4f px (five hours in)", d);
    p.line_noise = 0u;
    p.afc_speed = 0.0f;
    p.tension = -1.0f;
    p.amount = 0.3f;
    d = run_diff(&p, 12345u);
    CHECK(d < TOL_PX);
    printf(", %.4f px (slow set, no line noise)", d);
    sh4der_timebase_init(&p, 61u, 320.0f);               /* a small raster */
    p.head_switch_us = 8.0f;
    p.flutter = 5.0f;
    p.tension = 1.0f;
    p.afc_speed = 1.0f;
    d = run_diff(&p, 3u);
    CHECK(d < TOL_MAX_PX);
    printf(", %.4f px (small raster, every fault at its limit, fast set)\n", d);
}

/* The noise positions are walked from line to line by addition. Walking must give exactly what placing them afresh
 * (by division) at every line gives: the same cell and the same place in it, over a field's lines, early and late in
 * play; and so the same error (to 0.00001 microseconds: a fast-math build may round the two inlined copies apart). */
static void walk_equals_division(void) {
    sh4der_timebase_t p;
    sh4der_timebase_memo_t_ walk, fresh;
    const uint32_t lines = ROWS + sh4der_timebase_vbi(ROWS);
    const uint32_t lead = sh4der_timebase_switch_lead(lines);
    const int64_t fields[4] = { 0, 1, 77, (int64_t)LATE_FIELD };
    unsigned k;
    int32_t line;

    sh4der_timebase_init(&p, ROWS, WIDTH);
    p.head_switch_us = 2.0f;
    p.flutter = 1.0f;
    p.tension = 0.5f;
    for (k = 0; k < 4u; k++) {
        sh4der_timebase_memo_zero_(&walk);
        for (line = -2; line < (int32_t)lines + 2; line++) {
            float a, b;

            sh4der_timebase_memo_zero_(&fresh);
            a = sh4der_timebase_error_(&p, fields[k], line, lines, lead, &walk);
            b = sh4der_timebase_error_(&p, fields[k], line, lines, lead, &fresh);
            CHECK(fabsf(a - b) <= 1.0e-5f);
            CHECK(walk.hunt.i == fresh.hunt.i && walk.hunt.rem == fresh.hunt.rem);       /* both now stand at the next line */
            CHECK(walk.rough.i == fresh.rough.i && walk.rough.rem == fresh.rough.rem);
        }
    }
}

/* The head switching step: only the bottom rows jump, by about the step, and the side changes every field. */
static void head_switch(void) {
    static float a[SH4DER_TIMEBASE_ROWS_MAX];
    static float b[SH4DER_TIMEBASE_ROWS_MAX];
    sh4der_timebase_t p;
    sh4der_timebase_state_t st;
    const float px_per_us = WIDTH / SH4DER_TIMEBASE_ACTIVE_US;
    uint32_t f;

    sh4der_timebase_init(&p, ROWS, WIDTH);
    p.head_switch_us = 1.0f;
    p.afc_speed = 0.0f;
    sh4der_timebase_reset(&st);
    for (f = 0; f < 60u; f++) {                           /* let the set settle on the alternating signal */
        CHECK(sh4der_timebase_field(&p, &st, f, a) == ROWS);
    }
    CHECK(sh4der_timebase_field(&p, &st, 60u, a) == ROWS);
    CHECK(sh4der_timebase_field(&p, &st, 61u, b) == ROWS);
    /* the jump between row 354 and row 355 is the step (1 microsecond, with its wobble of a few tenths) */
    CHECK(fabsf(a[355] - a[354]) > 0.4f * px_per_us && fabsf(a[355] - a[354]) < 1.8f * px_per_us);
    CHECK(fabsf(a[354] - a[353]) < 0.1f * fabsf(a[355] - a[354]) && fabsf(a[359] - a[355]) < 0.3f * fabsf(a[355] - a[354]));
    printf("timebase: head switch 1 us, slow set: rows 353 .. 359 at %.2f %.2f | %.2f %.2f %.2f %.2f %.2f px\n", (double)a[353], (double)a[354],
           (double)a[355], (double)a[356], (double)a[357], (double)a[358], (double)a[359]);
    CHECK((a[355] - a[354]) * (b[355] - b[354]) < 0.0f);  /* the other head: the other side */
    CHECK(fabsf(a[180] - a[181]) < 0.1f);
}

/* Tension: the top rows hook, the hook dies away down the picture, and a slack tape hooks the other way. */
static void tension(void) {
    static float a[SH4DER_TIMEBASE_ROWS_MAX];
    static float b[SH4DER_TIMEBASE_ROWS_MAX];
    sh4der_timebase_t p;
    sh4der_timebase_state_t st;
    float rest;                                          /* the settled picture's shift, pixels */
    uint32_t f, y;

    sh4der_timebase_init(&p, ROWS, WIDTH);
    p.tension = 0.5f;
    p.afc_speed = 0.0f;
    sh4der_timebase_reset(&st);
    for (f = 0; f < 60u; f++) {
        CHECK(sh4der_timebase_field(&p, &st, f, a) == ROWS);
    }
    p.tension = -0.5f;
    sh4der_timebase_reset(&st);
    for (f = 0; f < 60u; f++) {
        CHECK(sh4der_timebase_field(&p, &st, f, b) == ROWS);
    }
    /* A loop that only steers by the error follows the tape's steady drift with a constant lag: the whole picture sits
     * a few pixels aside (rest), and the hook is what the top rows add to that. */
    rest = a[300];
    CHECK(fabsf(a[0] - rest) > 4.0f);
    CHECK(fabsf(a[0] - rest) > fabsf(a[38] - rest) && fabsf(a[38] - rest) > fabsf(a[114] - rest) && fabsf(a[114] - rest) > fabsf(a[250] - rest));
    CHECK(fabsf(a[250] - rest) < 0.02f * fabsf(a[0] - rest));
    for (y = 0; y < ROWS; y++) {
        CHECK(fabsf(a[y] + b[y]) < 0.02f * fabsf(a[0]) + 0.01f);   /* mirrored: the loop is close to linear here */
    }
    printf("timebase: tension 0.5, slow set, 640 px: rest %.2f px; hook from rest %.2f px at row 0, %.2f at 38, %.2f at 76, %.2f at 114, %.2f at 180\n",
           (double)rest, (double)(a[0] - rest), (double)(a[38] - rest), (double)(a[76] - rest), (double)(a[114] - rest), (double)(a[180] - rest));
}

/* The master amount at 0 silences every fault; no shift ever exceeds half a line; equal input, equal output. */
static void amount_bounds_determinism(void) {
    static float a[SH4DER_TIMEBASE_ROWS_MAX];
    static float b[SH4DER_TIMEBASE_ROWS_MAX];
    sh4der_timebase_t p;
    sh4der_timebase_state_t sa, sb;
    const float half_line_px = 0.5f * SH4DER_TIMEBASE_LINE_US * WIDTH / SH4DER_TIMEBASE_ACTIVE_US;
    uint32_t f, y;

    sh4der_timebase_init(&p, ROWS, WIDTH);
    p.head_switch_us = SH4DER_TIMEBASE_HEAD_SWITCH_MAX_US;
    p.flutter = SH4DER_TIMEBASE_FLUTTER_MAX;
    p.tension = 1.0f;
    sh4der_timebase_reset(&sa);
    sh4der_timebase_reset(&sb);
    for (f = 0; f < 120u; f++) {
        CHECK(sh4der_timebase_field(&p, &sa, f, a) == ROWS);
        CHECK(sh4der_timebase_field(&p, &sb, f, b) == ROWS);
        CHECK(memcmp(a, b, ROWS * sizeof(a[0])) == 0 && sa.theta == sb.theta);
        for (y = 0; y < ROWS; y++) {
            CHECK(fabsf(a[y]) <= half_line_px + 0.01f);
        }
    }
    p.amount = 0.0f;
    sh4der_timebase_reset(&sa);
    CHECK(sh4der_timebase_field(&p, &sa, 5u, a) == ROWS);
    for (y = 0; y < ROWS; y++) {
        CHECK(a[y] == 0.0f);
    }
}

/* A bad block, a bad state or a missing pointer: 0 rows, nothing written, the state untouched. */
static void hostile(void) {
    float sh[4] = { 9.0f, 9.0f, 9.0f, 9.0f };
    sh4der_timebase_t p, q;
    sh4der_timebase_state_t st;
    const float bad[3] = { bits(0x7fc00000u), bits(0x7f800000u), bits(0xff800000u) };   /* NaN, +infinity, -infinity */
    uint32_t i;

    sh4der_timebase_init(&p, 4u, WIDTH);
    sh4der_timebase_reset(&st);
    CHECK(sh4der_timebase_field(NULL, &st, 0u, sh) == 0u && sh4der_timebase_field(&p, NULL, 0u, sh) == 0u);
    CHECK(sh4der_timebase_field(&p, &st, 0u, NULL) == 0u);
    for (i = 0; i < 3u; i++) {
        q = p;
        q.width_px = bad[i];
        CHECK(sh4der_timebase_field(&q, &st, 0u, sh) == 0u);
        q = p;
        q.amount = bad[i];
        CHECK(sh4der_timebase_field(&q, &st, 0u, sh) == 0u);
        q = p;
        q.head_switch_us = bad[i];
        CHECK(sh4der_timebase_field(&q, &st, 0u, sh) == 0u);
        q = p;
        q.flutter = bad[i];
        CHECK(sh4der_timebase_field(&q, &st, 0u, sh) == 0u);
        q = p;
        q.tension = bad[i];
        CHECK(sh4der_timebase_field(&q, &st, 0u, sh) == 0u);
        q = p;
        q.afc_speed = bad[i];
        CHECK(sh4der_timebase_field(&q, &st, 0u, sh) == 0u);
        st.theta = bad[i];
        CHECK(sh4der_timebase_field(&p, &st, 0u, sh) == 0u);
        st.theta = 0.0f;
    }
    q = p;
    q.rows = 0u;
    CHECK(sh4der_timebase_field(&q, &st, 0u, sh) == 0u);
    q.rows = SH4DER_TIMEBASE_ROWS_MAX + 1u;
    CHECK(sh4der_timebase_field(&q, &st, 0u, sh) == 0u);
    q = p;
    q.head_switch_us = SH4DER_TIMEBASE_HEAD_SWITCH_MAX_US + 1.0f;
    CHECK(sh4der_timebase_field(&q, &st, 0u, sh) == 0u);
    q = p;
    q.line_noise = 2u;
    CHECK(sh4der_timebase_field(&q, &st, 0u, sh) == 0u);
    CHECK(sh[0] == 9.0f && sh[1] == 9.0f && sh[2] == 9.0f && sh[3] == 9.0f && st.theta == 0.0f);
    CHECK(sh4der_timebase_field(&p, &st, 0xffffffffu, sh) == 4u);   /* the largest field number is still a field */
    CHECK(sh[0] == 0.0f && sh[3] == 0.0f);
    sh4der_timebase_reset(NULL);                          /* must not crash */
}

int main(void) {
    defaults();
    against_reference();
    walk_equals_division();
    head_switch();
    tension();
    amount_bounds_determinism();
    hostile();
    if (fails) {
        fprintf(stderr, "timebase_test: %d FAILED\n", fails);
        return 1;
    }
    puts("timebase: PASS defaults, double reference (start, five hours in, slow set, small raster), walked noise equals divided, head switching step, tension hook, amount, bounds, determinism, hostile input");
    return 0;
}
