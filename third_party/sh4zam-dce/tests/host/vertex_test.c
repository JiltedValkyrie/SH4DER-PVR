/* SH4ZAM-DCE tests: shz_dce_vertex.h on the host (SH4ZAM's software back end)

   vertex_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   What is checked:
   - every record's seven floats and its colour against the scalar formula of the header, written out here from the record's BYTES
     (the reference reads the 16-bit and 32-bit fields itself, so the union's layout is checked too), bit for bit
   - every 16-bit value of each of the seven fields (q0..q4, n0, n1), each one run through all 65536 values, for three grids
   - batches of every count from 0 up to 69, at every input offset 0, 4, 8, 12 and every output offset 0, 4, ..., 28 from a buffer on a
     multiple of 32 (offset 0 is on a multiple of 32, the others on multiples of 4 that are not), guard bytes before and after the
     output, count 0 touching nothing, the pad field ignored; random grids
   - origins over the whole 32-bit range with shifts 0..15: the sum wraps modulo 2^32 and the reference wraps the same way
   - the software fused multiply-add against an independent reference: the exact a * b + c as a 128-bit integer, rounded once to
     float by integer arithmetic (no floating point in the reference). Random operands, and cancellations that put the exact sum next
     to a float midpoint, where a wrong rounding would show
   - the exact claim behind "bit-identical" for x, y, z: when |origin + (q << shift)| < 2^24 the sum is exact in float, and the one
     multiply rounds once: the result is the correctly rounded exact product
   - u and v: the fused value; with a power-of-two scale it equals the plain expression q * scale + base too (the product is exact);
     the number of other scales where the fused and the plain value differ is printed (the header promises the fused value)
*/

#include <sh4zam_dce/shz_dce_vertex.h>

#include "shz_dce_test.h"

#define NREC_FIELD 65536u

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | ((unsigned)p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

/* the generator's high bits (its low bits have short periods) */
static uint32_t rnd(void) { return t_rand() >> 8; }
static uint32_t rnd_below(uint32_t n) { return rnd() % n; }
static uint16_t rnd16(void) { return (uint16_t)(t_rand() >> 16); }

/* a power of two (or its negative): the mantissa is exactly 0.5 in magnitude */
static int is_pow2(float s) {
    int e;
    return fabsf(frexpf(s, &e)) == 0.5f;
}

/* the sign bit of a float, read from its bits */
static int sign_bit(float x) {
    uint32_t u;
    memcpy(&u, &x, sizeof u);
    return (int)(u >> 31);
}

/* a zero of either sign, tested on its bits */
static int is_zero(float x) {
    uint32_t u;
    memcpy(&u, &x, sizeof u);
    return (u & 0x7fffffffu) == 0u;
}

/* (-1)^neg * m * 2^E, rounded once to float by integer arithmetic (round to nearest, ties to even); m >= 1 */
static float round_to_float(unsigned __int128 m, int neg, int E) {
    unsigned __int128 t = m, q, r, half;
    int bits = 0, shift = 0;
    float f;
    while (t) { bits++; t >>= 1; }
    if (bits <= 24) {
        f = ldexpf((float)(uint32_t)m, E);
    } else {
        shift = bits - 24;
        q = m >> shift;
        r = m - (q << shift);
        half = (unsigned __int128)1 << (shift - 1);
        if (r > half || (r == half && (q & 1u)))
            q++;
        if (q == ((unsigned __int128)1 << 24)) {   /* the rounding carried into a new bit */
            q >>= 1;
            shift++;
        }
        f = ldexpf((float)(uint32_t)q, shift + E);
    }
    return neg ? -f : f;
}

/* the exact a * b + c rounded once to float, by 128-bit integers: each float is an integer times a power of two (its 24-bit
   significand), so the product is an integer and the sum is one integer; returns 0 when the exponents are too far apart to hold the
   sum in 128 bits (the caller counts those) */
static int fused_exact(float a, float b, float c, float *out) {
    int ea, eb, ec, ep, ecs, E, sp, sc, neg;
    float ma, mb, mc;
    __int128 p, sum;
    unsigned __int128 mag;

    if (is_zero(a) || is_zero(b)) {   /* the product is a zero: the sum of two zeros is -0 only when both are -0 (zero tests and signs
                                         read from the bits: float comparisons with 0 are not to be trusted under -ffast-math) */
        const int negp = sign_bit(a) != sign_bit(b);
        uint32_t zbits;
        if (!is_zero(c)) { *out = c; return 1; }
        zbits = (negp && sign_bit(c)) ? 0x80000000u : 0u;
        memcpy(out, &zbits, sizeof zbits);
        return 1;
    }
    ma = frexpf(a, &ea);
    mb = frexpf(b, &eb);
    p = (__int128)(int64_t)ldexpf(ma, 24) * (__int128)(int64_t)ldexpf(mb, 24);   /* a = ia * 2^(ea-24) */
    ep = ea + eb - 48;
    if (is_zero(c)) {
        sum = p;
        E = ep;
    } else {
        mc = frexpf(c, &ec);
        ecs = ec - 24;                                                     /* c = ic * 2^ecs */
        E = ep < ecs ? ep : ecs;
        sp = ep - E;
        sc = ecs - E;
        if (sp > 60 || sc > 60)
            return 0;
        sum = p * ((__int128)1 << sp) + (__int128)(int64_t)ldexpf(mc, 24) * ((__int128)1 << sc);
    }
    if (sum == 0) {
        *out = 0.0f;
        return 1;
    }
    neg = sum < 0;
    mag = (unsigned __int128)(neg ? -sum : sum);
    *out = round_to_float(mag, neg, E);
    return 1;
}

static long n_ref_out;   /* references the integer method could not hold: must stay 0 */

/* the fused value of u or v: exact, rounded once */
static float fused_val(float a, float b, float c) {
    float r = 0.0f;
    if (!fused_exact(a, b, c, &r)) {
        n_ref_out++;
        return 0.0f;
    }
    return r;
}

/* the scalar formula, written out literally, from the record's bytes; u and v: the fused value (the plain one for a power-of-two scale, equal) */
static void ref_record(shz_dce_vertex32_t *o, const uint8_t *r, const shz_dce_vertex20_grid_t *g) {
    const uint32_t q0 = rd16(r), q1 = rd16(r + 2), q2 = rd16(r + 4), q3 = rd16(r + 6), q4 = rd16(r + 8);
    const int32_t n0 = (int16_t)rd16(r + 10), n1 = (int16_t)rd16(r + 12);

    /* the sum is taken modulo 2^32 (the header's rule): the plain sum whenever it fits in 32 bits */
    o->x = (float)(int32_t)((uint32_t)g->origin[0] + (q0 << g->shift)) * g->pos_scale;
    o->y = (float)(int32_t)((uint32_t)g->origin[1] + (q1 << g->shift)) * g->pos_scale;
    o->z = (float)(int32_t)((uint32_t)g->origin[2] + (q2 << g->shift)) * g->pos_scale;
    if (is_pow2(g->u_scale))
        o->u = (float)q3 * g->u_scale + g->u_base;
    else
        o->u = fused_val((float)q3, g->u_scale, g->u_base);
    if (is_pow2(g->v_scale))
        o->v = (float)q4 * g->v_scale + g->v_base;
    else
        o->v = fused_val((float)q4, g->v_scale, g->v_base);
    o->nu = (float)n0 * g->n_scale;
    o->nv = (float)n1 * g->n_scale;
    o->argb = rd32(r + 16);
}

static int same_record(const shz_dce_vertex32_t *a, const shz_dce_vertex32_t *b) {
    return memcmp(a, b, sizeof *a) == 0;
}

static void show(const char *what, const shz_dce_vertex32_t *a, const shz_dce_vertex32_t *b) {
    printf("  %s: got x %.9g y %.9g z %.9g u %.9g v %.9g nu %.9g nv %.9g argb %08x\n    want x %.9g y %.9g z %.9g u %.9g v %.9g nu %.9g nv %.9g argb %08x\n",
           what, (double)a->x, (double)a->y, (double)a->z, (double)a->u, (double)a->v, (double)a->nu, (double)a->nv, (unsigned)a->argb,
           (double)b->x, (double)b->y, (double)b->z, (double)b->u, (double)b->v, (double)b->nu, (double)b->nv, (unsigned)b->argb);
}

/* a scale 2^e times a mantissa: the mantissa is 1 (a power of two) half of the time, else random in [1, 2); e in [min_exp, max_exp] */
static float rnd_scale(int max_exp, int min_exp) {
    const int e = (int)rnd_below((uint32_t)(max_exp - min_exp + 1)) + min_exp;
    const float m = (rnd() & 1u) ? 1.0f : t_frand(1.0f, 2.0f);
    const float s = ldexpf(m, e);
    return (rnd() & 2u) ? -s : s;
}

/* a grid with random values: origin within +-2^22, shift 0..shift_max */
static void rnd_grid(shz_dce_vertex20_grid_t *g, uint32_t shift_max) {
    g->origin[0] = (int32_t)rnd_below(8388609u) - 4194304;
    g->origin[1] = (int32_t)rnd_below(8388609u) - 4194304;
    g->origin[2] = (int32_t)rnd_below(8388609u) - 4194304;
    g->shift = rnd_below(shift_max + 1u);
    g->pos_scale = rnd_scale(8, -8);
    g->u_scale = rnd_scale(8, -16);
    g->u_base = t_frand(-64.0f, 64.0f);
    g->v_scale = rnd_scale(8, -16);
    g->v_base = t_frand(-64.0f, 64.0f);
    g->n_scale = rnd_scale(-4, -16);
}

/* one packed record from its fields */
static void make_record(uint8_t *r, const uint16_t q[5], const int16_t n[2], uint16_t pad, uint32_t argb) {
    int k;
    for (k = 0; k < 5; k++) wr16(r + 2 * k, q[k]);
    wr16(r + 10, (uint16_t)n[0]);
    wr16(r + 12, (uint16_t)n[1]);
    wr16(r + 14, pad);
    wr32(r + 16, argb);
}

/* the buffers are 32-bit words, so their base is on a multiple of 4 */
static uint32_t in_words[70 * 5 + 16];
static uint32_t out_words[70 * 8 + 32] __attribute__((aligned(32)));   /* on a multiple of 32: offset 0 is a cache line */
static uint32_t big_in_words[NREC_FIELD * 5];
static shz_dce_vertex32_t big_out[NREC_FIELD] __attribute__((aligned(32)));
static shz_dce_vertex32_t want;

static long n_records, n_fma_checked, n_fma_skipped, n_exact_checked, n_pow2_checked, n_fused_differs;

/* A: batches of every count from 0 to 69, at every input offset 0, 4, 8, 12 and output offset 0 to 28 in 4-byte steps, random grids,
   guard bytes around the output */
static void sweep_random(void) {
    int trial = 0;
    uint32_t count, in_off, out_off;
    for (count = 0; count < 70u; count++)
    for (in_off = 0; in_off < 16u; in_off += 4u)
    for (out_off = 0; out_off < 32u; out_off += 4u) {
        uint8_t *ib = (uint8_t *)in_words + in_off, *ob = (uint8_t *)out_words + out_off;
        shz_dce_vertex20_grid_t g;
        uint32_t i, k;
        int ok_guard = 1;

        rnd_grid(&g, 3u);
        for (k = 0; k < count * 20u; k++) ib[k] = (uint8_t)rnd();   /* random bytes: every field, pad included */
        memset(out_words, 0xee, sizeof out_words);
        shz_dce_vertex20_unpack((shz_dce_vertex32_t *)(void *)ob, (const shz_dce_vertex20_t *)(const void *)ib, count, &g);

        for (i = 0; i < count; i++) {
            const shz_dce_vertex32_t *got = (const shz_dce_vertex32_t *)(const void *)(ob + 32u * i);
            ref_record(&want, ib + 20u * i, &g);
            T_CHECK(same_record(got, &want), "trial %d record %u of %u (in offset %u, out offset %u)", trial, (unsigned)i, (unsigned)count,
                    (unsigned)in_off, (unsigned)out_off);
            if (!same_record(got, &want) && t_fails < 20) {
                show("random batch", got, &want);
                printf("    grid: pos_scale %a u_scale %a u_base %a v_scale %a v_base %a n_scale %a shift %u\n",
                       (double)g.pos_scale, (double)g.u_scale, (double)g.u_base, (double)g.v_scale, (double)g.v_base, (double)g.n_scale,
                       (unsigned)g.shift);
            }
            n_records++;
        }
        /* nothing outside the records changed: the bytes before the output, and everything after its last record */
        for (k = 0; k < out_off; k++) ok_guard &= ((uint8_t *)out_words)[k] == 0xee;
        for (k = out_off + 32u * count; k < sizeof out_words; k++) ok_guard &= ((uint8_t *)out_words)[k] == 0xee;
        T_CHECK(ok_guard, "trial %d: the batch wrote outside its records (count %u, out offset %u)", trial, (unsigned)count, (unsigned)out_off);
        trial++;
    }
}

/* B2: origins over the whole 32-bit range, every shift 0..15: the sums wrap modulo 2^32, and the routine gives the wrapped value (the
   reference does the same sum: the header's rule). Batches of 1 to 16 records. */
static void sweep_wrap(void) {
    int trial;
    for (trial = 0; trial < 20000; trial++) {
        const uint32_t count = 1u + rnd_below(16u);
        shz_dce_vertex20_grid_t g;
        uint32_t i, k;
        uint8_t *ib = (uint8_t *)in_words, *ob = (uint8_t *)out_words;

        rnd_grid(&g, 15u);
        g.origin[0] = (int32_t)t_rand();
        g.origin[1] = (int32_t)t_rand();
        g.origin[2] = (int32_t)t_rand();
        for (k = 0; k < count * 20u; k++) ib[k] = (uint8_t)rnd();
        memset(out_words, 0xee, sizeof out_words);
        shz_dce_vertex20_unpack((shz_dce_vertex32_t *)(void *)ob, (const shz_dce_vertex20_t *)(const void *)ib, count, &g);
        for (i = 0; i < count; i++) {
            const shz_dce_vertex32_t *got = (const shz_dce_vertex32_t *)(const void *)(ob + 32u * i);
            ref_record(&want, ib + 20u * i, &g);
            T_CHECK(same_record(got, &want), "wrap trial %d record %u of %u, shift %u, origins %d %d %d", trial, (unsigned)i, (unsigned)count,
                    (unsigned)g.shift, (int)g.origin[0], (int)g.origin[1], (int)g.origin[2]);
            n_records++;
        }
    }
}

/* B: every 16-bit value of each field, with random other fields, for three grids */
static void sweep_fields(void) {
    static const shz_dce_vertex20_grid_t grids[3] = {
        { { 0, 0, 0 }, 0u, 1.0f, 1.0f / 65536.0f, 0.5f, 0.1f, -3.25f, 1.0f / 32767.0f },
        { { -4194287, 4194303, 1234567 }, 3u, 0.37f, 0.0123f, 7.7f, 1.0f / 128.0f, 1.0e-3f, 1.0f / 32768.0f },
        { { 4194304, -4194304, 0 }, 2u, 3.0e3f, -2.0f, -1.5f, 3.0e-2f, 2048.0f, -(1.0f / 16384.0f) }
    };
    unsigned gi, field;
    uint32_t v, k;
    for (gi = 0; gi < 3u; gi++) {
        for (field = 0; field < 7u; field++) {
            for (v = 0; v < NREC_FIELD; v++) {
                uint8_t *r = (uint8_t *)(big_in_words + 5u * v);
                uint16_t q[5];
                int16_t n[2];
                for (k = 0; k < 5u; k++) q[k] = rnd16();
                n[0] = (int16_t)rnd16();
                n[1] = (int16_t)rnd16();
                if (field < 5u) q[field] = (uint16_t)v;
                else n[field - 5u] = (int16_t)(uint16_t)v;
                make_record(r, q, n, rnd16(), t_rand());
            }
            memset(big_out, 0xee, sizeof big_out);
            shz_dce_vertex20_unpack(big_out, (const shz_dce_vertex20_t *)(const void *)big_in_words, NREC_FIELD, &grids[gi]);
            for (v = 0; v < NREC_FIELD; v++) {
                ref_record(&want, (const uint8_t *)(big_in_words + 5u * v), &grids[gi]);
                T_CHECK(same_record(&big_out[v], &want), "field %u value %u grid %u", field, (unsigned)v, gi);
                if (!same_record(&big_out[v], &want) && t_fails < 20) show("field sweep", &big_out[v], &want);
                n_records++;
            }
        }
    }
}

/* C: the software fused multiply-add against the exact integer reference. Random operands over a moderate exponent range, and
   cancellations: c is the negated float product plus or minus a few units in the last place, so the exact sum sits next to a float
   midpoint, where a rounding error would show. */
static void sweep_fma(void) {
    int trial;
    for (trial = 0; trial < 1000000; trial++) {
        float a, b, c, want_f = 0.0f;
        if (trial % 4 == 0) {   /* cancellation */
            float p;
            int k;
            a = t_frand(-4.0f, 4.0f) * ldexpf(1.0f, (int)rnd_below(40u) - 20);
            b = t_frand(-4.0f, 4.0f) * ldexpf(1.0f, (int)rnd_below(40u) - 20);
            p = (float)((double)a * (double)b);
            c = -p;
            for (k = (int)rnd_below(5u); k > 0; k--) c = (rnd() & 1u) ? nextafterf(c, 1e30f) : nextafterf(c, -1e30f);
        } else {
            a = t_frand(-2.0f, 2.0f) * ldexpf(1.0f, (int)rnd_below(41u) - 20);
            b = t_frand(-2.0f, 2.0f) * ldexpf(1.0f, (int)rnd_below(41u) - 20);
            c = t_frand(-2.0f, 2.0f) * ldexpf(1.0f, (int)rnd_below(41u) - 20);
        }
        if (a == 0.0f || b == 0.0f || c == 0.0f || fabsf(a * b) < 1e-30f || fabsf(a * b) > 1e30f) {
            continue;   /* keep the operands in the normal range the header promises */
        }
        if (!fused_exact(a, b, c, &want_f)) {
            n_fma_skipped++;
            continue;
        }
        T_CHECK(t_bits(shz_dce_vertex_fmaf_sw(a, b, c)) == t_bits(want_f), "fma %a * %a + %a: software %a, exact %a", (double)a, (double)b,
                (double)c, (double)shz_dce_vertex_fmaf_sw(a, b, c), (double)want_f);
        T_CHECK(t_bits(shz_dce_vertex_fmaf(a, b, c)) == t_bits(want_f), "shz_dce_vertex_fmaf differs from the exact reference");
        n_fma_checked++;
    }
}

/* D: |origin + (q << shift)| < 2^24: the sum is exact in float, one multiply rounds once. The double product is exact here (a 24-bit
   integer times a 24-bit scale fits in 53 bits), and rounding it to float once is the correctly rounded exact product. */
static void sweep_exact(void) {
    int trial;
    for (trial = 0; trial < 200000; trial++) {
        shz_dce_vertex20_grid_t g;
        uint16_t q[5] = { 0, 0, 0, 0, 0 };
        int16_t n[2] = { 0, 0 };
        uint8_t r[20];
        shz_dce_vertex32_t got;
        int64_t sum;
        float expect;

        rnd_grid(&g, 3u);
        g.origin[0] = (int32_t)rnd_below(33554433u) - 16777216;   /* +-2^24 */
        q[0] = rnd16();
        make_record(r, q, n, 0u, 0u);
        shz_dce_vertex20_unpack(&got, (const shz_dce_vertex20_t *)(const void *)r, 1u, &g);
        sum = (int64_t)g.origin[0] + ((int64_t)q[0] << g.shift);
        if (sum <= -16777216LL || sum >= 16777216LL)
            continue;   /* outside the exact range: not checked here */
        expect = (float)((double)sum * (double)g.pos_scale);
        T_CHECK(t_bits(got.x) == t_bits(expect), "sum %lld: x %.9g, correctly rounded exact product %.9g", (long long)sum, (double)got.x,
                (double)expect);
        n_exact_checked++;
    }
}

/* E: u and v: the fused value, and the plain value for a power-of-two scale (equal); other scales: how often plain and fused differ */
static void sweep_fused(void) {
    int trial;
    for (trial = 0; trial < 20000; trial++) {
        shz_dce_vertex20_grid_t g;
        uint16_t q[5];
        int16_t n[2] = { 0, 0 };
        uint8_t r[20];
        shz_dce_vertex32_t got;
        float plain_u, plain_v;
        int k;

        rnd_grid(&g, 3u);
        for (k = 0; k < 5; k++) q[k] = rnd16();
        make_record(r, q, n, 0u, 0u);
        shz_dce_vertex20_unpack(&got, (const shz_dce_vertex20_t *)(const void *)r, 1u, &g);
        T_CHECK(t_bits(got.u) == t_bits(is_pow2(g.u_scale) ? (float)q[3] * g.u_scale + g.u_base : fused_val((float)q[3], g.u_scale, g.u_base)),
                "u is not the reference value");
        T_CHECK(t_bits(got.v) == t_bits(is_pow2(g.v_scale) ? (float)q[4] * g.v_scale + g.v_base : fused_val((float)q[4], g.v_scale, g.v_base)),
                "v is not the reference value");
        plain_u = (float)q[3] * g.u_scale + g.u_base;
        plain_v = (float)q[4] * g.v_scale + g.v_base;
        if (is_pow2(g.u_scale)) {
            T_CHECK(t_bits(got.u) == t_bits(plain_u), "power-of-two u scale: u differs from the plain expression");
            n_pow2_checked++;
        } else if (t_bits(got.u) != t_bits(plain_u)) {
            n_fused_differs++;
        }
        if (is_pow2(g.v_scale)) {
            T_CHECK(t_bits(got.v) == t_bits(plain_v), "power-of-two v scale: v differs from the plain expression");
            n_pow2_checked++;
        } else if (t_bits(got.v) != t_bits(plain_v)) {
            n_fused_differs++;
        }
    }
}

/* F: directed cases: extreme values, the shifts 0..15 with origins that keep every sum within 32 bits, zeros of either sign */
static void sweep_directed(void) {
    static const uint16_t vals[5] = { 0u, 1u, 0x7fffu, 0x8000u, 0xffffu };
    shz_dce_vertex20_grid_t g;
    int shift, origin_case, a, b, c, d, e;

    for (shift = 0; shift <= 15; shift++) {
        for (origin_case = 0; origin_case < 6; origin_case++) {
            /* origins 0, +32767 and -32768 keep every sum inside 32 bits (0xffff << 15 plus 32767 is 2^31 - 1 at most); cases 4 and 5
               (+-2^22) wrap at the larger shifts, and the reference's sum wraps the same way */
            g.origin[0] = origin_case == 1 ? 32767 : origin_case == 2 ? -32768 : origin_case == 4 ? 4194303 : origin_case == 5 ? -4194304 : 0;
            g.origin[1] = g.origin[0];
            g.origin[2] = 0;
            g.shift = (uint32_t)shift;
            g.pos_scale = origin_case == 3 ? -0.5f : 0.25f;
            g.u_scale = -1.0f;  g.u_base = -0.0f;
            g.v_scale = 0.0f;   g.v_base = -0.0f;
            g.n_scale = -1.0f;
            for (a = 0; a < 5; a++)
            for (b = 0; b < 5; b++)
            for (c = 0; c < 5; c++)
            for (d = 0; d < 5; d++)
            for (e = 0; e < 5; e++) {
                uint16_t q[5];
                int16_t n[2];
                uint8_t r[20];
                shz_dce_vertex32_t got;
                q[0] = vals[a]; q[1] = vals[b]; q[2] = vals[c]; q[3] = vals[d]; q[4] = vals[e];
                n[0] = (int16_t)vals[e]; n[1] = (int16_t)vals[d];
                make_record(r, q, n, 0xabcdu, 0x12345678u);
                shz_dce_vertex20_unpack(&got, (const shz_dce_vertex20_t *)(const void *)r, 1u, &g);
                ref_record(&want, r, &g);
                T_CHECK(same_record(&got, &want), "directed: shift %d origin case %d q %x %x %x %x %x", shift, origin_case, q[0], q[1], q[2], q[3], q[4]);
                if (!same_record(&got, &want) && t_fails < 20) show("directed", &got, &want);
                n_records++;
            }
        }
    }
    /* zero fields with negative scales: -0.0 in the position and the normal (signed zeros are checked bit for bit in the normal build) */
    {
        const uint16_t q[5] = { 0, 0, 0, 0, 0 };
        const int16_t n[2] = { 0, 0 };
        uint8_t r[20];
        shz_dce_vertex32_t got;
        make_record(r, q, n, 0u, 0u);
        g.origin[0] = 0; g.origin[1] = 0; g.origin[2] = 0; g.shift = 0;
        g.pos_scale = -1.0f; g.u_scale = -2.0f; g.u_base = 0.0f; g.v_scale = 0.5f; g.v_base = -0.0f; g.n_scale = -1.0f;
        shz_dce_vertex20_unpack(&got, (const shz_dce_vertex20_t *)(const void *)r, 1u, &g);
        T_CHECK(t_bits(got.x) == 0x80000000u && t_bits(got.nu) == 0x80000000u, "zero times a negative scale must give -0.0");
        T_CHECK(got.u == 0.0f && got.v == 0.0f, "zero records in u, v");
        n_records++;
    }
}

int main(void) {
    /* count 0 writes nothing (its input is not even given); count 1 gives its one record and nothing after it */
    {
        shz_dce_vertex20_grid_t g;
        shz_dce_vertex32_t out1[2];
        const uint8_t *b = (const uint8_t *)out1;
        uint8_t r[20];
        const uint16_t q[5] = { 1, 2, 3, 4, 5 };
        const int16_t n[2] = { -7, 9 };
        int k, untouched = 1;

        memset(out1, 0xee, sizeof out1);
        g.origin[0] = 10; g.origin[1] = -10; g.origin[2] = 0; g.shift = 2; g.pos_scale = 0.5f;
        g.u_scale = 0.25f; g.u_base = 1.0f; g.v_scale = -0.75f; g.v_base = 2.0f; g.n_scale = 0.125f;
        shz_dce_vertex20_unpack(out1, NULL, 0u, &g);
        for (k = 0; k < (int)sizeof out1; k++) untouched &= b[k] == 0xee;
        T_CHECK(untouched, "count 0 must write nothing");
        make_record(r, q, n, 0xffffu, 0xff00ff00u);
        shz_dce_vertex20_unpack(out1, (const shz_dce_vertex20_t *)(const void *)r, 1u, &g);
        ref_record(&want, r, &g);
        T_CHECK(same_record(&out1[0], &want), "count 1 differs from the formula");
        untouched = 1;
        for (k = 32; k < (int)sizeof out1; k++) untouched &= b[k] == 0xee;
        T_CHECK(untouched, "count 1 wrote past its record");
        n_records++;
    }
    T_CHECK(((uintptr_t)out_words & 31u) == 0u, "the output buffer is not on a multiple of 32");
    T_CHECK(((uintptr_t)big_out & 31u) == 0u, "the big output buffer is not on a multiple of 32");
    sweep_random();
    sweep_wrap();
    sweep_fields();
    sweep_fma();
    sweep_exact();
    sweep_fused();
    sweep_directed();
    T_CHECK(n_ref_out == 0, "%ld references were outside the integer reference's range", n_ref_out);
    printf("vertex_test: %ld records checked bit for bit; %ld software fma checked against the exact integer reference (%ld outside its range, skipped); "
           "%ld exact-range sums; %ld power-of-two u/v checks; %ld non-power-of-two u/v results differ from the plain q * scale + base (the fused value is the promised one)\n",
           n_records, n_fma_checked, n_fma_skipped, n_exact_checked, n_pow2_checked, n_fused_differs);
    return t_done("vertex_test");
}
