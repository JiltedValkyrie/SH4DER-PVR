/* SH4ZAM-DCE tests: shz_dce_tri_row.h on the host (SH4ZAM's software back end)

   tri_row_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   What is checked (the header's claims, on random triangles: slivers, triangles far outside the map that the bounding box clips, both
   windings, flat and perspective maps, maps of 64, 128 and 256 texels):
   - shz_dce_tri_row_step_c is BIT-IDENTICAL to the running-add row it replaces, texel by texel, for every count from 0 up and on
     whole triangles (the reference is a copy of that row, here, with the same setup)
   - the coverage bound: a texel whose exact edge value is clearly inside or outside the tolerance (by more than the running sums'
     bound, count x 2^-24 x M) is covered or not, as it should be
   - the guard (tri->iw[3]): a 1 / w at or below it stores depth 1; a flat map does not read 1 / w
   - a count of 0 writes nothing; a row's store stays inside its row; the dispatch name gives the C form bit for bit on the host
   - the SH4ZAM floor for the guard is a normal float's square root (SHZ_DCE_TRI_ROW_IW_FLOOR >= 1.1e-19)
   The depth error against the exact value is measured and printed, not asserted: the running sums have no tight bound (the header
   says so); the SH-4 loop's FIPR bound is checked on the SH-4 by tri_row_check.c.
*/

#include <sh4zam_dce/shz_dce_tri_row.h>

#include "shz_dce_test.h"

#define EDGE_TOL 1.0e-3f
#define ROW_MAX 256u

/* The triangle's setup: the same bounding box and vertex order as the plain row (either winding, the area made positive). Returns 0
   for a triangle the row fills nothing from. */
static int setup(const float in[3][4], const float **s0, const float **s1, const float **s2, unsigned size,
                 int *x0, int *x1, int *y0, int *y1) {
    const float n = (float)size;
    float lo_x, hi_x, lo_y, hi_y, area;
    const float *a = in[0], *b = in[1], *c = in[2];

    area = (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]);
    if (area < 0.0f) { const float *t = b; b = c; c = t; area = -area; }
    if (!(area > 1e-9f)) return 0;
    lo_x = a[0] < b[0] ? a[0] : b[0]; if (c[0] < lo_x) lo_x = c[0];
    hi_x = a[0] > b[0] ? a[0] : b[0]; if (c[0] > hi_x) hi_x = c[0];
    lo_y = a[1] < b[1] ? a[1] : b[1]; if (c[1] < lo_y) lo_y = c[1];
    hi_y = a[1] > b[1] ? a[1] : b[1]; if (c[1] > hi_y) hi_y = c[1];
    if (!(hi_x > 0.0f) || !(hi_y > 0.0f) || !(lo_x < n) || !(lo_y < n)) return 0;
    *x0 = lo_x > 0.5f ? (int)(lo_x - 0.5f) : 0;
    *y0 = lo_y > 0.5f ? (int)(lo_y - 0.5f) : 0;
    *x1 = hi_x < n ? (int)(hi_x + 0.5f) + 1 : (int)size;
    *y1 = hi_y < n ? (int)(hi_y + 0.5f) + 1 : (int)size;
    if (*x1 > (int)size) *x1 = (int)size;
    if (*y1 > (int)size) *y1 = (int)size;
    *s0 = a; *s1 = b; *s2 = c;
    return 1;
}

/* The constants of the row walk for a triangle (what the caller builds once per triangle). */
static int build_tri(const float in[3][4], unsigned size, shz_dce_tri_row_t *tri, int *x0, int *x1, int *y0, int *y1, int *flat,
                     const float **s0, const float **s1, const float **s2) {
    float area, inv, e0x, e1x, e2x;

    if (!setup(in, s0, s1, s2, size, x0, x1, y0, y1)) return 0;
    area = ((*s1)[0] - (*s0)[0]) * ((*s2)[1] - (*s0)[1]) - ((*s2)[0] - (*s0)[0]) * ((*s1)[1] - (*s0)[1]);
    e0x = (*s1)[1] - (*s2)[1];
    e1x = (*s2)[1] - (*s0)[1];
    e2x = (*s0)[1] - (*s1)[1];
    inv = 1.0f / area;
    *flat = (*s0)[2] == (*s1)[2] && (*s1)[2] == (*s2)[2];
    tri->step[0] = e0x; tri->step[1] = e1x; tri->step[2] = e2x; tri->step[3] = -EDGE_TOL;
    if (*flat) {
        const float w = 1.0f / (*s0)[2];

        tri->kw[0] = (*s0)[3] * w * inv; tri->kw[1] = (*s1)[3] * w * inv; tri->kw[2] = (*s2)[3] * w * inv;
        tri->iw[0] = tri->iw[1] = tri->iw[2] = 0.0f;
    } else {
        tri->iw[0] = (*s0)[2] * inv; tri->iw[1] = (*s1)[2] * inv; tri->iw[2] = (*s2)[2] * inv;
        tri->kw[0] = (*s0)[3] * inv; tri->kw[1] = (*s1)[3] * inv; tri->kw[2] = (*s2)[3] * inv;
    }
    tri->kw[3] = 0.0f;
    tri->iw[3] = SHZ_DCE_TRI_ROW_IW_FLOOR;
    return 1;
}

/* The row exactly as it stands in the shadow raster's fill loop before the kernel existed: the reference that _c must match. The
   row start (the three edges, iw and kw) is computed per row from the triangle's constants, the five running adds a texel. */
static void reference_row(uint16_t *row, unsigned count, const float edge[3], const shz_dce_tri_row_t *tri, int perspective) {
    const float e0x = tri->step[0], e1x = tri->step[1], e2x = tri->step[2], lo = tri->step[3];
    const float a0 = tri->iw[0], a1 = tri->iw[1], a2 = tri->iw[2], k0 = tri->kw[0], k1 = tri->kw[1], k2 = tri->kw[2];
    const float dwx = e0x * a0 + e1x * a1 + e2x * a2, dkx = e0x * k0 + e1x * k1 + e2x * k2;
    float e0 = edge[0], e1 = edge[1], e2 = edge[2];
    float iw = e0 * a0 + e1 * a1 + e2 * a2, kw = e0 * k0 + e1 * k1 + e2 * k2;
    unsigned x;

    for (x = 0; x < count; x++) {
        if (e0 >= lo && e1 >= lo && e2 >= lo) {
            float q = kw;

            if (perspective) q = iw > tri->iw[3] ? kw * shz_invf_fsrra(iw) : 0.0f;
            q = shz_clampf(q, 1.0f, 65535.0f);
            if ((uint16_t)q > row[x]) row[x] = (uint16_t)q;
        }
        e0 += e0x; e1 += e1x; e2 += e2x; iw += dwx; kw += dkx;
    }
}

/* A whole triangle, the reference way: the setup and one reference_row() per row. */
static void reference_tri(uint16_t *depth, unsigned size, const float in[3][4]) {
    const float *s0, *s1, *s2;
    shz_dce_tri_row_t tri;
    int x0, x1, y0, y1, y, flat;

    if (!build_tri(in, size, &tri, &x0, &x1, &y0, &y1, &flat, &s0, &s1, &s2)) return;
    for (y = y0; y < y1; y++) {
        const float px = (float)x0 + 0.5f, py = (float)y + 0.5f;
        float edge[3];

        edge[0] = (s1[0] - px) * (s2[1] - py) - (s2[0] - px) * (s1[1] - py);
        edge[1] = (s2[0] - px) * (s0[1] - py) - (s0[0] - px) * (s2[1] - py);
        edge[2] = (s0[0] - px) * (s1[1] - py) - (s1[0] - px) * (s0[1] - py);
        reference_row(depth + (size_t)y * size + x0, (unsigned)(x1 - x0), edge, &tri, !flat);
    }
}

/* The same triangle through shz_dce_tri_row_step (the dispatch name, the C form on the host). */
static void kernel_tri(uint16_t *depth, unsigned size, const float in[3][4]) {
    const float *s0, *s1, *s2;
    shz_dce_tri_row_t tri;
    int x0, x1, y0, y1, y, flat;

    if (!build_tri(in, size, &tri, &x0, &x1, &y0, &y1, &flat, &s0, &s1, &s2)) return;
    for (y = y0; y < y1; y++) {
        const float px = (float)x0 + 0.5f, py = (float)y + 0.5f;
        float edge[3];

        edge[0] = (s1[0] - px) * (s2[1] - py) - (s2[0] - px) * (s1[1] - py);
        edge[1] = (s2[0] - px) * (s0[1] - py) - (s0[0] - px) * (s2[1] - py);
        edge[2] = (s0[0] - px) * (s1[1] - py) - (s1[0] - px) * (s0[1] - py);
        shz_dce_tri_row_step(depth + (size_t)y * size + x0, (unsigned)(x1 - x0), edge, &tri, !flat);
    }
}

/* the exact depth of a texel centre in double: barycentric weights of the vertices as given (either winding), the depth K flat,
   K / w over the weighted 1 / w otherwise; for measuring only */
static double exact_depth(const float in[3][4], int flat, double px, double py) {
    const double ax = in[0][0], ay = in[0][1], bx = in[1][0], by = in[1][1], cx = in[2][0], cy = in[2][1];
    const double area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
    const double e0 = (bx - px) * (cy - py) - (cx - px) * (by - py);
    const double e1 = (cx - px) * (ay - py) - (ax - px) * (cy - py);
    const double e2 = (ax - px) * (by - py) - (bx - px) * (ay - py);
    const double w0 = e0 / area, w1 = e1 / area, w2 = e2 / area;

    if (flat) return w0 * (in[0][3] / in[0][2]) + w1 * (in[1][3] / in[1][2]) + w2 * (in[2][3] / in[2][2]);
    {
        const double kw = w0 * in[0][3] + w1 * in[1][3] + w2 * in[2][3];
        const double iw = w0 * in[0][2] + w1 * in[1][2] + w2 * in[2][2];
        return iw > 1e-30 ? kw / iw : 0.0;
    }
}

static void random_tri(float s[3][4], unsigned size, int flat, int huge, int sliver) {
    const float n = (float)size;
    int i, k;

    for (i = 0; i < 3; i++) {
        const float depth_units = t_frand(1.0f, 65000.0f);
        const float iwv = flat ? 0.5f : t_frand(0.05f, 2.0f);

        if (huge) { s[i][0] = t_frand(-20.0f * n, 20.0f * n); s[i][1] = t_frand(-20.0f * n, 20.0f * n); }
        else      { s[i][0] = t_frand(-0.2f * n, 1.2f * n); s[i][1] = t_frand(-0.2f * n, 1.2f * n); }
        s[i][2] = iwv;
        s[i][3] = depth_units * iwv;
    }
    if (sliver) {
        const float t = t_frand(0.0f, 1.0f);
        s[2][0] = s[0][0] + t * (s[1][0] - s[0][0]) + t_frand(-1e-3f, 1e-3f);
        s[2][1] = s[0][1] + t * (s[1][1] - s[0][1]) + t_frand(-1e-3f, 1e-3f);
    }
    if (t_rand() & 1u) {           /* the other winding on half of the triangles */
        float tmp[4];
        for (k = 0; k < 4; k++) { tmp[k] = s[1][k]; s[1][k] = s[2][k]; s[2][k] = tmp[k]; }
    }
}

int main(void) {
    static const unsigned sizes[] = { 64u, 128u, 256u };
    static uint16_t ref[256 * 256], got[256 * 256];
    static uint16_t row_a[ROW_MAX + 8], row_b[ROW_MAX + 8];
    int trial, k;
    unsigned long cov_checked = 0, cov_bad = 0, differ = 0, cov_diff = 0, covered = 0, dep_big = 0;
    unsigned max_diff = 0;
    double dep_max = 0.0;

    T_CHECK(SHZ_DCE_TRI_ROW_IW_FLOOR >= 1.1e-19f, "the guard floor %g is below the smallest iw whose square is normal", (double)SHZ_DCE_TRI_ROW_IW_FLOOR);

    /* -- whole triangles: the C routine against the reference row, bit for bit -- */
    for (trial = 0; trial < 20000; trial++) {
        const unsigned size = sizes[trial % 3];
        const int flat = (trial / 3) % 4 == 0;
        const int huge = trial % 5 == 0;
        const int sliver = trial % 7 == 3;
        float s[3][4];
        size_t cells = (size_t)size * size;

        random_tri(s, size, flat, huge, sliver);
        memset(ref, 0, sizeof ref);
        memset(got, 0, sizeof got);
        reference_tri(ref, size, s);
        kernel_tri(got, size, s);
        for (k = 0; k < (int)cells; k++) {
            if (ref[k] != got[k]) {
                const unsigned d = ref[k] > got[k] ? ref[k] - got[k] : got[k] - ref[k];
                differ++;
                if ((ref[k] != 0) != (got[k] != 0)) cov_diff++;
                if (d > max_diff) max_diff = d;
            }
        }
#ifndef __FAST_MATH__
        T_CHECK(!memcmp(ref, got, cells * sizeof(uint16_t)), "tri_row: the C routine differs from the reference row (trial %d, size %u, flat %d)", trial, size, flat);
#endif
    }
#ifndef __FAST_MATH__
    T_CHECK(differ == 0, "tri_row: %lu texels differ from the reference row", differ);
#endif
#ifdef __FAST_MATH__
#define TRI_ROW_BUILD "fast-math"
#else
#define TRI_ROW_BUILD "strict"
#endif
    printf("tri_row (%s build): %lu texels differ from the reference row, %lu of them in coverage, largest difference %u units\n",
           TRI_ROW_BUILD, differ, cov_diff, max_diff);

    /* -- the coverage bound and the measured depth against the exact value, on random triangles (not huge) -- */
    for (trial = 0; trial < 6000; trial++) {
        const unsigned size = sizes[trial % 3];
        const int flat = (trial / 3) % 4 == 0;
        float s[3][4];
        const float *s0, *s1, *s2;
        shz_dce_tri_row_t tri;
        int x0, x1, y0, y1, y, x, fl;

        random_tri(s, size, flat, 0, trial % 7 == 3);
        if (!build_tri(s, size, &tri, &x0, &x1, &y0, &y1, &fl, &s0, &s1, &s2)) continue;
        memset(got, 0, sizeof got);
        kernel_tri(got, size, s);
        for (y = y0; y < y1; y++) {
            const double py = (double)y + 0.5;
            double emax = 0.0;
            /* the row's edge values in double (the exact functions) and their largest magnitude, which sets the bound */
            for (x = x0; x < x1; x++) {
                const double px = (double)x + 0.5;
                const double e0 = (s1[0] - px) * (s2[1] - py) - (s2[0] - px) * (s1[1] - py);
                const double e1 = (s2[0] - px) * (s0[1] - py) - (s0[0] - px) * (s2[1] - py);
                const double e2 = (s0[0] - px) * (s1[1] - py) - (s1[0] - px) * (s0[1] - py);
                double m = fabs(e0); if (fabs(e1) > m) m = fabs(e1); if (fabs(e2) > m) m = fabs(e2);
                if (m > emax) emax = m;
            }
            for (x = x0; x < x1; x++) {
                const double px = (double)x + 0.5;
                const double e[3] = { (s1[0] - px) * (s2[1] - py) - (s2[0] - px) * (s1[1] - py),
                                      (s2[0] - px) * (s0[1] - py) - (s0[0] - px) * (s2[1] - py),
                                      (s0[0] - px) * (s1[1] - py) - (s1[0] - px) * (s0[1] - py) };
                const double tol = (double)(x1 - x0 + 1) * 6.0e-8 * (emax + 1.0);   /* count x 2^-24 x M, with headroom */
                const double lo = (double)tri.step[3];
                int clear_in = 1, clear_out = 0, i;
                const int got_cov = got[(size_t)y * size + x] != 0;

                for (i = 0; i < 3; i++) {
                    if (!(e[i] > lo + tol)) clear_in = 0;
                    if (e[i] < lo - tol) clear_out = 1;
                }
                if (clear_in || clear_out) {
                    cov_checked++;
                    if (clear_in && !got_cov) cov_bad++;     /* clearly inside: must be covered */
                    if (clear_out && got_cov) cov_bad++;     /* clearly outside: must not be */
                }
                if (got_cov) {
                    const double ex = exact_depth(s, fl, px, py);
                    const double stored = (double)got[(size_t)y * size + x];
                    const double d = fabs(stored - (ex < 1.0 ? 1.0 : ex > 65535.0 ? 65535.0 : ex));
                    covered++;
                    if (d > dep_max) dep_max = d;
                    if (d > 1.0) dep_big++;
                }
            }
        }
    }
    T_CHECK(cov_bad == 0, "tri_row: %lu texels clearly inside or outside the tolerance are covered otherwise", cov_bad);
    printf("tri_row: %lu clearly judged texels, coverage as the bound says; %lu covered texels: the C routine's depth is more than one unit "
           "from the exact depth on %lu of them, largest distance %.2f units (measured; the running sums are not bounded tighter)\n",
           cov_checked, covered, dep_big, dep_max);

    /* -- a count of 0 writes nothing; a store stays inside its row -- */
    {
        const float edge[3] = { 0.5f, 0.5f, 0.5f };
        shz_dce_tri_row_t tri;
        int j;

        memset(&tri, 0, sizeof tri);
        tri.step[3] = -EDGE_TOL;
        tri.kw[0] = tri.kw[1] = tri.kw[2] = 100.0f;
        tri.iw[0] = tri.iw[1] = tri.iw[2] = 1.0f;
        tri.iw[3] = SHZ_DCE_TRI_ROW_IW_FLOOR;
        for (j = 0; j < (int)(ROW_MAX + 8); j++) row_a[j] = 0;
        shz_dce_tri_row_step(row_a + 4, 0u, edge, &tri, 1);
        for (j = 0; j < (int)(ROW_MAX + 8); j++) T_CHECK(row_a[j] == 0, "a count of 0 wrote texel %d", j);
        shz_dce_tri_row_step(row_a + 4, 16u, edge, &tri, 1);
        for (j = 0; j < 4; j++) T_CHECK(row_a[j] == 0, "a store before the row, texel %d", j);
        for (j = 4 + 16; j < (int)(ROW_MAX + 8); j++) T_CHECK(row_a[j] == 0, "a store past the row, texel %d", j);
        /* kw 150 over iw 1.5 (three edges at 0.5, weights 1): 100 */
        T_CHECK(row_a[4] == 100u, "a covered texel stored %u, expected 100", row_a[4]);
    }

    /* -- the guard: iw at or below iw[3] stores depth 1; a flat map does not read iw -- */
    {
        const float edge[3] = { 0.5f, 0.5f, 0.5f };
        shz_dce_tri_row_t tri;
        int j;

        memset(&tri, 0, sizeof tri);
        tri.step[3] = -EDGE_TOL;
        tri.kw[0] = tri.kw[1] = tri.kw[2] = 1000.0f;
        tri.iw[0] = tri.iw[1] = tri.iw[2] = 0.0f;
        tri.iw[3] = SHZ_DCE_TRI_ROW_IW_FLOOR;
        for (j = 0; j < 8; j++) row_a[j] = 0;
        shz_dce_tri_row_step(row_a, 8u, edge, &tri, 1);
        for (j = 0; j < 8; j++) T_CHECK(row_a[j] == 1u, "guard: texel %d stored %u, expected 1", j, row_a[j]);
        tri.iw[0] = tri.iw[1] = tri.iw[2] = 1.0f;     /* iw is 1.5 now: above the guard, so the depth is kw / iw */
        for (j = 0; j < 8; j++) row_a[j] = 0;
        shz_dce_tri_row_step(row_a, 8u, edge, &tri, 1);
        for (j = 0; j < 8; j++) T_CHECK(row_a[j] == 1000u, "guard: texel %d stored %u, expected 1000 (kw 1500 over iw 1.5)", j, row_a[j]);
        for (j = 0; j < 8; j++) row_a[j] = 0;
        shz_dce_tri_row_step(row_a, 8u, edge, &tri, 0);
        for (j = 0; j < 8; j++) T_CHECK(row_a[j] == 1500u, "flat: texel %d stored %u, expected 1500 (kw 3 x 0.5 x 1000; iw is not read)", j, row_a[j]);
    }

    /* -- the dispatch name is the C routine on the host, bit for bit, both kinds of map -- */
    {
        const float edge[3] = { -0.25f, 0.5f, 0.5f };
        shz_dce_tri_row_t tri;
        int p, j;

        for (p = 0; p < 2; p++) {
            memset(&tri, 0, sizeof tri);
            tri.step[0] = 0.02f; tri.step[1] = -0.01f; tri.step[2] = -0.01f; tri.step[3] = -EDGE_TOL;
            tri.kw[0] = 3000.0f; tri.kw[1] = 9000.0f; tri.kw[2] = 1200.0f;
            tri.iw[0] = 0.5f; tri.iw[1] = 0.7f; tri.iw[2] = 0.6f; tri.iw[3] = SHZ_DCE_TRI_ROW_IW_FLOOR;
            for (j = 0; j < (int)(ROW_MAX + 8); j++) row_a[j] = row_b[j] = 0;
            shz_dce_tri_row_step(row_a, 64u, edge, &tri, p);
            shz_dce_tri_row_step_c(row_b, 64u, edge, &tri, p);
            T_CHECK(!memcmp(row_a, row_b, 64 * sizeof(uint16_t)), "dispatch %d differs from the C routine", p);
        }
    }

    /* -- the reference row against the C routine, prefilled rows (the larger of the old and the new depth), counts 0 .. ROW_MAX -- */
    {
        int c;
        for (c = 0; c <= (int)ROW_MAX; c++) {
            shz_dce_tri_row_t tri;
            float edge[3];
            int p, j;

            for (p = 0; p < 2; p++) {
                for (j = 0; j < 3; j++) edge[j] = t_frand(-0.5f, 0.5f);
                tri.step[0] = t_frand(-0.05f, 0.05f); tri.step[1] = t_frand(-0.05f, 0.05f); tri.step[2] = -(tri.step[0] + tri.step[1]);
                tri.step[3] = -EDGE_TOL;
                for (j = 0; j < 3; j++) { tri.kw[j] = t_frand(0.0f, 900.0f); tri.iw[j] = t_frand(0.1f, 2.0f); }
                tri.kw[3] = 0.0f; tri.iw[3] = SHZ_DCE_TRI_ROW_IW_FLOOR;
                for (j = 0; j < (int)(ROW_MAX + 8); j++) row_a[j] = row_b[j] = (uint16_t)(j % 3 == 0 ? 30000u : 0u);
                reference_row(row_a, (unsigned)c, edge, &tri, p);
                shz_dce_tri_row_step_c(row_b, (unsigned)c, edge, &tri, p);
                T_CHECK(!memcmp(row_a, row_b, (ROW_MAX + 8) * sizeof(uint16_t)), "count %d kind %d: the C routine differs from the reference row or wrote past it", c, p);
            }
        }
    }

    return t_done("tri_row_test");
}
