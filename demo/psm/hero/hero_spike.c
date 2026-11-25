/* SH4DER-PVR: hero1, a skinned hero on a clay set, each pass on and off with its cost, Dreamcast demo and test program

   demo/psm/hero/hero_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   One skinned figure (hero_model.h's own, or /cd/HERO.BIN: README.md) in a clay set: a light-grey ground and two walls,
   or the room (../ao/room_data.h, with the town's four hens wandering in it). The hero's passes, each on its own:

     OPAQUE  the base texture x the light a vertex (ambient + sun, the sun taken into each bone's space), opaque list
     DETAIL  the detail texture over the armour, 2 x multiply (DESTCOLOR x DESTCOLOR), translucent list, depth equal
     VISOR   a sphere map on the visor, added (ONE x ONE), translucent list, depth equal
     SHADOW  the hero (and the hens) slid along the sun onto the ground, drawn into a 256 x 256 target first; the
             ground (only) reads it

   A frame: the clip at t (hero_pose), each bone's palette, every vertex skinned (two matrices where a joint blends), the
   light, the sends. Every part is timed on the SH-4's clock; the PVR's render times and the TA's vertex buffer use come
   from pvr_get_stats after each scene.

   It runs by itself first (README.md "The autorun": ten stages, HERO_AUTO_S seconds each, the camera orbiting), then
   pages and a graph. START on the pages: the live view.

     X  OPAQUE    D-pad left  DETAIL    B  VISOR    A  SHADOW    Y  IDLE / RUN+AIM
     D-pad up  CLAY / ROOM     D-pad down  HENS (in the room)     L / R triggers  zoom out / in     stick  orbit
     START  the pages

   Waits are polled on the timer (the render-done interrupt noted by this program's handler in front of KallistiOS's), no
   longer than LIMIT_NS. The fault catcher (../psm_fault.h) is in. Only a real console's run is evidence. */

#include <kos.h>
#include <stdarg.h>
#include <stdio.h>

#define PSM_FAULT_IMPL
#include "../psm_fault.h"
#include "../psm_suite.h"
#include "hero_model.h"
#include "hero_file.h"
#include "hero_view.h"
#include "../ao/room_data.h"
#include "hero_hens.h"
#include <sh4zam_dce/shz_dce_atan2.h>

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
#include "../psm_page.h"
#define say pg_say
static psm_fault_crumbs_t crumbs;

static void step(const char *name) { psm_fault_beat(&crumbs); psm_fault_stage(&crumbs, name); crumbs.frame++; }

#define LIMIT_NS 200000000ull
static inline uint64_t ns(void) { const uint64_t n = timer_ns_gettime64(); return n - n / 1000000000ull * 2502400ull; }   /* ao4: the TMU2 reload step out */
static unsigned us_of(uint64_t t) { return (unsigned)((t + 500u) / 1000u); }

/* ---- memory, headers, the vertex run --------------------------------------------------------------------------------- */

#define MAP 256u
#define MAP_ROWS (MAP + 8u)
#define RUN 256u
#define TEXT_W 512u
#define TEXT_H 64u
#define TEXT_LINES 4u
#define SHADOW_ARGB 0xff8c8c8cu
#define CLAY 0.80f

static pvr_ptr_t map_mem, text_mem, tex_mem[HERO_TEXS];
static pvr_poly_hdr_t h_flat, h_set, h_ground, h_base, h_detail, h_visor, h_text, h_dim;
static pvr_vertex_t vrun[RUN] __attribute__((aligned(32)));
static unsigned run_n, sent;
static uint16_t text_px[TEXT_W * TEXT_H] __attribute__((aligned(32)));
static char text_ln[TEXT_LINES][65];
static int text_dirty;

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
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                       /* the set, the room, the hens: a vertex colour */
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
                for (x = 0; x < PG_CW; x++) text_px[(l * 16u + y) * TEXT_W + i * 8u + x] = (uint16_t)(((gl[y][x] >> 4) << 12) | 0x0fffu);
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
    else { for (t = 0; t < 20u && pvr_wait_render_done() < 0; t++) psm_fault_beat(&crumbs); if (t == 20u) return 0; }
    if (pvr_get_stats(&st) < 0) return 0;
    last_render_ns = st.rnd_last_time;
    last_vbuf = (uint32_t)st.vtx_buffer_used;
    return 1;
}

/* ---- the state ------------------------------------------------------------------------------------------------------ */

typedef struct view {
    int opq, det, vis, shd, hero_on, room, hens;
    unsigned clip;
    float yaw, pitch, dist, t;
} view_t;

typedef struct cost {
    uint64_t anim, pal, skin, light, op, det, vis, shd, env, hens, pvr_map, pvr_scr, frame;   /* ns */
    unsigned v_scr, v_map, tri_hero, tri_env;
    uint32_t ta_map, ta_scr;
} cost_t;

static hero_set_t set;
static float pose[HERO_BONES][12], pal[HERO_BONES][16] __attribute__((aligned(8))), spal[HERO_BONES][16] __attribute__((aligned(8)));
static float lb[HERO_BONES][3], rb[HERO_BONES][3], ub[HERO_BONES][3];
static float hs[HERO_V_MAX][3], hsh[HERO_V_MAX][3], vuv[HERO_V_MAX][2];
static uint8_t hl[HERO_V_MAX];
static float sscr[HERO_SET_V][3], suv[HERO_SET_V][2];
static uint8_t sok[HERO_SET_V];
static uint32_t scol[HERO_SET_V];
static float rscr[ROOM_V][3], ruv[ROOM_V][2];
static uint8_t rok[ROOM_V];
static uint32_t rcol[ROOM_V];
static uint8_t room_floor[ROOM_OBJS];
static const float sun[3] = { 0.4472f, 0.7826f, 0.4334f };   /* toward the sun, unit */
static const float amb = 0.42f, sunk = 0.60f;
static float smat[16] __attribute__((aligned(8)));

static uint32_t grey_(float k) { const unsigned g = (unsigned)(sh4der_sat_(k) * 255.0f); return 0xff000000u | (g << 16) | (g << 8) | g; }
static uint32_t rgb_(float r, float g, float b) {
    return 0xff000000u | ((unsigned)(sh4der_sat_(r) * 255.0f) << 16) | ((unsigned)(sh4der_sat_(g) * 255.0f) << 8) | (unsigned)(sh4der_sat_(b) * 255.0f);
}

/* ---- the hens: the town's template and wander (psm_town.h's psm_hen_step), moved into the room ------------------------ */

#define HENS 4u
/* The town's world (story x, -story z) turned a quarter about y into the room, as ao4's hens (ao4_hens.h): room x = story z
   + 0.2, room z = story x - 7.05. The pen and the yard land where ao4 puts them. */
#define HEN_OX 0.2f
#define HEN_OZ (-7.05f)
#define HEN_FLEE 1.5f
typedef struct hen { float x, z, bearing, tilt, t, b[4]; int mode; uint32_t rng; } hen_t;
static hen_t hen[HENS];
static float hen_scr[HENS][PSM_TOWN_HEN_V][3];
static uint8_t hen_ok[HENS][PSM_TOWN_HEN_V];
static uint32_t hen_col[HENS][PSM_TOWN_HEN_V];

static const float hen_home[HENS][3] = { { 8.2f, 0.0f, 1.0f }, { 8.8f, 1.2f, 1.0f }, { 7.0f, -1.6f, 0.0f }, { 6.3f, 0.9f, 0.0f } };

static float hen_rand_(uint32_t *r) { *r = *r * 1664525u + 1013904223u; return (float)(*r >> 8) * (1.0f / 16777216.0f); }

static void hens_reset(void) {
    unsigned i;

    for (i = 0; i < HENS; i++) {
        hen_t *m = &hen[i];

        memset(m, 0, sizeof *m);
        m->x = hen_home[i][0]; m->z = hen_home[i][1]; m->bearing = 1.3f * (float)i; m->rng = 0x9e3779b9u * (i + 1u); m->t = 0.1f * (float)i;
        if (hen_home[i][2] > 0.0f) { m->b[0] = 7.9f; m->b[1] = 9.1f; m->b[2] = -0.85f; m->b[3] = 1.85f; }
        else { m->b[0] = 5.3f; m->b[1] = 7.4f; m->b[2] = -3.0f; m->b[3] = 2.6f; }
    }
}

static void hen_step(hen_t *m, float dt, float ex, float ez) {   /* the walker: the eye, in story metres */
    const float pi = 3.14159265f, dx = m->x - ex, dz = m->z - ez;
    float sb, cb;

    if (dx * dx + dz * dz < HEN_FLEE * HEN_FLEE && m->mode != 2) {
        m->mode = 2; m->t = 0.6f;
        m->bearing = dx * dx + dz * dz > 1e-6f ? shz_dce_atan2f(dx, dz) : m->bearing + pi;
    }
    m->t -= dt;
    switch (m->mode) {
        case 0: {
            float s, c;

            sh4der_sincos_(pi * sh4der_clamp_(1.0f - m->t * 2.5f, 0.0f, 1.0f), &s, &c);
            m->tilt = 0.7f * s;
            if (m->t <= 0.0f) { m->mode = 1; m->t = 0.4f; m->tilt = 0.0f; m->bearing += (hen_rand_(&m->rng) - 0.5f) * 2.0f; }
            break;
        }
        case 1: case 2:
            sh4der_sincos_(m->bearing, &sb, &cb);
            m->x += sb * (m->mode == 2 ? 2.0f : 0.5f) * dt;
            m->z += cb * (m->mode == 2 ? 2.0f : 0.5f) * dt;
            m->tilt = 0.0f;
            if (m->t <= 0.0f) { m->mode = 0; m->t = 0.4f; }
            break;
        default: m->mode = 0; m->t = 0.4f; break;
    }
    if (m->x < m->b[0] || m->x > m->b[1]) { m->x = sh4der_clamp_(m->x, m->b[0], m->b[1]); m->bearing = -m->bearing; }
    if (m->z < m->b[2] || m->z > m->b[3]) { m->z = sh4der_clamp_(m->z, m->b[2], m->b[3]); m->bearing = pi - m->bearing; }
    if (m->bearing > pi) m->bearing -= 2.0f * pi;
    if (m->bearing < -pi) m->bearing += 2.0f * pi;
}

/* psm_scene.h's psm_mover_rows, in the room: p' = (p, 1) x r */
static void hen_rows(const hen_t *m, float r[16]) {
    float sb, cb, st, ct;

    sh4der_sincos_(m->bearing, &sb, &cb); sh4der_sincos_(m->tilt, &st, &ct);
    r[0] = cb;      r[1] = 0.0f; r[2] = sb;        r[3] = 0.0f;
    r[4] = sb * st; r[5] = ct;   r[6] = -cb * st;  r[7] = 0.0f;
    r[8] = -sb * ct; r[9] = st;  r[10] = cb * ct;  r[11] = 0.0f;
    r[12] = m->x - 0.1f * st * sb; r[13] = 0.1f - 0.1f * ct; r[14] = -m->z + 0.1f * st * cb; r[15] = 1.0f;
}

static void hen_world(const float r[16], const float p[3], float o[3]) {   /* the town's world point, then into the room */
    const float wx = p[0] * r[0] + p[1] * r[4] + p[2] * r[8] + r[12], wz = p[0] * r[2] + p[1] * r[6] + p[2] * r[10] + r[14];

    o[0] = HEN_OX - wz; o[1] = p[0] * r[1] + p[1] * r[5] + p[2] * r[9] + r[13]; o[2] = wx + HEN_OZ;
}

/* ---- load ---------------------------------------------------------------------------------------------------------- */

static void set_light(void) {
    unsigned i, k;

    hero_set_build(&set);
    for (i = 0; i < set.vn; i++) {
        const float d = sh4der_dot3_(set.n[i], sun);

        scol[i] = grey_(CLAY * (amb + sunk * (d > 0.0f ? d : 0.0f)));
        hero_ground_uv(set.p[i], suv[i]);
    }
    for (i = 0; i < ROOM_V; i++) {
        const float d = sh4der_dot3_(room_n[i], sun), a = (float)room_ao_v[i] * (1.0f / 255.0f);

        rcol[i] = grey_(CLAY * (amb * a + sunk * (d > 0.0f ? d : 0.0f)));
        hero_ground_uv(room_v[i], ruv[i]);
    }
    for (k = 0; k < ROOM_OBJS; k++) room_floor[k] = room_obj[k].surf >= 0 && room_surf[room_obj[k].surf].kind == ROOM_FLOOR;
    hero_shadow_matrix(sun, (float)MAP, smat);
}

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
    pvr_set_bg_color(0.80f, 0.82f, 0.86f);
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

static void hen_place(const view_t *v, const hero_cam_t *cam, float dt) {
    unsigned h, i;

    for (h = 0; h < HENS; h++) {
        float r[16], lr[3];

        const float st[3] = { sun[2], sun[1], -sun[0] };   /* the sun in the town's world: (room z, y, -room x) */

        hen_step(&hen[h], dt, cam->eye[2] - HEN_OZ, cam->eye[0] - HEN_OX);   /* the eye in story metres */
        hen_rows(&hen[h], r);
        lr[0] = st[0] * r[0] + st[1] * r[1] + st[2] * r[2];   /* the sun in the hen's own axes */
        lr[1] = st[0] * r[4] + st[1] * r[5] + st[2] * r[6];
        lr[2] = st[0] * r[8] + st[1] * r[9] + st[2] * r[10];
        for (i = 0; i < PSM_TOWN_HEN_V; i++) {
            float w[3];
            const float d = ((float)psm_town_hen_n[i][0] * lr[0] + (float)psm_town_hen_n[i][1] * lr[1] + (float)psm_town_hen_n[i][2] * lr[2]) * (1.0f / 127.0f);
            const float *c = hero_hen_pal[psm_town_hen_c[i] - HERO_HEN_PAL0], k = amb + sunk * (d > 0.0f ? d : 0.0f);

            hen_world(r, psm_town_hen_v[i], w);
            hen_ok[h][i] = (uint8_t)hero_view_pt(cam->vp, w, hen_scr[h][i]);
            hen_col[h][i] = rgb_(c[0] * k, c[1] * k, c[2] * k);
        }
    }
    (void)v;
}

static void hen_send(void) {
    unsigned h, s, i, k;

    for (h = 0; h < HENS; h++)
        for (s = 0, k = 0; s < PSM_TOWN_HEN_S; k += psm_town_hen_sl[s], s++) {
            int ok = 1;

            for (i = 0; i < psm_town_hen_sl[s]; i++) if (!hen_ok[h][psm_town_hen_sv[k + i]]) ok = 0;
            if (ok) for (i = 0; i < psm_town_hen_sl[s]; i++) {
                const unsigned q = psm_town_hen_sv[k + i];

                put(i + 1u == psm_town_hen_sl[s], hen_scr[h][q][0], hen_scr[h][q][1], hen_scr[h][q][2], 0, 0, hen_col[h][q]);
            }
        }
}

static void hen_casters(void) {
    unsigned h, t, k;

    for (h = 0; h < HENS; h++) {
        float r[16], p[PSM_TOWN_HEN_CV][3];

        hen_rows(&hen[h], r);
        for (k = 0; k < PSM_TOWN_HEN_CV; k++) {
            float w[3], o[4];

            hen_world(r, psm_town_hen_cv[k], w);
            hero_xf_(smat, w, o);
            p[k][0] = o[0]; p[k][1] = o[1];
        }
        for (t = 0; t < PSM_TOWN_HEN_CT; t++)
            for (k = 0; k < 3u; k++) put(k == 2u, p[psm_town_hen_ct[t][k]][0], p[psm_town_hen_ct[t][k]][1], 1.0f, 0, 0, SHADOW_ARGB);
    }
}

static int frame(view_t *v, cost_t *c, float dt) {
    const float target[3] = { 0.0f, 1.0f, 0.0f };
    const uint64_t tf = ns();
    hero_cam_t cam;
    uint64_t t0;
    unsigned i, k;

    memset(c, 0, sizeof *c);
    hero_cam_make(&cam, v->yaw, v->pitch, v->dist, target);
    if (v->hero_on) {
        t0 = ns();                                         /* anim: the clip at t, the light and the camera into each bone */
        hero_pose(&hero, v->clip, v->t, pose);
        hero_bone_dirs((const float (*)[12])pose, sun, lb);
        if (v->vis) { hero_bone_dirs((const float (*)[12])pose, cam.right, rb); hero_bone_dirs((const float (*)[12])pose, cam.up, ub); }
        c->anim = ns() - t0;
        t0 = ns(); hero_palette(cam.vp, (const float (*)[12])pose, pal); c->pal = ns() - t0;
        t0 = ns(); hero_skin(&hero, (const float (*)[16])pal, 1, HERO_NEAR, hs); c->skin = ns() - t0;
        t0 = ns(); hero_light(&hero, (const float (*)[3])lb, amb, sunk, hl); c->light = ns() - t0;
        c->tri_hero = hero.tris;
    }
    t0 = ns();                                             /* the set's or the room's vertices on screen */
    if (v->room) for (i = 0; i < ROOM_V; i++) rok[i] = (uint8_t)hero_view_pt(cam.vp, room_v[i], rscr[i]);
    else for (i = 0; i < set.vn; i++) sok[i] = (uint8_t)hero_view_pt(cam.vp, set.p[i], sscr[i]);
    c->env = ns() - t0;
    if (v->room && v->hens) { t0 = ns(); hen_place(v, &cam, dt); c->hens = ns() - t0; }

    if (v->shd) {                                          /* SHADOW: the target first */
        t0 = ns();
        if (v->hero_on) {
            hero_palette(smat, (const float (*)[12])pose, spal);
            hero_skin(&hero, (const float (*)[16])spal, 0, 0.5f, hsh);
        }
        c->shd = ns() - t0;
        if (!scene_open(1)) return 0;
        pvr_list_begin(PVR_LIST_OP_POLY);
        hdr(&h_flat);
        rect(0.0f, 0.0f, (float)MAP, (float)MAP + 2.0f, 0.1f, 0, 0, 0, 0, 0xffffffffu);
        t0 = ns(); sent = 0;
        if (v->hero_on) send_strips(0u, hero.strips, (const float (*)[3])hsh, 0, SHADOW_ARGB);
        if (v->room && v->hens) hen_casters();
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
    if (v->hero_on && v->vis) hero_sphere_uv(&hero, (const float (*)[3])rb, (const float (*)[3])ub, hero.visor_strip0, hero.strips, vuv);

    if (!scene_open(0)) return 0;
    sent = 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    t0 = ns();
    if (v->room) {                                         /* the room: a triangle at a time, those facing the eye */
        for (k = 0; k < ROOM_OBJS; k++) {
            const room_obj_t *o = &room_obj[k];
            const int ground = room_floor[k] && v->shd;

            hdr(ground ? &h_ground : &h_set);
            for (i = o->t0; i < (unsigned)o->t0 + o->tn; i++) {
                const unsigned a = room_t[i][0], b = room_t[i][1], d = room_t[i][2];

                if (!rok[a] || !rok[b] || !rok[d] || hero_area2(rscr[a], rscr[b], rscr[d]) >= 0.0f) continue;
                put(0, rscr[a][0], rscr[a][1], rscr[a][2], ruv[a][0], ruv[a][1], rcol[a]);
                put(0, rscr[b][0], rscr[b][1], rscr[b][2], ruv[b][0], ruv[b][1], rcol[b]);
                put(1, rscr[d][0], rscr[d][1], rscr[d][2], ruv[d][0], ruv[d][1], rcol[d]);
                c->tri_env++;
            }
        }
    } else {                                               /* the clay set: a strip a row; a wall only from its inside */
        unsigned r;
        int cur = -1;

        for (r = 0; r < set.rows; r++) {
            const unsigned v0 = set.row_v0[r], nu = set.row_nu[r], w = set.row_wall[r];
            const int want = w == 0u && v->shd;
            int ok = 1;

            if (!hero_wall_faces(w, cam.eye)) continue;
            for (i = 0; i <= nu; i++) if (!sok[v0 + i] || !sok[v0 + nu + 1u + i]) ok = 0;
            if (!ok) continue;
            if (want != cur) { hdr(want ? &h_ground : &h_set); cur = want; }
            for (i = 0; i <= nu; i++) {
                const unsigned up = v0 + nu + 1u + i, dn = v0 + i;

                put(0, sscr[up][0], sscr[up][1], sscr[up][2], suv[up][0], suv[up][1], scol[up]);
                put(i == nu, sscr[dn][0], sscr[dn][1], sscr[dn][2], suv[dn][0], suv[dn][1], scol[dn]);
            }
            c->tri_env += 2u * nu;
        }
    }
    if (v->room && v->hens) { hdr(&h_set); hen_send(); c->tri_env += HENS * PSM_TOWN_HEN_T; }
    flush();
    c->env += ns() - t0;
    if (v->hero_on && v->opq) { t0 = ns(); hdr(&h_base); send_strips(0u, hero.strips, (const float (*)[3])hs, 0, 0u); flush(); c->op = ns() - t0; }
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    if (v->hero_on && v->det) { t0 = ns(); hdr(&h_detail); send_strips(0u, hero.visor_strip0, (const float (*)[3])hs, 1, 0xffffffffu); flush(); c->det = ns() - t0; }
    if (v->hero_on && v->vis) { t0 = ns(); hdr(&h_visor); send_strips(hero.visor_strip0, hero.strips, (const float (*)[3])hs, 2, 0xff909090u); flush(); c->vis = ns() - t0; }
    c->v_scr = sent;
    text_line(0u, 16.0f); text_line(1u, 34.0f); text_line(2u, 52.0f); text_line(3u, 446.0f);
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

    text_set(0u, "%s %s OPQ %s DET %s VIS %s SHD %s %s%s", file_state == 0 ? "FILE" : "BUILT-IN", v->clip ? "RUN+AIM" : "IDLE",
             on_(v->opq), on_(v->det), on_(v->vis), on_(v->shd), v->room ? "ROOM" : "CLAY", v->room && v->hens ? " HENS" : "");
    text_set(1u, "frame %u.%u SH4 %u.%u PVR %u.%u ms  anim+skin %u.%u ms", f / 1000u, f % 1000u / 100u, s / 1000u, s % 1000u / 100u,
             p / 1000u, p % 1000u / 100u, a / 1000u, a % 1000u / 100u);
    text_set(2u, "TA %u v %uK  hero %u tri %u v  set %u tri  %u.%u m", c->v_scr + c->v_map, (unsigned)((c->ta_scr + c->ta_map + 512u) / 1024u),
             c->tri_hero, hero.vn, c->tri_env, (unsigned)v->dist, (unsigned)(v->dist * 10.0f) % 10u);
    text_set(3u, "X opq <det B vis A shd Y anim ^env vhens LT/RT zoom ST pages");
}

/* ---- the loop, the autorun ---------------------------------------------------------------------------------------- */

#define STAGES 10u
typedef struct stage { uint8_t opq, det, vis, shd, hero_on, room, hens, clip; float dist; const char *name; } stage_t;
static const stage_t stages[STAGES] = {
    { 1, 1, 1, 1, 1, 0, 0, HERO_IDLE, 3.5f, "all, IDLE" },
    { 1, 1, 1, 1, 1, 0, 0, HERO_RUN_AIM, 3.5f, "all, RUN+AIM" },
    { 1, 0, 0, 0, 1, 0, 0, HERO_RUN_AIM, 3.5f, "opaque only" },
    { 1, 1, 0, 0, 1, 0, 0, HERO_RUN_AIM, 3.5f, "opaque+detail" },
    { 1, 0, 1, 0, 1, 0, 0, HERO_RUN_AIM, 3.5f, "opaque+visor" },
    { 1, 0, 0, 1, 1, 0, 0, HERO_RUN_AIM, 3.5f, "opaque+shadow" },
    { 0, 0, 0, 0, 0, 0, 0, HERO_RUN_AIM, 3.5f, "no hero (set)" },
    { 1, 1, 1, 1, 1, 1, 0, HERO_RUN_AIM, 3.5f, "all, room" },
    { 1, 1, 1, 1, 1, 1, 1, HERO_RUN_AIM, 3.5f, "all, room+hens" },
    { 1, 1, 1, 1, 1, 0, 0, HERO_RUN_AIM, 7.0f, "all, 2x distance" },
};

typedef struct sums { cost_t c; unsigned n; } sums_t;
static sums_t auto_sum[STAGES];
static unsigned auto_done;

static void sums_add(sums_t *s, const cost_t *c) {
    s->c.anim += c->anim; s->c.pal += c->pal; s->c.skin += c->skin; s->c.light += c->light; s->c.op += c->op; s->c.det += c->det;
    s->c.vis += c->vis; s->c.shd += c->shd; s->c.env += c->env; s->c.hens += c->hens; s->c.pvr_map += c->pvr_map; s->c.pvr_scr += c->pvr_scr;
    s->c.frame += c->frame; s->c.v_scr += c->v_scr; s->c.v_map += c->v_map; s->c.ta_map += c->ta_map; s->c.ta_scr += c->ta_scr;
    s->c.tri_hero = c->tri_hero; s->c.tri_env = c->tri_env;
    s->n++;
}

static void sums_mean(const sums_t *s, cost_t *m) {
    const unsigned n = s->n ? s->n : 1u;

    *m = s->c;
    m->anim /= n; m->pal /= n; m->skin /= n; m->light /= n; m->op /= n; m->det /= n; m->vis /= n; m->shd /= n; m->env /= n; m->hens /= n;
    m->pvr_map /= n; m->pvr_scr /= n; m->frame /= n; m->v_scr /= n; m->v_map /= n; m->ta_map /= n; m->ta_scr /= n;
}

static void view_stage(view_t *v, unsigned k) {
    const stage_t *s = &stages[k];

    v->opq = s->opq; v->det = s->det; v->vis = s->vis; v->shd = s->shd; v->hero_on = s->hero_on;
    v->room = s->room; v->hens = s->hens; v->clip = s->clip; v->dist = s->dist; v->t = 0.0f;
}

static void loop(int auto_run) {
    view_t v;
    sums_t win;
    cost_t c, shown;
    unsigned held = ~0u, frames = 0, stage = 0;
    uint64_t t_prev = ns(), t_stage = ns();

    memset(&v, 0, sizeof v);
    view_stage(&v, 1u);
    v.pitch = 0.28f; v.yaw = 0.6f;
    if (auto_run) { view_stage(&v, 0u); memset(auto_sum, 0, sizeof auto_sum); auto_done = 0; }
    hens_reset();
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
        if (auto_run) {
            v.yaw += 0.35f * dt;
            if (t_now - t_stage > (uint64_t)HERO_AUTO_S * 1000000000ull) {
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
            if (hit & CONT_Y) { v.clip = v.clip == HERO_IDLE ? HERO_RUN_AIM : HERO_IDLE; v.t = 0.0f; changed = 1; }
            if (hit & CONT_DPAD_UP) { v.room = !v.room; changed = 1; }
            if (hit & CONT_DPAD_DOWN) { v.hens = !v.hens; changed = 1; }
            if (st) {
                if (st->joyx > 24 || st->joyx < -24) v.yaw += (float)st->joyx * (1.0f / 128.0f) * dt * 1.6f;
                if (st->joyy > 24 || st->joyy < -24) v.pitch += (float)st->joyy * (1.0f / 128.0f) * dt * 1.0f;
                v.pitch = sh4der_clamp_(v.pitch, 0.05f, 1.25f);
                v.dist += ((float)st->ltrig - (float)st->rtrig) * (1.0f / 255.0f) * dt * 3.0f;   /* L out, R in */
                v.dist = sh4der_clamp_(v.dist, 1.6f, 9.0f);
            }
        }
        if (changed) memset(&win, 0, sizeof win);
        if (!frame(&v, &c, dt)) { say("%s: NO RENDER END at frame %u (stage %u)", auto_run ? "auto" : "live", frames, stage); break; }
        frames++;
        sums_add(&win, &c);
        if (auto_run && t_now - t_stage > 500000000ull) sums_add(&auto_sum[stage], &c);   /* the first half second of a stage is not counted */
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
    cost_t m[STAGES];
    pg_plot_t *g;
    char rev[32];
    unsigned k;
    const char *const pass[4] = { "detail", "visor", "shadow", "opaque" };
    float add_s[6], add_p[6];

    suite_rev_text(rev, sizeof rev);
    say("hero1 %s: a skinned hero, each pass's cost. This run's numbers.", HERO_STR(HERO_TAG));
    say("%s. %u s a stage, the first 0.5 s not counted; ms unless said", rev, HERO_AUTO_S);
    say("hero: %s, %u vertices, %u triangles, %u strips, 33 bones, clips %u+%u frames", file_state == 0 ? "HERO.BIN" : "built-in (original)",
        hero.vn, hero.tris, hero.strips, hero.frames[0], hero.frames[1]);
    if (file_state < 0) say("HERO.BIN refused: %s; the built-in hero ran", hero_file_error(file_state));
    if (auto_done < STAGES) say("THE RUN DID NOT FINISH: the lines below are of what ran");
    for (k = 0; k < STAGES; k++) {
        sums_mean(&auto_sum[k], &m[k]);
        say("%u %-16s fr %5.2f SH4 %5.2f PVR %5.2f TA %4uv %3uK n%u", k + 1u, stages[k].name, ms_(m[k].frame), ms_(sh4_of(&m[k])),
            ms_(m[k].pvr_map + m[k].pvr_scr), m[k].v_scr + m[k].v_map, (unsigned)((m[k].ta_scr + m[k].ta_map + 512u) / 1024u), auto_sum[k].n);
    }
    say("stage 2 SH-4 parts: anim %.2f palette %.2f skin %.2f light %.2f", ms_(m[1].anim), ms_(m[1].pal), ms_(m[1].skin), ms_(m[1].light));
    say("  sends: opaque %.2f detail %.2f visor %.2f shadow %.2f set %.2f", ms_(m[1].op), ms_(m[1].det), ms_(m[1].vis), ms_(m[1].shd), ms_(m[1].env));
    say("  PVR: shadow target %.2f, screen %.2f; TA map %uK screen %uK", ms_(m[1].pvr_map), ms_(m[1].pvr_scr),
        (unsigned)((m[1].ta_map + 512u) / 1024u), (unsigned)((m[1].ta_scr + 512u) / 1024u));
    say("DERIVED, each pass over stage 3 (opaque only): SH-4 / PVR ms, TA vertices");
    for (k = 0; k < 3u; k++) {
        const cost_t *a = &m[3u + k], *b = &m[2];

        add_s[k] = ms_(sh4_of(a)) - ms_(sh4_of(b));
        add_p[k] = ms_(a->pvr_map + a->pvr_scr) - ms_(b->pvr_map + b->pvr_scr);
        say("  %-7s SH-4 %+.2f PVR %+.2f TA %+d v", pass[k], add_s[k], add_p[k], (int)(a->v_scr + a->v_map) - (int)(b->v_scr + b->v_map));
    }
    add_s[3] = ms_(sh4_of(&m[2])) - ms_(sh4_of(&m[6])); add_p[3] = ms_(m[2].pvr_map + m[2].pvr_scr) - ms_(m[6].pvr_map + m[6].pvr_scr);
    say("  hero, opaque (stage 3 - 7): SH-4 %+.2f PVR %+.2f TA %+d v", add_s[3], add_p[3], (int)(m[2].v_scr) - (int)(m[6].v_scr));
    add_s[4] = ms_(sh4_of(&m[7])) - ms_(sh4_of(&m[1])); add_p[4] = ms_(m[7].pvr_map + m[7].pvr_scr) - ms_(m[1].pvr_map + m[1].pvr_scr);
    say("  room over clay (8 - 2): SH-4 %+.2f PVR %+.2f, %u room tri drawn", add_s[4], add_p[4], m[7].tri_env);
    add_s[5] = ms_(sh4_of(&m[8])) - ms_(sh4_of(&m[7])); add_p[5] = ms_(m[8].pvr_map + m[8].pvr_scr) - ms_(m[7].pvr_map + m[7].pvr_scr);
    say("  4 hens (9 - 8): SH-4 %+.2f PVR %+.2f TA %+d v", add_s[5], add_p[5], (int)(m[8].v_scr + m[8].v_map) - (int)(m[7].v_scr + m[7].v_map));
    say("  2x distance (10 - 2): frame %+.2f PVR %+.2f", ms_(m[9].frame) - ms_(m[1].frame), ms_(m[9].pvr_map + m[9].pvr_scr) - ms_(m[1].pvr_map + m[1].pvr_scr));
    say("culling off on the hero (closed tubes): a back-face cull is not tried here");
    g = pg_plot("hero1: what each part adds to a frame, ms", "part", "ms", 1);
    pg_cat(g, "detail"); pg_cat(g, "visor"); pg_cat(g, "shadow"); pg_cat(g, "opaque"); pg_cat(g, "room"); pg_cat(g, "hens");
    pg_series(g, "SH-4"); for (k = 0; k < 6u; k++) pg_point(g, (float)k, add_s[k]);
    pg_series(g, "PVR"); for (k = 0; k < 6u; k++) pg_point(g, (float)k, add_p[k]);
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
    set_light();
    if (start()) {
        if (!fault_on) { fault_on = 1; if (psm_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused"); }
        loop(1);
        stop("auto end");
    }
    report();
    vid_set_mode(DM_640x480, PM_RGB565);
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;

        psm_fault_beat(&crumbs);
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
        if (redraw) { pg_draw(page % pages, "HERO hero1", suite_keys_text("START live view")); redraw = 0; }
        thd_sleep(20);
    }
    return 0;
}
