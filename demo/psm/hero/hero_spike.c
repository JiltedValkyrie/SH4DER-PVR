/* SH4DER-PVR: hero1, a skinned hero running a street between space hangars, each part's cost, Dreamcast demo and test

   demo/psm/hero/hero_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.
   The street's art (hero_strip_data.h, a build product of tools/bake_strip.py): Space Kit (2.0) by Kenney
   (www.kenney.nl), CC0 1.0.

   One skinned figure (hero_model.h's own, or /cd/HERO.BIN: README.md) runs a straight road between hangars, end to end
   and back, turning about at each end (hero_strip.h). The camera stays in the street and follows him; the shadow
   target's window is centred on him every frame. His passes:

     OPAQUE  the base texture x the light a vertex (ambient + sun, the sun taken into each bone's space), opaque list
     DETAIL  the detail texture over the armour, 2 x multiply (DESTCOLOR x DESTCOLOR), translucent list, depth equal
     VISOR   a sphere map on the visor, added (ONE x ONE), translucent list, depth equal
     SHADOW  the hero slid along the sun onto the ground, drawn into a 256 x 256 target first; the ground reads it

   Three hens stand beside the road (HENS): each a flat blob under it, a fan in the translucent list, no target.

   A frame: where he is (strip_run), the clip (hero_pose), each bone's palette through his place, every vertex skinned
   (two matrices where a joint blends), the light, the set's objects in view through the camera (a triangle that
   crosses the near plane is cut there, not dropped), the sends. Every part is timed on the SH-4's clock; the PVR's
   render times and the TA's vertex buffer use come from pvr_get_stats after each scene.

   It runs by itself first (README.md "The autorun": hero_strip.h's stages; the hero and his shadow are on in every one
   of them), then pages and a graph. START on the pages: the live view.

     X  OPAQUE    D-pad left  DETAIL    B  VISOR    A  SHADOW    Y  he runs / he stands
     D-pad up  the view from the other end of the street     D-pad down  HENS     L / R triggers  zoom out / in     stick  orbit (kept in the street)
     D-pad right  the effect labels (hidden / shown)     START  the pages

   The effect labels (../psm_effects.h, the table in hero_fx.h): two lines over the keys in the live view, OFF in the
   measured autorun stages, on the result pages under the title.

   Waits are polled on the timer (the render-done interrupt noted by this program's handler in front of KallistiOS's), no
   longer than LIMIT_NS. The fault catcher (include/sh4der_fault.h) is in. Only a real console's run is evidence. */

#include <kos.h>
#include <stdarg.h>
#include <stdio.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"
#include "../psm_suite.h"
#include "hero_model.h"
#include "hero_file.h"
#include "hero_view.h"
#include "hero_strip.h"
#include "hero_hens.h"                    /* the hen: a build product (tools/hero_hens.py, from the town's mesh header) */

KOS_INIT_FLAGS(INIT_DEFAULT);

#ifndef HERO_TAG
#define HERO_TAG untagged
#endif
#define HERO_STR_(x) #x
#define HERO_STR(x) HERO_STR_(x)
#ifndef HERO_AUTO_S
#define HERO_AUTO_S 5u
#endif
#ifndef HERO_FILE_PATH
#define HERO_FILE_PATH "/cd/HERO.BIN"
#endif

#define PG_PREFIX "hero1"
#define PG_LINES_MAX 120u
#define PG_TOP 1u
#include "../../../include/sh4der_page.h"
#include "../../../include/sh4der_fxlabel.h"
#include "hero_fx.h"
#define say pg_say
static sh4der_fault_crumbs_t crumbs;

static void step(const char *name) { sh4der_fault_beat(&crumbs); sh4der_fault_stage(&crumbs, name); crumbs.frame++; }

#define LIMIT_NS 200000000ull
static inline uint64_t ns(void) { const uint64_t n = timer_ns_gettime64(); return n - n / 1000000000ull * 2502400ull; }   /* ao4: the TMU2 reload step out */
static unsigned us_of(uint64_t t) { return (unsigned)((t + 500u) / 1000u); }

/* ---- memory, headers, the vertex run --------------------------------------------------------------------------------- */

#define MAP 256u
#define MAP_ROWS (MAP + 8u)
#define RUN 256u
#define TEXT_W 512u
#define TEXT_H 128u
#define TEXT_LINES 6u                  /* 4, 5: the effect labels */
#define SHADOW_ARGB 0xff8c8c8cu

static pvr_ptr_t map_mem, text_mem, tex_mem[HERO_TEXS];
static pvr_poly_hdr_t h_flat, h_set, h_ground, h_base, h_detail, h_visor, h_text, h_dim, h_blob;
static pvr_vertex_t vrun[RUN] __attribute__((aligned(32)));
static unsigned run_n, sent;
static uint16_t text_px[TEXT_W * TEXT_H] __attribute__((aligned(32)));
static char text_ln[TEXT_LINES][65];
static int text_dirty;
static fx_text_t fx_t;                 /* the labels' text: lines 4, 5; its spans are drawn yellow */
static int fx_on, fx_hidden;          /* fx_on: the live view shows the labels; fx_hidden: D-pad right */

static void flush(void) { if (run_n) { pvr_prim(vrun, run_n * sizeof vrun[0]); run_n = 0; } }

static inline void put(int eol, float x, float y, float z, float u, float v, uint32_t argb) {
    pvr_vertex_t *o;

    if (run_n == RUN) flush();
    o = &vrun[run_n++];
    o->flags = eol ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    o->x = x; o->y = y; o->z = z; o->u = u; o->v = v; o->argb = argb; o->oargb = 0u;
    sent++;
}

static void rect(float x0, float y0, float x1, float y1, float z, float u0, float v0, float u1, float v1, uint32_t argb) {
    put(0, x0, y1, z, u0, v1, argb); put(0, x0, y0, z, u0, v0, argb); put(0, x1, y1, z, u1, v1, argb); put(1, x1, y0, z, u1, v0, argb);
}

static void hdr(const pvr_poly_hdr_t *h) { flush(); pvr_prim((void *)h, sizeof *h); }

static hero_t hero;

static void headers(void) {
    pvr_poly_cxt_t c;
    const int fmt[HERO_TEXS] = { hero.tex_fmt[0] ? PVR_TXRFMT_ARGB4444 : PVR_TXRFMT_RGB565, hero.tex_fmt[1] ? PVR_TXRFMT_ARGB4444 : PVR_TXRFMT_RGB565,
                                 hero.tex_fmt[2] ? PVR_TXRFMT_ARGB4444 : PVR_TXRFMT_RGB565 };

    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                       /* the shadow target: the clear, the casters, the border */
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&h_flat, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                       /* the set: a vertex colour */
    c.gen.culling = PVR_CULLING_NONE; c.gen.shading = PVR_SHADE_GOURAUD;
    pvr_poly_compile(&h_set, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)MAP, (int)MAP, map_mem, PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE; c.gen.shading = PVR_SHADE_GOURAUD;   /* the ground through the shadow target */
    c.txr.env = PVR_TXRENV_MODULATE; c.txr.uv_clamp = PVR_UVCLAMP_UV;
    pvr_poly_compile(&h_ground, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, fmt[0] | PVR_TXRFMT_TWIDDLED, (int)hero.tex_w[0], (int)hero.tex_h[0], tex_mem[0], PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE; c.gen.shading = PVR_SHADE_GOURAUD; c.txr.env = PVR_TXRENV_MODULATE;
    pvr_poly_compile(&h_base, &c);                                /* OPAQUE */
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, fmt[1] | PVR_TXRFMT_TWIDDLED, (int)hero.tex_w[1], (int)hero.tex_h[1], tex_mem[1], PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE; c.txr.env = PVR_TXRENV_MODULATE;
    c.blend.src = PVR_BLEND_DESTCOLOR; c.blend.dst = PVR_BLEND_DESTCOLOR;   /* source x frame + frame x source: 2 x multiply */
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_detail, &c);                              /* DETAIL */
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, fmt[2] | PVR_TXRFMT_TWIDDLED, (int)hero.tex_w[2], (int)hero.tex_h[2], tex_mem[2], PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE; c.txr.env = PVR_TXRENV_MODULATE; c.txr.uv_clamp = PVR_UVCLAMP_UV;
    c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ONE;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_visor, &c);                               /* VISOR */
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, (int)TEXT_W, (int)TEXT_H, text_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE; c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_text, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    c.gen.culling = PVR_CULLING_NONE; c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_dim, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                       /* a hen's blob: black by its vertex alpha, over the ground */
    c.gen.culling = PVR_CULLING_NONE; c.gen.shading = PVR_SHADE_GOURAUD;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_blob, &c);
}

/* ---- the text (ao4's: four lines of 64 characters in a texture) --------------------------------------------------- */

static void text_set(unsigned line, const char *fmt, ...) {
    char s[65];
    va_list ap;

    va_start(ap, fmt); vsnprintf(s, sizeof s, fmt, ap); va_end(ap);
    if (strcmp(s, text_ln[line])) { strcpy(text_ln[line], s); text_dirty = 1; }
}

static void text_upload(void) {
    unsigned l, i, x, y;

    if (!text_dirty) return;
    memset(text_px, 0, sizeof text_px);
    for (l = 0; l < TEXT_LINES; l++)
        for (i = 0; i < 64u && text_ln[l][i]; i++) {
            const unsigned ch = (unsigned char)text_ln[l][i];
            const uint8_t (*gl)[PG_CW] = pg_glyph[ch >= 32u && ch < 127u ? ch - 32u : 0u];

            for (y = 0; y < PG_CH; y++)
                for (x = 0; x < PG_CW; x++) text_px[(l * 16u + y) * TEXT_W + i * 8u + x] = (uint16_t)(((gl[y][x] >> 4) << 12) |
                    (l >= 4u && fx_marked(&fx_t, l - 4u, i) ? 0x0fd0u : 0x0fffu));   /* a Realtime span: yellow */
        }
    pvr_txr_load(text_px, text_mem, sizeof text_px);
    text_dirty = 0;
}

static void text_line(unsigned line, float y) {
    const float v0 = (float)(line * 16u) / (float)TEXT_H, v1 = (float)(line * 16u + 16u) / (float)TEXT_H;

    hdr(&h_dim); rect(14.0f, y - 1.0f, 14.0f + 512.0f + 4.0f, y + 17.0f, 10.0f, 0, 0, 0, 0, 0xa0000000u);
    hdr(&h_text); rect(16.0f, y, 16.0f + 512.0f, y + 16.0f, 11.0f, 0.0f, v0, 1.0f, v1, 0xffffffffu);
}

/* ---- scenes on the PVR (ao4's polled waits) ------------------------------------------------------------------------ */

static uint64_t last_render_ns;
static uint32_t last_vbuf;
static asic_evt_handler_entry_t rd_prev;
static volatile uint32_t rd_seen;
static int rd_hooked;

static void rd_evt(uint32_t code, void *data) { (void)data; if (rd_prev.hdl) rd_prev.hdl(code, rd_prev.data); rd_seen = 1u; }

static int wait_ready(void) {
    const uint64_t t0 = ns();

    while (pvr_check_ready() < 0) if (ns() - t0 > LIMIT_NS) return 0;
    return 1;
}

static int scene_open(int target) {
    if (!wait_ready()) return 0;
    if (target) return pvr_scene_begin_rtt(map_mem, MAP, MAP + 2u, MAP) >= 0;
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
    if (rd_hooked) { while (!rd_seen) if (ns() - t0 > LIMIT_NS) return 0; }
    else { for (t = 0; t < 20u && pvr_wait_render_done() < 0; t++) sh4der_fault_beat(&crumbs); if (t == 20u) return 0; }
    if (pvr_get_stats(&st) < 0) return 0;
    last_render_ns = st.rnd_last_time;
    last_vbuf = (uint32_t)st.vtx_buffer_used;
    return 1;
}

/* ---- the state ------------------------------------------------------------------------------------------------------ */

typedef struct view {
    int opq, det, vis, shd, hens, run, flip, autocam;  /* run: he runs the street (0: he stands at its start); flip: the live view from the other end */
    float yaw, pitch, dist;
    float t, run_t;                              /* seconds: the stage's (or the live view's) own, and of running */
} view_t;

typedef struct cost {
    uint64_t anim, pal, skin, light, op, det, vis, shd, env, hens, pvr_map, pvr_scr, frame;   /* ns */
    unsigned v_scr, v_map, tri_hero, tri_env, tri_cut, obj_out;
    uint32_t ta_map, ta_scr;
} cost_t;

static float pose[HERO_BONES][12], pal[HERO_BONES][16] __attribute__((aligned(8))), spal[HERO_BONES][16] __attribute__((aligned(8)));
static float lb[HERO_BONES][3], rb[HERO_BONES][3], ub[HERO_BONES][3];
static float hs[HERO_V_MAX][3], hsh[HERO_V_MAX][3], vuv[HERO_V_MAX][2];
static uint8_t hl[HERO_V_MAX];
static float sxw[STRIP_V][3], sscr[STRIP_V][3], suv[STRIP_V][2];   /* the set: X, Y, W; on screen; the ground's lookup */
static uint8_t sok[STRIP_V], oseen[STRIP_OBJS];
static float hen_scr[STRIP_HENS][PSM_TOWN_HEN_V][3], blob_scr[STRIP_HENS][STRIP_BLOB_N + 1u][3];
static uint8_t hen_ok[STRIP_HENS][PSM_TOWN_HEN_V], hen_seen[STRIP_HENS], blob_ok[STRIP_HENS];
static uint32_t hen_col[STRIP_HENS][PSM_TOWN_HEN_V];

static uint32_t grey_(float k) { const unsigned g = (unsigned)(sh4der_sat_(k) * 255.0f); return 0xff000000u | (g << 16) | (g << 8) | g; }

/* ---- load ---------------------------------------------------------------------------------------------------------- */

static int hero_load(void) {
    FILE *f = fopen(HERO_FILE_PATH, "rb");
    int r = HERO_E_IO;

    if (f) { r = hero_file_read(f, &hero); fclose(f); }
    if (r != HERO_E_OK) hero_build(&hero);
    return f ? r : 1;                           /* 1: no file (the built-in hero) */
}

static int start(void) {
    pvr_init_params_t init = pvr_default_params;
    unsigned t;

    init.opb_sizes[PVR_LIST_OP_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_OP_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_TR_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_PT_POLY] = PVR_BINSIZE_0;
    init.vertex_buf_size = 768 * 1024;
    init.autosort_disabled = 1;
    init.opb_overflow_count = 4;
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return 0; }
    pvr_set_bg_color(0.96f, 0.80f, 0.68f);                        /* a dusty sky over the rock */
    map_mem = pvr_mem_malloc(MAP * MAP_ROWS * 2u);
    text_mem = pvr_mem_malloc(sizeof text_px);
    for (t = 0; t < HERO_TEXS; t++) tex_mem[t] = pvr_mem_malloc(hero.tex_w[t] * hero.tex_h[t] * 2u);
    if (!map_mem || !text_mem || !tex_mem[0] || !tex_mem[1] || !tex_mem[2]) { say("video memory allocation FAIL"); pvr_shutdown(); return 0; }
    for (t = 0; t < HERO_TEXS; t++) pvr_txr_load_ex(hero.tex[t], tex_mem[t], hero.tex_w[t], hero.tex_h[t], PVR_TXRLOAD_16BPP);
    headers();
    memset(text_ln, 0, sizeof text_ln);
    text_dirty = 1;
    text_upload();
    vid_set_dithering(false);
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

/* ---- a frame ------------------------------------------------------------------------------------------------------- */

static void send_strips(unsigned s0, unsigned s1, const float (*scr)[3], int uv_kind, uint32_t argb) {   /* uv: 0 base, 1 detail, 2 sphere */
    unsigned s, i, k = 0;

    for (s = 0; s < s0; s++) k += hero.slen[s];
    for (s = s0; s < s1; s++) {
        const unsigned n = hero.slen[s];
        int ok = 1;

        for (i = 0; i < n; i++) if (scr[hero.idx[k + i]][2] == 0.0f) { ok = 0; break; }
        if (ok)
            for (i = 0; i < n; i++) {
                const unsigned v = hero.idx[k + i];
                const float u = uv_kind == 2 ? vuv[v][0] : uv_kind == 1 ? hero.uv[v][0] * HERO_DETAIL_SCALE : hero.uv[v][0];
                const float w = uv_kind == 2 ? vuv[v][1] : uv_kind == 1 ? hero.uv[v][1] * HERO_DETAIL_SCALE : hero.uv[v][1];

                put(i + 1u == n, scr[v][0], scr[v][1], scr[v][2], u, w, argb ? argb : grey_((float)hl[v] * (1.0f / 255.0f)));
            }
        k += n;
    }
}

/* The set's objects in view through the camera: X, Y, W a vertex, the screen place of those in front of the near plane,
   the ground's lookup into the shadow window round (0, hz). */
static void set_place(const hero_cam_t *cam, float hz, int uv, cost_t *c) {
    unsigned k, i;

#ifndef SH4DER_CORE_NO_SH4ZAM
    shz_xmtrx_load_4x4((const shz_mat4x4_t *)cam->vp);             /* strip_seen does not use XMTRX */
#endif
    for (k = 0; k < STRIP_OBJS; k++) {
        const strip_obj_t *o = &strip_obj[k];

        oseen[k] = (uint8_t)strip_seen(cam, o->c, o->r);
        if (!oseen[k]) { c->obj_out++; continue; }
        for (i = o->v0; i < (unsigned)o->v0 + o->vn; i++) {
#ifndef SH4DER_CORE_NO_SH4ZAM
            const shz_vec4_t q = shz_xmtrx_transform_vec4(shz_vec4_init(strip_v[i][0], strip_v[i][1], strip_v[i][2], 1.0f));

            sxw[i][0] = q.x; sxw[i][1] = q.y; sxw[i][2] = q.w;
#else
            float q[4];

            hero_xf_(cam->vp, strip_v[i], q);
            sxw[i][0] = q[0]; sxw[i][1] = q[1]; sxw[i][2] = q[3];
#endif
            sok[i] = sxw[i][2] >= HERO_NEAR;
            if (sok[i]) {
                const float iw = sh4der_recip_pos_(sxw[i][2]);

                sscr[i][0] = sxw[i][0] * iw; sscr[i][1] = sxw[i][1] * iw; sscr[i][2] = iw;
            }
        }
        if (o->floor && uv) for (i = o->v0; i < (unsigned)o->v0 + o->vn; i++) strip_ground_uv(strip_v[i], 0.0f, hz, suv[i]);
    }
}

/* The set's triangles: those facing the eye; one that crosses the near plane is cut there. */
static void set_send(int shd, cost_t *c) {
    const pvr_poly_hdr_t *cur = NULL;
    unsigned k, i, e;

    for (k = 0; k < STRIP_OBJS; k++) {
        const strip_obj_t *o = &strip_obj[k];
        const int tex = o->floor && shd;
        const pvr_poly_hdr_t *want = tex ? &h_ground : &h_set;

        if (!oseen[k]) continue;
        if (want != cur) { hdr(want); cur = want; }
        for (i = o->t0; i < (unsigned)o->t0 + o->tn; i++) {
            const unsigned q[3] = { (unsigned)o->v0 + strip_t[i][0], (unsigned)o->v0 + strip_t[i][1], (unsigned)o->v0 + strip_t[i][2] };
            const unsigned in = (unsigned)sok[q[0]] + sok[q[1]] + sok[q[2]];

            if (in == 3u) {
                if (hero_area2(sscr[q[0]], sscr[q[1]], sscr[q[2]]) >= 0.0f) continue;
                for (e = 0; e < 3u; e++) put(e == 2u, sscr[q[e]][0], sscr[q[e]][1], sscr[q[e]][2], tex ? suv[q[e]][0] : 0.0f, tex ? suv[q[e]][1] : 0.0f, strip_c[q[e]]);
                c->tri_env++;
            } else if (in) {
                static const uint8_t order[4] = { 0, 1, 3, 2 };   /* a quad as a strip */
                strip_cv_t a[3], b[4];
                float s[4][3];
                unsigned n;

                for (e = 0; e < 3u; e++) {
                    a[e].x = sxw[q[e]][0]; a[e].y = sxw[q[e]][1]; a[e].w = sxw[q[e]][2];
                    a[e].u = tex ? suv[q[e]][0] : 0.0f; a[e].v = tex ? suv[q[e]][1] : 0.0f; a[e].argb = strip_c[q[e]];
                }
                n = strip_clip(a, HERO_NEAR, b);
                if (n < 3u) continue;
                for (e = 0; e < n; e++) { const float iw = sh4der_recip_pos_(b[e].w); s[e][0] = b[e].x * iw; s[e][1] = b[e].y * iw; s[e][2] = iw; }
                if (hero_area2(s[0], s[1], s[2]) >= 0.0f) continue;
                for (e = 0; e < n; e++) {
                    const unsigned j = n == 4u ? order[e] : e;

                    put(e + 1u == n, s[j][0], s[j][1], s[j][2], b[j].u, b[j].v, b[j].argb);
                }
                c->tri_env += n - 2u; c->tri_cut++;
            }
        }
    }
}

/* The hens: each one's place at t, its vertices on screen with their light, its blob's points. */
static void hen_place(const hero_cam_t *cam, float t) {
    unsigned h, i;

    for (h = 0; h < STRIP_HENS; h++) {
        const float cs[3] = { strip_hen_home[h][0], STRIP_HEN_CY, strip_hen_home[h][1] };
        float m[16] __attribute__((aligned(8))), vpm[16] __attribute__((aligned(8))), lr[3], bearing, tilt, b[STRIP_BLOB_N + 1u][3];

        hen_seen[h] = (uint8_t)strip_seen(cam, cs, STRIP_HEN_R);
        if (!hen_seen[h]) continue;
        strip_hen_pose(h, t, &bearing, &tilt);
        strip_hen_place(cs[0], cs[2], bearing, tilt, m);
        lr[0] = sh4der_dot3_(strip_sun, &m[0]); lr[1] = sh4der_dot3_(strip_sun, &m[4]); lr[2] = sh4der_dot3_(strip_sun, &m[8]);   /* the sun in the hen's axes */
        strip_mul(vpm, cam->vp, m);
#ifndef SH4DER_CORE_NO_SH4ZAM
        shz_xmtrx_load_4x4((const shz_mat4x4_t *)vpm);
#endif
        for (i = 0; i < PSM_TOWN_HEN_V; i++) {
            const float d = ((float)psm_town_hen_n[i][0] * lr[0] + (float)psm_town_hen_n[i][1] * lr[1] + (float)psm_town_hen_n[i][2] * lr[2]) * (1.0f / 127.0f);
            const float *col = hero_hen_pal[psm_town_hen_c[i] - HERO_HEN_PAL0], k = STRIP_AMB + STRIP_SUNK * (d > 0.0f ? d : 0.0f);
#ifndef SH4DER_CORE_NO_SH4ZAM
            const shz_vec4_t q = shz_xmtrx_transform_vec4(shz_vec4_init(psm_town_hen_v[i][0], psm_town_hen_v[i][1], psm_town_hen_v[i][2], 1.0f));
            const float qx = q.x, qy = q.y, qw = q.w;
#else
            float q[4], qx, qy, qw;

            hero_xf_(vpm, psm_town_hen_v[i], q); qx = q[0]; qy = q[1]; qw = q[3];
#endif
            hen_ok[h][i] = qw >= HERO_NEAR;
            if (hen_ok[h][i]) { const float iw = sh4der_recip_pos_(qw); hen_scr[h][i][0] = qx * iw; hen_scr[h][i][1] = qy * iw; hen_scr[h][i][2] = iw; }
            hen_col[h][i] = 0xff000000u | ((unsigned)(sh4der_sat_(col[0] * k) * 255.0f) << 16) | ((unsigned)(sh4der_sat_(col[1] * k) * 255.0f) << 8) | (unsigned)(sh4der_sat_(col[2] * k) * 255.0f);
        }
        strip_hen_blob(cs[0], cs[2], strip_sun, b);
        blob_ok[h] = 1;
        for (i = 0; i <= STRIP_BLOB_N; i++) if (!hero_view_pt(cam->vp, b[i], blob_scr[h][i])) blob_ok[h] = 0;
    }
}

static unsigned hen_send(void) {                           /* opaque list, the set's header; the triangles sent */
    unsigned h, s, i, k, n = 0;

    for (h = 0; h < STRIP_HENS; h++) {
        if (!hen_seen[h]) continue;
        for (s = 0, k = 0; s < PSM_TOWN_HEN_S; k += psm_town_hen_sl[s], s++) {
            int ok = 1;

            for (i = 0; i < psm_town_hen_sl[s]; i++) if (!hen_ok[h][psm_town_hen_sv[k + i]]) ok = 0;
            if (!ok) continue;
            for (i = 0; i < psm_town_hen_sl[s]; i++) {
                const unsigned q = psm_town_hen_sv[k + i];

                put(i + 1u == psm_town_hen_sl[s], hen_scr[h][q][0], hen_scr[h][q][1], hen_scr[h][q][2], 0, 0, hen_col[h][q]);
            }
            n += psm_town_hen_sl[s] - 2u;
        }
    }
    return n;
}

static void blob_send(void) {                              /* translucent list, first: under everything else in it */
    unsigned h, k;
    int any = 0;

    for (h = 0; h < STRIP_HENS; h++) {
        if (!hen_seen[h] || !blob_ok[h]) continue;
        if (!any) { hdr(&h_blob); any = 1; }
        for (k = 0; k < STRIP_BLOB_N; k++) {
            const float *a = blob_scr[h][0], *b = blob_scr[h][k + 1u], *d = blob_scr[h][(k + 1u) % STRIP_BLOB_N + 1u];

            if (sh4der_abs_(hero_area2(a, b, d)) < 0.02f) continue;   /* no screen area: the PVR would draw it as a line */
            put(0, a[0], a[1], a[2], 0, 0, (uint32_t)STRIP_BLOB_A << 24);
            put(0, b[0], b[1], b[2], 0, 0, 0x00000000u);
            put(1, d[0], d[1], d[2], 0, 0, 0x00000000u);
        }
    }
}

static int frame(view_t *v, cost_t *c) {
    const uint64_t tf = ns();
    float place[16] __attribute__((aligned(8))), vph[16] __attribute__((aligned(8))), smc[16] __attribute__((aligned(8))), smh[16] __attribute__((aligned(8)));
    float hz = STRIP_RUN, hd = STRIP_PI, ct = v->t, d3[3];
    unsigned clip = HERO_IDLE;
    hero_cam_t cam;
    uint64_t t0;

    memset(c, 0, sizeof *c);
    if (v->run) strip_run(v->run_t, &hz, &hd, &clip, &ct);
    if (v->autocam) strip_auto_cam(&cam, v->t, v->dist, hz);
    else {
        const float target[3] = { 0.0f, 1.0f, hz };

        hero_cam_make(&cam, v->yaw + (v->flip ? STRIP_PI : 0.0f), v->pitch, v->dist, target);
    }
    t0 = ns();                                             /* anim: the clip, his place, the light and the camera into each bone */
    hero_pose(&hero, clip, ct, pose);
    strip_unturn(hd, strip_sun, d3); hero_bone_dirs((const float (*)[12])pose, d3, lb);
    if (v->vis) {
        strip_unturn(hd, cam.right, d3); hero_bone_dirs((const float (*)[12])pose, d3, rb);
        strip_unturn(hd, cam.up, d3); hero_bone_dirs((const float (*)[12])pose, d3, ub);
    }
    strip_place(hd, 0.0f, hz, place);
    strip_mul(vph, cam.vp, place);
    c->anim = ns() - t0;
    t0 = ns(); hero_palette(vph, (const float (*)[12])pose, pal); c->pal = ns() - t0;
    t0 = ns(); hero_skin(&hero, (const float (*)[16])pal, 1, HERO_NEAR, hs); c->skin = ns() - t0;
    t0 = ns(); hero_light(&hero, (const float (*)[3])lb, STRIP_AMB, STRIP_SUNK, hl); c->light = ns() - t0;
    c->tri_hero = hero.tris;
    t0 = ns(); set_place(&cam, hz, v->shd, c); c->env = ns() - t0;
    if (v->hens) { t0 = ns(); hen_place(&cam, v->t); c->hens = ns() - t0; }

    if (v->shd) {                                          /* SHADOW: the target first, its window round him */
        t0 = ns();
        strip_shadow_matrix(strip_sun, (float)MAP, 0.0f, hz, smc);
        strip_mul(smh, smc, place);
        hero_palette(smh, (const float (*)[12])pose, spal);
        hero_skin(&hero, (const float (*)[16])spal, 0, 0.5f, hsh);
        c->shd = ns() - t0;
        if (!scene_open(1)) return 0;
        pvr_list_begin(PVR_LIST_OP_POLY);
        hdr(&h_flat);
        rect(0.0f, 0.0f, (float)MAP, (float)MAP + 2.0f, 0.1f, 0, 0, 0, 0, 0xffffffffu);
        t0 = ns(); sent = 0;
        send_strips(0u, hero.strips, (const float (*)[3])hsh, 0, SHADOW_ARGB);
        flush();
        c->shd += ns() - t0; c->v_map = sent;
        rect(0.0f, 0.0f, (float)MAP, 1.0f, 2.0f, 0, 0, 0, 0, 0xffffffffu);
        rect(0.0f, (float)MAP - 1.0f, (float)MAP, (float)MAP + 2.0f, 2.0f, 0, 0, 0, 0, 0xffffffffu);
        rect(0.0f, 0.0f, 1.0f, (float)MAP, 2.0f, 0, 0, 0, 0, 0xffffffffu);
        rect((float)MAP - 1.0f, 0.0f, (float)MAP, (float)MAP, 2.0f, 0, 0, 0, 0, 0xffffffffu);
        flush();
        pvr_list_finish();
        if (!scene_close()) return 0;
        c->pvr_map = last_render_ns; c->ta_map = last_vbuf;
    }
    if (v->vis) hero_sphere_uv(&hero, (const float (*)[3])rb, (const float (*)[3])ub, hero.visor_strip0, hero.strips, vuv);

    if (!scene_open(0)) return 0;
    sent = 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    t0 = ns(); set_send(v->shd, c); flush(); c->env += ns() - t0;
    if (v->hens) { t0 = ns(); hdr(&h_set); c->tri_env += hen_send(); flush(); c->hens += ns() - t0; }
    if (v->opq) { t0 = ns(); hdr(&h_base); send_strips(0u, hero.strips, (const float (*)[3])hs, 0, 0u); flush(); c->op = ns() - t0; }
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    if (v->hens) { t0 = ns(); blob_send(); flush(); c->hens += ns() - t0; }
    if (v->opq && v->det) { t0 = ns(); hdr(&h_detail); send_strips(0u, hero.visor_strip0, (const float (*)[3])hs, 1, 0xffffffffu); flush(); c->det = ns() - t0; }
    if (v->opq && v->vis) { t0 = ns(); hdr(&h_visor); send_strips(hero.visor_strip0, hero.strips, (const float (*)[3])hs, 2, 0xff909090u); flush(); c->vis = ns() - t0; }
    c->v_scr = sent;
    text_line(0u, 16.0f); text_line(1u, 34.0f); text_line(2u, 52.0f); text_line(3u, 446.0f);
    if (fx_on && !fx_hidden && fx_t.lines > 0u) { text_line(4u, 408.0f); if (fx_t.lines > 1u) text_line(5u, 426.0f); }
    flush();
    pvr_list_finish();
    if (!scene_close()) return 0;
    c->pvr_scr = last_render_ns; c->ta_scr = last_vbuf;
    c->frame = ns() - tf;
    return 1;
}

static uint64_t sh4_of(const cost_t *c) { return c->anim + c->pal + c->skin + c->light + c->op + c->det + c->vis + c->shd + c->env + c->hens; }
static const char *on_(int x) { return x ? "on" : "--"; }
static int file_state;

static void labels(const view_t *v, const cost_t *c) {
    const unsigned f = us_of(c->frame), s = us_of(sh4_of(c)), p = us_of(c->pvr_map + c->pvr_scr), a = us_of(c->anim + c->pal + c->skin + c->light);

    text_set(0u, "%s %s OPQ %s DET %s VIS %s SHD %s HENS %s%s", file_state == 0 ? "FILE" : "BUILT-IN", v->run ? "RUN" : "STAND",
             on_(v->opq), on_(v->det), on_(v->vis), on_(v->shd), on_(v->hens), v->flip ? " (far end)" : "");
    text_set(1u, "frame %u.%u SH4 %u.%u PVR %u.%u ms  anim+skin %u.%u ms", f / 1000u, f % 1000u / 100u, s / 1000u, s % 1000u / 100u,
             p / 1000u, p % 1000u / 100u, a / 1000u, a % 1000u / 100u);
    text_set(2u, "TA %u v %uK  hero %u tri  set %u tri %u cut  %u.%u m", c->v_scr + c->v_map, (unsigned)((c->ta_scr + c->ta_map + 512u) / 1024u),
             c->tri_hero, c->tri_env, c->tri_cut, (unsigned)v->dist, (unsigned)(v->dist * 10.0f) % 10u);
    text_set(3u, "X opq <det B vis A shd Y run ^far end vhens >fx LT/RT zoom ST pgs");
    {
        fx_entry_t e[HERO_FX_MAX];
        const unsigned n = hero_fx_live(v->opq, v->det, v->vis, v->shd, 0, e);

        memset(&fx_t, 0, sizeof fx_t);
        if (n) fx_format(&fx_t, e, n);
        text_set(4u, "%s", fx_t.line[0]); text_set(5u, "%s", fx_t.line[1]);
    }
}

/* ---- the loop, the autorun ---------------------------------------------------------------------------------------- */

#define STAGES STRIP_STAGES
#define stages strip_stage                 /* hero_strip.h: the host test replays the same table */

typedef struct sums { cost_t c; unsigned n; } sums_t;
static sums_t auto_sum[STAGES];
static unsigned auto_done;

static void sums_add(sums_t *s, const cost_t *c) {
    s->c.anim += c->anim; s->c.pal += c->pal; s->c.skin += c->skin; s->c.light += c->light; s->c.op += c->op; s->c.det += c->det;
    s->c.vis += c->vis; s->c.shd += c->shd; s->c.env += c->env; s->c.hens += c->hens; s->c.pvr_map += c->pvr_map; s->c.pvr_scr += c->pvr_scr;
    s->c.frame += c->frame; s->c.v_scr += c->v_scr; s->c.v_map += c->v_map; s->c.ta_map += c->ta_map; s->c.ta_scr += c->ta_scr;
    s->c.tri_env += c->tri_env; s->c.tri_cut += c->tri_cut; s->c.obj_out += c->obj_out;
    s->c.tri_hero = c->tri_hero;
    s->n++;
}

static void sums_mean(const sums_t *s, cost_t *m) {
    const unsigned n = s->n ? s->n : 1u;

    *m = s->c;
    m->anim /= n; m->pal /= n; m->skin /= n; m->light /= n; m->op /= n; m->det /= n; m->vis /= n; m->shd /= n; m->env /= n; m->hens /= n;
    m->pvr_map /= n; m->pvr_scr /= n; m->frame /= n; m->v_scr /= n; m->v_map /= n; m->ta_map /= n; m->ta_scr /= n;
    m->tri_env /= n; m->tri_cut /= n; m->obj_out /= n;
}

/* A stage starts the run and the camera's swing again, so stages 3 to 5 see what stage 2's first seconds saw: ref_sum
   is stage 2 over the same seconds as they are counted, and a difference from it is like for like. */
#define REF_STAGE 1u
static sums_t ref_sum;

static void view_stage(view_t *v, unsigned k) {
    const strip_stage_t *s = &stages[k];

    v->opq = 1; v->shd = 1; v->hens = 1; v->det = s->det; v->vis = s->vis; v->run = s->run; v->dist = s->dist; v->t = 0.0f; v->run_t = 0.0f;
}

static void loop(int auto_run) {
    view_t v;
    sums_t win;
    cost_t c, shown;
    unsigned held = ~0u, frames = 0, stage = 0;
    uint64_t t_prev = ns(), t_stage = ns();

    memset(&v, 0, sizeof v);
    view_stage(&v, 1u);
    v.pitch = STRIP_AUTO_PITCH; v.yaw = 0.3f;
    v.autocam = auto_run;
    if (auto_run) { view_stage(&v, 0u); memset(auto_sum, 0, sizeof auto_sum); memset(&ref_sum, 0, sizeof ref_sum); auto_done = 0; }
    fx_on = !auto_run;                                      /* the labels: live view only */
    memset(&win, 0, sizeof win); memset(&shown, 0, sizeof shown);
    labels(&v, &shown); text_upload();
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;
        const uint64_t t_now = ns();
        float dt = (float)(t_now - t_prev) * 1.0e-9f;
        int changed = 0;

        step("hero frame ");
        t_prev = t_now; held = now;
        if (dt > 0.1f) dt = 0.1f;
        v.t += dt;
        if (v.run) v.run_t += dt;
        if (auto_run) {
            if (t_now - t_stage > (uint64_t)(stages[stage].secs * 1.0e9f)) {
                t_stage = t_now; changed = 1;
                if (++stage >= STAGES) { auto_done = stage; break; }
                view_stage(&v, stage);
            }
        } else {
            if ((hit & CONT_START) && frames > 10u) break;
            if (hit & CONT_X) { v.opq = !v.opq; changed = 1; }
            if (hit & CONT_DPAD_LEFT) { v.det = !v.det; changed = 1; }
            if (hit & CONT_B) { v.vis = !v.vis; changed = 1; }
            if (hit & CONT_A) { v.shd = !v.shd; changed = 1; }
            if (hit & CONT_Y) { v.run = !v.run; changed = 1; }
            if (hit & CONT_DPAD_UP) { v.flip = !v.flip; changed = 1; }
            if (hit & CONT_DPAD_DOWN) { v.hens = !v.hens; changed = 1; }
            if (fx_hide_chord(1, (hit & CONT_DPAD_RIGHT) != 0u, &fx_hidden)) changed = 1;   /* D-pad right alone: free here */
            if (st) {
                if (st->joyx > 24 || st->joyx < -24) v.yaw += (float)st->joyx * (1.0f / 128.0f) * dt * 1.6f;
                if (st->joyy > 24 || st->joyy < -24) v.pitch += (float)st->joyy * (1.0f / 128.0f) * dt * 1.0f;
                v.pitch = sh4der_clamp_(v.pitch, 0.05f, 1.25f);
                v.dist += ((float)st->ltrig - (float)st->rtrig) * (1.0f / 255.0f) * dt * 3.0f;   /* L out, R in */
                v.dist = sh4der_clamp_(v.dist, 1.6f, 8.0f);
            }
            {                                               /* the eye stays in the street */
                float sp, cp, ym;

                sh4der_sincos_(v.pitch, &sp, &cp);
                ym = strip_yaw_max(v.dist, cp);
                v.yaw = sh4der_clamp_(v.yaw, -ym, ym);
            }
        }
        if (changed) memset(&win, 0, sizeof win);
        if (!frame(&v, &c)) { say("%s: NO RENDER END at frame %u (stage %u)", auto_run ? "auto" : "live", frames, stage); break; }
        frames++;
        sums_add(&win, &c);
        if (auto_run && t_now - t_stage > 500000000ull) {       /* the first half second of a stage is not counted */
            sums_add(&auto_sum[stage], &c);
            if (stage == REF_STAGE && t_now - t_stage <= (uint64_t)(stages[2].secs * 1.0e9f)) sums_add(&ref_sum, &c);
        }
        if (changed || win.n >= 30u) {
            if (win.n >= 30u || !shown.frame) sums_mean(&win, &shown);
            if (win.n >= 30u) memset(&win, 0, sizeof win);
            labels(&v, &shown);
            text_upload();
        }
    }
}

/* ---- the pages ----------------------------------------------------------------------------------------------------- */

static float ms_(uint64_t t) { return (float)us_of(t) * 1.0e-3f; }

static void report(void) {
    cost_t m[STAGES], r;
    pg_plot_t *g;
    char rev[32];
    unsigned k;
    float sh4[5], pvr[5];

    suite_rev_text(rev, sizeof rev);
    say("hero1 %s: a skinned hero runs a street, each part's cost. This run's numbers.", HERO_STR(HERO_TAG));
    say("%s. the first 0.5 s of a stage not counted; ms unless said", rev);
    say("effect labels: OFF in the autorun stages, ON in the live view (D-pad right hides them)");
    say("hero: %s, %u vertices, %u triangles, %u strips, 33 bones, clips %u+%u frames", file_state == 0 ? "HERO.BIN" : "built-in (original)",
        hero.vn, hero.tris, hero.strips, hero.frames[0], hero.frames[1]);
    if (file_state < 0) say("HERO.BIN refused: %s; the built-in hero ran", hero_file_error(file_state));
    say("set: the space stretch, %u triangles in %u objects; hero and shadow ON in every stage", STRIP_T, STRIP_OBJS);
    say("hens: %u beside the road in every stage, %u triangles each, a flat blob of %u under each", STRIP_HENS, PSM_TOWN_HEN_T, STRIP_BLOB_N);
    if (auto_done < STAGES) say("THE RUN DID NOT FINISH: the lines below are of what ran");
    for (k = 0; k < STAGES; k++) {
        sums_mean(&auto_sum[k], &m[k]);
        say("%u %-15s fr %5.2f SH4 %5.2f PVR %5.2f TA %4uv %3uK n%u", k + 1u, stages[k].name, ms_(m[k].frame), ms_(sh4_of(&m[k])),
            ms_(m[k].pvr_map + m[k].pvr_scr), m[k].v_scr + m[k].v_map, (unsigned)((m[k].ta_scr + m[k].ta_map + 512u) / 1024u), auto_sum[k].n);
    }
    say("stage 2 SH-4 parts: anim %.2f palette %.2f skin %.2f light %.2f", ms_(m[1].anim), ms_(m[1].pal), ms_(m[1].skin), ms_(m[1].light));
    say("  sends: opaque %.2f detail %.2f visor %.2f shadow %.2f set %.2f", ms_(m[1].op), ms_(m[1].det), ms_(m[1].vis), ms_(m[1].shd), ms_(m[1].env));
    say("  PVR: shadow target %.2f, screen %.2f; TA map %uK screen %uK", ms_(m[1].pvr_map), ms_(m[1].pvr_scr),
        (unsigned)((m[1].ta_map + 512u) / 1024u), (unsigned)((m[1].ta_scr + 512u) / 1024u));
    say("  set: %u triangles sent a frame, %u cut at the near plane, %u of %u objects left out", m[1].tri_env, m[1].tri_cut, m[1].obj_out, STRIP_OBJS);
    sh4[0] = ms_(m[1].anim + m[1].pal + m[1].skin + m[1].light + m[1].op); pvr[0] = 0.0f;
    sh4[1] = ms_(m[1].shd); pvr[1] = ms_(m[1].pvr_map);
    sh4[2] = ms_(m[1].env); pvr[2] = 0.0f;
    say("by its own timer, stage 2: hero (anim .. opaque send) SH-4 %.2f; set SH-4 %.2f; hens SH-4 %.2f", sh4[0], sh4[2], ms_(m[1].hens));
    say("  shadow SH-4 %.2f, its target's PVR render %.2f (the ground's read of it: in the screen)", sh4[1], pvr[1]);
    sums_mean(&ref_sum, &r);
    say("DERIVED, stage 2's first %u s less the stage without the pass (the same run, the same view):", (unsigned)stages[2].secs);
    for (k = 0; k < 2u; k++) {
        const cost_t *b = &m[2u + k];

        sh4[3u + k] = ms_(sh4_of(&r)) - ms_(sh4_of(b));
        pvr[3u + k] = ms_(r.pvr_map + r.pvr_scr) - ms_(b->pvr_map + b->pvr_scr);
        say("  %-7s SH-4 %+.2f PVR %+.2f TA %+d v", k ? "visor" : "detail", sh4[3u + k], pvr[3u + k], (int)(r.v_scr + r.v_map) - (int)(b->v_scr + b->v_map));
    }
    say("  far camera (5 - 2, %u.%u m against %u.%u m): frame %+.2f PVR %+.2f", (unsigned)stages[4].dist, (unsigned)(stages[4].dist * 10.0f) % 10u,
        (unsigned)stages[1].dist, (unsigned)(stages[1].dist * 10.0f) % 10u, ms_(m[4].frame) - ms_(r.frame), ms_(m[4].pvr_map + m[4].pvr_scr) - ms_(r.pvr_map + r.pvr_scr));
    say("the hero's and the set's PVR share is not apart from the screen's: not given");
    say("culling off on the hero (closed tubes): a back-face cull is not tried here");
    g = pg_plot("hero1: what each part costs in a frame, ms", "part", "ms", 1);
    pg_cat(g, "hero"); pg_cat(g, "shadow"); pg_cat(g, "set"); pg_cat(g, "detail"); pg_cat(g, "visor");
    pg_series(g, "SH-4"); for (k = 0; k < 5u; k++) pg_point(g, (float)k, sh4[k]);
    pg_series(g, "PVR"); for (k = 0; k < 5u; k++) pg_point(g, (float)k, pvr[k]);
}

int main(void) {
    unsigned page = 0, pages, held = ~0u;
    int redraw = 1, fault_on = 0;

    suite_init();
    vid_set_mode(DM_640x480, PM_RGB565);
    pg_init();
    vid_clear(0, 0, 0);
    pg_text(PG_X0, PG_Y0, PG_WHITE, "hero1: loading the hero ...");
    file_state = hero_load();
    if (start()) {
        if (!fault_on) { fault_on = 1; if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused"); }
        loop(1);
        stop("auto end");
    }
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
        else if (hit & CONT_START) {
            if (start()) { loop(0); stop("live end"); }
            vid_set_mode(DM_640x480, PM_RGB565);
            held = ~0u; redraw = 1;
        }
        else if (suite_tick(now, page, pages)) { page++; redraw = 1; }
        if (redraw) { pg_draw(page % pages, "HERO hero1", suite_keys_text("START live view")); fx_page(hero_fx_page, HERO_FX_PAGE_N); redraw = 0; }
        thd_sleep(20);
    }
    return 0;
}
