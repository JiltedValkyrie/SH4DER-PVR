/* SH4ZAM-DCE tests: shz_dce_sqrt.h as the SH-4 runs it

   sqrt_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   sqrt_check <sqrt_kernels.elf>
   The ELF is sqrt_kernels.c built by sh-elf-gcc; tests/sh4run executes it instruction by instruction. For every routine of the header,
   on the same input (zeros of both signs, denormals (zero to the SH-4's compare), numbers whose square underflows, negatives,
   infinities and ordinary numbers over the whole exponent range):
   - the wrapper against SH4ZAM's routine of the same name: bit for bit, every value
   - SH4ZAM's roots and lengths against the exact value, inside a loose bound
   - every call twice, the scratch registers seeded differently: the results must not change
   Pinned to SH4ZAM master ff39547 (2026-11-22): there shz_sqrtf is FSQRT on the SH-4 and shz_inv_sqrtf returns shz_opaquef(0) on
   its zero path; the two "SH4ZAM ff39547 claim" lines below fail by name if a later SH4ZAM changes either.
*/

#include <sh4zam_dce/shz_dce_sqrt.h>

#include "sqrt_jobs.h"
#include "sh4_check.h"

#define REL 9.5367431640625e-07   /* 2^-20: a sanity bound, looser than FSRRA's own error */

static const uint32_t special[] = {
    0x00000000u, 0x80000000u,                 /* +0, -0 */
    0x00000001u, 0x80000001u, 0x007fffffu,    /* denormals: zero to the SH-4 (FPSCR.DN = 1) */
    0x00800000u, 0x0da24260u, 0x1e3ce508u,    /* the smallest normal, 1e-30, 1e-20 (their squares underflow) */
    0x3f800000u, 0x40800000u, 0x3e800000u, 0x40000000u, 0x3f000000u,   /* 1, 4, 0.25, 2, 0.5 */
    0x7149f2cau, 0x7f7fffffu,                 /* 1e30, the largest float (their squares overflow) */
    0x7f800000u,                              /* +infinity */
    0xbf800000u, 0xc0800000u, 0x80800000u,    /* -1, -4, the smallest negative normal */
};

static float pick(int trial, uint32_t k) {
    if (k < sizeof special / sizeof *special && (trial & 1)) return t_float(special[k]);
    switch (t_rand() % 6u) {
    case 0: return t_float(special[t_rand() % (sizeof special / sizeof *special)]);
    case 1: return t_float((t_rand() % 0x7f000000u) + 0x00800000u);            /* any positive normal */
    case 2: return t_float(((t_rand() % 0x7f000000u) + 0x00800000u) | 0x80000000u);
    case 3: return t_frand(-4.0f, 4.0f);
    case 4: return t_frand(0.0f, 1.0f) * 1e-18f;
    default: return t_frand(-2000.0f, 2000.0f);
    }
}

static int finite_normal(float f) { const uint32_t e = (t_bits(f) >> 23) & 0xffu; return e != 0u && e != 0xffu; }

static uint32_t built;
static unsigned long values;
static int sqrt_inf_seen, inv_zero_seen;

static void test(const char *fn, uint32_t per, int vector) {
    int trial;
    for (trial = 0; trial < 240; trial++) {
        const uint32_t count = trial < 4 ? (uint32_t)trial : 1u + t_rand() % 96u;
        const uint32_t bytes = count * per * 4 + 8;
        const uint32_t aj = ram(sizeof(job_sqrt_t), 4, 0), a1 = ram(count * 16 + 4, 8, 4), a2 = ram(count * 16 + 4, 8, 4);
        const uint32_t ao = ram(bytes, 8, 4), ad = ram(bytes, 8, 4);
        job_sqrt_t *j = (job_sqrt_t *)host(aj, sizeof *j);
        float *in = (float *)host(a1, count * 16 + 4), *in2 = (float *)host(a2, count * 16 + 4);
        float *o = (float *)host(ao, bytes), *d = (float *)host(ad, bytes);
        static float keep[2][96 * SQRT_VECTOR_OUT];
        uint32_t k, c;
        for (k = 0; k < count * 4; k++) {
            in[k] = pick(trial, vector ? k % 23u : k / 4u);
            in2[k] = (trial % 5 == 0) ? in[k] : pick(trial, 1000u);   /* every fifth trial: equal vectors, a zero distance */
        }
        if (vector) for (k = 0; k + 4 <= count * 4; k += 4) {
            if (k % 28u == 0u) { in[k] = in[k + 1] = in[k + 2] = in[k + 3] = (k & 4u) ? -0.0f : 0.0f; }          /* the zero vector, both signs */
            else if (k % 28u == 8u) { in[k] = 1e-25f; in[k + 1] = -3e-24f; in[k + 2] = 2e-30f; in[k + 3] = 0.0f; }   /* its square underflows */
        }
        j->in = a1; j->in2 = a2; j->count = count; j->out_shz = ao; j->out_dce = ad;
        memset(o, 0xee, bytes); memset(d, 0xee, bytes);
        call(fn, aj);
        T_CHECK(!memcmp(o + count * per, d + count * per, 8) && t_bits(o[count * per]) == 0xeeeeeeeeu, "%s: a write past the last result", fn);
        for (k = 0; k < count * per; k++) {
            values++;
            T_CHECK(t_bits(o[k]) == t_bits(d[k]), "wrapper: %s element %u value %u: SH4ZAM %08x, shz_dce_ %08x", fn, (unsigned)(k / per), (unsigned)(k % per),
                    (unsigned)t_bits(o[k]), (unsigned)t_bits(d[k]));
        }
        memcpy(keep[0], o, count * per * 4); memcpy(keep[1], d, count * per * 4);
        call(fn, aj);   /* again, other garbage in the scratch registers */
        T_CHECK(!memcmp(keep[0], o, count * per * 4) && !memcmp(keep[1], d, count * per * 4), "%s: a second run gave other results (a register read before it was written)", fn);
        for (c = 0; c < count; c++) {   /* sanity against the exact values, for inputs whose squares neither underflow nor overflow */
            const float *a = in + 4 * c, *b = in2 + 4 * c, *r = o + c * per;
            if (!vector) {
                const double x = a[0], y = b[0], h = sqrt(x * x + y * y);
                if (t_bits(a[0]) == 0x7f800000u) {
                    sqrt_inf_seen++;
                    T_CHECK(t_bits(r[0]) == 0x7f800000u, "SH4ZAM ff39547 claim: sqrtf(+inf) is +inf (FSQRT), got %08x", (unsigned)t_bits(r[0]));
                }
                if (t_bits(a[0]) == 0u) {
                    inv_zero_seen++;
                    T_CHECK(t_bits(r[1]) == 0u, "SH4ZAM ff39547 claim: inv_sqrtf(+0) is +0 (behind shz_opaquef), got %08x", (unsigned)t_bits(r[1]));
                }
                if (finite_normal(a[0]) && a[0] > 0.0f) {
                    T_CHECK(fabs(r[0] - sqrt(x)) <= sqrt(x) * REL, "sqrtf(%g) = %g", x, (double)r[0]);
                    T_CHECK(fabs(r[1] - 1.0 / sqrt(x)) <= REL / sqrt(x), "inv_sqrtf(%g) = %g", x, (double)r[1]);
                }
                if (h > 1e-15 && h < 1e15) T_CHECK(fabs(r[2] - h) <= h * 2.0 * REL, "hypotf(%g, %g) = %g", x, y, (double)r[2]);
            } else {
                double m[5] = { 0, 0, 0, 0, 0 }, dd[5] = { 0, 0, 0, 0, 0 };
                int n, ok = 1;
                for (n = 0; n < 4; n++) {
                    if (!(fabs(a[n]) < 1e15) || !(fabs(b[n]) < 1e15)) ok = 0;
                    m[n + 1] = m[n] + (double)a[n] * a[n];
                    dd[n + 1] = dd[n] + ((double)a[n] - b[n]) * ((double)a[n] - b[n]);
                }
                if (!ok) continue;
                for (n = 2; n <= 4; n++) {
                    const double len = sqrt(m[n]), dist = sqrt(dd[n]);
                    if (len > 1e-15) {
                        const float *u = r + (n == 2 ? 9 : n == 3 ? 11 : 14);
                        double l2 = 0.0;
                        int w;
                        T_CHECK(fabs(r[n - 2] - len) <= len * 4.0 * REL, "vec%d magnitude %g, exact %g", n, (double)r[n - 2], len);
                        T_CHECK(fabs(r[n + 1] - 1.0 / len) <= 4.0 * REL / len, "vec%d magnitude_inv %g, exact %g", n, (double)r[n + 1], 1.0 / len);
                        for (w = 0; w < n; w++) l2 += (double)u[w] * u[w];
                        T_CHECK(fabs(l2 - 1.0) <= 8.0 * REL, "vec%d normalize_safe: squared length %.9g", n, l2);
                    }
                    if (dist > 1e-15) T_CHECK(fabs(r[n + 4] - dist) <= dist * 4.0 * REL, "vec%d distance %g, exact %g", n, (double)r[n + 4], dist);
                }
            }
        }
        sh4_alloc_reset(S);
    }
}

int main(int argc, char **argv) {
    const int rc = check_open(argc, argv);
    if (rc) return rc;
    {
        const uint32_t aj = ram(sizeof(job_sqrt_t), 4, 0);
        call("_t_sqrt_flags", aj);
        built = ((job_sqrt_t *)host(aj, sizeof(job_sqrt_t)))->count;
        sh4_alloc_reset(S);
    }
    test("_t_sqrt_scalar", SQRT_SCALAR_OUT, 0);
    test("_t_sqrt_vector", SQRT_VECTOR_OUT, 1);
    printf("wrapper: every shz_dce_ root and length == SH4ZAM's bit for bit on the SH-4, %lu values (%s build); sqrtf(+inf) seen %d times, inv_sqrtf(+0) %d times\n",
           values, (built & SQRT_BUILT_FAST) ? "-ffast-math" : "strict", sqrt_inf_seen, inv_zero_seen);
    T_CHECK(sqrt_inf_seen > 0 && inv_zero_seen > 0, "the special inputs were reached");
    return t_done("sqrt_check");
}
