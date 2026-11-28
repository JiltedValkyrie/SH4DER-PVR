/* SH4DER-PVR: pvrx, a layer over KallistiOS's PVR driver

   sh4der_pvrx.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
   Runs on KallistiOS (the KallistiOS licence, see its own LICENSE); it needs a current one
   (written against 568c3404, 2026-10-11). Nothing of KallistiOS is copied or changed.
*/

/** \file    sh4der_pvrx.h
    \brief   Five things this project's console tests showed KallistiOS's PVR driver does not
             do, or does slowly, gathered from the test programs into one place.

    It is a layer: it calls KallistiOS and reads or writes Holly's registers beside it. It
    does not fork KallistiOS and edits none of its files. Every Dreamcast function has a
    fallback to KallistiOS's own call, or refuses and says so.

    Each function below is LIFTED from a test program that ran; its comment names the file
    it came from and the ledger row (wiki/hardware/) with the number. The parts marked
    "measured: do not change without re-measuring" are the sequences the console proved.
    The proof is of the test program, not of this file: nothing here has run on a console
    AS THIS LAYER. The version stays v0.05 until a suite entry (pvrx-check) proves each call.

    | Call | What KallistiOS does | What this does | Ledger | Status |
    |---|---|---|---|---|
    | sh4der_pvrx_wait_render_done_poll | pvr_wait_render_done sleeps; it wakes on the 100 Hz tick, 2.9 to 9.9 ms late | spins on this layer's count of render-done events; 7 to 8 us late | PVR-117, PVR-136, errata K8 | ran in two test programs |
    | sh4der_pvrx_vram_download | no call | PVR-DMA, video memory to RAM, about 37 MB/s | BUS-020, BUS-021, BUS-051, BUS-052 | ran in four test programs |
    | sh4der_pvrx_ta_errors | hooks the TA's error events only under PVR_RENDER_DBG, and only logs | counts them after KallistiOS's handler, gives an overflow flag | errata K7, TOWN-093 | ran in the town's autorun images |
    | sh4der_pvrx_revision | no call | reads REVISION and SB_REVISION | PVR-123 | ran in two test programs |
    | sh4der_pvrx_clip_set | PVR_CMD_USERCLIP is the client's to send | a dummy polygon, then the clip (Sega's bug list, CORE 18) | PVR-125 | NOT run on a console yet |

    When to poll (PVR-136): the polled wait paid where a render-done wait runs on every
    frame, three times (39.9 to 23.3 ms a frame, one run). In a program whose wait ran on
    one frame in six it did not pay, and one part of that program got 2.8 ms slower for a
    reason not found (Q79). Measure both in your own frame.

    Not in v1 (design only, see the end of this file): a render target pass through a tile
    list cut to the target (PVR-092); the 480-row limit of a target (PVR-026).

    The inline part (alignment, addresses, the counters' arithmetic, the clip words, the
    revision table) is plain C with no KallistiOS in it: test/pvrx_test.c checks it on a
    host. The rest is src/sh4der_pvrx.c, Dreamcast only.

    Version: v0.05
    The effect: nothing on screen. Shorter frames where a render target pass is waited
      for, a copy of video memory in RAM, a flag when the tile accelerator ran out.
    Developed for: this library's own render target passes and console tests.
    Use it for: any KallistiOS program that renders to a texture every frame, reads a
      render target back, or wants to know that a frame overflowed.

    \author Jilted Valkyrie
*/

#ifndef SH4DER_PVRX_H
#define SH4DER_PVRX_H

#include <stddef.h>
#include <stdint.h>
#include "sh4der_guard.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_PVRX_WAIT_US       100000u   /**< \brief A polled wait gives up after this long (the town's a3e-poll value). */
#define SH4DER_PVRX_DMA_US        200000u   /**< \brief A download gives up after this long (the benches' limit; 512 KB takes about 14 ms). */
#define SH4DER_PVRX_DMA_UNIT      32u       /**< \brief PVR-DMA moves 32-byte units: the destination's alignment and the length's step. */
/** @} */

/* ---- 1. the polled wait: the count ---------------------------------------------------- */

/** \brief 1 while fewer render-done events were seen than scenes were finished.
    Both are counts that wrap; the difference as a signed number is right as long as fewer
    than 2^31 scenes are outstanding. From demo/psm/patches/a3e_poll.diff, wait_done_
    (ran on a console, PVR-136). */
static inline int sh4der_pvrx_wait_behind(uint32_t seen, uint32_t want) {
    return (int32_t)(seen - want) < 0;
}

/* ---- 2. the download: addresses ------------------------------------------------------- */

#define SH4DER_PVRX_AREA_64   0   /**< \brief Video memory through the 64-bit area (0x04000000): what a texture and a render target use. */
#define SH4DER_PVRX_AREA_32   1   /**< \brief Through the 32-bit area (0x05000000). */

#define SH4DER_PVRX_END_POLL  0   /**< \brief The end is read from the busy bit (SB_PDST). */
#define SH4DER_PVRX_END_IRQ   1   /**< \brief The end is taken from the interrupt (ISTNRM bit 11, event 0x000b). */

/** \brief 1 when PVR-DMA can fill this destination: not null, 32-byte aligned, a length
    that is a whole number of 32-byte units and not 0. The address is the pointer's value;
    nothing is read. */
static inline int sh4der_pvrx_dma_ok(const void *dst, uint32_t bytes) {
    const uintptr_t a = (uintptr_t)dst;

    return a != 0u && (a & (SH4DER_PVRX_DMA_UNIT - 1u)) == 0u && bytes != 0u && (bytes & (SH4DER_PVRX_DMA_UNIT - 1u)) == 0u;
}

/** \brief The length rounded up to whole 32-byte units: the size to give the buffer. */
static inline uint32_t sh4der_pvrx_dma_round(uint32_t bytes) {
    return (bytes + (SH4DER_PVRX_DMA_UNIT - 1u)) & ~(SH4DER_PVRX_DMA_UNIT - 1u);
}

/** \brief The word for SB_PDSTAP: the video memory address in the chosen area.
    From demo/psm/bench/bench_h1.c pd_start (64-bit form) and demo/psm/spike/ab_spike.c
    (both forms; BUS-020: 0 bytes wrong in 512 KB through each).
    Measured: do not change without re-measuring. */
static inline uint32_t sh4der_pvrx_dma_src(uint32_t vram_addr, int area) {
    return (area == SH4DER_PVRX_AREA_32 ? 0x05000000u : 0x04000000u) | (vram_addr & 0x00ffffffu);
}

/** \brief The word for SB_PDSTAR: the RAM address with the segment bits and the low five
    bits off. From bench_h1.c pd_start. Measured: do not change without re-measuring. */
static inline uint32_t sh4der_pvrx_dma_dst(uint32_t ram_addr) {
    return ram_addr & 0x1fffffe0u;
}

/* ---- 3. the TA's error events: the counts --------------------------------------------- */

/** \brief The six events, in the order of their codes 0x0200 to 0x0205. */
enum {
    SH4DER_PVRX_TA_ISP = 0,        /**< ISP / TSP parameters out of memory (the vertex buffer). */
    SH4DER_PVRX_TA_STRIP,          /**< Halt on a strip buffer error. */
    SH4DER_PVRX_TA_PARAM,          /**< TA parameters out of memory (the vertex buffer). */
    SH4DER_PVRX_TA_OPB,            /**< Object pointer blocks past their end (the bin pool). */
    SH4DER_PVRX_TA_INPUT_ERR,      /**< A vertex input error. */
    SH4DER_PVRX_TA_INPUT_OVERFLOW, /**< The input queue overflowed. */
    SH4DER_PVRX_TA_EVENTS
};

/** \brief Event counts since the hook (or since boot of the layer); they wrap. */
typedef struct sh4der_pvrx_ta {
    uint32_t n[SH4DER_PVRX_TA_EVENTS];
} sh4der_pvrx_ta_t;

/** \brief out = now - base, per event: the events between two readings (wrap safe).
    From demo/psm/patches/auto.diff, psm_auto_frame (the town's autorun: ran on a console). */
static inline void sh4der_pvrx_ta_since(const sh4der_pvrx_ta_t *now, const sh4der_pvrx_ta_t *base, sh4der_pvrx_ta_t *out) {
    unsigned i;

    for (i = 0; i < SH4DER_PVRX_TA_EVENTS; i++) out->n[i] = now->n[i] - base->n[i];
}

/** \brief 1 when a memory ran out in these counts: ISP, PARAM, OPB or the input queue.
    What was sent last in that frame is lost (TOWN-093): KallistiOS drops nothing and says
    nothing. */
static inline int sh4der_pvrx_ta_overflow(const sh4der_pvrx_ta_t *t) {
    return (t->n[SH4DER_PVRX_TA_ISP] | t->n[SH4DER_PVRX_TA_PARAM] | t->n[SH4DER_PVRX_TA_OPB] | t->n[SH4DER_PVRX_TA_INPUT_OVERFLOW]) != 0u;
}

/** \brief 1 when any of the six events is in these counts. */
static inline int sh4der_pvrx_ta_any(const sh4der_pvrx_ta_t *t) {
    uint32_t all = 0u;
    unsigned i;

    for (i = 0; i < SH4DER_PVRX_TA_EVENTS; i++) all |= t->n[i];
    return all != 0u;
}

/* ---- 4. the revision: the table ------------------------------------------------------- */

/** \brief Holly's version as text from the two registers' values, by Sega's table (System
    Architecture p.402, Table 9-1; PVR-123): "1.5", "2.2", "2.3", "2.4 or 2.41" (they read
    the same), "2.42", or "unknown". REVISION alone says only 1.5 or 2.x. */
static inline const char *sh4der_pvrx_revision_name(uint32_t revision, uint32_t sb_revision) {
    const uint32_t core = revision & 0xffffu, sb = sb_revision & 0xffu;

    if (core == 0x0001u && sb == 0x02u) return "1.5";
    if (core != 0x0011u) return "unknown";
    switch (sb) {
    case 0x08u: return "2.2";
    case 0x09u: return "2.3";
    case 0x0au: return "2.4 or 2.41";
    case 0x0bu: return "2.42";
    default: return "unknown";
    }
}

/* ---- 5. the user clip: the words ------------------------------------------------------ */

#define SH4DER_PVRX_CMD_USERCLIP 0x20000000u   /**< \brief The user clip parameter's command word (KallistiOS: PVR_CMD_USERCLIP). */

/** \brief The tiles (32 pixels) that hold the pixels x0 .. x1 - 1 and y0 .. y1 - 1, both
    ends counted, as the clip wants them. 0 when the rectangle is empty. */
static inline int sh4der_pvrx_clip_tiles(unsigned x0, unsigned y0, unsigned x1, unsigned y1, unsigned tile[4]) {
    if (x1 <= x0 || y1 <= y0) return 0;
    tile[0] = x0 >> 5; tile[1] = y0 >> 5; tile[2] = (x1 - 1u) >> 5; tile[3] = (y1 - 1u) >> 5;
    return 1;
}

/** \brief The user clip parameter's eight words for a rectangle in tiles, both ends counted.
    0 (and nothing written) when the rectangle is turned round or leaves a screen of
    tiles_x by tiles_y tiles (20 by 15 at 640 x 480). From demo/psm/ao/ao4_spike.c
    user_clip (NOT run on a console yet). */
static inline int sh4der_pvrx_clip_cmd(uint32_t cmd[8], unsigned tx0, unsigned ty0, unsigned tx1, unsigned ty1, unsigned tiles_x, unsigned tiles_y) {
    if (tx0 > tx1 || ty0 > ty1 || tx1 >= tiles_x || ty1 >= tiles_y) return 0;
    cmd[0] = SH4DER_PVRX_CMD_USERCLIP; cmd[1] = cmd[2] = cmd[3] = 0u;
    cmd[4] = tx0; cmd[5] = ty0; cmd[6] = tx1; cmd[7] = ty1;
    return 1;
}

/** \brief The dummy triangle that goes before a change of clip: three screen points
    (x, y, 1 / w) with area, off the screen's left edge, so it draws nothing. A change of
    user clip takes effect one polygon too soon on every revision (Sega's bug list, CORE 18):
    this polygon takes the early change in place of the client's last one. Returns 1 when
    the triangle has area by sh4der_tri_area_ok (a triangle with none is drawn as a line on
    a console and must not be sent). From ao4_spike.c user_clip (NOT run on a console yet). */
static inline int sh4der_pvrx_clip_dummy(float v[3][3]) {
    float a[3], b[3], c[3];

    v[0][0] = -64.0f; v[0][1] = 8.0f;  v[0][2] = 0.001f;
    v[1][0] = -64.0f; v[1][1] = 40.0f; v[1][2] = 0.001f;
    v[2][0] = -32.0f; v[2][1] = 8.0f;  v[2][2] = 0.001f;
    a[0] = v[0][0]; a[1] = v[0][1]; a[2] = 1.0f;       /* screen points: the guard's depth is 1, so its area is in pixels */
    b[0] = v[1][0]; b[1] = v[1][1]; b[2] = 1.0f;
    c[0] = v[2][0]; c[1] = v[2][1]; c[2] = 1.0f;
    return sh4der_tri_area_ok(a, b, c, 0.5f, 1.0f);
}

/* ---- the Dreamcast side (src/sh4der_pvrx.c) ------------------------------------------- */

#ifdef _arch_dreamcast

#include <dc/pvr.h>

/** \brief Put this layer's handler on the render-done event, KallistiOS's own kept and
    called first; on = 0 puts KallistiOS's back. Call it after pvr_init, and again after any
    later pvr_init. on = 1: the polled wait. on = 2 (PROTOTYPE, NOT RUN): the waits stay KallistiOS's sleeping ones, and
    the handler also calls thd_schedule(true), so the sleeper resumes at once instead of at the next 10 ms tick: no
    spinning, other threads keep their time. The same hook point and the same forward to KallistiOS's handler (an
    idea of Falco Girgis, KallistiOS). 1 when the hook is live; 0 when there was no handler to chain
    (the PVR is not started): the waits then fall back to KallistiOS's. */
int sh4der_pvrx_wait_hook(int on);

/** \brief pvr_scene_finish, counted: the polled wait waits for one render-done event a
    finished scene. Use it for EVERY scene (screen and render target) while the hook is on. */
int sh4der_pvrx_scene_finish(void);

/** \brief OPTIONAL link-time wrap, so a plain pvr_scene_finish is counted too (PROTOTYPE, NOT RUN on a console until
    build ra7). Compile src/sh4der_pvrx_wrap.c into the program and link with -Wl,--wrap=pvr_scene_finish: every
    pvr_scene_finish call in the binary then goes to __wrap_pvr_scene_finish, which calls the real one and counts the
    scene (sh4der_pvrx_scene_noted). With no wrap flag nothing changes and sh4der_pvrx_scene_finish counts as before. A
    program that links the wrap file but not the flag, or the flag but not the file, fails to link: it never half works.
    sh4der_pvrx_wrapped says 1 once the wrap has counted a scene; read it after a scene has finished. */
void sh4der_pvrx_scene_noted(void);
int sh4der_pvrx_wrapped(void);

/** \brief Wait until the PVR takes a new scene, spinning on pvr_check_ready.
    0 ready, -1 timed out (counted). */
int sh4der_pvrx_wait_ready_poll(uint32_t timeout_us);

/** \brief Wait for the renders of every scene finished so far, with no sleep.
    0 done, -1 timed out (counted; the count is resynchronised). Not hooked: KallistiOS's
    pvr_wait_render_done and its result. */
int sh4der_pvrx_wait_render_done_poll(uint32_t timeout_us);

/** \brief The whole "no render is reading or writing now" wait in one call: pvr_wait_ready, the polled render-done
    wait, then KallistiOS's pvr_wait_render_done as a backstop. Use it where the old code called
    pvr_wait_ready() + pvr_wait_render_done() (before a texture or tile-table rewrite, before a render target). With
    the hook off it is KallistiOS's two waits. 0 done, -1 when a wait failed or timed out (what the old
    `ready = pvr_wait_ready(); ready |= pvr_wait_render_done();` showed as < 0). The client ends its scenes through sh4der_pvrx_scene_finish or the wrap. */
int sh4der_pvrx_wait_idle(void);

/** \brief Polled waits that timed out since the hook. */
unsigned long sh4der_pvrx_wait_timeouts(void);

/** \brief Start a PVR-DMA copy of `bytes` from video memory at `vram` to `dst`.
    dst: 32-byte aligned RAM, bytes a multiple of 32 (sh4der_pvrx_dma_ok). 0 started,
    -1 refused. Do not read dst until the copy ended. The PVR must not be rendering into
    that video memory: wait for the render first. */
int sh4der_pvrx_vram_download_start(void *dst, pvr_ptr_t vram, uint32_t bytes, int area);

/** \brief 1 while the copy runs (the busy bit). Reading it often slows the copy (bench h1). */
int sh4der_pvrx_vram_download_busy(void);

/** \brief Stop a copy that still runs, switch PVR-DMA off, clear its interrupt bit. */
void sh4der_pvrx_vram_download_end(void);

/** \brief The whole copy: start, wait for the end (end_mode: SH4DER_PVRX_END_POLL or
    _END_IRQ), end. 0 copied, -1 refused or timed out (the copy is stopped). */
int sh4der_pvrx_vram_download(void *dst, pvr_ptr_t vram, uint32_t bytes, int area, int end_mode, uint32_t timeout_us);

/** \brief Count the TA's six error events (0x0200 to 0x0205), KallistiOS's handler for
    each kept and called after the count; on = 0 puts KallistiOS's back. OPT-IN: nothing
    calls this for you. Call it after pvr_init, and again after any later pvr_init. */
void sh4der_pvrx_ta_errors_hook(int on);

/** \brief The counts now. Take one at a frame's start and one at its end, then
    sh4der_pvrx_ta_since and sh4der_pvrx_ta_overflow. */
void sh4der_pvrx_ta_errors(sh4der_pvrx_ta_t *out);

/** \brief The PVR core's REVISION (0x005F8004) and the system bus's SB_REVISION
    (0x005F689C). Two reads, nothing written; the PVR need not be started. */
void sh4der_pvrx_revision(uint32_t *revision, uint32_t *sb_revision);

/** \brief Change the user clip inside an open list: the dummy triangle through `dummy`
    (a compiled header of that list: untextured, any clip mode), then the clip in tiles,
    both ends counted. Send your own batched vertices first. 1 sent, 0 refused (the header
    was never compiled, or the rectangle is wrong for the screen): nothing was sent. */
int sh4der_pvrx_clip_set(const pvr_poly_hdr_t *dummy, unsigned tx0, unsigned ty0, unsigned tx1, unsigned ty1);

#endif /* _arch_dreamcast */

/* ---- not in v1: design only -----------------------------------------------------------

   A. A render target pass through a tile list cut to the target.
      What KallistiOS does: pvr_scene_begin_rtt renders through the screen's whole tile
      list, so a target pass costs about 3.2 ms of PVR time whatever its size (PVR-004).
      Measured once, by rewriting KallistiOS's tile tables in video memory for one pass and
      putting them back: a 128 x 130 map pass 3,312 -> 375 us (20 tiles), a 320 x 240 mask
      pass 6,130 -> 3,760 us (80 tiles), the same picture (PVR-092; the savings as
      arithmetic, PVR-093). Open: Q54 (the cut in the library's passes), Q11 (why the floor).
      Why it is not here: it needs KallistiOS's tile tables rewritten every pass, with a
      word-for-word restore; a layer cannot own those tables safely. It is a candidate for a
      KallistiOS patch, and a spike of its own. The call it would be:
        int sh4der_pvrx_rtt_begin(pvr_ptr_t target, unsigned w, unsigned h, unsigned pitch);   // cut list in
        int sh4der_pvrx_rtt_finish(void);                                                      // whole list back
      with the fallback pvr_scene_begin_rtt when the tables are not where the layer expects.
      The spike: include/sh4der_tilecut.h (sh4der_tilecut_find, sh4der_tilecut_begin, sh4der_tilecut_end; the
      table logic host tested by test/tilecut_test.c), scope and risks in
      demo/psm/TILE_CUT_SCOPE.md. Its per-pass use is NOT console proven. It refuses a table
      that does not read as KallistiOS 568c3404 lays it out, and its end returns the words
      it could not put back (not 0: shut the PVR down and start it again).
      The caller's condition: no rewrite while a render that reads the table is running. A
      program that waits for render-done after every pass holds it; one that overlaps scenes
      does not, and that case needs a change inside KallistiOS.
      It moves into this file when a console run has proven the per-pass use.

   B. The 480-row limit. A KallistiOS render target does not render past row 479 (PVR-026):
      keep a target at most 480 rows. And render 2 rows taller than the map you need: the
      last 32 bytes of a render arrive one render late (PVR-021). Rules for the caller; a
      later sh4der_pvrx_rtt_begin would refuse the first and add the rows of the second.
*/

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_PVRX_H */
