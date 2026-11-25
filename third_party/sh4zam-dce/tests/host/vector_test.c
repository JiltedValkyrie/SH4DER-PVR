/* SH4ZAM-DCE tests: shz_dce_vector.h on the host (SH4ZAM's software back end)

   vector_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   What is checked:
   - the batch dot product (the host path is the plain C form) against the exact inner product (in double), inside the bound the header
     states: (P + |R|) x 2^-21; the count 0..37 covers every odd and even count; the guard float after the last result is untouched; the
     scales 1, 1e-15 and 1e15 (products inside the normal float range)
   - dot4: each lane against the exact value, inside the same bound; and bit for bit against four shz_vec3_dot() calls, the signed zeros
     included (strict-flag build: -ffast-math may change a zero's sign)
   - the cross batch against shz_vec3_cross(): bit for bit, for every count, and in place (out = a)
   - the normalising batch against the exact unit vector within 2^-19 relative, its distance from shz_vec3_normalize() in ulps (printed),
     and in place (out = in)
   - magnitude and the vec4 normalisation against the exact values within 2^-19 relative (a zero vector: magnitude 0), and their distance
     from SH4ZAM's routines in ulps (printed; on the host both are the same formulas, so 0)
   - the special inputs (a zero vector, an infinity, a NaN, a squared length that overflows): the same NaN and zero results as SH4ZAM's
     routines, and no effect on the other elements of a batch (strict-flag build)
*/

/* the FIPR bodies of the length and of the 4-vector normalisation are tested here: by default they are SH4ZAM's routines, called underneath */
/* SHZ_DCE_REWORK_FIPR is gone (shz_dce_cdefs.h): the FIPR names are SH4ZAM's routines, called underneath */
#include <sh4zam_dce/shz_dce_vector.h>

#include "shz_dce_test.h"

#define BOUND 4.76837158203125e-07   /* 2^-21 */
#define REL_NORM 1.9073486328125e-06 /* 2^-19 */

static SHZ_ALIGNAS(8) shz_vec3_t va[64], vb[64], vout[64], vref[64];
static SHZ_ALIGNAS(8) float fout[70];

static void random_vec(shz_vec3_t *v, float lo, float hi) {
    v->x = t_frand(lo, hi);
    v->y = t_frand(lo, hi);
    v->z = t_frand(lo, hi);
}

/* the exact inner product in double, and P: the largest of the three products */
static double exact_dot(const shz_vec3_t *a, const shz_vec3_t *b, double *pmax) {
    const double t[3] = { (double)a->x * b->x, (double)a->y * b->y, (double)a->z * b->z };
    int k;
    *pmax = 0.0;
    for (k = 0; k < 3; k++) if (fabs(t[k]) > *pmax) *pmax = fabs(t[k]);
    return t[0] + t[1] + t[2];
}

static int same_bits3(const shz_vec3_t *p, const shz_vec3_t *q) {
    return t_bits(p->x) == t_bits(q->x) && t_bits(p->y) == t_bits(q->y) && t_bits(p->z) == t_bits(q->z);
}

/* the unit vector of each input (in double), against the output: within 2^-19 relative for the components above 1e-3 */
static void check_unit(const shz_vec3_t *in, const shz_vec3_t *out, int count, const char *what, double *worst) {
    int i;
    for (i = 0; i < count; i++) {
        const double n2 = (double)in[i].x * in[i].x + (double)in[i].y * in[i].y + (double)in[i].z * in[i].z;
        const double inv = 1.0 / sqrt(n2);
        const double ex[3] = { in[i].x * inv, in[i].y * inv, in[i].z * inv };
        const float got[3] = { out[i].x, out[i].y, out[i].z };
        int k;
        for (k = 0; k < 3; k++) {
            if (fabs(ex[k]) > 1e-3) {
                const double rel = fabs((double)got[k] - ex[k]) / fabs(ex[k]);
                T_CHECK(rel <= REL_NORM, "%s: element %d component %d: %.9g, exact %.9g, relative %.3g", what, i, k, (double)got[k], ex[k], rel);
                if (worst && rel > *worst) *worst = rel;
            }
        }
    }
}

/* three scales: the normal range, and tiny and huge values (every product stays inside the normal float range) */
static void host_scales(void) {
    static const float scales[3] = { 1.0f, 1e-15f, 1e15f };
    int s, count;
    for (s = 0; s < 3; s++) {
        for (count = 0; count <= 37; count++) {
            const float sc = scales[s];
            int i;
            for (i = 0; i < 64; i++) {
                random_vec(&va[i], -6.0f * sc, 6.0f * sc);
                random_vec(&vb[i], -6.0f * sc, 6.0f * sc);
            }
            shz_dce_vec3_dot_batch(fout, va, vb, (size_t)count);
            for (i = 0; i < count; i++) {
                double pm;
                const double e = exact_dot(&va[i], &vb[i], &pm), bound = (pm + fabs(e)) * BOUND;
                T_CHECK(fabs((double)fout[i] - e) <= bound, "dot batch at scale %g, count %d element %d: %.9g, exact %.9g", (double)sc, count, i, (double)fout[i], e);
            }
            for (i = 0; i < 64; i++) random_vec(&va[i], -50.0f * sc, 50.0f * sc);
            shz_dce_vec3_normalize_batch(vout, va, (size_t)count);
            check_unit(va, vout, count, "normalize batch at a scale", NULL);
        }
    }
}

/* out == in: the batch gives the unit vectors (the loads of an element come before its stores) */
static void host_inplace_normalize(void) {
    int count;
    double worst = 0.0;
    for (count = 0; count <= 37; count++) {
        int i;
        for (i = 0; i < 64; i++) random_vec(&va[i], -50.0f, 50.0f);
        for (i = 0; i < 64; i++) vout[i] = va[i];
        shz_dce_vec3_normalize_batch(vout, vout, (size_t)count);
        check_unit(va, vout, count, "normalize batch in place", &worst);
    }
    printf("normalize batch in place: worst relative error against the exact unit vector %.3g\n", worst);
}

#ifndef __FAST_MATH__
/* the special inputs: a zero vector, an infinity, a NaN, and a squared length that overflows; kind 5 is a normal vector */
static void special_vec(shz_vec3_t *v, int kind) {
    random_vec(v, -50.0f, 50.0f);
    switch (kind) {
    case 0: v->x = 0.0f; v->y = 0.0f; v->z = 0.0f; break;
    case 1: v->x = t_float(0x7f800000u); break;
    case 2: v->y = t_float(0xff800000u); break;
    case 3: v->z = t_float(0x7fc00000u); break;
    case 4: v->x = 1e20f; v->y = 0.0f; v->z = 0.0f; break;
    default: break;
    }
}

static void special_vec4(shz_vec4_t *v, int kind) {
    int k;
    for (k = 0; k < 4; k++) v->e[k] = t_frand(-50.0f, 50.0f);
    switch (kind) {
    case 0: for (k = 0; k < 4; k++) v->e[k] = 0.0f; break;
    case 1: v->x = t_float(0x7f800000u); break;
    case 2: v->y = t_float(0xff800000u); break;
    case 3: v->w = t_float(0x7fc00000u); break;
    case 4: v->x = 1e20f; v->y = 0.0f; v->z = 0.0f; v->w = 0.0f; break;
    default: break;
    }
}

/* the classes of a result, read from its bits; two results agree: the same bits, or both NaN, or both infinite with the same sign */
static int is_nan_f(float f) { const uint32_t b = t_bits(f); return (b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) != 0u; }
static int is_inf_f(float f) { return (t_bits(f) & 0x7fffffffu) == 0x7f800000u; }
static int is_zero_bits_f(float f) { return (t_bits(f) & 0x7fffffffu) == 0u; }
static int agree_f(float got, float ref) {
    if (is_nan_f(ref)) return is_nan_f(got);
    if (is_inf_f(ref)) return is_inf_f(got) && (t_bits(got) >> 31) == (t_bits(ref) >> 31);
    return t_bits(got) == t_bits(ref);
}

/* a special value in one element of a dot batch: the others must be the plain dot products (no element reaches another) */
static void host_specials(void) {
    static const uint32_t planted[3] = { 0x7fc00000u, 0x7f800000u, 0xff800000u };   /* a quiet NaN, +infinity, -infinity */
    int trial, dot_cases = 0, dot_mismatch = 0, neighbour_hit = 0, vec_cases = 0, vec_mismatch = 0;
    for (trial = 0; trial < 300; trial++) {
        const int count = (int)(t_rand() % 38u);
        int i;
        for (i = 0; i < count; i++) {
            random_vec(&va[i], -6.0f, 6.0f);
            random_vec(&vb[i], -6.0f, 6.0f);
        }
        if (count) {
            const int p = (int)(t_rand() % (uint32_t)count);
            shz_vec3_t *v = (t_rand() & 1u) ? &va[p] : &vb[p];
            v->e[t_rand() % 3u] = t_float(planted[t_rand() % 3u]);
            shz_dce_vec3_dot_batch(fout, va, vb, (size_t)count);
            for (i = 0; i < count; i++) {
                const float ref = shz_vec3_dot(va[i], vb[i]);
                dot_cases++;
                if (!agree_f(fout[i], ref)) dot_mismatch++;
                if (i != p && (is_nan_f(fout[i]) || is_inf_f(fout[i]))) neighbour_hit++;
            }
        }
    }
    T_CHECK(dot_mismatch == 0, "dot batch with a special value: %d of %d results differ from shz_vec3_dot()", dot_mismatch, dot_cases);
    T_CHECK(neighbour_hit == 0, "dot batch with a special value: %d elements without one came out NaN or infinite", neighbour_hit);
    printf("dot batch with a special value: %d of %d results differ from shz_vec3_dot(); %d neighbours hit\n", dot_mismatch, dot_cases, neighbour_hit);

    /* the normalising batch: the special vectors give SH4ZAM's NaN and zero pattern, the normal ones the unit vector */
    for (trial = 0; trial < 200; trial++) {
        const int count = 1 + (int)(t_rand() % 20u);
        int kind[20];
        int i;
        for (i = 0; i < count; i++) {
            kind[i] = (int)(t_rand() % 6u);
            special_vec(&va[i], kind[i]);
        }
        shz_dce_vec3_normalize_batch(vout, va, (size_t)count);
        for (i = 0; i < count; i++) {
            const shz_vec3_t ref = shz_vec3_normalize(va[i]);
            const float got[3] = { vout[i].x, vout[i].y, vout[i].z };
            const float want[3] = { ref.x, ref.y, ref.z };
            int c;
            vec_cases++;
            if (kind[i] == 5) {
                check_unit(&va[i], &vout[i], 1, "normalize batch: normal element next to special ones", NULL);
                continue;
            }
            for (c = 0; c < 3; c++) {
                if (is_nan_f(want[c]) ? !is_nan_f(got[c]) : (is_zero_bits_f(want[c]) ? !is_zero_bits_f(got[c]) : !agree_f(got[c], want[c]))) vec_mismatch++;
            }
        }
    }
    T_CHECK(vec_mismatch == 0, "normalize batch special inputs: %d components differ from shz_vec3_normalize() in NaN or zero pattern", vec_mismatch);
    printf("normalize batch special inputs: %d vectors checked against shz_vec3_normalize()\n", vec_cases);

    /* the length and the vec4 normalisation on the same classes: SH4ZAM's NaN and zero */
    {
        int m_cases = 0, m_mismatch = 0, v4_mismatch = 0;
        for (trial = 0; trial < 300; trial++) {
            shz_vec3_t v;
            shz_vec4_t v4, n4, o4;
            const int kind = (int)(t_rand() % 6u);
            int k;
            special_vec(&v, kind);
            m_cases++;
            if (kind != 5 && !agree_f(shz_dce_vec3_magnitude_fipr(v), shz_vec3_magnitude(v))) m_mismatch++;
            special_vec4(&v4, kind);
            n4 = shz_dce_vec4_normalize(v4);
            o4 = shz_vec4_normalize(v4);
            if (kind != 5) {
                for (k = 0; k < 4; k++) {
                    if (!agree_f(n4.e[k], o4.e[k]) && !(is_zero_bits_f(o4.e[k]) && is_zero_bits_f(n4.e[k]))) v4_mismatch++;
                }
            }
        }
        T_CHECK(m_mismatch == 0, "magnitude special inputs: %d of %d differ from shz_vec3_magnitude()", m_mismatch, m_cases);
        T_CHECK(v4_mismatch == 0, "vec4 normalize special inputs: %d components differ from shz_vec4_normalize()", v4_mismatch);
        printf("magnitude and vec4 special inputs: %d vectors checked against SH4ZAM's routines\n", m_cases);
    }
}

/* exact zero results: every product -0 (or a zero sum from -0 terms): the same bits as shz_vec3_dot() on the host, sign of zero included */
static void host_dot4_zero(void) {
    int trial, differs = 0, lanes = 0;
    for (trial = 0; trial < 200; trial++) {
        shz_vec3_t a[4], b[4];
        shz_vec4_t r;
        int k;
        for (k = 0; k < 4; k++) {
            switch (t_rand() % 4u) {
            case 0: a[k] = shz_vec3_init(-1.0f, -1.0f, -1.0f); b[k] = shz_vec3_init(0.0f, 0.0f, 0.0f); break;
            case 1: a[k] = shz_vec3_init(0.0f, 0.0f, 0.0f); b[k] = shz_vec3_init(-1.0f, -1.0f, -1.0f); break;
            case 2: a[k] = shz_vec3_init(-0.0f, -0.0f, -0.0f); b[k] = shz_vec3_init(1.0f, 1.0f, 1.0f); break;
            default: random_vec(&a[k], -8.0f, 8.0f); random_vec(&b[k], -8.0f, 8.0f); break;
            }
        }
        r = shz_dce_vec3_dot4(a, b);
        {
            const float got[4] = { r.x, r.y, r.z, r.w };
            for (k = 0; k < 4; k++) {
                lanes++;
                if (t_bits(got[k]) != t_bits(shz_vec3_dot(a[k], b[k]))) differs++;
            }
        }
    }
    T_CHECK(differs == 0, "dot4 on signed zeros: %d of %d lanes differ in bits from shz_vec3_dot()", differs, lanes);
    printf("dot4 on signed zeros: %d of %d lanes differ in bits from shz_vec3_dot()\n", differs, lanes);
}
#endif

int main(void) {
    static const float sentinel = 1234.5f;
    const shz_vec3_t sent3 = shz_vec3_init(sentinel, sentinel, sentinel);
    uint32_t max_ulps_dot4 = 0u, max_ulps_norm = 0u, max_ulps_mag = 0u, max_ulps_v4 = 0u;
    double worst_norm_rel = 0.0, worst_mag_rel = 0.0, worst_v4_rel = 0.0;
    int count, trial, cdiff = 0;

    /* -- the batch dot product against the exact value -- */
    for (count = 0; count <= 37; count++) {
        int i;
        for (i = 0; i < 64; i++) {
            random_vec(&va[i], -6.0f, 6.0f);
            random_vec(&vb[i], -6.0f, 6.0f);
        }
        for (i = 0; i < 70; i++) fout[i] = sentinel;
        shz_dce_vec3_dot_batch(fout, va, vb, (size_t)count);
        T_CHECK(fout[count] == sentinel, "dot batch count %d wrote past its last result", count);
        for (i = 0; i < count; i++) {
            double pm;
            const double e = exact_dot(&va[i], &vb[i], &pm), bound = (pm + fabs(e)) * BOUND;
            const double from_shz = fabs((double)fout[i] - (double)shz_vec3_dot(va[i], vb[i]));
            T_CHECK(fabs((double)fout[i] - e) <= bound, "dot batch count %d element %d: %.9g, exact %.9g, bound %.3g", count, i, (double)fout[i], e, bound);
            /* the batch agrees with shz_vec3_dot() within (P + |R|) x 2^-20 (each of the two is within (P + |R|) x 2^-21 of the exact value) */
            T_CHECK(from_shz <= 2.0 * bound, "dot batch count %d element %d: %.3g from shz_vec3_dot(), over %.3g", count, i, from_shz, 2.0 * bound);
        }
        /* the plain C form is the same expression as shz_vec3_dot(): counted here, and required to match under strict flags */
        shz_dce_vec3_dot_batch_c(fout, va, vb, (size_t)count);
        for (i = 0; i < count; i++)
            if (t_bits(fout[i]) != t_bits(shz_vec3_dot(va[i], vb[i]))) cdiff++;
    }

    /* -- dot4: the four lanes against the exact value, and bit for bit against four shz_vec3_dot() calls -- */
    for (trial = 0; trial < 2000; trial++) {
        shz_vec3_t a[4], b[4];
        shz_vec4_t r;
        int k;
        for (k = 0; k < 4; k++) { random_vec(&a[k], -8.0f, 8.0f); random_vec(&b[k], -8.0f, 8.0f); }
        r = shz_dce_vec3_dot4(a, b);
        for (k = 0; k < 4; k++) {
            double pm;
            const double e = exact_dot(&a[k], &b[k], &pm), bound = (pm + fabs(e)) * BOUND;
            const float got = k == 0 ? r.x : k == 1 ? r.y : k == 2 ? r.z : r.w;
            const uint32_t u = t_ulps(got, shz_vec3_dot(a[k], b[k]));
            T_CHECK(fabs((double)got - e) <= bound, "dot4 lane %d: %.9g, exact %.9g, bound %.3g", k, (double)got, e, bound);
            if (u > max_ulps_dot4) max_ulps_dot4 = u;
        }
    }
    T_CHECK(max_ulps_dot4 == 0u, "dot4 differs from four shz_vec3_dot() calls by %u ulp", (unsigned)max_ulps_dot4);
    printf("dot4: largest distance from four shz_vec3_dot() calls: %u ulp\n", (unsigned)max_ulps_dot4);

    /* -- the cross batch: bit for bit against shz_vec3_cross(), every count, and in place -- */
    for (count = 0; count <= 37; count++) {
        int i;
        for (i = 0; i < 64; i++) {
            random_vec(&va[i], -4.0f, 4.0f);
            random_vec(&vb[i], -4.0f, 4.0f);
        }
        for (i = 0; i < count; i++) vref[i] = shz_vec3_cross(va[i], vb[i]);
        for (i = 0; i < 64; i++) vout[i] = sent3;
        shz_dce_vec3_cross_batch(vout, va, vb, (size_t)count);
        for (i = 0; i < count; i++)
            T_CHECK(same_bits3(&vout[i], &vref[i]), "cross batch count %d element %d differs from shz_vec3_cross()", count, i);
        T_CHECK(same_bits3(&vout[count], &sent3), "cross batch count %d wrote past its last element", count);
        for (i = 0; i < count; i++) vout[i] = va[i];   /* in place: out = a */
        shz_dce_vec3_cross_batch(vout, vout, vb, (size_t)count);
        for (i = 0; i < count; i++)
            T_CHECK(same_bits3(&vout[i], &vref[i]), "cross batch in place, count %d element %d differs", count, i);
    }

    /* -- the normalising batch: the exact unit vector within 2^-19 relative; and the distance from shz_vec3_normalize() in ulps -- */
    for (count = 0; count <= 37; count++) {
        int i;
        for (i = 0; i < 64; i++) random_vec(&va[i], -50.0f, 50.0f);
        for (i = 0; i < count; i++) vref[i] = shz_vec3_normalize(va[i]);
        for (i = 0; i < 64; i++) vout[i] = sent3;
        shz_dce_vec3_normalize_batch(vout, va, (size_t)count);
        T_CHECK(same_bits3(&vout[count], &sent3), "normalize batch count %d wrote past its last element", count);
        for (i = 0; i < count; i++) {
            const double n2 = (double)va[i].x * va[i].x + (double)va[i].y * va[i].y + (double)va[i].z * va[i].z;
            const double inv = 1.0 / sqrt(n2);
            const double ex[3] = { va[i].x * inv, va[i].y * inv, va[i].z * inv };
            const float got[3] = { vout[i].x, vout[i].y, vout[i].z };
            int k;
            for (k = 0; k < 3; k++) {
                const uint32_t u = t_ulps(got[k], vref[i].e[k]);
                if (u > max_ulps_norm) max_ulps_norm = u;
                /* the batch agrees with shz_vec3_normalize() within 2^-19 a component (a unit vector's components are at most 1) */
                T_CHECK(fabs((double)got[k] - (double)vref[i].e[k]) <= REL_NORM, "normalize element %d component %d: %.3g from shz_vec3_normalize(), over 2^-19", i, k,
                        fabs((double)got[k] - (double)vref[i].e[k]));
                if (fabs(ex[k]) > 1e-3) {
                    const double rel = fabs((double)got[k] - ex[k]) / fabs(ex[k]);
                    T_CHECK(rel <= REL_NORM, "normalize element %d component %d: %.9g, exact %.9g, relative %.3g", i, k, (double)got[k], ex[k], rel);
                    if (rel > worst_norm_rel) worst_norm_rel = rel;
                }
            }
            {
                const double len = sqrt((double)vout[i].x * vout[i].x + (double)vout[i].y * vout[i].y + (double)vout[i].z * vout[i].z);
                T_CHECK(fabs(len - 1.0) <= 2.0 * REL_NORM, "normalize element %d has length %.9g", i, len);
            }
        }
    }
    printf("normalize batch: worst relative error %.3g against the exact unit vector; largest distance from shz_vec3_normalize() %u ulp\n",
           worst_norm_rel, (unsigned)max_ulps_norm);

    /* -- magnitude and the vec4 normalisation: the exact value within 2^-19 relative; a zero vector gives a zero length -- */
    for (trial = 0; trial < 4000; trial++) {
        shz_vec3_t a;
        shz_vec4_t v4, n4, o4;
        double len, len4;
        random_vec(&a, -4.0f, 4.0f);
        if (trial % 9 == 0) a = shz_vec3_init(0.0f, 0.0f, 0.0f);
        len = sqrt((double)a.x * a.x + (double)a.y * a.y + (double)a.z * a.z);
        if (len == 0.0) {
            T_CHECK(shz_dce_vec3_magnitude_fipr(a) == 0.0f, "magnitude of a zero vector is not zero");
        } else {
            const double rel = fabs((double)shz_dce_vec3_magnitude_fipr(a) - len) / len;
            T_CHECK(rel <= REL_NORM, "magnitude: %.9g, exact %.9g", (double)shz_dce_vec3_magnitude_fipr(a), len);
            if (rel > worst_mag_rel) worst_mag_rel = rel;
        }
        {
            const uint32_t u = t_ulps(shz_dce_vec3_magnitude_fipr(a), shz_vec3_magnitude(a));
            if (u > max_ulps_mag) max_ulps_mag = u;
        }
        v4 = shz_vec4_init(a.x, a.y, a.z, t_frand(-4.0f, 4.0f));
        len4 = sqrt((double)v4.x * v4.x + (double)v4.y * v4.y + (double)v4.z * v4.z + (double)v4.w * v4.w);
        n4 = shz_dce_vec4_normalize(v4);
        o4 = shz_vec4_normalize(v4);
        {
            const float got[4] = { n4.x, n4.y, n4.z, n4.w };
            int k;
            for (k = 0; k < 4; k++) {
                const double ex = (double)v4.e[k] / len4;
                if (fabs(ex) > 1e-3) {
                    const double rel = fabs((double)got[k] - ex) / fabs(ex);
                    T_CHECK(rel <= REL_NORM, "vec4 normalize lane %d: %.9g, exact %.9g", k, (double)got[k], ex);
                    if (rel > worst_v4_rel) worst_v4_rel = rel;
                }
                {
                    const uint32_t u = t_ulps(got[k], o4.e[k]);
                    if (u > max_ulps_v4) max_ulps_v4 = u;
                }
            }
        }
    }
    printf("magnitude: worst relative error %.3g, largest distance from SH4ZAM's %u ulp\n", worst_mag_rel, (unsigned)max_ulps_mag);
    printf("vec4 normalize: worst relative error %.3g, largest distance from SH4ZAM's %u ulp\n", worst_v4_rel, (unsigned)max_ulps_v4);

#ifdef __FAST_MATH__
    printf("dot batch C form: %d elements differ from shz_vec3_dot() under -ffast-math (the compiler contracts the loop differently)\n", cdiff);
#else
    T_CHECK(cdiff == 0, "dot batch C form differs from shz_vec3_dot() in %d elements under strict flags", cdiff);
    printf("dot batch C form: %d elements differ from shz_vec3_dot() under strict flags\n", cdiff);
#endif

    host_scales();
    host_inplace_normalize();
#ifndef __FAST_MATH__
    host_specials();
    host_dot4_zero();
#endif

    return t_done("vector_test");
}
