/* SH4ZAM-DCE test tool: sh4run's own test, the host side

   selftest.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   selftest <selftest_sh4.elf>
   Runs the functions of selftest_funcs.h natively and through the interpreter on their SH-4 build: every buffer and result must agree bit
   for bit. Then the vector unit against the model the interpreter was built with, the faults it must catch, and the garbage seeding.
   Build without -ffast-math and with -ffp-contract=off (as the SH-4 side), -mfma so fmaf is one instruction.
*/

#include "selftest_funcs.h"
#include "sh4run.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef SH4RUN_HW_MODEL
#include "sh4_fpu_approx.h"
#endif

static int fails;
static uint32_t lcg_state = 12345u;
static uint32_t lcg(void) { lcg_state = lcg_state * 1664525u + 1013904223u; return lcg_state; }
static float frand(float lo, float hi) { return lo + (hi - lo) * (float)(lcg() >> 8) * (1.0f / 16777216.0f); }

#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void must_call(sh4_t *s, const char *name, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    if (sh4_call(s, sh4_sym(s, name), a0, a1, a2, a3) != 0) { fails++; printf("FAIL: %s: %s\n", name, sh4_error(s)); }
}

static void must_fault(sh4_t *s, const char *name, uint32_t a0, const char *needle) {
    const int rc = sh4_call(s, sh4_sym(s, name), a0, 0, 0, 0);
    CHECK(rc != 0 && strstr(sh4_error(s), needle), "%s should fail with '%s', got rc=%d '%s'", name, needle, rc, sh4_error(s));
}

typedef uint32_t (*buf_fn)(void *, uint32_t);

static void compare_buffers(sh4_t *s, const char *name, buf_fn native, int floats, uint32_t n, int rounds) {
    int r;
    for (r = 0; r < rounds; r++) {
        const uint32_t addr = sh4_alloc(s, n * 4u, 4u);
        uint32_t *sim = (uint32_t *)sh4_ptr(s, addr, n * 4u), *host = (uint32_t *)malloc(n * 4u), i, want;
        for (i = 0; i < n; i++) {
            if (floats) { const float f = frand(-50.0f, 50.0f); memcpy(&host[i], &f, 4); }
            else host[i] = lcg();
            sim[i] = host[i];
        }
        want = native(host, n);
        must_call(s, name, addr, n, 0, 0);
        CHECK(sh4_r0(s) == want, "%s round %d: result %08x, native %08x", name, r, (unsigned)sh4_r0(s), (unsigned)want);
        CHECK(!memcmp(sim, host, n * 4u), "%s round %d: buffers differ", name, r);
        free(host);
        sh4_alloc_reset(s);
    }
}

static uint32_t n_int(void *p, uint32_t n) { return st_int((uint32_t *)p, n); }
static uint32_t n_float(void *p, uint32_t n) { return st_float((float *)p, n); }
static uint32_t n_double(void *p, uint32_t n) { return st_double((float *)p, n); }
static uint32_t n_fma(void *p, uint32_t n) { return st_fma((float *)p, n); }

int main(int argc, char **argv) {
    sh4_t *s;
    int k;
    if (argc < 2) { printf("usage: selftest <selftest_sh4.elf>\n"); return 2; }
    s = sh4_new();
    if (!s || sh4_load_elf(s, argv[1]) != 0) { printf("FAIL: load: %s\n", s ? sh4_error(s) : "no memory"); return 1; }

    /* the host's fmaf must be one fused operation, or st_fma proves nothing */
    {
        volatile float x = 1.0f + 0x1p-23f, y = 1.0f - 0x1p-23f, z = -1.0f;
        CHECK(fmaf(x, y, z) == -0x1p-46f, "the host fmaf is not fused (build with -mfma)");
    }

    compare_buffers(s, "_sh_st_int", n_int, 0, 600u, 8);
    compare_buffers(s, "_sh_st_float", n_float, 1, 600u, 8);
    compare_buffers(s, "_sh_st_double", n_double, 1, 600u, 8);
    compare_buffers(s, "_sh_st_fma", n_fma, 1, 600u, 8);

    for (k = 0; k < 200; k++) {   /* the calling convention: floats in fr5, fr4, fr7, fr6, fr9, the result in fr0 */
        const float a = frand(-9, 9), b = frand(-9, 9), c = frand(-9, 9), d = frand(-9, 9), e = frand(-9, 9);
        const int i = (int)(lcg() % 100u), j = (int)(lcg() % 100u), kk = (int)(lcg() % 100u), l = (int)(lcg() % 100u);
        const float fa[5] = { a, b, c, d, e };
        const float want = st_args(a, i, b, c, j, d, kk, e, l);
        float got;
        sh4_float_args(s, fa, 5);
        must_call(s, "_sh_st_args", (uint32_t)i, (uint32_t)j, (uint32_t)kk, (uint32_t)l);
        got = sh4_fr0(s);
        CHECK(!memcmp(&got, &want, 4), "st_args %d: %a, native %a", k, (double)got, (double)want);
    }

    /* the vector unit: operands reach the right registers, XF is column major, results land where SH4ZAM reads them */
    for (k = 0; k < 500; k++) {
        const uint32_t am = sh4_alloc(s, 64u, 8u), av = sh4_alloc(s, 16u, 4u), ao = sh4_alloc(s, 64u, 8u), ar = sh4_alloc(s, 32u, 4u);
        float *m = (float *)sh4_ptr(s, am, 64u), *v = (float *)sh4_ptr(s, av, 16u), *o = (float *)sh4_ptr(s, ao, 64u), *r = (float *)sh4_ptr(s, ar, 32u);
        float x[16];
        int i, j;
        for (i = 0; i < 16; i++) m[i] = frand(-4, 4);
        for (i = 0; i < 4; i++) v[i] = frand(-4, 4);
        must_call(s, "_sh_st_ftrv", am, av, ao, 0);
        sh4_xmtrx_get(s, x);
        CHECK(!memcmp(x, m, 64u), "ftrv %d: XMTRX is not the matrix that was loaded", k);
#ifdef SH4RUN_HW_MODEL
        {
            uint32_t mb[16], vb[4];
            memcpy(mb, m, 64u); memcpy(vb, v, 16u);
            sh4_ftrv(mb, vb, 0);
            CHECK(!memcmp(o, vb, 16u), "ftrv %d: not the model's bits", k);
        }
#endif
        for (i = 0; i < 4; i++) {   /* and the mathematics: row i of a column-major matrix times the vector */
            double e = 0.0, mag = 0.0;
            for (j = 0; j < 4; j++) { e += (double)m[4 * j + i] * (double)v[j]; mag += fabs((double)m[4 * j + i] * (double)v[j]); }
            CHECK(fabs((double)o[i] - e) <= 1e-5 * (mag + 1e-30), "ftrv %d row %d: %g, exact %g", k, i, (double)o[i], e);
        }
        must_call(s, "_sh_st_xstore", am, ao, 0, 0);
        CHECK(!memcmp(o, m, 64u), "xstore %d: the matrix did not come back", k);

        for (i = 0; i < 4; i++) { r[i] = frand(-4, 4); r[4 + i] = frand(-4, 4); }
        must_call(s, "_sh_st_fipr2", av, ar, ar + 16u, ao);
        for (i = 0; i < 2; i++) {
            double e = 0.0, mag = 0.0;
            for (j = 0; j < 4; j++) { e += (double)v[j] * (double)r[4 * i + j]; mag += fabs((double)v[j] * (double)r[4 * i + j]); }
            CHECK(fabs((double)o[i] - e) <= 1e-5 * (mag + 1e-30), "fipr2 %d.%d: %g, exact %g", k, i, (double)o[i], e);
#ifdef SH4RUN_HW_MODEL
            {
                const float m1 = sh4_fipr_f(v, r + 4 * i, 0);
                CHECK(!memcmp(&o[i], &m1, 4), "fipr2 %d.%d: not the model's bits", k, i);
            }
#endif
        }
        sh4_alloc_reset(s);
    }
    {
        const uint32_t n = 4096u, ai = sh4_alloc(s, n * 4u, 4u), ao = sh4_alloc(s, n * 8u, 4u), au = sh4_alloc(s, n * 2u, 2u);
        float *in = (float *)sh4_ptr(s, ai, n * 4u), *o = (float *)sh4_ptr(s, ao, n * 8u);
        uint16_t *u = (uint16_t *)sh4_ptr(s, au, n * 2u);
        uint32_t i;
        for (i = 0; i < n; i++) { in[i] = frand(0.001f, 1000.0f); u[i] = (uint16_t)lcg(); }
        must_call(s, "_sh_st_fsrra", ai, ao, n, 0);
        for (i = 0; i < n; i++) {
            const double e = 1.0 / sqrt((double)in[i]);
            CHECK(fabs((double)o[i] - e) <= 2e-6 * e, "fsrra %u: %g, exact %g", (unsigned)i, (double)o[i], e);
        }
        must_call(s, "_sh_st_fsca", au, ao, n, 0);
        for (i = 0; i < n; i++) {
            const double a = (double)u[i] * (6.283185307179586 / 65536.0);
            CHECK(fabs((double)o[2 * i] - sin(a)) <= 3e-6 && fabs((double)o[2 * i + 1] - cos(a)) <= 3e-6, "fsca %u: %g %g", (unsigned)i, (double)o[2 * i], (double)o[2 * i + 1]);
        }
        sh4_alloc_reset(s);
    }

    /* MOVCA.L: the 12 bytes the routine wrote after the allocation are there, the 16 it did not write are poison */
    {
        const uint32_t al = sh4_alloc(s, 64u, 32u);
        uint32_t *line = (uint32_t *)sh4_ptr(s, al, 64u);
        for (k = 0; k < 16; k++) line[k] = 0x11111111u;
        must_call(s, "_sh_st_movca", al, 0, 0, 0);
        CHECK(line[0] == 1u && line[1] == 2u && line[2] == 3u && line[3] == 4u, "movca: the written words are wrong");
        CHECK(line[4] == 0xa5a5a5a5u && line[7] == 0xa5a5a5a5u, "movca: the unwritten half of the line is not poisoned");
        CHECK(line[8] == 0x11111111u, "movca: the next line was touched");
        sh4_alloc_reset(s);
    }

    /* the faults */
    {
        const uint32_t a = sh4_alloc(s, 128u, 8u);
        must_fault(s, "_sh_st_bad_read32", a + 2u, "ADDRESS ERROR");
        must_fault(s, "_sh_st_bad_read32", a + 1u, "ADDRESS ERROR");
        must_fault(s, "_sh_st_bad_read16", a + 1u, "ADDRESS ERROR");
        must_fault(s, "_sh_st_bad_pair", a + 4u, "ADDRESS ERROR");      /* a matrix on a multiple of 4 only: the 8-byte loads stop a console */
        must_fault(s, "_sh_st_bad_read32", 0x00000100u, "outside the RAM");
        must_fault(s, "_sh_st_bad_r8", 0, "r8 not restored");
        must_fault(s, "_sh_st_bad_fr12", 0, "fr12 not restored");
        must_fault(s, "_sh_st_bad_fschg", 0, "FPSCR mode bits");
        must_fault(s, "_sh_st_bad_sp", 0, "stack pointer");
        must_call(s, "_sh_st_bad_read32", a, 0, 0, 0);                   /* and the same routine on a good address passes */
        must_call(s, "_sh_st_bad_pair", a, 0, 0, 0);
        sh4_alloc_reset(s);
    }

    /* garbage: a routine that reads a register it never wrote gets a different answer on the next call */
    {
        float g1, g2;
        must_call(s, "_sh_st_reads_garbage", 0, 0, 0, 0); g1 = sh4_fr0(s);
        must_call(s, "_sh_st_reads_garbage", 0, 0, 0, 0); g2 = sh4_fr0(s);
        CHECK(memcmp(&g1, &g2, 4) != 0, "two calls gave the same garbage");
        CHECK((g1 != g1) != (g2 != g2), "one of two consecutive calls should start with NaNs in the scratch float registers");
    }

    /* the timing model on a loop it can be checked by hand on: see the README for the figures */
    {
        const sh4_stats_t *st;
        const uint32_t n = 64u, ai = sh4_alloc(s, n * 4u, 4u), ao = sh4_alloc(s, n * 4u, 4u);
        float *in = (float *)sh4_ptr(s, ai, n * 4u);
        uint32_t i;
        uint64_t c1, c2;
        for (i = 0; i < n; i++) in[i] = 2.0f;
        must_call(s, "_sh_st_fsrra", ai, ao, 32u, 0); st = sh4_stats(s); c1 = st->cycles;
        must_call(s, "_sh_st_fsrra", ai, ao, 64u, 0); st = sh4_stats(s); c2 = st->cycles;
        CHECK(st->fsrra == 64u, "fsrra count %u", (unsigned)st->fsrra);
        CHECK(c2 > c1 && (c2 - c1) / 32u >= 3u && (c2 - c1) / 32u <= 12u, "fsrra loop: %llu cycles an element is not plausible", (unsigned long long)((c2 - c1) / 32u));
        printf("timing: fsrra loop %.2f cycles an element (ESTIMATE)\n", (double)(c2 - c1) / 32.0);
        sh4_alloc_reset(s);
    }

    printf("hw model: %s\n", sh4_hw_model() ? "bit-exact FIPR / FTRV / FSRRA / FSCA" : "plain sums (not the console's bits)");
    if (fails) { printf("sh4run selftest: %d FAILED\n", fails); return 1; }
    printf("sh4run selftest: ok\n");
    sh4_free(s);
    return 0;
}
