/* SH4DER-PVR: the R3 page (the punch-through product), what the page works out from its pixels

   demo/psm/compare/r3_model.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Shared by the console program (r3_spike.c) and the host test (test/r3_model_test.c). The page is specified in
   ../recipes/R3_PAGE_SPEC.md; the candidate rules are ../recipes/recipes.h's (rcp_pt_pass). Nothing here is measured:
   it is the arithmetic the page does on what it reads back.

   A line of the page is one quad of 256 x 128 pixels through a 4444 texture whose alpha nibble is column / 16, drawn
   in the punch-through list with one vertex alpha a and one reference: the columns whose product passes are drawn.
*/

#ifndef R3_MODEL_H
#define R3_MODEL_H

#include "../recipes/recipes.h"

#define R3_W 256u
#define R3_H 128u
#define R3_BG 0x001fu                       /* pure blue: what the pass leaves where it drew nothing */

typedef struct r3_line {
    unsigned px;                            /* drawn pixels of the 256 x 128 */
    unsigned n;                             /* px / 2048: the nibbles drawn (16 columns x 128 rows a nibble) */
    unsigned lo;                            /* the lowest drawn column of row 64, / 16; 16 when the row is empty */
    unsigned step;                          /* 1: every row is drawn exactly from column lo x 16 to 255 */
} r3_line_t;

/* px: R3_H rows of R3_W pixels, 565. */
static inline void r3_derive(const uint16_t *px, r3_line_t *out) {
    unsigned x, y, first = R3_W;

    out->px = 0; out->step = 1u;
    for (x = 0; x < R3_W; x++) if (px[64u * R3_W + x] != R3_BG) { first = x; break; }
    out->lo = first / 16u;
    if (first % 16u) out->step = 0u;
    for (y = 0; y < R3_H; y++)
        for (x = 0; x < R3_W; x++) {
            const unsigned drawn = px[y * R3_W + x] != R3_BG;

            out->px += drawn;
            if (drawn != (x >= first)) out->step = 0u;
        }
    out->n = out->px / 2048u;
}

/* The nibbles a rule draws at vertex alpha a and this reference (the 4-bit alpha widened as k x 17). */
static inline unsigned r3_model_n(unsigned a, unsigned ref, unsigned rule) {
    unsigned k, n = 0;

    for (k = 0; k < 16u; k++) n += (unsigned)rcp_pt_pass(k, a, ref, RCP_EXP_REPLICATE, rule);
    return n;
}

/* The deciding lines: vertex alpha and reference. */
enum { R3_A, R3_B, R3_C, R3_D, R3_F, R3_C2, R3_D2, R3_LINES };
static const uint8_t r3_alpha[R3_LINES] = { 255, 128, 40, 174, 16, 40, 174 };
static const uint8_t r3_ref[R3_LINES] = { 128, 128, 16, 128, 16, 16, 128 };

/* The rule the lines C, D and F name: RCP_PT_VTX256 .. RCP_PT_FLOOR, or RCP_PT_N for none of the four. */
static inline unsigned r3_rule(unsigned nc, unsigned nd, unsigned nf) {
    unsigned r;

    for (r = RCP_PT_VTX256; r <= RCP_PT_FLOOR; r++)
        if (r3_model_n(40u, 16u, r) == nc && r3_model_n(174u, 128u, r) == nd && r3_model_n(16u, 16u, r) == nf) return r;
    return RCP_PT_N;
}

static inline const char *r3_rule_name(unsigned rule) {
    static const char *const name[RCP_PT_N + 1] = { "PLAIN", "VTX256", "BOTH256", "ROUND", "FLOOR", "NO RULE" };

    return name[rule <= RCP_PT_N ? rule : RCP_PT_N];
}

/* 1 when the run counts: A and B repeat c4 (16,384 and 2,048 pixels), every line is a clean step, C2 and D2 repeat
   C and D. */
static inline int r3_valid(const r3_line_t l[R3_LINES]) {
    unsigned k;

    for (k = 0; k < R3_LINES; k++) if (!l[k].step) return 0;
    return l[R3_A].px == 16384u && l[R3_B].px == 2048u && l[R3_C2].px == l[R3_C].px && l[R3_D2].px == l[R3_D].px;
}

/* Page 2: the fade curve at reference 16. Vertex alpha of row i of 33: 0, 8 .. 248, 255. */
#define R3_FADE 33u
static inline unsigned r3_fade_alpha(unsigned i) { return i < 32u ? 8u * i : 255u; }

#endif /* R3_MODEL_H */
