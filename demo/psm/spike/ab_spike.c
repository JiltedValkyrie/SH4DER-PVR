/* SH4DER-PVR: route AB spike, the Dreamcast test program

   demo/psm/spike/ab_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A test program, not an effect. It asks a console four things for route AB (demo/psm/ROUTE_AB_NOTES.md: the PVR
   draws the casters' depth into a render target as a colour code, the picture is copied back to RAM, the SH-4
   compares per vertex) and prints what it measured, on screen and on the serial line:

     1 RAMP   quads drawn into a 128 x 128 RGB565 render target with a point sampled REPLACE ramp texture whose
              texels are their own index (a 1024 x 8 ramp for 10 bits, a 1024 x 64 clamp-u wrap-v ramp for 16):
              one flat, one whose depth runs across it, two that overlap. Dither off, and once on to see whether
              the bit matters to a render target.
     2 COPY   the target read back twice: by the SH-4 through the uncached video memory window, and by PVR-DMA in
              the PVR to system memory direction (our own register writes, below; KallistiOS has no download).
     3 CHECK  every pixel of what came back against ab_model.h: pixels that differ, the largest error in codes,
              the sample offset that fits best.
     4 TIME   the PVR-DMA download of 32 KB and of 512 KB, a sparse read of 20,000 texels straight from video
              memory (4 a vertex x 5,000 vertices), the same from RAM, a whole 32 KB copy by the SH-4.

   The PVR-DMA download. Register names and the direction bit are from the Flycast source (core/hw/holly/sb.h,
   core/hw/pvr/pvr_sb_regs.cpp: SB_PDDIR set is "PVR -> System"), the area addresses from Sega's 1999 "SH4 access"
   table. No code is copied from either. This is not the DMA KallistiOS calls pvr_dma (that one is the 0x005F6800
   block on SH-4 channel 2, memory to the tile accelerator); this is the 0x005F7C00 block, which the emulator models
   on SH-4 channel 0 in DDT mode. Things this program does not know and so reports instead of assuming:
     - the form of the PVR address (it tries the 64-bit area, 0x04000000 + offset, then the 32-bit area, 0x05000000)
     - whether SB_PDAPRO (written 0x6702007F: key and the whole of system memory, a value from memory of the
       register's description, not from a source read for this file) is needed or right
     - whether SH-4 channel 0 needs anything set up beyond DMAOR = 0x8201, which it checks and prints
   A download that does not finish in 50 ms is stopped and its registers are printed.

   Every number on the result pages comes from this run. A run in an emulator shows the emulator's model, not the
   PVR's precision or the bus: say which it was when the numbers are passed on.

   KallistiOS paths: pvr_init, pvr_mem_malloc, pvr_txr_load, pvr_scene_begin_rtt, pvr_prim, pvr_wait_ready,
   pvr_wait_render_done, vid_set_dithering, timer_ns_gettime64, dcache_purge_range / dcache_inval_range, bfont, maple.
   Build: python demo/psm/spike/build.py (it refuses a KallistiOS without pvr_scene_begin_rtt). No SH4ZAM, no library
   code: only ab_model.h. Not run on a console or in an emulator when this file was written.
*/

#include <kos.h>
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
#define DMAC_SAR0   REG32(0xffa00000)
#define DMAC_DMATCR0 REG32(0xffa00008)
#define DMAC_CHCR0  REG32(0xffa0000c)
#define DMAC_DMAOR  REG32(0xffa00040)
#define DMAOR_WANT  0x8201u                /* DDT, priority 2-0-1-3, master enable: what the BIOS leaves and Flycast asks for */

#define PD_TIMEOUT_NS 50000000ull
#define BIG_BYTES (512u * 1024u)
#define MAP_BYTES (AB_SIZE * AB_SIZE * 2u)

/* ---- the result pages ----------------------------------------------------------- */

#define COLS 50u
#define ROWS 17u
#define LINES_MAX 160u
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
    snprintf(head, sizeof head, "AB SPIKE s1 p%u/%u  A next B back START rerun", page + 1u, pages);
    head[COLS] = 0;
    bfont_draw_str(vram_s + 16u * 640u + 20u, 640, true, head);
    for (i = 0; i < ROWS && page * ROWS + i < nlines; i++)
        bfont_draw_str(vram_s + (16u + 24u * (i + 2u)) * 640u + 20u, 640, true, lines[page * ROWS + i]);
}

/* ---- state ---------------------------------------------------------------------- */

static pvr_ptr_t target_mem, ramp_mem[2], big_mem;
static pvr_poly_hdr_t hdr[2];
static uint16_t ramp_px[AB_RAMP_W * AB_RAMP16_H] __attribute__((aligned(32)));
static uint16_t cpu_img[AB_SIZE * AB_SIZE] __attribute__((aligned(32)));      /* the target as the SH-4 read it */
static uint16_t dma_img[AB_SIZE * AB_SIZE] __attribute__((aligned(32)));      /* the target as PVR-DMA delivered it */
static uint8_t big_src[BIG_BYTES] __attribute__((aligned(32))), big_dst[BIG_BYTES] __attribute__((aligned(32)));
static int dma_form = -1;                  /* which PVR address form delivered the target: 0 the 64-bit area, 1 the 32-bit, -1 none */

/* ---- 1. the ramp scenes --------------------------------------------------------- */

static int load(void) {
    pvr_poly_cxt_t c;
    unsigned b;

    target_mem = pvr_mem_malloc(MAP_BYTES);
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
    return target_mem && big_mem;
}

/* Draw one test into the target and wait for the PVR to finish it. 0 when the pass could not be made. */
static int scene(const ab_test_t *t, int bits) {
    pvr_vertex_t v;
    unsigned r, i, tries;

    pvr_wait_ready();
    if (pvr_scene_begin_rtt(target_mem, AB_SIZE, AB_SIZE, AB_SIZE) < 0) return 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
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
    pvr_list_finish();
    pvr_scene_finish();
    for (tries = 0; tries < 10u && pvr_wait_ready() < 0; tries++) {}          /* the render has started (a vblank away) */
    if (tries == 10u) return 0;
    for (tries = 0; tries < 10u && pvr_wait_render_done() < 0; tries++) {}    /* and the target is whole */
    return tries < 10u;
}

/* ---- 2. the copies back --------------------------------------------------------- */

/* The target by the SH-4, 16 bits a read, through the uncached window the texture pointer is in. */
static void cpu_read(void) {
    const volatile uint16_t *src = (const volatile uint16_t *)target_mem;
    unsigned i;

    for (i = 0; i < AB_SIZE * AB_SIZE; i++) cpu_img[i] = src[i];
}

typedef struct pd_result {
    int done;                              /* SB_PDST came back to 0 inside the timeout */
    int irq;                               /* the end-of-PVR-DMA status bit was set */
    uint64_t ns;
    uint32_t pdst, lend, stapd, stard, chcr0, tcr0;   /* after: for a download that did not finish */
} pd_result_t;

/* PVR-DMA, PVR to system memory: `bytes` (a multiple of 32) from the PVR address `pvr_addr` to `dst` (32-byte
   aligned RAM). The destination's cache lines are written back and dropped first, and dropped again after, so what
   is read afterwards is what the DMA put in RAM. Interrupts stay on (the timer needs them), so one run's time may hold
   an interrupt: the caller repeats and keeps the smallest. The end is seen by polling SB_PDST, not by an interrupt. */
static void pd_download(uint32_t pvr_addr, void *dst, uint32_t bytes, pd_result_t *res) {
    uint64_t t0, t1;

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
    SB_PDST = 1;
    do { t1 = timer_ns_gettime64(); } while ((SB_PDST & 1u) && t1 - t0 < PD_TIMEOUT_NS);
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

static unsigned diff16(const uint16_t *a, const uint16_t *b, unsigned n) {
    unsigned i, d = 0;

    for (i = 0; i < n; i++) d += a[i] != b[i];
    return d;
}

/* The target by PVR-DMA, each address form in turn, against the SH-4's own read of the same window. */
static void dma_read(void) {
    static uint16_t ref[AB_SIZE * AB_SIZE];
    const uint32_t off = (uint32_t)(uintptr_t)target_mem & 0x00ffffffu;
    unsigned form, i;

    dma_form = -1;
    say("DMAOR %04x (want %04x) CHCR0 %08x", (unsigned)DMAC_DMAOR, DMAOR_WANT, (unsigned)DMAC_CHCR0);
    if ((DMAC_DMAOR & 0xffffu) != DMAOR_WANT) say(" DMAOR is not as expected (left as it is)");
    for (form = 0; form < 2u; form++) {
        const uint32_t base = form ? 0x05000000u : 0x04000000u;
        const volatile uint16_t *win = (const volatile uint16_t *)(uintptr_t)(0xa0000000u | base | off);
        pd_result_t r;
        unsigned changed = 0, wrong;

        for (i = 0; i < AB_SIZE * AB_SIZE; i++) { ref[i] = win[i]; dma_img[i] = 0xa5a5u; }
        pd_download(base | off, dma_img, MAP_BYTES, &r);
        for (i = 0; i < AB_SIZE * AB_SIZE; i++) changed += dma_img[i] != 0xa5a5u;
        wrong = diff16(dma_img, ref, AB_SIZE * AB_SIZE);
        say("DMA %s %s irq %d %lu us", form ? "32b area" : "64b area", r.done ? "done" : "TIMEOUT", r.irq, (unsigned long)(r.ns / 1000u));
        say(" wrote %u/%u px, %u differ from CPU read", changed, AB_SIZE * AB_SIZE, wrong);
        if (!r.done || wrong) {
            say(" PDST %x LEND %x STAPD %08x", (unsigned)r.pdst, (unsigned)r.lend, (unsigned)r.stapd);
            say(" STARD %08x CHCR0 %08x TCR0 %x", (unsigned)r.stard, (unsigned)r.chcr0, (unsigned)r.tcr0);
        }
        if (r.done && !wrong && changed && dma_form < 0) dma_form = (int)form;
    }
    if (dma_form < 0) say("DMA: no form gave the target back");
    else say("DMA: %s form returns the target exactly", dma_form ? "32-bit area" : "64-bit area");
}

/* ---- 3. the check --------------------------------------------------------------- */

static void check_one(unsigned which, int bits, int dither) {
    ab_test_t t;
    ab_stats_t st;
    float ox = 0.0f, oy = 0.0f;
    unsigned best;

    ab_test_make(&t, which, bits);
    vid_set_dithering(dither != 0);
    if (!scene(&t, bits)) { say("%2d %-4s d%d NOT DRAWN (pass refused or no end)", bits, t.name, dither); return; }
    cpu_read();
    ab_check(&t, bits, cpu_img, AB_SIZE, 0.0f, 0.0f, &st);
    say("%2d %-4s d%d bad%5u hrd%5u cov%5u max%5u", bits, t.name, dither, st.bad, st.hard, st.cover, st.max_err);
    if (!st.bad) return;
    if (bits == 16) say("  low 6 bits wrong %5u, high 10 wrong %5u", st.lo_bad, st.hi_bad);
    say("  first at %3u,%3u got %5u want %5u", st.first_x, st.first_y, st.first_got, st.first_want);
    best = ab_best_offset(&t, bits, cpu_img, AB_SIZE, &ox, &oy);
    say("  best sample offset %+.1f,%+.1f: bad %5u", (double)ox, (double)oy, best);
}

/* ---- 4. the times --------------------------------------------------------------- */

static volatile unsigned sink;

static void times(void) {
    const volatile uint16_t *vram = (const volatile uint16_t *)target_mem;
    uint64_t t0, lo, sum;
    pd_result_t r;
    unsigned i, n, s, wrong, ok;

    if (dma_form >= 0) {
        const uint32_t base = dma_form ? 0x05000000u : 0x04000000u;

        for (lo = ~0ull, sum = 0, ok = 0, n = 0; n < 8u; n++) {
            pd_download(base | ((uint32_t)(uintptr_t)target_mem & 0x00ffffffu), dma_img, MAP_BYTES, &r);
            if (!r.done) continue;
            ok++; sum += r.ns; if (r.ns < lo) lo = r.ns;
        }
        if (ok) say("DMA  32 KB: min %lu us, mean %lu us (%u runs)", (unsigned long)(lo / 1000u), (unsigned long)(sum / ok / 1000u), ok);
        else say("DMA  32 KB: no run finished");

        for (i = 0; i < BIG_BYTES; i++) big_src[i] = (uint8_t)(i * 7u + (i >> 9));
        pvr_txr_load(big_src, big_mem, BIG_BYTES);
        for (lo = ~0ull, sum = 0, ok = 0, n = 0; n < 4u; n++) {
            memset(big_dst, 0x5a, BIG_BYTES);
            pd_download(base | ((uint32_t)(uintptr_t)big_mem & 0x00ffffffu), big_dst, BIG_BYTES, &r);
            if (!r.done) continue;
            ok++; sum += r.ns; if (r.ns < lo) lo = r.ns;
        }
        if (ok && dma_form == 0) {                               /* the 64-bit area is the view the upload wrote: the bytes must be the ones sent */
            for (wrong = 0, i = 0; i < BIG_BYTES; i++) wrong += big_dst[i] != big_src[i];
            say("DMA 512 KB: %u/%u bytes differ from sent", wrong, BIG_BYTES);
        }
        if (ok) say("DMA 512 KB: min %lu us, mean %lu us (%u runs)", (unsigned long)(lo / 1000u), (unsigned long)(sum / ok / 1000u), ok);
        else say("DMA 512 KB: no run finished");
    } else {
        say("DMA times: none (no download worked)");
    }

    for (lo = ~0ull, n = 0; n < 4u; n++) {                       /* 4 texels a vertex x 5,000 vertices, straight from video memory */
        t0 = timer_ns_gettime64();
        for (s = 12345u, i = 0, wrong = 0; i < 20000u; i++) { s = s * 1664525u + 1013904223u; wrong += vram[(s >> 16) & (AB_SIZE * AB_SIZE - 1u)]; }
        t0 = timer_ns_gettime64() - t0;
        sink = wrong;
        if (t0 < lo) lo = t0;
    }
    say("20000 sparse reads, VRAM: %lu us", (unsigned long)(lo / 1000u));
    for (lo = ~0ull, n = 0; n < 4u; n++) {                       /* the same reads from the copy in RAM */
        t0 = timer_ns_gettime64();
        for (s = 12345u, i = 0, wrong = 0; i < 20000u; i++) { s = s * 1664525u + 1013904223u; wrong += ((const volatile uint16_t *)cpu_img)[(s >> 16) & (AB_SIZE * AB_SIZE - 1u)]; }
        t0 = timer_ns_gettime64() - t0;
        sink = wrong;
        if (t0 < lo) lo = t0;
    }
    say("20000 sparse reads, RAM:  %lu us", (unsigned long)(lo / 1000u));
    for (lo = ~0ull, n = 0; n < 4u; n++) {                       /* the whole map by the SH-4, 16 bits a read */
        t0 = timer_ns_gettime64();
        cpu_read();
        t0 = timer_ns_gettime64() - t0;
        if (t0 < lo) lo = t0;
    }
    say("SH-4 copy of 32 KB from VRAM: %lu us", (unsigned long)(lo / 1000u));
}

/* ---- the run -------------------------------------------------------------------- */

static void run(void) {
    static const int bits[2] = { 10, 16 };
    pvr_init_params_t init = pvr_default_params;
    ab_test_t t;
    unsigned b, w;

    nlines = 0;
    say("numbers below: measured by this run");
    say("(in an emulator they are the emulator's)");
    init.vertex_buf_size = 64 * 1024;
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return; }
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    if (!load()) { say("video memory allocation FAIL"); pvr_shutdown(); return; }

    say("1+3 RAMP into 128x128 RGB565 target, CPU read");
    say("bits test dither: bad hard cover max(codes)");
    for (b = 0; b < 2u; b++) for (w = 0; w < AB_TESTS; w++) check_one(w, bits[b], 0);
    say("FB_CFG_2 dither off %08x", (unsigned)PVR_GET(PVR_FB_CFG_2));
    for (b = 0; b < 2u; b++) check_one(AB_TEST_FLAT, bits[b], 1);
    say("FB_CFG_2 dither on  %08x", (unsigned)PVR_GET(PVR_FB_CFG_2));

    say("2 COPY BACK by PVR-DMA (PDDIR=1)");
    vid_set_dithering(false);
    ab_test_make(&t, AB_TEST_FLAT, 16);                          /* a picture with a different value at every pixel */
    if (scene(&t, 16)) { cpu_read(); dma_read(); }
    else say("no picture to copy: the pass was not made");

    say("4 TIME (KOS ns timer, smallest of the runs)");
    times();
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
