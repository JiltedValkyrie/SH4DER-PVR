/* SH4DER-PVR: d1, a halo that fades where it meets the scene, by a camera depth map the PVR draws; Dreamcast test

   demo/psm/ao/d1_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The idea (wiki/research/depth-shading-ideas.md, D1): a camera-facing glow cuts a hard line where it passes through
   a wall. If the scene's depth behind each pixel is known, the glow can fade out there. The PVR has no depth
   texture, so the depth is drawn: the scene once more from the camera into a small target through a ramp texture
   looked up by depth (d1_model.h), and that target read as a BUMP texture, whose unit subtracts one number a polygon
   (Q, the halo's own depth) from the texel and gives a level. The level goes into the frame's destination alpha and
   the halo is blended through it.

   THREE TESTS, each with its own result page and a picture held on the screen to photograph:
     T1  the destination-alpha layers alone, no depth map. A strip of the 256 codes as a bump texture, in five bands
         into a 320 x 240 target, read back:
           0  CLEAR, FADE (MODULATE, black, K1 0 K3 255), SHOW     1  the same with K1 128 K3 127
           2  CLEAR, FADE (MODULATEALPHA, black, K 0 / 255), SHOW  3  the level written as colour (REPLACE): the control
           4  CLEAR, FADE as band 0, NO show: the backdrop's colour must be untouched
         CLEAR is (1,1,1,0) with ZERO x DESTCOLOR (alpha to 0, colour kept); FADE adds (0,0,0,level) with ONE x ONE;
         SHOW is white with DESTALPHA x ZERO (alpha as grey). c1 ran CLEAR, an alpha-only add and DESTALPHA on a
         console (wiki PVR-040); what is NEW here is the bump level as that add, and K1 0 / K3 255.
         T3 uses the first of bands 0 and 2 that has the SHAPE the fade needs (full at code 64, rising to it, falling
         after it, nothing from code 128 on); with neither, T3's fade is not run. d1-a asked for a cosine to within 4
         and found none: on the console (suite s7, 2026-11-25) both bands read 252 at code 64, 172 at 96 (a cosine:
         180) and 0 from 128, so the layer was there and the rule was wrong. The page now prints band 0's curve.
     T2  the depth map. (a) A quad at one depth and a tilted quad through the ramp: every pixel's code against the
         depth the SH-4 computes for it. (b) The room from the camera: the codes at the floor's vertices.
     T3  the fade. One glow quad in the room's far corner, 0.4 m from each wall, straight ahead of the eye and so
         ACROSS the divider: the walls are 7 cm behind it at its sides and 61 cm at its middle (the eye has a long
         lens on it: the glow is about a third of the screen wide). One
         half the plain additive glow (the hard line), the other four quads: CLEAR, FADE (the map as bump texture,
         Q = the quad's depth, depth test on), FAR (alpha 255 from a quad D1_FADE behind, depth test on: without it
         the level comes round every D1_TURN), HALO (DESTALPHA x ONE). Stages: no glow; plain; the fade's alpha as
         grey; the fade; the fade with the halves swapped; the fade without FAR; the fade with the depth map's pass
         cut to its own 10 x 8 tiles (sh4der_tilecut.h; wiki PVR-156: the cut ran in ao4 on s7).
         d1-b (suite s8) ran the fade but showed nothing of it: its glow, 0.2 m from one wall beside the lamp, lay
         left of the divider (wiki PVR-173). Its cost was the depth map: 4.9 ms of PVR, 3.2 ms of SH-4 (PVR-172).

   Then the pages. START on the pages: T3 live. ANY BUTTON OR TRIGGER DURING THE RUN ends the run there and opens
   the live view at once (the pages after it say where the run stopped; nothing is measured while keys act).
     X  the fade / its alpha as grey     Y  swap the halves     A  the glow's distance from the walls: 0.2, 0.4, 0.6, 0.9 m
     B  the FAR quad on / off     d-pad down  the map's tile cut on / off     LT / RT  the divider
     d-pad left  the long lens on / off     d-pad up held + Y  the effect labels     stick  the camera

   The translucent list is sent in order (autosort off): the layers depend on it. Nothing is written but PVR
   registers and video memory; the frame buffer's write control is not touched (no 1555 threshold here). Waits are
   polled, none longer than LIMIT_NS; the fault catcher is in; the program goes back to the suite's menu.
   Build: python demo/psm/ao/build.py --spike d1 --tag <new tag>. NOT RUN ON A CONSOLE when this file was written:
   every step named NEW above is untested there, and the design (d1_model.h) is an ESTIMATE until it has. */

#include <kos.h>
#include <stdarg.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"
#include "../psm_suite.h"
#include "ao4_scene.h"
#include "ao4_draw.h"
#include "ao4_room.h"
#include "d1_model.h"
#include "sh4der_tilecut.h"
#include "sh4der_split.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#ifndef D1_TAG
#define D1_TAG untagged
#endif
#define D1_STR_(x) #x
#define D1_STR(x) D1_STR_(x)
#ifndef D1_AUTO_S
#define D1_AUTO_S 5u                        /* seconds a stage of T3, and a picture held on the screen */
#endif
#define CYC_PER_MS 200006u                  /* 200.006 MHz, MEASURED (bench s2) */

#define PG_PREFIX "d1"
#define PG_LINES_MAX 120u
#define PG_TOP 1u                           /* the effect labels of a page are two lines */
#include "../psm_page.h"
#include "../psm_effects.h"

static const fx_entry_t d1_fx[2] = {        /* the room's baked vertex AO (ao4_light); the fade: the SH-4 sends the scene again and four quads, the PVR draws the depth map and compares */
    { "Ambient Occlusion", FX_SH4, FX_BAKED, NULL }, { "Depth Fade", FX_BOTH, FX_REALTIME, "soft halo" },
};

#define say pg_say
static sh4der_fault_crumbs_t crumbs;
static sh4der_tilecut_t tcut;                     /* the tile tables, for the depth map's pass through its own tiles */

static void step(const char *name) {
    sh4der_fault_beat(&crumbs);
    sh4der_fault_stage(&crumbs, name);
    crumbs.frame++;
}

#define LIMIT_NS 200000000ull
static inline uint64_t ns(void) { const uint64_t n = timer_ns_gettime64(); return n - n / 1000000000ull * 2502400ull; }   /* KallistiOS's clock, its reload step taken out */
static inline uint64_t cyc(void) { return perf_cntr_count(PRFC1); }
static unsigned us_of(uint64_t cycles) { return (unsigned)((cycles * 1000ull + CYC_PER_MS / 2u) / CYC_PER_MS); }

static void wait_ns(uint64_t t) {
    const uint64_t t0 = ns();

    while (ns() - t0 < t) { }
}

/* ---- memory, headers ----------------------------------------------------------------- */

#define TW 512u                             /* the target as a texture */
#define TH 256u
#define MW 320u                             /* what is drawn of it: the depth map, half the screen each way */
#define MH 240u
#define RUN 256u
#define TEXT_W 512u
#define TEXT_H 128u
#define TEXT_LINES 6u
#define GLOW 32u

static pvr_ptr_t tgt_mem, ramp_mem, strip_mem, glow_mem, text_mem;
static pvr_poly_hdr_t h_flat, h_ramp, h_clear, h_fade_s[2], h_rep_s, h_show, h_tshow, h_text, h_dim;
static pvr_poly_hdr_t h_room, h_clear_c, h_fade_c[2], h_far_c, h_halo_c, h_plain_c, h_show_c, h_dummy;
static pvr_poly_hdr_t h_room_g, h_fade_g[2], h_far_g, h_halo_g, h_plain_g;   /* the same with no user clip: the halves cut by geometry */
static pvr_vertex_t vrun[RUN] __attribute__((aligned(32)));
static unsigned run_n, sent;
static uint32_t cur_oargb;
static uint16_t tgt_ram[TW * TH] __attribute__((aligned(32)));
static uint16_t tex16[256u * 8u] __attribute__((aligned(32)));
static uint16_t glow_px[GLOW * GLOW] __attribute__((aligned(32)));
static uint16_t text_px[TEXT_W * TEXT_H] __attribute__((aligned(32)));
static char text_ln[TEXT_LINES][65];
static int text_dirty;

static ao4_scene_t room;
static float scr[AO4_V_MAX][3];
static uint32_t col[AO4_V_MAX];

static void flush(void) {
    if (!run_n) return;
    pvr_prim(vrun, run_n * sizeof vrun[0]);
    run_n = 0;
}

static inline void put(int eol, float x, float y, float z, float u, float v, uint32_t argb) {
    pvr_vertex_t *o;

    if (run_n == RUN) flush();
    o = &vrun[run_n++];
    o->flags = eol ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    o->x = x; o->y = y; o->z = z; o->u = u; o->v = v; o->argb = argb; o->oargb = cur_oargb;
    sent++;
}

static void rect(float x0, float y0, float x1, float y1, float z, float u0, float v0, float u1, float v1, uint32_t argb) {
    put(0, x0, y1, z, u0, v1, argb);
    put(0, x0, y0, z, u0, v0, argb);
    put(0, x1, y1, z, u1, v1, argb);
    put(1, x1, y0, z, u1, v0, argb);
}

/* A header, then a rectangle through it. */
static void layer(const pvr_poly_hdr_t *h, float x0, float y0, float x1, float y1, float z, float u0, float v0, float u1, float v1, uint32_t argb, uint32_t oargb) {
    flush();
    pvr_prim((void *)(uintptr_t)h, sizeof *h);
    cur_oargb = oargb;
    rect(x0, y0, x1, y1, z, u0, v0, u1, v1, argb);
    flush();
    cur_oargb = 0u;
}

#define KQ(k1, k3, q) (((uint32_t)(k1) << 24) | ((uint32_t)(k3) << 8) | ((uint32_t)(q) & 255u))   /* the bump unit's K1, K2 = 0, K3 and Q: the offset colour */
#define BLACK_A 0xff000000u                 /* the FADE's and FAR's base colour: nothing for the colour, all for alpha */
#define CLEAR_C 0x00ffffffu                 /* the CLEAR's: the colour kept, alpha to 0 */

static void bump_cxt(pvr_poly_cxt_t *c, pvr_ptr_t mem, unsigned w, unsigned h, int env) {
    pvr_poly_cxt_txr(c, PVR_LIST_TR_POLY, PVR_TXRFMT_BUMP | PVR_TXRFMT_NONTWIDDLED, (int)w, (int)h, mem, PVR_FILTER_NEAREST);
    c->gen.culling = PVR_CULLING_NONE; c->gen.specular = 1;      /* the offset colour carries K1, K2, K3 and Q */
    c->txr.env = env; c->txr.alpha = PVR_TXRALPHA_ENABLE; c->txr.uv_clamp = PVR_UVCLAMP_UV;
    c->blend.src = PVR_BLEND_ONE; c->blend.dst = PVR_BLEND_ONE;  /* (0, 0, 0, level) added: alpha alone */
    c->depth.comparison = PVR_DEPTHCMP_ALWAYS; c->depth.write = PVR_DEPTHWRITE_DISABLE;
}

static void headers(void) {
    static const int env[2] = { PVR_TXRENV_MODULATE, PVR_TXRENV_MODULATEALPHA };
    pvr_poly_cxt_t c;
    unsigned k;

    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                      /* a backdrop */
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&h_flat, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, 256, 8, ramp_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE; c.txr.env = PVR_TXRENV_REPLACE; c.txr.uv_clamp = PVR_UVCLAMP_V;   /* the map pass: the ramp, u repeats (a turn a repeat) */
    pvr_poly_compile(&h_ramp, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                      /* CLEAR: dst x (1, 1, 1, 0) */
    c.gen.culling = PVR_CULLING_NONE;
    c.blend.src = PVR_BLEND_ZERO; c.blend.dst = PVR_BLEND_DESTCOLOR;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_clear, &c);
    c.gen.clip_mode = PVR_USERCLIP_INSIDE;
    pvr_poly_compile(&h_clear_c, &c);
    for (k = 0; k < 2u; k++) {                                   /* FADE: T1's strip, and T3's map inside the user clip with the depth test */
        bump_cxt(&c, strip_mem, 256u, 8u, env[k]);
        pvr_poly_compile(&h_fade_s[k], &c);
        bump_cxt(&c, tgt_mem, TW, TH, env[k]);
        c.depth.comparison = PVR_DEPTHCMP_GEQUAL;
        pvr_poly_compile(&h_fade_g[k], &c);
        c.gen.clip_mode = PVR_USERCLIP_INSIDE;
        pvr_poly_compile(&h_fade_c[k], &c);
    }
    bump_cxt(&c, strip_mem, 256u, 8u, PVR_TXRENV_REPLACE);       /* the control: the level as colour, as c4 wrote it */
    c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ZERO;
    pvr_poly_compile(&h_rep_s, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                      /* SHOW: white x destination alpha */
    c.gen.culling = PVR_CULLING_NONE;
    c.blend.src = PVR_BLEND_DESTALPHA; c.blend.dst = PVR_BLEND_ZERO;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_show, &c);
    c.gen.clip_mode = PVR_USERCLIP_INSIDE;
    pvr_poly_compile(&h_show_c, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                      /* FAR: (0, 0, 0, 255) added where the scene is behind it */
    c.gen.culling = PVR_CULLING_NONE;
    c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ONE;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_far_g, &c);
    c.gen.clip_mode = PVR_USERCLIP_INSIDE;
    pvr_poly_compile(&h_far_c, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)GLOW, (int)GLOW, glow_mem, PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE;
    c.txr.env = PVR_TXRENV_MODULATE; c.txr.uv_clamp = PVR_UVCLAMP_UV;
    c.blend.src = PVR_BLEND_DESTALPHA; c.blend.dst = PVR_BLEND_ONE;   /* HALO: the glow x destination alpha, added */
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_halo_g, &c);
    c.gen.clip_mode = PVR_USERCLIP_INSIDE;
    pvr_poly_compile(&h_halo_c, &c);
    c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ONE;    /* the plain glow: added where it is in front of the scene */
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    pvr_poly_compile(&h_plain_c, &c);
    c.gen.clip_mode = PVR_USERCLIP_DISABLE;
    pvr_poly_compile(&h_plain_g, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                      /* the room on the screen: once, or inside the user clip a half */
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&h_room_g, &c);
    c.gen.clip_mode = PVR_USERCLIP_INSIDE;
    pvr_poly_compile(&h_room, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                      /* the polygon before a change of clip in the translucent list */
    c.gen.culling = PVR_CULLING_NONE; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_dummy, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)TW, (int)TH, tgt_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE; c.txr.env = PVR_TXRENV_REPLACE;   /* the target shown on the screen */
    pvr_poly_compile(&h_tshow, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, (int)TEXT_W, (int)TEXT_H, text_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_text, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_dim, &c);
}

/* The user clip, with the polygon that takes its early change first (ao4_spike.c's sequence, CORE 18). */
static void user_clip(const pvr_poly_hdr_t *dummy, const sh4der_split_half_t *h) {
    static uint32_t cmd[8] __attribute__((aligned(32)));

    flush();
    pvr_prim((void *)(uintptr_t)dummy, sizeof *dummy);
    put(0, -64.0f, 8.0f, 0.001f, 0.0f, 0.0f, 0xff000000u);
    put(0, -64.0f, 40.0f, 0.001f, 0.0f, 0.0f, 0xff000000u);
    put(1, -32.0f, 8.0f, 0.001f, 0.0f, 0.0f, 0xff000000u);
    flush();
    cmd[0] = PVR_CMD_USERCLIP; cmd[1] = cmd[2] = cmd[3] = 0u;
    cmd[4] = h->tx0; cmd[5] = h->ty0; cmd[6] = h->tx1; cmd[7] = h->ty1;
    pvr_prim(cmd, sizeof cmd);
}

/* ---- the text ------------------------------------------------------------------------------- */

static void text_set(unsigned line, const char *fmt, ...) {
    char s[65];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s, sizeof s, fmt, ap);
    va_end(ap);
    if (strcmp(s, text_ln[line])) { strcpy(text_ln[line], s); text_dirty = 1; }
}

static void text_upload(void) {             /* call with no render running */
    unsigned l, i, x, y;

    if (!text_dirty) return;
    memset(text_px, 0, sizeof text_px);
    for (l = 0; l < TEXT_LINES; l++)
        for (i = 0; i < 64u && text_ln[l][i]; i++) {
            const unsigned c = (unsigned char)text_ln[l][i];
            const uint8_t (*gl)[PG_CW] = pg_glyph[c >= 32u && c < 127u ? c - 32u : 0u];

            for (y = 0; y < PG_CH; y++)
                for (x = 0; x < PG_CW; x++) text_px[(l * 16u + y) * TEXT_W + i * 8u + x] = (uint16_t)(((gl[y][x] >> 4) << 12) | 0x0fffu);
        }
    pvr_txr_load(text_px, text_mem, sizeof text_px);
    text_dirty = 0;
}

static fx_text_t fx_now;
static int fx_live, fx_hidden;

/* A text line at (x, y) on a dark strip; the effect lines' Realtime words in yellow. */
static void text_put(unsigned line, float x, float y) {
    const unsigned n = (unsigned)strlen(text_ln[line]);
    const float v0 = (float)(line * 16u) * (1.0f / (float)TEXT_H), v1 = (float)(line * 16u + 16u) * (1.0f / (float)TEXT_H);
    unsigned a = 0, c;

    if (!n) return;
    flush(); pvr_prim(&h_dim, sizeof h_dim);
    rect(x - 2.0f, y - 1.0f, x + (float)(n * 8u) + 2.0f, y + 17.0f, 10.0f, 0, 0, 0, 0, 0xa0000000u);
    flush(); pvr_prim(&h_text, sizeof h_text);
    for (c = 1; c <= n; c++) {
        const int ma = line >= 4u && fx_marked(&fx_now, line - 4u, a), mc = c < n && line >= 4u && fx_marked(&fx_now, line - 4u, c);

        if (c < n && ma == mc) continue;
        rect(x + (float)(a * 8u), y, x + (float)(c * 8u), y + 16.0f, 11.0f, (float)(a * 8u) * (1.0f / (float)TEXT_W), v0, (float)(c * 8u) * (1.0f / (float)TEXT_W), v1, ma ? 0xffffdc00u : 0xffffffffu);
        a = c;
    }
}

/* ---- scenes on the PVR --------------------------------------------------------------- */

static uint64_t last_render_ns;
static asic_evt_handler_entry_t rd_prev;
static volatile uint32_t rd_seen;
static int rd_hooked;

static void rd_evt(uint32_t code, void *data) {
    (void)data;
    if (rd_prev.hdl) rd_prev.hdl(code, rd_prev.data);
    rd_seen = 1u;
}

static int wait_ready(void) {
    const uint64_t t0 = ns();

    while (pvr_check_ready() < 0) if (ns() - t0 > LIMIT_NS) return 0;
    return 1;
}

/* to_target: the 320 x 242 target (two rows more than are read, as c4's passes). */
static int scene_open(int to_target) {
    if (!wait_ready()) return 0;
    if (to_target) return pvr_scene_begin_rtt(tgt_mem, MW, MH + 2u, TW) >= 0;
    pvr_scene_begin();
    return 1;
}

static int scene_close(void) {
    pvr_stats_t st;
    uint64_t t0;
    unsigned t;

    flush();
    rd_seen = 0u;
    pvr_scene_finish();
    if (!wait_ready()) return 0;
    t0 = ns();
    if (rd_hooked) {
        while (!rd_seen) if (ns() - t0 > LIMIT_NS) return 0;
    } else {
        for (t = 0; t < 20u && pvr_wait_render_done() < 0; t++) sh4der_fault_beat(&crumbs);
        if (t == 20u) return 0;
    }
    if (pvr_get_stats(&st) < 0) return 0;
    last_render_ns = st.rnd_last_time;
    sh4der_tilecut_find(&tcut);                   /* the table this render went through, until both are known (not while a cut is on) */
    return 1;
}

/* ---- the target to RAM: PVR-DMA, as c4's fetch (ran on a console: wiki BUS rows of c4) --------------------------- */

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

static int fetch_checked, fetch_direct;

/* The first `bytes` (a multiple of 32) of the target into tgt_ram. Its first use is checked against words read
   straight from video memory; if the DMA does not end or differs, the words are read one by one from then on. */
static void fetch(uint32_t bytes) {
    const volatile uint16_t *src = (const volatile uint16_t *)tgt_mem;
    uint32_t i;

    if (!fetch_direct) {
        const uint64_t t0 = ns();
        int ended;

        dcache_purge_range((uintptr_t)tgt_ram, bytes);
        SB_PDEN = 0;
        SB_ISTNRM = ISTNRM_PVR_DMA;
        SB_PDAPRO = 0x6702007fu;
        SB_PDSTAP = 0x04000000u | ((uint32_t)(uintptr_t)tgt_mem & 0x00ffffffu);
        SB_PDSTAR = (uint32_t)(uintptr_t)tgt_ram & 0x1fffffe0u;
        SB_PDLEN = bytes;
        SB_PDDIR = 1;
        SB_PDTSEL = 0;
        SB_PDEN = 1;
        SB_PDST = 1;
        do { wait_ns(250000u); ended = (SB_PDST & 1u) == 0u; } while (!ended && ns() - t0 < LIMIT_NS);
        if (!ended) SB_PDST = 0;
        SB_PDEN = 0;
        SB_ISTNRM = ISTNRM_PVR_DMA;
        if (ended && !fetch_checked) {
            fetch_checked = 1;
            for (i = 0; i < 256u; i++) { const uint32_t k = (i * 2654435761u) % (bytes / 2u); if (tgt_ram[k] != src[k]) ended = 0; }
            say("fetch: PVR-DMA of %u bytes %s words read from video memory", (unsigned)bytes, ended ? "matches 256" : "DIFFERS from");
        }
        if (ended) return;
        fetch_direct = 1;
        say("fetch: PVR-DMA not used from here; words read one by one");
    }
    for (i = 0; i < bytes / 2u; i++) tgt_ram[i] = src[i];
}

#define FETCH_BYTES (TW * 2u * (MH + 2u))   /* 247,808: a multiple of 32 */
#define AT(x, y) tgt_ram[(y) * TW + (x)]
#define G8(p) ((((p) >> 5) & 63u) << 2)     /* a 565 pixel's green as 0 .. 252: what a grey level reads back as, to 4 */

/* The pad during the run: 1 at a button or a trigger newly pressed (the stick does not count). The first call
   takes what is held as old, so the menu's A does not count. */
static int pad_stop;                        /* the pad ended the run: what is left of it is skipped and the live view opens */

static int pad_hit(void) {
    static unsigned held = ~0u;
    static int trig = 1;
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
    const unsigned now = st ? st->buttons : 0u;
    const int tr = st && (st->ltrig > 128 || st->rtrig > 128);
    const int hit = (now & ~held) != 0u || (tr && !trig);

    held = now; trig = tr;
    return hit;
}

/* The target held on the screen for D1_AUTO_S seconds with a line of text: the picture to photograph. */
static void show_target(const char *caption) {
    const uint64_t t0 = ns();

    text_set(0u, "%s", caption);
    text_set(1u, "MEASURING: any button = live view now");
    text_set(2u, ""); text_set(3u, ""); text_set(4u, ""); text_set(5u, "");
    text_upload();
    while (ns() - t0 < (uint64_t)D1_AUTO_S * 1000000000ull) {
        step("d1 show   ");
        if (pad_hit()) { pad_stop = 1; return; }
        if (!scene_open(0)) return;
        sent = 0;
        pvr_list_begin(PVR_LIST_OP_POLY);
        layer(&h_tshow, 0.0f, 0.0f, 640.0f, 480.0f, 1.0f, 0.0f, 0.0f, (float)MW / (float)TW, (float)MH / (float)TH, 0xffffffffu, 0u);
        pvr_list_finish();
        pvr_list_begin(PVR_LIST_TR_POLY);
        text_put(0u, 64.0f, 52.0f);
        text_put(1u, 64.0f, 412.0f);
        flush();
        pvr_list_finish();
        if (!scene_close()) return;
    }
}

/* ---- T1: the destination-alpha layers ------------------------------------------------------- */

#define T1_Q 64u                            /* the strip's Q: the level's peak sits at code 64 */
#define T1_X0 32u
#define T1_BANDS 5u
#define T1_Y(b) (16u + (b) * 44u)
#define BACKDROP 0xff808080u
static int fade_env = -1;                   /* the FADE that reads as the model says: 0 MODULATE, 1 MODULATEALPHA; -1 neither */

static void t1(void) {
    static const char *const name[T1_BANDS] = { "MODULATE K 0/255", "MODULATE K 128/127", "MODALPHA K 0/255", "REPLACE as colour", "no SHOW: backdrop" };
    static const uint8_t k1[T1_BANDS] = { 0, 128, 0, 0, 0 }, k3[T1_BANDS] = { 255, 127, 255, 255, 255 };
    unsigned b, i, match[T1_BANDS], curve[32];
    int shape[T1_BANDS];
    char line[80];

    memset(curve, 0, sizeof curve);
    say("T1 DESTINATION ALPHA, no depth map: 256 codes, Q %u, 5 bands", T1_Q);
    say(" CLEAR (1,1,1,0) ZEROxDESTCOLOR; FADE bump (0,0,0,level)");
    say(" ONExONE; SHOW white DESTALPHAxZERO. Model: K1+K3 cos, >= 0.");
    step("d1 t1     ");
    if (!scene_open(1)) { say("T1: NO READY"); return; }
    sent = 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    layer(&h_flat, 0.0f, 0.0f, (float)MW, (float)MH + 2.0f, 0.5f, 0, 0, 0, 0, BACKDROP, 0u);
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    for (b = 0; b < T1_BANDS; b++) {
        const float x0 = (float)T1_X0, x1 = (float)(T1_X0 + 256u), y0 = (float)T1_Y(b), y1 = y0 + 32.0f;

        if (b == 3u) { layer(&h_rep_s, x0, y0, x1, y1, 1.0f, 0.0f, 0.5f, 1.0f, 0.5f, 0xffffffffu, KQ(k1[b], k3[b], T1_Q)); continue; }
        layer(&h_clear, x0, y0, x1, y1, 1.0f, 0, 0, 0, 0, CLEAR_C, 0u);
        layer(&h_fade_s[b == 2u ? 1 : 0], x0, y0, x1, y1, 1.0f, 0.0f, 0.5f, 1.0f, 0.5f, BLACK_A, KQ(k1[b], k3[b], T1_Q));
        if (b != 4u) layer(&h_show, x0, y0, x1, y1, 1.0f, 0, 0, 0, 0, 0xffffffffu, 0u);
    }
    pvr_list_finish();
    if (!scene_close()) { say("T1: NO RENDER END"); return; }
    fetch(FETCH_BYTES);
    for (b = 0; b < T1_BANDS; b++) {
        unsigned lo = 255u, hi = 0u, kept = 0, prev = 0;
        const unsigned y = T1_Y(b) + 16u;

        match[b] = 0;
        shape[b] = G8(AT(T1_X0 + T1_Q, y)) >= 240u;         /* the shape the fade needs: full at the peak, */
        for (i = 0; i < 256u; i++) {
            const unsigned g = G8(AT(T1_X0 + i, y)), e = d1_level(k1[b], k3[b], (i - T1_Q) & 255u) & ~3u;

            if (g < lo) lo = g;
            if (g > hi) hi = g;
            match[b] += g + 4u >= e && g <= e + 4u;
            kept += AT(T1_X0 + i, y) == AT(4u, y);          /* the backdrop beside the strip */
            if (i >= 128u && g > 8u) shape[b] = 0;          /* nothing over the half turn behind it, */
            if (i && i <= T1_Q && g + 8u < prev) shape[b] = 0;               /* rising to the peak, */
            if (i > T1_Q && i <= 128u && g > prev + 8u) shape[b] = 0;        /* falling after it (a step of the 565 target's green allowed) */
            prev = g;
            if (b == 0u && (i & 7u) == 0u) curve[i >> 3] = g;
        }
        if (b == 4u) say("T1 %u %-18s backdrop kept on %u of 256 px", b, name[b], kept);
        else say("T1 %u %-18s model %u/256 %u..%u%s", b, name[b], match[b], lo, hi, b == 0u || b == 2u ? shape[b] ? " SHAPE OK" : " SHAPE NO" : "");
    }
    say("T1 band 0 as read, codes 0, 8 .. 248 (a cosine: 0 at 0, 252 at");
    say(" 64, 180 at 96, 0 from 128; the console's curve is its own):");
    for (b = 0; b < 2u; b++) {
        unsigned n = 0;

        for (i = 0; i < 16u; i++) n += (unsigned)snprintf(line + n, sizeof line - n, "%s%u", i ? " " : " ", curve[b * 16u + i]);
        say("%s", line);
    }
    fade_env = shape[0] ? 0 : shape[2] ? 1 : -1;
    say("T1 RESULT: the FADE for T3 is %s", fade_env == 0 ? "band 0, MODULATE" : fade_env == 1 ? "band 2, MODULATEALPHA" : "NONE: T3's fade is not run");
    show_target("T1: bands 0..4 top to bottom. PASS: 0 and 2 a black-white wave");
}

/* ---- T2: the depth map ---------------------------------------------------------------------- */

static void ramp_vertex(int eol, float x, float y, float depth) {
    put(eol, x, y, sh4der_recip_pos_(depth), d1_u(depth), 0.5f, 0xffffffffu);
}

static void t2(void) {
    const float da = 4.0f, d0 = 3.0f, d1 = 6.0f;
    unsigned x, same = 0, bins[4] = { 0, 0, 0, 0 }, i, seen = 0, off = 0;
    int worst = 0;
    ao4_cam_t cam;

    pg_break();
    say("T2 DEPTH MAP: the ramp looked up by depth, read back");
    step("d1 t2a    ");
    if (!scene_open(1)) { say("T2a: NO READY"); return; }
    sent = 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(&h_ramp, sizeof h_ramp);
    ramp_vertex(0, 0.0f, (float)MH + 2.0f, da); ramp_vertex(0, 0.0f, 0.0f, da); ramp_vertex(0, 160.0f, (float)MH + 2.0f, da); ramp_vertex(1, 160.0f, 0.0f, da);
    ramp_vertex(0, 160.0f, (float)MH + 2.0f, d0); ramp_vertex(0, 160.0f, 0.0f, d0); ramp_vertex(0, 320.0f, (float)MH + 2.0f, d1); ramp_vertex(1, 320.0f, 0.0f, d1);
    flush();
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);       /* this list has a bin: never left empty */
    user_clip(&h_dummy, &(sh4der_split_half_t){ 0u, 0u, 19u, 14u, 0 });
    pvr_list_finish();
    if (!scene_close()) { say("T2a: NO RENDER END"); return; }
    fetch(FETCH_BYTES);
    for (x = 0; x < 160u; x++) same += (AT(x, 120u) & 255u) == d1_code(da) && (AT(x, 120u) >> 8) == 0u;
    for (x = 160u; x < 320u; x++) {
        const int d = d1_diff(AT(x, 120u) & 255u, d1_code(d1_tilt_depth(160.0f, 320.0f, d0, d1, (float)x + 0.5f)));
        const unsigned a = (unsigned)(d < 0 ? -d : d);

        bins[a == 0u ? 0 : a == 1u ? 1 : a <= 3u ? 2 : 3]++;
        if ((int)a > (worst < 0 ? -worst : worst)) worst = d;
    }
    say("T2a flat quad at 4 m: %u of 160 px read code %u (high byte 0)", same, d1_code(da));
    say("T2a tilted 3..6 m (about 4 codes a px): off by 0: %u, 1: %u,", bins[0], bins[1]);
    say("    2-3: %u, more: %u of 160; worst %+d", bins[2], bins[3], worst);
    show_target("T2a: left one grey-blue; right fine stripes, even across");
    if (pad_stop) { say("T2b: not run (the pad ended the run)"); return; }

    step("d1 t2b    ");
    ao4_cam_at(&room, &cam, 0.0f, 0.0f, 0.0f);
    for (i = 0; i < room.vn; i++) ao4_cam_pt(&cam, room.p[i], scr[i]);
    if (!scene_open(1)) { say("T2b: NO READY"); return; }
    sent = 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(&h_ramp, sizeof h_ramp);
    {
        unsigned st, k;

        for (st = 0, k = 0; st < room.strips; k += room.slen[st], st++)
            for (i = 0; i < room.slen[st]; i++) {
                const float *s = scr[room.idx[k + i]];

                put(i + 1u == room.slen[st], s[0] * 0.5f, s[1] * 0.5f, s[2], sh4der_recip_pos_(s[2] * D1_TURN), 0.5f, 0xffffffffu);
            }
    }
    flush();
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    user_clip(&h_dummy, &(sh4der_split_half_t){ 0u, 0u, 19u, 14u, 0 });
    pvr_list_finish();
    if (!scene_close()) { say("T2b: NO RENDER END"); return; }
    fetch(FETCH_BYTES);
    memset(bins, 0, sizeof bins);
    for (i = 0; i < room.ground_vn; i++) {  /* the floor's vertices: the pixel each lands in against its own depth */
        const int px = (int)(scr[i][0] * 0.5f), py = (int)(scr[i][1] * 0.5f);
        int d;
        unsigned a;

        if (px < 1 || py < 1 || px >= (int)MW - 1 || py >= (int)MH - 1 || scr[i][2] > 1.5f) { off++; continue; }
        d = d1_diff(AT((unsigned)px, (unsigned)py) & 255u, d1_code(sh4der_recip_pos_(scr[i][2])));
        a = (unsigned)(d < 0 ? -d : d);
        bins[a <= 2u ? 0 : a <= 8u ? 1 : a <= 24u ? 2 : 3]++;
        seen++;
    }
    say("T2b the room, %u floor vertices on the map (%u off it):", seen, off);
    say("    code off by 0-2: %u, 3-8: %u, 9-24: %u, more: %u", bins[0], bins[1], bins[2], bins[3]);
    say("    (a vertex is not a pixel's middle, and the floor runs up");
    say("     to 11 codes a pixel; furniture hides some: 'more')");
    show_target("T2b: the room as depth codes: bands that follow floor and walls");
}

/* ---- T3: the fade -------------------------------------------------------------------------- */

typedef struct view {
    unsigned show;                          /* SHOW_ below: what the faded half draws */
    unsigned div, wall;                     /* the divider; the glow's distance from each wall of the corner: wall_m[] */
    int swap;                               /* the faded half is the left one */
    int far_on;
    int cut;                                /* the depth map's pass through its own tiles (sh4der_tilecut.h) */
    int zoom;                               /* the room's eye with a long lens aimed at the glow */
    int geo;                                /* 1: the halves cut at the divider by geometry, no user clip in the screen's lists; 0: a user clip a half (d1-c) */
    unsigned stage;                         /* the run's stage, for the text */
    float turn, lift, t;
} view_t;

enum { SHOW_NONE = 0, SHOW_PLAIN, SHOW_ALPHA, SHOW_FADE };
typedef struct t3_stage { uint8_t show, swap, far_on, cut, geo; const char *name; } t3_stage_t;
enum { T3_NONE = 0, T3_PLAIN, T3_PLAING, T3_ALPHA, T3_FADE, T3_SWAP, T3_NOFAR, T3_CUT, T3_CUTC, T3_STAGES };
static const t3_stage_t t3_stage[T3_STAGES] = {
    /* 0 and 1 as d1-c drew them (a user clip a half): 1 is the control for the flicker d1-c showed left of the divider.
       From 2 on the halves are cut by geometry and no user clip is sent */
    { SHOW_NONE, 0, 1, 0, 0, "no glow, clip" }, { SHOW_PLAIN, 0, 1, 0, 0, "plain, clip" }, { SHOW_PLAIN, 0, 1, 0, 1, "plain | plain" },
    { SHOW_ALPHA, 0, 1, 0, 1, "plain | alpha" }, { SHOW_FADE, 0, 1, 0, 1, "plain | fade" },
    { SHOW_FADE, 1, 1, 0, 1, "fade | plain" }, { SHOW_FADE, 0, 0, 0, 1, "plain | no FAR" }, { SHOW_FADE, 0, 1, 1, 1, "fade, map cut" },
    { SHOW_FADE, 0, 1, 2, 1, "fade, col cut" },                  /* the column cut: the map's 10 columns, every row: one word written a table */
};
static const float wall_m[4] = { 0.2f, 0.4f, 0.6f, 0.9f };
#define WALL_RUN 0u                         /* the run's: 0.2 m from each wall, so the glow's sides are inside the walls: the plain half is cut along them
                                               (d1-c ran at 0.4 m: the walls were behind all of the glow, the fade's band lay where the glow is dark already,
                                               and the two halves looked the same) */
#define GLOW_Y 2.1f                         /* the glow's middle: above the lamp, which stands in front of this corner */
#define GLOW_R 0.55f                        /* the glow quad's half side, metres: its sides reach the walls at 0.4 m */
#define GLOW_C 0xffffdc9eu                  /* its colour */
#define ZOOM_FOCAL 2400.0f                  /* pixels: from the room's eye, 12 m away, the glow is then about 215 pixels wide (58 at the room's 640) */
#define CUT_W (MW / 32u)                    /* the map's own tiles: 10 x 8 of the screen's 20 x 15 */
#define CUT_H ((MH + 2u + 31u) / 32u)

typedef struct cost { uint64_t frame, map_send, cut_begin, cut_end; uint64_t pvr_map, pvr_scr; unsigned verts, cut_on, cut_refused; int have_map; } cost_t;
static int cut_fatal;                       /* a tile table did not restore: the PVR is stopped */

static int frame(const view_t *v, cost_t *c) {
    const uint64_t c0 = cyc();
    const int fade = (v->show == SHOW_ALPHA || v->show == SHOW_FADE) && fade_env >= 0;
    const unsigned fside = v->swap ? 0u : 1u;                    /* the half that is faded */
    const float centre[3] = { -3.0f + wall_m[v->wall], GLOW_Y, -3.0f + wall_m[v->wall] };   /* in the corner of the walls x = -3 and z = -3: straight ahead of the room's eye */
    sh4der_split_half_t half[2];
    ao4_cam_t cam;
    float q[4][3], depth, zq, zfar;
    unsigned i, k, st, sd;

    memset(c, 0, sizeof *c);
    sh4der_split_halves(v->div, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &half[0], &half[1]);
    ao4_cam_at(&room, &cam, v->t, v->turn, v->lift);
    if (v->zoom) {                          /* the same eye, aimed at the glow (ao-lamp's near camera does the same for the bulb) */
        const float up[3] = { 0.0f, 1.0f, 0.0f };
        float r[3];

        cam.f[0] = centre[0] - cam.e[0]; cam.f[1] = centre[1] - cam.e[1]; cam.f[2] = centre[2] - cam.e[2];
        sh4der_norm3_(cam.f);
        sh4der_cross_(up, cam.f, r);
        sh4der_norm3_(r);
        sh4der_cross_(cam.f, r, cam.u);
        cam.r[0] = -r[0]; cam.r[1] = -r[1]; cam.r[2] = -r[2];   /* the room is a right-handed model (ao4_cam_at) */
        cam.focal = ZOOM_FOCAL;
    }
    for (i = 0; i < room.vn; i++) ao4_cam_pt(&cam, room.p[i], scr[i]);
    for (i = 0; i < 4u; i++) {              /* the quad's corners: the camera's own right and up, so every corner has the centre's depth */
        const float sx = (i & 1u) ? GLOW_R : -GLOW_R, sy = (i & 2u) ? GLOW_R : -GLOW_R;
        const float p[3] = { centre[0] + cam.r[0] * sx + cam.u[0] * sy, centre[1] + cam.r[1] * sx + cam.u[1] * sy, centre[2] + cam.r[2] * sx + cam.u[2] * sy };

        ao4_cam_pt(&cam, p, q[i]);
    }
    {
        const float d[3] = { centre[0] - cam.e[0], centre[1] - cam.e[1], centre[2] - cam.e[2] };

        depth = sh4der_dot3_(d, cam.f);
    }
    zq = sh4der_recip_pos_(depth); zfar = sh4der_recip_pos_(depth + D1_FADE);

    if (fade && !half[fside].hidden) {      /* the depth map: the room once more, through the ramp. No render is running: the last one was waited for */
        uint64_t t0;
        int did = 0, ok;

        if (v->cut) {                       /* the cut's own time, apart from the send's (d1-c had it inside: PVR-192) */
            const uint64_t tb = cyc();

            did = v->cut == 2 ? sh4der_tilecut_begin_cols(&tcut, CUT_W) : sh4der_tilecut_begin(&tcut, CUT_W, CUT_H);
            c->cut_begin = cyc() - tb;
            c->cut_on = (unsigned)did; c->cut_refused = (unsigned)!did;
        }
        t0 = cyc();
        ok = scene_open(1);
        if (ok) {
            sent = 0;
            pvr_list_begin(PVR_LIST_OP_POLY);
            pvr_prim(&h_ramp, sizeof h_ramp);
            for (st = 0, k = 0; st < room.strips; k += room.slen[st], st++)
                for (i = 0; i < room.slen[st]; i++) {
                    const float *s = scr[room.idx[k + i]];

                    put(i + 1u == room.slen[st], s[0] * 0.5f, s[1] * 0.5f, s[2], sh4der_recip_pos_(s[2] * D1_TURN), 0.5f, 0xffffffffu);
                }
            flush();
            pvr_list_finish();
            if (!v->geo) {
                pvr_list_begin(PVR_LIST_TR_POLY);
                user_clip(&h_dummy, &(sh4der_split_half_t){ 0u, 0u, 19u, 14u, 0 });
                pvr_list_finish();
            }
            c->map_send = cyc() - t0;
            ok = scene_close();
        }
        if (did) {                          /* the tables back before anything else is drawn */
            const uint64_t te = cyc();
            const unsigned bad = sh4der_tilecut_end(&tcut);

            c->cut_end = cyc() - te;
            if (bad) { cut_fatal = 1; return 0; }
        }
        if (!ok) return 0;
        c->pvr_map = last_render_ns; c->have_map = 1;
    }

    if (!scene_open(0)) return 0;
    sent = 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    for (sd = 0; sd < 2u; sd++) {
        if (v->geo) {                       /* the room is the same in both halves: once, no clip */
            if (sd) break;
            flush();
            pvr_prim(&h_room_g, sizeof h_room_g);
        } else {
            if (half[sd].hidden) continue;
            user_clip(&h_room, &half[sd]);
            pvr_prim(&h_room, sizeof h_room);
        }
        for (st = 0, k = 0; st < room.strips; k += room.slen[st], st++)
            for (i = 0; i < room.slen[st]; i++) {
                const float *s = scr[room.idx[k + i]];

                put(i + 1u == room.slen[st], s[0], s[1], s[2], 0.0f, 0.0f, col[room.idx[k + i]]);
            }
        flush();
    }
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    if (v->show != SHOW_NONE && q[0][2] < 1.5f && q[1][2] < 1.5f && q[2][2] < 1.5f && q[3][2] < 1.5f) {
        const float x0 = q[0][0] < q[3][0] ? q[0][0] : q[3][0], x1 = q[0][0] < q[3][0] ? q[3][0] : q[0][0];
        const float y0 = q[0][1] < q[3][1] ? q[0][1] : q[3][1], y1 = q[0][1] < q[3][1] ? q[3][1] : q[0][1];   /* a rectangle on the screen: the camera's right and up are the screen's */

        const float iw = sh4der_recip_pos_(x1 - x0 > 1.0e-3f ? x1 - x0 : 1.0e-3f);

        for (sd = 0; sd < 2u; sd++) {
            const int g = v->geo;
            float xa = x0, xb = x1, ua, ub;

            if (half[sd].hidden) continue;
            if (g) {                        /* this half's part of the rectangle: cut at the divider, the glow's u cut with it */
                const float dx = sh4der_split_x(v->div);

                if (sd == 0u) { if (xb > dx) xb = dx; } else if (xa < dx) xa = dx;
                if (!(xb > xa)) continue;
            } else
                user_clip(&h_dummy, &half[sd]);
            ua = (xa - x0) * iw; ub = (xb - x0) * iw;
            if (sd != fside || !fade) {     /* the plain glow: the depth test cuts it where it enters a wall */
                layer(g ? &h_plain_g : &h_plain_c, xa, y0, xb, y1, zq, ua, 0.0f, ub, 1.0f, GLOW_C, 0u);
                continue;
            }
            layer(g ? &h_clear : &h_clear_c, xa, y0, xb, y1, zq, 0, 0, 0, 0, CLEAR_C, 0u);
            layer(g ? &h_fade_g[fade_env] : &h_fade_c[fade_env], xa, y0, xb, y1, zq, xa * (0.5f / (float)TW), y0 * (0.5f / (float)TH), xb * (0.5f / (float)TW),
                  y1 * (0.5f / (float)TH), BLACK_A, KQ(0u, 255u, d1_q(depth)));
            if (v->far_on) layer(g ? &h_far_g : &h_far_c, xa, y0, xb, y1, zfar, 0, 0, 0, 0, BLACK_A, 0u);
            if (v->show == SHOW_ALPHA) layer(g ? &h_show : &h_show_c, xa, y0, xb, y1, zq, 0, 0, 0, 0, 0xffffffffu, 0u);
            else layer(g ? &h_halo_g : &h_halo_c, xa, y0, xb, y1, zq, ua, 0.0f, ub, 1.0f, GLOW_C, 0u);
        }
    }
    if (!sh4der_split_at_end(v->div, SH4DER_SPLIT_COLS)) {
        flush(); pvr_prim(&h_dim, sizeof h_dim);
        rect(sh4der_split_x(v->div) - 1.0f, 0.0f, sh4der_split_x(v->div) + 1.0f, 480.0f, 10.0f, 0, 0, 0, 0, 0xff101010u);
    }
    text_put(0u, 64.0f, 52.0f);
    text_put(1u, 64.0f, 392.0f);
    text_put(2u, 64.0f, 412.0f);
    if (fx_live && !fx_hidden) for (i = 0; i < fx_now.lines && i < 2u; i++) text_put(4u + i, 64.0f, 372.0f - 20.0f * (float)(fx_now.lines - 1u - i));
    flush();
    pvr_list_finish();
    c->verts = sent;
    if (!scene_close()) return 0;
    c->pvr_scr = last_render_ns;
    c->frame = cyc() - c0;
    return 1;
}

typedef struct sums { cost_t c; unsigned n, n_map; } sums_t;
static sums_t auto_sum[T3_STAGES];
static unsigned auto_done;

static void sums_add(sums_t *s, const cost_t *c) {
    s->c.frame += c->frame; s->c.pvr_scr += c->pvr_scr; s->c.verts = c->verts; s->n++;
    s->c.cut_on += c->cut_on; s->c.cut_refused += c->cut_refused;
    if (c->have_map) { s->c.map_send += c->map_send; s->c.pvr_map += c->pvr_map; s->c.cut_begin += c->cut_begin; s->c.cut_end += c->cut_end; s->n_map++; }
}

static void labels(const view_t *v, const sums_t *s) {
    static const char *const live[4] = { "no glow", "plain | plain", "plain | alpha", "plain | fade" };
    const unsigned n = s->n ? s->n : 1u, nm = s->n_map ? s->n_map : 1u;
    const int wants = v->show >= SHOW_ALPHA, fade = wants && fade_env >= 0;

    text_set(0u, "d1 %s%s%s  %u cm from the walls%s%s", fx_live ? live[v->show] : t3_stage[v->stage].name, fx_live && v->swap ? " SWAPPED" : "", wants && fade_env < 0 ? " (NO FADE: T1)" : "",
             (unsigned)(wall_m[v->wall] * 100.0f + 0.5f), v->far_on ? "" : " FAR OFF", !fx_live || !v->cut ? "" : v->cut == 2 ? " COL CUT" : " CUT");
    if (fx_live && !v->geo) text_set(0u, "d1 %s, USER CLIP halves  %u cm from the walls", live[v->show], (unsigned)(wall_m[v->wall] * 100.0f + 0.5f));
    text_set(1u, "frame %u us  PVR screen %u  map %u us  map send %u us", us_of(s->c.frame / n), (unsigned)(s->c.pvr_scr / n / 1000u), (unsigned)(s->c.pvr_map / nm / 1000u),
             us_of(s->c.map_send / nm));
    if (fx_live) text_set(2u, "X alph Y swap A wall B FAR dn cut lf zoom rt clip LT/RT START pg");
    else text_set(2u, "MEASURING stage %u of %u: any button = live view now", v->stage + 1u, (unsigned)T3_STAGES);
    fx_format(&fx_now, d1_fx, fade ? 2u : 1u);
    text_set(4u, "%s", fx_now.line[0]);
    text_set(5u, "%s", fx_now.line[1]);
}

static void stage_set(view_t *v, unsigned k) {
    v->stage = k; v->show = t3_stage[k].show; v->swap = t3_stage[k].swap; v->far_on = t3_stage[k].far_on; v->cut = t3_stage[k].cut;
    v->geo = t3_stage[k].geo;
}

/* T3's frames: the stages (auto_run 1), or the live view until START (0). */
static void loop(int auto_run) {
    view_t v;
    sums_t win;
    cost_t c;
    sh4der_split_step_t trig = { 0, 0u };
    unsigned held = ~0u, frames = 0;
    uint64_t t_prev = ns(), t_stage = ns();

    memset(&v, 0, sizeof v);
    v.div = sh4der_split_middle(SH4DER_SPLIT_COLS); v.wall = WALL_RUN; v.zoom = 1; v.geo = 1;
    stage_set(&v, auto_run ? T3_NONE : T3_FADE);
    fx_live = !auto_run;
    memset(&win, 0, sizeof win);
    labels(&v, &win);
    text_upload();
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;
        const uint64_t t_now = ns();
        const float dt = (float)(t_now - t_prev) * 1.0e-9f;
        int changed = 0;

        step("d1 t3     ");
        t_prev = t_now; held = now;
        if (auto_run) {
            if (pad_hit()) { pad_stop = 1; break; }         /* the stage in hand is not counted as done */
            if (t_now - t_stage > (uint64_t)D1_AUTO_S * 1000000000ull) {
                t_stage = t_now; auto_done = v.stage + 1u; changed = 1;
                if (v.stage + 1u >= T3_STAGES) break;
                stage_set(&v, v.stage + 1u);
            }
        } else {
            const unsigned d = st ? sh4der_split_step(&trig, v.div, SH4DER_SPLIT_COLS, st->ltrig > 128, st->rtrig > 128) : v.div;

            v.t += dt < 0.1f ? dt : 0.1f;
            if (st) {
                if (st->joyx > 24 || st->joyx < -24) v.turn += (float)st->joyx * (1.0f / 128.0f) * dt * 1.2f;
                if (st->joyy > 24 || st->joyy < -24) v.lift -= (float)st->joyy * (1.0f / 128.0f) * dt * 4.0f;
                if (v.lift < -4.5f) v.lift = -4.5f;
                if (v.lift > 7.0f) v.lift = 7.0f;
                if (v.turn > 0.6f) v.turn = 0.6f;
                if (v.turn < -0.6f) v.turn = -0.6f;
            }
            if (d != v.div) { v.div = d; changed = 1; }
            if ((hit & CONT_START) && frames > 10u) break;
            if (hit & CONT_X) { v.show = v.show == SHOW_FADE ? SHOW_ALPHA : SHOW_FADE; changed = 1; }
            if ((hit & CONT_Y) && (now & CONT_DPAD_UP)) fx_hide_chord(1, 1, &fx_hidden);
            else if (hit & CONT_Y) { v.swap = !v.swap; changed = 1; }
            if (hit & CONT_A) { v.wall = (v.wall + 1u) % 4u; changed = 1; }
            if (hit & CONT_B) { v.far_on = !v.far_on; changed = 1; }
            if (hit & CONT_DPAD_DOWN) { v.cut = (v.cut + 1) % 3; changed = 1; }   /* off, the map's own tiles, its columns */
            if (hit & CONT_DPAD_LEFT) { v.zoom = !v.zoom; changed = 1; }
            if (hit & CONT_DPAD_RIGHT) { v.geo = !v.geo; changed = 1; }       /* the halves by a user clip, as d1-c drew them, or by geometry */
        }
        if (changed) memset(&win, 0, sizeof win);
        if (!frame(&v, &c)) {
            if (cut_fatal) say("T3 TILE CUT: a tile table did NOT restore (frame %u): the PVR is stopped; its next start builds them anew", frames);
            else say("T3 %s: NO RENDER END at frame %u (%s)", auto_run ? "auto" : "live", frames, t3_stage[v.stage].name);
            break;
        }
        frames++;
        sums_add(&win, &c);
        if (auto_run && win.n > 2u) sums_add(&auto_sum[v.stage], &c);
        if (changed || win.n >= 30u) {
            labels(&v, &win);
            if (win.n >= 30u) memset(&win, 0, sizeof win);
            text_upload();                  /* the render is done: nothing reads the texture now */
        }
    }
}

static unsigned mean_us(uint64_t cycles, unsigned n) { return us_of(cycles / (n ? n : 1u)); }

static void t3_report(void) {
    pg_plot_t *g;
    unsigned i;

    pg_break();
    say("T3 THE FADE: one glow in the room's far corner, %u cm from", (unsigned)(wall_m[WALL_RUN] * 100.0f + 0.5f));
    say(" each wall, ACROSS the divider. fade %u cm, a turn %u cm,", (unsigned)(D1_FADE * 100.0f + 0.5f), (unsigned)(D1_TURN * 100.0f + 0.5f));
    say(" map %u x %u. At 20 cm the glow's sides are INSIDE the walls and", MW, MH);
    say(" the corner is about 30 cm behind its middle (geometry, not");
    say(" measured). The eye is the room's with a long lens on the");
    say(" glow: costs are this view's. Stages 0, 1: a user clip a half");
    say(" (as d1-c); 2 on: the halves cut by geometry, no user clip.");
    if (fade_env < 0) say(" T1 FOUND NO ALPHA-ONLY BUMP LAYER: the fade's stages drew the plain glow");
    if (auto_done < T3_STAGES && pad_stop) say(" RUN STOPPED BY THE PAD AT STAGE %u: stages %u.. were not measured", auto_done, auto_done);
    else if (auto_done < T3_STAGES) say(" THE RUN DID NOT FINISH: %u of %u stages", auto_done, (unsigned)T3_STAGES);
    say("stage            frame  PVR scr  PVR map  map send  verts");
    for (i = 0; i < auto_done; i++) {
        const sums_t *s = &auto_sum[i];
        const unsigned n = s->n ? s->n : 1u, nm = s->n_map ? s->n_map : 1u;

        say("%u %-14s %6u %7u %8u %9u %6u", i, t3_stage[i].name, us_of(s->c.frame / n), (unsigned)(s->c.pvr_scr / n / 1000u), (unsigned)(s->c.pvr_map / nm / 1000u), us_of(s->c.map_send / nm), s->c.verts);
    }
    if (auto_done > T3_FADE) {
        const sums_t *a = &auto_sum[T3_PLAING], *b = &auto_sum[T3_FADE];

        say("DERIVED the fade adds to a frame: %+d us (4 - 2)", (int)mean_us(b->c.frame, b->n) - (int)mean_us(a->c.frame, a->n));
        a = &auto_sum[T3_PLAIN]; b = &auto_sum[T3_PLAING];
        say("DERIVED no user clip, room sent once: %+d us a frame (2 - 1)", (int)mean_us(b->c.frame, b->n) - (int)mean_us(a->c.frame, a->n));
    }
    if (auto_done > T3_CUT) {
        const sums_t *b = &auto_sum[T3_FADE], *k = &auto_sum[T3_CUT];

        say("MAP PASS CUT to its %u x %u tiles of 20 x 15 (stage 7): passes", CUT_W, CUT_H);
        say(" cut %u, refused %u.", k->c.cut_on, k->c.cut_refused);
        say(" the cut's own SH-4 time a pass: begin %u us, end %u us", us_of(k->c.cut_begin / (k->n_map ? k->n_map : 1u)), us_of(k->c.cut_end / (k->n_map ? k->n_map : 1u)));
        if (k->c.cut_on)
            say("DERIVED the cut: PVR map %+d us, map send %+d us, frame %+d us (7 - 4)", (int)(k->c.pvr_map / (k->n_map ? k->n_map : 1u) / 1000u) - (int)(b->c.pvr_map / (b->n_map ? b->n_map : 1u) / 1000u),
                (int)us_of(k->c.map_send / (k->n_map ? k->n_map : 1u)) - (int)us_of(b->c.map_send / (b->n_map ? b->n_map : 1u)),
                (int)mean_us(k->c.frame, k->n) - (int)mean_us(b->c.frame, b->n));
        else say(" THE CUT DID NOT RUN: every pass refused (table not as read)");
    }
    if (auto_done > T3_CUTC) {
        const sums_t *b = &auto_sum[T3_FADE], *k = &auto_sum[T3_CUTC];
        const unsigned nm = k->n_map ? k->n_map : 1u, nb = b->n_map ? b->n_map : 1u;

        say("COLUMN CUT to the first %u columns, %u x 15 tiles (stage 8): cut", CUT_W, CUT_W);
        say(" %u, refused %u; begin %u us, end %u us a pass.", k->c.cut_on, k->c.cut_refused, us_of(k->c.cut_begin / nm), us_of(k->c.cut_end / nm));
        if (k->c.cut_on)
            say("DERIVED the column cut: PVR map %+d us, frame %+d us (8 - 4)", (int)(k->c.pvr_map / nm / 1000u) - (int)(b->c.pvr_map / nb / 1000u),
                (int)mean_us(k->c.frame, k->n) - (int)mean_us(b->c.frame, b->n));
    }
    say("us; one run, one console. The LOOK is not on this page:");
    say(" 1 against 2: is the left half steady without the user clip?");
    say(" 4: left the glow is cut along the walls, right it thins to");
    say(" them. 5: the halves swapped. 6 (no FAR): the level comes");
    say(" round where the scene is over 60 cm behind, as designed.");
    g = pg_plot("d1: a frame, us", "T3 stage", "us", 1);
    pg_cat(g, "none c"); pg_cat(g, "plain c"); pg_cat(g, "plain"); pg_cat(g, "alpha"); pg_cat(g, "fade"); pg_cat(g, "swap"); pg_cat(g, "no FAR"); pg_cat(g, "cut"); pg_cat(g, "col cut");
    pg_series(g, "frame"); for (i = 0; i < T3_STAGES; i++) pg_point(g, (float)i, i < auto_done ? (float)mean_us(auto_sum[i].c.frame, auto_sum[i].n) : 0.0f);
}

/* ---- start, stop --------------------------------------------------------------------------- */

static int start(void) {
    pvr_init_params_t init = pvr_default_params;
    unsigned x, y;

    init.opb_sizes[PVR_LIST_OP_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_OP_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_TR_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_PT_POLY] = PVR_BINSIZE_0;
    init.vertex_buf_size = 768 * 1024;
    init.autosort_disabled = 1;             /* the layers work only in the order sent */
    init.opb_overflow_count = 4;
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return 0; }
    sh4der_tilecut_init(&tcut, 20u, 15u);         /* the screen's tiles; the tables are found as scenes render */
    cut_fatal = 0;
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    tgt_mem = pvr_mem_malloc(TW * TH * 2u);
    ramp_mem = pvr_mem_malloc(sizeof tex16);
    strip_mem = pvr_mem_malloc(sizeof tex16);
    glow_mem = pvr_mem_malloc(sizeof glow_px);
    text_mem = pvr_mem_malloc(sizeof text_px);
    if (!tgt_mem || !ramp_mem || !strip_mem || !glow_mem || !text_mem) { say("video memory allocation FAIL"); pvr_shutdown(); return 0; }
    pvr_sq_set16(tgt_mem, 0u, TW * TH * 2u, PVR_DMA_VRAM64);
    for (y = 0; y < 8u; y++) for (x = 0; x < 256u; x++) tex16[y * 256u + x] = (uint16_t)x;     /* texel x is code x: the ramp, and T1's strip (S 0, R the code) */
    pvr_txr_load(tex16, ramp_mem, sizeof tex16);
    pvr_txr_load(tex16, strip_mem, sizeof tex16);
    for (y = 0; y < GLOW; y++)              /* the glow: (1 - r)^2, black at the rim so the quad's edge does not show */
        for (x = 0; x < GLOW; x++) {
            const float dx = ((float)x + 0.5f) * (2.0f / (float)GLOW) - 1.0f, dy = ((float)y + 0.5f) * (2.0f / (float)GLOW) - 1.0f;
            const float r = sh4der_sqrt_(dx * dx + dy * dy), f = r >= 1.0f ? 0.0f : (1.0f - r) * (1.0f - r);
            const unsigned l = (unsigned)(f * 255.0f);

            glow_px[y * GLOW + x] = (uint16_t)(((l >> 3) << 11) | ((l >> 2) << 5) | (l >> 3));
        }
    pvr_txr_load(glow_px, glow_mem, sizeof glow_px);
    headers();
    memset(text_ln, 0, sizeof text_ln);
    text_dirty = 1;
    text_upload();
    vid_set_dithering(false);               /* a code must reach the target as it is (wiki PVR-023) */
    rd_prev = asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, rd_evt, NULL);
    rd_hooked = rd_prev.hdl != NULL;
    if (!rd_hooked) asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, NULL, NULL);
    return 1;
}

static void stop(const char *stage) {
    step(stage);
    if (rd_hooked) { asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, rd_prev.hdl, rd_prev.data); rd_hooked = 0; }
    vid_set_dithering(true);
    pvr_shutdown();
}

int main(void) {
    unsigned page = 0, pages, held = ~0u, i;
    int redraw = 1, fault_on = 0;
    char rev[32];

    suite_init();                           /* the suite disc: the menu's binary into RAM, before anything else */
    vid_set_mode(DM_640x480, PM_RGB565);
    pg_init();
    perf_cntr_start(PRFC1, PMCR_ELAPSED_TIME_MODE, PMCR_COUNT_CPU_CYCLES);
    ao4_build_room(&room);
    ao4_light(&room, AO4_BAKED, AO4_CLAY, 0, col);
    for (i = 0; i < room.vn; i++) {         /* the room at half its light, so a glow shows on it */
        const uint32_t c = col[i];

        col[i] = 0xff000000u | ((c >> 1) & 0x007f7f7fu);
    }
    suite_rev_text(rev, sizeof rev);
    say("d1 %s: a halo that fades at the scene, by a PVR depth map.", D1_STR(D1_TAG));
    say("%s. Three tests, a page each; this run's numbers.", rev);
    say("Effect labels: off in the run, on in the live view.");
    if (start()) {
        fault_on = 1;
        if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused");
        t1();
        if (!pad_stop) t2(); else { pg_break(); say("T2: not run (the pad ended the run)"); }
        if (!pad_stop) loop(1);
        if (pad_stop) loop(0);              /* a button during the run: the live view at once, the pages after it */
        stop("auto end");
        t3_report();
    }
    (void)fault_on;
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
        else if (hit & CONT_START) {
            if (start()) { loop(0); stop("live end"); }
            vid_set_mode(DM_640x480, PM_RGB565);
            held = ~0u;
            redraw = 1;
        }
        else if (suite_tick(now, page, pages)) { page++; redraw = 1; }
        if (redraw) { pg_draw(page % pages, "D1 DEPTH FADE", suite_keys_text("START live view")); fx_page(d1_fx, 2u); redraw = 0; }
        thd_sleep(20);
    }
    return 0;
}
