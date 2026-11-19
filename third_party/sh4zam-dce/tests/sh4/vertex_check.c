/* SH4ZAM-DCE tests: shz_dce_vertex.h as the SH-4 runs it

   vertex_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   vertex_check <vertex_kernels.elf>
   The ELF is vertex_kernels.c built by sh-elf-gcc; tests/sh4run executes it instruction by instruction. The expected bytes are the host's
   (the same header, compiled for the host, which is exact for these operands: see vertex_test.c). For each batch:
   - the dispatching routine, the plain C form and the SH-4 form each write the host's bytes, every record, bit for bit
   - the output is placed on each alignment class the routine's comment allows: a multiple of 32 (the cache-line path), a multiple of 8
     that is not 32, a multiple of 4 that is not 8; the input and the grid on a multiple of 4 that is not 8
   - nothing outside the records is written (guard bytes around the output); a count of 0 writes nothing; XMTRX is unchanged
     (the routines do not document a change to it)
   - every call twice, with other garbage in the scratch registers each time: the second run gives the same bytes (no scratch register
     is read before it is written); the bytes past the input differ between the two runs (nothing reads past the records)
   - the interpreter stops a run on an unaligned load or store, a callee-saved register or FPSCR mode bit not put back, a cache line
     allocated and not filled: so a pass here is also the "alignment: checked" of every instruction the routine executed
   Also: every count from 0 to 69, each on one alignment class; origins over the whole 32-bit range (the sum wraps modulo 2^32); zero
   grids with the scales' signs varied (signed zeros); and every 16-bit value of each of the seven fields in one batch of 65536 records,
   each field on the three output classes. The host side is built with the FMA instruction (-mfma): its fused multiply-add is checked
   against the software one (vertex_test.c checks that one against an exact integer reference).
*/

#include <sh4zam_dce/shz_dce_vertex.h>

#include "vertex_jobs.h"
#include "sh4_check.h"

#define MAXREC 70u

static shz_dce_vertex32_t expect[MAXREC];
static shz_dce_vertex32_t expect_big[65536];

static uint32_t rnd(void) { return t_rand() >> 8; }
static uint32_t rnd_below(uint32_t n) { return rnd() % n; }

/* a scale 2^e times a mantissa: a power of two half of the time */
static float rnd_scale(int max_exp, int min_exp) {
    const int e = (int)rnd_below((uint32_t)(max_exp - min_exp + 1)) + min_exp;
    const float m = (rnd() & 1u) ? 1.0f : t_frand(1.0f, 2.0f);
    const float s = ldexpf(m, e);
    return (rnd() & 2u) ? -s : s;
}

/* a grid: origin within +-2^22, shift 0..7 (the sum stays in 32 bits), scales with and without a power of two */
static void rnd_grid(shz_dce_vertex20_grid_t *g) {
    g->origin[0] = (int32_t)rnd_below(8388609u) - 4194304;
    g->origin[1] = (int32_t)rnd_below(8388609u) - 4194304;
    g->origin[2] = (int32_t)rnd_below(8388609u) - 4194304;
    g->shift = rnd_below(8u);
    g->pos_scale = rnd_scale(8, -8);
    g->u_scale = rnd_scale(8, -16);
    g->u_base = t_frand(-64.0f, 64.0f);
    g->v_scale = rnd_scale(8, -16);
    g->v_base = t_frand(-64.0f, 64.0f);
    g->n_scale = rnd_scale(-4, -16);
}

/* the output buffer of one alignment class: 'bytes' of records at class offset 'off' within a block; returns the record address */
static uint32_t out_block(uint32_t bytes, uint32_t cls, uint32_t *block, uint32_t *off) {
    const uint32_t guard = 64u;
    uint32_t a;
    switch (cls) {
    case 0:  *off = 0u;  a = ram(bytes + guard, 32, 0); break;   /* on a multiple of 32 */
    case 1:  *off = 8u;  a = ram(bytes + guard + 8u, 32, 8); break;   /* on a multiple of 8, not of 32 */
    default: *off = 4u;  a = ram(bytes + guard + 4u, 8, 4); break;   /* on a multiple of 4, not of 8 */
    }
    *block = a - *off;
    return a;
}

static void check_batch(uint32_t count, const shz_dce_vertex20_grid_t *g, const uint8_t *recs, uint32_t cls, const char *tag) {
    const uint32_t ob = 32u * count, in_bytes = 20u * count + 64u;   /* the tail past the records: garbage the routine must not read */
    uint32_t aj, ain, agrid, ao_disp, ao_c, ao_sh4, blk_disp, blk_c, blk_sh4, off_disp, off_c, off_sh4, k, run;
    job_vertex_t *j;
    uint8_t *hin, *hd, *hc, *hs;
    int pass;
    float xm_before[16], xm_after[16];

    aj = ram(sizeof(job_vertex_t), 4, 0);
    ain = ram(in_bytes, 8, 4);                     /* on a multiple of 4, not of 8: the weakest the records allow */
    agrid = ram(sizeof(shz_dce_vertex20_grid_t), 8, 4);
    ao_disp = out_block(ob, cls, &blk_disp, &off_disp);
    ao_c = out_block(ob, cls, &blk_c, &off_c);
    ao_sh4 = out_block(ob, cls, &blk_sh4, &off_sh4);

    hin = (uint8_t *)host(ain, in_bytes);
    memcpy(hin, recs, 20u * count);
    memcpy(host(agrid, sizeof *g), g, sizeof *g);
    /* the host's expected bytes, from the same header */
    shz_dce_vertex20_unpack(expect, (const shz_dce_vertex20_t *)(const void *)hin, count, g);

    for (run = 0; run < 2; run++) {
        hd = (uint8_t *)host(blk_disp, ob + 64u + off_disp);
        hc = (uint8_t *)host(blk_c, ob + 64u + off_c);
        hs = (uint8_t *)host(blk_sh4, ob + 64u + off_sh4);
        memset(hd, 0xee, ob + 64u + off_disp);
        memset(hc, 0xee, ob + 64u + off_c);
        memset(hs, 0xee, ob + 64u + off_sh4);
        memset(hin + 20u * count, run ? 0xc3 : 0x5a, in_bytes - 20u * count);   /* the tail differs between the two runs */
        j = (job_vertex_t *)host(aj, sizeof *j);
        j->in = ain; j->grid = agrid; j->out_disp = ao_disp; j->out_c = ao_c; j->out_sh4 = ao_sh4; j->count = count;
        sh4_seed(S, 0x3579bdu + 0x10001u * run);   /* other garbage in the scratch registers on each run */
        sh4_xmtrx_get(S, xm_before);
        call("_t_vertex", aj);
        sh4_xmtrx_get(S, xm_after);
        T_CHECK(!memcmp(xm_before, xm_after, sizeof xm_before), "%s: XMTRX changed by a batch that does not document it", tag);
        pass = 1;
        pass &= memcmp(host(ao_disp, ob), expect, ob) == 0;
        pass &= memcmp(host(ao_c, ob), expect, ob) == 0;
        pass &= memcmp(host(ao_sh4, ob), expect, ob) == 0;
        T_CHECK(pass, "%s: count %u, out class %u, run %u: the SH-4 bytes differ from the host's", tag, (unsigned)count, (unsigned)cls, (unsigned)run);
        /* the guard bytes around each output are untouched */
        for (k = 0; k < off_disp; k++) T_CHECK(hd[k] == 0xee, "%s: the dispatching routine wrote before its output", tag);
        for (k = off_disp + ob; k < ob + 64u + off_disp; k++) T_CHECK(hd[k] == 0xee, "%s: the dispatching routine wrote past its output", tag);
        for (k = off_c + ob; k < ob + 64u + off_c; k++) T_CHECK(hc[k] == 0xee, "%s: the C form wrote past its output", tag);
        for (k = off_sh4 + ob; k < ob + 64u + off_sh4; k++) T_CHECK(hs[k] == 0xee, "%s: the SH-4 form wrote past its output", tag);
    }
    sh4_alloc_reset(S);
}

static void test_batches(void) {
    int trial;
    for (trial = 0; trial < 500; trial++) {
        const uint32_t count = trial % 5 == 0 ? 33u + rnd_below(34u) : rnd_below(41u);
        const uint32_t cls = (uint32_t)(trial % 3);
        shz_dce_vertex20_grid_t g;
        uint8_t recs[MAXREC * 20u];
        uint32_t k;
        rnd_grid(&g);
        for (k = 0; k < count * 20u; k++) recs[k] = (uint8_t)rnd();   /* random bytes: every field, the pad too */
        check_batch(count, &g, recs, cls, "random");
    }
    /* one batch of the grid's shifts 0..15 with origins that keep the sums in 32 bits */
    {
        uint8_t recs[MAXREC * 20u];
        shz_dce_vertex20_grid_t g;
        uint32_t k, s;
        for (s = 0; s < 16u; s++) {
            rnd_grid(&g);
            g.shift = s;
            g.origin[0] = g.origin[1] = g.origin[2] = (int32_t)rnd_below(65536u) - 32768;
            for (k = 0; k < 40u * 20u; k++) recs[k] = (uint8_t)rnd();
            check_batch(40u, &g, recs, s % 3u, "shifts");
        }
    }
}

/* the guards of one output: the bytes before it and the 64 after it are still 0xee */
static int guards_hold(const uint8_t *block, uint32_t off, uint32_t ob) {
    uint32_t k;
    int ok = 1;
    for (k = 0; k < off; k++) ok &= block[k] == 0xee;
    for (k = off + ob; k < off + ob + 64u; k++) ok &= block[k] == 0xee;
    return ok;
}

/* every count from 0 to MAXREC - 1, each on one alignment class (count mod 3): the counts where the loops and the tail meet */
static void test_counts(void) {
    uint32_t count, k;
    for (count = 0; count < MAXREC; count++) {
        shz_dce_vertex20_grid_t g;
        uint8_t recs[MAXREC * 20u];
        rnd_grid(&g);
        for (k = 0; k < count * 20u; k++) recs[k] = (uint8_t)rnd();
        check_batch(count, &g, recs, count % 3u, "counts");
    }
}

/* origins over the whole 32-bit range, every shift 0..15: the sums wrap modulo 2^32, on the SH-4 as on the host */
static void test_wrap(void) {
    uint32_t s, k;
    for (s = 0; s < 16u; s++) {
        shz_dce_vertex20_grid_t g;
        uint8_t recs[40u * 20u];
        rnd_grid(&g);
        g.shift = s;
        g.origin[0] = (int32_t)t_rand();
        g.origin[1] = (int32_t)t_rand();
        g.origin[2] = (int32_t)t_rand();
        for (k = 0; k < 40u * 20u; k++) recs[k] = (uint8_t)rnd();
        check_batch(40u, &g, recs, s % 3u, "wrap");
    }
}

/* zero origins and zero fields, the scales' signs varied: x, y, z and nu are +0 or -0 by the sign of their scale, bit for bit */
static void test_zeros(void) {
    static const shz_dce_vertex20_grid_t zs[3] = {
        { { 0, 0, 0 }, 0u, -1.0f, -2.0f, 0.0f, 0.5f, -0.0f, -1.0f },
        { { 0, 0, 0 }, 3u, 1.0f, -1.0f, -0.0f, -0.0f, 0.0f, 1.0f },
        { { 0, 0, 0 }, 15u, -0.0f, -2.0f, -2.0f, -1.0f, -0.0f, -0.0f }
    };
    uint8_t recs[4u * 20u];
    uint32_t gi;
    memset(recs, 0, sizeof recs);
    for (gi = 0; gi < 3u; gi++)
        check_batch(4u, &zs[gi], recs, gi, "zeros");
}

/* the fused multiply-add of this build (the FMA instruction, since the build has -mfma) against the software one, which vertex_test.c
   checks against an exact integer reference: the two must agree bit for bit, cancellations included */
static void test_fma_paths(void) {
    int i;
    for (i = 0; i < 200000; i++) {
        const float a = t_frand(-4.0f, 4.0f) * ldexpf(1.0f, (int)rnd_below(40u) - 20);
        const float b = t_frand(-4.0f, 4.0f) * ldexpf(1.0f, (int)rnd_below(40u) - 20);
        float c = t_frand(-4.0f, 4.0f) * ldexpf(1.0f, (int)rnd_below(40u) - 20);
        if (i & 1) c = -(float)((double)a * (double)b);   /* a cancellation: the exact sum is a few units in the last place */
        if (i & 2) c = nextafterf(c, (i & 4) ? 1e30f : -1e30f);
        if (a == 0.0f || b == 0.0f || c == 0.0f || fabsf(a * b) < 1e-30f || fabsf(a * b) > 1e30f) continue;
        T_CHECK(t_bits(shz_dce_vertex_fmaf(a, b, c)) == t_bits(shz_dce_vertex_fmaf_sw(a, b, c)),
                "fma %a * %a + %a: instruction %a, software %a", (double)a, (double)b, (double)c,
                (double)shz_dce_vertex_fmaf(a, b, c), (double)shz_dce_vertex_fmaf_sw(a, b, c));
    }
}

/* every 16-bit value of each of the seven fields (q0..q4, n0, n1) in one batch of 65536 records, the other fields random, each field's
   three outputs on three different alignment classes: the dispatching routine on class k mod 3, the plain form on k+1, the SH-4 form on k+2 */
static void test_sweeps(void) {
    static const uint32_t big = 65536u;
    uint32_t field, v, k;
    for (field = 0; field < 7u; field++) {
        shz_dce_vertex20_grid_t g;
        const uint32_t in_bytes = big * 20u + 64u, ob = big * 32u;
        const uint32_t ain = ram(in_bytes, 8, 4), agrid = ram(sizeof g, 8, 4);
        uint32_t ao_d, ao_c, ao_s, blk_d, blk_c, blk_s, off_d, off_c, off_s, aj;
        uint8_t *hin;
        job_vertex_t *j;
        float xm_before[16], xm_after[16];
        rnd_grid(&g);
        g.shift = 2u;
        hin = (uint8_t *)host(ain, in_bytes);
        for (v = 0; v < big; v++) {
            uint8_t *r = hin + 20u * v;
            for (k = 0; k < 20u; k++) r[k] = (uint8_t)rnd();
            r[2u * field] = (uint8_t)v;   /* the field: q0 at byte 0, q1 at 2, ... q4 at 8, n0 at 10, n1 at 12 */
            r[2u * field + 1u] = (uint8_t)(v >> 8);
        }
        memset(hin + 20u * big, 0x5a, in_bytes - 20u * big);   /* the tail, past the records */
        memcpy(host(agrid, sizeof g), &g, sizeof g);
        shz_dce_vertex20_unpack(expect_big, (const shz_dce_vertex20_t *)(const void *)hin, big, &g);
        /* the three outputs, each its own block, on three different alignment classes (k, k+1, k+2 mod 3) */
        ao_d = out_block(ob, field % 3u, &blk_d, &off_d);
        ao_c = out_block(ob, (field + 1u) % 3u, &blk_c, &off_c);
        ao_s = out_block(ob, (field + 2u) % 3u, &blk_s, &off_s);
        memset(host(blk_d, off_d + ob + 64u), 0xee, off_d + ob + 64u);
        memset(host(blk_c, off_c + ob + 64u), 0xee, off_c + ob + 64u);
        memset(host(blk_s, off_s + ob + 64u), 0xee, off_s + ob + 64u);
        aj = ram(sizeof(job_vertex_t), 4, 0);
        j = (job_vertex_t *)host(aj, sizeof *j);
        j->in = ain; j->grid = agrid; j->out_disp = ao_d; j->out_c = ao_c; j->out_sh4 = ao_s; j->count = big;
        sh4_seed(S, 0x2468aceu);
        sh4_xmtrx_get(S, xm_before);
        call("_t_vertex", aj);
        sh4_xmtrx_get(S, xm_after);
        T_CHECK(!memcmp(xm_before, xm_after, sizeof xm_before), "sweep %u: XMTRX changed", (unsigned)field);
        T_CHECK(guards_hold((const uint8_t *)host(blk_d, off_d + ob + 64u), off_d, ob), "sweep %u: the dispatching routine left its output", (unsigned)field);
        T_CHECK(guards_hold((const uint8_t *)host(blk_c, off_c + ob + 64u), off_c, ob), "sweep %u: the C form left its output", (unsigned)field);
        T_CHECK(guards_hold((const uint8_t *)host(blk_s, off_s + ob + 64u), off_s, ob), "sweep %u: the SH-4 form left its output", (unsigned)field);
        T_CHECK(memcmp(host(ao_d, ob), expect_big, ob) == 0, "sweep %u: the dispatching routine differs from the host", (unsigned)field);
        T_CHECK(memcmp(host(ao_c, ob), expect_big, ob) == 0, "sweep %u: the C form differs from the host", (unsigned)field);
        T_CHECK(memcmp(host(ao_s, ob), expect_big, ob) == 0, "sweep %u: the SH-4 form differs from the host", (unsigned)field);
        sh4_alloc_reset(S);
    }
}

int main(int argc, char **argv) {
    const int rc = check_open(argc, argv);
    if (rc) return rc;
    test_fma_paths();
    test_batches();
    test_counts();
    test_wrap();
    test_zeros();
    test_sweeps();
    return t_done("vertex_check");
}
