/* Host test of the street demo's hexagon glue (src/heat_hex.h): a turn moves the hexagons with the far picture (the
   library's scroll is heat.h's slip, summed); the first view after a reset adds nothing. The hexagons themselves are
   test/hexheat_test.c's. Plain C:
   gcc -std=c11 -O2 -Wall -Wextra -Werror -DSH4DER_CORE_NO_SH4ZAM -Iinclude -Ish4dow-dc/include demo/pl4nar/host/heat_hex_test.c -lm */
#include <math.h>
#include <stdio.h>

#include "../src/heat_hex.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

int main(void) {
    const float f0[3] = { 0.0f, 0.0f, 1.0f };
    float f1[3] = { 0.02f, 0.0f, 1.0f }, s[2];
    hexh_t x;

    hexh_reset(&x);
    hexh_view(&x, f1);                                 /* the first view: nothing to compare with */
    CHECK(x.frame == 1u && x.h.scroll == 0.0f);
    hexh_view(&x, f1);                                 /* still: no travel */
    CHECK(fabsf(x.h.scroll) < 1e-4f);
    sh4der_norm3_(f1);
    heat_slip(f0, f1, HEAT_FOCAL, s);
    hexh_reset(&x);
    hexh_view(&x, f0); hexh_view(&x, f1);              /* a turn: the scroll is the slip */
    CHECK(fabsf(x.h.scroll - s[0]) < 1e-3f && fabsf(s[0]) > 5.0f);
    CHECK(HEXH_NEAR_BACK > 0.0f && HEXH_NEAR_BACK < HEAT_DIST);

    printf(fails ? "heat_hex_test: %d FAILED\n" : "heat_hex_test: ok\n", fails);
    return fails != 0;
}
