/* SH4DER-PVR: R5 soft particles against a known plane, host check

   demo/psm/recipes/recipe_softplane_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A sprite vertex's alpha against a plane: 0 at or under it, 1 a radius over it, rising between, against a double
   reference; an upright sprite crossing the ground, cut 1 to 8 cells high, its alpha interpolated between vertex
   rows as the PVR does across a triangle, against the exact per-pixel fade: the error and the alpha left on the
   contact line fall as the cut gets finer; two planes multiply. Prints PASS and FAIL counts.
*/

#include <math.h>
#include <stdio.h>
#include "recipes.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.00002f
#endif

static unsigned passes, fails;
#define CHECK(c) do { if (c) passes++; else { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static double smooth_ref(double s, double r) {
    double t;
    if (s <= 0.0) return 0.0;
    if (s >= r) return 1.0;
    t = s / r;
    return t * t * (3.0 - 2.0 * t);
}

static const float UP[3] = { 0.0f, 1.0f, 0.0f };

/* 1, 2. The fade against the reference, its ends, and that it rises. */
static void t_fade(void) {
    const float r = 0.8f;
    float worst = 0.0f, prev = -1.0f;
    int rising = 1, ends = 1;
    unsigned i;
    for (i = 0; i <= 4000u; i++) {
        const float y = -1.0f + 3.0f * (float)i / 4000.0f, p[3] = { 3.0f, y + 0.05f, -2.0f };
        const float a = rcp_soft_alpha(p, UP, 0.05f, r), e = (float)smooth_ref((double)y, r);   /* the plane is y = 0.05 */
        if (fabsf(a - e) > worst) worst = fabsf(a - e);
        if (a < prev) rising = 0;
        if ((y <= 0.0f && a != 0.0f) || (y >= r && a != 1.0f)) ends = 0;
        prev = a;
    }
    printf("R5 fade against the reference: worst %.2e\n", (double)worst);
    CHECK(worst <= SH4DER_TEST_TOL * 25.0f);
    CHECK(rising);
    CHECK(ends);
}

/* 3. An upright sprite of radius r, centre at height hc over the ground, cut into g rows of cells: alpha at the
   vertex rows, linear between (the quad is upright, so alpha changes only with height). */
static void t_cut(void) {
    static const unsigned gs[4] = { 1u, 2u, 4u, 8u };
    static const float hcs[3] = { -0.4f, 0.0f, 0.4f };
    const float r = 1.0f;
    float err[4], line[4];
    unsigned a, b, i, falls = 1;
    for (a = 0; a < 4u; a++) { err[a] = 0.0f; line[a] = 0.0f; }
    for (b = 0; b < 3u; b++) {
        for (a = 0; a < 4u; a++) {
            const unsigned g = gs[a];
            float row[9];
            for (i = 0; i <= g; i++) {
                const float p[3] = { 0.0f, hcs[b] - r + 2.0f * r * (float)i / (float)g, 0.0f };
                row[i] = rcp_soft_alpha(p, UP, 0.0f, r);
            }
            for (i = 0; i < 2000u; i++) {          /* pixel rows up the sprite */
                const float f = ((float)i + 0.5f) / 2000.0f, y = hcs[b] - r + 2.0f * r * f;
                const float c = f * (float)g;
                const unsigned k = (unsigned)c < g ? (unsigned)c : g - 1u;
                const float v = row[k] + (row[k + 1u] - row[k]) * (c - (float)k);
                const float d = fabsf(v - (float)smooth_ref((double)y, r));
                if (d > err[a]) err[a] = d;
                if (fabsf(y) < r / 2000.0f && v > line[a]) line[a] = v;   /* the pixel row on the contact line */
            }
        }
    }
    for (a = 0; a < 4u; a++) {
        printf("R5 sprite cut %u row(s) high (%u vertices): worst alpha error %.3f, alpha on the contact line up to %.3f\n",
               gs[a], 2u * (gs[a] + 1u), (double)err[a], (double)line[a]);
        if (a && !(err[a] < err[a - 1u])) falls = 0;
    }
    CHECK(falls);
    CHECK(line[3] <= line[0]);
}

/* 4. Ground and a wall: the factors multiply, so a corner fades by both. */
static void t_two(void) {
    const float wall[3] = { -1.0f, 0.0f, 0.0f };   /* the wall x = 2, open side toward -x: dot(n, p) = d with d = -2 */
    unsigned i, ok = 1;
    for (i = 0; i < 1000u; i++) {
        const float p[3] = { 2.0f - 1.2f * (float)(i % 40u) / 40.0f, 1.2f * (float)(i / 40u) / 25.0f, 0.0f };
        const float ag = rcp_soft_alpha(p, UP, 0.0f, 1.0f), aw = rcp_soft_alpha(p, wall, -2.0f, 1.0f), both = ag * aw;
        if (both > ag + 1e-6f || both > aw + 1e-6f || both < 0.0f) ok = 0;
    }
    CHECK(ok);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    t_fade();
    t_cut();
    t_two();
    printf("recipe_softplane_test: PASS %u FAIL %u\n", passes, fails);
    return fails ? 1 : 0;
}
