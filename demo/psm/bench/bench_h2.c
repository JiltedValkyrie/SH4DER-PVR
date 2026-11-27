/* SH4DER-PVR: the chip bench, image h2: SH4ZAM beside KallistiOS, video memory read back, the G2 bus

   demo/psm/bench/bench_h2.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.
   Dreamcast maths and copies under test: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT), used as a library.

   A test program for a real console, after bench_h1.c. Many small tests, one number a line, each in its own stage so
   a hang names the test it was in (the fault catcher, include/sh4der_fault.h, shows it). Every line goes to the serial line
   ("h2:") as soon as it exists and to result pages (A next, B back). The tests run in four groups; the pages are
   shown after each, and START goes on to the next, so what has been measured can be photographed before anything
   less certain runs.

     group 1   1 rounding: one row vector x a 4 x 4 as the plain sum, as FIPR (shz_mat4x4_transform_vec4) and as
                 FTRV (shz_xmtrx_transform_vec4), each against the exact sum
               2 the transform: mat_load / mat_transform beside SH4ZAM's, and shz_dce_pvr_project
               3 copies RAM to RAM: memcpy and sq_cpy beside SH4ZAM's
               4 vertices into the TA: pvr_prim, pvr_sq_load, shz_sq_memcpy32
               5 texture uploads: pvr_txr_load beside shz_sq_memcpy32
               6 video memory to RAM: the SH-4's own reads of the uncached window, and PVR-DMA ended by its interrupt
               7 the warp's mesh and pixel pass sent a vertex at a time and a strip at a time (src/sh4der_warp.c,
                 compiled twice)
               8 the DMAC copying RAM, timed from before its start
     group 2   video memory read through the operand cache (the cached address of the same memory): not known to
               work on this bus, so it has its own group
     group 3   G2 DMA, channel 0: RAM to sound RAM and back, with a loop on the SH-4 beside it
     group 4   one G2 DMA from sound RAM to texture memory: KallistiOS's note on the protection register says the
               bus refuses it, Sega's access table lists it; one attempt, with a limit

   Where a test has a source and a destination in RAM their distance matters: the operand cache is 16 KB, one way,
   so two addresses a multiple of 16 KB apart share one cache entry. h1's buffers were 1 MB apart. Here every such
   test says which placing it ran with, and tests 2 and 3 run both.

   Only RAM, video memory, sound RAM and PVR / SH-4 / G2 registers are written; nothing outlives a power cycle. No
   flash, no GD-ROM, no AICA clock. No access to the sound chip while a G2 DMA runs. Every wait has a limit. Every
   number comes from the run that prints it: one run, one console; only that is evidence.
   Build: python demo/psm/bench/build.py --src bench_h2.c --tag h2.
*/

#include <kos.h>
#include <arch/dmac.h>
#include <dc/asic.h>
#include <dc/cache.h>
#include <dc/g2bus.h>
#include <dc/spu.h>
#include <errno.h>
#include <stdarg.h>

#include "sh4zam/shz_sh4zam.h"
#include "sh4zam_dce/shz_dce.h"
#include "sh4der_warp.h"

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

_Static_assert(sizeof(double) == 8, "the exact sums of test 1 need a real double (-m4-single, not -m4-single-only)");

/* ---- the result pages ----------------------------------------------------------- */

#define COLS 50u
#define ROWS 17u
#define LINES_MAX 420u
static char lines[LINES_MAX][COLS + 1u];
static unsigned nlines;
static sh4der_fault_crumbs_t crumbs;

/* A line for the pages and the serial line. A line over the page's width goes on, indented, on the next. */
static void say(const char *fmt, ...) {
    char big[240];
    const char *s = big;
    size_t left;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(big, sizeof big, fmt, ap);
    va_end(ap);
    printf("h2: %s\n", big);
    for (left = strlen(big); nlines < LINES_MAX; ) {
        const int cont = s != big;
        const size_t room = cont ? COLS - 3u : COLS, n = left < room ? left : room;

        snprintf(lines[nlines++], COLS + 1u, "%s%.*s", cont ? "   " : "", (int)n, s);
        s += n; left -= n;
        if (!left) break;
    }
}

static void page_draw(unsigned page, unsigned pages, const char *next) {
    char head[96];
    unsigned i;

    vid_clear(0, 0, 0);
    snprintf(head, sizeof head, "CHIP BENCH h2 p%u/%u  A/B page  %s", page + 1u, pages, next);
    bfont_draw_str(vram_s + 16u * 640u + 20u, 640, true, head);
    for (i = 0; i < ROWS && page * ROWS + i < nlines; i++)
        bfont_draw_str(vram_s + (16u + 24u * (i + 2u)) * 640u + 20u, 640, true, lines[page * ROWS + i]);
}

static void step(const char *name) {
    sh4der_fault_beat(&crumbs);
    sh4der_fault_stage(&crumbs, name);
    crumbs.frame++;
}

/* ---- clocks and memory ---------------------------------------------------------- */

#define MB (1024u * 1024u)
#define OC 16384u                          /* the operand cache: two addresses this far apart share an entry */
#define REG32(a) (*(volatile uint32_t *)(a))
#define LIMIT_NS 200000000ull              /* no wait in groups 1 and 2 is longer */

/* One block, so the distances are known. At equal offsets BUF_B is half a cache from BUF_A (no shared entry);
   BUF_B_ALIAS is exactly 1 MB from it (every entry shared: h1's placing). The two B's overlap: one is used at a time. */
static uint8_t pool[3u * MB + 2u * OC] __attribute__((aligned(32)));
#define BUF_A (pool)
#define BUF_B (pool + MB + OC / 2u)
#define BUF_B_ALIAS (pool + MB)
#define BUF_C (pool + 2u * MB + OC)

static inline uint64_t ns(void) { return timer_ns_gettime64(); }
static inline uint64_t cyc(void) { return perf_cntr_count(PRFC1); }
static unsigned us_of(uint64_t t) { return (unsigned)((t + 500u) / 1000u); }
static unsigned mbs10(uint64_t bytes, uint64_t t) { return t ? (unsigned)(bytes * 10000u / t) : 0u; }   /* 10 x (million bytes a second) */
static unsigned x100(uint64_t num, uint64_t den) { return den ? (unsigned)(num * 100u / den) : 0u; }
#define F2(v) (v) / 100u, (v) % 100u       /* a value x 100 for "%u.%02u" */
#define F1(v) (v) / 10u, (v) % 10u         /* a value x 10 for "%u.%u" */

static void noise(uint8_t *p, unsigned n, uint32_t seed) {
    unsigned i;

    for (i = 0; i < n; i += 4u) { seed = seed * 1664525u + 1013904223u; memcpy(p + i, &seed, 4u); }
}

static volatile uint32_t sink;
typedef int (*busy_fn)(void);

static void wait_ns(uint64_t d) {
    const uint64_t t0 = ns();

    while (ns() - t0 < d) { }
}

/* Count register-only work on the SH-4 while `busy` says so (no busy: for `limit` ns), never past `limit`. Returns
   the steps a millisecond; *took the time. */
static unsigned spin(busy_fn busy, uint64_t limit, uint64_t *took) {
    const uint64_t t0 = ns();
    uint64_t t, n = 0;
    uint32_t x = 1u;
    unsigned i;

    do {
        for (i = 0; i < 256u; i++) x = x * 1664525u + 1013904223u;
        n += 256u;
        t = ns() - t0;
    } while (t < limit && (!busy || busy()));
    sink += x;
    if (took) *took = t;
    return t ? (unsigned)(n * 1000000u / t) : 0u;
}

static unsigned alone;                     /* the loop's rate with nothing beside it */

static void t_timer(void) {
    uint64_t a = ns(), b, least = ~0ull, c0, t0;
    unsigned i;

    for (i = 0; i < 20000u; i++) { b = ns(); if (b != a && b - a < least) least = b - a; a = b; }
    perf_cntr_start(PRFC1, PMCR_ELAPSED_TIME_MODE, PMCR_COUNT_CPU_CYCLES);
    c0 = cyc(); t0 = ns();
    while (ns() - t0 < 1000000u) { }
    say("timer: least step %u ns; 1 ms = %u cycles", (unsigned)least, (unsigned)(cyc() - c0));
    alone = spin(NULL, 10000000u, NULL);
    say("SH-4 register loop alone: %u /ms", alone);
}

/* ---- 1. rounding: (x, y, z, w) x m three ways -------------------------------------------------- */

#define RN 4096u
typedef float quad_t[4];

/* A world-to-map matrix's kind of numbers, in row form: turns under 1, an offset of tens, a small w column. */
static float rmat[16] __attribute__((aligned(32))) = {
    0.8112f, -0.2931f, 0.0123f, 0.0f,
    0.1904f, 0.3518f, -0.0277f, 0.0041f,
    -0.5527f, -0.3090f, -0.0098f, -0.0122f,
    31.25f, -17.5f, 0.642f, 1.0f };
static quad_t rpts[RN] __attribute__((aligned(32))), rout[3][RN] __attribute__((aligned(32)));

/* The sum as include/sh4der_psm.h's plain path writes it (what the header was on the console before FIPR). */
static void __attribute__((noinline)) row_plain(const float m[16], const quad_t *p, quad_t *o, unsigned n) {
    unsigned i, c;

    for (i = 0; i < n; i++) {
        const float x = p[i][0], y = p[i][1], z = p[i][2], w = p[i][3];

        for (c = 0; c < 4u; c++) o[i][c] = x * m[c] + y * m[4u + c] + z * m[8u + c] + w * m[12u + c];
    }
}

/* The header's SH-4 path: four FIPR, XMTRX not touched. */
static void __attribute__((noinline)) row_fipr(const float m[16], const quad_t *p, quad_t *o, unsigned n) {
    unsigned i;

    for (i = 0; i < n; i++) {
        const shz_vec4_t v = shz_mat4x4_transform_vec4((const shz_mat4x4_t *)(const void *)m, shz_vec4_init(p[i][0], p[i][1], p[i][2], p[i][3]));

        o[i][0] = v.x; o[i][1] = v.y; o[i][2] = v.z; o[i][3] = v.w;
    }
}

/* One FTRV a point, the matrix loaded into XMTRX once. */
static void __attribute__((noinline)) row_ftrv(const float m[16], const quad_t *p, quad_t *o, unsigned n) {
    unsigned i;

    shz_xmtrx_load_unaligned_4x4(m);
    for (i = 0; i < n; i++) {
        const shz_vec4_t v = shz_xmtrx_transform_vec4(shz_vec4_init(p[i][0], p[i][1], p[i][2], p[i][3]));

        o[i][0] = v.x; o[i][1] = v.y; o[i][2] = v.z; o[i][3] = v.w;
    }
}

/* How many floats lie between a and b (0: the same bits, or +0 and -0). */
static uint32_t ulps(float a, float b) {
    int32_t ia, ib;
    int64_t d;

    memcpy(&ia, &a, 4u); memcpy(&ib, &b, 4u);
    if (ia < 0) ia = INT32_MIN - ia;
    if (ib < 0) ib = INT32_MIN - ib;
    d = (int64_t)ia - (int64_t)ib;
    return (uint32_t)(d < 0 ? -d : d);
}

static void t_round(void) {
    static const char *const name[3] = { "plain sum", "FIPR (shz_mat4x4_transform_vec4)", "FTRV (shz_xmtrx_transform_vec4)" };
    static void (*const fn[3])(const float *, const quad_t *, quad_t *, unsigned) = { row_plain, row_fipr, row_ftrv };
    quad_t *in = (quad_t *)(void *)BUF_A, *out = (quad_t *)(void *)(BUF_A + OC / 2u);
    unsigned off[3] = { 0, 0, 0 }, top[3] = { 0, 0, 0 }, cy[3], pd[2] = { 0, 0 }, pt[2] = { 0, 0 }, i, c, k, r;
    double unit[3] = { 0.0, 0.0, 0.0 };
    uint32_t seed = 17u;

    for (i = 0; i < RN; i++) {
        seed = seed * 1664525u + 1013904223u; rpts[i][0] = (float)((int)((seed >> 8) % 40001u) - 20000) * 0.01f;
        seed = seed * 1664525u + 1013904223u; rpts[i][1] = (float)((seed >> 8) % 4001u) * 0.01f;
        seed = seed * 1664525u + 1013904223u; rpts[i][2] = (float)((int)((seed >> 8) % 40001u) - 20000) * 0.01f;
        rpts[i][3] = 1.0f;
    }
    for (k = 0; k < 3u; k++) fn[k](rmat, rpts, rout[k], RN);
    for (i = 0; i < RN; i++) {
        for (c = 0; c < 4u; c++) {
            const double t0 = (double)rpts[i][0] * (double)rmat[c], t1 = (double)rpts[i][1] * (double)rmat[4u + c];
            const double t2 = (double)rpts[i][2] * (double)rmat[8u + c], t3 = (double)rpts[i][3] * (double)rmat[12u + c];
            const double exact = t0 + t1 + t2 + t3;
            const double mass = (t0 < 0.0 ? -t0 : t0) + (t1 < 0.0 ? -t1 : t1) + (t2 < 0.0 ? -t2 : t2) + (t3 < 0.0 ? -t3 : t3);

            for (k = 0; k < 3u; k++) {
                const uint32_t u = ulps(rout[k][i][c], (float)exact);
                const double e = (double)rout[k][i][c] - exact, a = e < 0.0 ? -e : e;

                off[k] += u != 0u;
                if (u > top[k]) top[k] = u;
                if (mass > 0.0 && a * 16777216.0 / mass > unit[k]) unit[k] = a * 16777216.0 / mass;
            }
            for (k = 0; k < 2u; k++) {     /* FIPR against the plain sum, FTRV against FIPR */
                const uint32_t u = ulps(rout[k + 1u][i][c], rout[k][i][c]);

                pd[k] += u != 0u;
                if (u > pt[k]) pt[k] = u;
            }
        }
    }
    say("1 ROUNDING %u points x 4 sums against the exact sum", RN);
    for (k = 0; k < 3u; k++) {
        const unsigned u100 = (unsigned)(unit[k] * 100.0 + 0.5);

        say("1 %s: %u of %u not the nearest float, max %u ulp, max %u.%02u units", name[k], off[k], RN * 4u, top[k], F2(u100));
    }
    say("  (a unit: 2^-24 of the sum of the four |products|)");
    say("1 FIPR against the plain sum: %u differ, max %u ulp", pd[0], pt[0]);
    say("1 FTRV against FIPR: %u differ, max %u ulp", pd[1], pt[1]);
    memcpy(in, rpts, 256u * sizeof(quad_t));
    for (k = 0; k < 3u; k++) {
        uint64_t c0;

        fn[k](rmat, in, out, 256u);
        c0 = cyc();
        for (r = 0; r < 64u; r++) fn[k](rmat, in, out, 256u);
        cy[k] = x100(cyc() - c0, 64u * 256u);
    }
    say("1 cycles a point, 256 in the cache: plain %u.%02u, FIPR %u.%02u, FTRV %u.%02u", F2(cy[0]), F2(cy[1]), F2(cy[2]));
}

/* ---- 2. the transform ---------------------------------------------------------------------------- */

static matrix_t tmat __attribute__((aligned(32))) = { { 0.8f, 0.1f, 0.0f, 0.0f }, { -0.1f, 0.8f, 0.2f, 0.0f }, { 0.0f, -0.2f, 0.9f, 0.01f }, { 3.0f, 4.0f, 5.0f, 1.0f } };
#define TMAT_SHZ ((const shz_mat4x4_t *)(const void *)&tmat)

/* What mat_transform does, on SH4ZAM: FTRV, then 1 / w by FSRRA (w > 0). */
static void __attribute__((noinline)) shz_transform(const vector_t *in, vector_t *out, unsigned n) {
    unsigned i;

    for (i = 0; i < n; i++) {
        const shz_vec4_t r = shz_xmtrx_transform_vec4(shz_vec4_init(in[i].x, in[i].y, in[i].z, 1.0f));
        const float iw = shz_invf_fsrra(r.w);

        out[i].x = r.x * iw; out[i].y = r.y * iw; out[i].z = iw;
    }
}

static void __attribute__((noinline)) pvr_project_asm(void *out, const void *in, unsigned n) { shz_dce_pvr_project(out, in, n); }
static void __attribute__((noinline)) pvr_project_c(void *out, const void *in, unsigned n) { shz_dce_pvr_project_c(out, in, n); }

static void t_transform(void) {
    vector_t *in = (vector_t *)(void *)BUF_A, *out[2] = { (vector_t *)(void *)(BUF_A + OC / 2u), (vector_t *)(void *)BUF_B_ALIAS };
    vector_t *out2 = (vector_t *)(void *)(BUF_A + 3u * OC / 4u);
    pvr_vertex_t *pin = (pvr_vertex_t *)(void *)BUF_A, *pout = (pvr_vertex_t *)(void *)(BUF_A + OC / 2u), *pout2 = (pvr_vertex_t *)(void *)(BUF_A + 3u * OC / 4u);
    unsigned kos[2], shz[2], differ = 0, top = 0, a, b, i, p;
    uint64_t c0;

    c0 = cyc(); for (i = 0; i < 4096u; i++) mat_load(&tmat); a = x100(cyc() - c0, 4096u);
    c0 = cyc(); for (i = 0; i < 4096u; i++) shz_xmtrx_load_4x4(TMAT_SHZ); b = x100(cyc() - c0, 4096u);
    say("2 load XMTRX: mat_load %u.%02u cycles, shz_xmtrx_load_4x4 %u.%02u", F2(a), F2(b));

    for (i = 0; i < 16384u; i++) { in[i].x = (float)(i & 63u); in[i].y = (float)(i & 31u); in[i].z = 1.0f + (float)(i & 15u); in[i].w = 1.0f; }
    mat_load(&tmat);
    for (p = 0; p < 2u; p++) {
        mat_transform(in, out[p], 256, sizeof(vector_t));
        c0 = cyc(); for (i = 0; i < 256u; i++) mat_transform(in, out[p], 256, sizeof(vector_t)); kos[p] = x100(cyc() - c0, 65536u);
        shz_transform(in, out[p], 256u);
        c0 = cyc(); for (i = 0; i < 256u; i++) shz_transform(in, out[p], 256u); shz[p] = x100(cyc() - c0, 65536u);
    }
    say("2 256 vectors in the cache, cycles a vertex:");
    say("2  out 8 KB from in: mat_transform %u.%02u, SH4ZAM loop %u.%02u", F2(kos[0]), F2(shz[0]));
    say("2  out 1 MB from in (h1's placing): mat_transform %u.%02u, SH4ZAM loop %u.%02u", F2(kos[1]), F2(shz[1]));
    mat_transform(in, out[0], 256, sizeof(vector_t));
    shz_transform(in, out2, 256u);
    for (i = 0; i < 256u; i++) {
        const uint32_t u[3] = { ulps(out[0][i].x, out2[i].x), ulps(out[0][i].y, out2[i].y), ulps(out[0][i].z, out2[i].z) };

        for (p = 0; p < 3u; p++) { differ += u[p] != 0u; if (u[p] > top) top = u[p]; }
    }
    say("2  the two results: %u of 768 numbers differ, max %u ulp", differ, top);

    dcache_purge_range((uintptr_t)BUF_A, MB / 4u); dcache_purge_range((uintptr_t)BUF_B, MB / 4u);
    c0 = cyc(); mat_transform(in, (vector_t *)(void *)BUF_B, 16384, sizeof(vector_t)); a = x100(cyc() - c0, 16384u);
    dcache_purge_range((uintptr_t)BUF_A, MB / 4u); dcache_purge_range((uintptr_t)BUF_B, MB / 4u);
    c0 = cyc(); shz_transform(in, (vector_t *)(void *)BUF_B, 16384u); b = x100(cyc() - c0, 16384u);
    say("2 16384 vectors not in the cache: mat_transform %u.%02u, SH4ZAM loop %u.%02u", F2(a), F2(b));

    for (i = 0; i < 16384u; i++) {
        pin[i].flags = PVR_CMD_VERTEX; pin[i].x = (float)(i & 63u); pin[i].y = (float)(i & 31u); pin[i].z = 1.0f + (float)(i & 15u);
        pin[i].u = 0.25f; pin[i].v = 0.75f; pin[i].argb = 0xff808080u + i; pin[i].oargb = i;
    }
    shz_xmtrx_load_4x4(TMAT_SHZ);
    pvr_project_asm(pout, pin, 128u);
    c0 = cyc(); for (i = 0; i < 512u; i++) pvr_project_asm(pout, pin, 128u); a = x100(cyc() - c0, 65536u);
    pvr_project_c(pout2, pin, 128u);
    c0 = cyc(); for (i = 0; i < 512u; i++) pvr_project_c(pout2, pin, 128u); b = x100(cyc() - c0, 65536u);
    say("2 128 PVR vertices in the cache: shz_dce_pvr_project %u.%02u cycles a vertex, its C form %u.%02u, bytes %s", F2(a), F2(b),
        memcmp(pout, pout2, 128u * 32u) ? "DIFFER" : "the same");
    dcache_purge_range((uintptr_t)BUF_A, MB / 2u); dcache_purge_range((uintptr_t)BUF_B, MB / 2u);
    c0 = cyc(); pvr_project_asm(BUF_B, pin, 16384u); a = x100(cyc() - c0, 16384u);
    dcache_purge_range((uintptr_t)BUF_A, MB / 2u); dcache_purge_range((uintptr_t)BUF_B, MB / 2u);
    c0 = cyc(); pvr_project_c(BUF_B, pin, 16384u); b = x100(cyc() - c0, 16384u);
    say("2 16384 PVR vertices not in the cache: %u.%02u, C form %u.%02u", F2(a), F2(b));
}

/* ---- 3. copies ------------------------------------------------------------------------------------ */

typedef void (*copy_fn)(void *d, const void *s, unsigned n);

/* A loop the compiler must leave a loop: at -O3 it would otherwise call memcpy for it. */
#define PLAIN_LOOP __attribute__((noinline, optimize("no-tree-loop-distribute-patterns")))

static void c_memcpy(void *d, const void *s, unsigned n) { memcpy(d, s, n); }
static void c_shz(void *d, const void *s, unsigned n) { shz_memcpy(d, s, n); }
static void c_shz8(void *d, const void *s, unsigned n) { shz_memcpy8(d, s, n); }
static void c_shz32(void *d, const void *s, unsigned n) { shz_memcpy32(d, s, n); }
static void c_shz64(void *d, const void *s, unsigned n) { shz_memcpy64(d, s, n); }
static void c_shz128(void *d, const void *s, unsigned n) { shz_memcpy128(d, s, n); }
static void c_sq(void *d, const void *s, unsigned n) { sq_cpy(d, s, n); }
static void c_shzsq(void *d, const void *s, unsigned n) { uint32_t *q = sq_lock(d); shz_sq_memcpy32(q, s, n); sq_unlock(); }

/* The source is volatile so that each read is one 16-bit read (at -O3 two would otherwise become one of 32 bits):
   this is the loop a frame read by pixels is. */
static void PLAIN_LOOP c_loop16(void *d, const void *s, unsigned n) {
    const volatile uint16_t *a = (const volatile uint16_t *)s;
    uint16_t *b = (uint16_t *)d;
    unsigned i;

    for (i = 0; i < n / 2u; i++) b[i] = a[i];
}

static void PLAIN_LOOP c_loop32(void *d, const void *s, unsigned n) {
    const uint32_t *a = (const uint32_t *)s;
    uint32_t *b = (uint32_t *)d;
    unsigned i;

    for (i = 0; i < n / 4u; i++) b[i] = a[i];
}

/* The 32-bit loop, asking for the source two lines ahead. */
static void PLAIN_LOOP c_loop32_pref(void *d, const void *s, unsigned n) {
    const uint32_t *a = (const uint32_t *)s;
    uint32_t *b = (uint32_t *)d;
    unsigned i, k;

    for (i = 0; i < n / 32u; i++, a += 8, b += 8) {
        __builtin_prefetch(a + 16);
        for (k = 0; k < 8u; k++) b[k] = a[k];
    }
}

typedef struct copier {
    const char *name;
    copy_fn fn;
} copier_t;

/* One copy of n bytes of fresh noise from BUF_A to dst, the destination out of the cache first. ns; *right the check. */
static uint64_t copy_once(copy_fn fn, uint8_t *dst, unsigned n, uint32_t seed, int *right) {
    uint64_t t0, t;

    noise(BUF_A, n, seed);
    dcache_purge_range((uintptr_t)dst, n);
    t0 = ns(); fn(dst, BUF_A, n); t = ns() - t0;
    *right = !memcmp(dst, BUF_A, n);
    return t;
}

/* 256 copies of 4 KB that stays in the cache: 10 x MB/s. */
static unsigned copy_small(copy_fn fn, uint8_t *dst) {
    uint64_t t0;
    unsigned i;

    fn(dst, BUF_A, 4096u);
    t0 = ns();
    for (i = 0; i < 256u; i++) fn(dst, BUF_A, 4096u);
    return mbs10(256u * 4096u, ns() - t0);
}

static void t_copy(void) {
    static const copier_t all[9] = {
        { "memcpy", c_memcpy }, { "shz_memcpy", c_shz }, { "shz_memcpy8", c_shz8 }, { "shz_memcpy32", c_shz32 }, { "shz_memcpy64", c_shz64 },
        { "shz_memcpy128", c_shz128 }, { "32-bit loop", c_loop32 }, { "sq_cpy", c_sq }, { "shz_sq_memcpy32", c_shzsq } };
    static const unsigned again[3] = { 0u, 3u, 6u };             /* the rows run again on h1's placing */
    unsigned i, a, b, c;
    int right;

    say("3 COPY 1 MB RAM to RAM, dst out of the cache, dst 8 KB off the source's cache entries:");
    for (i = 0; i < 9u; i++) {
        uint64_t t;

        step(all[i].name);
        t = copy_once(all[i].fn, BUF_B, MB, 30u + i, &right);
        say("3  %s: %u us, %u.%u MB/s, %s", all[i].name, us_of(t), F1(mbs10(MB, t)), right ? "right" : "WRONG");
    }
    say("3 the same, dst exactly 1 MB from the source (h1's placing):");
    for (i = 0; i < 3u; i++) {
        const uint64_t t = copy_once(all[again[i]].fn, BUF_B_ALIAS, MB, 40u + i, &right);

        say("3  %s: %u us, %u.%u MB/s, %s", all[again[i]].name, us_of(t), F1(mbs10(MB, t)), right ? "right" : "WRONG");
    }
    noise(BUF_A, 4096u, 50u);
    a = copy_small(c_memcpy, BUF_A + OC / 2u); b = copy_small(c_shz, BUF_A + OC / 2u); c = copy_small(c_shz8, BUF_A + OC / 2u);
    say("3 4 KB in the cache, dst 8 KB from src: memcpy %u.%u MB/s, shz_memcpy %u.%u, shz_memcpy8 %u.%u", F1(a), F1(b), F1(c));
    a = copy_small(c_memcpy, BUF_B_ALIAS); b = copy_small(c_shz, BUF_B_ALIAS); c = copy_small(c_shz8, BUF_B_ALIAS);
    say("3 4 KB, dst 1 MB from src (h1's placing): memcpy %u.%u MB/s, shz_memcpy %u.%u, shz_memcpy8 %u.%u", F1(a), F1(b), F1(c));
}

/* ---- the PVR ---------------------------------------------------------------------------------------- */

static pvr_ptr_t block512;
static uint64_t last_render_ns;

static int scene_close(void) {
    pvr_stats_t st;
    unsigned t;

    pvr_scene_finish();
    for (t = 0; t < 20u && pvr_wait_ready() < 0; t++) sh4der_fault_beat(&crumbs);
    if (t == 20u) return 0;
    for (t = 0; t < 20u && pvr_wait_render_done() < 0; t++) sh4der_fault_beat(&crumbs);
    if (t == 20u || pvr_get_stats(&st) < 0) return 0;
    last_render_ns = st.rnd_last_time;
    return 1;
}

static void flat_header(pvr_poly_hdr_t *h) {
    pvr_poly_cxt_t c;

    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(h, &c);
}

/* The store-queue address of a block of texture memory, by the TA's texture path (what pvr_txr_load writes to). */
static void *tex_path(pvr_ptr_t mem) { return (void *)(((uintptr_t)mem & 0x00ffffffu) | PVR_TA_TEX_MEM); }

/* ---- 4. vertices into the TA ------------------------------------------------------------------------- */

#define SEND_VERTS 15000u

/* 15,000 vertices (5,000 small triangles) from RAM into the TA three ways. Times the sending only. */
static void t_send(void) {
    static const char *const how[3] = { "pvr_prim a vertex", "pvr_sq_load, one run", "shz_sq_memcpy32, one run" };
    pvr_vertex_t *arr = (pvr_vertex_t *)(void *)BUF_A;
    pvr_poly_hdr_t h;
    uint32_t seed = 3u;
    unsigned i, m;

    flat_header(&h);
    for (i = 0; i < SEND_VERTS; i++) {
        if (i % 3u == 0u) seed = seed * 1664525u + 1013904223u;
        arr[i].flags = i % 3u == 2u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        arr[i].x = (float)((seed >> 8) % 630u) + (i % 3u == 1u ? 3.0f : 0.0f);
        arr[i].y = (float)((seed >> 20) % 470u) + (i % 3u == 2u ? 3.0f : 0.0f);
        arr[i].z = 1.0f; arr[i].u = arr[i].v = 0.0f; arr[i].argb = 0xff808080u; arr[i].oargb = 0u;
    }
    for (m = 0; m < 3u; m++) {
        uint64_t t0, t;

        step(how[m]);
        pvr_wait_ready();
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        pvr_prim(&h, sizeof h);
        t0 = ns();
        if (m == 0u) for (i = 0; i < SEND_VERTS; i++) pvr_prim(&arr[i], sizeof arr[i]);
        else if (m == 1u) pvr_sq_load(NULL, arr, SEND_VERTS * sizeof arr[0], PVR_DMA_TA);
        else { uint32_t *q = sq_lock((void *)PVR_TA_INPUT); shz_sq_memcpy32(q, arr, SEND_VERTS * sizeof arr[0]); sq_unlock(); }
        t = ns() - t0;
        pvr_list_finish();
        if (!scene_close()) { say("4 TA, %s: no render end", how[m]); continue; }
        say("4 TA, %s: %u us, %u ns/vertex, %u.%u MB/s", how[m], us_of(t), (unsigned)(t / SEND_VERTS), F1(mbs10(SEND_VERTS * 32u, t)));
    }
}

/* ---- 5. texture uploads -------------------------------------------------------------------------------- */

static void t_upload(void) {
    static const unsigned sizes[2] = { 32768u, 524288u };
    unsigned i, m;

    for (i = 0; i < 2u; i++) {
        for (m = 0; m < 2u; m++) {
            uint64_t t0, t;

            noise(BUF_A, sizes[i], 60u + 2u * i + m);
            t0 = ns();
            if (m == 0u) pvr_txr_load(BUF_A, block512, sizes[i]);
            else { uint32_t *q = sq_lock(tex_path(block512)); shz_sq_memcpy32(q, BUF_A, sizes[i]); sq_unlock(); }
            t = ns() - t0;
            say("5 upload %u KB, %s: %u us, %u.%u MB/s, read back %s", sizes[i] / 1024u, m ? "shz_sq_memcpy32" : "pvr_txr_load", us_of(t),
                F1(mbs10(sizes[i], t)), memcmp(block512, BUF_A, sizes[i]) ? "WRONG" : "right");
        }
    }
}

/* ---- 6. video memory to RAM ------------------------------------------------------------------------------ */

#define SB_ISTNRM   REG32(0xa05f6900)
#define SB_ISTEXT   REG32(0xa05f6904)
#define SB_ISTERR   REG32(0xa05f6908)
#define SB_PDSTAP   REG32(0xa05f7c00)
#define SB_PDSTAR   REG32(0xa05f7c04)
#define SB_PDLEN    REG32(0xa05f7c08)
#define SB_PDDIR    REG32(0xa05f7c0c)
#define SB_PDTSEL   REG32(0xa05f7c10)
#define SB_PDEN     REG32(0xa05f7c14)
#define SB_PDST     REG32(0xa05f7c18)
#define SB_PDAPRO   REG32(0xa05f7c80)
#define ISTNRM_PVR_DMA (1u << 11)
#define EVT_PVR_DMA_END 0x000bu            /* the same bit as an event code: register 0, bit 11 */

/* PVR-DMA, video memory to RAM. The caller has put the destination out of the cache. */
static void pd_start(pvr_ptr_t mem, void *dst, uint32_t bytes) {
    SB_PDEN = 0;
    SB_ISTNRM = ISTNRM_PVR_DMA;
    SB_PDAPRO = 0x6702007fu;
    SB_PDSTAP = 0x04000000u | ((uint32_t)(uintptr_t)mem & 0x00ffffffu);
    SB_PDSTAR = (uint32_t)(uintptr_t)dst & 0x1fffffe0u;
    SB_PDLEN = bytes;
    SB_PDDIR = 1;
    SB_PDTSEL = 0;
    SB_PDEN = 1;
    SB_PDST = 1;
}

static void pd_end(void) {
    if (SB_PDST & 1u) SB_PDST = 0;
    SB_PDEN = 0;
    SB_ISTNRM = ISTNRM_PVR_DMA;
}

static volatile uint64_t dma_done_ns;

static void dma_evt(uint32_t code, void *data) {
    (void)code; (void)data;
    dma_done_ns = ns();
}

static const unsigned vsizes[3] = { 32768u, 131072u, 524288u };
static uint64_t v_dma[3], v_best[3];       /* ns: PVR-DMA, and the quickest read by the SH-4 that came back right */
static const char *v_best_name[3];

/* One read of n bytes of the block (from `src`, one of its addresses) into dst, timed, checked against BUF_A. */
static void v_row(const char *tag, const char *name, copy_fn fn, const void *src, uint8_t *dst, unsigned k) {
    const unsigned n = vsizes[k];
    uint64_t t0, t;
    int right;

    dcache_purge_range((uintptr_t)dst, n);
    t0 = ns(); fn(dst, src, n); t = ns() - t0;
    right = !memcmp(dst, BUF_A, n);
    say("%s  %s: %u us, %u.%u MB/s, %s", tag, name, us_of(t), F1(mbs10(n, t)), right ? "right" : "WRONG");
    if (right && (!v_best[k] || t < v_best[k])) { v_best[k] = t; v_best_name[k] = name; }
}

static void v_ratio(const char *tag) {
    unsigned k;

    for (k = 0; k < 3u; k++) {
        if (!v_dma[k] || !v_best[k]) { say("%s %u KB: no ratio (a row is missing)", tag, vsizes[k] / 1024u); continue; }
        say("%s %u KB: PVR-DMA %u us; quickest SH-4 read %u us (%s); SH-4 / DMA = %u.%02u", tag, vsizes[k] / 1024u, us_of(v_dma[k]), us_of(v_best[k]),
            v_best_name[k], F2(x100(v_best[k], v_dma[k])));
    }
}

static void t_vram(void) {
    static const copier_t rd[5] = { { "memcpy", c_memcpy }, { "16-bit loop", c_loop16 }, { "32-bit loop", c_loop32 },
        { "shz_memcpy8 (64-bit moves)", c_shz8 }, { "shz_memcpy32", c_shz32 } };
    unsigned k, i;

    noise(BUF_A, 524288u, 70u);
    pvr_txr_load(BUF_A, block512, 524288u);
    say("6 VIDEO MEMORY TO RAM, the block at %08x (uncached address)", (unsigned)(uintptr_t)block512);
    for (k = 0; k < 3u; k++) {
        uint64_t t0;

        say("6 %u KB:", vsizes[k] / 1024u);
        for (i = 0; i < 5u; i++) { step(rd[i].name); v_row("6", rd[i].name, rd[i].fn, block512, BUF_B, k); }
        step("6 pvr-dma");
        dcache_purge_range((uintptr_t)BUF_B, vsizes[k]);
        dma_done_ns = 0;
        asic_evt_set_handler(EVT_PVR_DMA_END, dma_evt, NULL);
        asic_evt_enable(EVT_PVR_DMA_END, ASIC_IRQ_DEFAULT);
        t0 = ns();
        pd_start(block512, BUF_B, vsizes[k]);
        while (!dma_done_ns && ns() - t0 < LIMIT_NS) { }
        asic_evt_disable(EVT_PVR_DMA_END, ASIC_IRQ_DEFAULT);
        asic_evt_remove_handler(EVT_PVR_DMA_END);
        if (dma_done_ns) {
            const uint64_t t = dma_done_ns - t0;
            const int right = !memcmp(BUF_B, BUF_A, vsizes[k]);

            say("6  PVR-DMA, ended by its interrupt: %u us, %u.%u MB/s, %s", us_of(t), F1(mbs10(vsizes[k], t)), right ? "right" : "WRONG");
            if (right) v_dma[k] = t;
            pd_end();
        } else {                           /* no interrupt: the same copy again, its busy bit read every 250 us */
            uint64_t t;
            int right;

            say("6  PVR-DMA: no interrupt in 200 ms (busy bit %u)", (unsigned)(SB_PDST & 1u));
            pd_end();
            dcache_purge_range((uintptr_t)BUF_B, vsizes[k]);
            t0 = ns();
            pd_start(block512, BUF_B, vsizes[k]);
            do { wait_ns(250000u); t = ns() - t0; } while ((SB_PDST & 1u) && t < LIMIT_NS);
            right = !(SB_PDST & 1u) && !memcmp(BUF_B, BUF_A, vsizes[k]);
            say("6  PVR-DMA, busy bit read each 250 us: %u us, %u.%u MB/s, %s", us_of(t), F1(mbs10(vsizes[k], t)), right ? "right" : "WRONG or no end");
            if (right) v_dma[k] = t;
            pd_end();
        }
    }
    v_ratio("6");
}

/* ---- 7. the warp, sent two ways --------------------------------------------------------------------------- */

/* src/sh4der_warp.c compiled twice by build.py: SH4DER_WARP_SEND_PRIM 1 (a pvr_prim a vertex) and 0 (a strip in
   one store-queue run), its functions renamed on the command line. */
void h2_warp_mesh_prim(pvr_ptr_t tex, const sh4der_warp_frame_t *f, unsigned nx, unsigned ny, float sx, float sy, float z, uint32_t argb);
void h2_warp_mesh_sq(pvr_ptr_t tex, const sh4der_warp_frame_t *f, unsigned nx, unsigned ny, float sx, float sy, float z, uint32_t argb);
void h2_warp_pixel_prim(pvr_ptr_t tex, float sx, float sy, float z);
void h2_warp_pixel_sq(pvr_ptr_t tex, float sx, float sy, float z);

static const sh4der_warp_frame_t warp_frame = { 3u, {
    { SH4DER_WARP_RING, 160.0f, 120.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.4f, 1.0f },
    { SH4DER_WARP_HAZE, 0.0f, 0.0f, 40.0f, 60.0f, 140.0f, 220.0f, 1.3f, 1.0f },
    { SH4DER_WARP_WOBBLE, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.7f, 0.8f } } };

#define WARP_FRAMES 8u

/* One frame with the mesh (what 0) or the pixel pass (what 1), sent by pvr_prim (prim 1) or in runs (prim 0). The
   time is the draw call's alone. 0 when the frame did not come back. */
static uint64_t warp_frame_ns(pvr_ptr_t tex, int what, int prim) {
    uint64_t t0, t;

    sh4der_fault_beat(&crumbs);
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    t0 = ns();
    if (what == 0) (prim ? h2_warp_mesh_prim : h2_warp_mesh_sq)(tex, &warp_frame, SH4DER_WARP_FINE_X, SH4DER_WARP_FINE_Y, 2.0f, 2.0f, 1.0f, 0xffffffffu);
    else (prim ? h2_warp_pixel_prim : h2_warp_pixel_sq)(tex, 2.0f, 2.0f, 1.0f);
    t = ns() - t0;
    pvr_list_finish();
    return scene_close() ? t : 0u;
}

static void t_warp(void) {
    static sh4der_warp_vtx_t row[SH4DER_WARP_FINE_X + 1];
    static const unsigned verts[2] = { 2u * (SH4DER_WARP_FINE_X + 1u) * SH4DER_WARP_FINE_Y, 4u * (SH4DER_WARP_FRAME_W / SH4DER_WARP_PIXEL_CELL) * (SH4DER_WARP_FRAME_H / SH4DER_WARP_PIXEL_CELL) };
    static const char *const name[2] = { "mesh 40 x 30", "pixel pass" };
    pvr_ptr_t tex = pvr_mem_malloc(SH4DER_WARP_TEX_W * SH4DER_WARP_TEX_H * 2u);
    uint64_t maths = ~0ull;
    unsigned what, prim, r, j;

    if (!tex) { say("7 warp: no video memory"); return; }
    noise(BUF_A, SH4DER_WARP_TEX_W * SH4DER_WARP_TEX_H * 2u, 80u);
    pvr_txr_load(BUF_A, tex, SH4DER_WARP_TEX_W * SH4DER_WARP_TEX_H * 2u);
    pvr_txr_set_stride(SH4DER_WARP_FRAME_W);
    for (r = 0; r < WARP_FRAMES; r++) {
        const uint64_t t0 = ns();
        uint64_t t;

        for (j = 0; j <= SH4DER_WARP_FINE_Y; j++) sh4der_warp_row(&warp_frame, SH4DER_WARP_FINE_X, SH4DER_WARP_FINE_Y, j, row);
        t = ns() - t0;
        if (t < maths) maths = t;
    }
    sink += (uint32_t)row[3].u;
    say("7 WARP (src/sh4der_warp.c), least and mean of %u frames, the draw call alone:", WARP_FRAMES);
    for (what = 0; what < 2u; what++) {
        uint64_t least[2] = { ~0ull, ~0ull }, sum[2] = { 0, 0 }, render[2] = { 0, 0 };
        int ok = 1;

        step(name[what]);
        for (r = 0; r < WARP_FRAMES && ok; r++) {
            for (prim = 0; prim < 2u && ok; prim++) {  /* the two ways in turn, so neither has the warmer cache */
                const uint64_t t = warp_frame_ns(tex, (int)what, (int)prim);

                if (!t) { ok = 0; break; }
                if (t < least[prim]) least[prim] = t;
                sum[prim] += t; render[prim] = last_render_ns;
            }
        }
        if (!ok) { say("7 %s: no render end", name[what]); continue; }
        say("7 %s, %u vertices: by pvr_prim %u / %u us, by runs %u / %u us", name[what], verts[what], us_of(least[1]), us_of(sum[1] / WARP_FRAMES),
            us_of(least[0]), us_of(sum[0] / WARP_FRAMES));
        say("7  a vertex: %u ns against %u; PVR render %u / %u us", (unsigned)(least[1] / verts[what]), (unsigned)(least[0] / verts[what]),
            us_of(render[1]), us_of(render[0]));
    }
    say("7 the mesh's maths alone (31 rows of sh4der_warp_row): %u us", us_of(maths));
    pvr_mem_free(tex);
}

/* ---- 8. the DMAC, timed from before its start ------------------------------------------------------------ */

static dma_config_t dmac_cfg;

static int dmac_busy(void) { return dma_is_running(DMA_CHANNEL_1); }

static void t_dmac(void) {
    static const struct { dma_transmitmode_t mode; const char *name; } v[2] = { { DMA_TRANSMITMODE_BURST, "burst" }, { DMA_TRANSMITMODE_CYCLE_STEAL, "cycle steal" } };
    unsigned i;

    for (i = 0; i < 2u; i++) {
        uint64_t t0, t1, took;
        unsigned rate;

        step(v[i].name);
        noise(BUF_A, 524288u, 90u + i);
        memset(BUF_B, 0, 524288u);
        if (dma_is_running(DMA_CHANNEL_1)) dma_transfer_abort(DMA_CHANNEL_1);   /* dma_transfer would wait for it without a limit */
        dmac_cfg.channel = DMA_CHANNEL_1; dmac_cfg.request = DMA_REQUEST_AUTO_MEM_TO_MEM; dmac_cfg.unit_size = DMA_UNITSIZE_32BYTE;
        dmac_cfg.src_mode = DMA_ADDRMODE_INCREMENT; dmac_cfg.dst_mode = DMA_ADDRMODE_INCREMENT; dmac_cfg.transmit_mode = v[i].mode; dmac_cfg.callback = NULL;
        t0 = ns();
        if (dma_transfer(&dmac_cfg, dma_map_dst(BUF_B, 524288u), dma_map_src(BUF_A, 524288u), 524288u, NULL) < 0) { say("8 DMAC ch1 %s: refused", v[i].name); continue; }
        t1 = ns();
        rate = spin(dmac_busy, LIMIT_NS, &took);
        if (dmac_busy()) { dma_transfer_abort(DMA_CHANNEL_1); say("8 DMAC ch1 %s: no end in 200 ms", v[i].name); continue; }
        dcache_inval_range((uintptr_t)BUF_B, 524288u);
        say("8 DMAC ch1 %s, 512 KB: the start call came back after %u us, the copy was done %u us after that; copy %s", v[i].name, us_of(t1 - t0),
            us_of(took), memcmp(BUF_A, BUF_B, 524288u) ? "WRONG" : "right");
        say("8  register loop in that time: %u /ms (%u%% of alone)", rate, alone ? rate * 100u / alone : 0u);
    }
}

/* ---- group 2: video memory through the operand cache ------------------------------------------------------- */

/* 256 lines of 32 bytes in dst: how many hold `old`'s bytes, how many `new_`'s. */
static void lines_of(const uint8_t *dst, const uint8_t *old, const uint8_t *new_, unsigned *n_old, unsigned *n_new) {
    unsigned l;

    for (*n_old = *n_new = 0, l = 0; l < 256u; l++) {
        *n_old += !memcmp(dst + 32u * l, old + 32u * l, 32u);
        *n_new += !memcmp(dst + 32u * l, new_ + 32u * l, 32u);
    }
}

static void t_cached(void) {
    static const copier_t rd[3] = { { "cached, 32-bit loop", c_loop32 }, { "cached, 32-bit loop + prefetch", c_loop32_pref }, { "cached, shz_memcpy32", c_shz32 } };
    const uint8_t *mirror = (const uint8_t *)(((uintptr_t)block512 & 0x1fffffffu) | 0x80000000u);
    /* the destination half a cache from the mirror at equal offsets; `fresh` on the destination's cache entries */
    uint8_t *dst = BUF_B + (((uintptr_t)mirror + OC / 2u - (uintptr_t)BUF_B) & (OC - 1u)), *fresh = dst + 4u * OC;
    unsigned k, i, n_old, n_new;
    uint64_t t0, t;

    say("C VIDEO MEMORY THROUGH THE CACHE, the block at %08x", (unsigned)(uintptr_t)mirror);
    step("C first line");
    dcache_inval_range((uintptr_t)mirror, 32u);
    memcpy(dst, mirror, 32u);
    dcache_inval_range((uintptr_t)mirror, 32u);
    if (memcmp(dst, BUF_A, 32u)) { say("C the first line read through the cache is WRONG: the rest not run"); return; }
    say("C the first line read through the cache is right");
    for (k = 0; k < 3u; k++) {
        say("C %u KB:", vsizes[k] / 1024u);
        for (i = 0; i < 3u; i++) {
            step(rd[i].name);
            dcache_inval_range((uintptr_t)mirror, vsizes[k]);
            v_row("C", rd[i].name, rd[i].fn, mirror, dst, k);
        }
    }
    step("C inval");
    t0 = ns(); dcache_inval_range((uintptr_t)mirror, 524288u); t = ns() - t0;
    say("C dcache_inval_range over the 512 KB (needed before each read of new contents): %u us", us_of(t));

    step("C stale");
    c_loop32(dst, mirror, 8192u);                                /* 8 KB of the block is in the cache now */
    noise(fresh, 8192u, 71u);
    pvr_txr_load(fresh, block512, 8192u);                        /* new contents, written behind the cache */
    c_loop32(dst, mirror, 8192u);
    lines_of(dst, BUF_A, fresh, &n_old, &n_new);
    say("C 8 KB read, rewritten by the store queue, read again: %u of 256 lines old, %u new", n_old, n_new);
    dcache_inval_range((uintptr_t)mirror, 8192u);
    c_loop32(dst, mirror, 8192u);
    lines_of(dst, BUF_A, fresh, &n_old, &n_new);
    say("C  after dcache_inval_range: %u old, %u new", n_old, n_new);
    dcache_inval_range((uintptr_t)mirror, 8192u);
    v_ratio("C");
}

/* ---- group 3: G2 DMA, sound RAM ----------------------------------------------------------------------------- */

#define G2_OFF 0x100000u                   /* in sound RAM: clear of the ARM7's idle loop at 0 */
#define G2_LIMIT_NS 1000000000ull          /* under the fault catcher's two seconds */
#define G2_ADSTAG REG32(0xa05f7800)
#define G2_ADSTAR REG32(0xa05f7804)
#define G2_ADLEN  REG32(0xa05f7808)
#define G2_ADEN   REG32(0xa05f7814)
#define G2_ADST   REG32(0xa05f7818)

static volatile uint64_t g2_end_ns;
static volatile int g2_done;
static int g2_dead;                        /* a transfer did not end: KallistiOS still counts it as running */

static void g2_cb(void *data) {
    (void)data;
    g2_end_ns = ns();
    g2_done = 1;
}

static int g2_waiting(void) { return !g2_done; }

/* One G2 DMA on channel 0 between `ram` (RAM, or texture memory in group 4) and sound RAM at G2_OFF, started
   through KallistiOS without its unbounded wait; the end is its interrupt, the wait is the register loop (no status
   register is polled, the sound chip is not touched). 1 done (*t from before the start to the interrupt, *rate the
   loop's), 0 no end (the channel is switched off), -1 refused. */
static int g2_run(int to_ram, void *ram, unsigned n, uint64_t *t, unsigned *rate) {
    uint64_t t0;

    g2_done = 0;
    t0 = ns();
    if (g2_dma_transfer(ram, (void *)(uintptr_t)(SPU_RAM_BASE + G2_OFF), n, 0, g2_cb, NULL, to_ram ? G2_DMA_TO_SH4 : G2_DMA_TO_G2, 0, G2_DMA_CHAN_SPU, 0) < 0) return -1;
    *rate = spin(g2_waiting, G2_LIMIT_NS, NULL);
    if (!g2_done) {
        G2_ADEN = 0; G2_ADST = 0;
        g2_dead = 1;
        say("G  NO END in 1 s; channel 0 switched off. ADSTAG %08x ADSTAR %08x ADLEN %08x", (unsigned)G2_ADSTAG, (unsigned)G2_ADSTAR, (unsigned)G2_ADLEN);
        say("G  ISTNRM %08x ISTEXT %08x ISTERR %08x", (unsigned)SB_ISTNRM, (unsigned)SB_ISTEXT, (unsigned)SB_ISTERR);
        return 0;
    }
    *t = g2_end_ns - t0;
    return 1;
}

static void t_g2(void) {
    static const unsigned sizes[4] = { 4096u, 32768u, 131072u, 524288u };
    static uint8_t ends[512];
    uint64_t t0, t;
    unsigned i, rate;
    int r;

    step("G pio");
    noise(BUF_A, 32768u, 100u);
    t0 = ns(); spu_memload_sq(G2_OFF, BUF_A, 32768u); t = ns() - t0;
    say("G 32 KB to sound RAM, spu_memload_sq: %u us, %u.%u MB/s", us_of(t), F1(mbs10(32768u, t)));
    t0 = ns(); spu_memread(BUF_B, G2_OFF, 32768u); t = ns() - t0;
    say("G 32 KB from sound RAM, spu_memread (register reads): %u us, %u.%u MB/s, %s", us_of(t), F1(mbs10(32768u, t)), memcmp(BUF_B, BUF_A, 32768u) ? "WRONG" : "right");
    for (i = 0; i < 4u; i++) {
        const unsigned n = sizes[i];

        step("G to sound");
        noise(BUF_A, n, 101u + i);
        dcache_purge_range((uintptr_t)BUF_A, n);                 /* the DMA reads RAM, not the cache */
        r = g2_run(0, BUF_A, n, &t, &rate);
        if (r < 0) { say("G %u KB to sound RAM: refused (errno %d)", n / 1024u, errno); return; }
        if (!r) return;
        spu_memread(ends, G2_OFF, 256u);                         /* the DMA is over: the sound chip may be read again */
        spu_memread(ends + 256, G2_OFF + n - 256u, 256u);
        say("G %u KB to sound RAM by DMA: %u us, %u.%u MB/s; loop %u /ms (%u%%); both ends %s", n / 1024u, us_of(t), F1(mbs10(n, t)), rate,
            alone ? rate * 100u / alone : 0u, memcmp(ends, BUF_A, 256u) || memcmp(ends + 256, BUF_A + n - 256u, 256u) ? "WRONG" : "right");

        step("G to RAM");
        memset(BUF_B, 0x5a, n);
        dcache_purge_range((uintptr_t)BUF_B, n);                 /* nothing of the destination left in the cache */
        r = g2_run(1, BUF_B, n, &t, &rate);
        if (r < 0) { say("G %u KB to RAM: refused (errno %d)", n / 1024u, errno); return; }
        if (!r) return;
        say("G %u KB to RAM by DMA: %u us, %u.%u MB/s; loop %u /ms (%u%%); copy %s", n / 1024u, us_of(t), F1(mbs10(n, t)), rate,
            alone ? rate * 100u / alone : 0u, memcmp(BUF_B, BUF_A, n) ? "WRONG" : "right");
    }
}

/* ---- group 4: sound RAM to texture memory in one G2 DMA ---------------------------------------------------- */

static void t_g2_vram(void) {
    static uint8_t ends[256];
    const volatile uint8_t *seen = (const volatile uint8_t *)block512;
    unsigned i, moved = 0, kept = 0, rate;
    uint64_t t;
    int r;

    if (g2_dead) { say("V not run: group 3 left channel 0 without an end"); return; }
    step("V set up");
    noise(BUF_A, 32768u, 120u);
    spu_memload_sq(G2_OFF, BUF_A, 32768u);
    spu_memread(ends, G2_OFF, 256u);
    if (memcmp(ends, BUF_A, 256u)) { say("V sound RAM does not hold the test bytes: not run"); return; }
    noise(BUF_C, 32768u, 121u);
    pvr_txr_load(BUF_C, block512, 32768u);
    step("V dma");
    r = g2_run(1, (void *)(0x04000000u | ((uintptr_t)block512 & 0x00ffffffu)), 32768u, &t, &rate);
    if (r < 0) { say("V 32 KB sound RAM to texture memory: refused (errno %d)", errno); return; }
    for (i = 0; i < 32768u; i++) { moved += seen[i] == BUF_A[i]; kept += seen[i] == BUF_C[i]; }
    if (r) say("V 32 KB sound RAM to texture memory: ended after %u us, %u.%u MB/s", us_of(t), F1(mbs10(32768u, t)));
    say("V texture bytes: %u of 32768 are the sound RAM's, %u still what was there", moved, kept);
}

/* ---- the groups ------------------------------------------------------------------------------ */

typedef struct test {
    const char *name;
    void (*fn)(void);
} test_t;

static void run_tests(const test_t *t, unsigned n) {
    unsigned i;

    for (i = 0; i < n; i++) { step(t[i].name); t[i].fn(); }
}

/* The PVR up, and the 512 KB block of video memory holding the noise that is left in BUF_A. */
static int pvr_start(void) {
    pvr_init_params_t init = pvr_default_params;

    init.opb_sizes[PVR_LIST_OP_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_OP_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_TR_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_PT_POLY] = PVR_BINSIZE_0;
    init.vertex_buf_size = 1024 * 1024;
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return 0; }
    pvr_set_bg_color(0.0f, 0.0f, 0.2f);
    block512 = pvr_mem_malloc(524288u);
    if (!block512 || ((uintptr_t)block512 & 31u)) { say("video memory block FAIL (%08x)", (unsigned)(uintptr_t)block512); return 0; }
    noise(BUF_A, 524288u, 70u);
    pvr_txr_load(BUF_A, block512, 524288u);
    return 1;
}

static void group1(void) {
    static const test_t t[] = {
        { "timer", t_timer }, { "1 rounding", t_round }, { "2 transform", t_transform }, { "3 copy", t_copy }, { "4 send", t_send },
        { "5 upload", t_upload }, { "6 vram", t_vram }, { "7 warp", t_warp }, { "8 dmac", t_dmac } };

    say("GROUP 1: SH4ZAM beside KallistiOS, video memory to RAM. One run, this console.");
    if (!pvr_start()) return;
    if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused");
    run_tests(t, sizeof t / sizeof t[0]);
    step("group 1 end");
    pvr_shutdown();
}

static void group2(void) {
    say("GROUP 2: video memory through the operand cache");
    if (!pvr_start()) return;
    step("C cached");
    t_cached();
    step("group 2 end");
    pvr_shutdown();
}

static void group3(void) {
    say("GROUP 3: G2 DMA channel 0, RAM and sound RAM");
    step("G g2");
    t_g2();
    step("group 3 end");
}

static void group4(void) {
    say("GROUP 4: sound RAM to texture memory in one G2 DMA");
    if (!pvr_start()) return;
    t_g2_vram();
    step("group 4 end");
    pvr_shutdown();
}

int main(void) {
    static const char *const next[4] = { "START: VRAM by cache", "START: G2 DMA", "START: G2 to VRAM", "(end)" };
    static void (*const group[3])(void) = { group2, group3, group4 };
    unsigned page = 0, pages, held = ~0u, done = 1;

    group1();
    vid_set_mode(DM_640x480, PM_RGB565);
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;

        sh4der_fault_beat(&crumbs);
        pages = (nlines + ROWS - 1u) / ROWS;
        if (!pages) pages = 1u;
        if (held == ~0u) page_draw(page, pages, next[done - 1u]);
        else if (hit & CONT_A) { page = (page + 1u) % pages; page_draw(page, pages, next[done - 1u]); }
        else if (hit & CONT_B) { page = (page + pages - 1u) % pages; page_draw(page, pages, next[done - 1u]); }
        else if ((hit & CONT_START) && done < 4u) {
            const unsigned first = nlines;

            group[done - 1u]();
            done++;
            vid_set_mode(DM_640x480, PM_RGB565);
            pages = (nlines + ROWS - 1u) / ROWS;
            page = first / ROWS;                                 /* the page the new lines begin on */
            page_draw(page, pages, next[done - 1u]);
        }
        held = now;
        thd_sleep(20);
    }
    return 0;
}
