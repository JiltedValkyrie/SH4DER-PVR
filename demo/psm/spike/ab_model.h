/* SH4DER-PVR: route AB spike, the model of what the render target should hold

   demo/psm/spike/ab_model.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Plain C, no KallistiOS, no SH4ZAM: the same file is compiled into the Dreamcast test program (ab_spike.c) and into
   the host test (test/ab_model_test.c). It holds the test geometry, the ramp textures, the value each pixel of the
   128 x 128 target should come back with, and the count of what did not.

   The idea under test (demo/psm/ROUTE_AB_NOTES.md): a quad is drawn with a ramp texture whose texels hold their own
   index as the raw 16-bit colour, point sampled, texture environment REPLACE. The vertex depth is k (the PVR's 1 / w)
   and the texture coordinate is u = n / k, so the pixel should hold the code floor(S x n / k) with S the number of
   codes:

     10 bits  a 1024 x 8 ramp, texel (i, any row) = i. S = 1024, v = 0.5.
     16 bits  a 1024 x 64 ramp, texel (i, j) = 64 i + j. u clamps and picks the high 10 bits, v = 1024 u wraps over
              the 64 rows and picks the low 6. S = 65536.

   Every quad is a rectangle with whole-number edges, so which pixel centres it covers does not depend on the PVR's
   fill rule. k and n are affine over the screen, so the two triangles of the strip agree.

   This is a model of the idea. Whether the PVR does it (the texel bit-exact through to a 16-bit target, the texture
   coordinate's precision, where in the pixel it samples) is what the console run measures. Nothing here was run on
   a Dreamcast when this file was written.
*/

#ifndef AB_MODEL_H
#define AB_MODEL_H

#include <stdint.h>
#include <string.h>

#define AB_SIZE 128u               /* the target: 128 x 128, 16 bits a pixel, 32 KB */
#define AB_RAMP_W 1024u
#define AB_RAMP10_H 8u
#define AB_RAMP16_H 64u
#define AB_RECTS_MAX 6u
#define AB_HARD_MARGIN 0.125f      /* a wrong pixel whose exact value sits further than this (in codes) from a step is "hard" */

/* A rectangle on the target. k(X) = k0 + kx (X - x0); n(X, Y) = n0 + nx (X - x0) + ny (Y - y0); u = n / k. */
typedef struct ab_rect {
    float x0, y0, x1, y1;
    float k0, kx;
    float n0, nx, ny;
} ab_rect_t;

typedef struct ab_test {
    const char *name;
    unsigned nrect;
    ab_rect_t rect[AB_RECTS_MAX];  /* sent in this order; the nearest (largest k) should win where two overlap */
} ab_test_t;

enum { AB_TEST_FLAT = 0, AB_TEST_TILT = 1, AB_TEST_OVER = 2, AB_TESTS = 3 };
#define AB_TEST_PREC 3u            /* 16 bits only, beside the three: the wrapped v at six sizes of the code */
#define AB_PREC_BANDS 6u

static inline float ab_codes(int bits) { return bits == 16 ? 65536.0f : 1024.0f; }

/* A rectangle at constant depth k whose code at pixel (x, y) is a + b (x - x0) + c (y - y0), sampled mid-step: the
   exact value at a pixel centre is that whole number plus one half, the furthest it can be from a rounding edge.
   With b and c even, a half-pixel shift of the sample point moves the value by whole codes, so it stays readable. */
static inline void ab_rect_flat(ab_rect_t *r, int bits, float x0, float y0, float x1, float y1, float k, float a, float b, float c) {
    const float s = 1.0f / ab_codes(bits);

    r->x0 = x0; r->y0 = y0; r->x1 = x1; r->y1 = y1;
    r->k0 = k; r->kx = 0.0f;
    r->n0 = (a - 0.5f * b - 0.5f * c + 0.5f) * s * k;
    r->nx = b * s * k; r->ny = c * s * k;
}

/* A rectangle whose depth runs from kl at its left edge to kr at its right, with u = a / k: the depth code itself. */
static inline void ab_rect_tilt(ab_rect_t *r, float x0, float y0, float x1, float y1, float kl, float kr, float a) {
    r->x0 = x0; r->y0 = y0; r->x1 = x1; r->y1 = y1;
    r->k0 = kl; r->kx = (kr - kl) / (x1 - x0);
    r->n0 = a; r->nx = 0.0f; r->ny = 0.0f;
}

/* Band i of the precision test: the code at its left pixel, and its rows y0 .. y1 - 1. The code rises by one a pixel
   across the band, mid-step, so each band asks whether the low bits survive at that size of u and v (v = 1024 u is
   1 at the first band and 896 at the last). */
static inline unsigned ab_prec_base(unsigned i) {
    static const unsigned base[AB_PREC_BANDS] = { 64u, 1024u, 4096u, 16384u, 32768u, 57344u };

    return base[i < AB_PREC_BANDS ? i : 0u];
}
static inline void ab_prec_rows(unsigned i, unsigned *y0, unsigned *y1) { *y0 = 8u + 18u * i; *y1 = *y0 + 16u; }

/* The tests, for a ramp of `bits` (10 or 16; AB_TEST_PREC: 16 alone). 0 for a bad input. */
static inline int ab_test_make(ab_test_t *t, unsigned which, int bits) {
    if (!t || (bits != 10 && bits != 16)) return 0;
    memset(t, 0, sizeof *t);
    switch (which) {
    case AB_TEST_FLAT:                       /* one quad, constant depth, a different code at every pixel */
        t->name = "FLAT";
        t->nrect = 1u;
        if (bits == 10) ab_rect_flat(&t->rect[0], bits, 8.0f, 8.0f, 120.0f, 120.0f, 1.0f, 100.0f, 2.0f, 4.0f);
        else ab_rect_flat(&t->rect[0], bits, 8.0f, 8.0f, 120.0f, 120.0f, 1.0f, 4096.0f, 38.0f, 302.0f);
        return 1;
    case AB_TEST_TILT:                       /* one quad whose depth halves across it: the code is a / k */
        t->name = "TILT";
        t->nrect = 1u;
        ab_rect_tilt(&t->rect[0], 8.0f, 8.0f, 120.0f, 120.0f, 0.5f, 1.0f, 0.3f);
        return 1;
    case AB_TEST_OVER:                       /* two quads that overlap, near sent first on the top half, far first on the bottom */
        t->name = "OVER";
        t->nrect = 4u;
        if (bits == 10) {
            ab_rect_flat(&t->rect[0], bits, 8.0f, 8.0f, 80.0f, 64.0f, 0.8f, 700.0f, 0.0f, 0.0f);
            ab_rect_flat(&t->rect[1], bits, 48.0f, 8.0f, 120.0f, 64.0f, 0.4f, 300.0f, 0.0f, 0.0f);
            ab_rect_flat(&t->rect[2], bits, 48.0f, 64.0f, 120.0f, 120.0f, 0.4f, 300.0f, 0.0f, 0.0f);
            ab_rect_flat(&t->rect[3], bits, 8.0f, 64.0f, 80.0f, 120.0f, 0.8f, 700.0f, 0.0f, 0.0f);
        } else {
            ab_rect_flat(&t->rect[0], bits, 8.0f, 8.0f, 80.0f, 64.0f, 0.8f, 45227.0f, 0.0f, 0.0f);
            ab_rect_flat(&t->rect[1], bits, 48.0f, 8.0f, 120.0f, 64.0f, 0.4f, 19285.0f, 0.0f, 0.0f);
            ab_rect_flat(&t->rect[2], bits, 48.0f, 64.0f, 120.0f, 120.0f, 0.4f, 19285.0f, 0.0f, 0.0f);
            ab_rect_flat(&t->rect[3], bits, 8.0f, 64.0f, 80.0f, 120.0f, 0.8f, 45227.0f, 0.0f, 0.0f);
        }
        return 1;
    case AB_TEST_PREC:
        if (bits != 16) return 0;
        t->name = "PREC";
        t->nrect = AB_PREC_BANDS;
        for (which = 0; which < AB_PREC_BANDS; which++) {
            unsigned y0, y1;

            ab_prec_rows(which, &y0, &y1);
            ab_rect_flat(&t->rect[which], bits, 8.0f, (float)y0, 120.0f, (float)y1, 1.0f, (float)ab_prec_base(which), 1.0f, 0.0f);
        }
        return 1;
    default:
        return 0;
    }
}

/* ---- what the program sends ---------------------------------------------------- */

/* The ramp texture for `bits` into px (row major, 1024 wide; 8 rows for 10 bits, 64 for 16). Returns its height. */
static inline unsigned ab_ramp_fill(int bits, uint16_t *px) {
    const unsigned h = bits == 16 ? AB_RAMP16_H : AB_RAMP10_H;
    unsigned i, j;

    for (j = 0; j < h; j++)
        for (i = 0; i < AB_RAMP_W; i++) px[j * AB_RAMP_W + i] = (uint16_t)(bits == 16 ? i * AB_RAMP16_H + j : i);
    return h;
}

/* Corner i of a rectangle as a triangle strip (0: left bottom, 1: left top, 2: right bottom, 3: right top):
   xyz[2] is k, the PVR's 1 / w; uv are the texture coordinates for a ramp of `bits`. */
static inline void ab_rect_corner(const ab_rect_t *r, unsigned i, int bits, float xyz[3], float uv[2]) {
    const float x = (i & 2u) ? r->x1 : r->x0, y = (i & 1u) ? r->y0 : r->y1;
    const float k = r->k0 + r->kx * (x - r->x0);
    const float u = (r->n0 + r->nx * (x - r->x0) + r->ny * (y - r->y0)) / k;

    xyz[0] = x; xyz[1] = y; xyz[2] = k;
    uv[0] = u;
    uv[1] = bits == 16 ? u * (float)AB_RAMP_W : 0.5f;
}

/* ---- what should come back ------------------------------------------------------ */

/* The code pixel (x, y) should hold, with the sample point moved (ox, oy) pixels from the pixel's centre (0, 0: the
   centre; the PVR's own sample point is one of the things to find out). 0 where no quad covers the pixel (the
   target's background is black). *margin, when asked for: how far the exact value is from a step, in codes, 0 .. 0.5;
   a pixel that differs by one with a margin near 0 is rounding, not a wrong picture. */
static inline unsigned ab_predict(const ab_test_t *t, int bits, unsigned x, unsigned y, float ox, float oy, float *margin) {
    const float cx = (float)x + 0.5f, cy = (float)y + 0.5f, s = ab_codes(bits);
    const ab_rect_t *best = NULL;
    float bestk = 0.0f, c, f;
    unsigned i, code;

    for (i = 0; i < t->nrect; i++) {
        const ab_rect_t *r = &t->rect[i];
        const float k = r->k0 + r->kx * (cx - r->x0);

        if (cx < r->x0 || cx > r->x1 || cy < r->y0 || cy > r->y1) continue;
        if (!best || k > bestk) { best = r; bestk = k; }
    }
    if (margin) *margin = 0.5f;
    if (!best) return 0u;
    c = (best->n0 + best->nx * (cx + ox - best->x0) + best->ny * (cy + oy - best->y0)) / (best->k0 + best->kx * (cx + ox - best->x0)) * s;
    if (c < 0.0f) c = 0.0f;
    if (c > s - 1.0f) c = s - 1.0f;
    code = (unsigned)c;
    f = c - (float)code;
    if (margin) *margin = f < 0.5f ? f : 1.0f - f;
    return code;
}

/* What a check found. Errors are in codes. */
typedef struct ab_stats {
    unsigned pixels;               /* pixels compared (the whole target) */
    unsigned bad;                  /* pixels that differ from the model */
    unsigned hard;                 /* of those: not explained by rounding at a step (margin over AB_HARD_MARGIN, or off by more than 1) */
    unsigned cover;                /* of those: a quad's pixel that came back as background, or the other way round */
    unsigned max_err;              /* the largest difference, coverage errors left out */
    unsigned neg1, pos1, more;     /* by sign: came back one under the model, one over, further off (coverage errors left out) */
    unsigned cx0, cx1, cy0, cy1;   /* the box round the coverage errors (cover > 0) */
    unsigned lo_bad, hi_bad;       /* 16 bits: pixels whose low 6 bits (the wrapped v) differ, whose high 10 (the clamped u) differ */
    unsigned first_x, first_y, first_got, first_want;   /* the first hard pixel (or the first bad one when none is hard) */
    int first_set, first_hard;
} ab_stats_t;

/* Compare rows y0 .. y1 - 1 of a target (AB_SIZE wide, row y at got + y * stride) with the model. */
static inline void ab_check_rows(const ab_test_t *t, int bits, const uint16_t *got, unsigned stride, float ox, float oy,
                                 unsigned y0, unsigned y1, ab_stats_t *st) {
    unsigned x, y;

    memset(st, 0, sizeof *st);
    for (y = y0; y < y1 && y < AB_SIZE; y++) {
        for (x = 0; x < AB_SIZE; x++) {
            float margin;
            const unsigned want = ab_predict(t, bits, x, y, ox, oy, &margin), have = got[y * stride + x];
            const unsigned err = want > have ? want - have : have - want;
            int hard;

            st->pixels++;
            if (!err) continue;
            st->bad++;
            hard = err > 1u || margin > AB_HARD_MARGIN;
            if (!want || !have) {
                if (!st->cover) { st->cx0 = st->cx1 = x; st->cy0 = st->cy1 = y; }
                if (x < st->cx0) st->cx0 = x;
                if (x > st->cx1) st->cx1 = x;
                if (y < st->cy0) st->cy0 = y;
                if (y > st->cy1) st->cy1 = y;
                st->cover++; hard = 1;
            } else {
                if (err > st->max_err) st->max_err = err;
                if (err > 1u) st->more++;
                else if (have < want) st->neg1++;
                else st->pos1++;
            }
            st->hard += (unsigned)hard;
            if (bits == 16) {
                st->lo_bad += (want & 63u) != (have & 63u);
                st->hi_bad += (want >> 6) != (have >> 6);
            }
            if (!st->first_set || (hard && !st->first_hard)) {
                st->first_set = 1; st->first_hard = hard;
                st->first_x = x; st->first_y = y; st->first_got = have; st->first_want = want;
            }
        }
    }
}

/* Compare a whole target (AB_SIZE x AB_SIZE) with the model. */
static inline void ab_check(const ab_test_t *t, int bits, const uint16_t *got, unsigned stride, float ox, float oy, ab_stats_t *st) {
    ab_check_rows(t, bits, got, stride, ox, oy, 0u, AB_SIZE, st);
}

/* The sample point that fits best: the model is tried with the sample moved by -0.5, 0 and +0.5 pixels each way, and
   the offset with the fewest wrong pixels is returned in *ox, *oy with its count (the centre wins a tie). */
static inline unsigned ab_best_offset(const ab_test_t *t, int bits, const uint16_t *got, unsigned stride, float *ox, float *oy) {
    static const float o[3] = { 0.0f, -0.5f, 0.5f };
    unsigned i, j, best = ~0u;
    ab_stats_t st;

    for (j = 0; j < 3u; j++) {
        for (i = 0; i < 3u; i++) {
            ab_check(t, bits, got, stride, o[i], o[j], &st);
            if (st.bad < best) { best = st.bad; *ox = o[i]; *oy = o[j]; }
        }
    }
    return best;
}

#endif /* AB_MODEL_H */
