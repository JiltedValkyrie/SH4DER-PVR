/* SH4ZAM-DCE tests: shz_dce_quat.h as the SH-4 runs it

   quat_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   quat_check <quat_kernels.elf>
   The ELF is quat_kernels.c built by sh-elf-gcc; tests/sh4run executes it instruction by instruction. For each routine, over random
   arrays of 0 to 40 elements on a multiple of 4 (the header allows 0 or 4 mod 8: both are used, alternating):
   - the SH-4 asm loop and the plain C form against the exact value computed in double, inside the bound the header states
     (the normalised lerp's sign rule: the shorter way, either way inside the band of zero)
   - the SH4ZAM routine on the same input: measured, printed (not promised bit for bit)
   - every call twice: the same bytes; the guard bytes around each output still hold; XMTRX is unchanged (all three routines)
   - a length probe, printed: the normalise at lengths 1e-14 to 1e-18 against the exact unit quaternion (1e-14 and 1e-15 checked)
   - the interpreter itself stops a run on an unaligned load or store, a callee-saved register or FPSCR mode bit not put back, so a pass
     here is also the "alignment: checked" of every instruction the routines executed
*/

#include "quat_jobs.h"
#include "sh4_check.h"

#define NLERP_BOUND 1.9073486328125e-06    /* 2^-19 */
#define MULT_FACTOR 9.5367431640625e-07    /* 2^-20 */
#define NORM_BOUND  1.9073486328125e-06    /* 2^-19 */
#define SIGN_BAND   9.5367431640625e-07    /* 2^-20 */
#define GUARD_BYTE  0xa5
#define MAXN        40
#define ULP_ONE     1.1920928955078125e-07 /* 2^-23: one unit in the last place of 1.0 */

static double drand(double lo, double hi) {
    return lo + (hi - lo) * (double)(t_rand() >> 8) * (1.0 / 16777216.0);
}

static void qrand_unit(float q[4]) {
    double c[4], n;
    int i;
    do {
        n = 0.0;
        for (i = 0; i < 4; i++) { c[i] = drand(-1.0, 1.0); n += c[i] * c[i]; }
    } while (n < 0.01);
    n = sqrt(n);
    for (i = 0; i < 4; i++) q[i] = (float)(c[i] / n);
}

static void qrand_any(float q[4]) {
    const float s = (float)drand(0.5, 3.0);
    int i;
    qrand_unit(q);
    for (i = 0; i < 4; i++) q[i] *= s;
}

/* a unit quaternion (nearly) orthogonal to the unit quaternion a */
static void qrand_orth(const float a[4], float b[4]) {
    double v[4], d = 0.0, n = 0.0;
    int i;
    for (i = 0; i < 4; i++) v[i] = drand(-1.0, 1.0);
    for (i = 0; i < 4; i++) d += v[i] * a[i];
    for (i = 0; i < 4; i++) v[i] -= d * a[i];
    for (i = 0; i < 4; i++) n += v[i] * v[i];
    n = sqrt(n);
    for (i = 0; i < 4; i++) b[i] = (float)(v[i] / n);
}

static double dot4(const float *p, const float *q) {
    return (double)p[0] * q[0] + (double)p[1] * q[1] + (double)p[2] * q[2] + (double)p[3] * q[3];
}

/* the dot product of the unit directions: the sign band is a test on this, whatever the lengths of the inputs */
static double dot_dir(const float *p, const float *q) {
    return dot4(p, q) / (sqrt(dot4(p, p)) * sqrt(dot4(q, q)));
}

/* multiply a quaternion by a factor, componentwise (the factor rounds to float once) */
static void scale4(float *p, double s) {
    int i;
    for (i = 0; i < 4; i++) p[i] = (float)((double)p[i] * s);
}

/* the exact normalised lerp in double: b multiplied by sign (+1 or -1) in the blend */
static void exact_nlerp(const float *a, const float *b, double t, double sign, double out[4]) {
    double q[4], n = 0.0;
    int i;
    for (i = 0; i < 4; i++) q[i] = (1.0 - t) * (double)a[i] + t * sign * (double)b[i];
    for (i = 0; i < 4; i++) n += q[i] * q[i];
    n = sqrt(n);
    for (i = 0; i < 4; i++) out[i] = q[i] / n;
}

static double max_abs_diff4(const float *got, const double *want) {
    double m = 0.0;
    int i;
    for (i = 0; i < 4; i++) {
        const double e = fabs((double)got[i] - want[i]);
        if (e > m) m = e;
    }
    return m;
}

static double max_abs_diff_f4(const float *got, const float *want) {
    double m = 0.0;
    int i;
    for (i = 0; i < 4; i++) {
        const double e = fabs((double)got[i] - (double)want[i]);
        if (e > m) m = e;
    }
    return m;
}

/* an output array of count elements: 16 guard bytes before it, 16 + the array + 16 after it; the array starts 16 bytes into the block.
   off: 4 (the array on 4 mod 8, the weakest the header allows) or 0 (on 8): the header allows either, the test uses both */
#define OUT_PAD 48u
static uint32_t out_block(uint32_t count, uint32_t off) {
    return ram(count * 16u + OUT_PAD, 8, off) + 16u;
}

static void guard_fill(uint8_t *block_host, uint32_t count) {
    memset(block_host, GUARD_BYTE, count * 16u + OUT_PAD);
}

/* the 16 bytes before and the 16 after the output still hold the guard byte */
static int guard_ok(const uint8_t *block_host, uint32_t count) {
    uint32_t i;
    for (i = 0; i < 16u; i++)
        if (block_host[i] != GUARD_BYTE) return 0;
    for (i = 16u + 16u * count; i < count * 16u + OUT_PAD; i++)
        if (block_host[i] != GUARD_BYTE) return 0;
    return 1;
}

static void fill_float4(float *dst, const float *src) {
    dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3];
}

/* ---- the normalised lerp ---- */

static long nl_checked, nl_band, nl_took_neg, nl_sign_differs;
static double nl_worst_band_dot, nl_asm_orig, nl_asm_c;

static void check_nlerp_elem(const float *a, const float *b, float t, const float *o, const char *what, int trial, double *worst) {
    double pos[4], neg[4], d, e_pos, e_neg, e_right;

    exact_nlerp(a, b, (double)t, 1.0, pos);
    exact_nlerp(a, b, (double)t, -1.0, neg);
    d = dot_dir(a, b);
    e_pos = max_abs_diff4(o, pos);
    e_neg = max_abs_diff4(o, neg);
    nl_checked++;
    if (d > SIGN_BAND) {
        e_right = e_pos;
    } else if (d < -SIGN_BAND) {
        e_right = e_neg;
    } else {
        e_right = e_pos < e_neg ? e_pos : e_neg;
        nl_band++;
        if (fabs(d) > nl_worst_band_dot) nl_worst_band_dot = fabs(d);
        if (e_neg <= NLERP_BOUND && e_pos > NLERP_BOUND) nl_took_neg++;
    }
    if (e_right > *worst) *worst = e_right;
    T_CHECK(e_right <= NLERP_BOUND, "%s trial %d: nlerp error %.9g over the bound %.9g (dot %.9g, t %.9g)", what, trial, e_right,
            NLERP_BOUND, d, (double)t);
}

static void test_nlerp(void) {
    int trial;
    double worst_asm = 0.0, worst_c = 0.0;

    for (trial = 0; trial < 300; trial++) {
        const uint32_t count = (uint32_t)(trial % (MAXN + 1));   /* every count 0 to 40 in turn: every remainder class */
        const uint32_t aj = ram(sizeof(job_nlerp_t), 4, 0);
        const uint32_t off = (trial & 1) ? 0u : 4u;
        const uint32_t aa = ram(count * 16u + 16u, 8, off), ab = ram(count * 16u + 16u, 8, off);
        const uint32_t oa = out_block(count, off), oc = out_block(count, off), oo = out_block(count, off);
        job_nlerp_t *j = (job_nlerp_t *)host(aj, sizeof *j);
        float *A = (float *)host(aa, count * 16u + 16u), *B = (float *)host(ab, count * 16u + 16u);
        uint8_t *ga = (uint8_t *)host(oa - 16u, count * 16u + OUT_PAD), *gc = (uint8_t *)host(oc - 16u, count * 16u + OUT_PAD),
                *go = (uint8_t *)host(oo - 16u, count * 16u + OUT_PAD);
        float *OA = (float *)host(oa, count * 16u + 16u), *OC = (float *)host(oc, count * 16u + 16u),
              *OO = (float *)host(oo, count * 16u + 16u);
        const float t = (trial % 6 == 0) ? (float)(trial / 6 % 2) : (float)drand(0.0, 1.0);   /* 0 and 1 included */
        float x0[16], x1[16], run1[MAXN * 4];
        uint32_t k;

        for (k = 0; k < count; k++) {
            float *a = A + 4 * k, *b = B + 4 * k;
            qrand_unit(a);
            switch (k % 8) {
            case 0:  fill_float4(b, a); break;
            case 1:  b[0] = -a[0]; b[1] = -a[1]; b[2] = -a[2]; b[3] = -a[3]; break;
            case 2:  qrand_orth(a, b); break;
            default: qrand_unit(b); break;
            }
            if (k % 3 == 1) {   /* a third of the elements at a length between 1e-15 and 1e15 (the same length for both) */
                const double sc = pow(10.0, drand(-15.0, 15.0));
                scale4(a, sc);
                scale4(b, sc);
            }
        }
        guard_fill(ga, count);
        guard_fill(gc, count);
        guard_fill(go, count);
        j->a = aa; j->b = ab; j->out_asm = oa; j->out_c = oc; j->out_orig = oo; j->count = count;
        {
            union { uint32_t u; float f; } tc;
            tc.f = t;
            j->t = tc.u;
        }
        sh4_xmtrx_get(S, x0);
        call("_t_nlerp", aj);
        sh4_xmtrx_get(S, x1);
        T_CHECK(!memcmp(x0, x1, 64), "nlerp trial %d: XMTRX changed", trial);
        T_CHECK(guard_ok(ga, count), "nlerp trial %d count %u: the asm loop wrote outside its output", trial, (unsigned)count);
        T_CHECK(guard_ok(gc, count), "nlerp trial %d count %u: the C form wrote outside its output", trial, (unsigned)count);
        T_CHECK(guard_ok(go, count), "nlerp trial %d count %u: the SH4ZAM loop wrote outside its output", trial, (unsigned)count);
        for (k = 0; k < count; k++) {
            const double d = dot_dir(A + 4 * k, B + 4 * k);
            const double dv = max_abs_diff_f4(OA + 4 * k, OO + 4 * k);
            check_nlerp_elem(A + 4 * k, B + 4 * k, t, OA + 4 * k, "asm", trial, &worst_asm);
            check_nlerp_elem(A + 4 * k, B + 4 * k, t, OC + 4 * k, "C form", trial, &worst_c);
            if (d > SIGN_BAND || d < -SIGN_BAND) {
                /* both pick the same shorter way here: the difference is the arithmetic's alone */
                if (dv > nl_asm_orig) nl_asm_orig = dv;
            } else if (dv > NLERP_BOUND) {
                nl_sign_differs++;      /* in the band: SH4ZAM's loop and ours may take the other way */
            }
            if (d > SIGN_BAND || d < -SIGN_BAND) {   /* outside the band only: inside it the two signs may differ by up to 2 */
                const double dc = max_abs_diff_f4(OA + 4 * k, OC + 4 * k);
                if (dc > nl_asm_c) nl_asm_c = dc;
            }
        }
        memcpy(run1, OA, count * 16u);
        call("_t_nlerp", aj);   /* again: other garbage in the scratch registers */
        T_CHECK(!memcmp(run1, OA, count * 16u), "nlerp trial %d: a second run gave other bytes", trial);
        sh4_alloc_reset(S);
    }
    printf("nlerp: asm and C form against the exact nlerp: %ld results, largest error asm %.3g, C form %.3g (bound %.3g)\n", nl_checked,
           worst_asm, worst_c, NLERP_BOUND);
    printf("nlerp: %ld results inside the sign band (|a . b| <= %.3g, largest %.3g), %ld took the other way; asm vs SH4ZAM loop, outside the band, largest %.3g (%.2f units of 2^-23); in the band, %ld took a different way from SH4ZAM's loop; asm vs C form, outside the band, largest %.3g\n",
           nl_band, SIGN_BAND, nl_worst_band_dot, nl_took_neg, nl_asm_orig, nl_asm_orig / ULP_ONE, nl_sign_differs, nl_asm_c);
}

/* ---- the Hamilton product ---- */

static long mu_checked, mu_elems, mu_same_bits;
static double mu_worst_ratio, mu_asm_orig, mu_asm_c;

static void check_mult_elem(const float *p, const float *q, const float *o, const char *what, int trial) {
    /* the four products of each component, in double; P is the largest of them */
    const double m[4][4] = {
        { (double)p[0] * q[0], (double)p[1] * q[1], (double)p[2] * q[2], (double)p[3] * q[3] },
        { (double)p[0] * q[1], (double)p[1] * q[0], (double)p[2] * q[3], (double)p[3] * q[2] },
        { (double)p[0] * q[2], (double)p[1] * q[3], (double)p[2] * q[0], (double)p[3] * q[1] },
        { (double)p[0] * q[3], (double)p[1] * q[2], (double)p[2] * q[1], (double)p[3] * q[0] },
    };
    const double r[4] = {
        m[0][0] - m[0][1] - m[0][2] - m[0][3],
        m[1][0] + m[1][1] + m[1][2] - m[1][3],
        m[2][0] - m[2][1] + m[2][2] + m[2][3],
        m[3][0] + m[3][1] - m[3][2] + m[3][3],
    };
    int c, j;
    for (c = 0; c < 4; c++) {
        double P = 0.0;
        const double err = fabs((double)o[c] - r[c]);
        for (j = 0; j < 4; j++) if (fabs(m[c][j]) > P) P = fabs(m[c][j]);
        T_CHECK(err <= (P + fabs(r[c])) * MULT_FACTOR, "%s trial %d comp %d: error %.9g over (P + |R|) x 2^-20 = %.9g", what, trial, c,
                err, (P + fabs(r[c])) * MULT_FACTOR);
        if (P + fabs(r[c]) > 0.0 && err / (P + fabs(r[c])) > mu_worst_ratio) mu_worst_ratio = err / (P + fabs(r[c]));
    }
    mu_checked++;
}

static void test_mult(void) {
    int trial;
    for (trial = 0; trial < 300; trial++) {
        const uint32_t count = (uint32_t)(trial % (MAXN + 1));   /* every count 0 to 40 in turn: every remainder class */
        const uint32_t aj = ram(sizeof(job_mult_t), 4, 0);
        const uint32_t off = (trial & 1) ? 0u : 4u;
        const uint32_t aa = ram(count * 16u + 16u, 8, off), ab = ram(count * 16u + 16u, 8, off);
        const uint32_t oa = out_block(count, off), oc = out_block(count, off), oo = out_block(count, off);
        float x0[16], x1[16];
        job_mult_t *j = (job_mult_t *)host(aj, sizeof *j);
        float *A = (float *)host(aa, count * 16u + 16u), *B = (float *)host(ab, count * 16u + 16u);
        uint8_t *ga = (uint8_t *)host(oa - 16u, count * 16u + OUT_PAD), *gc = (uint8_t *)host(oc - 16u, count * 16u + OUT_PAD),
                *go = (uint8_t *)host(oo - 16u, count * 16u + OUT_PAD);
        float *OA = (float *)host(oa, count * 16u + 16u), *OC = (float *)host(oc, count * 16u + 16u),
              *OO = (float *)host(oo, count * 16u + 16u);
        float run1[MAXN * 4];
        uint32_t k;

        for (k = 0; k < count; k++) {
            qrand_any(A + 4 * k);
            qrand_any(B + 4 * k);
            if (k % 3 == 1) {   /* a third of the elements at magnitudes from 1e-4 to 1e4 */
                scale4(A + 4 * k, pow(10.0, drand(-4.0, 4.0)));
                scale4(B + 4 * k, pow(10.0, drand(-4.0, 4.0)));
            } else if (k % 3 == 2) {   /* and a third at factors of about 1e-15: products near 1e-30, where the flush of denormals starts to matter */
                scale4(A + 4 * k, pow(10.0, drand(-15.0, -14.5)));
                scale4(B + 4 * k, pow(10.0, drand(-15.0, -14.5)));
            }
        }
        guard_fill(ga, count);
        guard_fill(gc, count);
        guard_fill(go, count);
        j->a = aa; j->b = ab; j->out_asm = oa; j->out_c = oc; j->out_orig = oo; j->count = count;
        sh4_xmtrx_get(S, x0);
        call("_t_mult", aj);
        sh4_xmtrx_get(S, x1);
        T_CHECK(!memcmp(x0, x1, 64), "mult trial %d: XMTRX changed", trial);
        T_CHECK(guard_ok(ga, count), "mult trial %d count %u: the asm loop wrote outside its output", trial, (unsigned)count);
        T_CHECK(guard_ok(gc, count), "mult trial %d count %u: the C form wrote outside its output", trial, (unsigned)count);
        T_CHECK(guard_ok(go, count), "mult trial %d count %u: the SH4ZAM loop wrote outside its output", trial, (unsigned)count);
        for (k = 0; k < count; k++) {
            check_mult_elem(A + 4 * k, B + 4 * k, OA + 4 * k, "asm", trial);
            check_mult_elem(A + 4 * k, B + 4 * k, OC + 4 * k, "C form", trial);
            {
                const double dv = max_abs_diff_f4(OA + 4 * k, OO + 4 * k);
                const double dc = max_abs_diff_f4(OA + 4 * k, OC + 4 * k);
                if (dv > mu_asm_orig) mu_asm_orig = dv;
                if (dc > mu_asm_c) mu_asm_c = dc;
                mu_elems++;
                if (memcmp(OA + 4 * k, OO + 4 * k, 16) == 0) mu_same_bits++;
            }
        }
        memcpy(run1, OA, count * 16u);
        call("_t_mult", aj);
        T_CHECK(!memcmp(run1, OA, count * 16u), "mult trial %d: a second run gave other bytes", trial);
        sh4_alloc_reset(S);
    }
    printf("mult: asm and C form against the exact product: %ld results, every component inside (P + |R|) x 2^-20, largest error / (P + |R|) = %.3g (2^-%.1f)\n",
           mu_checked, mu_worst_ratio, -log(mu_worst_ratio) / log(2.0));
    printf("mult: asm vs SH4ZAM loop largest %.3g (%.2f units of 2^-23); asm vs C form largest %.3g\n", mu_asm_orig, mu_asm_orig / ULP_ONE,
           mu_asm_c);
    printf("mult: asm bit for bit the same as SH4ZAM's loop in %ld of %ld elements (measured, not promised)\n", mu_same_bits, mu_elems);
}

/* ---- the normalise ---- */

static long no_checked;
static double no_worst, no_asm_orig, no_asm_c;

static void test_norm(void) {
    int trial;
    for (trial = 0; trial < 300; trial++) {
        const uint32_t count = (uint32_t)(trial % (MAXN + 1));   /* every count 0 to 40 in turn: every remainder class */
        const uint32_t aj = ram(sizeof(job_norm_t), 4, 0);
        const uint32_t off = (trial & 1) ? 0u : 4u;
        const uint32_t ai = ram(count * 16u + 16u, 8, off);
        const uint32_t oa = out_block(count, off), oc = out_block(count, off), oo = out_block(count, off);
        float x0[16], x1[16];
        job_norm_t *j = (job_norm_t *)host(aj, sizeof *j);
        float *I = (float *)host(ai, count * 16u + 16u);
        uint8_t *ga = (uint8_t *)host(oa - 16u, count * 16u + OUT_PAD), *gc = (uint8_t *)host(oc - 16u, count * 16u + OUT_PAD),
                *go = (uint8_t *)host(oo - 16u, count * 16u + OUT_PAD);
        float *OA = (float *)host(oa, count * 16u + 16u), *OC = (float *)host(oc, count * 16u + 16u),
              *OO = (float *)host(oo, count * 16u + 16u);
        float run1[MAXN * 4];
        uint32_t k;

        for (k = 0; k < count; k++) {
            qrand_any(I + 4 * k);
            if (k % 3 == 1)   /* a third of the elements at lengths from 1e-15 to 1e15 */
                scale4(I + 4 * k, pow(10.0, drand(-15.0, 15.0)));
        }
        guard_fill(ga, count);
        guard_fill(gc, count);
        guard_fill(go, count);
        j->in = ai; j->out_asm = oa; j->out_c = oc; j->out_orig = oo; j->count = count;
        sh4_xmtrx_get(S, x0);
        call("_t_norm", aj);
        sh4_xmtrx_get(S, x1);
        T_CHECK(!memcmp(x0, x1, 64), "normalize trial %d: XMTRX changed", trial);
        T_CHECK(guard_ok(ga, count), "normalize trial %d count %u: the asm loop wrote outside its output", trial, (unsigned)count);
        T_CHECK(guard_ok(gc, count), "normalize trial %d count %u: the C form wrote outside its output", trial, (unsigned)count);
        T_CHECK(guard_ok(go, count), "normalize trial %d count %u: the SH4ZAM loop wrote outside its output", trial, (unsigned)count);
        for (k = 0; k < count; k++) {
            const float *p = I + 4 * k;
            double len = 0.0, want[4];
            int c;
            for (c = 0; c < 4; c++) len += (double)p[c] * p[c];
            len = sqrt(len);
            for (c = 0; c < 4; c++) want[c] = (double)p[c] / len;
            {
                const double ea = max_abs_diff4(OA + 4 * k, want), ec = max_abs_diff4(OC + 4 * k, want);
                T_CHECK(ea <= NORM_BOUND, "normalize trial %d elem %u: asm error %.9g over the bound", trial, (unsigned)k, ea);
                T_CHECK(ec <= NORM_BOUND, "normalize trial %d elem %u: C form error %.9g over the bound", trial, (unsigned)k, ec);
                if (ea > no_worst) no_worst = ea;
                if (ec > no_worst) no_worst = ec;
                no_checked++;
            }
            {
                const double dv = max_abs_diff_f4(OA + 4 * k, OO + 4 * k);
                if (dv > no_asm_orig) no_asm_orig = dv;
                const double dc = max_abs_diff_f4(OA + 4 * k, OC + 4 * k);
                if (dc > no_asm_c) no_asm_c = dc;
            }
        }
        memcpy(run1, OA, count * 16u);
        call("_t_norm", aj);
        T_CHECK(!memcmp(run1, OA, count * 16u), "normalize trial %d: a second run gave other bytes", trial);
        sh4_alloc_reset(S);
    }
    printf("normalize: asm and C form against the exact unit quaternion: %ld results, largest error %.3g (bound %.3g)\n", no_checked, no_worst,
           NORM_BOUND);
    printf("normalize: asm vs SH4ZAM loop largest %.3g (%.2f units of 2^-23); asm vs C form largest %.3g\n", no_asm_orig, no_asm_orig / ULP_ONE,
           no_asm_c);
}

/* ---- length probe: the unit direction (1, 0.01, 0.01, 0.01) / |.|, at length L, normalised on the SH-4. A component of ratio
        r = c / L is lost (its square is a zero) when c is below 2^-63 (its square below 2^-126): the console runs with FPSCR.DN = 1
        (denormals are zeros, and the interpreter models that), so a squared length built from plain multiplies and adds is short by
        about r^2 for each lost component (the loop's FIPR is not: see the printed error).
        The probe prints the error at each length; only the two longest are checked (the header's range is 1e-15 and up). ---- */
static void probe_lengths(void) {
    static const double lengths[] = { 1e-14, 1e-15, 1e-16, 1e-17, 1e-18 };
    size_t li;

    for (li = 0; li < sizeof lengths / sizeof lengths[0]; li++) {
        const double L = lengths[li];
        const uint32_t aj = ram(sizeof(job_norm_t), 4, 0);
        const uint32_t ai = ram(16u + 16u, 8, 4);
        const uint32_t oa = out_block(1, 4), oc = out_block(1, 4), oo = out_block(1, 4);
        job_norm_t *j = (job_norm_t *)host(aj, sizeof *j);
        float *I = (float *)host(ai, 16 + 16);
        float *OA = (float *)host(oa, 16), *OC = (float *)host(oc, 16), *OO = (float *)host(oo, 16);
        const double dir[4] = { 1.0, 0.01, 0.01, 0.01 };
        double want[4], n = 0.0;
        int c;

        for (c = 0; c < 4; c++) n += dir[c] * dir[c];
        n = sqrt(n);
        for (c = 0; c < 4; c++) {
            want[c] = dir[c] / n;
            I[c] = (float)(want[c] * L);
        }
        j->in = ai; j->out_asm = oa; j->out_c = oc; j->out_orig = oo; j->count = 1;
        call("_t_norm", aj);
        {
            const double ea = max_abs_diff4(OA, want), ec = max_abs_diff4(OC, want), eo = max_abs_diff4(OO, want);

            printf("normalize length probe: L = %g: error asm %.3g, C form %.3g, SH4ZAM's own %.3g (bound %.3g)\n", L, ea, ec, eo, NORM_BOUND);
            if (li < 2) {
                T_CHECK(ea <= NORM_BOUND, "length probe L = %g: asm error %.9g over the bound", L, ea);
                T_CHECK(ec <= NORM_BOUND, "length probe L = %g: C form error %.9g over the bound", L, ec);
            }
        }
        sh4_alloc_reset(S);
    }
}

int main(int argc, char **argv) {
    const int rc = check_open(argc, argv);
    if (rc) return rc;
    test_nlerp();
    test_mult();
    test_norm();
    probe_lengths();
    return t_done("quat_check");
}
