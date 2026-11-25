/* SH4ZAM-DCE tests: shz_dce_tri_row.h, the loops its cycle ESTIMATES are made from

   tri_row_bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc with the fast-math flags and run by tests/sh4run/sh4bench: every bench_ function is
   `void f(out, in0, in1, count)` and runs one row of `count` texels. The triangle's constants are fixed per function and pass through
   shz_dce_opaquef (the compiler must not learn a value, or it proves a path dead and the bench times nothing). Each shape runs twice,
   every texel covered and every texel missed, so the per-texel cost of each path is read on its own.
   The table reads: the BEFORE row (the running-add row the C form replaces, copied here unchanged), the C form, and the SH-4 loop.
   The figures are the interpreter's pipeline model (an ESTIMATE, cache hits assumed), not a console's.
*/

#include <sh4zam_dce/shz_dce_tri_row.h>

#define R SHZ_RESTRICT

#define TOL_NEG (-1.0e-3f)

static void make_tri(shz_dce_tri_row_t *t, const float *w) {
    const float wob = shz_dce_opaquef(w[0]) * 1.0e-9f;
    const float A = shz_dce_opaquef(100.0f);

    t->step[0] = wob; t->step[1] = -wob; t->step[2] = shz_dce_opaquef(0.0f); t->step[3] = shz_dce_opaquef(TOL_NEG);
    t->kw[0] = shz_dce_opaquef(3000.0f) / A + wob; t->kw[1] = shz_dce_opaquef(6000.0f) / A; t->kw[2] = shz_dce_opaquef(1500.0f) / A;
    t->kw[3] = 0.0f;
    t->iw[0] = shz_dce_opaquef(0.5f) / A; t->iw[1] = shz_dce_opaquef(0.7f) / A; t->iw[2] = shz_dce_opaquef(0.6f) / A;
    t->iw[3] = shz_dce_opaquef(SHZ_DCE_TRI_ROW_IW_FLOOR);
}

static void edges(float e[3], const float *w, int cov) {
    const float wob = shz_dce_opaquef(w[1]) * 1.0e-9f;
    if (cov) { e[0] = shz_dce_opaquef(40.0f) + wob; e[1] = shz_dce_opaquef(30.0f); e[2] = shz_dce_opaquef(30.0f); }
    else     { e[0] = shz_dce_opaquef(-40.0f) + wob; e[1] = shz_dce_opaquef(-30.0f); e[2] = shz_dce_opaquef(-30.0f); }
}

/* ---- BEFORE: the running-add row the C form replaces (five adds a texel; the row start from the constants) ---- */

static void before_row(uint16_t *R row, size_t n, const float edge[3], const shz_dce_tri_row_t *t, int persp) {
    const float e0x = t->step[0], e1x = t->step[1], e2x = t->step[2], lo = t->step[3];
    const float a0 = t->iw[0], a1 = t->iw[1], a2 = t->iw[2], k0 = t->kw[0], k1 = t->kw[1], k2 = t->kw[2];
    const float dwx = e0x * a0 + e1x * a1 + e2x * a2, dkx = e0x * k0 + e1x * k1 + e2x * k2;
    float e0 = edge[0], e1 = edge[1], e2 = edge[2];
    float iw = e0 * a0 + e1 * a1 + e2 * a2, kw = e0 * k0 + e1 * k1 + e2 * k2;
    size_t x;

    for (x = 0; x < n; x++) {
        if (e0 >= lo && e1 >= lo && e2 >= lo) {
            float q = kw;

            if (persp) q = iw > t->iw[3] ? kw * shz_invf_fsrra(iw) : 0.0f;
            q = shz_clampf(q, 1.0f, 65535.0f);
            if ((uint16_t)q > row[x]) row[x] = (uint16_t)q;
        }
        e0 += e0x; e1 += e1x; e2 += e2x; iw += dwx; kw += dkx;
    }
}

void bench_tri_row_step_before_persp_cov(uint16_t *R out, const float *R in0, const float *R in1, size_t n) {
    shz_dce_tri_row_t t; float e[3];
    make_tri(&t, in1); edges(e, in0, 1);
    before_row(out, n, e, &t, 1);
}

void bench_tri_row_step_before_persp_rej(uint16_t *R out, const float *R in0, const float *R in1, size_t n) {
    shz_dce_tri_row_t t; float e[3];
    make_tri(&t, in1); edges(e, in0, 0);
    before_row(out, n, e, &t, 1);
}

void bench_tri_row_step_before_flat_cov(uint16_t *R out, const float *R in0, const float *R in1, size_t n) {
    shz_dce_tri_row_t t; float e[3];
    make_tri(&t, in1); edges(e, in0, 1);
    before_row(out, n, e, &t, 0);
}

void bench_tri_row_step_before_flat_rej(uint16_t *R out, const float *R in0, const float *R in1, size_t n) {
    shz_dce_tri_row_t t; float e[3];
    make_tri(&t, in1); edges(e, in0, 0);
    before_row(out, n, e, &t, 0);
}

/* ---- the C form: shz_dce_tri_row_step_c ---- */

void bench_tri_row_step_c_persp_cov(uint16_t *R out, const float *R in0, const float *R in1, size_t n) {
    shz_dce_tri_row_t t; float e[3];
    make_tri(&t, in1); edges(e, in0, 1);
    shz_dce_tri_row_step_c(out, (unsigned)n, e, &t, 1);
}

void bench_tri_row_step_c_persp_rej(uint16_t *R out, const float *R in0, const float *R in1, size_t n) {
    shz_dce_tri_row_t t; float e[3];
    make_tri(&t, in1); edges(e, in0, 0);
    shz_dce_tri_row_step_c(out, (unsigned)n, e, &t, 1);
}

void bench_tri_row_step_c_flat_cov(uint16_t *R out, const float *R in0, const float *R in1, size_t n) {
    shz_dce_tri_row_t t; float e[3];
    make_tri(&t, in1); edges(e, in0, 1);
    shz_dce_tri_row_step_c(out, (unsigned)n, e, &t, 0);
}

void bench_tri_row_step_c_flat_rej(uint16_t *R out, const float *R in0, const float *R in1, size_t n) {
    shz_dce_tri_row_t t; float e[3];
    make_tri(&t, in1); edges(e, in0, 0);
    shz_dce_tri_row_step_c(out, (unsigned)n, e, &t, 0);
}

/* ---- the SH-4 loop: shz_dce_tri_row_step_sh4 ---- */

void bench_tri_row_step_sh4_persp_cov(uint16_t *R out, const float *R in0, const float *R in1, size_t n) {
    shz_dce_tri_row_t t; float e[3];
    make_tri(&t, in1); edges(e, in0, 1);
    shz_dce_tri_row_step_sh4(out, (unsigned)n, e, &t, 1);
}

void bench_tri_row_step_sh4_persp_rej(uint16_t *R out, const float *R in0, const float *R in1, size_t n) {
    shz_dce_tri_row_t t; float e[3];
    make_tri(&t, in1); edges(e, in0, 0);
    shz_dce_tri_row_step_sh4(out, (unsigned)n, e, &t, 1);
}

void bench_tri_row_step_sh4_flat_cov(uint16_t *R out, const float *R in0, const float *R in1, size_t n) {
    shz_dce_tri_row_t t; float e[3];
    make_tri(&t, in1); edges(e, in0, 1);
    shz_dce_tri_row_step_sh4(out, (unsigned)n, e, &t, 0);
}

void bench_tri_row_step_sh4_flat_rej(uint16_t *R out, const float *R in0, const float *R in1, size_t n) {
    shz_dce_tri_row_t t; float e[3];
    make_tri(&t, in1); edges(e, in0, 0);
    shz_dce_tri_row_step_sh4(out, (unsigned)n, e, &t, 0);
}
