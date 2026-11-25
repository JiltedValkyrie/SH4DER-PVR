/* SH4DER-PVR: ray bench, host count of the steps the town's own receivers take (not a measurement)

   demo/psm/bench/ray_town_n.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   ray_mean_n.c counts P2's 1,024 stand-ins; THESIS 5.9's 16:00 row needs the town's own receivers. This loads the town as
   the demo does (psm_scene_init_town, plain C path) and walks the bench's grid (ray_walk_ref, the reference the console
   kernel matches byte for byte) from:
     models  every static model vertex that faces the sun (N . s > 0; a vertex facing away needs no ray, 5.3), started
             off along its normal by [normal offset] and toward the sun by [sun offset] (TOWN-061, TOWN-065);
     ground  one ray a texel centre of a [side] x [side] map over the grid's 96 m square at y = 0.02 (5.3's world map;
             town_bake.c's layout).
   For each it prints the mean n, split lit / shadowed, a histogram, and the line C_setup + n C_step (default the
   console's 72 + 22.1 n, SH4-052) as the mean cost a ray and as k for a 2 ms slice. Host ESTIMATE: no console.

   Build (from the repository's root; the town mesh header from tools/bake_town.py in <BUILD>/psm_town, the sky in
   <BUILD>/psm_sky, as town_bake.c):
     cc -O2 -std=gnu11 -DSH4DER_CORE_NO_SH4ZAM -DPSM_HAVE_TOWN -Idemo/psm -Iinclude -Ish4dow-dc/include
        -Ithird_party/sh4zam/include -I<BUILD>/psm_town -I<BUILD>/psm_sky demo/psm/bench/ray_town_n.c src/sh4der_core.c
        -lm -o ray_town_n
   Run: ray_town_n [normal offset m, 0.05] [sun offset m, 0] [ground side, 192] [C_setup, 72] [C_step, 22.1]
*/

#include <stdio.h>
#include <stdlib.h>

#include "psm_scene.h"
#include "ray_kernels.h"

#define SLICE_CYCLES 400012.0          /* 2 ms at 200.006 MHz (SH4-001) */

static psm_scene_t S;

typedef struct { unsigned rays, lit, nmax, hist[8]; unsigned long long sum, sum_lit; } count_t;

static void add(count_t *c, unsigned lit, unsigned n) {
    static const unsigned edge[7] = { 8u, 16u, 24u, 32u, 48u, 64u, 96u };
    unsigned b = 0;
    while (b < 7u && n >= edge[b]) b++;
    c->rays++; c->sum += n; c->hist[b]++;
    if (lit) { c->lit++; c->sum_lit += n; }
    if (n > c->nmax) c->nmax = n;
}

static void report(const char *what, const count_t *c, double c_setup, double c_step) {
    const unsigned sh = c->rays - c->lit;
    const double mean = c->rays ? (double)c->sum / c->rays : 0.0, ray = c_setup + mean * c_step;
    printf("  %s: %u rays, lit %u, shadowed %u; mean n %.2f (lit %.2f, shadowed %.2f), max %u\n", what, c->rays, c->lit, sh, mean,
           c->lit ? (double)c->sum_lit / c->lit : 0.0, sh ? (double)(c->sum - c->sum_lit) / sh : 0.0, c->nmax);
    printf("    n in 0-7 / 8-15 / 16-23 / 24-31 / 32-47 / 48-63 / 64-95 / 96+: %u %u %u %u %u %u %u %u\n",
           c->hist[0], c->hist[1], c->hist[2], c->hist[3], c->hist[4], c->hist[5], c->hist[6], c->hist[7]);
    printf("    line: %.1f cycles a ray; k for a 2 ms slice %u\n", ray, (unsigned)(SLICE_CYCLES / ray));
}

int main(int argc, char **argv) {
    static const float az[2] = { TOWN_RAY_SUN14_AZ, TOWN_RAY_SUN16_AZ }, el[2] = { TOWN_RAY_SUN14_EL, TOWN_RAY_SUN16_EL };
    static const char *const name[2] = { "14:00", "16:00" };
    const float off = argc > 1 ? (float)atof(argv[1]) : 0.05f, soff = argc > 2 ? (float)atof(argv[2]) : 0.0f;
    const unsigned side = argc > 3 ? (unsigned)atoi(argv[3]) : 192u;
    const double c_setup = argc > 4 ? atof(argv[4]) : 72.0, c_step = argc > 5 ? atof(argv[5]) : 22.1;
    unsigned sun, i, j;

    (void)ray_slice; (void)ray_slice_ref; (void)ray_proxy_pass; (void)ray_proxy_pass_ref;
    psm_scene_init(&S);
    psm_scene_init_town(&S);
    printf("town: %u static model vertices; ground map %u x %u (%.3f m); start: %.2f m along the normal, %.2f m toward the sun;"
           " line %.2f + n x %.2f\n", S.mdyn0, side, side, 96.0f / side, (double)off, (double)soff, c_setup, c_step);
    for (sun = 0; sun < 2u; sun++) {
        count_t m = { 0 }, g = { 0 }, all;
        unsigned away = 0;
        ray_sun_t s;
        float sd[3];

        ray_sun_init(&s, az[sun], el[sun]);
        sd[0] = s.sx; sd[1] = s.sy; sd[2] = s.sz;
        for (i = 0; i < S.mdyn0; i++) {
            const float *n = S.mn[i];
            unsigned steps, lit;
            if (!(n[0] * sd[0] + n[1] * sd[1] + n[2] * sd[2] > 0.0f)) { away++; continue; }
            lit = ray_walk_ref(town_ray_h, &s, S.mv[i][0] + n[0] * off + sd[0] * soff, S.mv[i][1] + n[1] * off + sd[1] * soff,
                               S.mv[i][2] + n[2] * off + sd[2] * soff, &steps);
            add(&m, lit != 0u, steps);
        }
        for (j = 0; j < side; j++)
            for (i = 0; i < side; i++) {
                unsigned steps;
                const unsigned lit = ray_walk_ref(town_ray_h, &s, TOWN_RAY_X0 + (i + 0.5f) * 96.0f / side, 0.02f,
                                                  TOWN_RAY_Z0 + (j + 0.5f) * 96.0f / side, &steps);
                add(&g, lit != 0u, steps);
            }
        all = m;
        all.rays += g.rays; all.lit += g.lit; all.sum += g.sum; all.sum_lit += g.sum_lit;
        if (g.nmax > all.nmax) all.nmax = g.nmax;
        for (i = 0; i < 8u; i++) all.hist[i] += g.hist[i];
        printf("%s (az %.2f el %.2f): %u model vertices face away (no ray)\n", name[sun], (double)az[sun], (double)el[sun], away);
        report("models, facing the sun", &m, c_setup, c_step);
        report("ground texels", &g, c_setup, c_step);
        report("both", &all, c_setup, c_step);
        printf("    models: mean n over all %u static vertices, a facing-away vertex 0 steps: %.2f\n", S.mdyn0, (double)m.sum / S.mdyn0);
    }
    return 0;
}
