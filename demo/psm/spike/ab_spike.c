/* SH4DER-PVR: route AB spike, the Dreamcast test program

   demo/psm/spike/ab_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A test program for a real console, not an effect. It asks the hardware about route AB (demo/psm/ROUTE_AB_NOTES.md:
   the PVR draws the casters' depth into a render target as a colour code, the picture is copied back to RAM, the
   SH-4 compares per vertex) and prints what it measured on result pages (and on the serial line, "ab:" lines).

   s2 on a console (ROUTE_AB_NOTES.md, "Hardware result s2") found: the ramp codes come back within one code with
   dither off; dither on destroys them; the PVR-DMA download works; 16 pixels of the target's last row keep what was
   there before. s3 asks what was left open:

     0 SANITY a plain magenta quad on green, over a target the SH-4 filled with 0x1234 first.
     E EDGE   which pixels a render leaves unwritten, and what they hold after the next render: three passes with
              different background colours into a target 128, 130, 136 and 160 rows tall, then whether an ordinary
              screen pass in between makes them appear.
     R RAMP   the s1 tests (10 and 16 bits: FLAT, TILT, OVER), now with the error by sign: one under, one over, more.
     P PREC   16 bits: six bands whose code rises one a pixel, at six sizes of the code (v = 1024 u from 1 to 896):
              how many low bits of the wrapped v survive at each size.
     D DITHER dither off for the target pass only: FB_CFG_2 is written before the pass and put back after its
              render-done, then a screen pass is drawn. The target must be exact and the screen pass must be dithered
              (it is compared with the same pass drawn undithered).
     2 COPY   the PVR-DMA download (PVR to system memory, our own register writes) against the SH-4's read, and its
              spread over 8 runs.
     C COST   for a 128, 256 and 512 target: a 920-triangle ball with the 16-bit ramp drawn into it (the time to
              send it and the PVR's render time), the PVR-DMA download of the whole target, and 20,000 two-byte reads
              of the copy in RAM (4 a vertex x 5,000 vertices: the fetches of a compare, not the compare).

   The PVR-DMA download. Register names and the direction bit are from the Flycast source read as documentation of
   the register block (core/hw/holly/sb.h, core/hw/pvr/pvr_sb_regs.cpp: SB_PDDIR set is "PVR -> System"); the area
   addresses from Sega's 1999 "SH4 access" table. No code is copied from either. It is not the DMA KallistiOS calls
   pvr_dma (the 0x005F6800 block, memory to the tile accelerator on SH-4 channel 2); it is the 0x005F7C00 block on
   SH-4 channel 0 in DDT mode. On the console (s2) it ran with DMAOR = 0x8201 as the BIOS leaves it, SB_PDAPRO =
   0x6702007F, the PVR address in either area form, and left CHCR0 = 0x000853C3 (burst, 32-byte units).

   Every number on the result pages comes from the run that prints it. Only a real console's run is evidence.

   KallistiOS paths: pvr_init, pvr_mem_malloc, pvr_txr_load, pvr_scene_begin_rtt, pvr_scene_begin, pvr_prim,
   pvr_wait_ready, pvr_wait_render_done, vid_set_dithering, vid_waitvbl, timer_ns_gettime64, perf_cntr_start /
   perf_cntr_count (PRFC1), dcache_purge_range / dcache_inval_range, bfont, maple.
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
    snprintf(head, sizeof head, "AB SPIKE s3 p%u/%u  A next B back START rerun", page + 1u, pages);
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
static uint16_t dma_img[AB_SIZE * AB_SIZE] __attribute__((aligned(32)));      /* the target as PVR-DMA delivered it */
static uint16_t scr_img[2][AB_SIZE * AB_SIZE];                                /* a corner of the screen pass, dithered and not */
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

/* Draw one model test into the 128 x 128 target and wait for the PVR to finish it. 0 when the pass could not be made. */
static int scene(const ab_test_t *t, int bits) {
    pvr_vertex_t v;
    unsigned r, i;

    prefill(PREFILL);
    if (!pass_begin(target_mem, AB_SIZE, AB_SIZE)) return 0;
    pvr_prim(&hdr[bits == 16], sizeof hdr[0]);
    memset(&v, 0, sizeof v);
    v.argb = 0xffffffffu;
    for (r = 0; r < t->nrect; r++) {
        for (i = 0; i < 4u; i++) {
            float p[3], uv[2];

            ab_rect_corner(&t->rect[r], i, bits, p, uv);
            v.flags = i == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            v.x = p[0]; v.y = p[1]; v.z = p[2];
            v.u = uv[0]; v.v = uv[1];
            pvr_prim(&v, sizeof v);
        }
    }
    return pass_end();
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

static unsigned diff16(const uint16_t *a, const uint16_t *b, unsigned n) {
    unsigned i, d = 0;

    for (i = 0; i < n; i++) d += a[i] != b[i];
    return d;
}

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
    static const unsigned heights[4] = { 128u, 130u, 136u, 160u };
    unsigned i;

    say("E EDGE px not the background after a pass");
    say("  PRE = prefill, OLD = the pass before's colour");
    for (i = 0; i < 4u; i++) {
        edge_pass(heights[i], "p1", 1.0f, 0.0f, 0.0f, RED, 0);
        edge_pass(heights[i], "p2", 0.0f, 0.0f, 1.0f, BLUE, 0);
        edge_pass(heights[i], "p3", 1.0f, 0.0f, 0.0f, RED, 0);
    }
    edge_pass(128u, "sc", 0.0f, 0.0f, 1.0f, BLUE, 1);            /* does a screen pass after it bring them out */
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    if (pass_begin(target_mem, AB_SIZE, AB_SIZE)) pass_end();    /* leave black behind for the ramp tests */
}

/* ---- R. the ramp tests, P. the precision bands ----------------------------------- */

static void check_one(unsigned which, int bits) {
    ab_test_t t;
    ab_stats_t st;

    ab_test_make(&t, which, bits);
    if (!scene(&t, bits)) { say("%2d %-4s NOT DRAWN", bits, t.name); pass_say(); return; }
    cpu_read(AB_SIZE);
    ab_check(&t, bits, cpu_img, AB_SIZE, 0.0f, 0.0f, &st);
    say("%2d %-4s -1:%5u +1:%5u more:%5u cov:%3u", bits, t.name, st.neg1, st.pos1, st.more, st.cover);
    if (st.more) say("  max %u, first %u,%u got %u want %u", st.max_err, st.first_x, st.first_y, st.first_got, st.first_want);
    if (st.cover) say("  cov box x%3u-%3u y%3u-%3u", st.cx0, st.cx1, st.cy0, st.cy1);
}

static void precision(void) {
    ab_test_t t;
    ab_stats_t st;
    unsigned i, y0, y1;

    say("P PREC 16 bits, code +1 a pixel, 1792 px a band");
    ab_test_make(&t, AB_TEST_PREC, 16);
    if (!scene(&t, 16)) { say("P NOT DRAWN"); pass_say(); return; }
    cpu_read(AB_SIZE);
    for (i = 0; i < AB_PREC_BANDS; i++) {
        ab_prec_rows(i, &y0, &y1);
        ab_check_rows(&t, 16, cpu_img, AB_SIZE, 0.0f, 0.0f, y0, y1, &st);
        say("P %5u: -1:%4u +1:%4u more:%4u max %u", ab_prec_base(i), st.neg1, st.pos1, st.more + st.cover, st.max_err);
    }
}

/* ---- D. dither off for the target pass alone -------------------------------------- */

/* A screen pass: a slow grey run (0x20 to 0x30 over 256 pixels, which a 5/6-bit target can only show with dither),
   then 128 x 128 of the frame buffer it was rendered into, read by the SH-4. 0 when the pass was not made. */
static int screen_pass(uint16_t *img) {
    const volatile uint16_t *fb;
    unsigned x, y;

    if (!pass_begin(NULL, 0u, 0u)) return 0;
    quad_col(0.0f, 0.0f, 256.0f, 256.0f, 0xff202020u, 0xff303030u);
    if (!pass_end()) return 0;
    fb = (const volatile uint16_t *)(uintptr_t)(0xa5000000u | (last.render_addr & 0x00ffffffu));
    for (y = 0; y < AB_SIZE; y++) for (x = 0; x < AB_SIZE; x++) img[y * AB_SIZE + x] = fb[y * 640u + x];
    return 1;
}

static void dither(void) {
    ab_test_t t;
    ab_stats_t st;
    uint32_t cfg_off, cfg_on;
    int ok;

    say("D DITHER off for the target pass only");
    ab_test_make(&t, AB_TEST_FLAT, 10);
    vid_set_dithering(false);                                    /* before the target pass is closed */
    cfg_off = PVR_GET(PVR_FB_CFG_2);
    ok = scene(&t, 10);                                          /* returns after the target's render-done */
    vid_set_dithering(true);                                     /* back on before the screen pass's list is closed */
    cfg_on = PVR_GET(PVR_FB_CFG_2);
    if (!ok) { say("D target pass NOT DRAWN"); pass_say(); return; }
    cpu_read(AB_SIZE);
    ab_check(&t, 10, cpu_img, AB_SIZE, 0.0f, 0.0f, &st);
    say("D FB_CFG_2 %08x for target, %08x after", (unsigned)cfg_off, (unsigned)cfg_on);
    say("D target: -1:%u +1:%u more:%u (exact = 0 0 0)", st.neg1, st.pos1, st.more);
    ok = screen_pass(scr_img[0]);                                /* the frame's own pass, dither on again */
    vid_set_dithering(false);
    ok = ok && screen_pass(scr_img[1]);                          /* the same pass undithered, to compare with */
    if (!ok) { say("D screen pass NOT DRAWN"); pass_say(); return; }
    say("D screen: %u/16384 px differ from undithered", diff16(scr_img[0], scr_img[1], AB_SIZE * AB_SIZE));
    say("  (over 0: the screen kept its dither)");
}

/* ---- 2. the copy back ------------------------------------------------------------ */

static void copy_back(void) {
    uint64_t lo, hi, mean;
    pd_result_t r;
    ab_test_t t;
    unsigned i, changed = 0, ok;

    say("2 COPY BACK by PVR-DMA, 128x128 = 32 KB");
    ab_test_make(&t, AB_TEST_FLAT, 16);                          /* a picture with a different value at every pixel */
    if (!scene(&t, 16)) { say(" no picture to copy"); pass_say(); return; }
    cpu_read(AB_SIZE);
    for (i = 0; i < AB_SIZE * AB_SIZE; i++) dma_img[i] = 0xa5a5u;
    pd_download(pd_addr(target_mem), dma_img, MAP_BYTES, &r);
    for (i = 0; i < AB_SIZE * AB_SIZE; i++) changed += dma_img[i] != 0xa5a5u;
    say(" %s irq %d, wrote %u px, %u differ from CPU", r.done ? "done" : "TIMEOUT", r.irq, changed, diff16(dma_img, cpu_img, AB_SIZE * AB_SIZE));
    say(" DMAOR %04x CHCR0 %08x", (unsigned)(DMAC_DMAOR & 0xffffu), (unsigned)r.chcr0);
    if (!r.done) {
        say(" PDST %x LEND %x STAPD %08x", (unsigned)r.pdst, (unsigned)r.lend, (unsigned)r.stapd);
        say(" STARD %08x TCR0 %x", (unsigned)r.stard, (unsigned)r.tcr0);
        return;
    }
    ok = pd_spread(target_mem, dma_img, MAP_BYTES, 8u, &lo, &hi, &mean);
    if (ok) say(" %u runs us: min %lu max %lu mean %lu", ok, (unsigned long)(lo / 1000u), (unsigned long)(hi / 1000u), (unsigned long)(mean / 1000u));
    else say(" no run finished");
}

/* ---- C. the cost of a real pass, by target size ----------------------------------- */

#define BALL_STACKS 20u
#define BALL_SEGS 23u                      /* 20 strips of 46 triangles: 920, the cow's count in the demo */

/* A ball of 920 triangles in the middle of a size x size target, radius 0.3 size, its depth k from 0.3 to 0.7, the
   16-bit ramp with u = 0.25 / k: a stand-in for a caster. The rings stop short of the poles, so no triangle is
   without area on the sphere; front and back are both drawn, as a caster's are. */
static void ball(unsigned size) {
    const float c = 0.5f * (float)size, rad = 0.3f * (float)size;
    pvr_vertex_t v;
    unsigned st, sg, e;

    pvr_prim(&hdr[1], sizeof hdr[1]);
    memset(&v, 0, sizeof v);
    v.argb = 0xffffffffu;
    for (st = 0; st < BALL_STACKS; st++) {
        for (sg = 0; sg <= BALL_SEGS; sg++) {
            const float phi = 6.2831853f * (float)sg / (float)BALL_SEGS;

            for (e = 0; e < 2u; e++) {
                const float th = 3.1415927f * (0.08f + 0.84f * (float)(st + 1u - e) / (float)BALL_STACKS);
                const float k = 0.5f + 0.2f * sinf(th) * sinf(phi), u = 0.25f / k;

                v.flags = (sg == BALL_SEGS && e == 1u) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                v.x = c + rad * sinf(th) * cosf(phi); v.y = c + rad * cosf(th); v.z = k;
                v.u = u; v.v = u * (float)AB_RAMP_W;
                pvr_prim(&v, sizeof v);
            }
        }
    }
}

static void cost(void) {
    static const unsigned sizes[3] = { 128u, 256u, 512u };
    unsigned i, n, s, k, acc, covered, ok;
    uint64_t sub, rnd, t0, lo, hi, mean, taps;

    say("C COST 920-tri ball, 16-bit ramp; smallest of 3");
    for (i = 0; i < 3u; i++) {
        const unsigned size = sizes[i], bytes = size * size * 2u;
        const uint16_t *map = (const uint16_t *)big_dst;

        for (sub = rnd = ~0ull, ok = 0, n = 0; n < 3u; n++) {
            if (!pass_begin(big_mem, size, size)) continue;
            ball(size);
            if (!pass_end()) continue;
            ok++;
            if (last.submit_ns < sub) sub = last.submit_ns;
            if (last.render_ns < rnd) rnd = last.render_ns;
        }
        if (!ok) { say("C %3u: NOT DRAWN", size); pass_say(); continue; }
        say("C %3u: send %lu us, PVR render %lu us", size, (unsigned long)(sub / 1000u), (unsigned long)(rnd / 1000u));
        ok = pd_spread(big_mem, big_dst, bytes, 3u, &lo, &hi, &mean);
        if (!ok) { say("C %3u: DMA of %u KB did not finish", size, bytes / 1024u); continue; }
        for (covered = 0, k = 0; k < size * size; k++) covered += map[k] != 0u;
        for (taps = ~0ull, n = 0; n < 3u; n++) {                 /* 4 texels a vertex x 5,000 vertices from the copy in RAM */
            t0 = timer_ns_gettime64();
            for (s = 12345u, k = 0, acc = 0; k < 20000u; k++) { s = s * 1664525u + 1013904223u; acc += ((const volatile uint16_t *)map)[(s >> 12) & (size * size - 1u)]; }
            t0 = timer_ns_gettime64() - t0;
            sink = acc;
            if (t0 < taps) taps = t0;
        }
        say("C %3u: DMA %u KB %lu us, 20000 taps %lu us", size, bytes / 1024u, (unsigned long)(lo / 1000u), (unsigned long)(taps / 1000u));
        say("C %3u: ball covers %u of %u px", size, covered, size * size);
    }
}

/* ---- the run -------------------------------------------------------------------- */

static void run(void) {
    static const int bits[2] = { 10, 16 };
    pvr_init_params_t init = pvr_default_params;
    unsigned b, w;

    nlines = 0;
    say("(verdict)");                                            /* line 0: written once the sanity step has run */
    snprintf(lines[0], sizeof lines[0], "NOT RUN: see the next lines");
    init.vertex_buf_size = 256 * 1024;
    init.opb_overflow_count = 8;                                 /* the ball puts many strips in a few tiles of a small target */
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return; }
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    if (!load()) { say("video memory allocation FAIL"); pvr_shutdown(); return; }
    vid_set_dithering(false);
    clocks();
    sanity();
    memcpy(lines[0], verdict, sizeof verdict);
    printf("ab: VERDICT %s\n", verdict);
    edge();

    say("R RAMP dither off: got-want by sign, cov");
    for (b = 0; b < 2u; b++) for (w = 0; w < AB_TESTS; w++) check_one(w, bits[b]);
    precision();
    dither();
    vid_set_dithering(false);
    copy_back();
    cost();

    vid_set_dithering(true);
    pvr_mem_free(target_mem); pvr_mem_free(big_mem); pvr_mem_free(ramp_mem[0]); pvr_mem_free(ramp_mem[1]);
    pvr_shutdown();
}

int main(void) {
    unsigned page = 0, pages, held = ~0u;

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
        else if (hit & CONT_START) {
            run();
            vid_set_mode(DM_640x480, PM_RGB565);
            page = 0;
            pages = (nlines + ROWS - 1u) / ROWS;
            page_draw(page, pages ? pages : 1u);
        }
        held = now;
        thd_sleep(20);
    }
    return 0;
}
