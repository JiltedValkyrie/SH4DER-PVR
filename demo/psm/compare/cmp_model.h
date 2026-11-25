/* SH4DER-PVR: the PVR-side compare spike, the model

   demo/psm/compare/cmp_model.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Can the PVR itself decide "is this pixel in shadow", so the SH-4 does not? A shadow compare is the sign of M - R:
   M the map's depth at the pixel (a texture), R the receiver's depth (interpolated). The PVR has no subtract and
   combines a texture with vertex values by multiplying. The spike asks the console about four ways round that, and
   this file is the plain C MODEL of each: what a pixel would hold if the PVR's arithmetic is 8 bits a channel with
   the rounding named by `rule`. A model, not a measurement: the console's numbers are compared with it, and its
   whole purpose is that the rounding rule the console shows can be named.

   Every test is drawn over a 256 x 256 square. R runs across x and M down y, one level a pixel: R = x, M = y. The
   pixel is lit where M >= R, that is y >= x; the shadow begins at x = y + 1. A test's result is how far the console's
   boundary sits from that, row by row, in levels of 256.

     BLEND (routes C1 and F)  layers added with saturation:
        a  the pixel is set to 255 - R
        b  M is added (ONE / ONE): the pixel is min(255, 255 - R + M), which is 255 exactly where M >= R
        c  inverted (a white layer times 1 - destination): R - M where that is positive, else 0
        d  doubled n times (a white layer times destination, plus destination): 255 wherever R - M was 1 or more
     ALPHA (route C2)         the same in the frame's destination alpha, the colour left alone, then the colour is
                              multiplied by 1 - alpha.
     PUNCH (route C3)         the punch-through alpha test on a product: the pixel is drawn where
                              texture alpha x vertex alpha >= reference.
     VOLUME (route E)         a modifier volume: the pixels of a sloped quad that lie inside a box are scaled.

   The map as the PVR would draw it is RGB565: 6 bits of green. The model's `bits` says how many bits of M reach the
   blend: 8 (a paletted map) or 6.
*/

#ifndef CMP_MODEL_H
#define CMP_MODEL_H

#include <stdint.h>
#include <string.h>

#define CMP_N 256u                         /* the test square, and the number of levels */

/* How an 8-bit multiply a x b (both 0 .. 255, 255 standing for 1.0) might be rounded. */
enum {
    CMP_RULE_EXACT = 0,                    /* round(a b / 255): x times 1.0 is x */
    CMP_RULE_TRUNC = 1,                    /* floor(a b / 255): x times 1.0 is x */
    CMP_RULE_SHIFT = 2,                    /* (a b) >> 8: x times 1.0 is x - 1 (for x over 0) */
    CMP_RULE_SHIFT1 = 3,                   /* (a (b + 1)) >> 8: x times 1.0 is x */
    CMP_RULES = 4
};

static inline const char *cmp_rule_name(int rule) {
    static const char *const n[CMP_RULES] = { "round(ab/255)", "floor(ab/255)", "ab>>8", "a(b+1)>>8" };

    return n[rule >= 0 && rule < (int)CMP_RULES ? rule : 0];
}

static inline unsigned cmp_mul(unsigned a, unsigned b, int rule) {
    switch (rule) {
    case CMP_RULE_TRUNC: return a * b / 255u;
    case CMP_RULE_SHIFT: return (a * b) >> 8;
    case CMP_RULE_SHIFT1: return (a * (b + 1u)) >> 8;
    default: return (a * b + 127u) / 255u;
    }
}

static inline unsigned cmp_sat(unsigned v) { return v > 255u ? 255u : v; }

/* M as the blend sees it: all 8 bits, or the top `bits` of it spread back over 8 the way a 565 texel is (the top
   bits repeated below). */
static inline unsigned cmp_map_level(unsigned m, unsigned bits) {
    unsigned q;

    if (bits >= 8u) return m & 255u;
    q = (m & 255u) >> (8u - bits);
    return cmp_sat((q << (8u - bits)) | (q >> (2u * bits - 8u)));
}

/* BLEND: the pixel at (x, y) after the add, the invert and `doubles` doublings. 0 = lit, 255 = shadow when the
   doublings are enough (8 always are). */
static inline unsigned cmp_blend_pixel(unsigned x, unsigned y, unsigned doubles, unsigned bits, int rule) {
    unsigned v = cmp_sat((255u - x) + cmp_map_level(y, bits));   /* a, b */
    unsigned i;

    v = cmp_mul(255u, 255u - v, rule);                           /* c: white x (1 - dst) */
    for (i = 0; i < doubles; i++) v = cmp_sat(cmp_mul(255u, v, rule) + v);   /* d: white x dst + dst */
    return v;
}

/* ALPHA: the colour at (x, y) of a frame whose colour was `grey` everywhere, after the compare in destination alpha
   and the final multiply by 1 - alpha. `grey` = lit, 0 = shadow. */
static inline unsigned cmp_alpha_pixel(unsigned x, unsigned y, unsigned grey, int rule) {
    unsigned a = cmp_sat((255u - x) + y), i;

    a = cmp_mul(255u, 255u - a, rule);
    for (i = 0; i < 8u; i++) a = cmp_sat(cmp_mul(255u, a, rule) + a);
    return cmp_mul(grey, 255u - a, rule);
}

/* PUNCH: 1 when the punch-through test passes at (x, y): texture alpha x, vertex alpha 255 - y, against `ref`. */
static inline int cmp_punch_pixel(unsigned x, unsigned y, unsigned ref, int rule) {
    return cmp_mul(x, 255u - y, rule) >= ref;
}

/* VOLUME: the quad's depth runs from CMP_VOL_Z0 at x = 0 to CMP_VOL_Z1 at x = 256; the box covers CMP_VOL_LO ..
   CMP_VOL_HI in x and y and CMP_VOL_ZA .. CMP_VOL_ZB in depth. 1 when the pixel (its centre) is inside the box. */
#define CMP_VOL_Z0 0.2f
#define CMP_VOL_Z1 0.8f
#define CMP_VOL_LO 64u
#define CMP_VOL_HI 192u
#define CMP_VOL_ZA 0.4f
#define CMP_VOL_ZB 0.6f

static inline float cmp_volume_depth(float x) { return CMP_VOL_Z0 + (CMP_VOL_Z1 - CMP_VOL_Z0) * x / (float)CMP_N; }

static inline int cmp_volume_pixel(unsigned x, unsigned y) {
    const float z = cmp_volume_depth((float)x + 0.5f);

    return x >= CMP_VOL_LO && x < CMP_VOL_HI && y >= CMP_VOL_LO && y < CMP_VOL_HI && z > CMP_VOL_ZA && z < CMP_VOL_ZB;
}

/* ---- what a test found ------------------------------------------------------------- */

typedef struct cmp_stats {
    unsigned rows;                         /* rows looked at */
    unsigned found;                        /* rows that have a boundary: lit on the left, shadow on the right */
    unsigned ragged;                       /* rows that change more than once (not one clean edge) */
    int min_err, max_err;                  /* the boundary's place against the ideal x = y + 1, in levels (found rows) */
    int sum_err;                           /* their sum: the mean is sum_err / found */
    unsigned lit, shadow, between;         /* pixels by value: lit, shadow, neither */
} cmp_stats_t;

/* Look at a 256 x 256 picture of 8-bit values (row y at img + y * stride) in which a lit pixel is `lit` and a shadow
   pixel `shadow` (a value within `tol` of either counts as it). For each row: where does the shadow begin? Rows y =
   255 has no shadow in the ideal picture and is left out. */
static inline void cmp_boundary(const uint8_t *img, unsigned stride, unsigned lit, unsigned shadow, unsigned tol, cmp_stats_t *st) {
    unsigned x, y;

    memset(st, 0, sizeof *st);
    for (y = 0; y < CMP_N; y++) {
        int first = -1, state = 0;                               /* state: 0 lit so far, 1 in shadow */
        unsigned changes = 0;

        for (x = 0; x < CMP_N; x++) {
            const unsigned v = img[y * stride + x];
            const int is_lit = (v > lit ? v - lit : lit - v) <= tol, is_sh = (v > shadow ? v - shadow : shadow - v) <= tol;

            if (is_lit) st->lit++; else if (is_sh) st->shadow++; else st->between++;
            if (is_sh && !state) { if (first < 0) first = (int)x; state = 1; changes++; }
            else if (is_lit && state) { state = 0; changes++; }
        }
        if (y == CMP_N - 1u) continue;
        st->rows++;
        if (changes > 1u) st->ragged++;
        if (first >= 0) {
            const int err = first - (int)(y + 1u);

            if (!st->found || err < st->min_err) st->min_err = err;
            if (!st->found || err > st->max_err) st->max_err = err;
            st->sum_err += err;
            st->found++;
        }
    }
}

/* Pixels of a 256 x 256 picture of 0 / 1 values that differ from a model's (also 0 / 1). */
static inline unsigned cmp_count_diff(const uint8_t *img, const uint8_t *model) {
    unsigned i, d = 0;

    for (i = 0; i < CMP_N * CMP_N; i++) d += (img[i] != 0) != (model[i] != 0);
    return d;
}

#endif /* CMP_MODEL_H */
