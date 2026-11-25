/* SH4DER-PVR: the subtract spike, the models

   demo/psm/compare/sub_model.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Spike c1 showed the PVR can compare a map texture M with an interpolated receiver depth R by blending, exact at
   8 bits, in about 11 layers, and that a map the PVR draws itself reaches that compare with 6 bits. This file holds
   the plain C MODELS of three ways to do better. Models, not measurements: the console's numbers are compared with
   them.

     BUMP    a bump texel is two bytes, S (elevation) and R (azimuth); the PVR's bump unit gives the pixel
                 K1 + K2 sin(S) + K3 cos(S) cos(R - Q)
             with K1, K2, K3 and Q bytes of the vertex's offset colour. With S = 0, K1 = 128, K3 = 127, the texel's
             R = M - 64 and Q = the receiver's depth, the pixel is 128 + 127 sin(2 pi (M - Rcv) / 256): 128 or more
             exactly where M >= Rcv, for differences up to half a turn. One layer, and the texel is a whole byte.
     LEX     two digits: depth = high x 2^bits + low, each digit a grey level. Shadow where the high digit of R is
             greater, or the high digits are equal and the low digit of R is greater. Built from c1's saturating
             differences, one multiply and one add.
     DIAG    the test picture's reader: M runs down y, R across x, so the shadow begins at x = y + 1; a cyclic
             compare is right only near the diagonal, so only a window round it is read.
*/

#ifndef SUB_MODEL_H
#define SUB_MODEL_H

#include <math.h>

#include "cmp_model.h"

#define SUB_K1 128u
#define SUB_K3 127u
#define SUB_TURN 64u                       /* a quarter turn in azimuth bytes */
#define SUB_WINDOW 100u                    /* columns each side of the diagonal that are read */

/* The bump unit's level for azimuth byte r and Q byte q, S = 0: K1 + K3 cos(2 pi (r - q) / 256), as real arithmetic
   rounded to a byte. What the PVR's own arithmetic gives is the console's to say. */
static inline unsigned sub_bump_level(unsigned r, unsigned q) {
    const float v = (float)SUB_K1 + (float)SUB_K3 * cosf(6.2831853f * (float)((int)r - (int)q) / 256.0f);

    return v <= 0.0f ? 0u : v >= 255.0f ? 255u : (unsigned)(v + 0.5f);
}

/* The azimuth byte that stands for map depth m: m less a quarter turn, so the cosine becomes the sine of M - Rcv. */
static inline unsigned sub_bump_texel(unsigned m) { return (m - SUB_TURN) & 255u; }

/* A digit d of `bits` bits as the 8-bit level a texture of that many bits gives. */
static inline unsigned sub_digit_level(unsigned d, unsigned bits) { return bits >= 8u ? d & 255u : cmp_map_level(d << (8u - bits), bits); }

/* c1's saturating difference of two levels: a - b where that is positive, else 0. */
static inline unsigned sub_diff(unsigned a, unsigned b, int rule) { return cmp_mul(255u, 255u - cmp_sat((255u - a) + b), rule); }

/* LEX: 1 when the two-digit compare says shadow for receiver depth r and map depth m (both 2 x bits bits). `thr`:
   the smallest difference that counts as one (1 ideal; 2 if a difference of exactly 1 is lost, as c1's +1 may be). */
static inline int sub_lex_shadow(unsigned r, unsigned m, unsigned bits, unsigned thr, int rule) {
    const unsigned mask = (1u << bits) - 1u;
    const unsigned rh = sub_digit_level(r >> bits, bits), rl = sub_digit_level(r & mask, bits);
    const unsigned mh = sub_digit_level(m >> bits, bits), ml = sub_digit_level(m & mask, bits);
    const unsigned dh = sub_diff(rh, mh, rule) >= thr ? 255u : 0u;       /* high digit of R greater */
    const unsigned eh = sub_diff(mh, rh, rule) >= thr ? 255u : 0u;       /* high digit of M greater */
    const unsigned dl = sub_diff(rl, ml, rule) >= thr ? 255u : 0u;       /* low digit of R greater */

    return cmp_sat(dh + cmp_mul(dl, 255u - eh, rule)) >= 128u;
}

/* DIAG: like cmp_boundary, but each row is read only from SUB_WINDOW columns left of the diagonal to SUB_WINDOW
   right of it (and never column 0, which the test quad does not cover). */
static inline void sub_diag(const uint8_t *img, unsigned lit, unsigned shadow, unsigned tol, cmp_stats_t *st) {
    unsigned x, y;

    memset(st, 0, sizeof *st);
    for (y = 0; y + 1u < CMP_N; y++) {
        const unsigned x0 = y > SUB_WINDOW ? y - SUB_WINDOW : 1u, x1 = y + SUB_WINDOW < CMP_N ? y + SUB_WINDOW : CMP_N - 1u;
        int first = -1, state = 0;
        unsigned changes = 0;

        for (x = x0; x <= x1; x++) {
            const unsigned v = img[y * CMP_N + x];
            const int is_lit = (v > lit ? v - lit : lit - v) <= tol, is_sh = (v > shadow ? v - shadow : shadow - v) <= tol;

            if (is_lit) st->lit++; else if (is_sh) st->shadow++; else st->between++;
            if (is_sh && !state) { if (first < 0) first = (int)x; state = 1; changes++; }
            else if (is_lit && state) { state = 0; changes++; }
        }
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

#endif /* SUB_MODEL_H */
