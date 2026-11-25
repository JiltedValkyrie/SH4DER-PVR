/* SH4DER-PVR: are the town's casters closed and wound one way? (host check for AO method 2 drawn one side only)

   demo/psm/ao/test/ao_casters_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   ao_layer.h's "one side" (psm_ao2_cull 1 or -1) sends only the caster triangles of one sign of area in the target. It
   draws the same shadow as both sides only when every caster object is a closed surface wound one way. This looks at
   each caster object of the town's scene (psm_scene_init_town, the movers placed once) two ways:

     the mesh    its vertices welded by position; an edge of a closed, one-way-wound surface is used exactly twice, once
                 in each direction. open: edges used once. odd: edges used more than twice. flipped: edges used twice
                 in the same direction. vol: the signed volume (its sign says which way the object is wound).
     the shadow  the object's shadow from each of 8 sky directions (ao_slope) rastered into 256 x 256 texels three
                 times: both sides, side 1, side -1. d1 and d-1 are the texels (edge texels left out) where a side's
                 shadow is not the shadow of both sides. This is the test that matters: an object can fail the mesh
                 test and still cast the same shadow from these directions (a box with no floor, standing on the
                 ground), and the other way round never.

   It prints a row an object and a verdict: the sign to use, and the objects that cannot be drawn one side. It fails
   (exit 1) only on its own faults; open casters are a finding, not a failure, unless AO_CASTERS_STRICT is defined.

   Build: gcc -O2 -std=c99 -Wall -Wextra -DSH4DER_CORE_NO_SH4ZAM -DPSM_HAVE_TOWN -I<town mesh dir> -I<sky dir> -I<cow dir>
          -Iinclude -Ish4dow-dc/include demo/psm/ao/test/ao_casters_test.c -lm */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../psm_scene.h"
#include "../../ao_layer.h"

#define G 256
static psm_scene_t scene;
static uint8_t cov[3][G * G], edge[G * G];
static float xy[PSM_CV_MAX][2];

static float edge_fn(const float *a, const float *b, float px, float py) { return (b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0]); }

/* the object's triangles of side `cull` (0 both) from direction bit d into cov[which] */
static void raster(const psm_cobj_t *o, int cull, unsigned which, unsigned d) {
    unsigned t;

    for (t = o->t0; t < o->t0 + o->tn; t++) {
        const float *a = xy[scene.ct[t][0]], *b = xy[scene.ct[t][1]], *c = xy[scene.ct[t][2]];
        float x0 = a[0], x1 = a[0], y0 = a[1], y1 = a[1];
        int ix0, ix1, iy0, iy1, x, y;

        if (!ao_tri_ok(a, b, c, (float)G, cull)) continue;
        if (b[0] < x0) x0 = b[0];
        if (c[0] < x0) x0 = c[0];
        if (b[0] > x1) x1 = b[0];
        if (c[0] > x1) x1 = c[0];
        if (b[1] < y0) y0 = b[1];
        if (c[1] < y0) y0 = c[1];
        if (b[1] > y1) y1 = b[1];
        if (c[1] > y1) y1 = c[1];
        ix0 = x0 < 0.0f ? 0 : (int)x0; ix1 = x1 >= (float)G ? G - 1 : (int)x1;
        iy0 = y0 < 0.0f ? 0 : (int)y0; iy1 = y1 >= (float)G ? G - 1 : (int)y1;
        for (y = iy0; y <= iy1; y++)
            for (x = ix0; x <= ix1; x++) {
                const float px = (float)x + 0.5f, py = (float)y + 0.5f;
                const float e0 = edge_fn(a, b, px, py), e1 = edge_fn(b, c, px, py), e2 = edge_fn(c, a, px, py);

                if ((e0 >= 0.0f && e1 >= 0.0f && e2 >= 0.0f) || (e0 <= 0.0f && e1 <= 0.0f && e2 <= 0.0f)) cov[which][y * G + x] |= (uint8_t)(1u << d);
            }
    }
}

typedef struct { int a, b; } dedge_t;

int main(void) {
    static int weld[PSM_CV_MAX];
    static dedge_t de[PSM_CT_MAX * 3u];
    psm_state_t st;
    unsigned j, i, k, d, bad_objs = 0, pos = 0, neg = 0, need_pos = 0, need_neg = 0, either = 0;

    psm_scene_init_town(&scene);
    psm_state_default(&st);
    psm_state_town(&st);
    psm_town_update(&scene, &st, 0.0f);
    printf("the town's casters: %u objects, %u triangles, %u vertices\n", scene.con, scene.ctn, scene.cvn);
    printf(" obj mov tris  open odd flipped      vol    d1   d-1  verdict\n");
    for (j = 0; j < scene.con; j++) {
        const psm_cobj_t *o = &scene.co[j];
        unsigned ne = 0, open = 0, odd = 0, flipped = 0, d1 = 0, dm = 0, t;
        double vol = 0.0;
        float lo[2] = { 1e30f, 1e30f }, hi[2] = { -1e30f, -1e30f }, hmax = 0.0f, s, map[16];
        const char *verdict;

        /* ---- the mesh ---- */
        for (i = o->v0; i < o->v0 + o->vn; i++) {
            weld[i] = (int)i;
            for (k = o->v0; k < i; k++)
                if (fabsf(scene.cv[i][0] - scene.cv[k][0]) < 1e-4f && fabsf(scene.cv[i][1] - scene.cv[k][1]) < 1e-4f && fabsf(scene.cv[i][2] - scene.cv[k][2]) < 1e-4f) { weld[i] = weld[k]; break; }
            if (scene.cv[i][0] < lo[0]) lo[0] = scene.cv[i][0];
            if (scene.cv[i][0] > hi[0]) hi[0] = scene.cv[i][0];
            if (scene.cv[i][2] < lo[1]) lo[1] = scene.cv[i][2];
            if (scene.cv[i][2] > hi[1]) hi[1] = scene.cv[i][2];
            if (scene.cv[i][1] > hmax) hmax = scene.cv[i][1];
        }
        for (t = o->t0; t < o->t0 + o->tn; t++) {
            const float *a = scene.cv[scene.ct[t][0]], *b = scene.cv[scene.ct[t][1]], *c = scene.cv[scene.ct[t][2]];
            const int w[3] = { weld[scene.ct[t][0]], weld[scene.ct[t][1]], weld[scene.ct[t][2]] };

            vol += ((double)a[0] * ((double)b[1] * c[2] - (double)b[2] * c[1]) - (double)a[1] * ((double)b[0] * c[2] - (double)b[2] * c[0])
                  + (double)a[2] * ((double)b[0] * c[1] - (double)b[1] * c[0])) / 6.0;
            if (w[0] == w[1] || w[1] == w[2] || w[2] == w[0]) continue;      /* no area once welded */
            for (k = 0; k < 3u; k++) { de[ne].a = w[k]; de[ne].b = w[(k + 1u) % 3u]; ne++; }
        }
        for (i = 0; i < ne; i++) {
            unsigned same = 0, opp = 0;

            for (k = 0; k < ne; k++) {
                if (de[k].a == de[i].a && de[k].b == de[i].b) same++;
                if (de[k].a == de[i].b && de[k].b == de[i].a) opp++;
            }
            if (same + opp == 1u) open++;
            else if (same + opp > 2u) odd++;
            else if (same == 2u) flipped++;
        }
        flipped /= 2u; odd /= 3u;                                         /* counted from each of its uses (odd: at least 3) */

        /* ---- the shadow: the object and its reach in the 256 grid ---- */
        {
            const float reach = hmax * AO_COT_EL + 0.5f, cx = 0.5f * (lo[0] + hi[0]), cz = 0.5f * (lo[1] + hi[1]);
            const float half = 0.5f * ((hi[0] - lo[0]) > (hi[1] - lo[1]) ? (hi[0] - lo[0]) : (hi[1] - lo[1])) + reach;

            s = 1.0f / half;
            memset(map, 0, sizeof map);
            map[0] = s; map[9] = s; map[12] = -cx * s; map[13] = -cz * s; map[15] = 1.0f;
        }
        memset(cov, 0, sizeof cov);
        for (d = 0; d < 8u; d++) {
            float slope[2], m[16];

            ao_slope(8u, d, slope);
            ao_slide_map(map, slope, 0.0f, m);
            for (i = o->v0; i < o->v0 + o->vn; i++) {
                float h[4];

                sh4der_psm_xform(m, scene.cv[i], 1.0f, h);
                if (!ao_px(h, 1.0f, 0.5f * (float)G, xy[i])) { printf("FAIL: object %u vertex %u has no place in the grid\n", j, i); return 1; }
            }
            raster(o, 0, 0u, d); raster(o, 1, 1u, d); raster(o, -1, 2u, d);
        }
        for (i = 0; i < G * G; i++) {
            const unsigned x = i % G, y = i / G;
            int e = x == 0u || y == 0u || x == G - 1u || y == G - 1u, dx, dy;

            for (dy = -1; !e && dy <= 1; dy++) for (dx = -1; dx <= 1; dx++) if (cov[0][i + (unsigned)(dy * G + dx)] != cov[0][i]) e = 1;
            edge[i] = (uint8_t)e;
            if (e) continue;
            d1 += cov[1][i] != cov[0][i];
            dm += cov[2][i] != cov[0][i];
        }
        if (d1 == 0u && dm == 0u) { verdict = "either side"; either++; }
        else if (d1 == 0u) { verdict = "side 1 only"; need_pos++; }
        else if (dm == 0u) { verdict = "side -1 only"; need_neg++; }
        else { verdict = "BOTH SIDES NEEDED"; bad_objs++; }
        if (vol > 0.0) pos++; else neg++;
        printf(" %3u  %u  %4u  %4u %3u %7u %8.3f %5u %5u  %s\n", j, o->mover, o->tn, open, odd, flipped, vol, d1, dm, verdict);
    }
    printf("signed volume: %u objects positive, %u negative\n", pos, neg);
    printf("shadow test: %u either side, %u side 1 only, %u side -1 only, %u need both sides\n", either, need_pos, need_neg, bad_objs);
    if (bad_objs == 0u && !(need_pos && need_neg))
        printf("VERDICT: one side draws every caster's shadow; use psm_ao2_cull = %d\n", need_neg ? -1 : 1);
    else if (bad_objs == 0u)
        printf("VERDICT: every caster can be drawn one side, but not all with the same sign: the casters are not wound one way\n");
    else
        printf("VERDICT: %u objects need both sides: one side for the others, both for these\n", bad_objs);
#ifdef AO_CASTERS_STRICT
    return bad_objs || (need_pos && need_neg);
#else
    return 0;
#endif
}
