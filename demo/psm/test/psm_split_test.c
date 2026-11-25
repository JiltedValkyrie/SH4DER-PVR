/* SH4DER-PVR: host test of the movable split's column maths (demo/psm/psm_split.h)

   demo/psm/test/psm_split_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Every place of the divider: the two halves cover every tile column once and no column twice, a hidden half is
   flagged, the rectangles are valid user clips (first <= last, inside the screen); the trigger stepping (one step a
   press, a repeat while held, both triggers no move, the ends hold); the labels' places.

   Compile: cc -std=c11 -Wall -Wextra -Werror -Idemo/psm demo/psm/test/psm_split_test.c -o psm_split_test */

#include <stdio.h>

#include "../psm_split.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    split_half_t l, r;
    split_step_t s = { 0, 0u };
    unsigned d, x, i;

    CHECK(split_middle(SPLIT_COLS) == 10u && SPLIT_COLS * SPLIT_TILE == 640u && SPLIT_ROWS * SPLIT_TILE == 480u);
    for (d = 0; d <= SPLIT_COLS; d++) {
        unsigned cover[SPLIT_COLS] = { 0 };

        split_halves(d, SPLIT_COLS, SPLIT_ROWS, &l, &r);
        CHECK(l.hidden == (d == 0u) && r.hidden == (d == SPLIT_COLS));
        CHECK(split_at_end(d, SPLIT_COLS) == (l.hidden || r.hidden));
        if (!l.hidden) { CHECK(l.tx0 == 0u && l.tx1 == d - 1u && l.ty0 == 0u && l.ty1 == 14u); for (x = l.tx0; x <= l.tx1; x++) cover[x]++; }
        if (!r.hidden) { CHECK(r.tx0 == d && r.tx1 == 19u && r.ty0 == 0u && r.ty1 == 14u); for (x = r.tx0; x <= r.tx1; x++) cover[x]++; }
        for (x = 0; x < SPLIT_COLS; x++) CHECK(cover[x] == 1u);
        CHECK(l.tx1 < SPLIT_COLS && r.tx0 < SPLIT_COLS && r.tx1 < SPLIT_COLS);   /* even a hidden half holds nothing off the screen */
        CHECK(split_x(d) == (float)(d * 32u));
    }
    split_halves(99u, SPLIT_COLS, SPLIT_ROWS, &l, &r);                           /* held to the end */
    CHECK(r.hidden && !l.hidden && l.tx1 == 19u);
    split_halves(10u, SPLIT_COLS, SPLIT_ROWS, &l, &r);
    CHECK(l.tx1 == 9u && r.tx0 == 10u);                                          /* the middle: ao4's fixed split */

    /* the triggers */
    d = 10u;
    d = split_step(&s, d, SPLIT_COLS, 0, 0); CHECK(d == 10u);
    d = split_step(&s, d, SPLIT_COLS, 0, 1); CHECK(d == 11u);                    /* the press steps at once */
    for (i = 0; i < SPLIT_REPEAT - 1u; i++) { d = split_step(&s, d, SPLIT_COLS, 0, 1); CHECK(d == 11u); }
    d = split_step(&s, d, SPLIT_COLS, 0, 1); CHECK(d == 12u);                    /* then one every SPLIT_REPEAT frames */
    for (i = 0; i < SPLIT_REPEAT; i++) d = split_step(&s, d, SPLIT_COLS, 0, 1);
    CHECK(d == 13u);
    d = split_step(&s, d, SPLIT_COLS, 1, 1); CHECK(d == 13u);                    /* both: no move */
    for (i = 0; i < 20u; i++) d = split_step(&s, d, SPLIT_COLS, 1, 1);
    CHECK(d == 13u);
    d = split_step(&s, d, SPLIT_COLS, 1, 0); CHECK(d == 12u);                    /* one let go: that is a press of the other */
    d = split_step(&s, d, SPLIT_COLS, 0, 0); CHECK(d == 12u);
    d = split_step(&s, d, SPLIT_COLS, 1, 0); CHECK(d == 11u);                    /* tap, tap */
    d = split_step(&s, d, SPLIT_COLS, 0, 0);
    d = split_step(&s, d, SPLIT_COLS, 1, 0); CHECK(d == 10u);
    for (i = 0; i < 200u; i++) d = split_step(&s, d, SPLIT_COLS, 1, 0);
    CHECK(d == 0u);                                                              /* the end holds */
    d = split_step(&s, d, SPLIT_COLS, 0, 0);
    for (i = 0, x = 0; d < SPLIT_COLS && i < 1000u; i++) { d = split_step(&s, d, SPLIT_COLS, 0, 1); x++; }
    CHECK(d == SPLIT_COLS && x == 1u + 19u * SPLIT_REPEAT);                      /* the whole way: 115 frames, under 2 s */
    for (i = 0; i < 50u; i++) d = split_step(&s, d, SPLIT_COLS, 0, 1);
    CHECK(d == SPLIT_COLS);
    CHECK(split_step(&s, 99u, SPLIT_COLS, 0, 0) == SPLIT_COLS);

    /* the labels: centred on the half, inside 64 .. 576, gone when there is no room */
    split_halves(10u, SPLIT_COLS, SPLIT_ROWS, &l, &r);
    CHECK(split_label_x(&l, 128u) == 64 + (320 - 64 - 128) / 2 && split_label_x(&r, 128u) == 320 + (576 - 320 - 128) / 2);
    split_halves(3u, SPLIT_COLS, SPLIT_ROWS, &l, &r);
    CHECK(split_label_x(&l, 128u) == -1 && split_label_x(&l, 32u) == 64 && split_label_x(&r, 128u) == 96 + (576 - 96 - 128) / 2);
    split_halves(1u, SPLIT_COLS, SPLIT_ROWS, &l, &r);
    CHECK(split_label_x(&l, 8u) == -1);                                          /* the first two columns are outside the safe area */
    split_halves(0u, SPLIT_COLS, SPLIT_ROWS, &l, &r);
    CHECK(split_label_x(&l, 8u) == -1 && split_label_x(&r, 512u) == 64);
    split_halves(20u, SPLIT_COLS, SPLIT_ROWS, &l, &r);
    CHECK(split_label_x(&r, 8u) == -1 && split_label_x(&l, 512u) == 64 && split_label_x(&l, 513u) == -1);

    if (fails) printf("psm_split_test: %d FAILED\n", fails); else printf("psm_split_test: ok\n");
    return fails ? 1 : 0;
}
