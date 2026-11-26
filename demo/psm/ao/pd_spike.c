/* SH4DER-PVR: the PVR-DMA download proof, video memory to main RAM, Dreamcast test program

   demo/psm/ao/pd_spike.c   (it lives here for build.py; it is no part of the AO work)
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A page for people who want to see that the PVR-DMA channel reads video memory back on a real console. It writes
   a known pattern into 512 KB of video memory, copies it to main RAM by PVR-DMA and compares every byte; prints
   the register sequence it used; and times the copy against the two things one would do otherwise:

     PVR-DMA   SB_PDSTAP = the video memory address through the 64-bit area (04xxxxxx) or the 32-bit area (05xxxxxx),
               SB_PDSTAR = the RAM address, SB_PDLEN = bytes, SB_PDDIR = 1 (to RAM), SB_PDEN = 1, SB_PDST = 1.
               Its end read from SB_PDST (polled without a pause) or taken from the ASIC's interrupt (event 0x000B).
     memcpy    the C library's copy from the same video memory through the uncached area (A4xxxxxx, A5xxxxxx)
     shz 32-byte    SH4ZAM's 32-byte block copy (shz_memcpy32_1, inline) in a loop, RAM to RAM, cached: what a copy costs
               once the data is in RAM. (shz_memcpy32 itself hands a large copy to a routine in SH4ZAM's library,
               which this build does not link.)

   for 32, 128 and 512 KB, the best of PD_RUNS runs, timed with the SH-4's cycle counter (PRFC1). The same video
   memory reads differently through the two areas (they interleave the two memory chips differently), so the 04 copy
   is compared with the pattern as written, and the 05 copy with the same bytes read by the CPU through A5xxxxxx.

   Every wait is bounded (LIMIT_NS); nothing is drawn; only PVR-DMA's registers and video memory are written. The
   fault catcher (include/sh4der_fault.h) is in. Every number comes from the run that prints it.
   The sequence is c4's test 7 and bench_h1's (pd_start), run on one console before; include/sh4der_pvrx.h holds the
   same sequence as a library call. Build: python demo/psm/ao/build.py --spike pd --tag <new tag>. */

#include <kos.h>
#include <stdarg.h>
#include <sh4zam/shz_mem.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"
#include "../psm_suite.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#ifndef PD_TAG
#define PD_TAG untagged
#endif
#define PD_STR_(x) #x
#define PD_STR(x) PD_STR_(x)
#define PD_RUNS 3u
#define PD_BYTES 524288u
#define CYC_PER_MS 200006ull                /* 200.006 MHz, MEASURED (bench s2) */

#define PG_PREFIX "pvr-dma"
#define PG_LINES_MAX 60u
#include "../psm_page.h"
#include "../psm_effects.h"
#include "ao_fx.h"
#define say pg_say
static sh4der_fault_crumbs_t crumbs;

static void step(const char *name) {
    sh4der_fault_beat(&crumbs);
    sh4der_fault_stage(&crumbs, name);
    crumbs.frame++;
}

#define LIMIT_NS 200000000ull
static inline uint64_t ns(void) { const uint64_t n = timer_ns_gettime64(); return n - n / 1000000000ull * 2502400ull; }   /* the bounded waits only */
static inline uint64_t cyc(void) { return perf_cntr_count(PRFC1); }

/* ---- PVR-DMA, video memory to RAM ---------------------------------------------------- */

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
#define EVT_PVR_DMA_END 0x000bu             /* the same bit as an event code: register 0, bit 11 */

static uint32_t last_stap, last_star, last_len;

static void pd_start(uint32_t vram_offset, int area05, void *dst, uint32_t bytes) {
    dcache_purge_range((uintptr_t)dst, bytes);          /* the DMA writes RAM behind the cache's back */
    SB_PDEN = 0;
    SB_ISTNRM = ISTNRM_PVR_DMA;
    SB_PDAPRO = 0x6702007fu;                            /* the whole of RAM allowed as the target */
    last_stap = (area05 ? 0x05000000u : 0x04000000u) | (vram_offset & 0x00ffffe0u);
    last_star = (uint32_t)(uintptr_t)dst & 0x1fffffe0u;
    last_len = bytes;
    SB_PDSTAP = last_stap;
    SB_PDSTAR = last_star;
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

static volatile uint64_t dma_done_cyc;
static void dma_evt(uint32_t code, void *data) {
    (void)code; (void)data;
    dma_done_cyc = cyc();
}

static uint8_t pattern[PD_BYTES] __attribute__((aligned(32)));      /* what was written */
static uint8_t via05[PD_BYTES] __attribute__((aligned(32)));        /* the same video memory as the CPU reads it through A5xxxxxx */
static uint8_t got[PD_BYTES] __attribute__((aligned(32)));          /* what a copy brought */
static uint8_t ram2[PD_BYTES] __attribute__((aligned(32)));

/* One PVR-DMA copy. Returns its cycles, or 0 when it did not end inside the limit. */
static uint64_t dma_once(uint32_t off, int area05, uint32_t bytes, int by_irq) {
    const uint64_t t_lim = ns();
    uint64_t c0, c1 = 0;
    int ended = 0;

    if (by_irq) { dma_done_cyc = 0; asic_evt_set_handler(EVT_PVR_DMA_END, dma_evt, NULL); asic_evt_enable(EVT_PVR_DMA_END, ASIC_IRQ_DEFAULT); }
    c0 = cyc();
    pd_start(off, area05, got, bytes);
    if (by_irq) {
        while (!dma_done_cyc && ns() - t_lim < LIMIT_NS) { }
        ended = dma_done_cyc != 0; c1 = dma_done_cyc;
        asic_evt_disable(EVT_PVR_DMA_END, ASIC_IRQ_DEFAULT); asic_evt_remove_handler(EVT_PVR_DMA_END);
    } else {
        unsigned spin = 0;

        while (pd_busy()) { if ((++spin & 1023u) == 0u && ns() - t_lim > LIMIT_NS) break; }
        ended = !pd_busy(); c1 = cyc();
    }
    pd_end();
    return ended ? c1 - c0 : 0ull;
}

static unsigned wrong_bytes(const uint8_t *a, const uint8_t *b, uint32_t n) {
    unsigned w = 0;
    uint32_t i;

    for (i = 0; i < n; i++) w += a[i] != b[i];
    return w;
}

typedef struct cell { uint64_t best; unsigned wrong; int ok; } cell_t;
#define ROWS 6u
static cell_t cells[ROWS][3];
static const char *const row_name[ROWS] = { "PVR-DMA 04 polled", "PVR-DMA 05 polled", "PVR-DMA 04 irq", "memcpy from A4", "memcpy from A5", "shz 32-byte RAM" };
static const uint32_t sizes[3] = { 32768u, 131072u, 524288u };
static unsigned whole_wrong[2];
static int have_block;
static uint32_t shown_stap, shown_star, shown_len;

static void cell_put(cell_t *c, uint64_t cycles, unsigned wrong) {
    if (!cycles) return;
    if (!c->ok || cycles < c->best) c->best = cycles;
    c->wrong += wrong; c->ok = 1;
}

static void run(void) {
    pvr_ptr_t blk;
    uint32_t off, seed = 9u, i;
    unsigned s, r;

    if (pvr_init_defaults() < 0) { say("pvr_init FAIL"); return; }
    blk = pvr_mem_malloc(PD_BYTES);
    if (!blk) { say("no video memory for the block"); pvr_shutdown(); return; }
    have_block = 1;
    off = (uint32_t)(uintptr_t)blk & 0x00ffffffu;
    for (i = 0; i < PD_BYTES; i += 4u) { seed = seed * 1664525u + 1013904223u; memcpy(pattern + i, &seed, 4u); }
    pvr_txr_load(pattern, blk, PD_BYTES);
    step("pd read a5 ");
    memcpy(via05, (const void *)(uintptr_t)(0xa5000000u | off), PD_BYTES);

    step("pd whole   ");                    /* the proof: the whole block, each area, every byte compared */
    memset(got, 0, PD_BYTES);
    whole_wrong[0] = dma_once(off, 0, PD_BYTES, 0) ? wrong_bytes(got, pattern, PD_BYTES) : PD_BYTES;
    shown_stap = last_stap; shown_star = last_star; shown_len = last_len;
    memset(got, 0, PD_BYTES);
    whole_wrong[1] = dma_once(off, 1, PD_BYTES, 0) ? wrong_bytes(got, via05, PD_BYTES) : PD_BYTES;

    for (s = 0; s < 3u; s++)
        for (r = 0; r < PD_RUNS; r++) {
            const uint32_t n = sizes[s];
            uint64_t c0, c;

            step("pd times   ");
            memset(got, 0, n); c = dma_once(off, 0, n, 0); cell_put(&cells[0][s], c, c ? wrong_bytes(got, pattern, n) : 0u);
            memset(got, 0, n); c = dma_once(off, 1, n, 0); cell_put(&cells[1][s], c, c ? wrong_bytes(got, via05, n) : 0u);
            memset(got, 0, n); c = dma_once(off, 0, n, 1); cell_put(&cells[2][s], c, c ? wrong_bytes(got, pattern, n) : 0u);
            memset(got, 0, n); c0 = cyc(); memcpy(got, (const void *)(uintptr_t)(0xa4000000u | off), n); c = cyc() - c0; cell_put(&cells[3][s], c, wrong_bytes(got, pattern, n));
            memset(got, 0, n); c0 = cyc(); memcpy(got, (const void *)(uintptr_t)(0xa5000000u | off), n); c = cyc() - c0; cell_put(&cells[4][s], c, wrong_bytes(got, via05, n));
            memset(ram2, 0, n); c0 = cyc(); for (i = 0; i < n; i += 32u) shz_memcpy32_1(ram2 + i, pattern + i); c = cyc() - c0; cell_put(&cells[5][s], c, wrong_bytes(ram2, pattern, n));
        }
    pvr_mem_free(blk);
    pvr_shutdown();
}

static unsigned us_of_cyc(uint64_t c) { return (unsigned)((c * 1000ull + CYC_PER_MS / 2ull) / CYC_PER_MS); }
static unsigned mbs10(uint32_t bytes, uint64_t c) {     /* megabytes a second x 10 (a megabyte: 1,048,576 bytes) */
    return c ? (unsigned)(((uint64_t)bytes * CYC_PER_MS * 1000ull * 10ull) / (c * 1048576ull)) : 0u;
}

static void report(void) {
    pg_plot_t *g;
    unsigned r, s, bad = 0;
    char rev[32], line[80];

    suite_rev_text(rev, sizeof rev);
    say("PVR-DMA DOWNLOAD PROOF %s: video memory to main RAM", PD_STR(PD_TAG));
    say("%s", rev);
    if (!have_block) { say("THE TEST DID NOT RUN (see the line above)"); return; }
    for (r = 0; r < ROWS; r++) for (s = 0; s < 3u; s++) bad += !cells[r][s].ok || cells[r][s].wrong;
    say("PROVES: this console's PVR-DMA channel copies video memory");
    say("to RAM bit for bit: %s", whole_wrong[0] == 0u && whole_wrong[1] == 0u && !bad ? "YES, every copy below is right" : "NO, SEE THE WRONG COUNTS");
    say("Pattern: 524,288 bytes, written with pvr_txr_load, then");
    say(" by DMA through 04: %u of 524,288 bytes wrong", whole_wrong[0]);
    say(" by DMA through 05: %u wrong (against the CPU's read at A5)", whole_wrong[1]);
    say("Registers, in this order (the ASIC's, at A05F7Cxx):");
    say(" SB_PDEN=0  SB_ISTNRM=00000800 (clear)  SB_PDAPRO=6702007F");
    say(" SB_PDSTAP=%08X  video memory, a multiple of 32", (unsigned)shown_stap);
    say(" SB_PDSTAR=%08X  RAM, a multiple of 32, cache purged", (unsigned)shown_star);
    say(" SB_PDLEN=%08X  bytes, a multiple of 32", (unsigned)shown_len);
    say(" SB_PDDIR=1 (to RAM)  SB_PDTSEL=0  SB_PDEN=1  SB_PDST=1");
    say(" end: SB_PDST bit 0 reads 0, or ASIC event 000B; SB_PDEN=0");
    say("Best of %u, us and MB/s: 32 KB      128 KB       512 KB", PD_RUNS);
    for (r = 0; r < ROWS; r++) {
        unsigned n = (unsigned)snprintf(line, sizeof line, "%-17s", row_name[r]);

        for (s = 0; s < 3u && n < sizeof line; s++) {
            const cell_t *c = &cells[r][s];

            if (!c->ok) n += (unsigned)snprintf(line + n, sizeof line - n, "       NO END");
            else n += (unsigned)snprintf(line + n, sizeof line - n, " %5u %3u.%u%c", us_of_cyc(c->best), mbs10(sizes[s], c->best) / 10u, mbs10(sizes[s], c->best) % 10u, c->wrong ? '!' : ' ');
        }
        say("%s", line);
    }
    say("! = wrong bytes. Cycle counter. 04 / 05: 64 / 32-bit views.");
    g = pg_plot("PVR-DMA proof: 512 KB to RAM, MB/s", "way", "MB/s", 1);
    pg_cat(g, "DMA 04"); pg_cat(g, "DMA 05"); pg_cat(g, "DMA irq"); pg_cat(g, "memcpy A4"); pg_cat(g, "memcpy A5"); pg_cat(g, "RAM copy");
    pg_series(g, "MB/s");
    for (r = 0; r < ROWS; r++) pg_point(g, (float)r, cells[r][2].ok ? (float)mbs10(sizes[2], cells[r][2].best) * 0.1f : 0.0f);
}

int main(void) {
    unsigned page = 0, pages, held = ~0u;
    int redraw = 1;

    suite_init();                           /* the suite disc: the menu's binary into RAM, before anything else */
    vid_set_mode(DM_640x480, PM_RGB565);
    pg_init();
    perf_cntr_start(PRFC1, PMCR_ELAPSED_TIME_MODE, PMCR_COUNT_CPU_CYCLES);
    if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused");
    run();
    step("pd pages   ");
    report();
    vid_set_mode(DM_640x480, PM_RGB565);
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;

        sh4der_fault_beat(&crumbs);
        held = now;
        pages = pg_pages();
        if (hit & CONT_A) { page = (page + 1u) % pages; redraw = 1; }
        else if (hit & CONT_B) { page = (page + pages - 1u) % pages; redraw = 1; }
        else if (suite_tick(now, page, pages)) { page++; redraw = 1; }
        if (redraw) { pg_draw(page % pages, "PVR-DMA PROOF", suite_keys_text("A B pages")); fx_page(pd_fx_page, PD_FX_PAGE_N); redraw = 0; }
        thd_sleep(20);
    }
    return 0;
}
