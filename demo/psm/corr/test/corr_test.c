/* SH4DER-PVR: corr-a's host test: the corridor, the ring that never grows, the clip, the counters, the bias codes

   demo/psm/corr/test/corr_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   cc -DSH4DER_CORE_NO_SH4ZAM -I include -I sh4dow-dc/include demo/psm/corr/test/corr_test.c -lm */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../../psm_effects.h"
#include "../corr_scene.h"
#include "../corr_modes.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static corr_kind_t kinds[CORR_KINDS];
static corr_list_t list;

int main(void) {
    unsigned id, i, j, k, f;
    corr_eye_t e = { 0.0f, 0.0f, 0.0f };
    unsigned max_n = 0, min_n = ~0u;
    float t = 0.0f;
    int bmin = 1 << 30, bmax = -(1 << 30);

    corr_build(kinds);
    for (id = 0; id < CORR_KINDS; id++) {
        CHECK(kinds[id].n > 0u && kinds[id].n < CORR_Q_MAX);
        for (i = 0; i < kinds[id].n; i++) {
            const corr_quad_t *q = &kinds[id].q[i];
            const float l = q->n[0] * q->n[0] + q->n[1] * q->n[1] + q->n[2] * q->n[2];

            CHECK(fabsf(l - 1.0f) < 1e-5f);
            for (j = 0; j < 4u; j++) {                       /* inside the corridor's box, a segment long */
                CHECK(q->p[j][0] >= -CORR_HALF_W - 1e-4f && q->p[j][0] <= CORR_HALF_W + 1e-4f);
                CHECK(q->p[j][1] >= -1e-4f && q->p[j][1] <= CORR_H + 1e-4f);
                CHECK(q->p[j][2] >= -1e-4f && q->p[j][2] <= CORR_SEG + 1e-4f);
                CHECK((q->argb[j] >> 24) == 0xffu);
            }
            if (q->tex == CORR_T_FLOOR || q->tex == CORR_T_CEIL || (q->tex == CORR_T_WALL && q->n[2] == 0.0f)) {
                /* v runs along the corridor (corr-b): along each strip edge, v changes only with z, at one rate a quad */
                const float k = q->tex == CORR_T_WALL ? 0.5f : 1.0f;
                unsigned e;

                for (e = 1; e < 4u; e++) CHECK(fabsf((q->uv[e][1] - q->uv[0][1]) - k * (q->p[e][2] - q->p[0][2])) < 1e-5f);
            }
        }
    }
    CHECK(corr_has_door(4) && !corr_has_door(3) && corr_has_pillars(2) && corr_has_torch(0));

    /* the strip order of a loop */
    CHECK(corr_strip_index(0, 4) == 0 && corr_strip_index(1, 4) == 1 && corr_strip_index(2, 4) == 3 && corr_strip_index(3, 4) == 2);
    CHECK(corr_strip_index(0, 5) == 0 && corr_strip_index(1, 5) == 1 && corr_strip_index(2, 5) == 4 && corr_strip_index(3, 5) == 2 && corr_strip_index(4, 5) == 3);

    /* the ring: 10^6 frames at the fastest speed with the sway; s and the segment numbers stay bounded */
    for (f = 0; f < 1000000u; f++) {
        const int b = corr_ring_base(e.s);

        e.s = corr_walk(e.s, 12.0f, 1.0f / 60.0f);
        t += 1.0f / 60.0f;
        if (t > 20.0f) t -= 20.0f;                           /* the program wraps its sway clock at 20 s too */
        CHECK(e.s >= 0.0f && e.s < CORR_WRAP);
        if (b < bmin) bmin = b;
        if (b > bmax) bmax = b;
        if (fails > 10) break;
    }
    CHECK(bmin >= -1 && bmax + (int)CORR_RING <= 2 * (int)CORR_IDS);
    CHECK(fabsf(corr_sway(0.0f)) < 1e-6f && fabsf(corr_sway(5.0f) - 0.105f) < 1e-3f);

    /* a frame: every polygon on the screen side of the near plane, inside 3 .. 5 points, bins in range */
    for (f = 0; f < 400u; f++) {
        e.s = corr_walk(0.0f, 1.0f, (float)f * 0.16f); e.x = (float)(f % 7u) * 0.3f - 0.9f; e.yaw = corr_sway((float)f * 0.37f) + ((f & 1u) ? 0.3f : -0.3f);
        corr_frame(kinds, &e, 1, CORR_CAP, &list);
        if (f < 3u) printf("eye %u: %u polygons, %u culled, %u clipped, %u dropped\n", f, list.n, list.culled, list.clipped, list.dropped);
        if (list.n > max_n) max_n = list.n;
        if (list.n < min_n) min_n = list.n;
        for (i = 0; i < list.n; i++) {
            const corr_poly_t *p = &list.p[i];

            CHECK(p->n >= 3u && p->n <= 5u);
            CHECK(p->bx0 <= p->bx1 && p->bx1 < CORR_BINS_X && p->by0 <= p->by1 && p->by1 < CORR_BINS_Y);
            CHECK(p->ndv >= 0.0f && p->ndv <= 1.0f);
            for (j = 0; j < p->n; j++) CHECK(p->z[j] > 0.0f && p->z[j] <= 1.0f / CORR_NEAR + 1e-2f);
        }
        if (fails > 10) break;
    }
    CHECK(min_n > 100u && max_n < CORR_POLY_MAX);
    {   /* the far cap on the autorun's path (8 s from s = 2 at 3 m/s, the sway): the most filtered opaque boxes on one bin,
           capped and as the first design had it; the far kinds keep the near kinds' texture repeats (checked above: v) */
        static corr_bins_t bc, bu;
        unsigned worst_c = 0, worst_u = 0, tri_c = 0, tri_u = 0;

        for (f = 0; f < 8u * 60u; f++) {
            unsigned tc = 0, tu = 0;

            e.s = corr_walk(2.0f, 3.0f, (float)f / 60.0f); e.x = 0.0f; e.yaw = corr_sway((float)f / 60.0f);
            corr_frame(kinds, &e, 0, CORR_CAP, &list); corr_bins_clear(&bc);
            for (i = 0; i < list.n; i++) { if (CORR_TEXTURED(list.p[i].tex)) corr_bins_add(&bc, &list.p[i]); tc += list.p[i].n - 2u; }
            corr_frame(kinds, &e, 0, CORR_NO_CAP, &list); corr_bins_clear(&bu);
            for (i = 0; i < list.n; i++) { if (CORR_TEXTURED(list.p[i].tex)) corr_bins_add(&bu, &list.p[i]); tu += list.p[i].n - 2u; }
            if (corr_bins_max(&bc) > worst_c) worst_c = corr_bins_max(&bc);
            if (corr_bins_max(&bu) > worst_u) worst_u = corr_bins_max(&bu);
            if (tc > tri_c) tri_c = tc;
            if (tu > tri_u) tri_u = tu;
        }
        printf("autorun path: most boxes on one bin %u capped, %u uncapped; triangles at most %u capped, %u uncapped\n", worst_c, worst_u, tri_c, tri_u);
        CHECK(worst_c * 4u < worst_u);
        CHECK(worst_c < 100u);
    }
    printf("corr: %u..%u polygons a frame (400 eyes), kinds %u..%u quads\n", min_n, max_n, kinds[1].n, kinds[4].n);

    /* the clip: a quad straddling the near plane gives 5 points, all at z >= near */
    {
        corr_vtx_t_ in[4] = { { -1, 0, -1, 0, 0, 0xff000000u }, { 1, 0, -1, 1, 0, 0xff000000u }, { 1, 0, 3, 1, 1, 0xffffffffu }, { -1, 0, 3, 0, 1, 0xffffffffu } }, o[5];
        unsigned n = corr_clip_near(in, o);

        CHECK(n == 4u);                                       /* two out, two in */
        for (i = 0; i < n; i++) CHECK(o[i].z >= CORR_NEAR - 1e-5f);
        in[1].z = 3.0f;                                       /* one out */
        n = corr_clip_near(in, o);
        CHECK(n == 5u);
        for (i = 0; i < n; i++) CHECK(o[i].z >= CORR_NEAR - 1e-5f);
        in[1].z = in[2].z = -1.0f;                            /* three out */
        CHECK(corr_clip_near(in, o) == 3u);
    }

    /* the counter: a polygon over bins 2..4 x 1..2 adds 1 to each; two of them make 2 */
    {
        corr_bins_t b;
        corr_poly_t p;

        memset(&p, 0, sizeof p);
        p.bx0 = 2; p.bx1 = 4; p.by0 = 1; p.by1 = 2;
        corr_bins_clear(&b);
        corr_bins_add(&b, &p);
        CHECK(corr_bins_max(&b) == 1u && b.c[1][2] == 1u && b.c[2][4] == 1u && b.c[0][2] == 0u && b.c[1][5] == 0u);
        corr_bins_add(&b, &p);
        CHECK(corr_bins_max(&b) == 2u);
    }

    /* the modes: names, the bias selector, the slope codes through fakeaf, the header guard (a) */
    for (k = 0; k < CORR_SEL_N; k++) {
        const corr_sel_t *s = corr_sel(k);

        CHECK(s->name && strlen(s->name) <= 16u);
        for (f = 0; f <= 20u; f++) {
            const unsigned c = corr_sel_code(s, (float)f * 0.05f);

            CHECK(c >= 1u && c <= 15u);
            if (s->fixed) CHECK(c == s->fixed);
            else CHECK(c <= FAF_BIAS_NONE && c >= s->least);
        }
    }
    CHECK(corr_sel_code(corr_sel(CORR_SEL_SLOPE1_L2), 1.0f) == 4u);   /* facing: no bias */
    CHECK(corr_sel_code(corr_sel(CORR_SEL_SLOPE1_L2), 0.0f) == 2u);   /* edge on: held at least 0.50 */
    CHECK(corr_sel_code(corr_sel(CORR_SEL_SLOPE1_L1), 0.0f) == 1u);
    CHECK(corr_sel_code(corr_sel(CORR_SEL_FIXED050), 0.3f) == 2u);
    CHECK(corr_mode_next(CORR_M_BIAS) == CORR_M_NEAREST);              /* the ramp hook is skipped in the cycle */
    CHECK(corr_mode_next(CORR_M_NEAREST) == CORR_M_BILINEAR);
    CHECK(corr_ramp_draw(NULL) == 0);
    CHECK(corr_filter_ok(CORR_FMT_TWIDDLED | CORR_FMT_MIPMAP, 1) == 1);
    CHECK(corr_filter_ok(CORR_FMT_NONTWIDDLED, 1) == 0);               /* CORE 26: filtered and not twiddled: refused */
    CHECK(corr_filter_ok(CORR_FMT_TWIDDLED | CORR_FMT_BUMP, 1) == 0);
    CHECK(corr_filter_ok(CORR_FMT_NONTWIDDLED, 0) == 1);               /* point sampled: allowed */
    for (k = 0; k < CORR_MODES; k++) CHECK(corr_mode_name(k) != NULL);
    {   /* the labels: every mode alone and every split pair fit the live view's two lines; the page's table fits 3 */
        fx_text_t tx;
        unsigned a, b, fa, fb;

        for (a = 0; a < CORR_MODES; a++)
            for (fa = 0; fa < 2u; fa++)
                for (b = 0; b < CORR_MODES; b++)
                    for (fb = 0; fb < 2u; fb++) {
                        CHECK(corr_fx_live(&tx, a, (int)fa, 0, b, (int)fb) == 1);
                        CHECK(corr_fx_live(&tx, a, (int)fa, 1, b, (int)fb) == 1);
                    }
        CHECK(fx_format_page_n(&tx, corr_fx_page, CORR_FX_PAGE_N, 3u) == 1);
    }

    if (fails) { printf("corr_test: %d FAILED\n", fails); return 1; }
    printf("corr_test: OK\n");
    return 0;
}
