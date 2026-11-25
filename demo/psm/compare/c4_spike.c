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

   START on the result pages: the live view (d-pad the sun, A band width, X / Y bias down / up, B the shadow layer
   on / off, START back; the last settings and their costs are added to the pages).
   Y on the result pages, once, LAST: the tile list stage. KallistiOS renders a target through the screen's 300
   tiles; this stage rewrites the two tile tables in video memory to end after the target's own tiles (20 for the
   map, 80 for the mask), renders, and puts every word back (read back and compared). Photograph the pages first.

   Only PVR and system-bus DMA registers and video memory are written. The fault catcher (../psm_fault.h) is in;
   every wait is bounded. Every number comes from the run that prints it; only a real console's run is evidence.
   Build: python demo/psm/compare/build.py --spike c4 --sh4zam <checkout>. Not run on a console when this file was
   written.
*/

#include <kos.h>
#include <stdarg.h>

#include <sh4zam_dce/shz_dce_pvr.h>

#define PSM_FAULT_IMPL
#include "../psm_fault.h"
#include "c4_model.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

/* ---- the result pages ----------------------------------------------------------- */

#define COLS 50u
#define ROWS 17u
#define LINES_MAX 220u
static char lines[LINES_MAX][COLS + 1u];
static unsigned nlines;
static psm_fault_crumbs_t crumbs;

/* A line for the pages and the serial line. A line over the page's width goes on, indented, on the next. */
static void say(const char *fmt, ...) {
    char big[240];
    const char *s = big;
    size_t left;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(big, sizeof big, fmt, ap);
    va_end(ap);
    printf("c4: %s\n", big);
    for (left = strlen(big); nlines < LINES_MAX; ) {
        const int cont = s != big;
        const size_t room = cont ? COLS - 3u : COLS, n = left < room ? left : room;

        snprintf(lines[nlines++], COLS + 1u, "%s%.*s", cont ? "   " : "", (int)n, s);
        s += n; left -= n;
        if (!left) break;
    }
}

static void page_draw(unsigned page, unsigned pages, const char *keys) {
    char head[96];
    unsigned i;

    vid_clear(0, 0, 0);
    snprintf(head, sizeof head, "PVR CHAIN c4 p%u/%u A/B page %s", page + 1u, pages, keys);
    bfont_draw_str(vram_s + 16u * 640u + 20u, 640, true, head);
    for (i = 0; i < ROWS && page * ROWS + i < nlines; i++)
        bfont_draw_str(vram_s + (16u + 24u * (i + 2u)) * 640u + 20u, 640, true, lines[page * ROWS + i]);
}

static void step(const char *name) {
    psm_fault_beat(&crumbs);
    psm_fault_stage(&crumbs, name);
    crumbs.frame++;
}

/* ---- clocks ------------------------------------------------------------------------ */

#define LIMIT_NS 200000000ull              /* no wait in this file is longer */

static inline uint64_t ns(void) { return timer_ns_gettime64(); }
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

static pvr_vertex_t bsrc[BV_MAX] __attribute__((aligned(32)));    /* the band vertices, positions in the world */
static pvr_vertex_t bscr[BV_MAX] __attribute__((aligned(32)));    /* the same on the mask's screen */
static pvr_vertex_t bref[BV_MAX] __attribute__((aligned(32)));    /* the C form's, for the compare */
static pvr_vertex_t rectv[RECT_MAX] __attribute__((aligned(32))); /* screen-space quads of the tests */
static pvr_vertex_t mapv[(C4_FACES + 2u) * 4u] __attribute__((aligned(32)));
static pvr_vertex_t facesrc[C4_FACES * 4u] __attribute__((aligned(32))), facev[C4_FACES * 4u] __attribute__((aligned(32)));
static pvr_vertex_t overv[16] __attribute__((aligned(32)));
static shz_mat4x4_t cam_mat[2] __attribute__((aligned(32)));      /* [0] the mask's 320 x 240, [1] the screen's 640 x 480 */
static c4_cam_t cam[2];
static unsigned nbv, nbands, band_over, nmapv, ramp;

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

static pvr_poly_hdr_t h_map[2], h_flat, h_clear, h_bump, h_over, h_show[2];

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

static int scene_open(int to) {
    unsigned t;

    for (t = 0; t < 20u && pvr_wait_ready() < 0; t++) psm_fault_beat(&crumbs);
    if (t == 20u) return 0;
    if (to == TO_MAP) return pvr_scene_begin_rtt(map_mem, C4_MAP, C4_MAP + 2u, C4_MAP) >= 0;
    if (to == TO_MASK) return pvr_scene_begin_rtt(mask_mem, C4_MASK_W, C4_MASK_H + 2u, MASK_TW) >= 0;
    pvr_scene_begin();
    return 1;
}

static int scene_close(void) {
    pvr_stats_t st;
    unsigned t;

    pvr_scene_finish();
    for (t = 0; t < 20u && pvr_wait_ready() < 0; t++) psm_fault_beat(&crumbs);
    if (t == 20u) return 0;
    for (t = 0; t < 20u && pvr_wait_render_done() < 0; t++) psm_fault_beat(&crumbs);
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

            if (n) band_strip(out, n, KQ(c4_q(c4_band_code(b, st->gs), st->w, bias)));
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

/* The mask from n ready vertices (screen positions, map coordinates, Q in the offset colour): an opaque pass for
   depth, a clear, the same polygons through the bump unit; the target written as ARGB1555 with threshold thr. The
   write control register is set after the lists are sent and put back after the render (KallistiOS sets it only when
   the video mode is set). mask_ram gets the picture. 0 when the pass did not come back. */
static int mask_pass(const pvr_vertex_t *v, unsigned n, unsigned thr) {
    const uint32_t saved = PVR_GET(PVR_FB_CFG_2);
    uint64_t t0;
    int ok;

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

#define MASK_AT(x, y) mask_ram[(y) * MASK_TW + (x)]
#define MASK_DRAWN(p) (((p) & 0x7fffu) != BG565)
#define MASK_SHADOW(p) (((p) >> 15) != 0u)

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
                const unsigned p = MASK_AT(x, y), m = map_px[y * C4_MAP + (x >= STRIPS ? 96u : 32u)], level = c4_level[(m - STRIP_Q(x % STRIPS)) & 255u];
                const int gap = (int)level - (int)thr;

                if (!MASK_DRAWN(p)) { holes++; continue; }
                if (MASK_SHADOW(p) != (level >= thr)) { differ++; close += gap >= -2 && gap <= 2; }
            }
        }
        say("1 w %u thr %u: differ %u, of them within 2 levels of thr %u, holes %u", ws[k], thr, differ, close, holes);
    }
}

/* ---- 3. the bias -------------------------------------------------------------------------------- */

static void t_bias(void) {
    static const float b0s[6] = { 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f };
    unsigned sh[6] = { 0, 0, 0, 0, 0, 0 }, drawn = 0, k, x, y, gs;
    c4_light_t L;
    c4_set_t st;

    say("3 BIAS ground alone: every shadow px is acne");
    c4_light_fit(&L, 150.0f, 26.5f);
    step("3 map");
    c4_set_init(&st, 32u, 0u, C4_B0, 0.75f);
    if (!map_scene(&L, &st, 0u, 1u)) { say("3 map: NO RENDER END"); return; }
    for (gs = 0; gs < 3u; gs += 2u) {
        for (k = 0; k < 6u; k++) {
            step("3 bias");
            c4_set_init(&st, 32u, gs, b0s[k], 0.75f);
            bands_build(&L, &st, 0u, 1u);
            bands_project();
            if (!mask_pass(bscr, nbv, st.thr)) { say("3 b0 x10 %u: NO RENDER END", (unsigned)(b0s[k] * 10.0f)); return; }
            for (sh[k] = drawn = 0, y = 0; y < C4_MASK_H; y++) for (x = 0; x < C4_MASK_W; x++) { const unsigned p = MASK_AT(x, y); if (MASK_DRAWN(p)) { drawn++; sh[k] += MASK_SHADOW(p); } }
        }
        say("3 bands %u wide, b0 0 .5 1 1.5 2 3: %u %u %u %u %u %u shadow of %u px", 1u << gs, sh[0], sh[1], sh[2], sh[3], sh[4], sh[5], drawn);
    }
}

/* ---- 4. the scene --------------------------------------------------------------------------------- */

enum { I_ANS = 1, I_TRUTH = 2, I_OWN = 4, I_BEHIND = 8, I_INBIAS = 16 };

static void t_scene_at(float az, float el, unsigned w, unsigned gs) {
    static const float up[3] = { 0.0f, 1.0f, 0.0f };
    unsigned eq = 0, one = 0, more = 0, x, y, i;
    unsigned holes_in = 0, holes_out = 0, dif_in = 0, dif_out = 0, lit = 0, shadow = 0, acne = 0, edge = 0, rim = 0, inbias = 0, other = 0;
    c4_light_t L;
    c4_set_t st;
    float ground_bias;

    c4_light_fit(&L, az, el);
    c4_set_init(&st, w, gs, C4_B0, 0.75f);
    ground_bias = c4_bias(&L, up, st.b0, st.gs);
    say("4 SCENE sun %u/%u w %u bands %u wide grow .75", (unsigned)az, (unsigned)el, w, 1u << gs);
    step("4 map");
    if (!map_scene(&L, &st, 0u, C4_FACES)) { say("4 map: NO RENDER END"); return; }
    c4_model_map(&L, &st, model_map, model_ids);
    for (i = 0; i < C4_MAP * C4_MAP; i++) {
        const int d = (int)map_px[i] - (int)model_map[i];

        if (!d) eq++; else if (d == 1 || d == -1) one++; else more++;
    }
    say("4 map vs model: same %u, 1 code off %u, more %u, not a code %u", eq, one, more, map_notcode);
    step("4 bands");
    bands_build(&L, &st, 0u, C4_FACES);
    bands_project();
    if (band_over) { say("4 bands: over %u vertices", BV_MAX); return; }
    step("4 mask");
    if (!mask_pass(bscr, nbv, st.thr)) { say("4 mask: NO RENDER END"); return; }
    for (y = 0; y < C4_MASK_H; y++) {      /* the model, on the console's own map */
        psm_fault_beat(&crumbs);
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
    say("4 bands %u, vertices %u; holes inside %u, at an outline %u", nbands, nbv, holes_in, holes_out);
    say("4 mask vs model on this map: differ inside a face %u, at an outline %u", dif_in, dif_out);
    say("4 mask vs ray: lit %u shadow %u; acne %u edge %u rim %u; lit in error: in bias %u other %u", lit, shadow, acne, edge, rim, inbias, other);
}

static void t_scene(void) {
    t_scene_at(150.0f, 26.5f, 32u, 1u);
    t_scene_at(100.0f, 13.0f, 32u, 1u);
}

/* ---- 5. the cost ------------------------------------------------------------------------------------- */

#define REPEAT 8u

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
        c4_set_init(&st, 32u, gs, C4_B0, 0.75f);
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

    psm_fault_beat(&crumbs);
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
    return 1;
}

static void stop(const char *stage) {
    step(stage);
    vid_set_dithering(true);
    pvr_shutdown();
}

static void run(void) {
    say("numbers below: measured by this run");
    if (!start()) return;
    if (psm_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused");
    perf_cntr_start(PRFC1, PMCR_ELAPSED_TIME_MODE, PMCR_COUNT_CPU_CYCLES);
    t_ramp();
    t_window();
    t_bias();
    t_scene();
    t_cost();
    t_punch();
    t_dma();
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
        c4_set_init(&st, 32u, gs, b0, 0.75f);
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
    say("LIVE left at sun %u/%u, b0 x100 %u, bands %u wide, layer %s: %u vertices; SH-4 fit+cut+faces %u us, project %u us; PVR map %u mask %u screen %u us; frame %u us (it waits for the picture's turn)",
        (unsigned)az, (unsigned)el, (unsigned)(b0 * 100.0f), 1u << gs, over ? "on" : "off", nbv, us_of(cut), us_of(proj), us_of(pm), us_of(pk), us_of(ps), us_of(frame));
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
    c4_set_init(&st, 32u, 1u, C4_B0, 0.75f);
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

    run();
    vid_set_mode(DM_640x480, PM_RGB565);
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;
        int back = 0;

        psm_fault_beat(&crumbs);
        held = now;
        pages = (nlines + ROWS - 1u) / ROWS;
        if (!pages) pages = 1u;
        if (hit & CONT_A) { page = (page + 1u) % pages; redraw = 1; }
        else if (hit & CONT_B) { page = (page + pages - 1u) % pages; redraw = 1; }
        else if (hit & CONT_START) { live(); back = 1; }
        else if ((hit & CONT_Y) && !tiled) { tiled = 1; tiles(); back = 1; }
        if (back) {
            vid_set_mode(DM_640x480, PM_RGB565);
            pages = (nlines + ROWS - 1u) / ROWS;
            page = pages - 1u;
            held = ~0u;                    /* what is still held from the stage just left is not a press here */
            redraw = 1;
        }
        if (redraw) { page_draw(page, pages, tiled ? "START live" : "START live Y tiles"); redraw = 0; }
        thd_sleep(20);
    }
    return 0;
}
