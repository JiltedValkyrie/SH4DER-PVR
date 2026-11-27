/* SH4DER-PVR: the chip bench, image h1: what each part of the Dreamcast does in what time

   demo/psm/bench/bench_h1.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A test program for a real console. Many small tests, one number a line, each in its own stage so a hang names the
   test it was in (the fault catcher, include/sh4der_fault.h, shows it). Every line goes to the serial line ("h1:") as soon as
   it exists and to result pages (A next, B back). The tests run in three groups; the pages are shown after each, and
   START goes on to the next, so what has been measured can be photographed before anything riskier runs.

     group 1   SH-4: the timer, the operand cache (hit, miss, write, allocate, write back), copies, the matrix unit,
               twiddling a map. PVR: uploads (store queue, channel 2 DMA), render time by layer kind, texture
               locality, triangles in one tile, vertices into the TA.
     group 2   DMA and the bus: PVR-DMA downloads, the DMAC's channels 1 and 3 copying RAM, a counting loop on the
               SH-4 beside each of them and beside a render, the YUV converter.
     group 3   the operand cache as RAM (OC-RAM): it is switched on and LEFT ON until the power goes off.

   Only RAM, video memory and PVR / SH-4 registers are written; nothing outlives a power cycle. Every wait has a
   limit. Every number comes from the run that prints it: one run, one console; only that is evidence.
   Times are from timer_ns_gettime64 and the SH-4's cycle counter (the first lines say what they resolve).
   Build: python demo/psm/bench/build.py.
*/

#include <kos.h>
#include <arch/dmac.h>
#include <dc/cache.h>
#include <stdarg.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

/* ---- the result pages ----------------------------------------------------------- */

#define COLS 50u
#define ROWS 17u
#define LINES_MAX 260u
static char lines[LINES_MAX][COLS + 1u];
static unsigned nlines;
static sh4der_fault_crumbs_t crumbs;

/* A line for the pages and the serial line. A line over the page's width goes on, indented, on the next. */
static void say(const char *fmt, ...) {
    char big[200];
    const char *s = big;
    size_t left;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(big, sizeof big, fmt, ap);
    va_end(ap);
    printf("h1: %s\n", big);
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
    snprintf(head, sizeof head, "CHIP BENCH h1 p%u/%u  A/B page  %s", page + 1u, pages, next);
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
#define LINE 32u                           /* an operand cache line */
static uint8_t bufa[MB] __attribute__((aligned(32)));
static uint8_t bufb[MB] __attribute__((aligned(32)));
static uint8_t bufc[MB] __attribute__((aligned(32)));

static inline uint64_t ns(void) { return timer_ns_gettime64(); }
static inline uint64_t cyc(void) { return perf_cntr_count(PRFC1); }
static unsigned us_of(uint64_t t) { return (unsigned)((t + 500u) / 1000u); }
static unsigned mbs10(uint64_t bytes, uint64_t t) { return t ? (unsigned)(bytes * 10000u / t) : 0u; }   /* 10 x (million bytes a second) */
static unsigned x100(uint64_t num, uint64_t den) { return den ? (unsigned)(num * 100u / den) : 0u; }

static void noise(uint8_t *p, unsigned n, uint32_t seed) {
    unsigned i;

    for (i = 0; i < n; i += 4u) { seed = seed * 1664525u + 1013904223u; memcpy(p + i, &seed, 4u); }
}

/* ---- group 1, the SH-4 ---------------------------------------------------------------- */

static void t_timer(void) {
    uint64_t a = ns(), b, least = ~0ull, c0, t0;
    unsigned i;

    for (i = 0; i < 20000u; i++) { b = ns(); if (b != a && b - a < least) least = b - a; a = b; }
    perf_cntr_start(PRFC1, PMCR_ELAPSED_TIME_MODE, PMCR_COUNT_CPU_CYCLES);
    c0 = cyc(); t0 = ns();
    while (ns() - t0 < 1000000u) { }
    say("timer: least step %u ns; 1 ms = %u cycles", (unsigned)least, (unsigned)(cyc() - c0));
}

/* One word of each line of `n` bytes at p, read. Returns their sum (so the reads are kept). */
static uint32_t __attribute__((noinline)) touch_read(const uint8_t *p, unsigned n) {
    uint32_t s = 0;
    unsigned i;

    for (i = 0; i < n; i += LINE) s += *(const uint32_t *)(const void *)(p + i);
    return s;
}

static void __attribute__((noinline)) touch_write(uint8_t *p, unsigned n) {
    unsigned i;

    for (i = 0; i < n; i += LINE) *(uint32_t *)(void *)(p + i) = i;
}

static void __attribute__((noinline)) touch_alloc(uint8_t *p, unsigned n) {
    unsigned i;

    for (i = 0; i < n; i += LINE) { dcache_alloc_line(p + i); *(uint32_t *)(void *)(p + i + 4u) = i; }
}

static volatile uint32_t sink;

static void t_cache(void) {
    uint64_t c0, c;
    unsigned i;

    touch_read(bufa, 8192u);
    c0 = cyc();
    for (i = 0; i < 200u; i++) sink += touch_read(bufa, 8192u);
    c = cyc() - c0;
    say("cache hit, 1 word a line, 8 KB: %u.%02u cyc/line", x100(c, 200u * 256u) / 100u, x100(c, 200u * 256u) % 100u);

    dcache_purge_range((uintptr_t)bufa, MB);
    c0 = cyc(); sink += touch_read(bufa, MB); c = cyc() - c0;
    say("cache miss read, 1 MB: %u.%02u cyc/line", x100(c, MB / LINE) / 100u, x100(c, MB / LINE) % 100u);

    dcache_purge_range((uintptr_t)bufa, MB);
    c0 = cyc(); touch_write(bufa, MB); c = cyc() - c0;
    say("cache miss write, 1 MB: %u.%02u cyc/line", x100(c, MB / LINE) / 100u, x100(c, MB / LINE) % 100u);

    dcache_purge_range((uintptr_t)bufa, MB);
    c0 = cyc(); touch_alloc(bufa, MB); c = cyc() - c0;
    say("allocate + write, 1 MB: %u.%02u cyc/line", x100(c, MB / LINE) / 100u, x100(c, MB / LINE) % 100u);

    touch_write(bufa, 8192u);
    c0 = cyc(); dcache_wback_range((uintptr_t)bufa, 8192u); c = cyc() - c0;
    say("write back 8 KB dirty: %u.%02u cyc/line", x100(c, 256u) / 100u, x100(c, 256u) % 100u);
    c0 = cyc(); dcache_purge_range((uintptr_t)bufa, 8192u); c = cyc() - c0;
    say("purge 8 KB clean: %u.%02u cyc/line", x100(c, 256u) / 100u, x100(c, 256u) % 100u);
}

static void t_copy(void) {
    uint64_t t0, t;
    unsigned i;

    noise(bufa, MB, 1u);
    t0 = ns(); memcpy(bufb, bufa, MB); t = ns() - t0;
    say("memcpy 1 MB RAM to RAM: %u us, %u.%u MB/s", us_of(t), mbs10(MB, t) / 10u, mbs10(MB, t) % 10u);
    t0 = ns(); sq_cpy(bufb, bufa, MB); t = ns() - t0;
    say("store queue 1 MB RAM to RAM: %u us, %u.%u MB/s", us_of(t), mbs10(MB, t) / 10u, mbs10(MB, t) % 10u);
    memcpy(bufb, bufa, 4096u);
    t0 = ns(); for (i = 0; i < 256u; i++) memcpy(bufb, bufa, 4096u); t = ns() - t0;
    say("memcpy 4 KB in the cache: %u.%u MB/s", mbs10(256u * 4096u, t) / 10u, mbs10(256u * 4096u, t) % 10u);
}

static void __attribute__((noinline)) plain_transform(const matrix_t *m, const vector_t *in, vector_t *out, unsigned n) {
    unsigned i;

    for (i = 0; i < n; i++) {
        const float x = in[i].x, y = in[i].y, z = in[i].z;
        const float w = 1.0f / ((*m)[0][3] * x + (*m)[1][3] * y + (*m)[2][3] * z + (*m)[3][3]);

        out[i].x = ((*m)[0][0] * x + (*m)[1][0] * y + (*m)[2][0] * z + (*m)[3][0]) * w;
        out[i].y = ((*m)[0][1] * x + (*m)[1][1] * y + (*m)[2][1] * z + (*m)[3][1]) * w;
        out[i].z = w;
    }
}

static void t_matrix(void) {
    static matrix_t m __attribute__((aligned(32))) = { { 0.8f, 0.1f, 0.0f, 0.0f }, { -0.1f, 0.8f, 0.2f, 0.0f }, { 0.0f, -0.2f, 0.9f, 0.01f }, { 3.0f, 4.0f, 5.0f, 1.0f } };
    vector_t *in = (vector_t *)(void *)bufa, *out = (vector_t *)(void *)bufb;
    uint64_t c0, c;
    unsigned i;

    for (i = 0; i < MB / sizeof(vector_t); i++) { in[i].x = (float)(i & 63u); in[i].y = (float)(i & 31u); in[i].z = 1.0f + (float)(i & 15u); in[i].w = 1.0f; }
    mat_load(&m);
    mat_transform(in, out, 256, sizeof(vector_t));
    c0 = cyc();
    for (i = 0; i < 256u; i++) mat_transform(in, out, 256, sizeof(vector_t));
    c = cyc() - c0;
    say("mat_transform, 256 in cache: %u.%02u cyc/vertex", x100(c, 65536u) / 100u, x100(c, 65536u) % 100u);
    plain_transform(&m, in, out, 256u);
    c0 = cyc();
    for (i = 0; i < 256u; i++) plain_transform(&m, in, out, 256u);
    c = cyc() - c0;
    say("plain C 4x4 + divide, same: %u.%02u cyc/vertex", x100(c, 65536u) / 100u, x100(c, 65536u) % 100u);
    dcache_purge_range((uintptr_t)bufa, MB);
    dcache_purge_range((uintptr_t)bufb, MB);
    c0 = cyc(); mat_transform(in, out, 32768, sizeof(vector_t)); c = cyc() - c0;
    say("mat_transform, 32768 not in cache: %u.%02u cyc/vertex", x100(c, 32768u) / 100u, x100(c, 32768u) % 100u);
}

/* The twiddled place of texel (x, y) in a square texture: y's bits in the even places, x's in the odd. */
static unsigned twiddle_index(unsigned x, unsigned y) {
    unsigned r = 0, b;

    for (b = 0; b < 10u; b++) r |= ((y >> b) & 1u) << (2u * b) | ((x >> b) & 1u) << (2u * b + 1u);
    return r;
}

static void t_twiddle(void) {
    static uint16_t spread[128];
    const uint16_t *src = (const uint16_t *)(const void *)bufa;
    uint16_t *d1 = (uint16_t *)(void *)bufb, *d2 = (uint16_t *)(void *)bufc;
    uint64_t t0, t1, t2;
    unsigned x, y;

    noise(bufa, 32768u, 7u);
    memset(bufb, 0, 32768u); memset(bufc, 0, 32768u);
    t0 = ns();
    for (y = 0; y < 128u; y++) for (x = 0; x < 128u; x++) d1[twiddle_index(x, y)] = src[y * 128u + x];
    t1 = ns();
    for (x = 0; x < 128u; x++) spread[x] = (uint16_t)twiddle_index(0u, x);
    for (y = 0; y < 128u; y++) {
        const unsigned row = spread[y];

        for (x = 0; x < 128u; x++) d2[row | ((unsigned)spread[x] << 1)] = src[y * 128u + x];
    }
    t2 = ns();
    say("twiddle 128x128 16-bit (32 KB): bit loop %u us, table %u us, %s", us_of(t1 - t0), us_of(t2 - t1), memcmp(d1, d2, 32768u) ? "DIFFER" : "same result");
}

/* ---- the PVR: passes ---------------------------------------------------------------------- */

static pvr_ptr_t tex256, pal256, a4_256, big1024, block512, yuv_mem;
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

enum { TX_NONE = 0, TX_565_TW, TX_565_RAW, TX_PAL8, TX_4444_RAW, TX_BIG_TW, TX_BIG_RAW };

typedef struct kind {
    const char *name;
    pvr_list_t list;
    int tex, bilinear, add;
} kind_t;

static void kind_header(pvr_poly_hdr_t *h, const kind_t *k) {
    const pvr_filter_mode_t f = k->bilinear ? PVR_FILTER_BILINEAR : PVR_FILTER_NEAREST;
    pvr_poly_cxt_t c;

    switch (k->tex) {
    case TX_565_TW: pvr_poly_cxt_txr(&c, k->list, PVR_TXRFMT_RGB565, 256, 256, tex256, f); break;
    case TX_565_RAW: pvr_poly_cxt_txr(&c, k->list, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, 256, 256, tex256, f); break;
    case TX_PAL8: pvr_poly_cxt_txr(&c, k->list, PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(0), 256, 256, pal256, f); break;
    case TX_4444_RAW: pvr_poly_cxt_txr(&c, k->list, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, 256, 256, a4_256, f); break;
    case TX_BIG_TW: pvr_poly_cxt_txr(&c, k->list, PVR_TXRFMT_RGB565, 1024, 1024, big1024, f); break;
    case TX_BIG_RAW: pvr_poly_cxt_txr(&c, k->list, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, 1024, 1024, big1024, f); break;
    default: pvr_poly_cxt_col(&c, k->list); break;
    }
    c.gen.culling = PVR_CULLING_NONE;
    if (k->add) { c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ONE; }
    pvr_poly_compile(h, &c);
}

static void quad(const pvr_poly_hdr_t *h, float x0, float y0, float x1, float y1, float z, float u1, float v1, uint32_t col) {
    pvr_vertex_t v;
    unsigned i;

    pvr_prim((void *)h, sizeof *h);
    memset(&v, 0, sizeof v);
    v.argb = col; v.z = z;
    for (i = 0; i < 4u; i++) {
        v.flags = i == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = (i & 2u) ? x1 : x0; v.y = (i & 1u) ? y0 : y1;
        v.u = (i & 2u) ? u1 : 0.0f; v.v = (i & 1u) ? 0.0f : v1;
        pvr_prim(&v, sizeof v);
    }
}

/* The render time, us, of a frame of `n` full-screen layers of kind k (NULL: none), texture stretched by (u1, v1).
   The least of two frames; 0 when a frame did not come back. */
static unsigned layers_us(const kind_t *k, unsigned n, float u1, float v1) {
    static const kind_t flat = { "", PVR_LIST_OP_POLY, TX_NONE, 0, 0 };
    pvr_poly_hdr_t h0, h;
    uint64_t least = ~0ull;
    unsigned r, i;

    kind_header(&h0, &flat);
    if (k) kind_header(&h, k);
    for (r = 0; r < 2u; r++) {
        sh4der_fault_beat(&crumbs);
        pvr_wait_ready();
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        quad(&h0, 0.0f, 0.0f, 2.0f, 2.0f, 0.5f, 0.0f, 0.0f, 0xff202020u);                /* the list is never empty */
        if (k && k->list == PVR_LIST_OP_POLY) for (i = 0; i < n; i++) quad(&h, 0.0f, 0.0f, 640.0f, 480.0f, 1.0f + 0.01f * (float)i, u1, v1, 0x80c0c0c0u);
        pvr_list_finish();
        if (k && k->list != PVR_LIST_OP_POLY) {
            pvr_list_begin(k->list);
            for (i = 0; i < n; i++) quad(&h, 0.0f, 0.0f, 640.0f, 480.0f, 1.0f + 0.01f * (float)i, u1, v1, 0x80c0c0c0u);
            pvr_list_finish();
        }
        if (!scene_close()) return 0;
        if (last_render_ns < least) least = last_render_ns;
    }
    return us_of(least);
}

static unsigned base_us;

static void t_fill(void) {
    static const kind_t kinds[] = {
        { "OP flat", PVR_LIST_OP_POLY, TX_NONE, 0, 0 },
        { "OP 565 twid near", PVR_LIST_OP_POLY, TX_565_TW, 0, 0 },
        { "OP 565 twid bilin", PVR_LIST_OP_POLY, TX_565_TW, 1, 0 },
        { "OP 565 raster near", PVR_LIST_OP_POLY, TX_565_RAW, 0, 0 },
        { "OP pal8 near", PVR_LIST_OP_POLY, TX_PAL8, 0, 0 },
        { "OP 4444 bilin", PVR_LIST_OP_POLY, TX_4444_RAW, 1, 0 },
        { "TR flat blend", PVR_LIST_TR_POLY, TX_NONE, 0, 0 },
        { "TR flat add", PVR_LIST_TR_POLY, TX_NONE, 0, 1 },
        { "TR 565 bilin blend", PVR_LIST_TR_POLY, TX_565_TW, 1, 0 },
        { "TR 4444 bilin blend", PVR_LIST_TR_POLY, TX_4444_RAW, 1, 0 },
        { "PT 4444 near", PVR_LIST_PT_POLY, TX_4444_RAW, 0, 0 },
    };
    unsigned i;

    base_us = layers_us(NULL, 0u, 0.0f, 0.0f);
    say("render us, 640x480 layers; empty frame %u", base_us);
    say("kind: first layer / each further layer");
    for (i = 0; i < sizeof kinds / sizeof kinds[0]; i++) {
        unsigned one, five;

        step(kinds[i].name);
        one = layers_us(&kinds[i], 1u, 2.5f, 1.875f);
        five = one ? layers_us(&kinds[i], 5u, 2.5f, 1.875f) : 0u;
        if (!one || !five) { say("%s: no render end", kinds[i].name); continue; }
        say("%s: %d / %d us", kinds[i].name, (int)one - (int)base_us, ((int)five - (int)one) / 4);
    }
}

static void t_local(void) {
    static const kind_t tw = { "big twid", PVR_LIST_OP_POLY, TX_BIG_TW, 0, 0 }, raw = { "big raster", PVR_LIST_OP_POLY, TX_BIG_RAW, 0, 0 };
    const kind_t *k[2] = { &tw, &raw };
    unsigned i;

    for (i = 0; i < 2u; i++) {
        const unsigned a = layers_us(k[i], 1u, 0.625f, 0.46875f), b = layers_us(k[i], 1u, 10.0f, 7.5f), c = layers_us(k[i], 1u, 0.0390625f, 0.029296875f);

        say("1024 txr %s: 1 texel a px %d, 16 a px %d, 1/16 a px %d us", i ? "raster" : "twiddled", (int)a - (int)base_us, (int)b - (int)base_us, (int)c - (int)base_us);
    }
}

/* 4080 small opaque (or 256 translucent) triangles, spread over the screen or all in the first tile. */
static unsigned tris_us(int one_tile, int translucent) {
    static const kind_t op = { "", PVR_LIST_OP_POLY, TX_NONE, 0, 0 }, tr = { "", PVR_LIST_TR_POLY, TX_NONE, 0, 0 };
    const unsigned n = translucent ? 256u : 4080u;
    pvr_poly_hdr_t h0, h;
    pvr_vertex_t v;
    unsigned i, k;

    sh4der_fault_beat(&crumbs);
    kind_header(&h0, &op);
    kind_header(&h, translucent ? &tr : &op);
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    quad(&h0, 0.0f, 0.0f, 2.0f, 2.0f, 0.5f, 0.0f, 0.0f, 0xff202020u);
    if (translucent) { pvr_list_finish(); pvr_list_begin(PVR_LIST_TR_POLY); }
    pvr_prim(&h, sizeof h);
    memset(&v, 0, sizeof v);
    v.argb = 0x40ffffffu;
    for (i = 0; i < n; i++) {
        const float s = one_tile ? 30.0f : 7.0f, x = one_tile ? 1.0f : 8.0f * (float)(i % 80u), y = one_tile ? 1.0f : 8.0f * (float)(i / 80u);

        v.z = 1.0f + 0.0001f * (float)i;
        for (k = 0; k < 3u; k++) {
            v.flags = k == 2u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            v.x = x + (k == 1u ? s : 0.0f); v.y = y + (k == 2u ? s : 0.0f);
            pvr_prim(&v, sizeof v);
        }
    }
    pvr_list_finish();
    return scene_close() ? us_of(last_render_ns) : 0u;
}

static void t_tiles(void) {
    const unsigned a = tris_us(0, 0), b = tris_us(1, 0), c = tris_us(0, 1), d = tris_us(1, 1);

    say("4080 opaque triangles: spread %d, in one tile %d us", (int)a - (int)base_us, (int)b - (int)base_us);
    say("256 translucent (sorted): spread %d, in one tile %d us", (int)c - (int)base_us, (int)d - (int)base_us);
}

#define SEND_VERTS 15000u

/* 15,000 vertices (5,000 small triangles) from RAM into the TA three ways. Times the sending only. */
static void t_vertices(void) {
    static const kind_t op = { "", PVR_LIST_OP_POLY, TX_NONE, 0, 0 };
    static const char *const how[3] = { "pvr_prim a vertex", "direct (pvr_dr) a vertex", "one store-queue run" };
    pvr_vertex_t *arr = (pvr_vertex_t *)(void *)bufa;
    pvr_poly_hdr_t h;
    uint32_t seed = 3u;
    unsigned i, m;

    kind_header(&h, &op);
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
        else if (m == 1u) for (i = 0; i < SEND_VERTS; i++) { pvr_vertex_t *o = pvr_dr_target(); *o = arr[i]; pvr_dr_commit(o); }
        else pvr_sq_load(NULL, arr, SEND_VERTS * sizeof arr[0], PVR_DMA_TA);
        t = ns() - t0;
        pvr_list_finish();
        if (!scene_close()) { say("%s: no render end", how[m]); continue; }
        say("TA, %s: %u us, %u ns/vertex, %u.%u MB/s", how[m], us_of(t), (unsigned)(t / SEND_VERTS), mbs10(SEND_VERTS * 32u, t) / 10u, mbs10(SEND_VERTS * 32u, t) % 10u);
    }
}

/* Channel 2 DMA into video memory through KallistiOS, the end polled with a limit. ns, 0 = refused or not ended. */
static uint64_t dma_upload(const void *src, pvr_ptr_t dst, unsigned bytes) {
    const uint64_t t0 = ns();
    uint64_t t;

    if (pvr_txr_load_dma(src, dst, bytes, false, NULL, NULL) < 0) return 0;
    do { t = ns() - t0; } while (!pvr_dma_ready() && t < 200000000ull);
    return pvr_dma_ready() ? t : 0;
}

static void t_upload(void) {
    static const unsigned sizes[2] = { 32768u, 524288u };
    unsigned i;

    noise(bufa, 524288u, 11u);
    for (i = 0; i < 2u; i++) {
        uint64_t t0, t;

        t0 = ns(); pvr_txr_load(bufa, block512, sizes[i]); t = ns() - t0;
        say("upload %u KB, store queue: %u us, %u.%u MB/s", sizes[i] / 1024u, us_of(t), mbs10(sizes[i], t) / 10u, mbs10(sizes[i], t) % 10u);
        dcache_wback_range((uintptr_t)bufa, sizes[i]);
        t = dma_upload(bufa, block512, sizes[i]);
        if (!t) say("upload %u KB, channel 2 DMA: refused or no end", sizes[i] / 1024u);
        else say("upload %u KB, channel 2 DMA: %u us, %u.%u MB/s", sizes[i] / 1024u, us_of(t), mbs10(sizes[i], t) / 10u, mbs10(sizes[i], t) % 10u);
    }
}

/* ---- group 2: DMA and the bus --------------------------------------------------------------- */

#define REG32(a) (*(volatile uint32_t *)(a))
#define SB_ISTNRM   REG32(0xa05f6900)
#define SB_PDSTAP   REG32(0xa05f7c00)
#define SB_PDSTAR   REG32(0xa05f7c04)
#define SB_PDLEN    REG32(0xa05f7c08)
#define SB_PDDIR    REG32(0xa05f7c0c)
#define SB_PDTSEL   REG32(0xa05f7c10)
#define SB_PDEN     REG32(0xa05f7c14)
#define SB_PDST     REG32(0xa05f7c18)
#define SB_PDAPRO   REG32(0xa05f7c80)
#define ISTNRM_PVR_DMA (1u << 11)
#define LIMIT_NS 200000000ull              /* no wait in this file is longer */

/* PVR-DMA, video memory to RAM (the route AB spike's path): start it; pd_busy says when it has ended. */
static void pd_start(pvr_ptr_t mem, void *dst, uint32_t bytes) {
    dcache_purge_range((uintptr_t)dst, bytes);
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

static int pd_busy(void) { return (SB_PDST & 1u) != 0; }

static void pd_end(void) {
    if (pd_busy()) SB_PDST = 0;
    SB_PDEN = 0;
    SB_ISTNRM = ISTNRM_PVR_DMA;
}

static void t_pvrdma(void) {
    static const unsigned sizes[3] = { 32768u, 131072u, 524288u };
    unsigned i;

    for (i = 0; i < 3u; i++) {
        uint64_t t0, t;

        pd_start(block512, bufb, sizes[i]);
        t0 = ns();
        do { t = ns() - t0; } while (pd_busy() && t < LIMIT_NS);
        if (pd_busy()) say("PVR-DMA download %u KB: no end", sizes[i] / 1024u);
        else say("PVR-DMA download %u KB: %u us, %u.%u MB/s", sizes[i] / 1024u, us_of(t), mbs10(sizes[i], t) / 10u, mbs10(sizes[i], t) % 10u);
        pd_end();
    }
}

static dma_config_t dmac_cfg[4];
static dma_channel_t dmac_ch;

static int dmac_busy(void) { return dma_is_running(dmac_ch); }

/* Start a RAM to RAM copy of `bytes` from bufa to bufb on a DMAC channel, by auto-request. 0 when refused. */
static int dmac_start(dma_channel_t ch, dma_transmitmode_t mode, unsigned bytes) {
    dma_config_t *cfg = &dmac_cfg[ch];

    if (dma_is_running(ch)) dma_transfer_abort(ch);              /* dma_transfer would wait for it without a limit */
    cfg->channel = ch; cfg->request = DMA_REQUEST_AUTO_MEM_TO_MEM; cfg->unit_size = DMA_UNITSIZE_32BYTE;
    cfg->src_mode = DMA_ADDRMODE_INCREMENT; cfg->dst_mode = DMA_ADDRMODE_INCREMENT; cfg->transmit_mode = mode; cfg->callback = NULL;
    dmac_ch = ch;
    return dma_transfer(cfg, dma_map_dst(bufb, bytes), dma_map_src(bufa, bytes), bytes, NULL) >= 0;
}

static void t_dmac(void) {
    static const struct { dma_channel_t ch; dma_transmitmode_t mode; const char *name; } v[4] = {
        { DMA_CHANNEL_1, DMA_TRANSMITMODE_BURST, "ch1 burst" }, { DMA_CHANNEL_1, DMA_TRANSMITMODE_CYCLE_STEAL, "ch1 cycle steal" },
        { DMA_CHANNEL_3, DMA_TRANSMITMODE_BURST, "ch3 burst" }, { DMA_CHANNEL_3, DMA_TRANSMITMODE_CYCLE_STEAL, "ch3 cycle steal" } };
    unsigned i;

    say("DMAOR reads %04x", (unsigned)(REG32(0xffa00040) & 0xffffu));
    for (i = 0; i < 4u; i++) {
        uint64_t t0, t;

        step(v[i].name);
        noise(bufa, 524288u, 20u + i);
        memset(bufb, 0, 524288u);
        t0 = ns();
        if (!dmac_start(v[i].ch, v[i].mode, 524288u)) { say("DMAC %s: refused", v[i].name); continue; }
        do { t = ns() - t0; } while (dmac_busy() && t < LIMIT_NS);
        if (dmac_busy()) { dma_transfer_abort(v[i].ch); say("DMAC %s: no end in 200 ms", v[i].name); continue; }
        dcache_inval_range((uintptr_t)bufb, 524288u);
        say("DMAC %s, 512 KB RAM to RAM: %u us, %u.%u MB/s, copy %s", v[i].name, us_of(t), mbs10(524288u, t) / 10u, mbs10(524288u, t) % 10u,
            memcmp(bufa, bufb, 524288u) ? "WRONG" : "right");
    }
}

typedef int (*busy_fn)(void);

/* Count work on the SH-4 while `busy` says so (or, with no busy, for `window` ns), never past the limit. Work is 256
   register-only steps, or 256 reads of RAM that each miss the cache. Returns the steps a millisecond; *took the time. */
static unsigned spin(int ram, busy_fn busy, uint64_t window, uint64_t *took) {
    const uint64_t t0 = ns();
    uint64_t t, n = 0;
    uint32_t x = 1u, pos = 0u, s = 0u;
    unsigned i;

    do {
        if (ram) for (i = 0; i < 256u; i++) { s += *(const uint32_t *)(const void *)(bufc + pos); pos = (pos + 4128u) & (MB - 32u); }
        else for (i = 0; i < 256u; i++) x = x * 1664525u + 1013904223u;
        n += 256u;
        t = ns() - t0;
    } while (t < LIMIT_NS && (busy ? busy() : t < window));
    sink += x + s;
    if (took) *took = t;
    return t ? (unsigned)(n * 1000000u / t) : 0u;
}

static unsigned alone[2];

static void say_beside(const char *what, int ram, unsigned rate, uint64_t took) {
    say("%s, %s loop: %u /ms (%u%% of alone), took %u us", what, ram ? "RAM-miss" : "register", rate, alone[ram] ? rate * 100u / alone[ram] : 0u, us_of(took));
}

static void t_beside(void) {
    static const kind_t tr = { "", PVR_LIST_TR_POLY, TX_NONE, 0, 0 }, op = { "", PVR_LIST_OP_POLY, TX_NONE, 0, 0 };
    pvr_poly_hdr_t h0, h;
    uint64_t took;
    unsigned ram, rate, i, t;

    alone[0] = spin(0, NULL, 10000000u, NULL);
    alone[1] = spin(1, NULL, 10000000u, NULL);
    say("SH-4 loop alone: register %u /ms, RAM-miss %u /ms", alone[0], alone[1]);
    for (ram = 0; ram < 2u; ram++) {
        step("beside PVR-DMA");
        pd_start(block512, bufb, 524288u);
        rate = spin((int)ram, pd_busy, 0u, &took);
        pd_end();
        say_beside("PVR-DMA 512 KB", (int)ram, rate, took);

        step("beside DMAC burst");
        if (dmac_start(DMA_CHANNEL_1, DMA_TRANSMITMODE_BURST, 524288u)) {
            rate = spin((int)ram, dmac_busy, 0u, &took);
            if (dmac_busy()) dma_transfer_abort(DMA_CHANNEL_1);
            say_beside("DMAC ch1 burst 512 KB", (int)ram, rate, took);
        }
        step("beside DMAC steal");
        if (dmac_start(DMA_CHANNEL_1, DMA_TRANSMITMODE_CYCLE_STEAL, 524288u)) {
            rate = spin((int)ram, dmac_busy, 0u, &took);
            if (dmac_busy()) dma_transfer_abort(DMA_CHANNEL_1);
            say_beside("DMAC ch1 cycle steal 512 KB", (int)ram, rate, took);
        }

        step("beside render");
        kind_header(&h0, &op);
        kind_header(&h, &tr);
        pvr_wait_ready();
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        quad(&h0, 0.0f, 0.0f, 2.0f, 2.0f, 0.5f, 0.0f, 0.0f, 0xff202020u);
        pvr_list_finish();
        pvr_list_begin(PVR_LIST_TR_POLY);
        for (i = 0; i < 8u; i++) quad(&h, 0.0f, 0.0f, 640.0f, 480.0f, 1.0f + 0.01f * (float)i, 0.0f, 0.0f, 0x80c0c0c0u);
        pvr_list_finish();
        pvr_scene_finish();
        for (t = 0; t < 20u && pvr_wait_ready() < 0; t++) sh4der_fault_beat(&crumbs);     /* the render has begun */
        rate = spin((int)ram, NULL, 15000000u, &took);
        for (t = 0; t < 20u && pvr_wait_render_done() < 0; t++) sh4der_fault_beat(&crumbs);
        say_beside("PVR render, 8 layers", (int)ram, rate, took);
    }
}

/* The YUV converter: 256 macroblocks (a 256 x 256 picture) of YUV 4:2:0 through the store queues into a texture. */
static void t_yuv(void) {
    const volatile uint32_t *out = (const volatile uint32_t *)yuv_mem;
    volatile uint32_t *clr = (volatile uint32_t *)yuv_mem;
    uint64_t t0, t;
    unsigned i;

    for (i = 0; i < 256u; i++) { memset(bufa + 384u * i, 64, 64u); memset(bufa + 384u * i + 64u, 192, 64u); memset(bufa + 384u * i + 128u, 128, 256u); }
    for (i = 0; i < 131072u / 4u; i++) clr[i] = 0x12345678u;
    PVR_SET(PVR_YUV_ADDR, (uint32_t)(uintptr_t)yuv_mem & 0x00ffffffu);
    PVR_SET(PVR_YUV_CFG, (15u << 8) | 15u);
    (void)PVR_GET(PVR_YUV_CFG);
    t0 = ns();
    pvr_sq_load(NULL, bufa, 256u * 384u, PVR_DMA_YUV);
    t = ns() - t0;
    thd_sleep(20);
    say("YUV converter, 256 blocks (96 KB in): %u us, %u.%u MB/s in", us_of(t), mbs10(256u * 384u, t) / 10u, mbs10(256u * 384u, t) % 10u);
    say("   blocks counted %u; first words %08x %08x", (unsigned)(PVR_GET(PVR_YUV_STAT) & 0x1fffu), (unsigned)out[0], (unsigned)out[1]);
    for (i = 0; i < 131072u / 4u && out[i] != 0x12345678u; i++) { }
    say("   words written %u of 32768 (U 64, V 192, Y 128 went in)", i);
}

/* ---- group 3: the operand cache as RAM -------------------------------------------------------- */

#define OCRAM ((volatile uint8_t *)0x7c001000u)                  /* where KallistiOS's linker script puts it */
#define CCR REG32(0xff00001c)

static unsigned random_reads(const volatile uint8_t *base, unsigned mask) {
    uint64_t t0 = ns();
    uint32_t x = 9u, s = 0;
    unsigned i;

    for (i = 0; i < 20000u; i++) { x = x * 1664525u + 1013904223u; s += *(const volatile uint32_t *)(const void *)(base + ((x >> 8) & mask & ~3u)); }
    sink += s;
    return us_of(ns() - t0);
}

static void t_ocram(void) {
    unsigned i, hold = 0, hot, cold, oc, hot2;

    touch_read(bufa, 8192u);
    hot = random_reads(bufa, 8191u);
    cold = random_reads(bufc, MB - 1u);
    say("20000 random reads: 8 KB of RAM in cache %u us, 1 MB of RAM %u us", hot, cold);
    say("CCR before %08x; switching OC-RAM on (stays on)", (unsigned)CCR);
    dcache_toggle_ocram(true);
    say("CCR now %08x", (unsigned)CCR);
    step("ocram use");
    for (i = 0; i < 8192u; i++) OCRAM[i] = (uint8_t)(i * 37u + (i >> 8));
    for (i = 0; i < 8192u; i++) hold += OCRAM[i] == (uint8_t)(i * 37u + (i >> 8));
    say("OC-RAM at 7c001000: %u of 8192 bytes hold", hold);
    oc = random_reads(OCRAM, 8191u);
    touch_read(bufa, 8192u);
    hot2 = random_reads(bufa, 8191u);
    say("20000 random reads: OC-RAM %u us; the 8 KB of RAM now %u us", oc, hot2);
    say("(with OC-RAM on the cache for RAM is 8 KB)");
    step("ocram alias");
    OCRAM[0] = 0xa5u; OCRAM[4096] = 0x5au;
    say("7c000000 reads %02x, 7c003000 reads %02x (a5 / 5a = the same RAM again)", (unsigned)*(volatile uint8_t *)0x7c000000u, (unsigned)*(volatile uint8_t *)0x7c003000u);
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

static int pvr_start(int big) {
    pvr_init_params_t init = pvr_default_params;
    unsigned i;

    init.opb_sizes[PVR_LIST_OP_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_OP_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_TR_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_PT_POLY] = PVR_BINSIZE_16;
    init.vertex_buf_size = big ? 1024 * 1024 : 256 * 1024;
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return 0; }
    pvr_set_bg_color(0.0f, 0.0f, 0.2f);
    block512 = pvr_mem_malloc(524288u);
    if (big) {
        tex256 = pvr_mem_malloc(131072u); pal256 = pvr_mem_malloc(65536u); a4_256 = pvr_mem_malloc(131072u); big1024 = pvr_mem_malloc(2u * MB);
        if (!block512 || !tex256 || !pal256 || !a4_256 || !big1024) { say("video memory allocation FAIL"); return 0; }
        noise(bufa, MB, 5u);
        pvr_txr_load(bufa, tex256, 131072u);
        pvr_txr_load(bufa, pal256, 65536u);
        pvr_txr_load(bufa, big1024, MB);
        pvr_txr_load(bufa, (uint8_t *)big1024 + MB, MB);
        for (i = 1; i < 131072u; i += 2u) bufa[i] |= 0xf0u;       /* 4444 with every alpha full */
        pvr_txr_load(bufa, a4_256, 131072u);
        pvr_set_pal_format(PVR_PAL_ARGB8888);
        for (i = 0; i < 256u; i++) pvr_set_pal_entry(i, 0xff000000u | (i << 16) | (i << 8) | i);
    } else {
        yuv_mem = pvr_mem_malloc(131072u);
        if (!block512 || !yuv_mem) { say("video memory allocation FAIL"); return 0; }
        noise(bufa, 524288u, 6u);
        pvr_txr_load(bufa, block512, 524288u);
    }
    return 1;
}

static void group1(void) {
    static const test_t t[] = {
        { "timer", t_timer }, { "cache", t_cache }, { "copy", t_copy }, { "matrix", t_matrix }, { "twiddle", t_twiddle },
        { "upload", t_upload }, { "fill", t_fill }, { "locality", t_local }, { "tiles", t_tiles }, { "vertices", t_vertices } };

    say("GROUP 1: SH-4 and PVR. One run, this console.");
    if (!pvr_start(1)) return;
    if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused");
    run_tests(t, sizeof t / sizeof t[0]);
    step("group 1 end");
    pvr_shutdown();
}

static void group2(void) {
    static const test_t t[] = { { "pvr-dma", t_pvrdma }, { "dmac", t_dmac }, { "beside", t_beside }, { "yuv", t_yuv } };

    say("GROUP 2: DMA and the bus");
    if (!pvr_start(0)) return;
    run_tests(t, sizeof t / sizeof t[0]);
    step("group 2 end");
    pvr_shutdown();
}

static void group3(void) {
    say("GROUP 3: the operand cache as RAM");
    step("ocram");
    t_ocram();
    step("group 3 end");
}

static void cont_reset_btn_callback_(uint8_t addr, uint32_t btns) {
    (void)addr;
    (void)btns;
    arch_exit();
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    cont_btn_callback(0, CONT_RESET_BUTTONS, cont_reset_btn_callback_);
    static const char *const next[4] = { "START: DMA, bus", "START: OC-RAM", "(end)", "(end)" };
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
        else if ((hit & CONT_START) && done < 3u) {
            const unsigned first = nlines;

            if (done == 1u) group2(); else group3();
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
