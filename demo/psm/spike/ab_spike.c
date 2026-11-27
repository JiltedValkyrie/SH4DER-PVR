/* SH4DER-PVR: route AB spike, the Dreamcast test program

   demo/psm/spike/ab_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A test program for a real console, not an effect. It asks the hardware about route AB (demo/psm/ROUTE_AB_NOTES.md:
   the PVR draws the casters' depth into a render target as a colour code, the picture is copied back to RAM, the
   SH-4 compares per vertex) and prints what it measured on result pages (and on the serial line, "ab:" lines).

   s2 and s3 on a console (ROUTE_AB_NOTES.md, "Hardware result s2" and "s3") settled the ramp codes, the dither
   switch, the download and the 16 pixels a render holds back on its last row. s3's cost lines were at fault in two
   places (the "send" time held the test's own trigonometry; the "PVR render" time was KallistiOS's scheduler tick).
   s4 measures those two properly and asks how the download compares with the SH-4's own ways of copying:

     0 SANITY a plain magenta quad on green, over a target the SH-4 filled with 0x1234 first.
     E EDGE   the held-back pixels with one and with two spare rows (129 and 130 rows).
     S SEND   960 ready vertices (920 triangles) sent three ways: pvr_prim a vertex, the store queues a vertex,
              one store-queue copy of the array. The vertices are built before the clock starts.
     T RENDER the PVR's render time from pvr_get_stats (stamped in the interrupts): an empty pass, a small ball and
              a large one, at 128 and at 256.
     B COPY   32 KB and 128 KB out of video memory: the PVR-DMA download, memcpy, loops of 16, 32 and 64 bits a
              read through the uncached window, and reads through the cached mirror (lines invalidated first), with
              and without a prefetch. Every copy is compared with the DMA's.
     K CACHE  whether the cached mirror is ever stale after an invalidate, and that it is stale without one.

   The PVR-DMA download. Register names and the direction bit are from the Flycast source read as documentation of
   the register block (core/hw/holly/sb.h, core/hw/pvr/pvr_sb_regs.cpp: SB_PDDIR set is "PVR -> System"); the area
   addresses from Sega's 1999 "SH4 access" table. No code is copied from either. It is not the DMA KallistiOS calls
   pvr_dma (the 0x005F6800 block, memory to the tile accelerator on SH-4 channel 2); it is the 0x005F7C00 block on
   SH-4 channel 0 in DDT mode. On the console (s2) it ran with DMAOR = 0x8201 as the BIOS leaves it, SB_PDAPRO =
   0x6702007F, the PVR address in either area form, and left CHCR0 = 0x000853C3 (burst, 32-byte units).

   Every number on the result pages comes from the run that prints it. Only a real console's run is evidence.

   KallistiOS paths: pvr_init, pvr_mem_malloc, pvr_txr_load, pvr_scene_begin_rtt, pvr_scene_begin, pvr_prim,
   pvr_dr_target / pvr_dr_commit, pvr_sq_load, pvr_wait_ready, pvr_wait_render_done, pvr_get_stats,
   vid_set_dithering, timer_ns_gettime64, perf_cntr_start / perf_cntr_count (PRFC1), dcache_purge_range /
   dcache_inval_range, bfont, maple.
   Build: python demo/psm/spike/build.py (it refuses a KallistiOS without pvr_scene_begin_rtt). No SH4ZAM, no library
   code: only ab_model.h.
*/

#include <kos.h>
#include <math.h>
#include <stdarg.h>
#include "ab_model.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

/* ---- registers (plain pokes) ---------------------------------------------------- */

#define REG32(a) (*(volatile uint32_t *)(a))
#define SB_ISTNRM   REG32(0xa05f6900)      /* normal interrupt status; write 1 to clear a bit */
#define SB_PDSTAP   REG32(0xa05f7c00)      /* PVR-DMA: PVR start address */
#define SB_PDSTAR   REG32(0xa05f7c04)      /* PVR-DMA: system memory start address */
#define SB_PDLEN    REG32(0xa05f7c08)      /* PVR-DMA: length in bytes, a multiple of 32 */
#define SB_PDDIR    REG32(0xa05f7c0c)      /* PVR-DMA: direction, 1 = PVR to system memory */
#define SB_PDTSEL   REG32(0xa05f7c10)      /* PVR-DMA: trigger, 0 = a write to SB_PDST */
#define SB_PDEN     REG32(0xa05f7c14)      /* PVR-DMA: enable */
#define SB_PDST     REG32(0xa05f7c18)      /* PVR-DMA: start; reads 1 while it runs */
#define SB_PDAPRO   REG32(0xa05f7c80)      /* PVR-DMA: system memory range allowed (write only) */
#define SB_PDSTAPD  REG32(0xa05f7cf0)      /* PVR-DMA: PVR address counter */
#define SB_PDSTARD  REG32(0xa05f7cf4)      /* PVR-DMA: system address counter */
#define SB_PDLEND   REG32(0xa05f7cf8)      /* PVR-DMA: transfer counter */
#define ISTNRM_PVR_DMA (1u << 11)          /* "end of PVR-DMA" in SB_ISTNRM */
#define DMAC_DMATCR0 REG32(0xffa00008)
#define DMAC_CHCR0  REG32(0xffa0000c)
#define DMAC_DMAOR  REG32(0xffa00040)
#define DMAOR_WANT  0x8201u                /* DDT, priority 2-0-1-3, master enable: what the BIOS leaves */

#define PD_TIMEOUT_NS 50000000ull
#define BIG_BYTES (512u * 1024u)           /* a 512 x 512 target, and the largest download */
#define TALL 160u                          /* rows allocated for the 128-wide target: the edge test draws up to this many */
#define MAP_BYTES (AB_SIZE * AB_SIZE * 2u)

/* ---- the result pages ----------------------------------------------------------- */

#define COLS 50u
#define ROWS 17u
#define LINES_MAX 200u
static char lines[LINES_MAX][COLS + 1u];
static unsigned nlines;

static void say(const char *fmt, ...) {
    char big[160];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(big, sizeof big, fmt, ap);
    va_end(ap);
    printf("ab: %s\n", big);
    if (nlines < LINES_MAX) { memcpy(lines[nlines], big, COLS); lines[nlines][COLS] = 0; nlines++; }
}

static void page_draw(unsigned page, unsigned pages) {
    char head[96];
    unsigned i;

    vid_clear(0, 0, 0);
    snprintf(head, sizeof head, "AB SPIKE s4 p%u/%u  A next B back START part 2", page + 1u, pages);
    head[COLS] = 0;
    bfont_draw_str(vram_s + 16u * 640u + 20u, 640, true, head);
    for (i = 0; i < ROWS && page * ROWS + i < nlines; i++)
        bfont_draw_str(vram_s + (16u + 24u * (i + 2u)) * 640u + 20u, 640, true, lines[page * ROWS + i]);
}

/* ---- state ---------------------------------------------------------------------- */

#define PREFILL 0x1234u                    /* put in the target by the SH-4 before a pass: a pixel still holding it was never written */
#define MAGENTA 0xf81fu
#define GREEN   0x07e0u
#define RED     0xf800u
#define BLUE    0x001fu

static pvr_ptr_t target_mem, ramp_mem[2], big_mem;
static pvr_poly_hdr_t hdr[2], hdr_col;
static struct {                            /* the last pass */
    int rtt_rc;                            /* pvr_scene_begin_rtt's return (0 for a screen pass) */
    int ready, done;                       /* the render started (pvr_wait_ready), the render ended (pvr_wait_render_done) */
    unsigned ready_tries, done_tries;      /* waits of up to 100 ms each that timed out first */
    uint32_t render_addr;                  /* PVR_RENDER_ADDR as read back after the render */
    uint64_t t0, submit_ns, render_ns;     /* list open to scene finish; render start to render-done, as this thread saw them */
} last;

static uint16_t ramp_px[AB_RAMP_W * AB_RAMP16_H] __attribute__((aligned(32)));
static uint16_t cpu_img[AB_SIZE * TALL] __attribute__((aligned(32)));         /* the target as the SH-4 read it */
static uint8_t big_dst[BIG_BYTES] __attribute__((aligned(32)));
static char verdict[COLS + 1u];
static int target_ok;                      /* the plain picture came back as drawn (pixels held back apart) */
static volatile unsigned sink;

/* two clocks: the KOS ns timer and the SH-4 performance counter 1 counting CPU cycles */
static inline uint64_t cyc(void) { return perf_cntr_count(PRFC1); }

/* ---- passes --------------------------------------------------------------------- */

static int load(void) {
    pvr_poly_cxt_t c;
    unsigned b;

    target_mem = pvr_mem_malloc(AB_SIZE * TALL * 2u);
    big_mem = pvr_mem_malloc(BIG_BYTES);
    for (b = 0; b < 2u; b++) {
        const int bits = b ? 16 : 10;
        const unsigned h = ab_ramp_fill(bits, ramp_px);

        ramp_mem[b] = pvr_mem_malloc(AB_RAMP_W * h * 2u);
        if (!ramp_mem[b]) return 0;
        pvr_txr_load(ramp_px, ramp_mem[b], AB_RAMP_W * h * 2u);
        pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)AB_RAMP_W, (int)h, ramp_mem[b], PVR_FILTER_NEAREST);
        c.txr.env = PVR_TXRENV_REPLACE;                          /* the texel as it is: no multiply by the vertex colour */
        c.txr.uv_clamp = b ? PVR_UVCLAMP_U : PVR_UVCLAMP_UV;     /* 16 bits: u clamps (the high bits), v wraps (the low bits) */
        c.gen.culling = PVR_CULLING_NONE;
        c.gen.shading = PVR_SHADE_FLAT;
        pvr_poly_compile(&hdr[b], &c);
    }
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                      /* plain colour, Gouraud: the sanity quad and the screen pass */
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&hdr_col, &c);
    return target_mem && big_mem;
}

static void prefill(unsigned value) {
    volatile uint16_t *dst = (volatile uint16_t *)target_mem;
    unsigned i;

    for (i = 0; i < AB_SIZE * TALL; i++) dst[i] = (uint16_t)value;
}

/* Open a pass into a render target w x h at `mem` (row stride w), or into the screen when mem is NULL. 0 when
   KallistiOS refused it. */
static int pass_begin(pvr_ptr_t mem, unsigned w, unsigned h) {
    memset(&last, 0, sizeof last);
    pvr_wait_ready();
    last.t0 = timer_ns_gettime64();
    if (mem) last.rtt_rc = pvr_scene_begin_rtt(mem, w, h, w);
    else pvr_scene_begin();
    if (last.rtt_rc < 0) return 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    return 1;
}

/* Close the pass and wait: for the render to start (KallistiOS starts it from an interrupt once the list is in), then
   for the PVR's render-done interrupt. Only then is the target whole. 1 when both waits ended. */
static int pass_end(void) {
    uint64_t t;

    pvr_list_finish();
    pvr_scene_finish();
    last.submit_ns = timer_ns_gettime64() - last.t0;
    for (last.ready_tries = 0; last.ready_tries < 10u && pvr_wait_ready() < 0; last.ready_tries++) {}
    last.ready = last.ready_tries < 10u;
    t = timer_ns_gettime64();
    if (last.ready) {
        for (last.done_tries = 0; last.done_tries < 10u && pvr_wait_render_done() < 0; last.done_tries++) {}
        last.done = last.done_tries < 10u;
    }
    last.render_ns = timer_ns_gettime64() - t;
    last.render_addr = PVR_GET(PVR_RENDER_ADDR);
    return last.ready && last.done;
}

static void pass_say(void) {
    say(" rtt rc %d start %d(%u) end %d(%u) RADDR %08x", last.rtt_rc, last.ready, last.ready_tries, last.done, last.done_tries, (unsigned)last.render_addr);
}

/* A rectangle of one colour (or a Gouraud run from c0 at the left to c1 at the right) at depth 1. */
static void quad_col(float x0, float y0, float x1, float y1, uint32_t c0, uint32_t c1) {
    pvr_vertex_t v;
    unsigned i;

    pvr_prim(&hdr_col, sizeof hdr_col);
    memset(&v, 0, sizeof v);
    for (i = 0; i < 4u; i++) {
        v.flags = i == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = (i & 2u) ? x1 : x0; v.y = (i & 1u) ? y0 : y1; v.z = 1.0f;
        v.argb = (i & 2u) ? c1 : c0;
        pvr_prim(&v, sizeof v);
    }
}

/* `rows` rows of the 128-wide target by the SH-4, 16 bits a read, through the uncached window the pointer is in. */
static void cpu_read(unsigned rows) {
    const volatile uint16_t *src = (const volatile uint16_t *)target_mem;
    unsigned i;

    for (i = 0; i < AB_SIZE * rows; i++) cpu_img[i] = src[i];
}

/* ---- the PVR-DMA download ------------------------------------------------------- */

typedef struct pd_result {
    int done;                              /* SB_PDST came back to 0 inside the timeout */
    int irq;                               /* the end-of-PVR-DMA status bit was set */
    uint64_t ns, cycles;
    uint32_t pdst, lend, stapd, stard, chcr0, tcr0;   /* after: for a download that did not finish */
} pd_result_t;

/* PVR-DMA, PVR to system memory: `bytes` (a multiple of 32) from the PVR address `pvr_addr` to `dst` (32-byte
   aligned RAM). The destination's cache lines are written back and dropped first, and dropped again after, so what
   is read afterwards is what the DMA put in RAM. Interrupts stay on (the timer needs them), so one run's time may hold
   an interrupt: the caller repeats. The end is seen by polling SB_PDST, not by an interrupt. */
static void pd_download(uint32_t pvr_addr, void *dst, uint32_t bytes, pd_result_t *res) {
    uint64_t t0, t1, c0;

    memset(res, 0, sizeof *res);
    dcache_purge_range((uintptr_t)dst, bytes);
    SB_PDEN = 0;
    SB_ISTNRM = ISTNRM_PVR_DMA;
    SB_PDAPRO = 0x6702007fu;
    SB_PDSTAP = pvr_addr;
    SB_PDSTAR = (uint32_t)(uintptr_t)dst & 0x1fffffe0u;
    SB_PDLEN = bytes;
    SB_PDDIR = 1;
    SB_PDTSEL = 0;
    SB_PDEN = 1;
    t0 = timer_ns_gettime64();
    c0 = cyc();
    SB_PDST = 1;
    do { t1 = timer_ns_gettime64(); } while ((SB_PDST & 1u) && t1 - t0 < PD_TIMEOUT_NS);
    res->cycles = cyc() - c0;
    res->ns = t1 - t0;
    res->pdst = SB_PDST; res->lend = SB_PDLEND; res->stapd = SB_PDSTAPD; res->stard = SB_PDSTARD;
    res->chcr0 = DMAC_CHCR0; res->tcr0 = DMAC_DMATCR0;
    res->done = !(res->pdst & 1u);
    res->irq = (SB_ISTNRM & ISTNRM_PVR_DMA) != 0;
    if (!res->done) SB_PDST = 0;
    SB_PDEN = 0;
    SB_ISTNRM = ISTNRM_PVR_DMA;
    dcache_inval_range((uintptr_t)dst, bytes);
}

/* The PVR address of video memory at `mem` for the download: the 64-bit area form (s2: both forms gave the target). */
static uint32_t pd_addr(pvr_ptr_t mem) { return 0x04000000u | ((uint32_t)(uintptr_t)mem & 0x00ffffffu); }

/* Smallest, largest and mean of `runs` downloads of `bytes` from `mem` into `dst`. Returns the runs that finished. */
static unsigned pd_spread(pvr_ptr_t mem, void *dst, uint32_t bytes, unsigned runs, uint64_t *lo, uint64_t *hi, uint64_t *mean) {
    pd_result_t r;
    uint64_t sum = 0;
    unsigned n, ok = 0;

    *lo = ~0ull; *hi = 0;
    for (n = 0; n < runs; n++) {
        pd_download(pd_addr(mem), dst, bytes, &r);
        if (!r.done) continue;
        ok++; sum += r.ns;
        if (r.ns < *lo) *lo = r.ns;
        if (r.ns > *hi) *hi = r.ns;
    }
    *mean = ok ? sum / ok : 0;
    return ok;
}

/* ---- the clocks ----------------------------------------------------------------- */

static void clocks(void) {
    uint64_t t0, t1, c0, step = ~0ull;
    unsigned i, steps = 0;

    perf_cntr_start(PRFC1, PMCR_ELAPSED_TIME_MODE, PMCR_COUNT_CPU_CYCLES);
    t0 = timer_ns_gettime64();
    for (i = 0; i < 20000u; i++) {                               /* the smallest step the ns timer shows */
        t1 = timer_ns_gettime64();
        if (t1 != t0) { steps++; if (t1 - t0 < step) step = t1 - t0; }
        t0 = t1;
    }
    t0 = timer_ns_gettime64(); c0 = cyc();                       /* the two clocks against each other: 200,000 on a 200 MHz SH-4 */
    do { t1 = timer_ns_gettime64(); } while (t1 - t0 < 1000000ull);
    say("timer step %lu ns, 1 ms = %lu CPU cycles", steps ? (unsigned long)step : 0ul, (unsigned long)(cyc() - c0));
}

/* ---- 0. sanity ------------------------------------------------------------------- */

/* The first place in a window of video memory (8 MB, 16 bits a read) that holds `value` in 8 words running: its
   byte offset, or -1. About two seconds a window on a console; only run on a failure. */
static long find_run(uint32_t window, unsigned value) {
    const volatile uint16_t *v = (const volatile uint16_t *)(uintptr_t)window;
    unsigned i, run = 0;

    for (i = 0; i < 0x400000u; i++) {
        run = v[i] == value ? run + 1u : 0u;
        if (run == 8u) return (long)((i - 7u) * 2u);
    }
    return -1;
}

static void sanity(void) {
    const uint32_t off = (uint32_t)(uintptr_t)target_mem & 0x00ffffffu;
    unsigned x, y, quad_ok = 0, bg_ok = 0, zero = 0, pre = 0, other = 0, other_v = 0;
    int drawn;

    say("0 SANITY magenta quad on green, prefill %04x", PREFILL);
    prefill(PREFILL);
    pvr_set_bg_color(0.0f, 1.0f, 0.0f);
    drawn = pass_begin(target_mem, AB_SIZE, AB_SIZE);
    if (drawn) {
        quad_col(16.0f, 16.0f, 112.0f, 112.0f, 0xffff00ffu, 0xffff00ffu);
        drawn = pass_end();
    }
    cpu_read(AB_SIZE);
    for (y = 0; y < AB_SIZE; y++) {
        for (x = 0; x < AB_SIZE; x++) {
            const unsigned px = cpu_img[y * AB_SIZE + x], in = x >= 16u && x < 112u && y >= 16u && y < 112u;

            if (px == (in ? MAGENTA : GREEN)) { if (in) quad_ok++; else bg_ok++; }
            else if (px == PREFILL) pre++;
            else if (px == 0u) zero++;
            else { if (!other) other_v = px; other++; }
        }
    }
    say(" quad %u/9216 bg %u/7168 right", quad_ok, bg_ok);
    say(" still prefill %u, zero %u, other %u (%04x)", pre, zero, other, other_v);

    /* drawn, with at most a few pixels of background still holding the prefill: the render is right, and the E lines
       say which pixels are held back */
    target_ok = drawn && quad_ok == 9216u && !zero && !other && pre <= 64u;
    if (target_ok && !pre) snprintf(verdict, sizeof verdict, "RENDER OK: target written and read");
    else if (target_ok) snprintf(verdict, sizeof verdict, "RENDER OK, %u px held back (see E lines)", pre);
    else if (!drawn) snprintf(verdict, sizeof verdict, "NO PASS: the render was refused or did not end");
    else if (pre == AB_SIZE * AB_SIZE) snprintf(verdict, sizeof verdict, "TARGET UNCHANGED: render did not reach VRAM");
    else if (zero == AB_SIZE * AB_SIZE) snprintf(verdict, sizeof verdict, "ALL ZERO: target not written or read wrong");
    else snprintf(verdict, sizeof verdict, "TARGET WRONG: see 0 SANITY");
    if (!target_ok) {
        const long m64 = find_run(0xa4000000u, MAGENTA), m32 = find_run(0xa5000000u, MAGENTA);

        say(" target ptr %08x", (unsigned)(uintptr_t)target_mem);
        pass_say();
        say(" RADDR wanted %08x", (unsigned)(off | (1u << 24)));
        if (m64 < 0 && m32 < 0) say(" magenta is nowhere in the 8 MB of VRAM");
        if (m64 >= 0) say(" magenta found: 64b area offset %06lx", (unsigned long)m64);
        if (m32 >= 0) say(" magenta found: 32b area offset %06lx", (unsigned long)m32);
        say("THE RESULTS BELOW MEAN NOTHING");
    }
}

/* ---- E. the pixels a render holds back ------------------------------------------- */

static unsigned edge_prev = GREEN;         /* the background of the pass before: the sanity pass's green to begin with */

/* One pass into the 128-wide target, h rows: background `bg` (r, g, b; `bg565` as it packs), one small magenta quad in
   the top left corner. Every other pixel should be the background. Says how many are not, the box round them, the
   first one's value, and what that value is: PRE (the SH-4's prefill: never written), OLD (the background of the pass
   before: written late), or ?. `screen_between`: an ordinary screen pass is drawn before the read. Returns the count. */
static unsigned edge_pass(unsigned h, const char *tag, float r, float g, float b, unsigned bg565, int screen_between) {
    unsigned x, y, n = 0, x0 = 0, x1 = 0, y0 = 0, y1 = 0, val = 0;

    prefill(PREFILL);
    pvr_set_bg_color(r, g, b);
    if (!pass_begin(target_mem, AB_SIZE, h)) { say("E h%3u %s: pass refused", h, tag); return 0; }
    quad_col(0.0f, 0.0f, 8.0f, 8.0f, 0xffff00ffu, 0xffff00ffu);
    if (!pass_end()) { say("E h%3u %s: no render end", h, tag); return 0; }
    if (screen_between) {
        pvr_set_bg_color(0.0f, 0.0f, 0.0f);
        if (pass_begin(NULL, 0u, 0u)) { quad_col(0.0f, 0.0f, 8.0f, 8.0f, 0xff404040u, 0xff404040u); pass_end(); }
    }
    cpu_read(h);
    for (y = 0; y < h; y++) {
        for (x = 0; x < AB_SIZE; x++) {
            const unsigned px = cpu_img[y * AB_SIZE + x];

            if (px == ((x < 8u && y < 8u) ? MAGENTA : bg565)) continue;
            if (!n) { x0 = x1 = x; y0 = y1 = y; val = px; }
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
            n++;
        }
    }
    if (!n) say("E h%3u %s: all written", h, tag);
    else say("E h%3u %s:%3u px x%3u-%3u y%3u-%3u =%04x %s", h, tag, n, x0, x1, y0, y1, val,
             val == PREFILL ? "PRE" : val == edge_prev ? "OLD" : "?");
    edge_prev = bg565;
    return n;
}

static void edge(void) {
    static const unsigned heights[2] = { 129u, 130u };          /* s3: 128, 130, 136, 160; one spare row was not tried */
    unsigned i;

    say("E EDGE px not the background after a pass");
    say("  PRE = prefill, OLD = the pass before's colour");
    for (i = 0; i < 2u; i++) {
        edge_pass(heights[i], "p1", 1.0f, 0.0f, 0.0f, RED, 0);
        edge_pass(heights[i], "p2", 0.0f, 0.0f, 1.0f, BLUE, 0);
    }
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    if (pass_begin(target_mem, AB_SIZE, AB_SIZE)) pass_end();    /* leave black behind for the ramp tests */
}

/* ---- S. sending 920 triangles three ways; T. the PVR's render time ---------------- */

static pvr_vertex_t ball_v[AB_BALL_VERTS] __attribute__((aligned(32)));

/* The ball's 960 vertices, ready to send, for a size x size target. Built outside every timing. */
static void ball_build(unsigned size, float frac) {
    unsigned i;

    for (i = 0; i < AB_BALL_VERTS; i++) {
        float p[3], uv[2];
        const int end = ab_ball_vertex(i, size, frac, p, uv);

        ball_v[i].flags = end ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        ball_v[i].x = p[0]; ball_v[i].y = p[1]; ball_v[i].z = p[2];
        ball_v[i].u = uv[0]; ball_v[i].v = uv[1];
        ball_v[i].argb = 0xffffffffu; ball_v[i].oargb = 0u;
    }
}

enum { SEND_PRIM = 0, SEND_DR = 1, SEND_SQ = 2 };

/* Send the header and the ball's vertices inside an open list. Returns the time of the vertices alone, in ns:
   SEND_PRIM  pvr_prim, one call a vertex
   SEND_DR    the store queues, one vertex at a time (pvr_dr_target / pvr_dr_commit: what demo/psm/psm_dc.c does)
   SEND_SQ    one store-queue copy of the whole array to the tile accelerator (pvr_sq_load) */
static uint64_t ball_send(int how) {
    uint64_t t0;
    unsigned i;

    pvr_prim(&hdr[1], sizeof hdr[1]);
    t0 = timer_ns_gettime64();
    if (how == SEND_PRIM) {
        for (i = 0; i < AB_BALL_VERTS; i++) pvr_prim(&ball_v[i], sizeof ball_v[0]);
    } else if (how == SEND_DR) {
        for (i = 0; i < AB_BALL_VERTS; i++) {
            pvr_vertex_t *o = pvr_dr_target();

            *o = ball_v[i];
            pvr_dr_commit(o);
        }
    } else {
        pvr_sq_load(NULL, ball_v, sizeof ball_v, PVR_DMA_TA);
    }
    return timer_ns_gettime64() - t0;
}

/* The PVR's render time of the pass that just ended, from KallistiOS's own statistics: stamped inside the render
   start and render-done interrupts, so it does not hold the scheduler's wait that s3's figure did. */
static uint64_t render_stat_ns(void) {
    pvr_stats_t st;

    if (pvr_get_stats(&st) < 0) return 0;
    return st.rnd_last_time;
}

static unsigned covered(unsigned rows) {
    unsigned i, n = 0;

    cpu_read(rows);
    for (i = 0; i < AB_SIZE * rows; i++) n += cpu_img[i] != 0u;
    return n;
}

static void send_test(void) {
    static const char *const name[3] = { "pvr_prim  ", "sq, 1 vtx ", "sq, array " };
    unsigned how, n, cov = 0;

    say("S SEND 960 vertices (920 tris), built before");
    ball_build(AB_SIZE, 0.3f);
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    for (how = 0; how < 3u; how++) {
        uint64_t lo = ~0ull, t;
        int ok = 0;

        for (n = 0; n < 3u; n++) {
            if (!pass_begin(target_mem, AB_SIZE, AB_SIZE + 2u)) continue;
            t = ball_send((int)how);
            if (!pass_end()) continue;
            ok = 1;
            if (t < lo) lo = t;
        }
        if (!ok) { say("S %s NOT DRAWN", name[how]); pass_say(); continue; }
        cov = covered(AB_SIZE);
        say("S %s%5lu us, %4lu ns/vtx, covers %u", name[how], (unsigned long)(lo / 1000u), (unsigned long)(lo / AB_BALL_VERTS), cov);
    }
}

/* One pass into big_mem at size x size: nothing but a 2 x 2 quad (frac 0), or the ball at radius frac x size.
   Returns the smallest of 3 render times from the statistics, in ns; 0 when no pass was made. */
static uint64_t render_time(unsigned size, float frac) {
    uint64_t lo = ~0ull, t;
    unsigned n;

    if (frac > 0.0f) ball_build(size, frac);
    for (n = 0; n < 3u; n++) {
        if (!pass_begin(big_mem, size, size)) continue;
        if (frac > 0.0f) ball_send(SEND_SQ);
        else quad_col(0.0f, 0.0f, 2.0f, 2.0f, 0xff000000u, 0xff000000u);
        if (!pass_end()) continue;
        t = render_stat_ns();
        if (t < lo) lo = t;
    }
    return lo == ~0ull ? 0 : lo;
}

static void render_test(void) {
    static const unsigned sizes[2] = { 128u, 256u };
    unsigned i;

    say("T PVR RENDER us, from pvr_get_stats, min of 3");
    say("T size: empty, ball r 0.15, ball r 0.30");
    for (i = 0; i < 2u; i++) {
        const unsigned long e = (unsigned long)(render_time(sizes[i], 0.0f) / 1000u);
        const unsigned long b1 = (unsigned long)(render_time(sizes[i], 0.15f) / 1000u);
        const unsigned long b2 = (unsigned long)(render_time(sizes[i], 0.3f) / 1000u);

        say("T %3u: %6lu, %6lu, %6lu", sizes[i], e, b1, b2);
    }
}

/* ---- B. copying the map out of video memory: the DMA against the SH-4's own ways ---- */

#define COPY_MAX (256u * 256u * 2u)
static uint8_t copy_dst[COPY_MAX] __attribute__((aligned(32)));

/* Every way gets the destination, the source through the uncached window (P2), the same source through the cached
   mirror (P1), and the byte count (a multiple of 32). */
typedef void (*copy_fn)(void *dst, const void *unc, const void *cached, unsigned n);

static void copy_memcpy(void *dst, const void *unc, const void *cached, unsigned n) { (void)cached; memcpy(dst, unc, n); }

static void copy_16(void *dst, const void *unc, const void *cached, unsigned n) {          /* s2's loop: 16 bits a read */
    const volatile uint16_t *s = (const volatile uint16_t *)unc;
    uint16_t *d = (uint16_t *)dst;
    unsigned i;

    (void)cached;
    for (i = 0; i < n / 2u; i++) d[i] = s[i];
}

static void copy_32(void *dst, const void *unc, const void *cached, unsigned n) {          /* 32 bits a read */
    const volatile uint32_t *s = (const volatile uint32_t *)unc;
    uint32_t *d = (uint32_t *)dst;
    unsigned i;

    (void)cached;
    for (i = 0; i < n / 4u; i++) d[i] = s[i];
}

/* 64 bits a read: the FPU's pair moves (FPSCR.SZ set for the loop), four a 32-byte line. A measuring loop, not a
   library kernel. */
static void copy_64(void *dst, const void *unc, const void *cached, unsigned n) {
    unsigned lines = n / 32u;

    (void)cached;
    __asm__ volatile (
        "fschg\n"
        "1:\n\t"
        "fmov @%1+, dr0\n\t"
        "fmov @%1+, dr2\n\t"
        "fmov @%1+, dr4\n\t"
        "fmov @%1+, dr6\n\t"
        "fmov dr0, @%0\n\t"
        "add #8, %0\n\t"
        "fmov dr2, @%0\n\t"
        "add #8, %0\n\t"
        "fmov dr4, @%0\n\t"
        "add #8, %0\n\t"
        "fmov dr6, @%0\n\t"
        "dt %2\n\t"
        "bf/s 1b\n\t"
        "add #8, %0\n\t"
        "fschg\n"
        : "+r"(dst), "+r"(unc), "+r"(lines)
        :
        : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "t", "memory");
}

/* Through the cache: the source's lines are dropped first (the PVR has written under them), then each 32-byte line
   comes in as one line fill. The invalidate is inside the time: a real use pays it. */
static void copy_cached(void *dst, const void *unc, const void *cached, unsigned n) {
    const uint32_t *s = (const uint32_t *)cached;
    uint32_t *d = (uint32_t *)dst;
    unsigned i;

    (void)unc;
    dcache_inval_range((uintptr_t)cached, n);
    for (i = 0; i < n / 4u; i++) d[i] = s[i];
}

static void copy_cached_pref(void *dst, const void *unc, const void *cached, unsigned n) {  /* the same, the line after next asked for early */
    const uint32_t *s = (const uint32_t *)cached;
    uint32_t *d = (uint32_t *)dst;
    unsigned i, k;

    (void)unc;
    dcache_inval_range((uintptr_t)cached, n);
    for (i = 0; i < n / 4u; i += 8u) {
        __builtin_prefetch(s + i + 16u);
        for (k = 0; k < 8u; k++) d[i + k] = s[i + k];
    }
}

static unsigned diff32(const void *a, const void *b, unsigned n) {
    const uint32_t *x = (const uint32_t *)a, *y = (const uint32_t *)b;
    unsigned i, d = 0;

    for (i = 0; i < n / 4u; i++) d += x[i] != y[i];
    return d;
}

static void *cached_of(pvr_ptr_t mem) { return (void *)(((uintptr_t)mem & 0x1fffffffu) | 0x80000000u); }

/* `bytes` of `mem` (a finished render): the PVR-DMA download, then each way of the SH-4, smallest of 8 runs by both
   clocks, and how many 32-bit words of each copy differ from the DMA's (summed over the runs: 0 = always the same). */
static void baseline(pvr_ptr_t mem, unsigned bytes, unsigned first, unsigned count) {
    static const struct { const char *name; copy_fn fn; } way[6] = {
        { "memcpy P2", copy_memcpy }, { "16-bit P2", copy_16 }, { "32-bit P2", copy_32 }, { "64-bit P2", copy_64 },
        { "cached P1", copy_cached }, { "P1 + pref", copy_cached_pref },
    };
    uint64_t lo, hi, mean, t, c, clo;
    unsigned w, n, bad;

    if (!pd_spread(mem, big_dst, bytes, 8u, &lo, &hi, &mean)) { say("B %3uK DMA did not finish", bytes / 1024u); return; }
    if (!first) say("B %3uK PVR-DMA   %6lu us", bytes / 1024u, (unsigned long)(lo / 1000u));
    for (w = first; w < first + count && w < 6u; w++) {
        for (lo = clo = ~0ull, bad = 0, n = 0; n < 8u; n++) {
            memset(copy_dst, 0x5a, bytes);
            t = timer_ns_gettime64(); c = cyc();
            way[w].fn(copy_dst, mem, cached_of(mem), bytes);
            c = cyc() - c;
            t = timer_ns_gettime64() - t;
            if (t < lo) lo = t;
            if (c < clo) clo = c;
            bad += diff32(copy_dst, big_dst, bytes);
        }
        say("B %3uK %s %6lu us %8lu cyc d%u", bytes / 1024u, way[w].name, (unsigned long)(lo / 1000u), (unsigned long)clo, bad);
    }
    if (first + count > 4u) dcache_inval_range((uintptr_t)cached_of(mem), bytes);   /* leave no line of video memory in the cache */
}

/* Words of the 128 x 128 target, from byte `first` on, that read differently through the cached mirror and through
   the uncached window. Compared in place, a word at a time, so nothing else is written and no line is pushed out of
   the cache by the test's own copies. `inval`: the mirror's lines are dropped first. */
static unsigned mirror_diff(unsigned first, unsigned bytes, int inval) {
    const uint32_t *c = (const uint32_t *)((const uint8_t *)cached_of(target_mem) + first);
    const volatile uint32_t *u = (const volatile uint32_t *)((const uint8_t *)target_mem + first);
    unsigned i, d = 0;

    if (inval) dcache_inval_range((uintptr_t)c, bytes);
    for (i = 0; i < bytes / 4u; i++) d += c[i] != u[i];
    return d;
}

/* Is the cached mirror ever stale after an invalidate? And is it stale without one (if not, the test proves nothing)? */
static void coherency(void) {
    const unsigned tail = MAP_BYTES - 8192u;                     /* the last 32 rows: 8 KB, half the SH-4's data cache */
    unsigned round, total = 0, stale;
    int ok = 1;

    say("K CACHED MIRROR vs uncached read, words");
    for (round = 0; round < 8u && ok; round++) {                 /* draw, invalidate, read: 8 rounds, the picture changing each time */
        ball_build(AB_SIZE, (round & 1u) ? 0.15f : 0.3f);
        ok = pass_begin(target_mem, AB_SIZE, AB_SIZE + 2u);
        if (ok) { ball_send(SEND_SQ); ok = pass_end(); }
        if (ok) total += mirror_diff(0u, MAP_BYTES, 1);
    }
    if (!ok) { say("K NOT DRAWN"); pass_say(); return; }
    say("K 8 x (render, invalidate, read): %u stale", total);

    /* without the invalidate: bring the last 32 rows into the cache, draw magenta over them, read again */
    stale = mirror_diff(tail, 8192u, 1);
    ok = pass_begin(target_mem, AB_SIZE, AB_SIZE + 2u);
    if (ok) { quad_col(0.0f, 96.0f, 128.0f, 127.0f, 0xffff00ffu, 0xffff00ffu); ok = pass_end(); }
    if (!ok) { say("K NOT DRAWN"); pass_say(); return; }
    say("K rows 96-127 read (%u stale), then redrawn:", stale);
    say("K  NO invalidate: %u of 2048 stale", mirror_diff(tail, 8192u, 0));
    say("K  invalidate:    %u of 2048 stale", mirror_diff(tail, 8192u, 1));
    dcache_inval_range((uintptr_t)cached_of(target_mem), MAP_BYTES);
}

/* The ways `first` .. `first + count - 1` of baseline(), at 32 KB and at 128 KB. */
static void baselines(unsigned first, unsigned count) {
    int ok;

    say("B COPY OUT of VRAM, min of 8; d = words not as DMA");
    ball_build(AB_SIZE, 0.3f);
    ok = pass_begin(target_mem, AB_SIZE, AB_SIZE + 2u);
    if (ok) { ball_send(SEND_SQ); ok = pass_end(); }
    if (ok) baseline(target_mem, MAP_BYTES, first, count);
    else { say("B 32K NOT DRAWN"); pass_say(); }
    ball_build(256u, 0.3f);
    ok = pass_begin(big_mem, 256u, 256u);
    if (ok) { ball_send(SEND_SQ); ok = pass_end(); }
    if (ok) baseline(big_mem, COPY_MAX, first, count);
    else { say("B 128K NOT DRAWN"); pass_say(); }
}

/* ---- the run -------------------------------------------------------------------- */

static int start(void) {
    pvr_init_params_t init = pvr_default_params;

    init.vertex_buf_size = 256 * 1024;
    init.opb_overflow_count = 8;                                 /* the ball puts many strips in a few tiles of a small target */
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return 0; }
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    if (!load()) { say("video memory allocation FAIL"); pvr_shutdown(); return 0; }
    vid_set_dithering(false);
    return 1;
}

static void stop(void) {
    vid_set_dithering(true);
    pvr_mem_free(target_mem); pvr_mem_free(big_mem); pvr_mem_free(ramp_mem[0]); pvr_mem_free(ramp_mem[1]);
    pvr_shutdown();
}

/* Part one: everything that reads video memory the way s2 and s3 did. Its pages are on screen before part two. */
static void run(void) {
    nlines = 0;
    say("(verdict)");                                            /* line 0: written once the sanity step has run */
    snprintf(lines[0], sizeof lines[0], "NOT RUN: see the next lines");
    if (!start()) return;
    clocks();
    sanity();
    memcpy(lines[0], verdict, sizeof verdict);
    printf("ab: VERDICT %s\n", verdict);
    edge();
    send_test();
    render_test();
    baselines(0u, 4u);
    stop();
    say("START: part 2, reads through the cache");
    say(" (new on hardware: these pages stay if it hangs)");
}

/* Part two, on START: video memory read through the SH-4's cached mirror, which no run has done before. */
static void run_cached(void) {
    say("PART 2");
    if (!start()) return;
    baselines(4u, 2u);
    coherency();
    stop();
    say("PART 2 done");
}

static void cont_reset_btn_callback_(uint8_t addr, uint32_t btns) {
    (void)addr;
    (void)btns;
    arch_exit();
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    cont_btn_callback(0, CONT_RESET_BUTTONS, cont_reset_btn_callback_);
    unsigned page = 0, pages, held = ~0u;
    int part2 = 0;

    run();
    vid_set_mode(DM_640x480, PM_RGB565);
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;

        pages = (nlines + ROWS - 1u) / ROWS;
        if (!pages) pages = 1u;
        if (held == ~0u) page_draw(page, pages);                 /* the first pass: nothing counts as a press */
        else if (hit & CONT_A) { page = (page + 1u) % pages; page_draw(page, pages); }
        else if (hit & CONT_B) { page = (page + pages - 1u) % pages; page_draw(page, pages); }
        else if ((hit & CONT_START) && !part2) {
            part2 = 1;
            page = (nlines + ROWS - 1u) / ROWS;                  /* open on the first new page */
            run_cached();
            vid_set_mode(DM_640x480, PM_RGB565);
            pages = (nlines + ROWS - 1u) / ROWS;
            if (page >= pages) page = pages - 1u;
            page_draw(page, pages);
        }
        held = now;
        thd_sleep(20);
    }
    return 0;
}
