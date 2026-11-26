/* gfx back end for the Dreamcast. KOS path: pvr_init (bins OP 32 / TR 32, 1 MB vertex buffer a scene, 4 overflow bins),
 * pvr_mem_malloc + pvr_txr_load (store queue) for the baked textures, pvr_poly_cxt_* + pvr_poly_compile headers,
 * pvr_scene_begin / pvr_scene_begin_rtt (through sh4der_pl4nar.c), pvr_list_begin, and direct rendering for every packet
 * (pvr_dr_target / eight word stores / pvr_dr_commit: the store queue straight into the tile accelerator).
 * Guards from SH4DER-PVR (sh4der_guard.h): a header that was never compiled is not sent (the TA would stop at it);
 * zero-area triangles are dropped in gfx.c before they get here. No maths here. */
#include <kos.h>
#include <dc/pvr.h>
#include <arch/timer.h>
#include "gfx.h"
#include "gfx_pvr.h"
#include "assets.h"
#include "sh4der_pl4nar.h"
#include "sh4der_guard.h"
#include "../../psm/ao/tile_cut.h"

_Static_assert(sizeof(pvr_vertex_t) == 32, "pvr_vertex_t is one 32-byte packet");
_Static_assert(sizeof(pvr_poly_hdr_t) == 32, "pvr_poly_hdr_t is one 32-byte packet");

static pvr_poly_hdr_t s_hdr[GFX_MAT_COUNT];
static pvr_poly_hdr_t s_hdr_clip[2];        /* GFX_MAT_REFLECT, _ADD with PVR_USERCLIP_INSIDE (header bits 17..16) */
static int            s_clip_on;
static sh4der_pl4nar_target_t s_rt;
static int            s_list = -1;          /* the open list (PVR_LIST_*), -1 none */
static uint32_t       s_bad_bind, s_frame;
static uint64_t       s_wait_us;
static uint32_t       s_rtt_us;
static int            s_sample;             /* this frame: 1 both scenes are waited for and read, 2 its last render is
                                               waited for only, so the next frame starts with the PVR idle (gfx_pvr_sample) */
/* Bounded waits (hero1's pattern, ../psm/hero/hero_spike.c): ready polled on pvr_check_ready, render done noted by this
   program's handler in front of KallistiOS's (the handler chains to it), both given up after GFX_WAIT_US and counted. */
#define GFX_WAIT_US 200000u
static asic_evt_handler_entry_t s_rd_prev;
static volatile uint32_t s_rd_seen;
static int            s_rd_hooked;
static uint32_t       s_timeouts;
/* The tile cut round the reflection pass (a stage). Its condition: no render reads the table while it is rewritten.
   So the pass waits for the screen's last render before the cut (that wait is the cut's price: the CPU no longer fills
   the mirror while the PVR draws the last screen), and the cut is undone after the target's render, before the screen
   scene is finished (its render reads the table; the TA, filling it, does not). The tables are taken after a render
   with no cut on. */
static tile_cut_t     s_cut;
static gfx_cut_stats_t s_cut_st;
static int            s_cut_dead;          /* a table did not read back: the cut is off for the run */
static int            s_screen_rendering;  /* a screen scene was finished and its render not waited for */
static unsigned       s_rt_w, s_rt_h;      /* the last target size, for a restart */
static gfx_pvr_pass_t s_pass[2];            /* the last sampled screen [0] and mirror [1] scene */

static void rd_evt(uint32_t code, void *data) { (void)data; if (s_rd_prev.hdl) s_rd_prev.hdl(code, s_rd_prev.data); s_rd_seen = 1u; }

static int wait_ready_(void) {
    const uint64_t t0 = timer_us_gettime64();
    while (pvr_check_ready() < 0) if (timer_us_gettime64() - t0 > GFX_WAIT_US) { s_timeouts++; return 0; }
    return 1;
}

/* After pvr_scene_finish (s_rd_seen cleared before it): the render of that scene is done. Without the hook, KallistiOS's
   own wait (unbounded). */
static int wait_done_(void) {
    const uint64_t t0 = timer_us_gettime64();
    if (!s_rd_hooked) { pvr_wait_render_done(); return 1; }
    while (!s_rd_seen) if (timer_us_gettime64() - t0 > GFX_WAIT_US) { s_timeouts++; return 0; }
    return 1;
}
uint32_t gfx_pvr_timeouts(void) { return s_timeouts; }

uint64_t gfx_pvr_wait_us(void) { const uint64_t w = s_wait_us; s_wait_us = 0; return w; }
uint32_t gfx_pvr_rtt_us(void) { return s_rtt_us; }
void gfx_pvr_sample(int on) { s_sample = on; }
const gfx_pvr_pass_t *gfx_pvr_pass(unsigned which) { return &s_pass[which & 1u]; }

/* A sampled scene, after pvr_scene_finish: wait for its render to start and to end, then read what KallistiOS kept of
   it. Its registration is done before its render starts (pvr_misc.c: vtx_buf_used is set at the REGDONE event), so
   vtx_buffer_used is this scene's alone; rnd_last_time is its render (RNDSTART .. RNDDONE). The waits are counted as
   waiting, not CPU. */
static void sample_scene(unsigned which) {
    gfx_pvr_pass_t *o = &s_pass[which];
    uint64_t t0 = timer_us_gettime64(), t1;
    pvr_stats_t ps;
    (void)wait_ready_();
    t1 = timer_us_gettime64();
    (void)wait_done_();
    o->wait_us = (uint32_t)(timer_us_gettime64() - t1);
    s_wait_us += timer_us_gettime64() - t0;
    if (pvr_get_stats(&ps) == 0) {
        o->ta_bytes = (uint32_t)ps.vtx_buffer_used;
        o->ta_max = (uint32_t)ps.vtx_buffer_used_max;
        o->rnd_us = (uint32_t)(ps.rnd_last_time / 1000u);
    }
    o->n++;
}
uint32_t gfx_pvr_bad_binds(void) { return s_bad_bind; }

static inline void packet(const void *pkt) {
    volatile uint32_t *d = (volatile uint32_t *)pvr_dr_target(NULL);
    const uint32_t *s = (const uint32_t *)pkt;
    d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
    d[4] = s[4]; d[5] = s[5]; d[6] = s[6]; d[7] = s[7];
    pvr_dr_commit((void *)d);
}

static pvr_ptr_t load(const uint16_t *words, uint32_t bytes) {
    pvr_ptr_t t = pvr_mem_malloc(bytes);
    if (t) pvr_txr_load(words, t, bytes);
    else printf("gfx: no video memory for a %lu B texture\n", (unsigned long)bytes);
    return t;
}

static void world_tex(int mat, const uint16_t *words, int size) {
    pvr_poly_cxt_t c;
    pvr_ptr_t t = load(words, tex_mip_bytes((uint32_t)size));
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_TWIDDLED, size, size, t, PVR_FILTER_BILINEAR);
    c.txr.mipmap = PVR_MIPMAP_ENABLE;
    c.txr.mipmap_bias = PVR_MIPBIAS_NORMAL;
    c.txr.env = PVR_TXRENV_MODULATE;
    c.gen.specular = PVR_SPECULAR_ENABLE;       /* offset colour: the distance haze */
    c.gen.culling = PVR_CULLING_NONE;           /* gfx.c culls by the signed area */
    pvr_poly_compile(&s_hdr[mat], &c);
}

/* A 64 x 64, 16-colour texture: 2,048 B, its colours in palette bank `bank` (16 entries from bank x 16). Point sampled,
   no mipmaps. KOS: pvr_set_pal_entry, PVR_TXRFMT_PAL4BPP | PVR_TXRFMT_4BPP_PAL(bank). */
static void pal_tex(int mat, const uint8_t *nibbles, const uint16_t *pal, unsigned bank) {
    pvr_poly_cxt_t c;
    pvr_ptr_t t = pvr_mem_malloc(TEX_P_SIZE * TEX_P_SIZE / 2u);
    unsigned i;
    if (!t) { printf("gfx: no video memory for a palette texture\n"); return; }     /* the header stays zero: gfx_be_bind refuses it */
    pvr_txr_load(nibbles, t, TEX_P_SIZE * TEX_P_SIZE / 2u);
    for (i = 0; i < 16u; i++) pvr_set_pal_entry(bank * 16u + i, pal[i]);
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_PAL4BPP | PVR_TXRFMT_4BPP_PAL(bank), TEX_P_SIZE, TEX_P_SIZE, t, PVR_FILTER_NEAREST);
    c.txr.env = PVR_TXRENV_MODULATE;
    c.gen.specular = PVR_SPECULAR_ENABLE;       /* offset colour: the distance haze */
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&s_hdr[mat], &c);
}

void gfx_be_init(void) {
    static const uint16_t *const fac[TEX_FACADE_N] = { tex_facade0, tex_facade1, tex_facade2, tex_facade3 };
    const pvr_init_params_t pp = {
        .opb_sizes = { PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_0 },
        .vertex_buf_size = 1024 * 1024,
        .opb_overflow_count = 4
    };
    pvr_poly_cxt_t c;
    pvr_ptr_t font;
    int i;
    pvr_init(&pp);
    s_rd_prev = asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, rd_evt, NULL);
    s_rd_hooked = s_rd_prev.hdl != NULL;
    if (!s_rd_hooked) asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, NULL, NULL);
    pvr_set_bg_color(0.77f, 0.81f, 0.84f);      /* the haze colour: what shows past the last building */
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&s_hdr[GFX_MAT_FLAT], &c);
    for (i = 0; i < TEX_FACADE_N; i++) world_tex(GFX_MAT_FACADE0 + i, fac[i], TEX_FACADE_SIZE);
    world_tex(GFX_MAT_ROAD, tex_road, TEX_ROAD_SIZE);
    {
        static const uint8_t *const pt[TEX_P_N] = { tex_p_facade0, tex_p_facade1, tex_p_facade2, tex_p_facade3, tex_p_road };
        static const uint16_t *const pp[TEX_P_N] = { pal_facade0, pal_facade1, pal_facade2, pal_facade3, pal_road };
        pvr_set_pal_format(PVR_PAL_RGB565);
        for (i = 0; i < TEX_P_N; i++) pal_tex(GFX_MAT_P_FACADE0 + i, pt[i], pp[i], (unsigned)i);
    }
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&s_hdr[GFX_MAT_HUD], &c);
    font = load(tex_font, TEX_FONT_SIZE * TEX_FONT_SIZE * 2u);
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, TEX_FONT_SIZE, TEX_FONT_SIZE, font, PVR_FILTER_NEAREST);
    c.txr.env = PVR_TXRENV_MODULATEALPHA;
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&s_hdr[GFX_MAT_FONT], &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                  /* light pools: vertex colour, added */
    c.gen.culling = PVR_CULLING_NONE;
    c.blend.src = PVR_BLEND_SRCALPHA; c.blend.dst = PVR_BLEND_ONE;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&s_hdr[GFX_MAT_ADD], &c);
    {   /* halos: a 32 x 32 glow (2 KB), texel x vertex colour, added; no alpha in the texture: black adds nothing */
        pvr_ptr_t glow = load(tex_glow, TEX_GLOW_SIZE * TEX_GLOW_SIZE * 2u);
        pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, TEX_GLOW_SIZE, TEX_GLOW_SIZE, glow, PVR_FILTER_BILINEAR);
        c.gen.culling = PVR_CULLING_NONE;
        c.txr.env = PVR_TXRENV_MODULATE;
        c.txr.uv_clamp = PVR_UVCLAMP_UV;
        c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ONE;
        c.depth.write = PVR_DEPTHWRITE_DISABLE;
        pvr_poly_compile(&s_hdr[GFX_MAT_GLOW], &c);
    }
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                  /* baked occlusion: vertex colour x the frame */
    c.gen.culling = PVR_CULLING_NONE;
    c.blend.src = PVR_BLEND_DESTCOLOR; c.blend.dst = PVR_BLEND_ZERO;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&s_hdr[GFX_MAT_MUL], &c);
#ifdef PL4NAR_WET
    {   /* the wet road's grit: 128 x 128 ARGB4444 with mips (43,696 B), texel x vertex colour, alpha blended, repeats */
        pvr_ptr_t grit = load(tex_grit, tex_mip_bytes(TEX_GRIT_SIZE));
        if (grit) {
            pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_TWIDDLED, TEX_GRIT_SIZE, TEX_GRIT_SIZE, grit, PVR_FILTER_BILINEAR);
            c.txr.mipmap = PVR_MIPMAP_ENABLE;
            c.txr.mipmap_bias = PVR_MIPBIAS_NORMAL;
            c.txr.env = PVR_TXRENV_MODULATEALPHA;
            c.gen.culling = PVR_CULLING_NONE;
            c.depth.write = PVR_DEPTHWRITE_DISABLE;
            pvr_poly_compile(&s_hdr[GFX_MAT_GRIT], &c);
        }
    }
#endif
    /* GFX_MAT_REFLECT / _ADD / _SHADOW are compiled by gfx_be_reflect_resize (they name the target) */
}

void gfx_be_shutdown(void) {
    if (s_rd_hooked) { asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, s_rd_prev.hdl, s_rd_prev.data); s_rd_hooked = 0; }
    pvr_shutdown();
}

void gfx_be_clear_colour(uint32_t argb) {                    /* hand: three byte scales */
    pvr_set_bg_color((float)((argb >> 16) & 255u) * (1.0f / 255.0f), (float)((argb >> 8) & 255u) * (1.0f / 255.0f), (float)(argb & 255u) * (1.0f / 255.0f));
}

int gfx_be_reflect_resize(unsigned w, unsigned h) {
    s_rt_w = w; s_rt_h = h;
    sh4der_pl4nar_target_free(&s_rt);            /* waits for the PVR to be done with the old target */
    if (!sh4der_pl4nar_target_init(&s_rt, w, h)) {
        printf("gfx: no video memory for a %u x %u reflection target\n", w, h);
        memset(&s_hdr[GFX_MAT_REFLECT], 0, sizeof s_hdr[0]);
        memset(&s_hdr[GFX_MAT_REFLECT_ADD], 0, sizeof s_hdr[0]);
        memset(&s_hdr[GFX_MAT_SHADOW], 0, sizeof s_hdr[0]);
        memset(&s_hdr[GFX_MAT_WET], 0, sizeof s_hdr[0]);
        return -1;
    }
    sh4der_pl4nar_layer_header(&s_rt, &s_hdr[GFX_MAT_REFLECT], 0u);
    sh4der_pl4nar_layer_header(&s_rt, &s_hdr[GFX_MAT_REFLECT_ADD], 1u);
    s_hdr_clip[0] = s_hdr[GFX_MAT_REFLECT]; s_hdr_clip[1] = s_hdr[GFX_MAT_REFLECT_ADD];
    s_hdr_clip[0].cmd = (s_hdr_clip[0].cmd & ~(3u << 16)) | ((uint32_t)PVR_USERCLIP_INSIDE << 16);
    s_hdr_clip[1].cmd = (s_hdr_clip[1].cmd & ~(3u << 16)) | ((uint32_t)PVR_USERCLIP_INSIDE << 16);
    {   /* the shadow tiles in the same target: texel x vertex colour, multiplied into the frame (DESTCOLOR x ZERO) */
        pvr_poly_cxt_t c;
        pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)s_rt.pitch, (int)s_rt.h, (pvr_ptr_t)s_rt.tex, PVR_FILTER_BILINEAR);
        c.gen.culling = PVR_CULLING_NONE;
        c.txr.alpha = PVR_TXRALPHA_DISABLE;
        c.txr.env = PVR_TXRENV_MODULATE;
        c.txr.uv_clamp = PVR_UVCLAMP_UV;
        c.blend.src = PVR_BLEND_DESTCOLOR; c.blend.dst = PVR_BLEND_ZERO;
        c.depth.write = PVR_DEPTHWRITE_DISABLE;
        pvr_poly_compile(&s_hdr[GFX_MAT_SHADOW], &c);
        /* the wet road: the target itself in the opaque list, texel x vertex colour (the reflection's share) + offset
           colour (the asphalt's own tone and the haze): one pass where the road was, no layer over it */
        pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)s_rt.pitch, (int)s_rt.h, (pvr_ptr_t)s_rt.tex, PVR_FILTER_BILINEAR);
        c.gen.culling = PVR_CULLING_NONE;
        c.gen.specular = PVR_SPECULAR_ENABLE;
        c.txr.env = PVR_TXRENV_MODULATE;
        c.txr.uv_clamp = PVR_UVCLAMP_UV;
        pvr_poly_compile(&s_hdr[GFX_MAT_WET], &c);
    }
    return 0;
}
unsigned gfx_be_reflect_bytes(void) { return s_rt.bytes; }
float gfx_be_reflect_u_scale(void) { return s_rt.tex ? s_rt.u_scale : 1.0f; }

static void ready(void) {                        /* the wait for the PVR is not the frame's CPU time */
    const uint64_t t0 = timer_us_gettime64();
    (void)wait_ready_();
    s_wait_us += timer_us_gettime64() - t0;
}

void gfx_be_tile_cut(unsigned cw, unsigned ch) { s_cut_st.cw = cw; s_cut_st.ch = ch; }
void gfx_be_tile_cut_stats(gfx_cut_stats_t *o) { *o = s_cut_st; o->tables = s_cut.n; }

/* Before the pass: the screen's last render done, then the tables (taken while no cut is on), then the cut. */
static void cut_begin_(void) {
    if (!s_cut_st.cw || !s_cut_st.ch) return;
    if (s_cut_dead || !s_rd_hooked) { s_cut_st.refused++; return; }
    if (s_screen_rendering) {
        const uint64_t t0 = timer_us_gettime64();
        const int ok = wait_done_();
        s_wait_us += timer_us_gettime64() - t0;
        if (!ok) { s_cut_st.refused++; return; }
        s_screen_rendering = 0;
    }
    if (!s_cut.tw) tile_cut_init(&s_cut, 20u, 15u);
    (void)tile_cut_find(&s_cut);
    if (tile_cut_begin(&s_cut, s_cut_st.cw, s_cut_st.ch)) s_cut_st.passes++;
    else s_cut_st.refused++;
}

/* After the target's render, before the screen scene is finished: the tables as they were. A table that does not read
   back would show the next screen as a corner of itself: the PVR is shut down and started again (pvr_init builds the
   tables anew; every texture and the target are loaded again), and the cut stays off. */
static void cut_end_(void) {
    unsigned bad;
    if (!s_cut.on) return;
    {
        const uint64_t t0 = timer_us_gettime64();
        (void)wait_done_();
        s_wait_us += timer_us_gettime64() - t0;
    }
    bad = tile_cut_end(&s_cut);
    if (!bad) return;
    s_cut_st.failed++; s_cut_st.bad_words += bad; s_cut_dead = 1;
    printf("gfx: TILE CUT FAILED (%u words): PVR restarted, cut off\n", bad);
    if (s_list >= 0) { pvr_list_finish(); s_list = -1; }
    gfx_be_shutdown();
    memset(&s_rt, 0, sizeof s_rt);
    tile_cut_init(&s_cut, 20u, 15u);
    gfx_be_init();
    (void)gfx_be_reflect_resize(s_rt_w, s_rt_h);
    s_screen_rendering = 0;
    pvr_scene_begin();                               /* a scene again, so the caller's translucent list and finish land */
    pvr_list_begin(PVR_LIST_OP_POLY);
    s_list = PVR_LIST_OP_POLY;
}

int gfx_be_reflect_begin(void) {
    if (!s_rt.tex) return -1;
    ready();
    cut_begin_();
    if (!sh4der_pl4nar_target_begin(&s_rt)) { if (s_cut.on) (void)tile_cut_end(&s_cut); return -1; }
    pvr_list_begin(PVR_LIST_OP_POLY);
    s_list = PVR_LIST_OP_POLY;
    return 0;
}

void gfx_be_reflect_end(void) {
    pvr_list_finish();
    s_list = -1;
    s_rd_seen = 0u;
    sh4der_pl4nar_target_end();
    if (s_sample == 1) {                              /* a sampled frame: how long the PVR takes over the reflection pass */
        sample_scene(1u);
        s_rtt_us = s_pass[1].wait_us;
    }
}

void gfx_be_frame_begin(void) {
    ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    s_list = PVR_LIST_OP_POLY;
    s_frame++;
}

void gfx_be_list_translucent(void) {
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    s_list = PVR_LIST_TR_POLY;
}

void gfx_be_frame_end(void) {
    cut_end_();
    pvr_list_finish();
    s_list = -1;
    s_rd_seen = 0u;
    pvr_scene_finish();
    s_screen_rendering = 1;
    if (s_sample == 1) sample_scene(0u);
    else if (s_sample == 2) {                    /* before a sampled frame: let this render end, so the next frame's
                                                    render-done is its own */
        const uint64_t t0 = timer_us_gettime64();
        (void)wait_ready_();
        (void)wait_done_();
        s_wait_us += timer_us_gettime64() - t0;
    }
}

static int s_bound_ok;

void gfx_be_bind(int mat) {
    const pvr_poly_hdr_t *h = s_clip_on && (mat == GFX_MAT_REFLECT || mat == GFX_MAT_REFLECT_ADD) ? &s_hdr_clip[mat == GFX_MAT_REFLECT_ADD] : &s_hdr[mat];
    /* a header of another list stalls the TA; a header never compiled (all zero) ends the list: neither is sent */
    s_bound_ok = sh4der_poly_hdr_ok((const uint32_t *)h) && (int)((h->cmd >> 24) & 7u) == s_list;
    if (s_bound_ok) packet(h);
    else s_bad_bind++;
}

/* The dummy (a triangle with area off the screen's left edge, through the untextured translucent header), then the clip
   command. Translucent list only; a clip outside the screen's tiles is refused and nothing is sent. */
void gfx_be_clip(const unsigned *tiles) {
    static const float dv[3][2] = { { -64.0f, 8.0f }, { -64.0f, 40.0f }, { -32.0f, 8.0f } };
    uint32_t cmd[8] __attribute__((aligned(32)));
    unsigned i;
    s_clip_on = 0;
    if (!tiles) return;
    if (s_list != PVR_LIST_TR_POLY || tiles[0] > tiles[2] || tiles[1] > tiles[3] || tiles[2] > 19u || tiles[3] > 14u) { s_bad_bind++; return; }
    packet(&s_hdr[GFX_MAT_HUD]);
    for (i = 0; i < 3u; i++) {
        pvr_vertex_t *o = (pvr_vertex_t *)pvr_dr_target(NULL);
        o->flags = i == 2u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        o->x = dv[i][0]; o->y = dv[i][1]; o->z = 0.001f; o->u = o->v = 0.0f; o->argb = 0xff000000u; o->oargb = 0u;
        pvr_dr_commit(o);
    }
    cmd[0] = PVR_CMD_USERCLIP; cmd[1] = cmd[2] = cmd[3] = 0u;
    cmd[4] = tiles[0]; cmd[5] = tiles[1]; cmd[6] = tiles[2]; cmd[7] = tiles[3];
    packet(cmd);
    s_bound_ok = 0;                                  /* the next triangle wants its own header */
    s_clip_on = 1;
}

void gfx_be_tri(const gfx_vert_t *a, const gfx_vert_t *b, const gfx_vert_t *c) {
    const gfx_vert_t *v[3] = { a, b, c };
    unsigned i;
    if (!s_bound_ok) return;
    for (i = 0; i < 3u; i++) {
        pvr_vertex_t *o = (pvr_vertex_t *)pvr_dr_target(NULL);
        o->flags = i == 2u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        o->x = v[i]->x; o->y = v[i]->y; o->z = v[i]->z;
        o->u = v[i]->u; o->v = v[i]->v;
        o->argb = v[i]->argb; o->oargb = v[i]->oargb;
        pvr_dr_commit(o);
    }
}
