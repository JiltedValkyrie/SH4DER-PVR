/* SH4ZAM-DCE tests: shz_dce_quat.h on the host (SH4ZAM's software back end)

   quat_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   What is checked:
   - the normalised lerp, the Hamilton product and the normalise against the exact result computed in double, inside the bound
     the header states (2^-19 for the lerp and the normalise, (P + |R|) x 2^-20 for the product)
   - the lerp's sign rule: the shorter way (b negated when a . b < 0); where |a . b| is inside the band of zero either way is
     accepted, and the count of results that took the other way is printed
   - the batch against the single element, for every count from 0 to 40, with the inputs on a multiple of 8 or on the next multiple
     of 4 (alternating), with guard words around the output: the batch must not write outside its output, and count 0 must write nothing
   - the layout the batch routines assume (16 bytes an element, w x y z in order); the lengths 1e-15 and 1e15 at both ends, with one
     small component on odd reps; the product bit for bit against shz_quat_mult under strict flags (not under -ffast-math)
   - against SH4ZAM's own routines on the same inputs (shz_quat_nlerp, shz_quat_mult, shz_quat_normalize): the largest difference
     and how many results are bit for bit the same. These are measured figures, printed, not promised.
*/

#include <sh4zam_dce/shz_dce_quat.h>

#include "shz_dce_test.h"

#define NLERP_BOUND 1.9073486328125e-06    /* 2^-19 */
#define MULT_FACTOR 9.5367431640625e-07    /* 2^-20 */
#define NORM_BOUND  1.9073486328125e-06    /* 2^-19 */
#define SIGN_BAND   9.5367431640625e-07    /* 2^-20: |a . b| inside this: either way of the shorter path is accepted */
#define GUARD_BYTE  0xa5
#define MAXN        40
#define BUF_FLOATS  (4 * MAXN + 8)

static double drand(double lo, double hi) {
    return lo + (hi - lo) * (double)(t_rand() >> 8) * (1.0 / 16777216.0);
}

/* a random unit quaternion, components rounded to float */
static shz_quat_t qrand_unit(void) {
    double c[4], n;
    int i;
    do {
        n = 0.0;
        for (i = 0; i < 4; i++) { c[i] = drand(-1.0, 1.0); n += c[i] * c[i]; }
    } while (n < 0.01);
    n = sqrt(n);
    return shz_quat_init((float)(c[0] / n), (float)(c[1] / n), (float)(c[2] / n), (float)(c[3] / n));
}

/* a random quaternion of any length between 0.5 and 3 */
static shz_quat_t qrand_any(void) {
    const shz_quat_t u = qrand_unit();
    const float s = (float)drand(0.5, 3.0);
    return shz_quat_init(u.w * s, u.x * s, u.y * s, u.z * s);
}

/* a unit quaternion (nearly) orthogonal to the unit quaternion a: the dot product is within float rounding of zero */
static shz_quat_t qrand_orth(const shz_quat_t *a) {
    const double ea[4] = { a->w, a->x, a->y, a->z };
    double v[4], d = 0.0, n = 0.0;
    int i;
    for (i = 0; i < 4; i++) v[i] = drand(-1.0, 1.0);
    for (i = 0; i < 4; i++) d += v[i] * ea[i];
    for (i = 0; i < 4; i++) v[i] -= d * ea[i];
    for (i = 0; i < 4; i++) n += v[i] * v[i];
    n = sqrt(n);
    return shz_quat_init((float)(v[0] / n), (float)(v[1] / n), (float)(v[2] / n), (float)(v[3] / n));
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

/* guard bytes are written and read as bytes (character types may alias anything): the float buffer is a plain array of bytes here */
static void fill_guard(float *buf) {
    memset(buf, GUARD_BYTE, sizeof(float) * BUF_FLOATS);
}

/* the floats of a buffer outside its output (output: floats 1 .. 4 * count) still hold the guard bytes */
static int guard_ok(const float *buf, size_t count) {
    const unsigned char *c = (const unsigned char *)buf;
    size_t i;
    for (i = 0; i < sizeof(float); i++)                                   /* the float before the output */
        if (c[i] != GUARD_BYTE)
            return 0;
    for (i = sizeof(float) * (1 + 4 * count); i < sizeof(float) * BUF_FLOATS; i++)   /* everything after it */
        if (c[i] != GUARD_BYTE)
            return 0;
    return 1;
}

/* nlerp counters */
static long nlerp_checked, nlerp_band, nlerp_took_neg;
static double worst_nlerp_err, worst_band_dot;

/* one nlerp result against the exact one, with the sign rule of the header */
static void check_nlerp_elem(const shz_quat_t *a, const shz_quat_t *b, float t, const shz_quat_t *o, const char *what, int trial) {
    double pos[4], neg[4], d, e_pos, e_neg, e_right;
    const double tt = (double)t;

    exact_nlerp(a->e, b->e, tt, 1.0, pos);
    exact_nlerp(a->e, b->e, tt, -1.0, neg);
    d = dot_dir(a->e, b->e);
    e_pos = max_abs_diff4(o->e, pos);
    e_neg = max_abs_diff4(o->e, neg);
    nlerp_checked++;

    if (d > SIGN_BAND) {
        e_right = e_pos;                          /* the shorter way is +b */
    } else if (d < -SIGN_BAND) {
        e_right = e_neg;                          /* the shorter way is -b */
    } else {                                      /* inside the band: either way is a shorter way */
        e_right = e_pos < e_neg ? e_pos : e_neg;
        nlerp_band++;
        if (fabs(d) > worst_band_dot) worst_band_dot = fabs(d);
        if (e_neg <= NLERP_BOUND && e_pos > NLERP_BOUND) nlerp_took_neg++;
    }
    if (e_right > worst_nlerp_err) worst_nlerp_err = e_right;
    T_CHECK(e_right <= NLERP_BOUND,
            "%s trial %d: nlerp error %.9g over the bound %.9g (dot %.9g, t %.9g)", what, trial, e_right, NLERP_BOUND, d, tt);
}

static void test_nlerp(void) {
    static float ia[BUF_FLOATS], ib[BUF_FLOATS], ob[BUF_FLOATS];
    size_t count;
    int trial = 0;
    double max_vs_orig = 0.0;
    long same_bits = 0, orig_n = 0;

    for (count = 0; count <= MAXN; count++) {
        const size_t reps = count == 0 ? 4 : 40;
        size_t rep;
        for (rep = 0; rep < reps; rep++, trial++) {
            /* A and B on a multiple of 8 or on a multiple of 4 that is not one of 8 (both allowed); O on the second kind */
            const size_t off = rep & 1u;
            shz_quat_t *A = (shz_quat_t *)(ia + off), *B = (shz_quat_t *)(ib + off), *O = (shz_quat_t *)(ob + 1);
            const float t = (rep % 5 == 0) ? (float)(rep % 2) : (float)drand(0.0, 1.0);   /* 0 and 1 included */
            size_t k;

            fill_guard(ob);
            for (k = 0; k < count; k++) {
                A[k] = qrand_unit();
                switch (k % 8) {
                case 0:  B[k] = A[k]; break;                                                        /* the same quaternion */
                case 1:  B[k] = shz_quat_init(-A[k].w, -A[k].x, -A[k].y, -A[k].z); break;           /* the other way round */
                case 2:  B[k] = qrand_orth(&A[k]); break;                                           /* dot product zero: either way */
                default: B[k] = qrand_unit(); break;
                }
                if (k % 3 == 1) {   /* a third of the elements at a length between 1e-15 and 1e15 (the same length for both) */
                    const double sc = pow(10.0, drand(-15.0, 15.0));
                    scale4(A[k].e, sc);
                    scale4(B[k].e, sc);
                }
            }
            shz_dce_quat_nlerp_batch(O, A, B, t, count);
            T_CHECK(guard_ok(ob, count), "nlerp count %u: the batch wrote outside its output", (unsigned)count);
            for (k = 0; k < count; k++) {
                shz_quat_t single;
                shz_quat_t ref;
                double dif, d;

                check_nlerp_elem(&A[k], &B[k], t, &O[k], "batch", trial);
                d = dot_dir(A[k].e, B[k].e);
                /* the same element on its own: the same arithmetic, but with -ffast-math the compiler may contract the
                   multiply-adds differently in the two inlined copies: a few units in the last place is the allowance.
                   Inside the sign band the two copies may take different (both right) ways: not compared there. */
                shz_dce_quat_nlerp_batch(&single, &A[k], &B[k], t, 1);
                if (d > SIGN_BAND || d < -SIGN_BAND) {
                    dif = max_abs_diff4(O[k].e, (double[]){ single.w, single.x, single.y, single.z });
                    T_CHECK(dif <= 4.0 * 1.1920928955078125e-07, "nlerp count %u elem %u: the batch differs from the single element by %.3g",
                            (unsigned)count, (unsigned)k, dif);
                }
                /* SH4ZAM's own routine on the same inputs: measured, not promised (the sign is not in the band) */
                if (d > SIGN_BAND || d < -SIGN_BAND) {
                    ref = shz_quat_nlerp(A[k], B[k], t);
                    dif = max_abs_diff4(O[k].e, (double[]){ ref.w, ref.x, ref.y, ref.z });
                    orig_n++;
                    if (memcmp(&ref, &O[k], 16) == 0) same_bits++;
                    if (dif > max_vs_orig) max_vs_orig = dif;
                }
            }
        }
    }
    printf("nlerp: %ld results against the exact nlerp, largest error %.3g (bound %.3g); %ld inside the sign band (|a . b| <= %.3g, largest %.3g), %ld of them took the other way\n",
           nlerp_checked, worst_nlerp_err, NLERP_BOUND, nlerp_band, SIGN_BAND, worst_band_dot, nlerp_took_neg);
    printf("nlerp vs shz_quat_nlerp: %ld results, %ld bit for bit the same, largest difference %.3g (%.2f units of 2^-23)\n",
           orig_n, same_bits, max_vs_orig, max_vs_orig / 1.1920928955078125e-07);
}

static void test_mult(void) {
    static float ia[BUF_FLOATS], ib[BUF_FLOATS], ob[BUF_FLOATS];
    size_t count;
    long same_bits = 0, n = 0;
    double max_vs_orig = 0.0, worst_ratio = 0.0;

    for (count = 0; count <= MAXN; count++) {
        const size_t reps = count == 0 ? 4 : 40;
        size_t rep;
        for (rep = 0; rep < reps; rep++) {
            const size_t off = rep & 1u;
            shz_quat_t *A = (shz_quat_t *)(ia + off), *B = (shz_quat_t *)(ib + off), *O = (shz_quat_t *)(ob + 1);
            size_t k;

            fill_guard(ob);
            for (k = 0; k < count; k++) {
                A[k] = qrand_any();
                B[k] = qrand_any();
                if (k % 3 == 1) {   /* a third of the elements at magnitudes from 1e-4 to 1e4 */
                    scale4(A[k].e, pow(10.0, drand(-4.0, 4.0)));
                    scale4(B[k].e, pow(10.0, drand(-4.0, 4.0)));
                }
            }
            shz_dce_quat_mult_batch(O, A, B, count);
            T_CHECK(guard_ok(ob, count), "mult count %u: the batch wrote outside its output", (unsigned)count);
            for (k = 0; k < count; k++) {
                const float *p = A[k].e, *q = B[k].e;
                /* the exact products in double: the four products that make each component (P is the largest of them) */
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
                    const double err = fabs((double)O[k].e[c] - r[c]);
                    for (j = 0; j < 4; j++) if (fabs(m[c][j]) > P) P = fabs(m[c][j]);
                    T_CHECK(err <= (P + fabs(r[c])) * MULT_FACTOR,
                            "mult count %u elem %u comp %d: error %.9g over (P + |R|) x 2^-20 = %.9g", (unsigned)count, (unsigned)k, c,
                            err, (P + fabs(r[c])) * MULT_FACTOR);
                    if (P + fabs(r[c]) > 0.0 && err / (P + fabs(r[c])) > worst_ratio) worst_ratio = err / (P + fabs(r[c]));
                }
                {   /* SH4ZAM's own product on the same inputs */
                    const shz_quat_t ref = shz_quat_mult(A[k], B[k]);
#if !defined(__FAST_MATH__)   /* the header's claim: the same bits as SH4ZAM's product under strict IEEE flags */
                    T_CHECK(memcmp(&ref, &O[k], 16) == 0, "mult count %u elem %u: not bit for bit the same as shz_quat_mult",
                            (unsigned)count, (unsigned)k);
#endif
                    const double dif = max_abs_diff4(O[k].e, (double[]){ ref.w, ref.x, ref.y, ref.z });
                    n++;
                    if (memcmp(&ref, &O[k], 16) == 0) same_bits++;
                    if (dif > max_vs_orig) max_vs_orig = dif;
                }
            }
        }
    }
    printf("mult: every component inside (P + |R|) x 2^-20; largest error / (P + |R|) = %.3g\n", worst_ratio);
    printf("mult vs shz_quat_mult: %ld results, %ld bit for bit the same, largest difference %.3g\n", n, same_bits, max_vs_orig);
}

static void test_normalize(void) {
    static float ia[BUF_FLOATS], ob[BUF_FLOATS];
    size_t count;
    long same_bits = 0, n = 0;
    double worst = 0.0, max_vs_orig = 0.0;

    for (count = 0; count <= MAXN; count++) {
        const size_t reps = count == 0 ? 4 : 40;
        size_t rep;
        for (rep = 0; rep < reps; rep++) {
            const size_t off = rep & 1u;
            shz_quat_t *A = (shz_quat_t *)(ia + off), *O = (shz_quat_t *)(ob + 1);
            size_t k;

            fill_guard(ob);
            for (k = 0; k < count; k++) {
                A[k] = qrand_any();
                if (k % 3 == 1)   /* a third of the elements at lengths from 1e-15 to 1e15 */
                    scale4(A[k].e, pow(10.0, drand(-15.0, 15.0)));
            }
            shz_dce_quat_normalize_batch(O, A, count);
            T_CHECK(guard_ok(ob, count), "normalize count %u: the batch wrote outside its output", (unsigned)count);
            for (k = 0; k < count; k++) {
                const float *p = A[k].e;
                double len = 0.0, want[4];
                int c;
                for (c = 0; c < 4; c++) len += (double)p[c] * p[c];
                len = sqrt(len);
                for (c = 0; c < 4; c++) want[c] = (double)p[c] / len;
                {
                    const double err = max_abs_diff4(O[k].e, want);
                    T_CHECK(err <= NORM_BOUND, "normalize count %u elem %u: error %.9g over the bound", (unsigned)count, (unsigned)k, err);
                    if (err > worst) worst = err;
                }
                {   /* SH4ZAM's own normalise on the same input */
                    const shz_quat_t ref = shz_quat_normalize(A[k]);
                    const double dif = max_abs_diff4(O[k].e, (double[]){ ref.w, ref.x, ref.y, ref.z });
                    n++;
                    if (memcmp(&ref, &O[k], 16) == 0) same_bits++;
                    if (dif > max_vs_orig) max_vs_orig = dif;
                }
            }
        }
    }
    printf("normalize: largest error against the exact unit quaternion %.3g (bound %.3g)\n", worst, NORM_BOUND);
    printf("normalize vs shz_quat_normalize: %ld results, %ld bit for bit the same, largest difference %.3g\n", n, same_bits, max_vs_orig);
}

static void test_edges(void) {
    /* identity and the trivial cases, exactly or inside the bound */
    const shz_quat_t id = shz_quat_init(1.0f, 0.0f, 0.0f, 0.0f);
    const shz_quat_t q = shz_quat_init(0.5f, -0.5f, 0.5f, -0.5f);   /* a unit quaternion */
    shz_quat_t o[2];
    int i;

    shz_dce_quat_mult_batch(o, &id, &q, 1);
    for (i = 0; i < 4; i++) T_CHECK(o[0].e[i] == q.e[i], "identity * q: component %d is %g, not %g", i, (double)o[0].e[i], (double)q.e[i]);
    shz_dce_quat_mult_batch(o, &q, &id, 1);
    for (i = 0; i < 4; i++) T_CHECK(o[0].e[i] == q.e[i], "q * identity: component %d is %g, not %g", i, (double)o[0].e[i], (double)q.e[i]);

    /* nlerp at t = 0 gives a, at t = 1 gives b (normalised), a and b being the same quaternion here */
    shz_dce_quat_nlerp_batch(o, &q, &id, 0.0f, 1);
    for (i = 0; i < 4; i++) T_CHECK(fabs((double)o[0].e[i] - (double)q.e[i]) <= NLERP_BOUND, "nlerp t=0 component %d", i);
    shz_dce_quat_nlerp_batch(o, &q, &q, 1.0f, 1);
    for (i = 0; i < 4; i++) T_CHECK(fabs((double)o[0].e[i] - (double)q.e[i]) <= NLERP_BOUND, "nlerp t=1 component %d", i);

    /* the two sides of a quaternion and its negation: a shorter path of zero length */
    {
        const shz_quat_t nq = shz_quat_init(-0.5f, 0.5f, -0.5f, 0.5f);
        shz_dce_quat_nlerp_batch(o, &q, &nq, 0.25f, 1);
        for (i = 0; i < 4; i++) T_CHECK(fabs((double)o[0].e[i] - (double)q.e[i]) <= NLERP_BOUND, "nlerp of q and -q, component %d", i);
    }

    /* normalise of a unit quaternion is itself */
    shz_dce_quat_normalize_batch(o, &q, 1);
    for (i = 0; i < 4; i++) T_CHECK(fabs((double)o[0].e[i] - (double)q.e[i]) <= NORM_BOUND, "normalize unit component %d", i);
}

/* the header's input range (lengths 1e-15 and 1e15) at both ends, with a small component (1e-3 of the rest) on odd reps:
   the normalise and the nlerp against the exact values, inside the bound */
static void test_range_ends(void) {
    static const double lens[2] = { 1e-15, 1e15 };
    int li, rep;

    for (li = 0; li < 2; li++) {
        for (rep = 0; rep < 200; rep++) {
            const double L = lens[li];
            double d[4], e[4], nd = 0.0, ne = 0.0, want[4], len = 0.0;
            shz_quat_t a[1], b[1], on[1], ob[1];
            int i;

            for (i = 0; i < 4; i++) { d[i] = drand(-1.0, 1.0); e[i] = drand(-1.0, 1.0); }
            if (rep & 1) d[1] *= 1e-3;
            for (i = 0; i < 4; i++) { nd += d[i] * d[i]; ne += e[i] * e[i]; }
            nd = sqrt(nd);
            ne = sqrt(ne);
            for (i = 0; i < 4; i++) {
                a[0].e[i] = (float)(L * d[i] / nd);
                b[0].e[i] = (float)(L * e[i] / ne);
            }
            /* the exact unit quaternion of the float input a, in double */
            for (i = 0; i < 4; i++) len += (double)a[0].e[i] * a[0].e[i];
            len = sqrt(len);
            for (i = 0; i < 4; i++) want[i] = (double)a[0].e[i] / len;
            shz_dce_quat_normalize_batch(on, a, 1);
            T_CHECK(max_abs_diff4(on[0].e, want) <= NORM_BOUND, "normalize at length %g (rep %d): error %.9g over the bound", L, rep,
                    max_abs_diff4(on[0].e, want));
            shz_dce_quat_nlerp_batch(ob, a, b, 0.3f, 1);
            check_nlerp_elem(&a[0], &b[0], 0.3f, &ob[0], "nlerp at the range end", rep);
        }
    }
}

/* the layout the batch routines assume: 16 bytes an element, the floats w, x, y, z in order */
static void test_layout(void) {
    const shz_quat_t q = shz_quat_init(1.0f, 2.0f, 3.0f, 4.0f);

    T_CHECK(sizeof(shz_quat_t) == 16u, "shz_quat_t is %u bytes: the batch routines step 16 bytes an element", (unsigned)sizeof(shz_quat_t));
    T_CHECK(q.e[0] == 1.0f && q.e[1] == 2.0f && q.e[2] == 3.0f && q.e[3] == 4.0f, "e[] is not w, x, y, z");
}

int main(void) {
    test_layout();
    test_nlerp();
    test_mult();
    test_normalize();
    test_edges();
    test_range_ends();
    return t_done("quat_test");
}
