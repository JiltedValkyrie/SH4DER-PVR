/* Host test of sh4der_hexheat.h: the hexagons' lives, rise and lean; the shift's range and median; the mesh (every
   hexagon and its piece inside the band, whole pixels, the piece the hexagon moved by its shift, 1 : 1); the slip; the
   gate; the band's refusal. Prints the numbers set against the reference's. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "sh4der_hexheat.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static sh4der_hexheat_vtx_t m[SH4DER_HEXHEAT_N * 6u];
static float lives[200000];
static int cmpf(const void *a, const void *b) { return (*(const float *)a > *(const float *)b) - (*(const float *)a < *(const float *)b); }

int main(void) {
    const float dt = 1.0f / 60.0f, bw = (float)SH4DER_HEXHEAT_BAND_W, bh = (float)SH4DER_HEXHEAT_BAND_H;
    unsigned i, k, fr, nl = 0, alive_sum = 0, drawn_sum = 0, frames = 0, shifts[SH4DER_HEXHEAT_SHIFT_MAX + 1] = { 0 }, nshift = 0;
    sh4der_hexheat_t x;
    float x0, y0;

    /* lives: alive runs of each slot over 200 s at 60 a second; the rise and the lean while alive */
    for (i = 0; i < SH4DER_HEXHEAT_N; i++) {
        uint32_t life, was = 0u;
        float bx, up, up0 = 0.0f, t0 = 0.0f, bx0 = 0.0f;
        int on = 0;
        for (fr = 0; fr < 12000u; fr++) {
            const int a = sh4der_hexheat_at(i, (float)fr * dt, &life, &bx, &up);
            if (a && (!on || life != was)) { on = 1; was = life; t0 = (float)fr * dt; up0 = up; bx0 = bx; }
            else if (!a && on) { on = 0; if (nl < 200000u && t0 > 0.0f) lives[nl++] = (float)fr * dt - t0; }
            if (a) {
                const float age = (float)fr * dt - t0;
                CHECK(up > 0.0f && up < SH4DER_HEXHEAT_TOP + SH4DER_HEXHEAT_HEX_H + SH4DER_HEXHEAT_RISE * (SH4DER_HEXHEAT_LIFE_MIN + SH4DER_HEXHEAT_LIFE_SPAN) + 2.0f);
                CHECK(fabsf(up - up0 - SH4DER_HEXHEAT_RISE * age) <= 2.0f * SH4DER_HEXHEAT_WOBBLE + 0.1f);
                CHECK(fabsf(bx - bx0) <= SH4DER_HEXHEAT_LEAN * age + 2.0f * SH4DER_HEXHEAT_WOBBLE + 0.1f);
            }
        }
    }
    qsort(lives, nl, sizeof lives[0], cmpf);
    CHECK(nl > 1000u && lives[nl / 2u] > 1.4f && lives[nl / 2u] < 2.0f && lives[nl - 1u] <= SH4DER_HEXHEAT_LIFE_MIN + SH4DER_HEXHEAT_LIFE_SPAN + 0.05f);

    /* 600 frames with a slow turn: each drawn hexagon and its piece in the band, whole pixels, the piece 1 : 1 */
    CHECK(sh4der_hexheat_band(320.0f, 300.0f, &x0, &y0) && x0 == 192.0f && y0 == 300.0f + SH4DER_HEXHEAT_FOOT - bh);
    sh4der_hexheat_reset(&x);
    for (fr = 0; fr < 600u; fr++) {
        const float t = (float)fr * dt;
        unsigned n;
        sh4der_hexheat_slip(&x, 0.7f);
        n = sh4der_hexheat_mesh(&x, t, 1.0f, x0, y0, m);
        CHECK(n <= SH4DER_HEXHEAT_N);
        frames++; drawn_sum += n;
        for (i = 0; i < SH4DER_HEXHEAT_N; i++) { uint32_t l; float a, b; alive_sum += (unsigned)sh4der_hexheat_at(i, t, &l, &a, &b); }
        for (k = 0; k < n; k++) {
            const sh4der_hexheat_vtx_t *v = &m[k * 6u];
            const float dx = v[0].u - (v[0].x - x0), dy = v[0].v - (v[0].y - y0);
            unsigned j, big;
            CHECK(v[5].x - v[0].x == SH4DER_HEXHEAT_HEX_W && v[2].y - v[1].y == SH4DER_HEXHEAT_HEX_H);
            CHECK(v[0].x == floorf(v[0].x) && v[0].y == floorf(v[0].y) && dx == floorf(dx) && dy == floorf(dy));
            CHECK((dx != 0.0f || dy != 0.0f) && fabsf(dx) <= SH4DER_HEXHEAT_SHIFT_MAX && fabsf(dy) <= SH4DER_HEXHEAT_SHIFT_MAX);
            for (j = 0; j < 6u; j++) {
                CHECK(v[j].u - (v[j].x - x0) == dx && v[j].v - (v[j].y - y0) == dy);              /* one shift: 1 : 1 */
                CHECK(v[j].x >= x0 && v[j].x <= x0 + bw && v[j].y >= y0 && v[j].y <= y0 + bh);       /* the hexagon in the band */
                CHECK(v[j].u >= 0.0f && v[j].u <= bw && v[j].v >= 0.0f && v[j].v <= bh);             /* its piece too */
            }
            big = (unsigned)(fabsf(dx) > fabsf(dy) ? fabsf(dx) : fabsf(dy));
            shifts[big]++; nshift++;
        }
    }
    CHECK(drawn_sum > 0u && fabsf(x.scroll - 600.0f * 0.7f) < 0.5f);
    {
        unsigned acc = 0, med = 0;
        for (k = 0; k <= SH4DER_HEXHEAT_SHIFT_MAX; k++) { acc += shifts[k]; if (acc * 2u >= nshift) { med = k; break; } }
        CHECK(shifts[0] == 0u && med >= 2u && med <= 3u);
        printf("shift: 1 .. %d px, median %u (the reference: 1 to 5, median 2)\n", SH4DER_HEXHEAT_SHIFT_MAX, med);
    }
    printf("life: median %.2f s, longest %.2f s (the reference: 1.7 s)\n", lives[nl / 2u], lives[nl - 1u]);
    printf("hexagons: %u slots, %.1f alive a frame, %.1f drawn a frame (%.0f strip vertices, %.1f KB of TA with the header); worst %u vertices\n",
           SH4DER_HEXHEAT_N, (float)alive_sum / (float)frames, (float)drawn_sum / (float)frames, 6.0f * (float)drawn_sum / (float)frames,
           (6.0f * (float)drawn_sum / (float)frames + 1.0f) * 32.0f / 1024.0f, SH4DER_HEXHEAT_N * 6u);

    /* a slip that is a cut (too large, not a number) is not summed */
    {
        const float before = x.scroll;
        sh4der_hexheat_slip(&x, 5000.0f); sh4der_hexheat_slip(&x, NAN);
        CHECK(x.scroll == before);
    }
    /* the gate: closed, nothing; half, only the first half of the slots (each hexagon's piece names its slot by place) */
    CHECK(sh4der_hexheat_mesh(&x, 3.0f, 0.0f, x0, y0, m) == 0u && sh4der_hexheat_mesh(&x, 3.0f, NAN, x0, y0, m) == 0u);
    {
        unsigned full = 0, half = 0;
        for (fr = 0; fr < 600u; fr++) {
            full += sh4der_hexheat_mesh(&x, (float)fr * dt, 1.0f, x0, y0, m);
            half += sh4der_hexheat_mesh(&x, (float)fr * dt, 0.5f, x0, y0, m);
        }
        CHECK(half > 0u && half < full && half * 10u > full * 3u && half * 10u < full * 7u);
    }
    /* an anchor no screen holds: no band */
    CHECK(!sh4der_hexheat_band(NAN, 0.0f, &x0, &y0) && !sh4der_hexheat_band(0.0f, 1e9f, &x0, &y0));

    printf(fails ? "hexheat_test: %d FAILED\n" : "hexheat_test: ok\n", fails);
    return fails != 0;
}
