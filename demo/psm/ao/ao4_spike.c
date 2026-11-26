/* SH4DER-PVR: ao4, ambient occlusion side by side on simple scenes, Dreamcast demo and test program

   demo/psm/ao/ao4_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A demo for a real console: the same scene drawn twice, the left half of the screen one way and the right half
   another, so the difference AO makes can be seen. Scenes, methods and looks are ao4_scene.h's:

     SCENE A  a big sphere, a small sphere and a box on a plain ground      SCENE B  two walls, a corner, a sphere in it
     SCENE C  the living room (ao4_room.h; its AO baked on the host by tools/room_bake.c), no cast sun shadow, and
              the town's four hens walking in it (ao4_hens.h): movers a bake knows nothing of
     OFF      no AO
     1 BAKED  a byte a vertex made once at load (64 rays a vertex against the exact shapes), on the ambient light:
              the ground, the walls and the shapes themselves
     2 MAP    the casters drawn into the sun's shadow target from K sky directions, one side, plain blending
              (../ao_layer.h): THE GROUND ONLY, nothing on walls or shapes
     3 LIGHTMAP  scene C alone: each flat surface (the floor, a wall) as ONE quad with its baked AO as a texture, in
              place of its grid of vertices: method 1's AO with far fewer vertices, for a texture's video memory
     3+M      scene C alone: 3, and a tile a hen (ao4_hens.h, ../AO2_DESIGN.md's movers' half): the hens' casters
              drawn from the K sky directions into a 128 x 128 target, a 32-pixel tile each, every frame, and one
              multiply quad a hen on the floor. What 2 does for a mover, without sending the room's casters
     looks    CLAY (grey, the ambient strong), COLOUR, AO ONLY (white x AO, no sun)

   It runs by itself first: scene A then scene B, each for the three splits OFF | 1, OFF | 2, 1 | 2, then scene C for
   OFF | 1, 1 | 3, OFF | 3, 3 | 2 with the hens and 3 | 2 without, 3 | 3+M, 3+M | 2, 3+M | 3+M and that with the quads
   and their target left out, AO4_AUTO_S seconds each; then two short stages for the clip guard (below) and, LAST, two
   for the tile list cut (sh4der_tilecut.h): 3+M | 3+M with the hens standing still, target M's pass through the screen's
   whole tile table and then through a table cut to its own 4 x 5 tiles, the target's texels summed after each. With the costs counted; then the result pages and one graph. START on the pages: the live view, no time limit.

     left + Y  the clip guard off / on (the polygon sent before each change of user clip, CORE 18): with it off, a
               magenta mark is the left half's last polygon and a cyan one the right half's; if the change of clip
               takes effect a polygon early, the magenta mark is cut away
     X  the next split                            Y  swap the sides          A  EXAGGERATE x2 on / off
     B  the look: CLAY, COLOUR, AO ONLY           LT + RT together  the scene      up / down  K: 2, 4, 8 (scene C: up K, down the hens on / off)
     left / right  method 2's reach              stick  turn and lift the camera      START  back to the pages
     LT / RT  the divider (sh4der_split.h): a tile column (32 pixels) a step, held they repeat. At an end one half is
              hidden and is NOT SENT (no light, no vertices, no target pass that only it used): the other half then
              shows its method's cost on the frame's time. The run that counts keeps the divider at the middle.
     up + B   the effect labels on / off (../psm_effects.h)

   The two halves are cut by the PVR's user clip (a rectangle in tiles; KallistiOS writes the clip bits into every
   header; a dummy polygon goes before each change of clip, see user_clip). The halves share the vertices' places and differ in colour and in which shadow target the ground reads:
   target S (the sun's shadow alone) or target A (the sun's shadow and the sky directions).

   What the screen's cost line means, a side. OFF: the SH-4's lighting of every vertex and its send of the sun's casters,
   and the PVR's render of target S. 1 BAKED: what it ADDS to OFF, the lighting with the AO byte less the lighting
   without; the PVR adds nothing. 2 MAP: what it adds, the SH-4's send of the sky directions and the PVR's render of
   target A less that of target S.

   Waits: "ready" and "render done" are polled on the timer (the render-done interrupt noted by this program's handler
   in front of KallistiOS's); no wait is longer than LIMIT_NS. The text texture is uploaded only after a render is
   done. Only PVR registers and video memory are written. The fault catcher (include/sh4der_fault.h) is in.
   Every number comes from the run that prints it; only a real console's run is evidence.
   ANY BUTTON OR TRIGGER DURING THE RUN (ao4-i) ends the run there and opens the live view at once, as that stage; the
   pages after it say the run was stopped. The stick moves the eye in the run too, as it always did.
   Build: python demo/psm/ao/build.py --spike ao4 --tag <new tag>. Not run on a console when this file was written. */

#include <kos.h>
#include <stdarg.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"
#include "../../../include/sh4der_suite.h"
#include "ao4_scene.h"
#include "ao4_draw.h"
#include "ao4_room.h"
#include "ao4_hens.h"
#include "sh4der_tilecut.h"
#include "sh4der_split.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#ifndef AO4_TAG
#define AO4_TAG untagged
#endif
#define AO4_STR_(x) #x
#define AO4_STR(x) AO4_STR_(x)
#ifndef AO4_AUTO_S
#define AO4_AUTO_S 6u                       /* seconds a split in the run that counts */
#endif

/* ---- the result pages ----------------------------------------------------------- */

#define PG_PREFIX "ao4"
#define PG_LINES_MAX 120u
#define PG_TOP 1u                             /* the effect labels of a page are two lines */
#include "../../../include/sh4der_page.h"
#include "../../../include/sh4der_fxlabel.h"
#include "ao_fx.h"
#define say pg_say
static sh4der_fault_crumbs_t crumbs;

static void step(const char *name) {
    sh4der_fault_beat(&crumbs);
    sh4der_fault_stage(&crumbs, name);
    crumbs.frame++;
}
static void beat(void) { sh4der_fault_beat(&crumbs); }

/* ---- clocks ------------------------------------------------------------------------ */

#define LIMIT_NS 200000000ull

/* KallistiOS's ns clock steps 2,502,400 ns ahead at each reload of TMU2 (MEASURED, image ray2 CLOCK.4): taken out. */
static inline uint64_t ns(void) { const uint64_t n = timer_ns_gettime64(); return n - n / 1000000000ull * 2502400ull; }
static unsigned us_of(uint64_t t) { return (unsigned)((t + 500u) / 1000u); }

/* ---- memory, headers ----------------------------------------------------------------- */

#define MAP_ROWS (AO4_SIZE + 8u)            /* rows allocated for a target; AO4_SIZE + 2 are drawn, as c4's map */
#define RUN 256u
#define TEXT_W 512u
#define TEXT_H 128u
#define TEXT_LINES 6u                         /* 4 and 5: the effect labels */

static pvr_ptr_t map_mem[2], text_mem;      /* [0] target S: the sun alone; [1] target A: the sun and the sky directions */
static pvr_poly_hdr_t h_flat, h_ao, h_ground[2], h_obj, h_text, h_dim;
static pvr_vertex_t vrun[RUN] __attribute__((aligned(32)));
static unsigned run_n, sent;
static uint16_t text_px[TEXT_W * TEXT_H] __attribute__((aligned(32)));
static char text_ln[TEXT_LINES][65];
static int text_dirty;

#define N_SCENES (AO4_SCENES + 1u)          /* A, B, and C the room */
#define SCENE_ROOM AO4_SCENES
static ao4_scene_t scenes[N_SCENES];
static unsigned bake_ms[AO4_SCENES];
static pvr_ptr_t lm_mem[AO4_ROOM_SURF_MAX][2];   /* the room's lightmaps: as baked, and with the occlusion doubled */
static pvr_poly_hdr_t h_lm[AO4_ROOM_SURF_MAX][2];
static uint16_t lm_px[128u * 128u] __attribute__((aligned(32)));
static uint32_t cur_oargb;                  /* the offset colour of the vertices being sent */
static ao4_hens_t hens;                     /* the room's movers */
#define M_ROWS (AO4_HEN_ATLAS + 8u)
static pvr_ptr_t m_mem;                     /* target M: the movers' atlas */
static pvr_poly_hdr_t h_mq, h_mdummy;       /* a mover's quad on the floor; the polygon before a change of clip in the translucent list */
static uint64_t hens_ns;                    /* what their walk and placing took this frame */
static sh4der_tilecut_t tcut;                     /* the tile tables, for target M's pass through its own tiles */
static int clip_guard = 1;                  /* user_clip sends its polygon first (0: the test of what happens without) */
static int cut_fatal;                       /* a tile table did not restore: the PVR is stopped */
static float scr[AO4_V_MAX][3];
static uint32_t col[2][AO4_V_MAX], col_ref[AO4_V_MAX];

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

static void headers(void) {
    pvr_poly_cxt_t c;
    unsigned i;

    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                      /* the map pass: the clear, the sun's casters, the border */
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&h_flat, &c);
    sh4der_aosky_header(&h_ao, 0);                                   /* the sky directions: plain blending (one side is the caller's) */
    for (i = 0; i < 2u; i++) {                                   /* the ground: its colour x a target, inside the user clip */
        pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)AO4_SIZE, (int)AO4_SIZE, map_mem[i], PVR_FILTER_BILINEAR);
        c.gen.culling = PVR_CULLING_NONE; c.gen.clip_mode = PVR_USERCLIP_INSIDE;
        c.txr.env = PVR_TXRENV_MODULATE; c.txr.uv_clamp = PVR_UVCLAMP_UV;
        pvr_poly_compile(&h_ground[i], &c);
    }
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                      /* the shapes and walls, inside the user clip */
    c.gen.culling = PVR_CULLING_NONE; c.gen.clip_mode = PVR_USERCLIP_INSIDE;
    pvr_poly_compile(&h_obj, &c);
    for (i = 0; i < ROOM_SURFS; i++) {                           /* a lightmap: texel x base + offset, inside the user clip */
        unsigned e;

        for (e = 0; e < 2u; e++) {
            pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)room_surf[i].w, (int)room_surf[i].h, lm_mem[i][e], PVR_FILTER_BILINEAR);
            c.gen.culling = PVR_CULLING_NONE; c.gen.clip_mode = PVR_USERCLIP_INSIDE; c.gen.specular = 1;
            c.txr.env = PVR_TXRENV_MODULATE; c.txr.uv_clamp = PVR_UVCLAMP_UV;
            pvr_poly_compile(&h_lm[i][e], &c);
        }
    }
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)AO4_HEN_ATLAS, (int)AO4_HEN_ATLAS, m_mem, PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE; c.gen.clip_mode = PVR_USERCLIP_INSIDE;   /* a mover's quad: what is there x the tile */
    c.blend.src = PVR_BLEND_DESTCOLOR; c.blend.dst = PVR_BLEND_ZERO;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.txr.env = PVR_TXRENV_REPLACE; c.txr.alpha = PVR_TXRALPHA_DISABLE; c.txr.uv_clamp = PVR_UVCLAMP_UV;
    pvr_poly_compile(&h_mq, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_mdummy, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, (int)TEXT_W, (int)TEXT_H, text_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_text, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                      /* the strips under the text, the dividing line */
    c.gen.culling = PVR_CULLING_NONE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_dim, &c);
}

/* The user clip rectangle, in tiles of 32 pixels, both ends counted: what the headers with PVR_USERCLIP_INSIDE keep.
   A change of user clip takes effect one polygon too soon on every revision of the chip (Sega's bug list, CORE 18;
   wiki/research/pvr-bugs-as-features.md C18), so a polygon that does not matter goes first and takes the early
   change: a triangle with area, off the screen's left edge. */
static void user_clip(const pvr_poly_hdr_t *dummy, unsigned tx0, unsigned ty0, unsigned tx1, unsigned ty1) {
    static uint32_t cmd[8] __attribute__((aligned(32)));

    flush();
    if (clip_guard) {
        pvr_prim((void *)(uintptr_t)dummy, sizeof *dummy);
        put(0, -64.0f, 8.0f, 0.001f, 0.0f, 0.0f, 0xff000000u);
        put(0, -64.0f, 40.0f, 0.001f, 0.0f, 0.0f, 0xff000000u);
        put(1, -32.0f, 8.0f, 0.001f, 0.0f, 0.0f, 0xff000000u);
        flush();
    }
    cmd[0] = PVR_CMD_USERCLIP; cmd[1] = cmd[2] = cmd[3] = 0u;
    cmd[4] = tx0; cmd[5] = ty0; cmd[6] = tx1; cmd[7] = ty1;
    pvr_prim(cmd, sizeof cmd);
}

/* ---- the text: six lines of 64 characters in a texture ------------------------------------- */

static void text_set(unsigned line, const char *fmt, ...) {
    char s[65];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s, sizeof s, fmt, ap);
    va_end(ap);
    if (strcmp(s, text_ln[line])) { strcpy(text_ln[line], s); text_dirty = 1; }
}

/* Draw the lines into RAM (white, alpha = the glyph's cover) and load the texture. Call with no render running. */
static void text_upload(void) {
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

/* Columns c0 .. c1 of a text line at (x, y), on a dark strip. */
static void text_put(unsigned line, unsigned c0, unsigned c1, float x, float y, int *hdr) {
    const float u0 = (float)(c0 * 8u) * (1.0f / (float)TEXT_W), u1 = (float)(c1 * 8u) * (1.0f / (float)TEXT_W);
    const float v0 = (float)(line * 16u) * (1.0f / (float)TEXT_H), v1 = (float)(line * 16u + 16u) * (1.0f / (float)TEXT_H);

    if (*hdr != 0) { flush(); pvr_prim(&h_dim, sizeof h_dim); *hdr = 0; }
    rect(x - 2.0f, y - 1.0f, x + (float)((c1 - c0) * 8u) + 2.0f, y + 17.0f, 10.0f, 0, 0, 0, 0, 0xa0000000u);
    flush(); pvr_prim(&h_text, sizeof h_text); *hdr = 1;
    rect(x, y, x + (float)((c1 - c0) * 8u), y + 16.0f, 11.0f, u0, v0, u1, v1, 0xffffffffu);
}

/* ---- the effect labels (../psm_effects.h): text lines 4 and 5, the Realtime words in yellow ------------- */

static fx_text_t fx_now;                    /* the labels of the view on screen (labels() makes them) */
static int fx_live;                         /* 0 during the measured stages: two lines of blended pixels are fill */
static int fx_hidden;                       /* d-pad up held + B */

static void fx_put(unsigned l, float x, float y, int *hdr) {
    const unsigned n = (unsigned)strlen(fx_now.line[l]);
    const float v0 = (float)((4u + l) * 16u) * (1.0f / (float)TEXT_H), v1 = (float)((4u + l) * 16u + 16u) * (1.0f / (float)TEXT_H);
    unsigned c0 = 0, c;

    if (!n) return;
    if (*hdr != 0) { flush(); pvr_prim(&h_dim, sizeof h_dim); *hdr = 0; }
    rect(x - 2.0f, y - 1.0f, x + (float)(n * 8u) + 2.0f, y + 17.0f, 10.0f, 0, 0, 0, 0, 0xa0000000u);
    flush(); pvr_prim(&h_text, sizeof h_text); *hdr = 1;
    for (c = 1; c <= n; c++)
        if (c == n || fx_marked(&fx_now, l, c) != fx_marked(&fx_now, l, c0)) {
            rect(x + (float)(c0 * 8u), y, x + (float)(c * 8u), y + 16.0f, 11.0f, (float)(c0 * 8u) * (1.0f / (float)TEXT_W), v0, (float)(c * 8u) * (1.0f / (float)TEXT_W), v1,
                 fx_marked(&fx_now, l, c0) ? 0xffffdc00u : 0xffffffffu);
            c0 = c;
        }
}

/* ---- scenes on the PVR --------------------------------------------------------------- */

static uint64_t last_render_ns;
static uint32_t last_vbuf;
static asic_evt_handler_entry_t rd_prev;
static volatile uint32_t rd_seen;
static int rd_hooked;
static volatile uint32_t ta_evt[3];
static int ta_hooked[3];
static asic_evt_handler_entry_t ta_prev[3];
static const uint16_t ta_code[3] = { ASIC_EVT_PVR_PARAM_OUTOFMEM, ASIC_EVT_PVR_OPB_OUTOFMEM, ASIC_EVT_PVR_ISP_OUTOFMEM };

static void rd_evt(uint32_t code, void *data) {
    (void)data;
    if (rd_prev.hdl) rd_prev.hdl(code, rd_prev.data);
    rd_seen = 1u;
}

static void ta_evt_fn(uint32_t code, void *data) {
    const unsigned i = (unsigned)(uintptr_t)data;

    if (ta_prev[i].hdl) ta_prev[i].hdl(code, ta_prev[i].data);
    ta_evt[i]++;
}

static int wait_ready(void) {
    const uint64_t t0 = ns();

    while (pvr_check_ready() < 0) if (ns() - t0 > LIMIT_NS) return 0;
    return 1;
}

static int scene_open(int target) {         /* -1: the screen; 0, 1: a shadow target; 2: the movers' atlas */
    if (!wait_ready()) return 0;
    if (target == 2) return pvr_scene_begin_rtt(m_mem, AO4_HEN_ATLAS, AO4_HEN_ATLAS + 2u, AO4_HEN_ATLAS) >= 0;
    if (target >= 0) return pvr_scene_begin_rtt(map_mem[target], AO4_SIZE, AO4_SIZE + 2u, AO4_SIZE) >= 0;
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
    last_vbuf = (uint32_t)st.vtx_buffer_used;
    sh4der_tilecut_find(&tcut);                   /* the table this render went through, until both are known */
    return 1;
}

/* ---- the state and a frame's numbers ----------------------------------------------------- */

typedef struct view {
    unsigned scene, split, look, ki, reach;
    int swap, exaggerate, hens;             /* hens: the room's four are walking */
    int mq;                                 /* 3+M draws its target and its quads (0: the run's stage that leaves them out) */
    int cut, still;                         /* target M's pass through its own tiles; the hens stand at their start (the cut's two stages) */
    int marks;                              /* a mark as each half's last polygon (the clip guard's test) */
    unsigned div;                           /* the divider: the tile column where the right half starts (sh4der_split.h); 10 in the run that counts */
    float turn, lift, t;
} view_t;

static const uint8_t k_of[3] = { 2, 4, 8 };
static const uint8_t split_n[N_SCENES] = { 3, 3, 7 }, split_first[N_SCENES] = { 2, 2, 1 };
static const uint8_t split_lr[N_SCENES][7][2] = {
    { { AO4_OFF, AO4_BAKED }, { AO4_OFF, AO4_MAP }, { AO4_BAKED, AO4_MAP }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
    { { AO4_OFF, AO4_BAKED }, { AO4_OFF, AO4_MAP }, { AO4_BAKED, AO4_MAP }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
    { { AO4_OFF, AO4_BAKED }, { AO4_BAKED, AO4_LMAP }, { AO4_OFF, AO4_LMAP }, { AO4_LMAP, AO4_MAP },
      { AO4_LMAP, AO4_LMAP_M }, { AO4_LMAP_M, AO4_MAP }, { AO4_LMAP_M, AO4_LMAP_M } }
};
static const char *const method_name[5] = { "AO OFF", "1 BAKED: a byte a vertex", "2 PVR MAP: ground only", "3 LIGHTMAP: a texture a surface",
                                            "3+M LIGHTMAP + a tile a hen" };
static const char *const look_name[AO4_LOOKS] = { "CLAY", "COLOUR", "AO ONLY" };

typedef struct cost {
    uint64_t place, light[3], sun, sky, pvr_s, pvr_a, pvr_scr, frame;   /* ns; light by method: OFF, BAKED, (unused) */
    uint64_t hens;                          /* ns: the hens' walk and placing */
    uint64_t mv, pvr_m;                     /* ns: the send of the movers' tiles; the PVR's render of target M */
    unsigned mv_v, mq_px;                   /* vertices sent to target M; the quads' area on the screen, pixels (before the clip to a half) */
    uint32_t vbuf_m;
    int have_m;
    uint64_t cut_ns;                        /* ns: the tile tables' rewrite and restore round target M's pass */
    unsigned cut_on, cut_refused, cut_bad;  /* the pass was cut; the cut was refused (no table, or not as read); words not restored */
    uint32_t m_sum;                         /* target M's texels summed (the cut's stages) */
    int have_sum;
    unsigned sun_v, sky_v, scr_v;
    unsigned side_v[2], side_b[2];          /* a half of the screen: vertices sent, TA bytes estimated */
    uint32_t vbuf_s, vbuf_a, vbuf_scr;
    int have_a, have_baked;
} cost_t;

static void emit_tri(void *user, const float a[2], const float b[2], const float c[2], float z, uint32_t argb, int blend) {
    (void)user; (void)blend;
    put(0, a[0], a[1], z, 0.0f, 0.0f, argb);
    put(0, b[0], b[1], z, 0.0f, 0.0f, argb);
    put(1, c[0], c[1], z, 0.0f, 0.0f, argb);
}

/* A shadow target: white, the sun's casters, a white border; with_sky: then the K sky directions. */
static int map_pass(const ao4_scene_t *s, const view_t *v, int with_sky, cost_t *c) {
    const float n = (float)AO4_SIZE;
    uint64_t t0;

    if (!scene_open(with_sky)) return 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(&h_flat, sizeof h_flat);
    rect(0.0f, 0.0f, n, n + 2.0f, 0.1f, 0, 0, 0, 0, 0xffffffffu);
    flush();
    t0 = ns(); sent = 0;
    ao4_draw_sun(s, v->look, emit_tri, NULL);
    flush();
    if (!with_sky) { c->sun = ns() - t0; c->sun_v = sent; }
    rect(0.0f, 0.0f, n, 1.0f, 2.0f, 0, 0, 0, 0, 0xffffffffu);
    rect(0.0f, n - 1.0f, n, n + 2.0f, 2.0f, 0, 0, 0, 0, 0xffffffffu);
    rect(0.0f, 0.0f, 1.0f, n, 2.0f, 0, 0, 0, 0, 0xffffffffu);
    rect(n - 1.0f, 0.0f, n, n, 2.0f, 0, 0, 0, 0, 0xffffffffu);
    flush();
    pvr_list_finish();
    if (with_sky) {
        pvr_list_begin(PVR_LIST_TR_POLY);
        t0 = ns(); sent = 0;
        pvr_prim(&h_ao, sizeof h_ao);
        ao4_draw_sky(s, k_of[v->ki], v->exaggerate, ao4_reach[v->reach], emit_tri, NULL);
        flush();
        c->sky = ns() - t0; c->sky_v = sent;
        pvr_list_finish();
    }
    if (!scene_close()) return 0;
    if (with_sky) { c->pvr_a = last_render_ns; c->vbuf_a = last_vbuf; }
    else { c->pvr_s = last_render_ns; c->vbuf_s = last_vbuf; }
    return 1;
}

/* Target M: white, then each hen's caster from the K sky directions into its tile. */
static int m_pass_(const ao4_scene_t *s, const view_t *v, cost_t *c) {
    const float n = (float)AO4_HEN_ATLAS;
    uint64_t t0;

    if (!scene_open(2)) return 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(&h_flat, sizeof h_flat);
    rect(0.0f, 0.0f, n, n + 2.0f, 0.1f, 0, 0, 0, 0, 0xffffffffu);
    flush();
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    t0 = ns(); sent = 0;
    pvr_prim(&h_ao, sizeof h_ao);
    ao4_hens_tiles(s, &hens, k_of[v->ki], v->exaggerate, ao4_reach[v->reach], emit_tri, NULL);
    flush();
    c->mv = ns() - t0; c->mv_v = sent;
    pvr_list_finish();
    if (!scene_close()) return 0;
    c->pvr_m = last_render_ns; c->vbuf_m = last_vbuf;
    return 1;
}

/* Target M's pass, through the screen's whole tile table or (v->cut) one cut to the target's tiles. No render is
   running here: every pass before it waited for its render to end. */
static int m_pass(const ao4_scene_t *s, const view_t *v, cost_t *c, unsigned frame_no) {
    uint64_t t0 = ns();
    int did = 0, ok;

    if (v->cut) {
        did = sh4der_tilecut_begin(&tcut, AO4_HEN_ATLAS / 32u, (AO4_HEN_ATLAS + 2u + 31u) / 32u);
        c->cut_ns = ns() - t0;
        c->cut_on = (unsigned)did; c->cut_refused = (unsigned)!did;
    }
    ok = m_pass_(s, v, c);
    if (did) {
        t0 = ns();
        c->cut_bad = sh4der_tilecut_end(&tcut);
        c->cut_ns += ns() - t0;
        if (c->cut_bad) { cut_fatal = 1; return 0; }
    }
    if (ok && v->still && (frame_no & 15u) == 0u) {     /* the target's texels, read back: the same hens give the same sum */
        const volatile uint16_t *px = (const volatile uint16_t *)m_mem;
        uint32_t sum = 0;
        unsigned i;

        for (i = 0; i < AO4_HEN_ATLAS * AO4_HEN_ATLAS; i++) sum = sum * 31u + px[i];
        c->m_sum = sum; c->have_sum = 1;
    }
    return ok;
}

/* The hens' quads on the floor, in the translucent list. Returns their area on the screen in pixels. */
static unsigned m_quads(const ao4_cam_t *cam) {
    static const uint8_t order[4] = { 0, 1, 3, 2 };
    float area = 0.0f;
    unsigned k, c;

    pvr_prim(&h_mq, sizeof h_mq);
    for (k = 0; k < AO4_HENS; k++) {
        float sp[4][3];

        for (c = 0; c < 4u; c++) {
            float p[3], uv[2];

            ao4_hen_quad(&hens, k, order[c], p, uv);
            ao4_cam_pt(cam, p, sp[c]);
            put(c == 3u, sp[c][0], sp[c][1], sp[c][2], uv[0], uv[1], 0xffffffffu);
        }
        area += sh4der_abs_((sp[1][0] - sp[0][0]) * (sp[2][1] - sp[0][1]) - (sp[2][0] - sp[0][0]) * (sp[1][1] - sp[0][1]));   /* the strip's two triangles */
    }
    flush();
    return (unsigned)area;
}

/* The TA's bytes for a strip of n vertices of `words` words each: pieces of up to six triangles, each its 3-word
   header and its vertices (../psm_send.h's estimate). */
static inline unsigned strip_bytes(unsigned n, unsigned words) {
    const unsigned t = n - 2u, p = (t + 5u) / 6u;

    return 12u * p + (t + 2u * p) * 4u * words;
}

/* One half of the screen in `argb`: the ground through target `tgt`, then the rest. method AO4_LMAP (the room): each
   flat surface as one quad with its lightmap instead of its strips. *bytes gets the TA estimate. */
static void half(const ao4_scene_t *s, const uint32_t *argb, unsigned tgt, unsigned method, const view_t *v, const ao4_cam_t *cam, unsigned *bytes, unsigned side) {
    unsigned st = 0, i, k = 0, b = 0;

    if (method >= AO4_LMAP) {
        for (; st < s->surf_strips; st++) k += s->slen[st];
        for (i = 0; i < ROOM_SURFS; i++) {
            static const uint8_t order[4] = { 0, 1, 3, 2 };     /* a strip of the four corners */
            uint32_t base, offset;
            unsigned c;

            ao4_room_colours(s, i, v->look, &base, &offset);
            pvr_prim(&h_lm[i][v->exaggerate ? 1 : 0], sizeof h_lm[0][0]);
            cur_oargb = offset;
            for (c = 0; c < 4u; c++) {
                float p[3], uv[2], sp[3];

                ao4_room_quad(i, order[c], p, uv);
                ao4_cam_pt(cam, p, sp);
                put(c == 3u, sp[0], sp[1], sp[2], uv[0], uv[1], base);
            }
            flush();
            cur_oargb = 0u;
            b += strip_bytes(4u, 7u);
        }
    } else {
        pvr_prim(&h_ground[tgt], sizeof h_ground[0]);
        for (; st < s->ground_strips; st++) {
            for (i = 0; i < s->slen[st]; i++, k++) {
                const unsigned x = s->idx[k];

                put(i + 1u == s->slen[st], scr[x][0], scr[x][1], scr[x][2], ao4_ground_u(s->p[x][0]), ao4_ground_v(s->p[x][2]), argb[x]);
            }
            b += strip_bytes(s->slen[st], 6u);
        }
        flush();
    }
    pvr_prim(&h_obj, sizeof h_obj);
    for (; st < s->strips; st++) {
        for (i = 0; i < s->slen[st]; i++, k++) {
            const unsigned x = s->idx[k];

            put(i + 1u == s->slen[st], scr[x][0], scr[x][1], scr[x][2], 0.0f, 0.0f, argb[x]);
        }
        b += strip_bytes(s->slen[st], 4u);
    }
    flush();
    if (v->marks) {                         /* this half's last polygon: magenta on the left of the line, cyan on the right */
        if (side) rect(336.0f, 96.0f, 368.0f, 128.0f, 5.0f, 0, 0, 0, 0, 0xff00ffffu);
        else rect(272.0f, 96.0f, 304.0f, 128.0f, 5.0f, 0, 0, 0, 0, 0xffff00ffu);
        flush();
    }
    *bytes = b;
}

/* A whole frame. 0 when a pass did not come back. */
static int frame(const view_t *v, cost_t *c, unsigned frame_no) {
    const ao4_scene_t *s = &scenes[v->scene];
    const unsigned ml = split_lr[v->scene][v->split][v->swap ? 1 : 0], mr = split_lr[v->scene][v->split][v->swap ? 0 : 1];
    const unsigned method[2] = { ml, mr };
    const uint64_t tf = ns();
    sh4der_split_half_t hf[2];
    ao4_cam_t cam;
    uint64_t t0;
    unsigned side, i;
    int hdr = -1;

    memset(c, 0, sizeof *c);
    sh4der_split_halves(v->div, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &hf[0], &hf[1]);   /* a half the divider hides is not lit and not sent */
    c->hens = hens_ns;
    t0 = ns();
    ao4_cam_at(s, &cam, v->t, v->turn, v->lift);
    for (i = 0; i < s->vn; i++) ao4_cam_pt(&cam, s->p[i], scr[i]);
    c->place = ns() - t0;
    for (side = 0; side < 2u; side++) {
        const unsigned kind = method[side] == AO4_BAKED || method[side] >= AO4_LMAP ? 1u : 0u;

        if (hf[side].hidden) continue;
        t0 = ns();
        ao4_light(s, method[side], v->look, v->exaggerate, col[side]);
        c->light[kind] = ns() - t0;
        if (kind) c->have_baked = 1;
    }
    if (!c->light[0] || (frame_no & 31u) == 0u) {      /* no side is lit without the byte this frame: time that once in a while */
        t0 = ns();
        ao4_light(s, AO4_OFF, v->look, v->exaggerate, col_ref);
        c->light[0] = ns() - t0;
    }
    if (!map_pass(s, v, 0, c)) return 0;
    if ((ml == AO4_MAP && !hf[0].hidden) || (mr == AO4_MAP && !hf[1].hidden)) { c->have_a = 1; if (!map_pass(s, v, 1, c)) return 0; }
    if (((ml == AO4_LMAP_M && !hf[0].hidden) || (mr == AO4_LMAP_M && !hf[1].hidden)) && v->hens && v->mq) { c->have_m = 1; if (!m_pass(s, v, c, frame_no)) return 0; }

    if (!scene_open(-1)) return 0;
    sent = 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    for (side = 0; side < 2u; side++) {     /* at the middle: tiles 0 .. 9 and 10 .. 19, as before the divider moved */
        if (hf[side].hidden) continue;
        user_clip(&h_obj, hf[side].tx0, hf[side].ty0, hf[side].tx1, hf[side].ty1);
        i = sent;
        half(s, col[side], method[side] == AO4_MAP ? 1u : 0u, method[side], v, &cam, &c->side_b[side], side);
        c->side_v[side] = sent - i;
    }
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    if (c->have_m) {                        /* the movers' quads, a half at a time */
        for (side = 0; side < 2u; side++)
            if (method[side] == AO4_LMAP_M && !hf[side].hidden) { user_clip(&h_mdummy, hf[side].tx0, hf[side].ty0, hf[side].tx1, hf[side].ty1); c->mq_px = m_quads(&cam); }
    }
    pvr_prim(&h_dim, sizeof h_dim); hdr = 0;
    if (!sh4der_split_at_end(v->div, SH4DER_SPLIT_COLS)) rect(sh4der_split_x(v->div) - 1.0f, 0.0f, sh4der_split_x(v->div) + 1.0f, 480.0f, 10.0f, 0, 0, 0, 0, 0xff101010u);
    text_put(0u, 0u, 31u, 68.0f, 52.0f, &hdr);
    text_put(0u, 32u, 63u, 328.0f, 52.0f, &hdr);
    text_put(1u, 0u, 31u, 68.0f, 372.0f, &hdr);
    text_put(1u, 32u, 63u, 328.0f, 372.0f, &hdr);
    text_put(2u, 0u, 63u, 64.0f, 392.0f, &hdr);
    text_put(3u, 0u, 63u, 64.0f, 412.0f, &hdr);
    if (fx_live && !fx_hidden) {            /* above the costs; one line sits on the lower row */
        unsigned l;

        for (l = 0; l < fx_now.lines; l++) fx_put(l, 64.0f, 352.0f - 20.0f * (float)(fx_now.lines - 1u - l), &hdr);
    }
    flush();
    pvr_list_finish();
    c->scr_v = sent;
    if (!scene_close()) return 0;
    c->pvr_scr = last_render_ns; c->vbuf_scr = last_vbuf;
    c->frame = ns() - tf;
    return 1;
}

/* The text of this view and these costs (the costs: a mean the caller keeps). */
static int run_stage = -1;                  /* the run's stage in hand; -1: the live view */
static int pad_stop;                        /* a button during the run: the run ends there and the live view opens */
static int pad_from = -1;                   /* the stage the live view then starts as */

/* The pad during the run: 1 at a button or a trigger newly pressed (the stick does not count). The first call takes
   what is held as old, so the menu's A does not count. */
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

static void labels(const view_t *v, const cost_t *c) {
    const unsigned ml = split_lr[v->scene][v->split][v->swap ? 1 : 0], mr = split_lr[v->scene][v->split][v->swap ? 0 : 1];
    const unsigned method[2] = { ml, mr };
    const int room = v->scene == SCENE_ROOM;
    char half_cost[2][33], tail[40];
    sh4der_split_half_t hf[2];
    unsigned side;

    text_set(0u, "%-32.32s%-32.32s", method_name[ml], method_name[mr]);
    sh4der_split_halves(v->div, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &hf[0], &hf[1]);
    for (side = 0; side < 2u; side++) {
        if (hf[side].hidden) snprintf(half_cost[side], sizeof half_cost[0], "half hidden: not sent");
        else if (room && method[side] != AO4_MAP && method[side] != AO4_LMAP_M)   /* the room: what a half sends is the comparison */
            snprintf(half_cost[side], sizeof half_cost[0], "%u vertices, TA %u KB est", c->side_v[side], (c->side_b[side] + 512u) / 1024u);
        else if (method[side] == AO4_OFF)
            snprintf(half_cost[side], sizeof half_cost[0], "SH4 %u+%u us  PVR map %u us", us_of(c->light[0]), us_of(c->sun), us_of(c->pvr_s));
        else if (method[side] == AO4_LMAP_M)
            snprintf(half_cost[side], sizeof half_cost[0], "M adds SH4 %u us  PVR pass %u", us_of(c->mv), us_of(c->pvr_m));
        else if (method[side] == AO4_BAKED)
            snprintf(half_cost[side], sizeof half_cost[0], "adds SH4 %d us  PVR 0", (int)us_of(c->light[1]) - (int)us_of(c->light[0]));
        else
            snprintf(half_cost[side], sizeof half_cost[0], "adds SH4 %u us  PVR %d us", us_of(c->sky), (int)us_of(c->pvr_a) - (int)us_of(c->pvr_s));
    }
    text_set(1u, "%-32.32s%-32.32s", half_cost[0], half_cost[1]);
    tail[0] = 0;
    if (room && v->hens) snprintf(tail, sizeof tail, " HENS %u tri +%u us", (unsigned)AO4_HEN_TRIS, us_of(c->hens));
    else if (room) snprintf(tail, sizeof tail, " HENS off");
    text_set(2u, "SCENE %c %s%s K%u reach%u%% %u.%u ms%s", 'A' + v->scene, look_name[v->look], v->exaggerate ? " x2" : "", k_of[v->ki],
             (unsigned)(ao4_reach[v->reach] * 100.0f + 0.5f), us_of(c->frame) / 1000u, us_of(c->frame) % 1000u / 100u, tail);
    if (run_stage >= 0) text_set(3u, "MEASURING stage %u of 19: any button = live view. GUARD %s", (unsigned)run_stage + 1u, clip_guard ? "ON" : "OFF");
    else if (room) text_set(3u, "X pair Y swap A x2 B look LT/RT div L+R sc up K dn hen GUARD %s", clip_guard ? "ON" : "OFF");
    else text_set(3u, "X pair Y swap A x2 B look LT/RT div L+R sc pad K,reach GUARD %s", clip_guard ? "ON" : "OFF");
    {                                       /* the effect labels: what each half shows now (ao_fx.h says where each is read from) */
        const int sun = !scenes[v->scene].no_sun_shadow && v->look != AO4_AO_ONLY, movers = room && v->hens && v->mq;
        fx_entry_t fl[3], fr[3];
        const unsigned nl = ao4_fx_half(ml, sun, movers, fl), nr = ao4_fx_half(mr, sun, movers, fr);

        if (hf[0].hidden) fx_format(&fx_now, fr, nr);           /* one half alone on the screen: its own effects, no L / R */
        else if (hf[1].hidden) fx_format(&fx_now, fl, nl);
        else fx_format_split(&fx_now, fl, nl, fr, nr);
        text_set(4u, "%s", fx_now.line[0]);
        text_set(5u, "%s", fx_now.line[1]);
    }
}

/* ---- start, stop ------------------------------------------------------------------- */

static int start(void) {
    pvr_init_params_t init = pvr_default_params;
    unsigned i;

    init.opb_sizes[PVR_LIST_OP_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_OP_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_TR_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_PT_POLY] = PVR_BINSIZE_0;
    init.vertex_buf_size = 768 * 1024;
    init.autosort_disabled = 1;
    init.opb_overflow_count = 4;
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return 0; }
    pvr_set_bg_color(AO4_BACKDROP_R, AO4_BACKDROP_G, AO4_BACKDROP_B);
    map_mem[0] = pvr_mem_malloc(AO4_SIZE * MAP_ROWS * 2u);
    map_mem[1] = pvr_mem_malloc(AO4_SIZE * MAP_ROWS * 2u);
    m_mem = pvr_mem_malloc(AO4_HEN_ATLAS * M_ROWS * 2u);
    text_mem = pvr_mem_malloc(sizeof text_px);
    if (!map_mem[0] || !map_mem[1] || !text_mem || !m_mem) { say("video memory allocation FAIL"); pvr_shutdown(); return 0; }
    for (i = 0; i < ROOM_SURFS; i++) {      /* the room's lightmaps, as baked and with the occlusion doubled */
        const unsigned bytes = (unsigned)room_surf[i].w * room_surf[i].h * 2u;
        unsigned e;

        for (e = 0; e < 2u; e++) {
            lm_mem[i][e] = pvr_mem_malloc(bytes);
            if (!lm_mem[i][e]) { say("video memory allocation FAIL (lightmap %u)", i); pvr_shutdown(); return 0; }
            ao4_room_lm565(i, (int)e, lm_px);
            pvr_txr_load(lm_px, lm_mem[i][e], bytes);
        }
    }
    headers();
    memset(text_ln, 0, sizeof text_ln);
    text_dirty = 1;
    text_upload();
    vid_set_dithering(false);
    rd_prev = asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, rd_evt, NULL);
    rd_hooked = rd_prev.hdl != NULL;
    sh4der_tilecut_init(&tcut, 20u, 15u);         /* the screen's tiles: 640 / 32 by 480 / 32; the tables are found as scenes render */
    clip_guard = 1; cut_fatal = 0;
    if (!rd_hooked) asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, NULL, NULL);
    for (i = 0; i < 3u; i++) {
        ta_prev[i] = asic_evt_set_handler(ta_code[i], ta_evt_fn, (void *)(uintptr_t)i);
        if (!ta_prev[i].hdl) asic_evt_enable(ta_code[i], ASIC_IRQ_DEFAULT);
        ta_hooked[i] = 1;
    }
    return 1;
}

static void stop(const char *stage) {
    unsigned i;

    step(stage);
    if (rd_hooked) { asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, rd_prev.hdl, rd_prev.data); rd_hooked = 0; }
    for (i = 0; i < 3u; i++) {
        if (!ta_hooked[i]) continue;
        if (ta_prev[i].hdl) asic_evt_set_handler(ta_code[i], ta_prev[i].hdl, ta_prev[i].data);
        else { asic_evt_disable(ta_code[i], ASIC_IRQ_DEFAULT); asic_evt_remove_handler(ta_code[i]); }
        ta_hooked[i] = 0;
    }
    vid_set_dithering(true);
    pvr_shutdown();
}

/* ---- the loop: the run that counts, and the live view ------------------------------------------ */

static void view_default(view_t *v) {
    memset(v, 0, sizeof *v);
    v->split = split_first[0]; v->ki = 1u; v->hens = 1; v->mq = 1; v->reach = AO4_REACH_DEFAULT; v->look = AO4_CLAY;
    v->div = sh4der_split_middle(SH4DER_SPLIT_COLS);
}

typedef struct sums {
    cost_t c;                               /* sums over n frames */
    unsigned n, n_a, n_baked, n_m;
    int guard;                              /* the clip guard's state in this stage */
} sums_t;

static void sums_add(sums_t *s, const cost_t *c) {
    s->c.place += c->place; s->c.light[0] += c->light[0]; s->c.sun += c->sun; s->c.pvr_s += c->pvr_s; s->c.pvr_scr += c->pvr_scr; s->c.frame += c->frame;
    s->c.sun_v = c->sun_v; s->c.scr_v = c->scr_v; s->c.vbuf_s = c->vbuf_s; s->c.vbuf_scr = c->vbuf_scr; s->c.hens += c->hens;
    s->c.side_v[0] = c->side_v[0]; s->c.side_v[1] = c->side_v[1]; s->c.side_b[0] = c->side_b[0]; s->c.side_b[1] = c->side_b[1];
    s->n++;
    if (c->have_baked) { s->c.light[1] += c->light[1]; s->n_baked++; }
    s->c.cut_ns += c->cut_ns; s->c.cut_on += c->cut_on; s->c.cut_refused += c->cut_refused; s->c.cut_bad += c->cut_bad;
    if (c->have_sum) { s->c.m_sum = c->m_sum; s->c.have_sum = 1; }
    if (c->have_m) { s->c.mv += c->mv; s->c.pvr_m += c->pvr_m; s->c.mv_v = c->mv_v; s->c.mq_px += c->mq_px; s->c.vbuf_m = c->vbuf_m; s->n_m++; }
    if (c->have_a) { s->c.sky += c->sky; s->c.pvr_a += c->pvr_a; s->c.sky_v = c->sky_v; s->c.vbuf_a = c->vbuf_a; s->n_a++; }
}

static void sums_mean(const sums_t *s, cost_t *m) {
    const unsigned n = s->n ? s->n : 1u, na = s->n_a ? s->n_a : 1u, nb = s->n_baked ? s->n_baked : 1u;

    *m = s->c;
    m->hens /= n; m->place /= n; m->light[0] /= n; m->sun /= n; m->pvr_s /= n; m->pvr_scr /= n; m->frame /= n;
    m->light[1] /= nb; m->sky /= na; m->pvr_a /= na;
    if (s->c.cut_on) m->cut_ns /= s->c.cut_on;
    if (s->n_m) { m->mv /= s->n_m; m->pvr_m /= s->n_m; m->mq_px /= s->n_m; }
}

/* the run that counts, a stage: scene, split, hens, 3+M's target and quads, the tile cut, the hens still, the clip
   guard, the marks, seconds. The clip guard's two stages come after the costs', the tile cut's two LAST. */
typedef struct stage { uint8_t scene, split, hens, mq, cut, still, guard, marks, secs; } stage_t;
#define AUTO_STAGES 19u
#define STAGE_GUARD 15u
#define STAGE_CUT   17u
static const stage_t auto_stage[AUTO_STAGES] = {
    { 0, 0, 0, 1, 0, 0, 1, 0, AO4_AUTO_S }, { 0, 1, 0, 1, 0, 0, 1, 0, AO4_AUTO_S }, { 0, 2, 0, 1, 0, 0, 1, 0, AO4_AUTO_S },
    { 1, 0, 0, 1, 0, 0, 1, 0, AO4_AUTO_S }, { 1, 1, 0, 1, 0, 0, 1, 0, AO4_AUTO_S }, { 1, 2, 0, 1, 0, 0, 1, 0, AO4_AUTO_S },
    { 2, 0, 1, 1, 0, 0, 1, 0, AO4_AUTO_S }, { 2, 1, 1, 1, 0, 0, 1, 0, AO4_AUTO_S }, { 2, 2, 1, 1, 0, 0, 1, 0, AO4_AUTO_S },
    { 2, 3, 1, 1, 0, 0, 1, 0, AO4_AUTO_S }, { 2, 3, 0, 1, 0, 0, 1, 0, AO4_AUTO_S },
    { 2, 4, 1, 1, 0, 0, 1, 0, AO4_AUTO_S }, { 2, 5, 1, 1, 0, 0, 1, 0, AO4_AUTO_S }, { 2, 6, 1, 1, 0, 0, 1, 0, AO4_AUTO_S }, { 2, 6, 1, 0, 0, 0, 1, 0, AO4_AUTO_S },
    { 0, 2, 0, 1, 0, 0, 0, 1, 3 }, { 0, 2, 0, 1, 0, 0, 1, 1, 3 },                       /* the clip guard: off, on */
    { 2, 6, 1, 1, 0, 1, 1, 0, AO4_AUTO_S }, { 2, 6, 1, 1, 1, 1, 1, 0, AO4_AUTO_S }      /* target M: uncut, cut */
};

static void stage_set(view_t *v, unsigned k) {
    const stage_t *g = &auto_stage[k];

    v->scene = g->scene; v->split = g->split; v->hens = g->hens; v->mq = g->mq; v->cut = g->cut; v->still = g->still; v->marks = g->marks;
    v->turn = 0.0f; v->lift = 0.0f;
    clip_guard = g->guard;
    if (g->still) ao4_hens_reset(&hens);
}

static const uint8_t auto_first[N_SCENES] = { 0, 3, 6 };
static sums_t auto_sum[AUTO_STAGES];
static int auto_ok;

/* Frames until `auto_run` is over (auto_run 1) or START is pressed (0). */
static void loop(int auto_run) {
    view_t v;
    sums_t win;
    cost_t c, shown;
    unsigned held = ~0u, frames = 0, stage = 0;
    uint64_t t_prev = ns(), t_stage = ns();
    sh4der_split_step_t trig = { 0, 0u };
    int trig_was = 1;

    view_default(&v);
    fx_live = !auto_run;
    memset(&win, 0, sizeof win);
    memset(&shown, 0, sizeof shown);
    ao4_hens_reset(&hens);
    if (auto_run) { memset(auto_sum, 0, sizeof auto_sum); stage_set(&v, 0u); }
    else if (pad_from >= 0) {               /* the live view a button opened: as the stage it stopped, with nothing of the tests left on */
        stage_set(&v, (unsigned)pad_from);
        v.cut = 0; v.still = 0; v.marks = 0; clip_guard = 1;
    }
    pad_from = -1;
    run_stage = auto_run ? 0 : -1;
    labels(&v, &shown);
    text_upload();
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;
        const uint64_t t_now = ns();
        const float dt = (float)(t_now - t_prev) * 1.0e-9f;
        int changed = 0;

        step("ao4 frame  ");
        t_prev = t_now; held = now;
        v.t += dt < 0.1f ? dt : 0.1f;
        if (st) {
            if (st->joyx > 24 || st->joyx < -24) v.turn += (float)st->joyx * (1.0f / 128.0f) * dt * 1.2f;
            if (st->joyy > 24 || st->joyy < -24) v.lift -= (float)st->joyy * (1.0f / 128.0f) * dt * 4.0f;   /* the stick up lifts the eye */
            if (v.lift < -4.5f) v.lift = -4.5f;
            if (v.lift > 7.0f) v.lift = 7.0f;
            if (scenes[v.scene].cam_swing > 0.0f) { if (v.turn > 0.6f) v.turn = 0.6f; if (v.turn < -0.6f) v.turn = -0.6f; }
        }
        if (auto_run) {
            if (pad_hit()) { pad_stop = 1; pad_from = (int)stage; break; }   /* the stage in hand keeps what it counted: the page says where the run stopped */
            if (t_now - t_stage > (uint64_t)auto_stage[stage].secs * 1000000000ull) {
                t_stage = t_now; stage++; changed = 1;
                if (stage >= AUTO_STAGES) { auto_ok = 1; break; }
                stage_set(&v, stage);
                run_stage = (int)stage;
            }
        } else {
            if ((hit & CONT_START) && frames > 10u) break;
            if (hit & CONT_X) { v.split = (v.split + 1u) % split_n[v.scene]; changed = 1; }
            if ((hit & CONT_Y) && (now & CONT_DPAD_LEFT)) { clip_guard = !clip_guard; v.marks = 1; changed = 1; }     /* left held, then Y: the clip guard */
            else if (hit & CONT_Y) { v.swap = !v.swap; changed = 1; }
            if (hit & CONT_A) { v.exaggerate = !v.exaggerate; changed = 1; }
            if ((hit & CONT_B) && (now & CONT_DPAD_UP)) fx_hide_chord(1, 1, &fx_hidden);                            /* up held, then B: the effect labels */
            else if (hit & CONT_B) { v.look = (v.look + 1u) % AO4_LOOKS; changed = 1; }
            if (hit & CONT_DPAD_UP) { v.ki = (v.ki + 1u) % 3u; changed = 1; }
            if (hit & CONT_DPAD_DOWN) {     /* the room: the hens; else K down */
                if (v.scene == SCENE_ROOM) v.hens = !v.hens;
                else v.ki = (v.ki + 2u) % 3u;
                changed = 1;
            }
            if (hit & CONT_DPAD_RIGHT) { v.reach = (v.reach + 1u) % AO4_REACHES; changed = 1; }
            if ((hit & CONT_DPAD_LEFT) && !(now & CONT_Y)) { v.reach = (v.reach + AO4_REACHES - 1u) % AO4_REACHES; changed = 1; }
            {                               /* a trigger moves the divider; both together are one more button: the next scene */
                const int lt = st && st->ltrig > 128, rt = st && st->rtrig > 128, both = lt && rt;
                const unsigned d = sh4der_split_step(&trig, v.div, SH4DER_SPLIT_COLS, lt, rt);

                if (d != v.div) { v.div = d; changed = 1; }
                if (both && !trig_was) { v.scene = (v.scene + 1u) % N_SCENES; v.split = split_first[v.scene]; v.turn = 0.0f; v.lift = 0.0f; changed = 1; }
                trig_was = both;
            }
        }
        if (changed) { memset(&win, 0, sizeof win); }
        hens_ns = 0;
        if (v.scene == SCENE_ROOM) {        /* the hens walk only while they are shown */
            ao4_hens_show(&scenes[SCENE_ROOM], &hens, v.hens);
            if (v.hens) {
                const uint64_t t0 = ns();

                if (!v.still) ao4_hens_step(&hens, dt < 0.1f ? dt : 0.1f);
                ao4_hens_place(&scenes[SCENE_ROOM], &hens);
                hens_ns = ns() - t0;
            }
        }
        if (!frame(&v, &c, frames)) {
            if (cut_fatal) say("TILE CUT: %u words of a tile table did NOT restore (frame %u): the PVR is stopped; its next start builds them anew", c.cut_bad, frames);
            else say("%s: NO RENDER END at frame %u (scene %c, split %u%s)", auto_run ? "auto" : "live", frames, 'A' + v.scene, v.split, v.cut ? ", tile cut on" : "");
            break;
        }
        frames++;
        sums_add(&win, &c);
        if (auto_run && frames > 2u) { sums_add(&auto_sum[stage], &c); auto_sum[stage].guard = clip_guard; }
        if (changed || win.n >= 30u) {      /* the text: when the view changes, and the costs' mean every 30 frames */
            if (win.n >= 30u || !shown.frame) sums_mean(&win, &shown);
            if (win.n >= 30u) memset(&win, 0, sizeof win);
            labels(&v, &shown);
            text_upload();                  /* the last render is done (scene_close): nothing reads the texture now */
        }
    }
    run_stage = -1;
}

/* ---- the run ---------------------------------------------------------------------- */

static int fault_on;

static void report(void) {
    pg_plot_t *g;
    unsigned sc, i;
    cost_t m[9], mu, mc;
    float add[AO4_SCENES][2][2];
    char rev[32];

    suite_rev_text(rev, sizeof rev);
    say("ao4 %s AO side by side. This run's numbers; us unless said.", AO4_STR(AO4_TAG));
    say("Effect labels: off in the measured stages, on in the live");
    say(" view (d-pad up held + B hides them).");
    say("%s. CLAY, K 4, reach 60%%, %u s a split", rev, AO4_AUTO_S);
    say("bake at load, 64 rays a vertex: A %u ms, B %u ms", bake_ms[0], bake_ms[1]);
    if (!auto_ok && pad_stop) say("RUN STOPPED BY THE PAD: lines of 0 or of a short stage are not whole");
    else if (!auto_ok) { say("THE RUN DID NOT FINISH: the lines below are of what ran"); }
    for (sc = 0; sc < AO4_SCENES; sc++) {
        const ao4_scene_t *s = &scenes[sc];
        const sums_t *a = &auto_sum[auto_first[sc]];

        for (i = 0; i < 3u; i++) sums_mean(&a[i], &m[i]);
        say("%c scene: %u vertices, %u triangles, %u caster triangles", 'A' + sc, s->vn, s->tris, s->ctn);
        say("%c frame ms x10: OFF|1 %u OFF|2 %u 1|2 %u (frames %u %u %u)", 'A' + sc, us_of(m[0].frame) / 100u, us_of(m[1].frame) / 100u, us_of(m[2].frame) / 100u,
            a[0].n, a[1].n, a[2].n);
        say("%c SH-4: place %u, light off %u, with the AO byte %u", 'A' + sc, us_of(m[0].place), us_of(m[0].light[0]), us_of(m[0].light[1]));
        say("%c target S: sun send %u (%u v), PVR %u, TA %uK", 'A' + sc, us_of(m[1].sun), m[1].sun_v, us_of(m[1].pvr_s), (unsigned)((m[1].vbuf_s + 512u) / 1024u));
        say("%c target A: sky send %u (%u v), PVR %u, TA %uK", 'A' + sc, us_of(m[1].sky), m[1].sky_v, us_of(m[1].pvr_a), (unsigned)((m[1].vbuf_a + 512u) / 1024u));
        say("%c screen: %u v, TA %uK, PVR %u", 'A' + sc, m[2].scr_v, (unsigned)((m[2].vbuf_scr + 512u) / 1024u), us_of(m[2].pvr_scr));
        add[sc][0][0] = (float)us_of(m[0].light[1]) - (float)us_of(m[0].light[0]); add[sc][0][1] = 0.0f;
        add[sc][1][0] = (float)us_of(m[1].sky); add[sc][1][1] = (float)us_of(m[1].pvr_a) - (float)us_of(m[1].pvr_s);
        say("%c DERIVED adds: 1 baked SH-4 %d PVR 0; 2 map SH-4 %d PVR %d", 'A' + sc, (int)add[sc][0][0], (int)add[sc][1][0], (int)add[sc][1][1]);
    }
    {                                       /* C, the room: OFF|1, 1|3, OFF|3, 3|2 with the hens, then 3|2 without */
        const sums_t *a = &auto_sum[auto_first[SCENE_ROOM]];
        unsigned lmkb = 0;

        for (i = 0; i < 9u; i++) sums_mean(&a[i], &m[i]);
        for (i = 0; i < ROOM_SURFS; i++) lmkb += (unsigned)room_surf[i].w * room_surf[i].h * 2u;
        say("C room: %u vertices, %u triangles, %u caster triangles", hens.base[0], hens.base[3], hens.base[5]);
        say("C hens: 4, +%u triangles, +%u caster tri, walk+place %u", (unsigned)AO4_HEN_TRIS, (unsigned)AO4_HEN_CTRIS, us_of(m[3].hens));
        say("C frame ms x10: OFF|1 %u 1|3 %u OFF|3 %u 3|2 %u; no hens 3|2 %u", us_of(m[0].frame) / 100u, us_of(m[1].frame) / 100u, us_of(m[2].frame) / 100u,
            us_of(m[3].frame) / 100u, us_of(m[4].frame) / 100u);
        say("C a half, 1 vertex AO: %u v, TA %uK est", m[1].side_v[0], (m[1].side_b[0] + 512u) / 1024u);
        say("C a half, 3 lightmap: %u v, TA %uK est, textures %uK", m[1].side_v[1], (m[1].side_b[1] + 512u) / 1024u, (lmkb + 512u) / 1024u);
        say("C screen PVR: OFF|1 %u 1|3 %u OFF|3 %u 3|2 %u", us_of(m[0].pvr_scr), us_of(m[1].pvr_scr), us_of(m[2].pvr_scr), us_of(m[3].pvr_scr));
        say("C screen TA K: OFF|1 %u 1|3 %u OFF|3 %u", (unsigned)((m[0].vbuf_scr + 512u) / 1024u), (unsigned)((m[1].vbuf_scr + 512u) / 1024u), (unsigned)((m[2].vbuf_scr + 512u) / 1024u));
        say("C target A: sky send %u (%u v), PVR %u, TA %uK", us_of(m[3].sky), m[3].sky_v, us_of(m[3].pvr_a), (unsigned)((m[3].vbuf_a + 512u) / 1024u));
        say("C target A, no hens: send %u (%u v), PVR %u", us_of(m[4].sky), m[4].sky_v, us_of(m[4].pvr_a));
        say("C 3+M: a %u px tile a hen (%u cm a texel) in a %u x %u target", (unsigned)AO4_HEN_TILE, (unsigned)(AO4_HEN_TILE_M * 100.0f / (float)AO4_HEN_TILE + 0.5f),
            (unsigned)AO4_HEN_ATLAS, (unsigned)AO4_HEN_ATLAS);
        say("C target M: send %u (%u v), PVR %u, TA %uK", us_of(m[7].mv), m[7].mv_v, us_of(m[7].pvr_m), (unsigned)((m[7].vbuf_m + 512u) / 1024u));
        say("C 4 quads, %u px: screen PVR with %u, without %u", m[7].mq_px, us_of(m[7].pvr_scr), us_of(m[8].pvr_scr));
        say("C frame ms x10: 3|3+M %u 3+M|2 %u 3+M|3+M %u, M off %u", us_of(m[5].frame) / 100u, us_of(m[6].frame) / 100u, us_of(m[7].frame) / 100u,
            us_of(m[8].frame) / 100u);
    }
    say("CLIP GUARD (a polygon before each change of user clip):");
    say(" ON in every stage but one: stage %u OFF (%u frames),", STAGE_GUARD + 1u, auto_sum[STAGE_GUARD].n);
    say(" stage %u ON (%u frames), scene A, 3 s each, with marks:", STAGE_GUARD + 2u, auto_sum[STAGE_GUARD + 1u].n);
    say(" magenta = the left half's last polygon, cyan the right's.");
    say(" Magenta missing while the label says OFF = CORE 18 seen.");
    for (i = 0; i < AUTO_STAGES; i++) if ((auto_sum[i].n != 0u) && (auto_sum[i].guard != (int)auto_stage[i].guard)) say(" stage %u ran with the guard %s, not as planned", i + 1u, auto_sum[i].guard ? "ON" : "OFF");
    sums_mean(&auto_sum[STAGE_CUT], &mu); sums_mean(&auto_sum[STAGE_CUT + 1u], &mc);
    say("TILE CUT, target M's pass, hens still (the LAST two stages):");
    say(" tile tables found and as read: %u of 2", tcut.n);
    if (!auto_sum[STAGE_CUT].n) say(" whole table: STAGE NOT REACHED (0 frames: the run ended before it)");
    else say(" whole table (300 tiles): PVR %u us, texel sum %08x", us_of(mu.pvr_m), (unsigned)mu.m_sum);
    if (!auto_sum[STAGE_CUT + 1u].n) say(" the cut: STAGE NOT REACHED (0 frames: the run ended before it)");
    else if (mc.cut_on) {
        say(" cut to 4 x 5 tiles: PVR %u us, texel sum %08x", us_of(mc.pvr_m), (unsigned)mc.m_sum);
        say(" picture %s; passes cut %u, refused %u", !mu.have_sum || !mc.have_sum ? "NOT SUMMED" : mu.m_sum == mc.m_sum ? "THE SAME" : "DIFFERENT", mc.cut_on, mc.cut_refused);
        say(" rewrite + restore %u us a pass; words not restored %u", us_of(mc.cut_ns), mc.cut_bad);
    } else say(" THE CUT DID NOT RUN: refused %u times (table not as read)", mc.cut_refused);
    say("TA out-of-memory events: param %u bins %u isp %u", (unsigned)ta_evt[0], (unsigned)ta_evt[1], (unsigned)ta_evt[2]);
    say("2 darkens the ground only; 1 and 3 all that was baked,");
    say("so only 2 and 3+M darken the floor under a hen");
    g = pg_plot("ao4: what a method ADDS to a frame, us", "scene and method", "us", 1);
    pg_cat(g, "A 1 baked"); pg_cat(g, "A 2 map"); pg_cat(g, "B 1 baked"); pg_cat(g, "B 2 map");
    pg_series(g, "SH-4"); for (sc = 0; sc < AO4_SCENES; sc++) { pg_point(g, (float)(sc * 2u), add[sc][0][0]); pg_point(g, (float)(sc * 2u + 1u), add[sc][1][0]); }
    pg_series(g, "PVR"); for (sc = 0; sc < AO4_SCENES; sc++) { pg_point(g, (float)(sc * 2u), add[sc][0][1]); pg_point(g, (float)(sc * 2u + 1u), add[sc][1][1]); }
}

static void bake_all(void) {
    unsigned sc;

    for (sc = 0; sc < AO4_SCENES; sc++) {
        const uint64_t t0 = ns();

        sh4der_fault_stage(&crumbs, "ao4 bake   ");
        ao4_build(&scenes[sc], sc);
        ao4_bake(&scenes[sc], beat);
        bake_ms[sc] = (unsigned)((ns() - t0) / 1000000ull);
    }
    ao4_build_room(&scenes[SCENE_ROOM]);    /* its AO came baked with it */
    ao4_hens_add(&scenes[SCENE_ROOM], &hens);
}

int main(void) {
    unsigned page = 0, pages, held = ~0u;
    int redraw = 1;

    suite_init();                           /* the suite disc: the menu's binary into RAM, before anything else */
    vid_set_mode(DM_640x480, PM_RGB565);
    pg_init();
    vid_clear(0, 0, 0);
    pg_text(PG_X0, PG_Y0, PG_WHITE, "ao4: BAKING scenes A and B's AO (64 rays a vertex) ...");
    bake_all();
    if (start()) {
        if (!fault_on) { fault_on = 1; if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused"); }
        loop(1);
        if (pad_stop) loop(0);              /* a button during the run: the live view at once, the pages after it */
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
            held = ~0u;
            redraw = 1;
        }
        else if (suite_tick(now, page, pages)) { page++; redraw = 1; }
        if (redraw) { pg_draw(page % pages, "AO ao4", suite_keys_text("START live view")); fx_page(ao4_fx_page, AO4_FX_PAGE_N); redraw = 0; }
        thd_sleep(20);
    }
    return 0;
}
