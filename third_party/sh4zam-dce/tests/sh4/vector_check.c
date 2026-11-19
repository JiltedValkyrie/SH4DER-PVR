/* SH4ZAM-DCE tests: shz_dce_vector.h as the SH-4 runs it

   vector_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   vector_check <vector_kernels.elf>
   The ELF is vector_kernels.c built by sh-elf-gcc; tests/sh4run executes it instruction by instruction. For each routine:
   - the SH-4 batch against the exact value, inside the bound the header states (the FIPR path and the plain C form both), at three scales
   - the FIPR batch's elements against the single-element FIPR (the odd tail's path): bit for bit, every element; and with a NaN or an
     infinity planted in one element no other element may change (the two elements of a pair share one FIPR)
   - the cross batch against SH4ZAM's cross loop on the SH-4, bit for bit, also in place; dot4 against four SH4ZAM dot products, bit for
     bit, the signed zeros included in the strict-flag builds
   - the normalising batch, also in place, and the one-vector routines: against the exact value (relative), and against SH4ZAM's routines
     (distance in ulps, and the same NaN and zero results for a zero vector, an infinity, a NaN and a squared length that overflows)
   - every call twice, the scratch registers seeded differently: the results must not change
   - the interpreter itself stops a run on an unaligned load or store, a callee-saved register or FPSCR mode bit not put back: so a pass here
     is also the "alignment: checked" of every instruction the routines executed
   Data is placed on the weakest alignment each routine's comment allows: 8 for the dot batch's element arrays (its outputs on 4 mod 8),
   and 4 mod 8 for every other buffer, so an 8-byte access would stop the run. The interpreter stops only on an access outside the RAM
   or a misaligned one: a read past the end of an array inside the RAM is not seen here (the code is checked by reading it).
*/

#include "vector_jobs.h"
#include "sh4_check.h"

#define BOUND 4.76837158203125e-07   /* 2^-21 */
#define REL_NORM 1.9073486328125e-06   /* 2^-19: the FSRRA approximation and the FIPR products, with room */

static float first_buf[64 * 4 + 8];
static int strict_build = 1;   /* 0 for the -ffast-math kernels (the compiler may then drop signed zeros) */

static void random3(float *v, float lo, float hi) {
    v[0] = t_frand(lo, hi); v[1] = t_frand(lo, hi); v[2] = t_frand(lo, hi);
}

/* the exact inner product of two 3-vectors (in double) and P, the largest of the three products */
static double exact3(const float *a, const float *b, double *pmax) {
    const double t[3] = { (double)a[0] * b[0], (double)a[1] * b[1], (double)a[2] * b[2] };
    int k;
    *pmax = 0.0;
    for (k = 0; k < 3; k++) if (fabs(t[k]) > *pmax) *pmax = fabs(t[k]);
    return t[0] + t[1] + t[2];
}

static void stamp(void *p, uint32_t bytes) { memset(p, 0xee, bytes); }

static int guard_ok(const void *p, uint32_t bytes) {
    const uint8_t *q = (const uint8_t *)p;
    uint32_t i;
    for (i = 0; i < bytes; i++) if (q[i] != 0xee) return 0;
    return 1;
}

/* the classes of a result, read from its bits (the compiler may assume there are no NaNs under -ffast-math: the tests do not ask it) */
static int is_nan_bits(float f) { const uint32_t b = t_bits(f); return (b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) != 0u; }
static int is_inf_bits(float f) { return (t_bits(f) & 0x7fffffffu) == 0x7f800000u; }
static int is_zero_bits(float f) { return (t_bits(f) & 0x7fffffffu) == 0u; }

/* two results agree: the same bits, or both NaN, or both infinite with the same sign */
static int agree(float got, float ref) {
    if (is_nan_bits(ref)) return is_nan_bits(got);
    if (is_inf_bits(ref)) return is_inf_bits(got) && (t_bits(got) >> 31) == (t_bits(ref) >> 31);
    return t_bits(got) == t_bits(ref);
}

/* the special inputs the tests plant: 0 a zero vector, 1 +infinity in x, 2 -infinity in y, 3 NaN in z, 4 a squared length that overflows,
   5 a normal vector */
static void special3(float *v, int kind) {
    random3(v, -50.0f, 50.0f);
    switch (kind) {
    case 0: v[0] = 0.0f; v[1] = 0.0f; v[2] = 0.0f; break;
    case 1: v[0] = t_float(0x7f800000u); break;
    case 2: v[1] = t_float(0xff800000u); break;
    case 3: v[2] = t_float(0x7fc00000u); break;
    case 4: v[0] = 1e20f; v[1] = 0.0f; v[2] = 0.0f; break;
    default: break;
    }
}

/* the same classes for a 4-vector (the NaN goes in w) */
static void special4(float *v, int kind) {
    int k;
    for (k = 0; k < 4; k++) v[k] = t_frand(-50.0f, 50.0f);
    switch (kind) {
    case 0: for (k = 0; k < 4; k++) v[k] = 0.0f; break;
    case 1: v[0] = t_float(0x7f800000u); break;
    case 2: v[1] = t_float(0xff800000u); break;
    case 3: v[3] = t_float(0x7fc00000u); break;
    case 4: v[0] = 1e20f; v[1] = 0.0f; v[2] = 0.0f; v[3] = 0.0f; break;
    default: break;
    }
}

static void test_dot_batch(void) {
    int trial, even_differs = 0, odd_differs = 0;
    double worst_ratio = 0.0, worst_c_ratio = 0.0;
    for (trial = 0; trial < 300; trial++) {
        const uint32_t count = t_rand() % 38u;
        const uint32_t bytes_in = count * 12u + 8u;
        const float sc = trial % 3 == 0 ? 1.0f : trial % 3 == 1 ? 1e-15f : 1e15f;   /* normal, tiny and huge: the products stay normal */
        const uint32_t aj = ram(sizeof(job_dot_t), 8, 4);
        const uint32_t aa = ram(bytes_in, 8, 0), ab = ram(bytes_in, 8, 0);
        const uint32_t ao1 = ram(count * 4u + 8u, 8, 4), ao2 = ram(count * 4u + 8u, 8, 4), ao3 = ram(count * 4u + 8u, 8, 4);
        job_dot_t *j = (job_dot_t *)host(aj, sizeof *j);
        float *a = (float *)host(aa, bytes_in), *b = (float *)host(ab, bytes_in);
        float *o1 = (float *)host(ao1, count * 4u + 8u), *o2 = (float *)host(ao2, count * 4u + 8u), *o3 = (float *)host(ao3, count * 4u + 8u);
        uint32_t i, k;
        for (i = 0; i < count; i++) {
            random3(a + 3 * i, -6.0f * sc, 6.0f * sc);
            random3(b + 3 * i, -6.0f * sc, 6.0f * sc);
        }
        stamp(o1, count * 4u + 8u); stamp(o2, count * 4u + 8u); stamp(o3, count * 4u + 8u);
        j->a = aa; j->b = ab; j->out_asm = ao1; j->out_c = ao2; j->out_one = ao3; j->count = count;
        call("_t_dot_batch", aj);
        memcpy(first_buf, o1, count * 4u);
        call("_t_dot_batch", aj);   /* again, other garbage in the scratch registers */
        T_CHECK(!memcmp(first_buf, o1, count * 4u), "dot batch count %u: a second run gave other results", (unsigned)count);
        T_CHECK(guard_ok((const char *)o1 + count * 4u, 8u), "dot batch count %u wrote past its last result", (unsigned)count);
        T_CHECK(guard_ok((const char *)o2 + count * 4u, 8u), "dot batch (C form) count %u wrote past its last result", (unsigned)count);
        T_CHECK(guard_ok((const char *)o3 + count * 4u, 8u), "dot batch (one by one) count %u wrote past its last result", (unsigned)count);
        for (k = 0; k < count; k++) {
            double pm;
            const double e = exact3(a + 3 * k, b + 3 * k, &pm);
            const double bound = (pm + fabs(e)) * BOUND;
            const double r1 = fabs((double)o1[k] - e) / bound, r2 = fabs((double)o2[k] - e) / bound;
            T_CHECK(r1 <= 1.0, "dot batch count %u element %u: %.9g, exact %.9g, bound %.3g", (unsigned)count, (unsigned)k, (double)o1[k], e, bound);
            T_CHECK(r2 <= 1.0, "dot batch C form count %u element %u: %.9g, exact %.9g, bound %.3g", (unsigned)count, (unsigned)k, (double)o2[k], e, bound);
            if (bound > 0.0 && r1 > worst_ratio) worst_ratio = r1;
            if (bound > 0.0 && r2 > worst_c_ratio) worst_c_ratio = r2;
            if (t_bits(o1[k]) != t_bits(o3[k])) {
                if (k & 1u) odd_differs++; else even_differs++;
            }
        }
        T_CHECK(even_differs == 0 && odd_differs == 0, "dot batch: an element differs from the single-element FIPR (trial %d)", trial);
        sh4_alloc_reset(S);
    }
    printf("dot batch: %d elements differ from the single-element FIPR (even %d, odd %d); worst error / bound: FIPR %.3g, C form %.3g\n",
           even_differs + odd_differs, even_differs, odd_differs, worst_ratio, worst_c_ratio);
}

/* a NaN or an infinity planted in one element (in a or in b): the batch pairs two elements in one FIPR, so every result must still equal
   the single-element FIPR bit for bit, and no element without a planted value may come out NaN or infinite */
static void test_dot_poison(void) {
    static const uint32_t planted[3] = { 0x7fc00000u, 0x7f800000u, 0xff800000u };   /* a quiet NaN, +infinity, -infinity */
    int trial, mismatches = 0, neighbour_hit = 0, cases = 0;
    for (trial = 0; trial < 400; trial++) {
        const uint32_t count = 2u + t_rand() % 36u;   /* 2..37: pairs, and odd tails */
        const uint32_t bytes_in = count * 12u;
        const uint32_t aj = ram(sizeof(job_dot_t), 8, 4);
        const uint32_t aa = ram(bytes_in, 8, 0), ab = ram(bytes_in, 8, 0);
        const uint32_t ao1 = ram(count * 4u + 8u, 8, 4), ao2 = ram(count * 4u + 8u, 8, 4), ao3 = ram(count * 4u + 8u, 8, 4);
        job_dot_t *j = (job_dot_t *)host(aj, sizeof *j);
        float *a = (float *)host(aa, bytes_in), *b = (float *)host(ab, bytes_in);
        float *o1 = (float *)host(ao1, count * 4u + 8u), *o2 = (float *)host(ao2, count * 4u + 8u), *o3 = (float *)host(ao3, count * 4u + 8u);
        const uint32_t p = t_rand() % count;
        uint32_t i;
        for (i = 0; i < count; i++) {
            random3(a + 3 * i, -6.0f, 6.0f);
            random3(b + 3 * i, -6.0f, 6.0f);
        }
        if (t_rand() & 1u) a[3 * p + t_rand() % 3u] = t_float(planted[t_rand() % 3u]);
        else               b[3 * p + t_rand() % 3u] = t_float(planted[t_rand() % 3u]);
        stamp(o1, count * 4u + 8u); stamp(o2, count * 4u + 8u); stamp(o3, count * 4u + 8u);
        j->a = aa; j->b = ab; j->out_asm = ao1; j->out_c = ao2; j->out_one = ao3; j->count = count;
        call("_t_dot_batch", aj);
        T_CHECK(guard_ok((const char *)o1 + count * 4u, 8u), "dot batch with a planted value wrote past its last result");
        for (i = 0; i < count; i++) {
            cases++;
            if (t_bits(o1[i]) != t_bits(o3[i])) mismatches++;
            if (i != p && (t_bits(o1[i]) & 0x7f800000u) == 0x7f800000u) neighbour_hit++;
        }
        sh4_alloc_reset(S);
    }
    T_CHECK(mismatches == 0, "dot batch with a planted value: %d of %d results differ from the single-element FIPR", mismatches, cases);
    T_CHECK(neighbour_hit == 0, "dot batch with a planted value: %d elements without one came out NaN or infinite", neighbour_hit);
    printf("dot batch with a planted NaN or infinity: %d of %d results differ from the single-element FIPR; %d neighbours hit\n",
           mismatches, cases, neighbour_hit);
}

static void test_dot4(void) {
    int trial, differs = 0;
    for (trial = 0; trial < 300; trial++) {
        const uint32_t aj = ram(sizeof(job_dot4_t), 8, 4);
        const uint32_t aa = ram(48u, 8, 4), ab = ram(48u, 8, 4);
        const uint32_t ac = ram(16u, 8, 4), ar = ram(16u, 8, 4);
        job_dot4_t *j = (job_dot4_t *)host(aj, sizeof *j);
        float *a = (float *)host(aa, 48u), *b = (float *)host(ab, 48u);
        float *oc = (float *)host(ac, 16u), *orf = (float *)host(ar, 16u);
        int k;
        for (k = 0; k < 4; k++) { random3(a + 3 * k, -8.0f, 8.0f); random3(b + 3 * k, -8.0f, 8.0f); }
        j->a = aa; j->b = ab; j->out_c = ac; j->out_ref = ar;
        call("_t_dot4", aj);
        for (k = 0; k < 4; k++) {
            double pm;
            const double e = exact3(a + 3 * k, b + 3 * k, &pm), bound = (pm + fabs(e)) * BOUND;
            T_CHECK(fabs((double)oc[k] - e) <= bound, "dot4 lane %d: %.9g, exact %.9g, bound %.3g", k, (double)oc[k], e, bound);
            if (t_bits(oc[k]) != t_bits(orf[k])) differs++;
        }
        sh4_alloc_reset(S);
    }
    T_CHECK(differs == 0, "dot4: %d lanes differ from four shz_vec3_dot() calls on the SH-4", differs);
    printf("dot4: %d lanes of 1200 differ from four shz_vec3_dot() calls on the SH-4\n", differs);
}

/* exact zero results (every product -0): SH4ZAM's dot product on the SH-4 adds a zero term (its w.w lane), which turns -0 into +0; this
   routine does not. So the lanes may differ in the sign of an exact zero, and in nothing else: any other difference fails */
static void test_dot4_zero(void) {
    int trial, differs = 0, lanes = 0, other = 0;
    for (trial = 0; trial < 120; trial++) {
        const uint32_t aj = ram(sizeof(job_dot4_t), 8, 4);
        const uint32_t aa = ram(48u, 8, 4), ab = ram(48u, 8, 4);
        const uint32_t ac = ram(16u, 8, 4), ar = ram(16u, 8, 4);
        job_dot4_t *j = (job_dot4_t *)host(aj, sizeof *j);
        float *a = (float *)host(aa, 48u), *b = (float *)host(ab, 48u);
        float *oc = (float *)host(ac, 16u), *orf = (float *)host(ar, 16u);
        int k;
        for (k = 0; k < 4; k++) {
            float *x = a + 3 * k, *y = b + 3 * k;
            switch (t_rand() % 4u) {
            case 0: x[0] = -1.0f; x[1] = -1.0f; x[2] = -1.0f; y[0] = 0.0f; y[1] = 0.0f; y[2] = 0.0f; break;
            case 1: x[0] = 0.0f; x[1] = 0.0f; x[2] = 0.0f; y[0] = -1.0f; y[1] = -1.0f; y[2] = -1.0f; break;
            case 2: x[0] = -0.0f; x[1] = -0.0f; x[2] = -0.0f; y[0] = 1.0f; y[1] = 1.0f; y[2] = 1.0f; break;
            default: random3(x, -8.0f, 8.0f); random3(y, -8.0f, 8.0f); break;
            }
        }
        j->a = aa; j->b = ab; j->out_c = ac; j->out_ref = ar;
        call("_t_dot4", aj);
        for (k = 0; k < 4; k++) {
            lanes++;
            if (t_bits(oc[k]) != t_bits(orf[k])) {
                differs++;
                if (!(is_zero_bits(oc[k]) && is_zero_bits(orf[k]))) other++;
            }
        }
        sh4_alloc_reset(S);
    }
    T_CHECK(other == 0, "dot4 on signed zeros: %d lanes differ from shz_vec3_dot() in more than the sign of an exact zero", other);
    printf("dot4 on signed zeros: %d of %d lanes differ from shz_vec3_dot() on the SH-4, all of them in the sign of an exact zero (%s build)\n",
           differs, lanes, strict_build ? "strict" : "fast-math");
}

static void test_cross(void) {
    int trial;
    for (trial = 0; trial < 300; trial++) {
        const uint32_t count = t_rand() % 38u;
        const uint32_t bytes = count * 12u;
        const uint32_t aj = ram(sizeof(job_cross_t), 8, 4);
        const uint32_t aa = ram(bytes + 4u, 8, 4), ab = ram(bytes + 4u, 8, 4);
        const uint32_t ao = ram(bytes + 12u, 8, 4), ar = ram(bytes + 12u, 8, 4);
        job_cross_t *j = (job_cross_t *)host(aj, sizeof *j);
        float *a = (float *)host(aa, bytes + 4u), *b = (float *)host(ab, bytes + 4u);
        float *oa = (float *)host(ao, bytes + 12u), *orf = (float *)host(ar, bytes + 12u);
        uint32_t k;
        for (k = 0; k < count; k++) { random3(a + 3 * k, -4.0f, 4.0f); random3(b + 3 * k, -4.0f, 4.0f); }
        stamp(oa, bytes + 12u); stamp(orf, bytes + 12u);
        j->a = aa; j->b = ab; j->out_asm = ao; j->out_ref = ar; j->count = count;
        call("_t_cross_batch", aj);
        T_CHECK(!memcmp(oa, orf, bytes), "cross batch count %u: not the bits of shz_vec3_cross() on the SH-4", (unsigned)count);
        T_CHECK(guard_ok((const char *)oa + bytes, 12u), "cross batch count %u wrote past its last element", (unsigned)count);
        sh4_alloc_reset(S);
    }
    printf("cross batch: SH-4 batch == SH4ZAM's cross loop, bit for bit\n");
}

/* out == a: the batch reads an element before it writes it; the reference is taken before the call (the kernel does that) */
static void test_cross_inplace(void) {
    int trial;
    for (trial = 0; trial < 150; trial++) {
        const uint32_t count = t_rand() % 38u;
        const uint32_t bytes = count * 12u;
        const uint32_t aj = ram(sizeof(job_cross_t), 8, 4);
        const uint32_t aa = ram(bytes + 4u, 8, 4), ab = ram(bytes + 4u, 8, 4), ar = ram(bytes + 12u, 8, 4);
        job_cross_t *j = (job_cross_t *)host(aj, sizeof *j);
        float *a = (float *)host(aa, bytes + 4u), *b = (float *)host(ab, bytes + 4u), *orf = (float *)host(ar, bytes + 12u);
        uint32_t k;
        for (k = 0; k < count; k++) { random3(a + 3 * k, -4.0f, 4.0f); random3(b + 3 * k, -4.0f, 4.0f); }
        stamp(orf, bytes + 12u);
        j->a = aa; j->b = ab; j->out_asm = aa; j->out_ref = ar; j->count = count;
        call("_t_cross_batch", aj);
        T_CHECK(!memcmp(a, orf, bytes), "cross batch in place, count %u: not the bits of shz_vec3_cross() on the SH-4", (unsigned)count);
        sh4_alloc_reset(S);
    }
    printf("cross batch in place: SH-4 batch == SH4ZAM's cross loop, bit for bit\n");
}

/* the normalising batch against the exact unit vector, and against SH4ZAM's loop; three scales: normal, tiny and huge */
static void test_normalize(void) {
    int trial, differs = 0;
    double worst = 0.0;
    for (trial = 0; trial < 300; trial++) {
        const uint32_t count = t_rand() % 38u;
        const uint32_t bytes = count * 12u;
        const float sc = trial % 3 == 0 ? 50.0f : trial % 3 == 1 ? 5e-14f : 5e15f;
        const uint32_t aj = ram(sizeof(job_norm_t), 8, 4);
        const uint32_t ai = ram(bytes + 4u, 8, 4);
        const uint32_t ac = ram(bytes + 12u, 8, 4), ar = ram(bytes + 12u, 8, 4);
        job_norm_t *j = (job_norm_t *)host(aj, sizeof *j);
        float *in = (float *)host(ai, bytes + 4u);
        float *oc = (float *)host(ac, bytes + 12u), *orf = (float *)host(ar, bytes + 12u);
        uint32_t k;
        for (k = 0; k < count; k++) random3(in + 3 * k, -sc, sc);
        stamp(oc, bytes + 12u); stamp(orf, bytes + 12u);
        j->in = ai; j->out_c = ac; j->out_ref = ar; j->count = count;
        call("_t_normalize_batch", aj);
        T_CHECK(guard_ok((const char *)oc + bytes, 12u), "normalize batch count %u wrote past its last element", (unsigned)count);
        for (k = 0; k < count; k++) {
            const double n2 = (double)in[3 * k] * in[3 * k] + (double)in[3 * k + 1] * in[3 * k + 1] + (double)in[3 * k + 2] * in[3 * k + 2];
            const double inv = 1.0 / sqrt(n2);
            int c;
            for (c = 0; c < 3; c++) {
                const double ex = in[3 * k + c] * inv;
                if (t_bits(oc[3 * k + c]) != t_bits(orf[3 * k + c])) differs++;
                if (fabs(ex) > 1e-3) {
                    const double rel = fabs((double)oc[3 * k + c] - ex) / fabs(ex);
                    T_CHECK(rel <= REL_NORM, "normalize element %u component %d: relative %.3g", (unsigned)k, c, rel);
                    if (rel > worst) worst = rel;
                }
            }
        }
        sh4_alloc_reset(S);
    }
    printf("normalize batch: %d components differ from SH4ZAM's normalize loop on the SH-4; worst relative error against the exact unit vector %.3g\n",
           differs, worst);
}

/* out == in: the batch loads each element before it stores it; checked against the exact unit vector */
static void test_normalize_inplace(void) {
    int trial;
    double worst = 0.0;
    for (trial = 0; trial < 200; trial++) {
        const uint32_t count = t_rand() % 38u;
        const uint32_t bytes = count * 12u;
        const uint32_t aj = ram(sizeof(job_norm_t), 8, 4);
        const uint32_t ai = ram(bytes + 4u, 8, 4), ar = ram(bytes + 12u, 8, 4);
        job_norm_t *j = (job_norm_t *)host(aj, sizeof *j);
        float *io = (float *)host(ai, bytes + 4u);
        float *orf = (float *)host(ar, bytes + 12u);
        float keep[38 * 3];
        uint32_t k;
        for (k = 0; k < count; k++) random3(io + 3 * k, -50.0f, 50.0f);
        memcpy(keep, io, bytes);
        stamp(orf, bytes + 12u);
        memset((char *)io + bytes, 0xee, 4u);
        j->in = ai; j->out_c = ai; j->out_ref = ar; j->count = count;
        call("_t_normalize_batch", aj);
        T_CHECK(guard_ok((const char *)io + bytes, 4u), "normalize batch in place, count %u wrote past its last element", (unsigned)count);
        for (k = 0; k < count; k++) {
            const double n2 = (double)keep[3 * k] * keep[3 * k] + (double)keep[3 * k + 1] * keep[3 * k + 1] + (double)keep[3 * k + 2] * keep[3 * k + 2];
            const double inv = 1.0 / sqrt(n2);
            int c;
            for (c = 0; c < 3; c++) {
                const double ex = keep[3 * k + c] * inv;
                if (fabs(ex) > 1e-3) {
                    const double rel = fabs((double)io[3 * k + c] - ex) / fabs(ex);
                    T_CHECK(rel <= REL_NORM, "normalize in place, element %u component %d: relative %.3g", (unsigned)k, c, rel);
                    if (rel > worst) worst = rel;
                }
            }
        }
        sh4_alloc_reset(S);
    }
    printf("normalize batch in place: worst relative error against the exact unit vector %.3g\n", worst);
}

/* the special inputs of the normalising batch: where SH4ZAM's routine gives NaN, or zero, the batch does too; an element with a special input
   changes no other element (the normal vectors are checked against the exact unit vector) */
static void test_normalize_special(void) {
    int trial, cases = 0;
    for (trial = 0; trial < 200; trial++) {
        const uint32_t count = 1u + t_rand() % 20u;
        const uint32_t bytes = count * 12u;
        const uint32_t aj = ram(sizeof(job_norm_t), 8, 4);
        const uint32_t ai = ram(bytes + 4u, 8, 4), ac = ram(bytes + 12u, 8, 4), ar = ram(bytes + 12u, 8, 4);
        job_norm_t *j = (job_norm_t *)host(aj, sizeof *j);
        float *in = (float *)host(ai, bytes + 4u), *oc = (float *)host(ac, bytes + 12u), *orf = (float *)host(ar, bytes + 12u);
        int kind[20];
        uint32_t k;
        for (k = 0; k < count; k++) {
            kind[k] = (int)(t_rand() % 6u);
            special3(in + 3 * k, kind[k]);
        }
        stamp(oc, bytes + 12u); stamp(orf, bytes + 12u);
        j->in = ai; j->out_c = ac; j->out_ref = ar; j->count = count;
        call("_t_normalize_batch", aj);
        T_CHECK(guard_ok((const char *)oc + bytes, 12u), "normalize batch special inputs: count %u wrote past its last element", (unsigned)count);
        for (k = 0; k < count; k++) {
            int c;
            cases++;
            for (c = 0; c < 3; c++) {
                const float got = oc[3 * k + c], ref = orf[3 * k + c];
                if (kind[k] == 5) {
                    const double n2 = (double)in[3 * k] * in[3 * k] + (double)in[3 * k + 1] * in[3 * k + 1] + (double)in[3 * k + 2] * in[3 * k + 2];
                    const double ex = in[3 * k + c] / sqrt(n2);
                    if (fabs(ex) > 1e-3) {
                        const double rel = fabs((double)got - ex) / fabs(ex);
                        T_CHECK(rel <= REL_NORM, "normalize special: normal element %u component %d: relative %.3g", (unsigned)k, c, rel);
                    }
                } else if (is_nan_bits(ref)) {
                    T_CHECK(is_nan_bits(got), "normalize special: kind %d element %u component %d: %g, SH4ZAM NaN", kind[k], (unsigned)k, c, (double)got);
                } else if (is_zero_bits(ref)) {
                    T_CHECK(is_zero_bits(got), "normalize special: kind %d element %u component %d: %g, SH4ZAM zero", kind[k], (unsigned)k, c, (double)got);
                } else {
                    T_CHECK(agree(got, ref), "normalize special: kind %d element %u component %d: %g, SH4ZAM %g", kind[k], (unsigned)k, c, (double)got, (double)ref);
                }
            }
        }
        sh4_alloc_reset(S);
    }
    printf("normalize batch special inputs: %d vectors checked against SH4ZAM's routine\n", cases);
}

/* the one-vector routines: magnitude against the exact length, and both routines against SH4ZAM's in ulps */
static void test_one(void) {
    uint32_t max_ulps = 0u;
    double worst = 0.0;
    int trial;
    for (trial = 0; trial < 300; trial++) {
        const uint32_t count = 1u + t_rand() % 37u;
        const uint32_t bytes = count * 12u;
        const uint32_t aj = ram(sizeof(job_one_t), 8, 4);
        const uint32_t aa = ram(bytes + 4u, 8, 4);
        const uint32_t om = ram(count * 4u + 8u, 8, 4), rm = ram(count * 4u + 8u, 8, 4);
        job_one_t *j = (job_one_t *)host(aj, sizeof *j);
        float *a = (float *)host(aa, bytes + 4u);
        float *gm = (float *)host(om, count * 4u + 8u), *rmg = (float *)host(rm, count * 4u + 8u);
        uint32_t k;
        for (k = 0; k < count; k++) {
            random3(a + 3 * k, -4.0f, 4.0f);
            if (k % 7u == 3u) { a[3 * k] = a[3 * k + 1] = a[3 * k + 2] = 0.0f; }   /* a zero vector now and then */
        }
        j->a = aa; j->count = count; j->o_mag = om; j->r_mag = rm;
        call("_t_one", aj);
        for (k = 0; k < count; k++) {
            const float *v = a + 3 * k;
            const double len = sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]);
            if (len == 0.0) {
                T_CHECK(gm[k] == 0.0f, "magnitude of a zero vector is %g", (double)gm[k]);
            } else {
                const double rel = fabs((double)gm[k] - len) / len;
                T_CHECK(rel <= REL_NORM, "magnitude element %u: %.9g, exact %.9g", (unsigned)k, (double)gm[k], len);
                if (rel > worst) worst = rel;
            }
            if (t_ulps(gm[k], rmg[k]) > max_ulps) max_ulps = t_ulps(gm[k], rmg[k]);
        }
        sh4_alloc_reset(S);
    }
    printf("magnitude: worst relative error against the exact length %.3g, largest distance from SH4ZAM's %u ulp\n", worst, (unsigned)max_ulps);
}

/* the length on the special inputs against SH4ZAM's: the same NaN for an infinite or NaN input, the same zero for a zero vector */
static void test_one_special(void) {
    int trial, cases = 0;
    for (trial = 0; trial < 200; trial++) {
        const uint32_t count = 1u + t_rand() % 20u;
        const uint32_t aj = ram(sizeof(job_one_t), 8, 4);
        const uint32_t aa = ram(count * 12u + 4u, 8, 4);
        const uint32_t om = ram(count * 4u + 8u, 8, 4), rm = ram(count * 4u + 8u, 8, 4);
        job_one_t *j = (job_one_t *)host(aj, sizeof *j);
        float *a = (float *)host(aa, count * 12u + 4u);
        float *gm = (float *)host(om, count * 4u + 8u), *rmg = (float *)host(rm, count * 4u + 8u);
        int kind[20];
        uint32_t k;
        for (k = 0; k < count; k++) {
            kind[k] = (int)(t_rand() % 6u);
            special3(a + 3 * k, kind[k]);
        }
        j->a = aa; j->count = count; j->o_mag = om; j->r_mag = rm;
        call("_t_one", aj);
        for (k = 0; k < count; k++) {
            cases++;
            if (kind[k] == 5) {
                const float *v = a + 3 * k;
                const double len = sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]);
                T_CHECK(fabs((double)gm[k] - len) / len <= REL_NORM, "magnitude special: normal element %u: %.9g, exact %.9g", (unsigned)k, (double)gm[k], len);
            } else {
                T_CHECK(agree(gm[k], rmg[k]), "magnitude special: kind %d element %u: %g, SH4ZAM %g", kind[k], (unsigned)k, (double)gm[k], (double)rmg[k]);
            }
        }
        sh4_alloc_reset(S);
    }
    printf("magnitude special inputs: %d vectors agree with SH4ZAM's routine (NaN and zero included)\n", cases);
}

/* the vec4 normalisation: each component against the exact unit vector, and against SH4ZAM's in ulps */
static void test_one4(void) {
    uint32_t max_ulps = 0u;
    double worst = 0.0;
    int trial;
    for (trial = 0; trial < 300; trial++) {
        const uint32_t count = 1u + t_rand() % 37u;
        const uint32_t aj = ram(sizeof(job_one4_t), 8, 4);
        const uint32_t ai = ram(count * 16u + 8u, 8, 4);
        const uint32_t ao = ram(count * 16u + 16u, 8, 4), ar = ram(count * 16u + 16u, 8, 4);
        job_one4_t *j = (job_one4_t *)host(aj, sizeof(job_one4_t));
        float *in = (float *)host(ai, count * 16u + 8u);
        float *oo = (float *)host(ao, count * 16u + 16u), *rr = (float *)host(ar, count * 16u + 16u);
        uint32_t k;
        for (k = 0; k < count * 4u; k++) in[k] = t_frand(-4.0f, 4.0f);
        j->in = ai; j->o_v4 = ao; j->r_v4 = ar; j->count = count;
        call("_t_one4", aj);
        sh4_alloc_reset(S);
        for (k = 0; k < count; k++) {
            const float *v = in + 4 * k;
            const double len = sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2] + (double)v[3] * v[3]);
            int c;
            for (c = 0; c < 4; c++) {
                const double ex = v[c] / len;
                if (fabs(ex) > 1e-3) {
                    const double rel = fabs((double)oo[4 * k + c] - ex) / fabs(ex);
                    T_CHECK(rel <= REL_NORM, "vec4 normalize element %u component %d: relative %.3g", (unsigned)k, c, rel);
                    if (rel > worst) worst = rel;
                }
                if (t_ulps(oo[4 * k + c], rr[4 * k + c]) > max_ulps) max_ulps = t_ulps(oo[4 * k + c], rr[4 * k + c]);
            }
        }
    }
    printf("vec4 normalize: worst relative error against the exact unit vector %.3g, largest distance from SH4ZAM's %u ulp\n", worst, (unsigned)max_ulps);
}

/* the vec4 normalisation on the special inputs: the same NaN and zero pattern as SH4ZAM's */
static void test_one4_special(void) {
    int trial, cases = 0;
    for (trial = 0; trial < 200; trial++) {
        const uint32_t count = 1u + t_rand() % 20u;
        const uint32_t aj = ram(sizeof(job_one4_t), 8, 4);
        const uint32_t ai = ram(count * 16u + 8u, 8, 4);
        const uint32_t ao = ram(count * 16u + 16u, 8, 4), ar = ram(count * 16u + 16u, 8, 4);
        job_one4_t *j = (job_one4_t *)host(aj, sizeof(job_one4_t));
        float *in = (float *)host(ai, count * 16u + 8u);
        float *oo = (float *)host(ao, count * 16u + 16u), *rr = (float *)host(ar, count * 16u + 16u);
        int kind[20];
        uint32_t k;
        for (k = 0; k < count; k++) {
            kind[k] = (int)(t_rand() % 6u);
            special4(in + 4 * k, kind[k]);
        }
        j->in = ai; j->o_v4 = ao; j->r_v4 = ar; j->count = count;
        call("_t_one4", aj);
        sh4_alloc_reset(S);
        for (k = 0; k < count; k++) {
            int c;
            cases++;
            for (c = 0; c < 4; c++) {
                const float got = oo[4 * k + c], ref = rr[4 * k + c];
                if (kind[k] == 5) {
                    const float *v = in + 4 * k;
                    const double len = sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2] + (double)v[3] * v[3]);
                    const double ex = v[c] / len;
                    if (fabs(ex) > 1e-3) T_CHECK(fabs((double)got - ex) / fabs(ex) <= REL_NORM, "vec4 special: normal element %u component %d", (unsigned)k, c);
                } else if (is_nan_bits(ref)) {
                    T_CHECK(is_nan_bits(got), "vec4 special: kind %d element %u component %d: %g, SH4ZAM NaN", kind[k], (unsigned)k, c, (double)got);
                } else if (is_zero_bits(ref)) {
                    T_CHECK(is_zero_bits(got), "vec4 special: kind %d element %u component %d: %g, SH4ZAM zero", kind[k], (unsigned)k, c, (double)got);
                } else {
                    T_CHECK(agree(got, ref), "vec4 special: kind %d element %u component %d: %g, SH4ZAM %g", kind[k], (unsigned)k, c, (double)got, (double)ref);
                }
            }
        }
    }
    printf("vec4 normalize special inputs: %d vectors checked against SH4ZAM's routine\n", cases);
}

int main(int argc, char **argv) {
    const int rc = check_open(argc, argv);
    if (rc) return rc;
    strict_build = strstr(argv[1], "fast") == NULL;
    test_dot_batch();
    test_dot_poison();
    test_dot4();
    test_dot4_zero();
    test_cross();
    test_cross_inplace();
    test_normalize();
    test_normalize_inplace();
    test_normalize_special();
    test_one();
    test_one_special();
    test_one4();
    test_one4_special();
    return t_done("vector_check");
}
