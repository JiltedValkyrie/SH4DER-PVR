/* SH4DER-PVR: the ray bench's host test. Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   ray_kernels.h on the host, on SH4ZAM's software back end, before any console runs it:
   1 the walk: the kernel gives the reference's bytes, on the town grid, at 14:00, 16:00 and 96 other suns
   2 the walk on an empty grid: its cell count against THESIS 5.3's n = (L / c)(|cos a'| + |sin a'|), over azimuths
   3 the walk against one wall: the hit is on the cell the geometry says
   4 the proxies: the kernel (early outs) gives the reference's bytes; a person's shadow lands where its run says
   5 what the console bench will find: rays of each n among the stand-in points (P1's buckets), the slice's mean n and
     share shadowed (P2), the receivers in the proxies' runs at 16:00 (P9)
   Numbers here are the host's: they size the console's pages, they are not timings and not evidence of the console.

   Build (from the repository's root):
     gcc -std=c11 -O2 -Wall -Wextra -I third_party/sh4zam/include demo/psm/bench/ray_test.c -lm -o build/ray_test
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ray_kernels.h"

static unsigned fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define K 262144u
static float pts[K][4];
static uint8_t out_a[K], out_b[K], steps[K];
static uint8_t grid0[RAY_N][RAY_N];

static void t_bytes(void) {
    static const float az[2] = { TOWN_RAY_SUN14_AZ, TOWN_RAY_SUN16_AZ }, el[2] = { TOWN_RAY_SUN14_EL, TOWN_RAY_SUN16_EL };
    unsigned k, r, differ = 0;
    ray_sun_t s;

    ray_points(pts, K, 1234u);
    for (k = 0; k < 2u + 96u; k++) {
        const float a = k < 2u ? az[k] : (float)(k - 2u) * 3.75f + 0.3f, e = k < 2u ? el[k] : 5.0f + (float)((k * 7u) % 60u);

        ray_sun_init(&s, a, e);
        ray_slice_ref(town_ray_h, &s, (const float (*)[4])pts, out_a, NULL, k < 2u ? K : 8192u);
        ray_slice(&town_ray_h[0][0], &s, (const float (*)[4])pts, out_b, k < 2u ? K : 8192u);
        for (r = 0; r < (k < 2u ? K : 8192u); r++) differ += out_a[r] != out_b[r];
    }
    CHECK(differ == 0u, "walk: %u bytes differ between the kernel and the reference", differ);
    printf("1 walk: kernel == reference, %u points at 14:00 and 16:00, 8192 at 96 other suns (%u bytes differ)\n", K, differ);
}

static void t_empty(void) {
    const float el = 60.0f, L = TOWN_RAY_H / tanf(el * 3.14159265f / 180.0f);   /* from y = 0: clear at H */
    double sum_n = 0.0, sum_f = 0.0, worst = 0.0;
    uint32_t seed = 99u;
    unsigned r, n;
    ray_sun_t s;

    for (r = 0; r < 20000u; r++) {
        const float a = ray_frand(&seed, 0.0f, 360.0f), x = ray_frand(&seed, -20.0f, 0.0f), z = ray_frand(&seed, 0.0f, 20.0f);
        double f, d;

        ray_sun_init(&s, a, el);
        CHECK(ray_walk_ref(grid0, &s, x, 0.0f, z, &n) == RAY_LIT, "empty grid shadowed a point");
        f = (L / TOWN_RAY_CELL) * (fabs(s.sx) + fabs(s.sz)) / cos(el * 3.14159265 / 180.0);
        d = (double)n - 1.0 - f;                     /* n counts the start cell: crossings = n - 1 */
        if (fabs(d) > worst) worst = fabs(d);
        sum_n += n - 1.0; sum_f += f;
    }
    CHECK(worst <= 2.0, "empty grid: a walk is %.2f crossings from 5.3's n", worst);
    CHECK(fabs(sum_n / sum_f - 1.0) < 0.02, "empty grid: mean crossings %.3f of 5.3's", sum_n / sum_f);
    printf("2 empty grid, e 60: mean crossings %.2f, 5.3's n %.2f (ratio %.4f), worst ray %.2f off\n", sum_n / 20000.0, sum_f / 20000.0, sum_n / sum_f, worst);
}

static void t_wall(void) {
    static uint8_t g[RAY_N][RAY_N];
    unsigned n, lit;
    ray_sun_t s;

    g[32][40] = 255u;
    ray_sun_init(&s, 90.0f, 10.0f);                  /* toward the sun: +x */
    lit = ray_walk_ref(g, &s, TOWN_RAY_X0 + 30.5f * TOWN_RAY_CELL, 0.05f, TOWN_RAY_Z0 + 32.5f * TOWN_RAY_CELL, &n);
    CHECK(lit == 0u && n == 11u, "wall 10 cells east: lit %u after %u cells (want 0 after 11)", lit, n);
    lit = ray_walk(&g[0][0], &s, TOWN_RAY_X0 + 30.5f * TOWN_RAY_CELL, 0.05f, TOWN_RAY_Z0 + 32.5f * TOWN_RAY_CELL);
    CHECK(lit == 0u, "wall: the kernel says %u", lit);
    lit = ray_walk_ref(g, &s, TOWN_RAY_X0 + 30.5f * TOWN_RAY_CELL, 0.05f, TOWN_RAY_Z0 + 33.5f * TOWN_RAY_CELL, &n);
    CHECK(lit == RAY_LIT, "one row over the wall is not in the way: lit %u", lit);
    printf("3 wall: hit on the 11th cell; the next row lit\n");
}

static ray_proxy_t prox[TOWN_RAY_MOVERS];
static unsigned first[TOWN_RAY_MOVERS + 1u];

static unsigned p9_points(const ray_sun_t *s, unsigned max) {
    unsigned m, n = 0;

    for (m = 0; m < TOWN_RAY_MOVERS; m++) { first[m] = n; n += ray_run_points(&prox[m], s, pts + n, max - n); }
    first[TOWN_RAY_MOVERS] = n;
    return n;
}

static void t_proxy(void) {
    unsigned n, r, differ = 0, dark = 0, part = 0, m;
    ray_sun_t s;

    ray_proxies(prox);
    ray_sun_init(&s, TOWN_RAY_SUN16_AZ, TOWN_RAY_SUN16_EL);
    n = p9_points(&s, K);
    memset(out_a, 255, n); memset(out_b, 255, n);
    ray_proxy_pass_ref(prox, TOWN_RAY_MOVERS, first, &s, (const float (*)[4])pts, out_a);
    ray_proxy_pass(prox, TOWN_RAY_MOVERS, first, &s, (const float (*)[4])pts, out_b);
    for (r = 0; r < n; r++) { differ += out_a[r] != out_b[r]; dark += out_a[r] == 0u; part += out_a[r] && out_a[r] < 255u; }
    CHECK(differ == 0u, "proxies: %u bytes differ between the kernel and the reference", differ);
    CHECK(dark > 0u, "proxies: no receiver is shadowed at 16:00");
    for (m = 0; m < TOWN_RAY_MOVERS; m++) {           /* a point half way along each run is in the shadow; 1.5 m aside is not */
        const float hl = sqrtf(s.sx * s.sx + s.sz * s.sz), dx = -s.sx / hl, dz = -s.sz / hl, w = prox[m].run * 0.5f;
        const float in[3] = { prox[m].mx + w * dx, 0.02f, prox[m].mz + w * dz };
        const float aside[3] = { in[0] + 1.5f * dz, 0.02f, in[2] - 1.5f * dx };

        if (prox[m].kind == 0) CHECK(ray_proxy_ref(&prox[m], &s, in) == 0u, "mover %u: mid-run point lit %u", m, ray_proxy_ref(&prox[m], &s, in));
        CHECK(ray_proxy_ref(&prox[m], &s, aside) == RAY_LIT, "mover %u: point 1.5 m aside lit %u", m, ray_proxy_ref(&prox[m], &s, aside));
    }
    printf("4 proxies at 16:00: kernel == reference over %u receivers (%u differ); %u shadowed, %u in a penumbra\n", n, differ, dark, part);
    for (m = 0; m < TOWN_RAY_MOVERS; m++) printf("  mover %2u (%s): %u receivers\n", m, prox[m].kind ? "hen" : "person", first[m + 1u] - first[m]);
    printf("5 P9 at 16:00: %u receivers in all, 12 proxies at their load places\n", n);
}

static void t_sizes(void) {
    static const char *const name[2] = { "14:00", "16:00" };
    static const float az[2] = { TOWN_RAY_SUN14_AZ, TOWN_RAY_SUN16_AZ }, el[2] = { TOWN_RAY_SUN14_EL, TOWN_RAY_SUN16_EL };
    static const unsigned want[] = { 1, 2, 4, 8, 12, 16, 24, 32, 48, 64, 96 };
    unsigned k, r, b;
    ray_sun_t s;

    ray_points(pts, K, 7u);
    for (k = 0; k < 2u; k++) {
        unsigned lit_n[129] = { 0 }, hit = 0;
        double sum = 0.0;

        ray_sun_init(&s, az[k], el[k]);
        ray_slice_ref(town_ray_h, &s, (const float (*)[4])pts, out_a, steps, K);
        for (r = 0; r < K; r++) { sum += steps[r]; if (out_a[r]) lit_n[steps[r]]++; else hit++; }
        printf("5 P2 at %s: %u stand-in points, mean n %.1f, %.1f %% shadowed\n", name[k], K, sum / K, 100.0 * hit / K);
        printf("5 P1 at %s, lit rays of each n among %u:", name[k], K);
        for (b = 0; b < sizeof want / sizeof want[0]; b++) printf(" n%u:%u", want[b], lit_n[want[b]]);
        printf("\n");
    }
    ray_points_any(pts, K, 11u);
    for (k = 0; k < 2u; k++) {
        unsigned lit_n[129] = { 0 }, all = 1;

        ray_sun_init(&s, az[k], el[k]);
        ray_slice_ref(grid0, &s, (const float (*)[4])pts, out_a, steps, K);
        for (r = 0; r < K; r++) { lit_n[steps[r]]++; all &= out_a[r] == RAY_LIT; }
        CHECK(all, "grid of zeros: a ray was shadowed");
        printf("5 P1 at %s on a grid of zeros, rays of each n among %u:", name[k], K);
        for (b = 0; b < sizeof want / sizeof want[0]; b++) { printf(" n%u:%u", want[b], lit_n[want[b]]); CHECK(want[b] > 48u ||lit_n[want[b]] >= 256u, "P1 bucket n %u has %u rays", want[b], lit_n[want[b]]); }
        printf("\n");
    }
}

/* 6 (ray2) the ray against a triangle: known hits and misses; the kernels give their references' bytes; the form
   prepared for the sun agrees with the general form and with a double-precision test but for rays at an edge. */
#define TRI_N 128u
#define TRI_P 4096u

static int tri_double(const ray_tri_t *t, const float o[3], const float d[3], double *margin) {
    const double e1[3] = { t->e1[0], t->e1[1], t->e1[2] }, e2[3] = { t->e2[0], t->e2[1], t->e2[2] }, dd[3] = { d[0], d[1], d[2] };
    const double h[3] = { dd[1] * e2[2] - dd[2] * e2[1], dd[2] * e2[0] - dd[0] * e2[2], dd[0] * e2[1] - dd[1] * e2[0] };
    const double det = e1[0] * h[0] + e1[1] * h[1] + e1[2] * h[2];
    const double s[3] = { (double)o[0] - t->a[0], (double)o[1] - t->a[1], (double)o[2] - t->a[2] };
    const double q[3] = { s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0] };
    double u, v, w, m;

    *margin = 0.0;
    if (fabs(det) < 1e-9) return 0;
    u = (s[0] * h[0] + s[1] * h[1] + s[2] * h[2]) / det;
    v = (dd[0] * q[0] + dd[1] * q[1] + dd[2] * q[2]) / det;
    w = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) / det;
    m = fabs(u); if (fabs(v) < m) m = fabs(v); if (fabs(1.0 - u - v) < m) m = fabs(1.0 - u - v);
    if (fabs(w - RAY_TRI_EPS) < m) m = fabs(w - RAY_TRI_EPS);
    *margin = m;                                     /* how far the ray is from an edge of the test */
    return u >= 0.0 && v >= 0.0 && u + v <= 1.0 && w > RAY_TRI_EPS;
}

static void t_tri(void) {
    static ray_tri_t tri[TRI_N];
    static ray_tri_sun_t tsun[TRI_N];
    static const ray_tri_t flat = { { -1.0f, 5.0f, -1.0f }, { 4.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 4.0f }, { 0.0f, 0.0f, 0.0f } };
    static const float up[3] = { 0.0f, 1.0f, 0.0f }, under[3] = { 0.0f, 0.0f, 0.0f }, aside[3] = { 5.0f, 0.0f, 0.0f }, above[3] = { 0.0f, 6.0f, 0.0f };
    unsigned r, i, k, ta, tb, differ = 0, differ_sun = 0, hits = 0, form = 0, form_edge = 0, dbl = 0, dbl_edge = 0, pairs = 0;
    ray_tri_sun_t fs;
    ray_sun_t s;
    float d[3];

    ray_tri_sun_init(&fs, &flat, up);
    CHECK(ray_tri_ref(&flat, under, up) == 1u && ray_tri(&flat, under, up) == 1u && ray_tri_sun(&fs, under) == 1u && ray_tri_sun_ref(&fs, under) == 1u, "a ray up under the triangle misses");
    CHECK(ray_tri_ref(&flat, aside, up) == 0u && ray_tri(&flat, aside, up) == 0u && ray_tri_sun(&fs, aside) == 0u, "a ray up beside the triangle hits");
    CHECK(ray_tri_ref(&flat, above, up) == 0u && ray_tri(&flat, above, up) == 0u && ray_tri_sun(&fs, above) == 0u, "a ray from above the triangle hits it behind");
    for (k = 0; k < 2u; k++) {
        ray_sun_init(&s, k ? TOWN_RAY_SUN16_AZ : TOWN_RAY_SUN14_AZ, k ? TOWN_RAY_SUN16_EL : TOWN_RAY_SUN14_EL);
        d[0] = s.sx; d[1] = s.sy; d[2] = s.sz;
        ray_tri_scene(tri, TRI_N, pts, TRI_P, d, 1.5f, 5u + k);
        for (i = 0; i < TRI_N; i++) ray_tri_sun_init(&tsun[i], &tri[i], d);
        ta = ray_tri_pass_ref(tri, TRI_N, (const float (*)[4])pts, TRI_P, d, out_a);
        tb = ray_tri_pass(tri, TRI_N, (const float (*)[4])pts, TRI_P, d, out_b);
        for (r = 0; r < TRI_P; r++) { differ += out_a[r] != out_b[r]; hits += out_a[r] == 0u; }
        CHECK(ta == tb, "triangles: the kernel made %u tests, the reference %u", tb, ta);
        ta = ray_tri_sun_pass_ref(tsun, TRI_N, (const float (*)[4])pts, TRI_P, out_a);
        tb = ray_tri_sun_pass(tsun, TRI_N, (const float (*)[4])pts, TRI_P, out_b);
        for (r = 0; r < TRI_P; r++) differ_sun += out_a[r] != out_b[r];
        CHECK(ta == tb, "triangles, sun form: the kernel made %u tests, the reference %u", tb, ta);
        if (!k) printf("6 triangles at 14:00: %u points x %u triangles, %u tests to the first hit (%.1f a ray), %u of %u rays shadowed\n",
                       TRI_P, TRI_N, ta, (double)ta / TRI_P, hits, TRI_P);
        for (r = 0; r < 512u; r++)
            for (i = 0; i < TRI_N; i++) {
                double m;
                const int want = tri_double(&tri[i], pts[r], d, &m);
                const unsigned g = ray_tri_ref(&tri[i], pts[r], d), q = ray_tri_sun_ref(&tsun[i], pts[r]);

                pairs++;
                if (g != q) { form++; form_edge += m < 1e-4; }
                if ((int)g != want) { dbl++; dbl_edge += m < 1e-4; }
            }
    }
    CHECK(differ == 0u, "triangles: %u bytes differ between the kernel and the reference", differ);
    CHECK(differ_sun == 0u, "triangles, sun form: %u bytes differ between the kernel and the reference", differ_sun);
    CHECK(form == form_edge && dbl == dbl_edge, "triangles: %u (sun form) and %u (double) answers differ away from an edge", form - form_edge, dbl - dbl_edge);
    CHECK(form * 1000u < pairs && dbl * 1000u < pairs, "triangles: %u and %u of %u pairs differ", form, dbl, pairs);
    CHECK(hits > TRI_P / 10u && hits < 2u * TRI_P * 9u / 10u, "triangles: %u rays shadowed over two suns of %u each", hits, TRI_P);
    printf("6 triangles: kernel == reference (%u and %u bytes differ, general and sun form); of %u pairs the sun form differs from the general in %u and the general from a double test in %u, all within 1e-4 of an edge\n",
           differ, differ_sun, pairs, form, dbl);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    t_tri();
    t_bytes();
    t_empty();
    t_wall();
    t_proxy();
    t_sizes();
    printf("ray_test: %u checks, %u failed\n", checks, fails);
    return fails ? 1 : 0;
}
