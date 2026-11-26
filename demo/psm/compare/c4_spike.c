/* SH4DER-PVR: the chain spike (c4), the Dreamcast test program

   demo/psm/compare/c4_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A test program for a real console. Spike c3b showed three things the PVR does by itself: the bump unit gives
   128 + 127 cos(texel - Q) in alpha, a render target can be the bump texture, and a target written as ARGB1555 keeps
   one bit by a threshold. This program joins them into a whole shadow chain on a small town-like scene (c4_model.h:
   a ground, a house, a wall with an eave, a hen, a post) with no depth compare and no raster on the SH-4, and no
   modifier volume:

     MAP     the faces turned to the sun, drawn from the sun into a 128 x 128 target through a ramp texture whose
             texel is the depth code; the PVR's own depth test keeps the nearest.
     MASK    the scene as the camera sees it, into a 320 x 240 target: an opaque pass for depth, then the same
             polygons with the map as a bump texture and one Q byte a polygon (the receivers cut into bands of equal
             depth code); the target is written as ARGB1555 with the alpha threshold: the alpha bit is the shadow.
     SCREEN  the scene, and one layer textured with the mask that darkens where the bit is set.

   The SH-4's part: cut the bands when the sun moves, put every band vertex on the screen (shz_dce_pvr_project of
   SH4ZAM-DCE: FTRV and FSRRA, an asm loop), and send the blocks to the tile accelerator by store queue.

   It runs its steps once and prints numbers on result pages (A next, B back) and on the serial line ("c4:"):

     0 RAMP    the depth code through the ramp texture, plain (1024 texels) and with 8 sub-steps a texel through v
               (the texture coordinate is a constant over the pixel's own depth, so the PVR's perspective divide
               gives code-linear lookup): pixels whose code is not the nearest code. The better ramp is used after.
     1 WINDOW  every map code against 160 Q values, at window half-widths 16, 32, 64: pixels that differ from the
               model's level table, how many of those sit within 2 levels of the threshold, and pixels the second
               pass did not draw at equal depth (holes).
     3 BIAS    the ground alone (nothing casts): shadow pixels for b0 = 0, 0.5, 1, 1.5, 2, 3. All of it is acne; the
               least b0 with 0 is the bias this console needs.
     4 SCENE   the whole chain at two suns: the map against the model's map; the mask against the model run on the
               console's own map; the mask against a ray to the sun (acne, edge, rim, lit in bias, other).
     5 COST    SH-4: band cut, projection (the asm loop against its C form: cycles a vertex, and whether the bytes are
               the same), sends. PVR: render time of each pass.
     6 PUNCH   punch-through with KallistiOS's own header: which one setting made c3b's test draw everything, and
               whether the bump level works as the tested alpha.
     7 DMA     PVR-DMA from video memory to RAM, 128 KB and 512 KB, the end polled once a millisecond, four times a
               millisecond, and taken by interrupt (h1 found the status polling itself slows the transfer).
     8 FRAME   (c4b) 60 frames of the live chain, twice: with KallistiOS's waits, and with the "ready" waits polled.
               Each frame's pictures (vertical blanks counted by a handler of this program) and each wait timed apart:
               why c4's frames took 39.9 and 49.9 ms for 19 to 27 ms of work (OPEN after c4).
     9 ROUTE   (c4b) the mask made the second way: the bump level tested by punch-through (reference = the threshold)
               instead of the 1555 write threshold, at two suns; pixels that differ from the 1555 route, and the
               PVR's time for each.

   c4b, after c4's two runs (compare/NOTES.md):
     - the band vertices (bsrc, bscr, bref) sit in one pool at offsets that are not a multiple of the 16 KB operand
       cache apart; c4 had them exactly 256 KB apart (SH4-046). The run prints their addresses.
     - fix 4: test 1's picture at window 32 gives, for every difference (M - Q) & 255, how many pixels read shadow;
       the window's two edges are taken from it at boot (c4_edges), and from then on Q is made from the measured
       lower edge (c4_q_set). Tests 3 and 4 run with the cosine's edge AND the measured one, so the page shows both.
     - test 8 and test 9 above; the punch-through route is also a switch for the live view (C4B_MASK_PT, default 0).

   START on the result pages: the live view (d-pad the sun, A band width, X / Y bias down / up, B the shadow layer
   on / off, START back; the last settings and their costs are added to the pages).
   Y on the result pages, once, LAST: the tile list stage. KallistiOS renders a target through the screen's 300
   tiles; this stage rewrites the two tile tables in video memory to end after the target's own tiles (20 for the
   map, 80 for the mask), renders, and puts every word back (read back and compared). Photograph the pages first.

   Only PVR and system-bus DMA registers and video memory are written. The fault catcher (include/sh4der_fault.h) is in;
   every wait is bounded. Every number comes from the run that prints it; only a real console's run is evidence.
   c4c, after c4b's run (compare/NOTES.md "c4b: hardware result"):
     - test 8 has a third run, 8b: the render-done wait polled too. This program's handler sits in front of
       KallistiOS's for the render-done event (it calls KallistiOS's, then notes the time), so every done-wait also
       prints how long after the interrupt it came back and the vertical blanks that passed in it.
     - page S: both scenes at b0 0 to 3 by 0.5 with Q from the measured edge. Test 3 has b0 2.5 and bands 2 wide too.
     - page C: the band cut timed in parts by the cycle counter, with Q from each edge (c4b's cut was 22 to 28 %
       slower than c4's: OPEN).

   Build: python demo/psm/compare/build.py --spike c4 --tag <new tag> --sh4zam <checkout>. c4b ran on a console on
   2026-11-25; c4c: not run on a console when this file was written.
*/

#include <kos.h>
#include <stdarg.h>

#include <sh4zam_dce/shz_dce_pvr.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"
#include "../../../include/sh4der_suite.h"
#include "c4_model.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

/* ---- the result pages ----------------------------------------------------------- */

#define PG_PREFIX "c4"
#include "../../../include/sh4der_page.h"
#define say pg_say
static sh4der_fault_crumbs_t crumbs;

static void step(const char *name) {
    sh4der_fault_beat(&crumbs);
    sh4der_fault_stage(&crumbs, name);
    crumbs.frame++;
}

/* ---- clocks ------------------------------------------------------------------------ */

#define LIMIT_NS 200000000ull              /* no wait in this file is longer */

/* KallistiOS reloads TMU2 each 997,497,600 ns of its own count and then adds a whole second, so its ns clock steps
   2,502,400 ns ahead once a reload (MEASURED 2026-11-25, one console, image ray2 CLOCK.4: largest step 2,502,321 ns).
   An interval across a reload would read 2.5 ms long; this takes the step out. */
static inline uint64_t ns(void) { const uint64_t n = timer_ns_gettime64(); return n - n / 1000000000ull * 2502400ull; }
static inline uint64_t cyc(void) { return perf_cntr_count(PRFC1); }
static unsigned us_of(uint64_t t) { return (unsigned)((t + 500u) / 1000u); }
static unsigned mbs10(uint64_t bytes, uint64_t t) { return t ? (unsigned)(bytes * 10000u / t) : 0u; }   /* 10 x (million bytes a second) */

/* Wait on the timer alone: no bus access but the SH-4's own registers. */
static void wait_ns(uint64_t t) {
    const uint64_t t0 = ns();

    while (ns() - t0 < t) { }
}

/* ---- memory ------------------------------------------------------------------------ */

#define MAP_ROWS (C4_MAP + 8u)             /* rows allocated for the map target; C4_MAP + 2 are drawn */
#define MASK_TW 512u                       /* the mask target as a texture */
#define MASK_TH 256u
#define BG565 0x001fu                      /* pure blue: what a pass leaves where it drew nothing */
#define BV_MAX 8192u                       /* band vertices at most */
#define RECT_MAX 1600u

static pvr_ptr_t ramp_mem[2], map_mem, mask_mem, a4_mem;
static uint16_t tex16[1024u * 8u] __attribute__((aligned(32)));
static uint16_t map_ram[C4_MAP * C4_MAP] __attribute__((aligned(32)));
static uint16_t mask_ram[MASK_TW * C4_MASK_H] __attribute__((aligned(32)));
static uint8_t map_px[C4_MAP * C4_MAP], model_map[C4_MAP * C4_MAP], model_ids[C4_MAP * C4_MAP];
static uint8_t m_face[C4_MASK_W * C4_MASK_H], m_info[C4_MASK_W * C4_MASK_H];

/* The band vertices: one pool, so their distances are known (c4b). The operand cache is 16 KB, one way: two arrays a
   multiple of 16 KB apart share every cache entry, and c4's three static arrays of 256 KB did (SH4-046: the projection
   took 132 cycles a vertex there, 21 in bench h2). Here bscr starts half a cache past the end of bsrc and bref a
   quarter past the end of bscr, so no two of them are a multiple of 16 KB apart. */
#define OC 16384u
#define BV_BYTES (BV_MAX * 32u)
#define BSCR_OFF (BV_BYTES + OC / 2u)
#define BREF_OFF (2u * BV_BYTES + OC + OC / 4u)
static uint8_t vpool[2u * BV_BYTES + OC + OC / 4u + BV_BYTES] __attribute__((aligned(32)));
#define bsrc ((pvr_vertex_t *)(void *)vpool)                       /* the band vertices, positions in the world */
#define bscr ((pvr_vertex_t *)(void *)(vpool + BSCR_OFF))          /* the same on the mask's screen */
#define bref ((pvr_vertex_t *)(void *)(vpool + BREF_OFF))          /* the C form's, for the compare; the route test's copy */
_Static_assert(BSCR_OFF % OC != 0u && BREF_OFF % OC != 0u && (BREF_OFF - BSCR_OFF) % OC != 0u, "two band arrays a multiple of the cache apart");
static pvr_vertex_t rectv[RECT_MAX] __attribute__((aligned(32))); /* screen-space quads of the tests */
static pvr_vertex_t mapv[(C4_FACES + 2u) * 4u] __attribute__((aligned(32)));
static pvr_vertex_t facesrc[C4_FACES * 4u] __attribute__((aligned(32))), facev[C4_FACES * 4u] __attribute__((aligned(32)));
static pvr_vertex_t overv[16] __attribute__((aligned(32)));
static shz_mat4x4_t cam_mat[2] __attribute__((aligned(32)));      /* [0] the mask's 320 x 240, [1] the screen's 640 x 480 */
static c4_cam_t cam[2];
static unsigned nbv, nbands, band_over, nmapv, ramp;

/* c4b, fix 4: the window measured at boot (test 1, window 32). e_ok: measured; edges_on: Q made from it. */
static uint16_t e_sh[256], e_n[256];
static uint8_t e_tab[256];
static int e_lo, e_hi, e_ok, edges_on;

/* The settings for a pass: c4_set_init, then the measured window when it is on and the window is the one measured. */
static void set_init(c4_set_t *st, unsigned w, unsigned gs, float b0, float grow) {
    c4_set_init(st, w, gs, b0, grow);
    if (edges_on && e_ok && st->w == 32u) c4_set_edges(st, e_lo, e_hi, e_tab);
}

/* The code the ramp holds where 16384 x the texture coordinate u is d: 512 - 256 / u, rounded down, 0 .. 255. u is
   1 / (2 z) and z = 1 - (code + 0.5) / 512, so that is the pixel's own code rounded to the nearest. */
static unsigned ramp_code(uint32_t d) {
    const int c = 512 - (int)((4194304u + d - 1u) / d);

    return c < 0 ? 0u : c > 255 ? 255u : (unsigned)c;
}

static int load(void) {
    unsigned tu, tv, k;

    ramp_mem[0] = pvr_mem_malloc(sizeof tex16);
    ramp_mem[1] = pvr_mem_malloc(sizeof tex16);
    map_mem = pvr_mem_malloc(C4_MAP * MAP_ROWS * 2u);
    mask_mem = pvr_mem_malloc(MASK_TW * MASK_TH * 2u);
    a4_mem = pvr_mem_malloc(256u * 8u * 2u);
    if (!ramp_mem[0] || !ramp_mem[1] || !map_mem || !mask_mem || !a4_mem) return 0;
    pvr_sq_set16(mask_mem, 0u, MASK_TW * MASK_TH * 2u, PVR_DMA_VRAM64);   /* the texels past the 320 x 242 that are drawn: alpha 0, for the filter at the edge */
    for (k = 0; k < 2u; k++) {             /* [0] one code a texel's middle; [1] v splits a texel of u into 8 steps */
        for (tv = 0; tv < 8u; tv++) for (tu = 0; tu < 1024u; tu++) tex16[tv * 1024u + tu] = (uint16_t)ramp_code(k ? 16u * tu + 2u * tv + 1u : 16u * tu + 8u);
        pvr_txr_load(tex16, ramp_mem[k], sizeof tex16);
    }
    for (tv = 0; tv < 8u; tv++) for (tu = 0; tu < 256u; tu++) tex16[tv * 256u + tu] = (uint16_t)(((tu >> 4) << 12) | 0x0fffu);   /* alpha x / 16, white */
    pvr_txr_load(tex16, a4_mem, 256u * 8u * 2u);
    for (k = 0; k < 2u; k++) {
        c4_camera(&cam[k], k ? 640u : C4_MASK_W, k ? 480u : C4_MASK_H);
        c4_cam_matrix(&cam[k], cam_mat[k].elem);
    }
    return 1;
}

/* ---- headers ----------------------------------------------------------------------- */

static pvr_poly_hdr_t h_map[2], h_flat, h_clear, h_bump, h_bump_pt, h_over, h_show[2];

static void headers(void) {
    pvr_poly_cxt_t c;
    unsigned k;

    for (k = 0; k < 2u; k++) {             /* the map pass: the ramp, the nearest to the light wins */
        pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, 1024, 8, ramp_mem[k], PVR_FILTER_NEAREST);
        c.gen.culling = PVR_CULLING_NONE; c.txr.env = PVR_TXRENV_REPLACE; c.txr.uv_clamp = PVR_UVCLAMP_U;
        pvr_poly_compile(&h_map[k], &c);
    }
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                      /* the depth pass of the mask, and the scene on screen */
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&h_flat, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                      /* the mask's clear: colour and alpha replaced, depth left alone */
    c.gen.culling = PVR_CULLING_NONE;
    c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ZERO;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_clear, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_BUMP | PVR_TXRFMT_NONTWIDDLED, (int)C4_MAP, (int)C4_MAP, map_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE; c.gen.specular = 1;        /* the offset colour carries K1, K2, K3 and Q */
    c.txr.env = PVR_TXRENV_REPLACE; c.txr.alpha = PVR_TXRALPHA_ENABLE; c.txr.uv_clamp = PVR_UVCLAMP_UV;
    c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ZERO;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL; c.depth.write = PVR_DEPTHWRITE_DISABLE;   /* only the surface the depth pass kept */
    pvr_poly_compile(&h_bump, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_PT_POLY, PVR_TXRFMT_BUMP | PVR_TXRFMT_NONTWIDDLED, (int)C4_MAP, (int)C4_MAP, map_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE; c.gen.specular = 1;        /* c4b route 2: test 6's header, which drew by the level */
    c.txr.env = PVR_TXRENV_REPLACE; c.txr.uv_clamp = PVR_UVCLAMP_UV;
    pvr_poly_compile(&h_bump_pt, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, (int)MASK_TW, (int)MASK_TH, mask_mem, PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE;                            /* the shadow layer: black, alpha = the mask's bit x the vertex's */
    c.txr.env = PVR_TXRENV_MODULATEALPHA; c.txr.alpha = PVR_TXRALPHA_ENABLE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_over, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)C4_MAP, (int)C4_MAP, map_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE; c.txr.env = PVR_TXRENV_REPLACE;
    pvr_poly_compile(&h_show[0], &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, (int)MASK_TW, (int)MASK_TH, mask_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE; c.txr.env = PVR_TXRENV_REPLACE;
    pvr_poly_compile(&h_show[1], &c);
}

/* A rectangle as four vertices of a strip. Returns the next free vertex. */
static pvr_vertex_t *rect(pvr_vertex_t *v, float x0, float y0, float x1, float y1, float z, float u0, float v0, float u1, float v1, uint32_t argb, uint32_t oargb) {
    unsigned i;

    for (i = 0; i < 4u; i++, v++) {
        v->flags = i == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v->x = (i & 2u) ? x1 : x0; v->y = (i & 1u) ? y0 : y1; v->z = z;
        v->u = (i & 2u) ? u1 : u0; v->v = (i & 1u) ? v0 : v1;
        v->argb = argb; v->oargb = oargb;
    }
    return v;
}

/* A header and a block of ready vertices: one store-queue run into the tile accelerator. */
static void ta_send(const pvr_poly_hdr_t *h, const pvr_vertex_t *v, unsigned n) {
    if (!n) return;
    pvr_prim((void *)h, sizeof *h);
    pvr_sq_load(NULL, v, n * sizeof *v, PVR_DMA_TA);
}

/* ---- scenes ------------------------------------------------------------------------ */

enum { TO_SCREEN = 0, TO_MAP = 1, TO_MASK = 2 };

static uint64_t last_render_ns;
static uint32_t render_addr;
static int poll_ready;                     /* c4b test 8: 1 = the "ready" waits poll pvr_check_ready instead of sleeping */
static uint64_t w_open_ns, w_close_ns, w_done_ns;   /* the last pass's three waits, apart */

/* c4c test 8b: the render-done interrupt seen by this program. KallistiOS's handler for the event stays first (ours
   calls it, then notes the time); nothing of KallistiOS is changed. poll_done: 1 = scene_close waits for rd_seen on the
   timer and never sleeps in pvr_wait_render_done. w_late_ns: how long after the interrupt the wait came back.
   w_done_vbl: vertical blanks that passed during the wait. */
static asic_evt_handler_entry_t rd_prev;
static volatile uint64_t rd_ns;
static volatile uint32_t rd_seen;
static int rd_hooked, poll_done;
static uint64_t w_late_ns;
static uint32_t w_done_vbl;
static volatile uint32_t vbl_count;        /* vertical blanks, counted by this program's own handler */

static void rd_evt(uint32_t code, void *data) {
    (void)data;
    if (rd_prev.hdl) rd_prev.hdl(code, rd_prev.data);
    rd_ns = ns();
    rd_seen = 1u;
}

/* Wait until the PVR takes a new scene: KallistiOS's sleeping wait (100 ms a try, 20 tries), or a poll on the timer. */
static int wait_ready(void) {
    unsigned t;

    if (poll_ready) {
        const uint64_t t0 = ns();

        while (pvr_check_ready() < 0) if (ns() - t0 > LIMIT_NS) return 0;
        return 1;
    }
    for (t = 0; t < 20u && pvr_wait_ready() < 0; t++) sh4der_fault_beat(&crumbs);
    return t < 20u;
}

static int scene_open(int to) {
    uint64_t t0 = ns();
    int ok = wait_ready();

    w_open_ns = ns() - t0;
    if (!ok) return 0;
    if (to == TO_MAP) return pvr_scene_begin_rtt(map_mem, C4_MAP, C4_MAP + 2u, C4_MAP) >= 0;
    if (to == TO_MASK) return pvr_scene_begin_rtt(mask_mem, C4_MASK_W, C4_MASK_H + 2u, MASK_TW) >= 0;
    pvr_scene_begin();
    return 1;
}

static int scene_close(void) {
    pvr_stats_t st;
    unsigned t;

    uint64_t t0;

    uint32_t v0;

    rd_seen = 0u;                          /* the render starts inside pvr_scene_finish's wait at the earliest */
    pvr_scene_finish();
    t0 = ns();
    t = (unsigned)wait_ready();
    w_close_ns = ns() - t0;
    if (!t) return 0;
    t0 = ns(); v0 = vbl_count;
    if (poll_done && rd_hooked) {
        while (!rd_seen) if (ns() - t0 > LIMIT_NS) return 0;
        t = 0;
    } else {
        for (t = 0; t < 20u && pvr_wait_render_done() < 0; t++) sh4der_fault_beat(&crumbs);
    }
    w_done_ns = ns() - t0; w_done_vbl = vbl_count - v0;
    w_late_ns = rd_hooked && rd_seen ? ns() - rd_ns : 0u;
    if (t == 20u || pvr_get_stats(&st) < 0) return 0;
    last_render_ns = st.rnd_last_time;
    render_addr = PVR_GET(PVR_RENDER_ADDR);
    return 1;
}

/* ---- video memory to RAM: PVR-DMA ---------------------------------------------------- */

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
#define EVT_PVR_DMA_END 0x000bu            /* the same bit as an event code: register 0, bit 11 */

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

static int fetch_checked, fetch_direct, fetch_off;   /* fetch_off: the live view draws only, nothing is read back */
static unsigned fetch_slow;

/* `bytes` (a multiple of 32) of video memory at `mem` into RAM at dst (on a multiple of 32). By PVR-DMA, its end
   looked for four times a millisecond; the first block is also checked against words read straight from video
   memory, and if either fails the words are read one by one from then on (and the pages say so). */
static void fetch(pvr_ptr_t mem, uint16_t *dst, uint32_t bytes) {
    const volatile uint16_t *src = (const volatile uint16_t *)mem;
    uint32_t i;

    if (!fetch_direct) {
        const uint64_t t0 = ns();
        int ended;

        pd_start(mem, dst, bytes);
        do { wait_ns(250000u); ended = !pd_busy(); } while (!ended && ns() - t0 < LIMIT_NS);
        pd_end();
        if (ended && !fetch_checked) {
            fetch_checked = 1;
            for (i = 0; i < 256u; i++) { const uint32_t k = (i * 2654435761u) % (bytes / 2u); if (dst[k] != src[k]) ended = 0; }
            say("fetch: PVR-DMA of %u bytes %s words read from video memory", (unsigned)bytes, ended ? "matches 256" : "DIFFERS from");
        }
        if (ended) return;
        fetch_direct = 1;
        say("fetch: PVR-DMA not used from here; words read one by one");
    }
    fetch_slow++;
    for (i = 0; i < bytes / 2u; i++) dst[i] = src[i];
}

/* ---- the map pass ---------------------------------------------------------------------- */

static void map_begin(void) {
    /* behind everything: the farthest code, 255 (the last texel of either ramp) */
    rect(mapv, 0.0f, 0.0f, (float)C4_MAP, (float)C4_MAP + 2.0f, 0.25f, 0.9995f, 0.5f, 0.9995f, 0.5f, 0xffffffffu, 0u);
    nmapv = 4u;
}

/* One quad of the map: corners as map pixel x, y and the depth code. The depth the PVR tests is z = 1 - (code + 0.5) /
   512 (nearer the light = larger), and the texture coordinate is 1 / (2 z): the PVR divides the interpolated u z by
   the interpolated z, so with u z constant every pixel gets 1 / (2 z) of its OWN z, which the ramp turns back into
   the code. */
static void map_quad(float q[4][3]) {
    static const uint8_t order[4] = { 0, 1, 3, 2 };
    pvr_vertex_t *v = mapv + nmapv;
    unsigned k;

    for (k = 0; k < 4u; k++, v++) {
        const float *c = q[order[k]];
        const float z = 1.0f - (c[2] + 0.5f) * (1.0f / 512.0f), iz = sh4der_recip_pos_(z);

        v->flags = k == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v->x = c[0]; v->y = c[1]; v->z = z;
        v->u = 0.5f * iz; v->v = ramp ? 512.0f * iz : 0.5f;
        v->argb = 0xffffffffu; v->oargb = 0u;
    }
    nmapv += 4u;
}

static unsigned map_notcode;
static uint64_t map_send_ns;

/* Render the quads into the map target and bring it to RAM: map_px the codes. 0 when the pass did not come back. */
static int map_end(void) {
    uint64_t t0;
    unsigned i;

    if (!scene_open(TO_MAP)) return 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    t0 = ns();
    ta_send(&h_map[ramp], mapv, nmapv);
    map_send_ns = ns() - t0;
    pvr_list_finish();
    if (!scene_close()) return 0;
    if (fetch_off) return 1;
    fetch(map_mem, map_ram, sizeof map_ram);
    for (map_notcode = 0, i = 0; i < C4_MAP * C4_MAP; i++) { map_px[i] = (uint8_t)map_ram[i]; map_notcode += (map_ram[i] >> 8) != 0u; }
    return 1;
}

/* The scene's map: every face toward the sun, grown. Faces [first, last). */
static int map_scene(const c4_light_t *L, const c4_set_t *st, unsigned first, unsigned last) {
    c4_face_t f[C4_FACES];
    float q[4][3];
    unsigned i;

    c4_faces(f, C4_SKIRT);
    map_begin();
    for (i = first; i < last; i++) if (c4_map_quad(L, &f[i], st, q)) map_quad(q);
    return map_end();
}

/* A map whose pixel (x, y) holds 2 y on the left half and 2 y + 1 on the right: every code once, each at the middle
   of its texel of the ramp. */
static int map_synthetic(void) {
    static float left[4][3] = { { 0, 0, -1 }, { 64, 0, -1 }, { 64, 128, 255 }, { 0, 128, 255 } };
    static float right[4][3] = { { 64, 0, 0 }, { 128, 0, 0 }, { 128, 128, 256 }, { 64, 128, 256 } };

    map_begin();
    map_quad(left); map_quad(right);
    return map_end();
}

/* ---- the bands --------------------------------------------------------------------------- */

#define KQ(q) ((C4_K1 << 24) | (C4_K3 << 8) | ((q) & 255u))      /* K1, K2 = 0, K3, Q */
#define K_SHADOW (255u << 24)                                    /* K1 = 255, K3 = 0: the level is 255 whatever the map holds */
#define K_LIT 0u                                                 /* K1 = 0, K3 = 0: the level is 0 whatever the map holds */

/* A convex polygon as one strip: corners 0, 1, n - 1, 2, n - 2 .. */
static void band_strip(const c4_corner_t *c, unsigned n, uint32_t oargb) {
    pvr_vertex_t *v = bsrc + nbv;
    unsigned k;

    if (nbv + n > BV_MAX) { band_over = 1; return; }
    for (k = 0; k < n; k++, v++) {
        const c4_corner_t *p = &c[(k & 1u) ? (k + 1u) / 2u : k ? n - k / 2u : 0u];

        v->flags = k + 1u == n ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v->x = p->p[0]; v->y = p->p[1]; v->z = p->p[2];
        v->u = p->m[0] * (1.0f / (float)C4_MAP); v->v = p->m[1] * (1.0f / (float)C4_MAP);
        v->argb = 0xffffffffu; v->oargb = oargb;
    }
    nbv += n; nbands++;
}

/* Cut faces [first, last) into bands for this light and these settings: bsrc, positions in the world. */
static void bands_build(const c4_light_t *L, const c4_set_t *st, unsigned first, unsigned last) {
    c4_face_t f[C4_FACES];
    unsigned i, k;

    c4_faces(f, 0.0f);
    nbv = nbands = band_over = 0;
    for (i = first; i < last; i++) {
        c4_corner_t face[4], out[C4_BAND_MAX];
        float inv[4], lo, hi, bias;
        int b, b1;

        const int kind = c4_face_kind(L, f[i].n, st);

        c4_face_corners(L, &f[i], face, inv);
        if (kind != C4_FACE_COMPARE) {     /* no compare: one polygon, shadow or lit whatever the map holds */
            band_strip(face, 4u, kind == C4_FACE_AWAY || st->steep_shadow ? K_SHADOW : K_LIT);
            continue;
        }
        lo = hi = face[0].m[2];
        for (k = 1; k < 4u; k++) { if (face[k].m[2] < lo) lo = face[k].m[2]; if (face[k].m[2] > hi) hi = face[k].m[2]; }
        bias = c4_bias(L, f[i].n, st->b0, st->gs);
        for (b = c4_band(lo, st->gs), b1 = c4_band(hi, st->gs); b <= b1; b++) {
            const unsigned n = c4_band_poly(face, inv, b, st->gs, out);

            if (n) band_strip(out, n, KQ(c4_q_set(st, c4_band_code(b, st->gs), bias)));
        }
    }
}

/* The band vertices onto the mask's screen: bscr. */
static void bands_project(void) {
    shz_xmtrx_load_4x4(&cam_mat[0]);
    shz_dce_pvr_project(bscr, bsrc, nbv);
}

/* ---- the mask pass ------------------------------------------------------------------------ */

#define CFG2_MASK 0x00ff0007u              /* alpha threshold (bits 23-16) and pack mode (bits 2-0) */
#define PACK_1555 3u

static uint64_t mask_send_ns;

#ifndef C4B_MASK_PT
#define C4B_MASK_PT 0                      /* 1: the live view makes its mask by punch-through (test 9's route) */
#endif
static int mask_route = C4B_MASK_PT;       /* 0: the 1555 write threshold; 1: punch-through with reference thr */
#define GREEN565 0x07e0u                   /* route 1: what the depth pass paints, so a punch-through pixel shows */

/* The mask from n ready vertices (screen positions, map coordinates, Q in the offset colour): an opaque pass for
   depth, a clear, the same polygons through the bump unit; the target written as ARGB1555 with threshold thr. The
   write control register is set after the lists are sent and put back after the render (KallistiOS sets it only when
   the video mode is set). mask_ram gets the picture. 0 when the pass did not come back. */
static int mask_pass_pt(const pvr_vertex_t *v, unsigned n, unsigned thr);

static int mask_pass(const pvr_vertex_t *v, unsigned n, unsigned thr) {
    const uint32_t saved = PVR_GET(PVR_FB_CFG_2);
    uint64_t t0;
    int ok;

    if (mask_route) return mask_pass_pt(v, n, thr);

    rect(overv + 8, 0.0f, 0.0f, (float)C4_MASK_W, (float)C4_MASK_H + 2.0f, 0.001f, 0.0f, 0.0f, 0.0f, 0.0f, 0x000000ffu, 0u);   /* blue, alpha 0 */
    if (!scene_open(TO_MASK)) return 0;
    t0 = ns();
    pvr_list_begin(PVR_LIST_OP_POLY);
    ta_send(&h_flat, v, n);
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    ta_send(&h_clear, overv + 8, 4u);
    ta_send(&h_bump, v, n);
    pvr_list_finish();
    mask_send_ns = ns() - t0;
    PVR_SET(PVR_FB_CFG_2, (saved & ~CFG2_MASK) | (thr << 16) | PACK_1555);
    ok = scene_close();
    PVR_SET(PVR_FB_CFG_2, saved);
    if (ok && !fetch_off) fetch(mask_mem, mask_ram, sizeof mask_ram);
    return ok;
}

/* Route 1 (c4b): the depth pass in green (a copy of the vertices in bref, colour changed), then the same polygons
   through the bump unit in the punch-through list, reference thr: a pixel whose level passes is drawn, and its colour
   is not green. The target is written as the display's 565, no threshold. The copy is made outside the timing. */
static int mask_pass_pt(const pvr_vertex_t *v, unsigned n, unsigned thr) {
    uint64_t t0;
    unsigned i;

    if (v != bref) { memcpy(bref, v, n * sizeof *v); for (i = 0; i < n; i++) bref[i].argb = 0xff00ff00u; }
    if (!scene_open(TO_MASK)) return 0;
    t0 = ns();
    pvr_list_begin(PVR_LIST_OP_POLY);
    ta_send(&h_flat, bref, n);
    pvr_list_finish();
    PVR_SET(PVR_PT_ALPHA_REF, thr);
    pvr_list_begin(PVR_LIST_PT_POLY);
    ta_send(&h_bump_pt, v, n);
    pvr_list_finish();
    mask_send_ns = ns() - t0;
    PVR_SET(PVR_PT_ALPHA_REF, thr);
    if (!scene_close()) return 0;
    if (!fetch_off) fetch(mask_mem, mask_ram, sizeof mask_ram);
    return 1;
}

#define MASK_AT(x, y) mask_ram[(y) * MASK_TW + (x)]
#define MASK_DRAWN(p) (mask_route ? (p) != BG565 : ((p) & 0x7fffu) != BG565)
#define MASK_SHADOW(p) (mask_route ? (p) != GREEN565 : ((p) >> 15) != 0u)

/* ---- the screen pass ----------------------------------------------------------------------- */

static unsigned nfacev;

static uint32_t pack_rgb(float r, float g, float b) {
    return 0xff000000u | ((uint32_t)(sh4der_sat_(r) * 255.0f) << 16) | ((uint32_t)(sh4der_sat_(g) * 255.0f) << 8) | (uint32_t)(sh4der_sat_(b) * 255.0f);
}

/* The scene's faces as flat-lit quads, positions in the world. */
static void faces_build(const c4_light_t *L) {
    static const uint8_t order[4] = { 0, 1, 3, 2 };
    c4_face_t f[C4_FACES];
    pvr_vertex_t *v = facesrc;
    unsigned i, k;

    c4_faces(f, 0.0f);
    for (i = 0; i < C4_FACES; i++) {
        const float d = c4_dot(f[i].n, L->s), lit = 0.35f + 0.65f * (d > 0.0f ? d : 0.0f);
        const uint32_t col = i ? pack_rgb(0.80f * lit, 0.74f * lit, 0.62f * lit) : pack_rgb(0.46f * lit, 0.58f * lit, 0.36f * lit);

        for (k = 0; k < 4u; k++, v++) {
            const float *p = f[i].p[order[k]];

            v->flags = k == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            v->x = p[0]; v->y = p[1]; v->z = p[2]; v->u = v->v = 0.0f; v->argb = col; v->oargb = 0u;
        }
    }
    nfacev = C4_FACES * 4u;
}

/* The scene on the screen, the shadow layer over it (strength 0: none), the map and the mask in two corners. */
static int screen_pass(unsigned strength, int previews) {
    shz_xmtrx_load_4x4(&cam_mat[1]);
    shz_dce_pvr_project(facev, facesrc, nfacev);
    rect(overv, 0.0f, 0.0f, 640.0f, 480.0f, 50.0f, 0.0f, 0.0f, (float)C4_MASK_W / (float)MASK_TW, (float)C4_MASK_H / (float)MASK_TH, strength << 24, 0u);
    rect(overv + 4, 8.0f, 8.0f, 136.0f, 136.0f, 100.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0xffffffffu, 0u);
    rect(overv + 12, 472.0f, 8.0f, 632.0f, 128.0f, 100.0f, 0.0f, 0.0f, (float)C4_MASK_W / (float)MASK_TW, (float)C4_MASK_H / (float)MASK_TH, 0xffffffffu, 0u);
    if (!scene_open(TO_SCREEN)) return 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    ta_send(&h_flat, facev, nfacev);
    if (previews) { ta_send(&h_show[0], overv + 4, 4u); ta_send(&h_show[1], overv + 12, 4u); }
    pvr_list_finish();
    if (strength) {
        pvr_list_begin(PVR_LIST_TR_POLY);
        ta_send(&h_over, overv, 4u);
        pvr_list_finish();
    }
    return scene_close();
}

/* ---- 0. the ramp ---------------------------------------------------------------------------- */

/* A map whose code runs finely: 2 x + y / 64 at the pixel, so every pixel's code has its own fraction. Returns the
   pixels that do not hold the nearest code; *worst the largest difference. */
static int ramp_wrong(unsigned *worst) {
    static float q[4][3] = { { 0, 0, 0 }, { 128, 0, 256 }, { 128, 128, 258 }, { 0, 128, 2 } };   /* code = 2 x + y / 64 at the corners */
    unsigned x, y, wrong = 0;

    map_begin();
    map_quad(q);
    if (!map_end()) return -1;
    for (*worst = 0, y = 0; y < C4_MAP; y++) {
        for (x = 0; x < C4_MAP; x++) {
            const int want0 = c4_ifloor(2.0f * ((float)x + 0.5f) + ((float)y + 0.5f) * (1.0f / 64.0f) + 0.5f), want = want0 > 255 ? 255 : want0;
            const int d = (int)map_px[y * C4_MAP + x] - want, a = d < 0 ? -d : d;

            if (a) wrong++;
            if ((unsigned)a > *worst) *worst = (unsigned)a;
        }
    }
    return (int)wrong;
}

static void t_ramp(void) {
    unsigned worst[2] = { 0, 0 }, x, y, bad = 0;
    int wrong[2];

    say("0 RAMP code from the pixel's own depth, px wrong of 16384");
    for (ramp = 0; ramp < 2u; ramp++) {
        step(ramp ? "0 ramp sub" : "0 ramp");
        wrong[ramp] = ramp_wrong(&worst[ramp]);
        if (wrong[ramp] < 0) say("0 %s: NO RENDER END", ramp ? "8 sub-steps" : "plain");
        else say("0 %s: %d wrong, worst %u codes, %u px not a code", ramp ? "8 sub-steps through v" : "plain 1024 texels", wrong[ramp], worst[ramp], map_notcode);
    }
    ramp = wrong[1] >= 0 && (wrong[0] < 0 || wrong[1] < wrong[0]) ? 1u : 0u;
    say("0 ramp used from here: %s", ramp ? "8 sub-steps" : "plain");
    step("0 whole codes");
    if (!map_synthetic()) { say("0 whole codes: NO RENDER END"); return; }
    for (y = 0; y < C4_MAP; y++) for (x = 0; x < C4_MAP; x++) bad += map_px[y * C4_MAP + x] != 2u * y + (x >= 64u);
    say("0 whole codes (2y, 2y+1): %u of 16384 wrong", bad);
}

/* ---- 1. the window --------------------------------------------------------------------------- */

#define STRIPS 160u
#define STRIP_Q(i) ((13u * (i) + 5u) & 255u)

/* 160 strips one pixel wide, each with its own Q, twice: over the map's left half (even codes) and its right (odd).
   Row y reads map row y. */
static unsigned window_quads(void) {
    pvr_vertex_t *v = rectv;
    unsigned i, half;

    for (half = 0; half < 2u; half++) {
        for (i = 0; i < STRIPS; i++) {
            const float x = (float)(half * STRIPS + i), u = half ? 0.75f : 0.25f;

            v = rect(v, x, 0.0f, x + 1.0f, (float)C4_MAP, 1.0f, u, 0.0f, u, 1.0f, 0xffffffffu, KQ(STRIP_Q(i)));
        }
    }
    return (unsigned)(v - rectv);
}

/* c4b fix 4: the window's edges from test 1's picture at window 32, and from here on Q made from them. */
static void t_edges(void) {
    unsigned d, below = 0, above = 0, seen = 0;
    int mixed, lo_n, hi_n;

    for (lo_n = 128; lo_n > 0 && c4_level[lo_n - 1] < c4_threshold[32]; lo_n--) { }
    for (hi_n = 128; hi_n < 255 && c4_level[hi_n + 1] < c4_threshold[32]; hi_n++) { }
    for (d = 0; d < 256u; d++) seen += e_n[d] != 0u;
    mixed = c4_edges(e_sh, e_n, &e_lo, &e_hi, e_tab);
    for (d = 0; d < 256u; d++) { if (e_tab[d] != (c4_level[d] >= c4_threshold[32])) { if (d < 128u) below++; else above++; } }
    if (mixed < 0) { say("1 EDGES w 32: difference 128 read shadow; NOT measured, Q stays the cosine's"); return; }
    e_ok = 1; edges_on = 1;
    say("1 EDGES w 32 thr %u: lit from %d to %d (cosine %d to %d); %d mixed; %u of 256 seen", c4_threshold[32], e_lo, e_hi, lo_n, hi_n, mixed, seen);
    say("1 EDGES differences whose answer is not the cosine's: %u under 128, %u over", below, above);
    say("1 EDGES from here on Q is made from the edge %d (c4b fix 4), unless a line says cosine", e_lo);
}

static void t_window(void) {
    static const unsigned ws[3] = { 16u, 32u, 64u };
    const unsigned n = window_quads();
    unsigned k, x, y;

    say("1 WINDOW each code against 160 Q, px of 40960");
    step("1 map");
    if (!map_synthetic()) { say("1 map: NO RENDER END"); return; }
    for (k = 0; k < 3u; k++) {
        const unsigned thr = c4_threshold[ws[k]];
        unsigned differ = 0, close = 0, holes = 0;

        step("1 window");
        if (!mask_pass(rectv, n, thr)) { say("1 w %u: NO RENDER END", ws[k]); continue; }
        for (y = 0; y < C4_MAP; y++) {
            for (x = 0; x < 2u * STRIPS; x++) {
                const unsigned p = MASK_AT(x, y), m = map_px[y * C4_MAP + (x >= STRIPS ? 96u : 32u)], d = (m - STRIP_Q(x % STRIPS)) & 255u, level = c4_level[d];
                const int gap = (int)level - (int)thr;

                if (!MASK_DRAWN(p)) { holes++; continue; }
                if (ws[k] == 32u) { e_n[d]++; e_sh[d] += MASK_SHADOW(p) != 0u; }
                if (MASK_SHADOW(p) != (level >= thr)) { differ++; close += gap >= -2 && gap <= 2; }
            }
        }
        say("1 w %u thr %u: differ %u, of them within 2 levels of thr %u, holes %u", ws[k], thr, differ, close, holes);
    }
    t_edges();
}

/* ---- 3. the bias -------------------------------------------------------------------------------- */

static void t_bias_one(void);

static void t_bias(void) {
    const int had = edges_on;
    unsigned k;

    for (k = 0; k < (e_ok ? 2u : 1u); k++) { edges_on = (int)k; t_bias_one(); }
    edges_on = had;
}

static void t_bias_one(void) {
    static const float b0s[7] = { 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f };
    unsigned sh[7] = { 0, 0, 0, 0, 0, 0, 0 }, drawn = 0, k, x, y, gs;
    c4_light_t L;
    c4_set_t st;

    pg_plot_t *g = pg_plot(edges_on ? "3 BIAS ground alone, MEASURED edge: acne px against b0" : "3 BIAS ground alone, cosine's edge: acne px against b0", "b0, codes", "shadow px (all acne)", 0);

    say("3 BIAS ground alone: every shadow px is acne; Q from the %s edge", edges_on ? "MEASURED" : "cosine's");
    c4_light_fit(&L, 150.0f, 26.5f);
    step("3 map");
    c4_set_init(&st, 32u, 0u, C4_B0, 0.75f);
    if (!map_scene(&L, &st, 0u, 1u)) { say("3 map: NO RENDER END"); return; }
    for (gs = 0; gs < 3u; gs++) {
        for (k = 0; k < 7u; k++) {
            step("3 bias");
            set_init(&st, 32u, gs, b0s[k], 0.75f);
            bands_build(&L, &st, 0u, 1u);
            bands_project();
            if (!mask_pass(bscr, nbv, st.thr)) { say("3 b0 x10 %u: NO RENDER END", (unsigned)(b0s[k] * 10.0f)); return; }
            for (sh[k] = drawn = 0, y = 0; y < C4_MASK_H; y++) for (x = 0; x < C4_MASK_W; x++) { const unsigned p = MASK_AT(x, y); if (MASK_DRAWN(p)) { drawn++; sh[k] += MASK_SHADOW(p); } }
        }
        say("3 bands %u wide, b0 0 .5 1 1.5 2 2.5 3: %u %u %u %u %u %u %u shadow of %u px", 1u << gs, sh[0], sh[1], sh[2], sh[3], sh[4], sh[5], sh[6], drawn);
        pg_series(g, gs == 0u ? "bands 1 wide" : gs == 1u ? "bands 2 wide" : "bands 4 wide");
        for (k = 0; k < 7u; k++) pg_point(g, b0s[k], (float)sh[k]);
    }
}

/* ---- 4. the scene --------------------------------------------------------------------------------- */

enum { I_ANS = 1, I_TRUTH = 2, I_OWN = 4, I_BEHIND = 8, I_INBIAS = 16 };

static unsigned sc_acne, sc_edge, sc_literr, sc_ok;   /* the last scene's counts, for page S's graphs */

static void t_scene_at(float az, float el, unsigned w, unsigned gs, float b0, int brief) {
    static const float up[3] = { 0.0f, 1.0f, 0.0f };
    unsigned eq = 0, one = 0, more = 0, x, y, i;
    unsigned holes_in = 0, holes_out = 0, dif_in = 0, dif_out = 0, lit = 0, shadow = 0, acne = 0, edge = 0, rim = 0, inbias = 0, other = 0;
    c4_light_t L;
    c4_set_t st;
    float ground_bias;

    sc_ok = 0;
    c4_light_fit(&L, az, el);
    set_init(&st, w, gs, b0, 0.75f);
    ground_bias = c4_bias(&L, up, st.b0, st.gs);
    if (!brief) say("4 SCENE sun %u/%u w %u bands %u wide grow .75 b0 x10 %u, Q from the %s edge", (unsigned)az, (unsigned)el, w, 1u << gs, (unsigned)(st.b0 * 10.0f), st.dtab ? "MEASURED" : "cosine's");
    step("4 map");
    if (!map_scene(&L, &st, 0u, C4_FACES)) { say("4 map: NO RENDER END"); return; }
    c4_model_map(&L, &st, model_map, model_ids);
    for (i = 0; i < C4_MAP * C4_MAP; i++) {
        const int d = (int)map_px[i] - (int)model_map[i];

        if (!d) eq++; else if (d == 1 || d == -1) one++; else more++;
    }
    if (!brief) say("4 map vs model: same %u, 1 code off %u, more %u, not a code %u", eq, one, more, map_notcode);
    step("4 bands");
    bands_build(&L, &st, 0u, C4_FACES);
    bands_project();
    if (band_over) { say("4 bands: over %u vertices", BV_MAX); return; }
    step("4 mask");
    if (!mask_pass(bscr, nbv, st.thr)) { say("4 mask: NO RENDER END"); return; }
    for (y = 0; y < C4_MASK_H; y++) {      /* the model, on the console's own map */
        sh4der_fault_beat(&crumbs);
        for (x = 0; x < C4_MASK_W; x++) {
            c4_pixel_t px;
            const int m = c4_model_pixel(&L, &cam[0], map_px, &st, x, y, &px);
            unsigned info = 0;

            i = y * C4_MASK_W + x;
            if (m == C4_SKY) { m_face[i] = 255u; m_info[i] = 0u; continue; }
            m_face[i] = (uint8_t)px.face;
            if (m == C4_SHADOW) info |= I_ANS;
            if (px.truth == C4_SHADOW) info |= I_TRUTH;
            if (model_ids[px.my * C4_MAP + px.mx] == px.face) info |= I_OWN;
            if (px.sep <= 0.0f) info |= I_BEHIND;
            if (px.sep < ground_bias + 2.0f) info |= I_INBIAS;
            m_info[i] = (uint8_t)info;
        }
    }
    for (y = 1; y + 1u < C4_MASK_H; y++) {
        for (x = 1; x + 1u < C4_MASK_W; x++) {
            const unsigned at = y * C4_MASK_W + x, p = MASK_AT(x, y), f = m_face[at], info = m_info[at];
            const int outline = m_face[at - 1u] != f || m_face[at + 1u] != f || m_face[at - C4_MASK_W] != f || m_face[at + C4_MASK_W] != f;
            const int s = MASK_SHADOW(p);

            if (f == 255u) continue;
            if (!MASK_DRAWN(p)) { if (outline) holes_out++; else holes_in++; continue; }
            if (s != ((info & I_ANS) != 0u)) { if (outline) dif_out++; else dif_in++; }
            if (!(info & I_TRUTH)) {
                lit++;
                if (s) { if (info & I_OWN) acne++; else if (info & I_BEHIND) rim++; else edge++; }
            } else {
                shadow++;
                if (!s) { if (info & I_INBIAS) inbias++; else other++; }
            }
        }
    }
    sc_acne = acne; sc_edge = edge; sc_literr = inbias + other; sc_ok = 1u;
    if (brief) {
        say("S  b0 x10 %u: acne %u edge %u rim %u; lit in error %u + %u; vs model %u + %u; holes %u", (unsigned)(b0 * 10.0f + 0.5f), acne, edge, rim, inbias, other, dif_in, dif_out, holes_in + holes_out);
        return;
    }
    say("4 bands %u, vertices %u; holes inside %u, at an outline %u", nbands, nbv, holes_in, holes_out);
    say("4 mask vs model on this map: differ inside a face %u, at an outline %u", dif_in, dif_out);
    say("4 mask vs ray: lit %u shadow %u; acne %u edge %u rim %u; lit in error: in bias %u other %u", lit, shadow, acne, edge, rim, inbias, other);
}

/* Each sun twice: Q from the cosine's edge (c4), then from the measured one (c4b), when it was measured. The model's
   compare follows: the cosine table, then the console's own table from test 1. */
static void t_scene(void) {
    const int had = edges_on;
    unsigned k;

    for (k = 0; k < (e_ok ? 2u : 1u); k++) {
        edges_on = (int)k;
        t_scene_at(150.0f, 26.5f, 32u, 1u, C4_B0, 0);
        t_scene_at(100.0f, 13.0f, 32u, 1u, C4_B0, 0);
    }
    edges_on = had;
}

/* ---- S. the b0 sweep in the scenes (c4c, Q57) ------------------------------------------------------------------ */

/* Both scenes at b0 0 to 3 by 0.5 with Q from the measured edge: where acne starts, and what a smaller bias gives back
   (edge: false shadow from a nearer face; real shadow shown lit). One line a b0; the columns are test 4's. */
static void t_sweep(void) {
    static const float suns[2][2] = { { 150.0f, 26.5f }, { 100.0f, 13.0f } };
    const int had = edges_on;
    unsigned s, k, j, v[7][3], got[7];
    pg_plot_t *g;

    if (!e_ok) { say("S SWEEP: the edges were not measured; not run"); return; }
    edges_on = 1;
    for (s = 0; s < 2u; s++) {
        say("S SWEEP sun %u/%u w 32 bands 2 wide grow .75, Q from the MEASURED edge; counts as test 4's", (unsigned)suns[s][0], (unsigned)suns[s][1]);
        for (k = 0; k < 7u; k++) { t_scene_at(suns[s][0], suns[s][1], 32u, 1u, 0.5f * (float)k, 1); v[k][0] = sc_acne; v[k][1] = sc_edge; v[k][2] = sc_literr; got[k] = sc_ok; }
        g = pg_plot(s ? "S SWEEP sun 100/13, measured edge: px against b0" : "S SWEEP sun 150/26, measured edge: px against b0", "b0, codes", "px of the 320 x 240 mask", 0);
        for (j = 0; j < 3u; j++) {
            pg_series(g, j == 0u ? "acne" : j == 1u ? "edge (false shadow)" : "real shadow shown lit");
            for (k = 0; k < 7u; k++) if (got[k]) pg_point(g, 0.5f * (float)k, (float)v[k][j]);
        }
    }
    edges_on = had;
    say("   (host model on a table 99..157, ESTIMATE: acne 0 at every b0; edge 617 to 503 and 1262 to 1225; lit in error 4 to 17 and 17 to 43)");
}

/* ---- C. the band cut in parts (c4c, Q64) ------------------------------------------------------------------------- */

#define REPEAT 8u

static volatile float cut_sink;

/* bands_build up to a stage: 0 the faces alone; 1 and each face's kind, corners, depth range and bias; 2 and every
   band's polygon (not stored); 3 the whole cut (bands_build itself). The differences are the parts. */
static void bands_upto(const c4_light_t *L, const c4_set_t *st, unsigned upto) {
    c4_face_t f[C4_FACES];
    unsigned i, k;
    float sink = 0.0f;

    if (upto >= 3u) { bands_build(L, st, 0u, C4_FACES); return; }
    c4_faces(f, 0.0f);
    for (i = 0; upto >= 1u && i < C4_FACES; i++) {
        c4_corner_t face[4], out[C4_BAND_MAX];
        float inv[4], lo, hi;
        int b, b1;
        const int kind = c4_face_kind(L, f[i].n, st);

        c4_face_corners(L, &f[i], face, inv);
        if (kind != C4_FACE_COMPARE) continue;
        lo = hi = face[0].m[2];
        for (k = 1; k < 4u; k++) { if (face[k].m[2] < lo) lo = face[k].m[2]; if (face[k].m[2] > hi) hi = face[k].m[2]; }
        sink += c4_bias(L, f[i].n, st->b0, st->gs);
        if (upto < 2u) continue;
        for (b = c4_band(lo, st->gs), b1 = c4_band(hi, st->gs); b <= b1; b++) {
            const unsigned n = c4_band_poly(face, inv, b, st->gs, out);

            if (n) sink += out[n - 1u].p[0];
        }
    }
    cut_sink = sink + f[C4_FACES - 1u].n[1];
}

static void t_cut(void) {
    const int had = edges_on;
    unsigned gs, e, u, r, i, qa = 0, qb = 0;
    c4_light_t L;
    c4_set_t st;
    uint64_t c0, ca, cb, bar[3][4];
    pg_plot_t *g;

    say("C CUT the band cut in parts, sun 150/26.5, cycles / 200 = us, mean of %u; (c4b: 3183 / 1691 / 908 us; c4: 2483 / 1332 / 744)", REPEAT);
    c4_light_fit(&L, 150.0f, 26.5f);
    for (gs = 0; gs < 3u; gs++) {
        uint64_t part[2][4];

        step("C cut");
        for (e = 0; e < 2u; e++) {
            edges_on = (int)e;
            set_init(&st, 32u, gs, C4_B0, 0.75f);
            for (u = 0; u < 4u; u++) {
                bands_upto(&L, &st, u);
                c0 = cyc(); for (r = 0; r < REPEAT; r++) bands_upto(&L, &st, u); part[e][u] = (cyc() - c0) / REPEAT;
            }
        }
        say("C bands %u wide, %u vertices; whole cut: cosine edge %u us, MEASURED edge %u us", 1u << gs, nbv, (unsigned)(part[0][3] / 200u), (unsigned)(part[1][3] / 200u));
        say("C  parts, MEASURED edge, us: faces %u; kinds, corners, bias %u; band polygons %u; Q and the strips %u", (unsigned)(part[1][0] / 200u),
            (unsigned)((part[1][1] - part[1][0]) / 200u), (unsigned)((part[1][2] - part[1][1]) / 200u), (unsigned)((part[1][3] - part[1][2]) / 200u));
        for (u = 0; u < 4u; u++) bar[gs][u] = u ? part[1][u] - part[1][u - 1u] : part[1][0];
        say("C  parts, cosine edge, us: faces %u; kinds, corners, bias %u; band polygons %u; Q and the strips %u", (unsigned)(part[0][0] / 200u),
            (unsigned)((part[0][1] - part[0][0]) / 200u), (unsigned)((part[0][2] - part[0][1]) / 200u), (unsigned)((part[0][3] - part[0][2]) / 200u));
    }
    g = pg_plot("C CUT the band cut's parts, measured edge, us", "band width, codes", "us", 1);
    pg_cat(g, "1 wide"); pg_cat(g, "2 wide"); pg_cat(g, "4 wide");
    for (u = 0; u < 4u; u++) {
        pg_series(g, u == 0u ? "faces" : u == 1u ? "kinds, corners, bias" : u == 2u ? "band polygons" : "Q and the strips");
        for (gs = 0; gs < 3u; gs++) pg_point(g, (float)gs, (float)(bar[gs][u] / 200u));
    }
    edges_on = 1; set_init(&st, 32u, 0u, C4_B0, 0.75f);
    c0 = cyc(); for (i = 0; i < 65536u; i++) qa += c4_q_set(&st, (float)(i & 255u) + 0.5f, 3.5f); ca = cyc() - c0;
    c0 = cyc(); for (i = 0; i < 65536u; i++) qb += c4_q((float)(i & 255u) + 0.5f, 32u, 3.5f); cb = cyc() - c0;
    cut_sink = (float)(qa + qb);
    say("C  one Q: from the measured edge %u.%u cycles, from the cosine's %u.%u (65536 calls each, the loop included)", (unsigned)(ca * 10u / 65536u) / 10u, (unsigned)(ca * 10u / 65536u) % 10u,
        (unsigned)(cb * 10u / 65536u) / 10u, (unsigned)(cb * 10u / 65536u) % 10u);
    edges_on = had;
    say("   (a part is the difference of two timed runs; test 5 times the cut by the ns clock, this page by the cycle counter)");
}

/* ---- 5. the cost ------------------------------------------------------------------------------------- */

static void t_cost(void) {
    c4_light_t L;
    c4_set_t st;
    unsigned gs, r;

    say("5 COST sun 150/26, SH-4 us and cycles, PVR render us");
    c4_light_fit(&L, 150.0f, 26.5f);
    faces_build(&L);
    for (gs = 0; gs < 3u; gs++) {
        uint64_t t0, c0, cut = 0, proj = 0, projc = 0, fit = 0;
        unsigned map_us, mask_us;
        int same;

        step("5 cost");
        set_init(&st, 32u, gs, C4_B0, 0.75f);
        for (r = 0; r < REPEAT; r++) {
            t0 = ns(); c4_light_fit(&L, 150.0f, 26.5f); fit += ns() - t0;
            t0 = ns(); bands_build(&L, &st, 0u, C4_FACES); cut += ns() - t0;
            shz_xmtrx_load_4x4(&cam_mat[0]);
            c0 = cyc(); shz_dce_pvr_project(bscr, bsrc, nbv); proj += cyc() - c0;
            c0 = cyc(); shz_dce_pvr_project_c(bref, bsrc, nbv); projc += cyc() - c0;
        }
        same = !memcmp(bscr, bref, nbv * sizeof bscr[0]);
        if (!map_scene(&L, &st, 0u, C4_FACES)) { say("5 map: NO RENDER END"); return; }
        map_us = us_of(last_render_ns);
        if (!mask_pass(bscr, nbv, st.thr)) { say("5 mask: NO RENDER END"); return; }
        mask_us = us_of(last_render_ns);
        say("5 bands %u wide: %u bands %u vertices; fit %u us, cut %u us", 1u << gs, nbands, nbv, us_of(fit / REPEAT), us_of(cut / REPEAT));
        say("5  project: asm %u.%u cycles a vertex, C form %u.%u, bytes %s", (unsigned)(proj * 10u / (REPEAT * nbv)) / 10u, (unsigned)(proj * 10u / (REPEAT * nbv)) % 10u,
            (unsigned)(projc * 10u / (REPEAT * nbv)) / 10u, (unsigned)(projc * 10u / (REPEAT * nbv)) % 10u, same ? "the same" : "DIFFER");
        say("5  send map %u us (%u vertices), mask %u us (2 x %u); PVR map %u us, mask %u us", us_of(map_send_ns), nmapv, us_of(mask_send_ns), nbv, map_us, mask_us);
    }
    step("5 screen");
    if (!screen_pass(0u, 0)) { say("5 screen: NO RENDER END"); return; }
    r = us_of(last_render_ns);
    if (!screen_pass(140u, 0)) { say("5 screen: NO RENDER END"); return; }
    say("5 PVR screen: scene %u us, with the shadow layer %u us", r, us_of(last_render_ns));
}

/* ---- 6. punch-through ------------------------------------------------------------------------------------ */

static uint16_t scr_px[256u * C4_MAP];

/* One punch-through pass onto the screen: n ready vertices through h, reference ref. Returns the pixels of the
   w x 128 corner that are not the background, -1 when the frame did not come back; scr_px holds the corner. */
static int punch_pass(const pvr_poly_hdr_t *h, const pvr_vertex_t *v, unsigned n, unsigned ref, unsigned w) {
    const volatile uint16_t *fb;
    unsigned x, y;
    int drawn = 0;

    sh4der_fault_beat(&crumbs);
    PVR_SET(PVR_PT_ALPHA_REF, ref);
    if (!scene_open(TO_SCREEN)) return -1;
    pvr_list_begin(PVR_LIST_PT_POLY);
    ta_send(h, v, n);
    pvr_list_finish();
    PVR_SET(PVR_PT_ALPHA_REF, ref);
    if (!scene_close()) return -1;
    fb = (const volatile uint16_t *)(uintptr_t)(0xa5000000u | (render_addr & 0x00ffffffu));
    for (y = 0; y < C4_MAP; y++) for (x = 0; x < w; x++) { const unsigned p = fb[y * 640u + x]; scr_px[y * 256u + x] = (uint16_t)p; drawn += p != BG565; }
    return drawn;
}

static void t_punch(void) {
    static const char *const what[5] = { "KallistiOS's header", "no depth write", "depth ALWAYS", "blend ONE/ZERO", "vertex alpha 128" };
    pvr_poly_hdr_t h;
    pvr_poly_cxt_t c;
    unsigned k, n, x, y, model = 0, differ = 0;
    int got[5], drawn;

    say("6 PUNCH ref 128, 4444 alpha ramp: px drawn of 32768");
    for (k = 0; k < 5u; k++) {
        step("6 punch");
        pvr_poly_cxt_txr(&c, PVR_LIST_PT_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, 256, 8, a4_mem, PVR_FILTER_NEAREST);
        c.gen.culling = PVR_CULLING_NONE;                        /* otherwise KallistiOS's own settings for the list */
        if (k == 1u) c.depth.write = PVR_DEPTHWRITE_DISABLE;
        if (k == 2u) c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        if (k == 3u) { c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ZERO; }
        pvr_poly_compile(&h, &c);
        rect(rectv, 0.0f, 0.0f, 256.0f, (float)C4_MAP, 1.0f, 0.0f, 0.5f, 1.0f, 0.5f, k == 4u ? 0x80ffffffu : 0xffffffffu, 0u);
        got[k] = punch_pass(&h, rectv, 4u, 128u, 256u);
    }
    for (k = 0; k < 5u; k++) say("6 %s: %d", what[k], got[k]);
    say("   (texture alpha >= ref: 16384; all: 32768; if the vertex alpha counts, the last line is 2048 or 0)");
    step("6 bump");
    if (!map_synthetic()) { say("6 bump map: NO RENDER END"); return; }
    pvr_poly_cxt_txr(&c, PVR_LIST_PT_POLY, PVR_TXRFMT_BUMP | PVR_TXRFMT_NONTWIDDLED, (int)C4_MAP, (int)C4_MAP, map_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE; c.gen.specular = 1; c.txr.uv_clamp = PVR_UVCLAMP_UV;
    pvr_poly_compile(&h, &c);
    n = window_quads() / 2u;               /* the left block: even codes */
    drawn = punch_pass(&h, rectv, n, 128u, STRIPS);
    if (drawn < 0) { say("6 bump: NO RENDER END"); return; }
    for (y = 0; y < C4_MAP; y++) {
        for (x = 0; x < STRIPS; x++) {
            const int want = c4_level[(map_px[y * C4_MAP + 32u] - STRIP_Q(x)) & 255u] >= 128u;

            model += (unsigned)want; differ += (scr_px[y * 256u + x] != BG565) != want;
        }
    }
    say("6 bump level as the tested alpha: %d drawn of 20480, model %u, differ %u", drawn, model, differ);
}

/* ---- 7. PVR-DMA with rare polling ------------------------------------------------------------------------- */

static uint8_t dma_dst[524288u] __attribute__((aligned(32))), dma_src[524288u] __attribute__((aligned(32)));
static volatile uint64_t dma_done_ns;

static void dma_evt(uint32_t code, void *data) {
    (void)code; (void)data;
    dma_done_ns = ns();
}

static void t_dma(void) {
    static const unsigned sizes[2] = { 131072u, 524288u };
    static const char *const how[3] = { "polled each 1 ms", "polled each 250 us", "by interrupt" };
    pvr_ptr_t blk = pvr_mem_malloc(sizeof dma_src);
    uint32_t seed = 9u;
    unsigned i, m;

    say("7 DMA video memory to RAM by PVR-DMA");
    if (!blk) { say("7: no video memory for the block"); return; }
    for (i = 0; i < sizeof dma_src; i += 4u) { seed = seed * 1664525u + 1013904223u; memcpy(dma_src + i, &seed, 4u); }
    pvr_txr_load(dma_src, blk, sizeof dma_src);
    for (i = 0; i < 2u; i++) {
        for (m = 0; m < 3u; m++) {
            uint64_t t0, t = 0;
            int ended = 0;

            step("7 dma");
            memset(dma_dst, 0, sizes[i]);
            if (m == 2u) { dma_done_ns = 0; asic_evt_set_handler(EVT_PVR_DMA_END, dma_evt, NULL); asic_evt_enable(EVT_PVR_DMA_END, ASIC_IRQ_DEFAULT); }
            pd_start(blk, dma_dst, sizes[i]);
            t0 = ns();
            if (m == 2u) {
                while (!dma_done_ns && ns() - t0 < LIMIT_NS) { }
                ended = dma_done_ns != 0; t = dma_done_ns - t0;
                asic_evt_disable(EVT_PVR_DMA_END, ASIC_IRQ_DEFAULT); asic_evt_remove_handler(EVT_PVR_DMA_END);
            } else {
                do { wait_ns(m ? 250000u : 1000000u); ended = !pd_busy(); t = ns() - t0; } while (!ended && t < LIMIT_NS);
            }
            pd_end();
            if (!ended) say("7 %u KB %s: no end", sizes[i] / 1024u, how[m]);
            else say("7 %u KB %s: %u us, %u.%u MB/s, copy %s", sizes[i] / 1024u, how[m], us_of(t), mbs10(sizes[i], t) / 10u, mbs10(sizes[i], t) % 10u,
                     memcmp(dma_dst, dma_src, sizes[i]) ? "WRONG" : "right");
        }
    }
    say("   (a polled time is rounded up to the next poll)");
    pvr_mem_free(blk);
}

/* ---- 8. the frame: pictures and waits (c4b) ------------------------------------------------------------------ */

static void vbl_tick(uint32_t code, void *data) {
    (void)code; (void)data;
    vbl_count++;
}

#define FR_N 60u
#define FR_WAITS 9u                        /* map, mask, screen x (open ready, close ready, render done) */

typedef struct fr_stat { uint64_t sum, max; } fr_stat_t;

static void fr_add(fr_stat_t *s, uint64_t v) { s->sum += v; if (v > s->max) s->max = v; }

static void t_frame(void) {
    static const char *const pass[3] = { "map", "mask", "screen" };
    static const char *const mname[3] = { "KallistiOS waits", "ready POLLED", "8b: ready and done POLLED" };
    static uint32_t ft[FR_N];
    unsigned mode, f, k, i;
    pg_plot_t *gt, *gw, *gv;
    uint64_t rnd0[3] = { 0, 0, 0 };
    c4_light_t L;
    c4_set_t st;

    gt = pg_plot("8 FRAME each frame's time, us", "frame", "us", 0);
    gw = pg_plot("8 FRAME render-done wait of each pass, mean us", "pass", "us", 1);
    gv = pg_plot("8 FRAME vertical blanks a frame: frames of 60 with each count", "vblanks in the frame", "frames", 1);
    pg_cat(gw, "map"); pg_cat(gw, "mask"); pg_cat(gw, "screen");
    for (k = 0; k < 8u; k++) { static const char *const c[8] = { "0", "1", "2", "3", "4", "5", "6", "7+" }; pg_cat(gv, c[k]); }
    say("8 FRAME %u frames of the live chain at sun 150/26.5, bands 2 wide, layer on; us", FR_N);
    say("8  scheduler %u Hz; render-done interrupt %s", (unsigned)thd_get_hz(), rd_hooked ? "seen by this program" : "NOT hooked: no 8b, no late figures");
    fetch_off = 1;
    for (mode = 0; mode < (rd_hooked ? 3u : 2u); mode++) {
        fr_stat_t w[FR_WAITS], rnd[3], late[3], fr;
        unsigned hist[8], dvb[3] = { 0, 0, 0 };
        uint64_t fmin = ~0ull, awake = 0;

        memset(w, 0, sizeof w); memset(rnd, 0, sizeof rnd); memset(late, 0, sizeof late); memset(&fr, 0, sizeof fr); memset(hist, 0, sizeof hist);
        poll_ready = mode >= 1u;
        poll_done = mode == 2u;
        for (f = 0; f < FR_N; f++) {
            const uint32_t v0 = vbl_count;
            const uint64_t t0 = ns(), c0 = cyc();
            uint64_t t;
            unsigned vb;

            step("8 frame");
            c4_light_fit(&L, 150.0f, 26.5f);
            set_init(&st, 32u, 1u, C4_B0, 0.75f);
            bands_build(&L, &st, 0u, C4_FACES);
            faces_build(&L);
            if (!map_scene(&L, &st, 0u, C4_FACES)) { say("8 map: NO RENDER END"); goto out; }
            fr_add(&w[0], w_open_ns); fr_add(&w[1], w_close_ns); fr_add(&w[2], w_done_ns); fr_add(&rnd[0], last_render_ns); fr_add(&late[0], w_late_ns); dvb[0] += w_done_vbl;
            bands_project();
            if (!mask_pass(bscr, nbv, st.thr)) { say("8 mask: NO RENDER END"); goto out; }
            fr_add(&w[3], w_open_ns); fr_add(&w[4], w_close_ns); fr_add(&w[5], w_done_ns); fr_add(&rnd[1], last_render_ns); fr_add(&late[1], w_late_ns); dvb[1] += w_done_vbl;
            if (!screen_pass(140u, 1)) { say("8 screen: NO RENDER END"); goto out; }
            fr_add(&w[6], w_open_ns); fr_add(&w[7], w_close_ns); fr_add(&w[8], w_done_ns); fr_add(&rnd[2], last_render_ns); fr_add(&late[2], w_late_ns); dvb[2] += w_done_vbl;
            t = ns() - t0; vb = vbl_count - v0;
            awake += cyc() - c0;
            fr_add(&fr, t); if (t < fmin) fmin = t;
            ft[f] = (uint32_t)t;
            hist[vb < 7u ? vb : 7u]++;
        }
        say("8 %s: frame min %u mean %u max %u", mname[mode], us_of(fmin), us_of(fr.sum / FR_N), us_of(fr.max));
        pg_series(gt, mname[mode]); for (f = 0; f < FR_N; f++) pg_point(gt, (float)(f + 1u), (float)us_of(ft[f]));
        pg_series(gw, mname[mode]); for (k = 0; k < 3u; k++) pg_point(gw, (float)k, (float)us_of(w[3u * k + 2u].sum / FR_N));
        pg_series(gv, mname[mode]); for (k = 0; k < 8u; k++) pg_point(gv, (float)k, (float)hist[k]);
        if (mode == 0u) for (k = 0; k < 3u; k++) rnd0[k] = rnd[k].sum / FR_N;
        say("8  vblanks a frame 0..6, 7+: %u %u %u %u %u %u %u %u", hist[0], hist[1], hist[2], hist[3], hist[4], hist[5], hist[6], hist[7]);
        for (k = 0; k < 3u; k++) {
            say("8  %s waits mean/max: open %u/%u close %u/%u done %u/%u; render %u", pass[k], us_of(w[3u * k].sum / FR_N), us_of(w[3u * k].max),
                us_of(w[3u * k + 1u].sum / FR_N), us_of(w[3u * k + 1u].max), us_of(w[3u * k + 2u].sum / FR_N), us_of(w[3u * k + 2u].max), us_of(rnd[k].sum / FR_N));
            say("8   %s done-wait: back %u/%u us after the interrupt; %u vblanks in the %u waits", pass[k], us_of(late[k].sum / FR_N), us_of(late[k].max), dvb[k], FR_N);
        }
        say("8  frames 1-8: %u %u %u %u %u %u %u %u", us_of(ft[0]), us_of(ft[1]), us_of(ft[2]), us_of(ft[3]), us_of(ft[4]), us_of(ft[5]), us_of(ft[6]), us_of(ft[7]));
        {
            uint64_t waits = 0;

            for (i = 0; i < FR_WAITS; i++) waits += w[i].sum;
            say("8  DERIVED mean frame minus mean waits: %u", us_of((fr.sum - (waits < fr.sum ? waits : fr.sum)) / FR_N));
        }
        say("8  mean frame by the cycle counter: %u us (it may stop while the CPU sleeps in a wait)", (unsigned)(awake / FR_N / 200u));
    }
    pg_series(gw, "the PVR's render (run 1)"); for (k = 0; k < 3u; k++) pg_point(gw, (float)k, (float)us_of(rnd0[k]));
out:
    poll_ready = 0;
    poll_done = 0;
    fetch_off = 0;
    say("   (a vblank is one picture: 16.7 ms at 60 Hz. The ns clock may step 2.5 ms ahead once a second: a max about 2.5 ms over its mean can be that step, ray2 CLOCK.4)");
    say("   (8b predicted from KallistiOS's source, ESTIMATE: the render-done wake does not reschedule, so the sleeping wait comes back on the scheduler's next tick)");
}

/* ---- 9. the mask by punch-through (c4b) ------------------------------------------------------------------------ */

static uint8_t route_a[C4_MASK_W * C4_MASK_H];

static void t_route_at(float az, float el) {
    unsigned x, y, sa = 0, sb = 0, differ = 0, holes = 0, us_a, us_b, ncol = 0, k;
    uint16_t col[4] = { 0, 0, 0, 0 };
    unsigned coln[4] = { 0, 0, 0, 0 };
    c4_light_t L;
    c4_set_t st;

    c4_light_fit(&L, az, el);
    set_init(&st, 32u, 1u, C4_B0, 0.75f);
    step("9 route");
    if (!map_scene(&L, &st, 0u, C4_FACES)) { say("9 map: NO RENDER END"); return; }
    bands_build(&L, &st, 0u, C4_FACES);
    bands_project();
    mask_route = 0;
    if (!mask_pass(bscr, nbv, st.thr)) { say("9 1555: NO RENDER END"); return; }
    us_a = us_of(last_render_ns);
    for (y = 0; y < C4_MASK_H; y++) for (x = 0; x < C4_MASK_W; x++) { const unsigned p = MASK_AT(x, y); route_a[y * C4_MASK_W + x] = (uint8_t)(MASK_DRAWN(p) ? 1u + (MASK_SHADOW(p) != 0u) : 0u); }
    mask_route = 1;
    if (!mask_pass(bscr, nbv, st.thr)) { mask_route = C4B_MASK_PT; say("9 punch-through: NO RENDER END"); return; }
    us_b = us_of(last_render_ns);
    for (y = 0; y < C4_MASK_H; y++) {
        for (x = 0; x < C4_MASK_W; x++) {
            const unsigned p = MASK_AT(x, y), a = route_a[y * C4_MASK_W + x];
            const unsigned b = MASK_DRAWN(p) ? 1u + (MASK_SHADOW(p) != 0u) : 0u;

            if (!b && a) holes++;
            sa += a == 2u; sb += b == 2u; differ += a != b;
            if (b == 2u) {                   /* the colours punch-through left, the four most common */
                for (k = 0; k < ncol && col[k] != p; k++) { }
                if (k < ncol) coln[k]++; else if (ncol < 4u) { col[ncol] = (uint16_t)p; coln[ncol++] = 1u; }
            }
        }
    }
    mask_route = C4B_MASK_PT;
    say("9 sun %u/%u: shadow px 1555 %u, punch-through %u; differ %u; not drawn by route 2 %u", (unsigned)az, (unsigned)el, sa, sb, differ, holes);
    say("9  PVR mask pass: 1555 %u us, punch-through %u us", us_a, us_b);
    say("9  punch-through colours: %04x x%u %04x x%u %04x x%u %04x x%u", col[0], coln[0], col[1], coln[1], col[2], coln[2], col[3], coln[3]);
}

static void t_route(void) {
    say("9 ROUTE the mask by punch-through, reference = threshold %u, Q from the %s edge", c4_threshold[32], edges_on && e_ok ? "measured" : "cosine's");
    t_route_at(150.0f, 26.5f);
    t_route_at(100.0f, 13.0f);
    say("   (the tested alpha is texture x vertex alpha: PVR-064; here vertex alpha 255)");
}

/* ---- the run ---------------------------------------------------------------------------------------------- */

/* Start the PVR and load the textures. 0 when it could not. */
static int start(void) {
    pvr_init_params_t init = pvr_default_params;

    init.opb_sizes[PVR_LIST_OP_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_OP_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_TR_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_PT_POLY] = PVR_BINSIZE_16;
    init.vertex_buf_size = 768 * 1024;
    init.autosort_disabled = 1;                                  /* translucent polygons in the order they are sent */
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return 0; }
    pvr_set_bg_color(0.0f, 0.0f, 1.0f);
    if (!load()) { say("video memory allocation FAIL"); pvr_shutdown(); return 0; }
    headers();
    vid_set_dithering(false);
    rd_prev = asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, rd_evt, NULL);   /* c4c: ours in front of KallistiOS's */
    rd_hooked = rd_prev.hdl != NULL;
    if (!rd_hooked) asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, NULL, NULL);
    return 1;
}

static void stop(const char *stage) {
    step(stage);
    if (rd_hooked) { asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, rd_prev.hdl, rd_prev.data); rd_hooked = 0; }   /* KallistiOS's own back */
    vid_set_dithering(true);
    pvr_shutdown();
}

static void run(void) {
    say("c4g. ns clock: reload step taken out. numbers below: measured by this run. GRAPH pages follow the text pages: shape only, the values are in the text");
    if (vblank_handler_add(vbl_tick, NULL) < 0) say("vblank counter: refused (test 8 counts 0)");
    if (!start()) return;
    if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused");
    perf_cntr_start(PRFC1, PMCR_ELAPSED_TIME_MODE, PMCR_COUNT_CPU_CYCLES);
    say("PLACING bsrc %08x bscr %08x bref %08x; apart mod 16 KB: %u %u %u", (unsigned)(uintptr_t)bsrc, (unsigned)(uintptr_t)bscr, (unsigned)(uintptr_t)bref,
        (unsigned)(((uintptr_t)bscr - (uintptr_t)bsrc) % OC), (unsigned)(((uintptr_t)bref - (uintptr_t)bsrc) % OC), (unsigned)(((uintptr_t)bref - (uintptr_t)bscr) % OC));
    t_ramp();
    t_window();
    t_bias();
    t_scene();
    t_sweep();
    t_cost();
    t_cut();
    t_punch();
    t_dma();
    t_frame();
    t_route();
    if (fetch_slow) say("fetch: %u blocks were read word by word", fetch_slow);
    stop("done");
    say("START: live view. Y: the tile list stage, LAST (photograph first).");
}

/* ---- the live view ---------------------------------------------------------------------------------------- */

static void live(void) {
    float az = 150.0f, el = 26.5f, b0 = C4_B0;
    unsigned gs = 1u, over = 1u, held = ~0u, frames = 0;
    uint64_t cut = 0, proj = 0, pm = 0, pk = 0, ps = 0, frame = 0, t0, tf;
    c4_light_t L;
    c4_set_t st;

    if (!start()) return;
    fetch_off = 1;
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *pad = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = pad ? pad->buttons : 0u, hit = held == ~0u ? 0u : now & ~held;

        held = now;
        if (hit & CONT_START) break;
        if (now & CONT_DPAD_LEFT) az -= 1.0f;
        if (now & CONT_DPAD_RIGHT) az += 1.0f;
        if (now & CONT_DPAD_UP) el += 0.5f;
        if (now & CONT_DPAD_DOWN) el -= 0.5f;
        if (az < 0.0f) az += 360.0f;
        if (az >= 360.0f) az -= 360.0f;
        el = sh4der_clamp_(el, 8.0f, 80.0f);
        if (hit & CONT_A) gs = (gs + 1u) % 3u;
        if (hit & CONT_B) over ^= 1u;
        if (hit & CONT_X) b0 -= 0.25f;
        if (hit & CONT_Y) b0 += 0.25f;
        b0 = sh4der_clamp_(b0, 0.0f, 8.0f);

        step("live");
        tf = ns();
        t0 = ns();                         /* everything is made again every frame: the cost when the sun moves */
        c4_light_fit(&L, az, el);
        set_init(&st, 32u, gs, b0, 0.75f);
        bands_build(&L, &st, 0u, C4_FACES);
        faces_build(&L);
        cut = ns() - t0;
        if (!map_scene(&L, &st, 0u, C4_FACES)) { say("live: map NO RENDER END"); break; }
        pm = last_render_ns;
        t0 = ns();
        bands_project();
        proj = ns() - t0;
        if (!mask_pass(bscr, nbv, st.thr)) { say("live: mask NO RENDER END"); break; }
        pk = last_render_ns;
        if (!screen_pass(over ? 140u : 0u, 1)) { say("live: screen NO RENDER END"); break; }
        ps = last_render_ns;
        frame = ns() - tf;
        if (++frames % 120u == 0u)
            printf("c4: live az %u el %u b0 x100 %u bands %u wide: %u vertices, cut %u us, project %u us, PVR %u %u %u us, frame %u us\n", (unsigned)az, (unsigned)el,
                   (unsigned)(b0 * 100.0f), 1u << gs, nbv, us_of(cut), us_of(proj), us_of(pm), us_of(pk), us_of(ps), us_of(frame));
    }
    fetch_off = 0;
    stop("live end");
    say("LIVE left at sun %u/%u, b0 x100 %u, bands %u wide, layer %s, Q from the %s edge, mask by %s: %u vertices; SH-4 fit+cut+faces %u us, project %u us; PVR map %u mask %u screen %u us; frame %u us (it waits for the picture's turn)",
        (unsigned)az, (unsigned)el, (unsigned)(b0 * 100.0f), 1u << gs, over ? "on" : "off", edges_on && e_ok ? "measured" : "cosine's", mask_route ? "punch-through" : "1555",
        nbv, us_of(cut), us_of(proj), us_of(pm), us_of(pk), us_of(ps), us_of(frame));
}

/* ---- the last stage: a tile list that ends early ------------------------------------------------------------- */

#define TM_W 20u                           /* the screen's tiles: 640 / 32 by 480 / 32 */
#define TM_H 15u
#define TM_WORDS (6u + 6u * TM_W * TM_H)

static uint32_t tm_save[2][TM_WORDS], tm_addr[2];
static unsigned tm_n;

static volatile uint32_t *tm_at(unsigned k) { return (volatile uint32_t *)(uintptr_t)(0xa5000000u | tm_addr[k]); }

/* 1 when the table at tm_addr[k] is laid out as KallistiOS's source says: a first entry of 10000000 and five
   80000000, then one entry a tile, column by column, the last one marked. */
static int tm_valid(unsigned k) {
    const volatile uint32_t *w = tm_at(k);
    unsigned x, y, i;

    if (w[0] != 0x10000000u) return 0;
    for (i = 1; i < 6u; i++) if (w[i] != 0x80000000u) return 0;
    for (x = 0; x < TM_W; x++) {
        for (y = 0; y < TM_H; y++) {
            const volatile uint32_t *e = w + 6u + 6u * (x * TM_H + y);
            const int last = x == TM_W - 1u && y == TM_H - 1u;

            if ((e[0] & 0x0000fffcu) != ((y << 8) | (x << 2))) return 0;
            if (((e[0] >> 31) != 0u) != last) return 0;
            for (i = 1; i < 6u; i++) if (e[i] != 0x80000000u && e[i] >= 0x00800000u) return 0;
        }
    }
    return 1;
}

/* Make every table end after the tiles of a w x h tile corner. */
static void tm_short(unsigned w, unsigned h) {
    unsigned k, x, y, i, j;

    for (k = 0; k < tm_n; k++) {
        volatile uint32_t *t = tm_at(k);

        for (j = 0, x = 0; x < w; x++) {
            for (y = 0; y < h; y++, j++) {
                const uint32_t *e = tm_save[k] + 6u + 6u * (x * TM_H + y);

                t[6u + 6u * j] = (e[0] & 0x7fffffffu) | (j + 1u == w * h ? 0x80000000u : 0u);
                for (i = 1; i < 6u; i++) t[6u + 6u * j + i] = e[i];
            }
        }
    }
}

/* Put every word of every table back. Returns the words that do not read back as saved (0 = as before). */
static unsigned tm_restore(void) {
    unsigned k, i, tries, bad = 0;

    for (tries = 0; tries < 3u; tries++) {
        for (bad = 0, k = 0; k < tm_n; k++) {
            volatile uint32_t *t = tm_at(k);

            for (i = 0; i < TM_WORDS; i++) t[i] = tm_save[k][i];
            for (i = 0; i < TM_WORDS; i++) bad += t[i] != tm_save[k][i];
        }
        if (!bad) break;
    }
    return bad;
}

static uint32_t sum16(const uint16_t *p, unsigned n) {
    uint32_t s = 0;
    unsigned i;

    for (i = 0; i < n; i++) s = s * 31u + p[i];
    return s;
}

static void tiles(void) {
    uint32_t sum[2][3] = { { 0, 0, 0 }, { 0, 0, 0 } };
    unsigned us[2][3] = { { 0, 0, 0 }, { 0, 0, 0 } }, k, i, r, bad = 0;
    c4_light_t L;
    c4_set_t st;

    say("TILES a tile list that ends after the target's tiles");
    if (!start()) return;
    c4_light_fit(&L, 150.0f, 26.5f);
    set_init(&st, 32u, 1u, C4_B0, 0.75f);
    bands_build(&L, &st, 0u, C4_FACES);
    bands_project();
    for (tm_n = 0, r = 0; r < 4u; r++) {   /* find the tables: KallistiOS alternates two */
        uint32_t a;

        step("tiles find");
        if (!map_scene(&L, &st, 0u, C4_FACES)) { say("TILES: NO RENDER END before any write"); stop("tiles end"); return; }
        a = PVR_GET(PVR_ISP_TILEMAT_ADDR) & 0x00fffffcu;
        for (k = 0; k < tm_n; k++) if (tm_addr[k] == a) break;
        if (k == tm_n && tm_n < 2u) tm_addr[tm_n++] = a;
    }
    for (k = 0; k < tm_n; k++) {
        if (!tm_valid(k)) { say("TILES: the table at %06x is not laid out as read: nothing written, stage left out", (unsigned)tm_addr[k]); stop("tiles end"); return; }
        for (i = 0; i < TM_WORDS; i++) tm_save[k][i] = tm_at(k)[i];
    }
    say("TILES %u table(s) at %06x %06x, %u words each, saved", tm_n, (unsigned)tm_addr[0], (unsigned)(tm_n > 1u ? tm_addr[1] : 0u), TM_WORDS);
    for (r = 0; r < 3u && !bad; r++) {     /* 0 the whole list, 1 the short list, 2 the whole list again */
        step(r == 1u ? "tiles short" : "tiles whole");
        if (r == 1u) tm_short(4u, 5u);     /* 128 x 130 pixels */
        k = (unsigned)map_scene(&L, &st, 0u, C4_FACES);
        if (r == 1u) bad = tm_restore();
        if (!k) { say("TILES map run %u: NO RENDER END; words not restored %u", r, bad); break; }
        us[0][r] = us_of(last_render_ns); sum[0][r] = sum16(map_ram, C4_MAP * C4_MAP);
        if (bad) break;
        if (r == 1u) tm_short(10u, 8u);    /* 320 x 242 pixels */
        k = (unsigned)mask_pass(bscr, nbv, st.thr);
        if (r == 1u) bad = tm_restore();
        if (!k) { say("TILES mask run %u: NO RENDER END; words not restored %u", r, bad); break; }
        us[1][r] = us_of(last_render_ns); sum[1][r] = sum16(mask_ram, MASK_TW * C4_MASK_H);
    }
    say("TILES map: whole list %u us, 20 tiles %u us, whole again %u us; picture %s", us[0][0], us[0][1], us[0][2],
        sum[0][0] == sum[0][1] && sum[0][1] == sum[0][2] ? "the same all three" : "DIFFERS");
    say("TILES mask: whole list %u us, 80 tiles %u us, whole again %u us; picture %s", us[1][0], us[1][1], us[1][2],
        sum[1][0] == sum[1][1] && sum[1][1] == sum[1][2] ? "the same all three" : "DIFFERS");
    say("TILES restore: %u words differ from the saved tables (0 = as before)%s", bad, bad ? "; the PVR is shut down now, which builds them anew at its next start" : "");
    stop("tiles end");
}

int main(void) {
    unsigned page = 0, pages, held = ~0u;
    int tiled = 0, redraw = 1;

    suite_init();                          /* the suite disc: the menu's binary into RAM, before anything else */
    run();
    vid_set_mode(DM_640x480, PM_RGB565);
    pg_init();
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;
        int back = 0;

        sh4der_fault_beat(&crumbs);
        held = now;
        pages = pg_pages();
        if (hit & CONT_A) { page = (page + 1u) % pages; redraw = 1; }
        else if (hit & CONT_B) { page = (page + pages - 1u) % pages; redraw = 1; }
        else if (hit & CONT_START) { live(); back = 1; }
        else if ((hit & CONT_Y) && !tiled) { tiled = 1; tiles(); back = 1; }
        else if (suite_tick(now, page, pages)) { page++; redraw = 1; }   /* the suite disc: a chained run turns the page; ABXY leaves */
        if (back) {
            vid_set_mode(DM_640x480, PM_RGB565);
            pages = pg_pages();
            page = pg_text_pages() ? pg_text_pages() - 1u : 0u;   /* the last text page: what the stage just added */
            held = ~0u;                    /* what is still held from the stage just left is not a press here */
            redraw = 1;
        }
        if (redraw) { pg_draw(page, "PVR CHAIN c4", suite_keys_text(tiled ? "START live" : "START live Y tiles")); redraw = 0; }
        thd_sleep(20);
    }
    return 0;
}
