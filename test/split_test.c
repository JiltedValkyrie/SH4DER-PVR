/* SH4DER-PVR: split divider test (the column maths, include/sh4der_split.h)

   test/split_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Every place of the divider: the two halves cover every tile column once and no column twice, a hidden half is
   flagged, the rectangles are valid user clips (first <= last, inside the screen); the trigger stepping (one step a
   press, a repeat while held, both triggers no move, the ends hold); the labels' places.

   Compile: cc -std=c11 -Wall -Wextra -Werror -Iinclude test/split_test.c -o split_test */

#include <stdio.h>

#include "../include/sh4der_split.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    sh4der_split_half_t l, r;
    sh4der_split_step_t s = { 0, 0u };
    unsigned d, x, i;

    CHECK(sh4der_split_middle(SH4DER_SPLIT_COLS) == 10u && SH4DER_SPLIT_COLS * SH4DER_SPLIT_TILE == 640u && SH4DER_SPLIT_ROWS * SH4DER_SPLIT_TILE == 480u);
    for (d = 0; d <= SH4DER_SPLIT_COLS; d++) {
        unsigned cover[SH4DER_SPLIT_COLS] = { 0 };

        sh4der_split_halves(d, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &l, &r);
        CHECK(l.hidden == (d == 0u) && r.hidden == (d == SH4DER_SPLIT_COLS));
        CHECK(sh4der_split_at_end(d, SH4DER_SPLIT_COLS) == (l.hidden || r.hidden));
        if (!l.hidden) { CHECK(l.tx0 == 0u && l.tx1 == d - 1u && l.ty0 == 0u && l.ty1 == 14u); for (x = l.tx0; x <= l.tx1; x++) cover[x]++; }
        if (!r.hidden) { CHECK(r.tx0 == d && r.tx1 == 19u && r.ty0 == 0u && r.ty1 == 14u); for (x = r.tx0; x <= r.tx1; x++) cover[x]++; }
        for (x = 0; x < SH4DER_SPLIT_COLS; x++) CHECK(cover[x] == 1u);
        CHECK(l.tx1 < SH4DER_SPLIT_COLS && r.tx0 < SH4DER_SPLIT_COLS && r.tx1 < SH4DER_SPLIT_COLS);   /* even a hidden half holds nothing off the screen */
        CHECK(sh4der_split_x(d) == (float)(d * 32u));
    }
    sh4der_split_halves(99u, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &l, &r);                           /* held to the end */
    CHECK(r.hidden && !l.hidden && l.tx1 == 19u);
    sh4der_split_halves(10u, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &l, &r);
    CHECK(l.tx1 == 9u && r.tx0 == 10u);                                          /* the middle: ao4's fixed split */

    /* the triggers */
    d = 10u;
    d = sh4der_split_step(&s, d, SH4DER_SPLIT_COLS, 0, 0); CHECK(d == 10u);
    d = sh4der_split_step(&s, d, SH4DER_SPLIT_COLS, 0, 1); CHECK(d == 11u);                    /* the press steps at once */
    for (i = 0; i < SH4DER_SPLIT_REPEAT - 1u; i++) { d = sh4der_split_step(&s, d, SH4DER_SPLIT_COLS, 0, 1); CHECK(d == 11u); }
    d = sh4der_split_step(&s, d, SH4DER_SPLIT_COLS, 0, 1); CHECK(d == 12u);                    /* then one every SH4DER_SPLIT_REPEAT frames */
    for (i = 0; i < SH4DER_SPLIT_REPEAT; i++) d = sh4der_split_step(&s, d, SH4DER_SPLIT_COLS, 0, 1);
    CHECK(d == 13u);
    d = sh4der_split_step(&s, d, SH4DER_SPLIT_COLS, 1, 1); CHECK(d == 13u);                    /* both: no move */
    for (i = 0; i < 20u; i++) d = sh4der_split_step(&s, d, SH4DER_SPLIT_COLS, 1, 1);
    CHECK(d == 13u);
    d = sh4der_split_step(&s, d, SH4DER_SPLIT_COLS, 1, 0); CHECK(d == 12u);                    /* one let go: that is a press of the other */
    d = sh4der_split_step(&s, d, SH4DER_SPLIT_COLS, 0, 0); CHECK(d == 12u);
    d = sh4der_split_step(&s, d, SH4DER_SPLIT_COLS, 1, 0); CHECK(d == 11u);                    /* tap, tap */
    d = sh4der_split_step(&s, d, SH4DER_SPLIT_COLS, 0, 0);
    d = sh4der_split_step(&s, d, SH4DER_SPLIT_COLS, 1, 0); CHECK(d == 10u);
    for (i = 0; i < 200u; i++) d = sh4der_split_step(&s, d, SH4DER_SPLIT_COLS, 1, 0);
    CHECK(d == 0u);                                                              /* the end holds */
    d = sh4der_split_step(&s, d, SH4DER_SPLIT_COLS, 0, 0);
    for (i = 0, x = 0; d < SH4DER_SPLIT_COLS && i < 1000u; i++) { d = sh4der_split_step(&s, d, SH4DER_SPLIT_COLS, 0, 1); x++; }
    CHECK(d == SH4DER_SPLIT_COLS && x == 1u + 19u * SH4DER_SPLIT_REPEAT);                      /* the whole way: 115 frames, under 2 s */
    for (i = 0; i < 50u; i++) d = sh4der_split_step(&s, d, SH4DER_SPLIT_COLS, 0, 1);
    CHECK(d == SH4DER_SPLIT_COLS);
    CHECK(sh4der_split_step(&s, 99u, SH4DER_SPLIT_COLS, 0, 0) == SH4DER_SPLIT_COLS);

    /* the labels: centred on the half, inside 64 .. 576, gone when there is no room */
    sh4der_split_halves(10u, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &l, &r);
    CHECK(sh4der_split_label_x(&l, 128u) == 64 + (320 - 64 - 128) / 2 && sh4der_split_label_x(&r, 128u) == 320 + (576 - 320 - 128) / 2);
    sh4der_split_halves(3u, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &l, &r);
    CHECK(sh4der_split_label_x(&l, 128u) == -1 && sh4der_split_label_x(&l, 32u) == 64 && sh4der_split_label_x(&r, 128u) == 96 + (576 - 96 - 128) / 2);
    sh4der_split_halves(1u, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &l, &r);
    CHECK(sh4der_split_label_x(&l, 8u) == -1);                                          /* the first two columns are outside the safe area */
    sh4der_split_halves(0u, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &l, &r);
    CHECK(sh4der_split_label_x(&l, 8u) == -1 && sh4der_split_label_x(&r, 512u) == 64);
    sh4der_split_halves(20u, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &l, &r);
    CHECK(sh4der_split_label_x(&r, 8u) == -1 && sh4der_split_label_x(&l, 512u) == 64 && sh4der_split_label_x(&l, 513u) == -1);

    if (fails) printf("split_test: %d FAILED\n", fails); else printf("split_test: ok\n");
    return fails ? 1 : 0;
}
