/* SH4DER-PVR: pvrx, a layer over KallistiOS's PVR driver: the Dreamcast side

   sh4der_pvrx.c
   Version: v0.05
   The effect: nothing on screen. Shorter frames where a render target pass is
     waited for, a copy of video memory in RAM, a flag when the tile
     accelerator ran out.
   Developed for: this library's own render target passes and console tests.
   Use it for: any KallistiOS program that renders to a texture every frame,
     reads a render target back, or wants to know that a frame overflowed.

   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
   Runs on KallistiOS (the KallistiOS licence, see its own LICENSE); it needs a current one
   (written against 568c3404, 2026-10-11). Nothing of KallistiOS is copied or changed.
*/

/* Every function here is lifted from a test program that ran on a console, or says that it did not. The comment over
   each names the file and the ledger row. "Measured" marks a sequence the console proved in that program: do not change
   it without measuring again. Nothing here has run on a console as this layer (the suite entry pvrx-check will).
   No maths in this file: counts, register writes and KallistiOS calls. Compiles to nothing off the Dreamcast. */

#ifdef _arch_dreamcast

#include <kos.h>
#include "sh4der_pvrx.h"

#define REG32(a) (*(volatile uint32_t *)(a))

/* ---- 1. the polled waits ---------------------------------------------------------------------------------------------
   Source: demo/psm/patches/a3e_poll.diff (rd_evt, rd_hook, scene_finish_, wait_ready_, wait_done_), the count form of
   demo/psm/compare/c4_spike.c test 8b.
   Ledger: PVR-117 (c4g, 60 frames, one run: frame 39,909 -> 23,264 us; each wait back 7 to 8 us after its interrupt,
   against 2.9 to 9.9 ms asleep), errata K8. PVR-136: in the town the same code did not pay (the wait runs on one frame
   in six) and one part got 2.8 ms slower (Q79, open).
   Replaces: pvr_wait_ready and pvr_wait_render_done, which sleep and wake on the 100 Hz tick.
   Fallback: not hooked, the render-done wait is KallistiOS's.
   Interrupts stay on: the spin reads a counter the handler writes. */

static asic_evt_handler_entry_t rd_prev;
static volatile uint32_t rd_n;            /* render-done events seen */
static uint32_t rd_want;                  /* scenes finished since the hook */
static int rd_hooked;
static int rd_wake;                       /* hook mode 2: also reschedule at once (thd_schedule), no polling */
static unsigned long wait_timeouts;

/* Measured: KallistiOS's handler first, then the count. Do not change without re-measuring. */
static void rd_evt(uint32_t code, void *data) {
    (void)data;
    if (rd_prev.hdl) rd_prev.hdl(code, rd_prev.data);
    rd_n++;
    if (rd_wake) thd_schedule(true);        /* KallistiOS's handler woke the sleeper (genwait_wake_all) but does not reschedule here, so the wake waited for the next 10 ms tick; its ready-wait handler does reschedule (pvr_irq.c). PROTOTYPE, NOT RUN. */
}

int sh4der_pvrx_wait_hook(int on) {
    rd_wake = on != 0;                       /* the polled wait is dropped: any hook is the woken wait (owner, 2026-11-28; the polled results were void, see below) */
    if (on && !rd_hooked) {
        pvr_wait_render_done();           /* a render still in flight would fire the handler after the counters are zeroed: rd_n one ahead of rd_want for ever, the polled wait then returns at once and the backstop sleeps (found by reading waitpoll_spike's mode change, DERIVED, NOT run). Returns at once when idle. */
        rd_n = 0u; rd_want = 0u; wait_timeouts = 0ul;
        rd_prev = asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, rd_evt, NULL);
        rd_hooked = rd_prev.hdl != NULL;
        if (!rd_hooked) asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, NULL, NULL);   /* none to chain: KallistiOS's waits */
    } else if (!on && rd_hooked) {
        asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, rd_prev.hdl, rd_prev.data);    /* KallistiOS's own back */
        rd_hooked = 0;
    }
    return rd_hooked;
}

static int scene_wrapped;                 /* 1 once the link-time wrap (src/sh4der_pvrx_wrap.c) has counted a scene */

/* The wrap's count: every pvr_scene_finish in the program, plain or ours, comes here after the real one. */
void sh4der_pvrx_scene_noted(void) {
    scene_wrapped = 1;
    if (rd_hooked) rd_want++;
}

int sh4der_pvrx_wrapped(void) { return scene_wrapped; }

int sh4der_pvrx_scene_finish(void) {
    const int r = pvr_scene_finish();       /* with the wrap linked this is the wrapped call, which has counted the scene already */

    if (rd_hooked && !scene_wrapped) rd_want++;
    return r;
}

int sh4der_pvrx_wait_ready_poll(uint32_t timeout_us) {
    const uint64_t t0 = timer_us_gettime64();

    while (pvr_check_ready() < 0)
        if (timer_us_gettime64() - t0 > timeout_us) { wait_timeouts++; return -1; }
    return 0;
}

int sh4der_pvrx_wait_render_done_poll(uint32_t timeout_us) {
    uint64_t t0;

    if (!rd_hooked || rd_wake) return pvr_wait_render_done();   /* mode 2: a sleeping wait, woken at once by the handler */
    t0 = timer_us_gettime64();
    while (sh4der_pvrx_wait_behind(rd_n, rd_want))
        if (timer_us_gettime64() - t0 > timeout_us) { wait_timeouts++; rd_want = rd_n; return -1; }
    return 0;
}

int sh4der_pvrx_wait_idle(void) {
    int r = pvr_wait_ready();           /* stays sleeping: a console stall separates ready from render-done */

    if (sh4der_pvrx_wait_render_done_poll(SH4DER_PVRX_WAIT_US) < 0) r = -1;
    if (pvr_wait_render_done() < 0) r = -1;   /* backstop: returns at once when idle (DERIVED from reading pvr_scene.c, NOT run) */
    return r;
}

unsigned long sh4der_pvrx_wait_timeouts(void) { return wait_timeouts; }

/* ---- 2. video memory to RAM by PVR-DMA -------------------------------------------------------------------------------
   Source: demo/psm/bench/bench_h1.c (pd_start, pd_busy, pd_end), the same in demo/psm/compare/c4_spike.c with its test 7
   (t_dma: the end polled and by interrupt); demo/psm/spike/ab_spike.c for the 32-bit area form.
   Ledger: BUS-020 (s2: both areas, 0 bytes wrong in 512 KB), BUS-021 (h2, ended by interrupt: 32 KB 869 us, 128 KB
   3,461 us, 512 KB 13,742 us), BUS-051 and BUS-052 (c4 and c4b test 7: every copy right; 128 KB 3.4 to 4.0 ms,
   512 KB 13.5 to 14.5 ms), BUS-026 (the end is ISTNRM bit 11, event 0x000b; KallistiOS's ASIC_EVT_PVR_DMA is another
   DMA's). demo/psm/ROUTE_AB_NOTES.md is the reading behind it.
   Replaces: nothing. KallistiOS has no download.
   Fallback: none; a refused or timed-out copy returns -1 and the caller reads video memory itself (slow) or skips. */

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
#define EVT_PVR_DMA_END 0x000bu            /* the same bit as an event code: register 0, bit 11 */

static volatile uint32_t dma_ended;

static void dma_evt(uint32_t code, void *data) {
    (void)code; (void)data;
    dma_ended = 1u;
}

/* Measured (bench_h1.c pd_start): the order and the values of these writes. Do not change without re-measuring. */
int sh4der_pvrx_vram_download_start(void *dst, pvr_ptr_t vram, uint32_t bytes, int area) {
    if (!vram || !sh4der_pvrx_dma_ok(dst, bytes)) return -1;
    dcache_purge_range((uintptr_t)dst, bytes);
    SB_PDEN = 0;
    SB_ISTNRM = ISTNRM_PVR_DMA;
    SB_PDAPRO = 0x6702007fu;
    SB_PDSTAP = sh4der_pvrx_dma_src((uint32_t)(uintptr_t)vram, area);
    SB_PDSTAR = sh4der_pvrx_dma_dst((uint32_t)(uintptr_t)dst);
    SB_PDLEN = bytes;
    SB_PDDIR = 1;
    SB_PDTSEL = 0;
    SB_PDEN = 1;
    SB_PDST = 1;
    return 0;
}

int sh4der_pvrx_vram_download_busy(void) { return (SB_PDST & 1u) != 0; }

/* Measured (bench_h1.c pd_end). Do not change without re-measuring. */
void sh4der_pvrx_vram_download_end(void) {
    if (sh4der_pvrx_vram_download_busy()) SB_PDST = 0;
    SB_PDEN = 0;
    SB_ISTNRM = ISTNRM_PVR_DMA;
}

/* The two ends of c4_spike.c t_dma. By interrupt: the handler goes on before the start and comes off after, as there.
   Polled: bench_h1.c t_pvrdma's loop, the busy bit read as fast as the timer allows (c4's test 7 read it once in
   250 us or 1 ms because h1 found that reading it slows the copy; a client that wants that uses start / busy / end). */
int sh4der_pvrx_vram_download(void *dst, pvr_ptr_t vram, uint32_t bytes, int area, int end_mode, uint32_t timeout_us) {
    const int irq = end_mode == SH4DER_PVRX_END_IRQ;
    uint64_t t0;
    int ended;

    if (!vram || !sh4der_pvrx_dma_ok(dst, bytes)) return -1;
    if (irq) { dma_ended = 0u; asic_evt_set_handler(EVT_PVR_DMA_END, dma_evt, NULL); asic_evt_enable(EVT_PVR_DMA_END, ASIC_IRQ_DEFAULT); }
    sh4der_pvrx_vram_download_start(dst, vram, bytes, area);
    t0 = timer_us_gettime64();
    if (irq) {
        while (!dma_ended && timer_us_gettime64() - t0 < timeout_us) { }
        ended = dma_ended != 0u;
        asic_evt_disable(EVT_PVR_DMA_END, ASIC_IRQ_DEFAULT); asic_evt_remove_handler(EVT_PVR_DMA_END);
    } else {
        while (sh4der_pvrx_vram_download_busy() && timer_us_gettime64() - t0 < timeout_us) { }
        ended = !sh4der_pvrx_vram_download_busy();
    }
    sh4der_pvrx_vram_download_end();
    return ended ? 0 : -1;
}

/* ---- 3. the TA's error events ----------------------------------------------------------------------------------------
   Source: demo/psm/patches/auto.diff (auto_evt_hnd, auto_hook, auto_evt_now), the town's autorun counters; the same
   handler as demo/psm/patches/hud1.diff diag_evt_hnd.
   Ledger: errata K7 and TOWN-093 (KallistiOS 568c3404 hooks these events only under PVR_RENDER_DBG and only logs them:
   an overflow drops nothing and sets no flag). The counters ran in the town's autorun images on suite s1 and s3 (they
   are how those pages say OVERFLOW).
   Replaces: nothing a program can read.
   Fallback: not hooked, the counts stay 0.
   OPT-IN, and after pvr_init: one image that hooked these events (hud1b) hung at its first frame and the cause is not
   found (the hud1c images are the bisect, not run yet). Hooked the autorun's way, after pvr_init and the load, it ran. */

static volatile uint32_t ta_evt[SH4DER_PVRX_TA_EVENTS];
static asic_evt_handler_entry_t ta_prev[SH4DER_PVRX_TA_EVENTS];
static int ta_hooked;

/* Ran on a console in this form: the count, then KallistiOS's handler if it has one. */
static void ta_evt_hnd(uint32_t code, void *data) {
    const unsigned i = (unsigned)(code - ASIC_EVT_PVR_ISP_OUTOFMEM);

    (void)data;
    if (i < SH4DER_PVRX_TA_EVENTS) {
        ta_evt[i]++;
        if (ta_prev[i].hdl) ta_prev[i].hdl(code, ta_prev[i].data);
    }
}

void sh4der_pvrx_ta_errors_hook(int on) {
    unsigned i;

    if ((on != 0) == ta_hooked) return;
    for (i = 0; i < SH4DER_PVRX_TA_EVENTS; i++) {
        const uint16_t code = (uint16_t)(ASIC_EVT_PVR_ISP_OUTOFMEM + i);

        if (on) { ta_prev[i] = asic_evt_set_handler(code, ta_evt_hnd, NULL); asic_evt_enable(code, ASIC_IRQ_DEFAULT); }
        else asic_evt_set_handler(code, ta_prev[i].hdl, ta_prev[i].data);
    }
    ta_hooked = on != 0;
}

void sh4der_pvrx_ta_errors(sh4der_pvrx_ta_t *out) {
    unsigned i;

    for (i = 0; i < SH4DER_PVRX_TA_EVENTS; i++) out->n[i] = ta_evt[i];
}

/* ---- 4. the revision -------------------------------------------------------------------------------------------------
   Source: include/sh4der_suite.h suite_rev_text (moved from demo/psm/psm_suite.h).
   Ledger: PVR-123 (suite s3, two programs: REVISION 0x0011, SB_REVISION 0x0B on our console: Holly 2.42 by Sega's table).
   Replaces: nothing; KallistiOS has no call. Through the uncached mirror, nothing written. */

void sh4der_pvrx_revision(uint32_t *revision, uint32_t *sb_revision) {
    if (revision) *revision = REG32(0xa05f8004u);
    if (sb_revision) *sb_revision = REG32(0xa05f689cu);
}

/* ---- 5. the user clip ------------------------------------------------------------------------------------------------
   Source: demo/psm/ao/ao4_spike.c user_clip (ao4-b and later). NOT RUN ON A CONSOLE YET (suite s4c was never run; s5 / s6
   pending).
   Ledger: PVR-125 (FACT: KallistiOS writes the user clip bits into every header it compiles, so Sega's CORE 27 does not
   bite); Sega's bug list CORE 18 (FACT from the document: a change of user clip takes effect one polygon too soon, every
   revision; wiki/research/pvr-bugs-as-features.md).
   Replaces: sending PVR_CMD_USERCLIP by hand with no polygon before it.
   Fallback: refused (0), nothing sent: the old clip stays. */

int sh4der_pvrx_clip_set(const pvr_poly_hdr_t *dummy, unsigned tx0, unsigned ty0, unsigned tx1, unsigned ty1) {
    static uint32_t cmd[8] __attribute__((aligned(32)));
    static pvr_vertex_t tri[3] __attribute__((aligned(32)));
    float v[3][3];
    unsigned i;

    if (!dummy || !sh4der_poly_hdr_ok((const uint32_t *)(const void *)dummy)) return 0;
    if (!sh4der_pvrx_clip_cmd(cmd, tx0, ty0, tx1, ty1, ((unsigned)vid_mode->width + 31u) >> 5, ((unsigned)vid_mode->height + 31u) >> 5)) return 0;
    if (!sh4der_pvrx_clip_dummy(v)) return 0;
    for (i = 0; i < 3u; i++) {
        tri[i].flags = i == 2u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        tri[i].x = v[i][0]; tri[i].y = v[i][1]; tri[i].z = v[i][2];
        tri[i].u = 0.0f; tri[i].v = 0.0f; tri[i].argb = 0xff000000u; tri[i].oargb = 0u;
    }
    pvr_prim((void *)(uintptr_t)dummy, sizeof *dummy);
    pvr_prim(tri, sizeof tri);
    pvr_prim(cmd, sizeof cmd);
    return 1;
}

#endif /* _arch_dreamcast */
