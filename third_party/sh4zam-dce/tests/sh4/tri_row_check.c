/* SH4ZAM-DCE tests: shz_dce_tri_row.h as the SH-4 runs it

   tri_row_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   tri_row_check <tri_row_kernels.elf>
   The ELF is tri_row_kernels.c built by sh-elf-gcc; tests/sh4run executes it instruction by instruction, with the bit-exact FPU models
   when they are present (the vector unit line says which). Per row:
   - the C form is bit-identical to the reference row (the running-add loop it replaces), on the SH-4
   - the dispatch name gives the SH-4 loop's bits (it is the same routine)
   - the SH-4 loop: a texel the edges clearly cover is covered, one they clearly miss is not, and a covered texel's depth is within one
     unit of floor(exact depth), exact in double from the same edges and constants (the FIPR bound of the header)
   - the C form covers the same texels as the SH-4 loop on the clearly judged texels
   - the row keeps the larger of the stored and the new depth (a row prefilled with 30000 keeps it where the depth is lower)
   - the bytes past the row are untouched; a count of 0 touches nothing
   - every call twice, the scratch registers seeded differently: the same bytes both times
   - XMTRX is unchanged (the routines do not load it)
   The data sits on the weakest alignment the header allows (rows on a multiple of 2 that is not of 4, the 48-byte record on a
   multiple of 4 that is not of 8), so the interpreter also proves the loops need no more.
*/

#include <sh4zam_dce/shz_dce_tri_row.h>

#include "tri_row_jobs.h"
#include "sh4_check.h"

#define EPS_CLEAR 1.0e-4   /* a texel this close to an edge's tolerance is not judged on its coverage */
#define ROW_MAX 64u

/* a random triangle's constants, the way a real triangle's are: the three edge functions sum to the area A (their weights are
   barycentric, one may be negative), their steps sum to zero, the depths are K_i and 1 / w_i, and the constants carry 1 / A */
static void random_case(shz_dce_tri_row_t *t, float e0[3], int persp) {
    const float A = t_frand(5.0f, 300.0f);
    float b[3], K[3], iwc[3];
    int i;

    b[0] = t_frand(-0.2f, 1.0f);
    b[1] = t_frand(-0.2f, 1.0f);
    b[2] = 1.0f - b[0] - b[1];
    for (i = 0; i < 3; i++) e0[i] = A * b[i];
    t->step[0] = t_frand(-3.0f, 3.0f);
    t->step[1] = t_frand(-3.0f, 3.0f);
    t->step[2] = -(t->step[0] + t->step[1]);
    t->step[3] = -1.0e-3f;
    for (i = 0; i < 3; i++) {
        K[i] = t_frand(1.0f, 60000.0f);
        iwc[i] = t_frand(0.1f, 2.0f);
        if (persp) {
            t->kw[i] = K[i] * iwc[i] / A;
            t->iw[i] = iwc[i] / A;
        } else {
            t->kw[i] = K[i] / A;
            t->iw[i] = t_frand(-1.0f, 1.0f);   /* a flat map never reads it: any value */
        }
    }
    t->kw[3] = t_frand(-5.0f, 5.0f);            /* the fourth element is never read */
    t->iw[3] = SHZ_DCE_TRI_ROW_IW_FLOOR;
}

/* the exact value at texel x, in double, from the edges as exact arithmetic gives them (the barycentric form) */
static double exact_q(const shz_dce_tri_row_t *t, const float e0[3], int persp, uint32_t x, double e_out[3]) {
    double kw = 0.0, iw = 0.0;
    int i;

    for (i = 0; i < 3; i++) {
        const double e = (double)e0[i] + (double)x * (double)t->step[i];
        e_out[i] = e;
        kw += e * (double)t->kw[i];
        iw += e * (double)t->iw[i];
    }
    if (!persp) return kw;
    return iw > (double)t->iw[3] ? kw / iw : 0.0;
}

/* the value a texel stores: clamped to 1 .. 65535, truncated */
static double stored_of(double q) {
    q = q < 1.0 ? 1.0 : q > 65535.0 ? 65535.0 : q;
    return floor(q);
}

/* Judges one row's coverage and depth against the exact answer (the SH-4 loop's bound). prefill: the value the row starts at. */
static int judge(const uint16_t *got, const shz_dce_tri_row_t *t, const float e0[3], int persp, uint32_t count, uint16_t prefill,
                 unsigned long *n_in, unsigned long *n_out) {
    const double lo = (double)t->step[3];
    uint32_t x;
    int bad = 0;

    for (x = 0; x < count; x++) {
        double e[3], q, s;
        int clear_in = 1, clear_out = 0, i;

        q = exact_q(t, e0, persp, x, e);
        s = stored_of(q);
        for (i = 0; i < 3; i++) {
            if (!(e[i] > lo + EPS_CLEAR)) clear_in = 0;
            if (e[i] < lo - EPS_CLEAR) clear_out = 1;
        }
        if (clear_out) {
            if (n_out) (*n_out)++;
            if (got[x] != prefill) bad++;
            continue;
        }
        if (clear_in) {
            if (n_in) (*n_in)++;
            if (prefill == 0u) {
                if (got[x] == 0u || fabs((double)got[x] - s) > 1.0) bad++;
            } else if (s <= (double)prefill - 2.0) {
                if (got[x] != prefill) bad++;                  /* the depth is lower: the row keeps its prefill */
            } else if (s >= (double)prefill + 2.0) {
                if (fabs((double)got[x] - s) > 1.0) bad++;     /* the depth is higher: it is stored */
            } else if (got[x] != prefill && fabs((double)got[x] - s) > 1.0) {
                bad++;                                         /* in the band the answer is either */
            }
            continue;
        }
        /* a texel in the band between the two: covered or not; a covered one's depth is within a unit */
        if (got[x] != prefill && fabs((double)got[x] - s) > 1.0) bad++;
    }
    return bad;
}

/* the C form's coverage against the SH-4 loop's, on the texels both judge clearly: the same texels pass */
static int same_coverage(const uint16_t *c, const uint16_t *a, const shz_dce_tri_row_t *t, const float e0[3], uint32_t count, uint16_t prefill) {
    const double lo = (double)t->step[3];
    uint32_t x;
    int bad = 0;

    for (x = 0; x < count; x++) {
        double e[3];
        int clear = 1, i;

        exact_q(t, e0, 0, x, e);
        for (i = 0; i < 3; i++) if (fabs(e[i] - lo) <= EPS_CLEAR) clear = 0;
        if (clear && ((c[x] != prefill) != (a[x] != prefill))) bad++;
    }
    return bad;
}

static void test_rows(void) {
    static const uint32_t counts[] = { 0u, 1u, 2u, 3u, 7u, 16u, 31u, 64u };
    int trial;
    unsigned long checked = 0, n_in = 0, n_out = 0, asm_bad = 0, c_cov_bad = 0, asm_vs_c = 0, dispatch_bad = 0, ref_bad = 0;

    for (trial = 0; trial < 6000; trial++) {
        const int persp = trial & 1;
        const uint32_t count = trial % 9 == 8 ? t_rand() % (ROW_MAX + 1u) : counts[trial % 8];
        const uint16_t prefill = trial % 3 == 0 ? 30000u : 0u;
        const uint32_t bytes = count * 2u + 8u;
        const uint32_t aj = ram(sizeof(job_tri_row_t), 4, 0), ae = ram(12, 4, 0), at = ram(48, 8, 4);
        const uint32_t ra = ram(bytes, 2, 2), rc = ram(bytes, 2, 2), rd = ram(bytes, 2, 2), rr = ram(bytes, 2, 2);
        job_tri_row_t *j = (job_tri_row_t *)host(aj, sizeof *j);
        float *ep = (float *)host(ae, 12);
        shz_dce_tri_row_t t;
        float e0[3];
        uint16_t *a = (uint16_t *)host(ra, bytes), *c = (uint16_t *)host(rc, bytes), *d = (uint16_t *)host(rd, bytes),
                 *r = (uint16_t *)host(rr, bytes);
        uint16_t first_a[ROW_MAX], first_c[ROW_MAX], first_d[ROW_MAX];
        float xm_before[16], xm_after[16];
        uint32_t k;

        random_case(&t, e0, persp);
        memcpy(host(at, 48), &t, 48);
        memcpy(ep, e0, 12);
        j->row_asm = ra; j->row_c = rc; j->row_d = rd; j->row_ref = rr; j->count = count; j->edge = ae; j->tri = at; j->perspective = (uint32_t)persp;

        /* prefilled rows, the bytes past the row marked */
        memset(host(ra, bytes), 0xee, bytes);
        memset(host(rc, bytes), 0xee, bytes);
        memset(host(rd, bytes), 0xee, bytes);
        memset(host(rr, bytes), 0xee, bytes);
        for (k = 0; k < count; k++) { a[k] = prefill; c[k] = prefill; d[k] = prefill; r[k] = prefill; }

        sh4_xmtrx_get(S, xm_before);
        call("_t_tri_row", aj);
        sh4_xmtrx_get(S, xm_after);
        T_CHECK(!memcmp(xm_before, xm_after, 64), "tri_row: XMTRX changed by the row walk (count %u)", count);
        for (k = count; k < count + 4u; k++)
            T_CHECK(a[k] == 0xeeeeu && c[k] == 0xeeeeu && d[k] == 0xeeeeu && r[k] == 0xeeeeu, "tri_row: a store past the row (count %u, texel %u)", count, k);

        if (memcmp(d, a, count * 2u)) dispatch_bad++;
        T_CHECK(memcmp(d, a, count * 2u) == 0, "tri_row: the dispatch name differs from the SH-4 loop (count %u, persp %d)", count, persp);
        T_CHECK(memcmp(c, r, count * 2u) == 0, "tri_row: the C form differs from the reference row on the SH-4 (count %u, persp %d)", count, persp);
        if (memcmp(c, r, count * 2u)) ref_bad++;
        memcpy(first_a, a, count * 2u);
        memcpy(first_c, c, count * 2u);
        memcpy(first_d, d, count * 2u);

        {
            const int ba = judge(a, &t, e0, persp, count, prefill, &n_in, &n_out);

            asm_bad += (unsigned long)ba;
            T_CHECK(ba == 0, "tri_row: SH-4 loop: %d texels off the exact coverage or depth (count %u, persp %d, prefill %u)", ba, count, persp, (unsigned)prefill);
            {
                const int bc = same_coverage(c, a, &t, e0, count, prefill);
                c_cov_bad += (unsigned long)bc;
                T_CHECK(bc == 0, "tri_row: the C form and the SH-4 loop cover different texels (count %u, persp %d)", count, persp);
            }
        }
        for (k = 0; k < count; k++) if (a[k] != c[k]) asm_vs_c++;
        checked += count;

        /* again, the scratch registers seeded differently by the first run's leftovers: the same bytes */
        for (k = 0; k < count; k++) { a[k] = prefill; c[k] = prefill; d[k] = prefill; }
        call("_t_tri_row", aj);
        T_CHECK(memcmp(first_a, a, count * 2u) == 0, "tri_row: a second run gave other SH-4 depths (count %u): a register read before it was written", count);
        T_CHECK(memcmp(first_c, c, count * 2u) == 0, "tri_row: a second run gave other C-form depths (count %u)", count);
        T_CHECK(memcmp(first_d, d, count * 2u) == 0, "tri_row: a second run gave other dispatch depths (count %u)", count);
        sh4_alloc_reset(S);
    }
    printf("tri_row: %lu texels run; the dispatch name equal to the SH-4 loop on all of them (%lu rows differed); the C form equal to the "
           "reference row on the SH-4 (%lu rows differed); clearly covered %lu, clearly missed %lu texels judged; SH-4 loop off by %lu; "
           "C form and SH-4 loop cover different clearly judged texels %lu times; the SH-4 loop and the C form differ on %lu texels "
           "(FIPR against running sums)\n", checked, dispatch_bad, ref_bad, n_in, n_out, asm_bad, c_cov_bad, asm_vs_c);
}

int main(int argc, char **argv) {
    const int rc = check_open(argc, argv);
    if (rc) return rc;
    test_rows();
    return t_done("tri_row_check");
}
