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

_Static_assert(sizeof(pvr_vertex_t) == 32, "pvr_vertex_t is one 32-byte packet");
_Static_assert(sizeof(pvr_poly_hdr_t) == 32, "pvr_poly_hdr_t is one 32-byte packet");

static pvr_poly_hdr_t s_hdr[GFX_MAT_COUNT];
static sh4der_pl4nar_target_t s_rt;
static int            s_list = -1;          /* the open list (PVR_LIST_*), -1 none */
static uint32_t       s_bad_bind, s_frame;
static uint64_t       s_wait_us;
static uint32_t       s_rtt_us;

uint64_t gfx_pvr_wait_us(void) { const uint64_t w = s_wait_us; s_wait_us = 0; return w; }
uint32_t gfx_pvr_rtt_us(void) { return s_rtt_us; }
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
    /* GFX_MAT_REFLECT / _ADD are compiled by gfx_be_reflect_resize (they name the target) */
}

void gfx_be_shutdown(void) { pvr_shutdown(); }

void gfx_be_clear_colour(uint32_t argb) {                    /* hand: three byte scales */
    pvr_set_bg_color((float)((argb >> 16) & 255u) * (1.0f / 255.0f), (float)((argb >> 8) & 255u) * (1.0f / 255.0f), (float)(argb & 255u) * (1.0f / 255.0f));
}

int gfx_be_reflect_resize(unsigned w, unsigned h) {
    sh4der_pl4nar_target_free(&s_rt);            /* waits for the PVR to be done with the old target */
    if (!sh4der_pl4nar_target_init(&s_rt, w, h)) {
        printf("gfx: no video memory for a %u x %u reflection target\n", w, h);
        memset(&s_hdr[GFX_MAT_REFLECT], 0, sizeof s_hdr[0]);
        memset(&s_hdr[GFX_MAT_REFLECT_ADD], 0, sizeof s_hdr[0]);
        return -1;
    }
    sh4der_pl4nar_layer_header(&s_rt, &s_hdr[GFX_MAT_REFLECT], 0u);
    sh4der_pl4nar_layer_header(&s_rt, &s_hdr[GFX_MAT_REFLECT_ADD], 1u);
    return 0;
}
unsigned gfx_be_reflect_bytes(void) { return s_rt.bytes; }
float gfx_be_reflect_u_scale(void) { return s_rt.tex ? s_rt.u_scale : 1.0f; }

static void ready(void) {                        /* the wait for the PVR is not the frame's CPU time */
    const uint64_t t0 = timer_us_gettime64();
    pvr_wait_ready();
    s_wait_us += timer_us_gettime64() - t0;
}

int gfx_be_reflect_begin(void) {
    if (!s_rt.tex) return -1;
    ready();
    if (!sh4der_pl4nar_target_begin(&s_rt)) return -1;
    pvr_list_begin(PVR_LIST_OP_POLY);
    s_list = PVR_LIST_OP_POLY;
    return 0;
}

void gfx_be_reflect_end(void) {
    pvr_list_finish();
    s_list = -1;
    sh4der_pl4nar_target_end();
    if ((s_frame & 31u) == 0u) {                 /* one frame in 32: how long the PVR takes over the reflection pass */
        uint64_t t0 = timer_us_gettime64(), t1;
        pvr_wait_ready();                        /* its render has started */
        t1 = timer_us_gettime64();
        pvr_wait_render_done();
        s_rtt_us = (uint32_t)(timer_us_gettime64() - t1);
        s_wait_us += timer_us_gettime64() - t0;
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
    pvr_list_finish();
    s_list = -1;
    pvr_scene_finish();
}

static int s_bound_ok;

void gfx_be_bind(int mat) {
    const pvr_poly_hdr_t *h = &s_hdr[mat];
    /* a header of another list stalls the TA; a header never compiled (all zero) ends the list: neither is sent */
    s_bound_ok = sh4der_poly_hdr_ok((const uint32_t *)h) && (int)((h->cmd >> 24) & 7u) == s_list;
    if (s_bound_ok) packet(h);
    else s_bad_bind++;
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
