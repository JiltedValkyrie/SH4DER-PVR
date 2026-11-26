/* SH4DER-PVR: ao-lamp, the room's lamp switched on, side by side with a divider the triggers move, Dreamcast program

   demo/psm/ao/lamp_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The living room (room_data.h, its baked vertex AO, CLAY) with the town's hens walking in it, drawn twice, the left
   half of the screen one way and the right half another (the PVR's user tile clip), to show the lamp look of
   ao4_lamp.h and to read its cost off the screen:

     LAMP OFF | LAMP ON       the room as ao4 shows it | dimmed, the shade lit from inside (emission + sss), a whole
                              shade, a glow at the bulb
     SLOTS OFF | SLOTS ON     the lamp on in both: a whole shade | a slotted one (punch-through). The slots are in
                              this comparison alone: on the console (suite s7, lamp-c) a slot showed the dark room
                              through the far slot and the open bottom, and read as a hole. D-pad down draws a lit
                              inside behind the slits (a second, smaller shade in the opaque list): off in the run.
     LIGHT OFF | LIGHT ON     the lamp on in both, a whole shade: the room as dimmed | the bulb's light on the room's
                              vertices (a point light a vertex: no shadow, the shade does not block it)
     LAMP OFF | LAMP OFF      the same picture twice: the base the others are read against

   The divider (sh4der_split.h): LT / RT move it a tile column (32 pixels) a step, held they repeat. At an end one half
   is hidden and is NOT SENT: the other half then shows its own cost on the frame's time.

   The shade goes through the punch-through list, a third list for pvr_init (one more bin a tile). What that list
   costs when it is there and unused is measured: the run's first stage is on a PVR started without it, the second on
   one started with it, the same picture (the list then holds one triangle off the screen: it is never sent empty).

   It runs by itself first (LAMP_AUTO_S seconds a stage), then the result pages and a graph. START on the pages: the
   live view. ANY BUTTON OR TRIGGER DURING THE RUN ends the run there and opens the live view at once, in that
   stage's comparison (the pages after it say where the run stopped; nothing is measured while keys act).
     X  the comparison     Y  swap the halves     A  the camera near the lamp / the room's     B  the hens on / off
     LT / RT  the divider  d-pad up held + B  the effect labels on / off     stick  the camera     START  the pages
     d-pad down  the lit inside behind the slits on / off

   WHAT IS TIMED: the SH-4's parts with its cycle counter (PRFC1), the PVR's render from pvr_get_stats, a frame from
   its first line to its render's end. The effect labels (../psm_effects.h) are OFF in the measured stages.
   Waits: "ready" and "render done" are polled; no wait is longer than LIMIT_NS. Only PVR registers and video memory
   are written. The fault catcher (include/sh4der_fault.h) is in; the program goes back to the suite's menu.
   Build: python demo/psm/ao/build.py --spike lamp --tag <new tag>. NOT RUN ON A CONSOLE when this file was written:
   the punch-through list, the 1-bit texture, a divider that moves and the lamp's whole look are untested there. */

#include <kos.h>
#include <stdarg.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"
#include "../psm_suite.h"
#include "ao4_scene.h"
#include "ao4_draw.h"
#include "ao4_room.h"
#include "ao4_hens.h"
#include "ao4_lamp.h"
#include "sh4der_split.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#ifndef LAMP_TAG
#define LAMP_TAG untagged
#endif
#define LAMP_STR_(x) #x
#define LAMP_STR(x) LAMP_STR_(x)
#ifndef LAMP_AUTO_S
#define LAMP_AUTO_S 4u                      /* seconds a stage in the run that counts */
#endif
#define CYC_PER_MS 200006u                  /* 200.006 MHz, MEASURED (bench s2) */
#define CLOSE_FOCAL 3200.0f                 /* the camera near the lamp: the room's eye, a long lens aimed at the bulb (nothing gets behind the eye) */
#define PL_GAIN 0.9f                        /* the point light: its level at the bulb, and how it falls: 1 / (1 + PL_FALL d^2) */
#define PL_FALL 0.6f

/* ---- the result pages ----------------------------------------------------------- */

#define PG_PREFIX "ao-lamp"
#define PG_LINES_MAX 80u
#define PG_TOP 2u                           /* the effect labels of a page are three lines */
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

#define LIMIT_NS 200000000ull
/* KallistiOS's ns clock steps 2,502,400 ns ahead at each reload of TMU2 (MEASURED, image ray2 CLOCK.4): taken out.
   Used for the bounded waits and the stages' lengths only; every cost is in cycles. */
static inline uint64_t ns(void) { const uint64_t n = timer_ns_gettime64(); return n - n / 1000000000ull * 2502400ull; }
static inline uint64_t cyc(void) { return perf_cntr_count(PRFC1); }
static unsigned us_of(uint64_t cycles) { return (unsigned)((cycles * 1000ull + CYC_PER_MS / 2u) / CYC_PER_MS); }

/* ---- memory, headers ----------------------------------------------------------------- */

#define RUN 256u
#define TEXT_W 512u
#define TEXT_H 128u
#define TEXT_LINES 6u                       /* 4 and 5: the effect labels */

static pvr_ptr_t text_mem, slot_mem[2];     /* the shade's texture: [0] whole, [1] slotted */
static pvr_poly_hdr_t h_obj, h_shade[2], h_ptdummy, h_halo, h_trdummy, h_text, h_dim;
static pvr_vertex_t vrun[RUN] __attribute__((aligned(32)));
static unsigned run_n, sent;
static uint32_t cur_oargb;                  /* the offset colour of the vertices being sent */
static uint16_t text_px[TEXT_W * TEXT_H] __attribute__((aligned(32)));
static uint16_t slot_px[AO4_LAMP_TEX * AO4_LAMP_TEX] __attribute__((aligned(32)));
static char text_ln[TEXT_LINES][65];
static int text_dirty;
static int pt_on;                           /* this PVR was started with the punch-through list */

static ao4_scene_t room;
static ao4_hens_t hens;
static ao4_lamp_t lamp;
static int lamp_ok;
static ao4_lamp_plane_t planes[AO4_ROOM_SURF_MAX];
static uint8_t shade_strip[AO4_STRIP_MAX];  /* 1: this strip is a triangle of the model's own shade (left out where the lamp is on) */
static uint8_t halo_tri[AO4_LAMP_HALO_T][3];
static float scr[AO4_V_MAX][3], shade_scr[AO4_LAMP_V][3], halo_xyz[AO4_LAMP_HALO_V][3], halo_scr[AO4_LAMP_HALO_V][3];
static uint32_t lit[AO4_V_MAX], dimmed[2][AO4_V_MAX];   /* the room's colours: as ao4's; dimmed for the lamp, [1] with the bulb's light */
static uint32_t shade_base[AO4_LAMP_V], shade_off[AO4_LAMP_V], halo_col[AO4_LAMP_HALO_V];
static float liner_scr[AO4_LAMP_V][3];      /* the lit inside: the shade's rings at LINER of their radius */
#define LINER 0.9f

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

    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                      /* the room and the hens, inside the user clip */
    c.gen.culling = PVR_CULLING_NONE; c.gen.clip_mode = PVR_USERCLIP_INSIDE;
    pvr_poly_compile(&h_obj, &c);
    if (pt_on) {
        for (i = 0; i < 2u; i++) {                               /* the shade: texel x base + offset, the texel's alpha bit tested; no culling */
            pvr_poly_cxt_txr(&c, PVR_LIST_PT_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, (int)AO4_LAMP_TEX, (int)AO4_LAMP_TEX, slot_mem[i], PVR_FILTER_NEAREST);
            c.gen.culling = PVR_CULLING_NONE; c.gen.clip_mode = PVR_USERCLIP_INSIDE; c.gen.specular = 1;
            c.txr.env = PVR_TXRENV_MODULATE;
            pvr_poly_compile(&h_shade[i], &c);
        }
        pvr_poly_cxt_col(&c, PVR_LIST_PT_POLY);                  /* the polygon before a change of clip in the punch-through list */
        c.gen.culling = PVR_CULLING_NONE;
        pvr_poly_compile(&h_ptdummy, &c);
    }
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                      /* the halo: added, behind what is nearer, inside the user clip */
    c.gen.culling = PVR_CULLING_NONE; c.gen.clip_mode = PVR_USERCLIP_INSIDE;
    c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ONE;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_halo, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                      /* the polygon before a change of clip in the translucent list */
    c.gen.culling = PVR_CULLING_NONE;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_trdummy, &c);
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
   A change of user clip takes effect one polygon too soon on every revision of the chip (Sega's bug list, CORE 18),
   so a polygon that does not matter goes first and takes the early change: a triangle with area, off the screen's
   left edge. The sequence is ao4_spike.c's, which ran on a console with a clip that did not move (PVR-144). */
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

/* Columns c0 .. c1 - 1 of a text line at (x, y) on a dark strip; the columns `marked` says so in yellow. */
static void text_put(unsigned line, unsigned c0, unsigned c1, float x, float y, int (*marked)(unsigned line, unsigned c)) {
    const float v0 = (float)(line * 16u) * (1.0f / (float)TEXT_H), v1 = (float)(line * 16u + 16u) * (1.0f / (float)TEXT_H);
    unsigned a = c0, c;

    if (c1 <= c0) return;
    flush(); pvr_prim(&h_dim, sizeof h_dim);
    rect(x - 2.0f, y - 1.0f, x + (float)((c1 - c0) * 8u) + 2.0f, y + 17.0f, 10.0f, 0, 0, 0, 0, 0xa0000000u);
    flush(); pvr_prim(&h_text, sizeof h_text);
    for (c = c0 + 1u; c <= c1; c++)
        if (c == c1 || (marked && marked(line, c) != marked(line, a))) {
            rect(x + (float)((a - c0) * 8u), y, x + (float)((c - c0) * 8u), y + 16.0f, 11.0f, (float)(a * 8u) * (1.0f / (float)TEXT_W), v0, (float)(c * 8u) * (1.0f / (float)TEXT_W), v1,
                 marked && marked(line, a) ? 0xffffdc00u : 0xffffffffu);
            a = c;
        }
}

static fx_text_t fx_now;                    /* the labels of the view on screen */
static int fx_live, fx_hidden;              /* 0 during the measured stages; d-pad up held + B */
static int fx_mark(unsigned line, unsigned c) { return line >= 4u && fx_marked(&fx_now, line - 4u, c); }

/* ---- scenes on the PVR --------------------------------------------------------------- */

static uint64_t last_render_ns;
static uint32_t last_vbuf;
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
    return 1;
}

/* ---- the view ---------------------------------------------------------------------------- */

typedef struct view {
    unsigned mode, div;
    int swap, close, hens;
    int liner;                              /* a lit inside behind the slits of a slotted shade */
    float turn, lift, t;
} view_t;

typedef struct side { int lamp, slots, light; } side_t;

#define MODES 4u
#define MODE_BASE 3u
static const side_t mode_side[MODES][2] = {
    { { 0, 0, 0 }, { 1, 0, 0 } },           /* LAMP OFF | LAMP ON: a whole shade */
    { { 1, 0, 0 }, { 1, 1, 0 } },           /* SLOTS OFF | SLOTS ON */
    { { 1, 0, 0 }, { 1, 0, 1 } },           /* LIGHT OFF | LIGHT ON */
    { { 0, 0, 0 }, { 0, 0, 0 } },           /* LAMP OFF | LAMP OFF */
};
static const char *const mode_name[MODES][2] = {
    { "LAMP OFF", "LAMP ON" }, { "SLOTS OFF", "SLOTS ON" }, { "ROOM LIGHT OFF", "ROOM LIGHT ON" }, { "LAMP OFF", "LAMP OFF" },
};

typedef struct cost {
    uint64_t light, dim, pl, shade, halo, frame;   /* cycles: ao4's light; the dimming; the point light; the shade's colours and place; the halo's */
    uint64_t pvr;                           /* ns: the PVR's render */
    unsigned verts, pl_verts, pt_tris, halo_tris;
    uint32_t vbuf;
    int have_lamp, have_pl;
} cost_t;

/* The room's colours dimmed for the lamp look: each channel x AO4_LAMP_DIM (97 / 256). */
static void room_dim(const uint32_t *in, uint32_t *out, unsigned n) {
    unsigned i;

    for (i = 0; i < n; i++) {
        const uint32_t c = in[i];

        out[i] = 0xff000000u | ((((c >> 16) & 255u) * 97u >> 8) << 16) | ((((c >> 8) & 255u) * 97u >> 8) << 8) | ((c & 255u) * 97u >> 8);
    }
}

/* The bulb's light on the room's vertices, added to `in`: a point light a vertex, N.L over 1 + PL_FALL d^2, in the
   bulb's colour on the clay. No shadow, and the shade does not block it. */
static void room_light(const ao4_scene_t *s, const uint32_t *in, uint32_t *out) {
    const float k = PL_GAIN * AO4_CLAY_GREY * 255.0f;
    unsigned i;

    for (i = 0; i < s->vn; i++) {
        float l[3] = { lamp.bulb[0] - s->p[i][0], lamp.bulb[1] - s->p[i][1], lamp.bulb[2] - s->p[i][2] };
        const float d2 = sh4der_dot3_(l, l);
        const uint32_t c = in[i];
        float ndl, a;
        unsigned r, g, b;

        sh4der_norm3_(l);
        ndl = sh4der_dot3_(s->n[i], l);
        if (!(ndl > 0.0f)) { out[i] = c; continue; }
        a = k * ndl * sh4der_recip_pos_(sh4der_fma_(d2, PL_FALL, 1.0f));
        r = ((c >> 16) & 255u) + (unsigned)(a * lamp.warm[0]); g = ((c >> 8) & 255u) + (unsigned)(a * lamp.warm[1]); b = (c & 255u) + (unsigned)(a * lamp.warm[2]);
        out[i] = 0xff000000u | ((r > 255u ? 255u : r) << 16) | ((g > 255u ? 255u : g) << 8) | (b > 255u ? 255u : b);
    }
}

/* 1 when a triangle on the screen has area: a triangle without one is not sent (a real console's rule). */
static inline int tri_ok(const float a[3], const float b[3], const float c[3]) {
    const float area2 = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);

    return sh4der_abs_(area2) > 0.25f && a[2] < 1.5f && b[2] < 1.5f && c[2] < 1.5f;      /* and every point is in front of the eye (ao4_cam_pt marks one that is not with 2) */
}

/* A whole frame. 0 when the render did not come back. */
static int frame(const view_t *v, cost_t *c) {
    const side_t sl = mode_side[v->mode][v->swap ? 1 : 0], sr = mode_side[v->mode][v->swap ? 0 : 1];
    const side_t side[2] = { sl, sr };
    const uint64_t c_frame = cyc();
    const uint32_t *colour[2];
    sh4der_split_half_t half[2];
    ao4_cam_t cam;
    uint64_t t0;
    unsigned i, k, st, sd, any_lamp = 0, any_pl = 0;

    memset(c, 0, sizeof *c);
    sh4der_split_halves(v->div, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &half[0], &half[1]);
    for (sd = 0; sd < 2u; sd++) if (!half[sd].hidden && lamp_ok) { any_lamp |= (unsigned)side[sd].lamp; any_pl |= (unsigned)(side[sd].lamp && side[sd].light); }
    ao4_cam_at(&room, &cam, v->t, v->turn, v->lift);
    if (v->close && lamp_ok) {              /* the room's eye, a long lens aimed at the bulb */
        const float up[3] = { 0.0f, 1.0f, 0.0f };
        float r[3];

        cam.f[0] = lamp.bulb[0] - cam.e[0]; cam.f[1] = lamp.bulb[1] - cam.e[1]; cam.f[2] = lamp.bulb[2] - cam.e[2];
        sh4der_norm3_(cam.f);
        sh4der_cross_(up, cam.f, r);
        sh4der_norm3_(r);
        sh4der_cross_(cam.f, r, cam.u);
        cam.r[0] = -r[0]; cam.r[1] = -r[1]; cam.r[2] = -r[2];   /* the room is a right-handed model (ao4_cam_at) */
        cam.focal = CLOSE_FOCAL;
    }
    for (i = 0; i < room.vn; i++) ao4_cam_pt(&cam, room.p[i], scr[i]);
    t0 = cyc();
    ao4_light(&room, AO4_BAKED, AO4_CLAY, 0, lit);
    c->light = cyc() - t0;
    if (any_lamp) {
        t0 = cyc();
        room_dim(lit, dimmed[0], room.vn);
        c->dim = cyc() - t0;
        if (any_pl) {
            t0 = cyc();
            room_light(&room, dimmed[0], dimmed[1]);
            c->pl = cyc() - t0;
            c->pl_verts = room.vn; c->have_pl = 1;
        }
        t0 = cyc();
        ao4_lamp_shade(&lamp, cam.e, AO4_CLAY_GREY, shade_base, shade_off);
        for (i = 0; i < AO4_LAMP_V; i++) ao4_cam_pt(&cam, lamp.p[i], shade_scr[i]);
        if (v->liner)
            for (i = 0; i < AO4_LAMP_V; i++) {
                const float q[3] = { lamp.cx + (lamp.p[i][0] - lamp.cx) * LINER, lamp.p[i][1], lamp.cz + (lamp.p[i][2] - lamp.cz) * LINER };

                ao4_cam_pt(&cam, q, liner_scr[i]);
            }
        c->shade = cyc() - t0;
        t0 = cyc();
        ao4_lamp_halo(&lamp, cam.e, 0.0f, planes, ROOM_SURFS, halo_xyz, halo_col);
        for (i = 0; i < AO4_LAMP_HALO_V; i++) ao4_cam_pt(&cam, halo_xyz[i], halo_scr[i]);
        c->halo = cyc() - t0;
        c->have_lamp = 1;
    }
    for (sd = 0; sd < 2u; sd++) colour[sd] = !side[sd].lamp || !lamp_ok ? lit : dimmed[side[sd].light ? 1 : 0];

    if (!wait_ready()) return 0;
    pvr_scene_begin();
    sent = 0; cur_oargb = 0u;
    pvr_list_begin(PVR_LIST_OP_POLY);
    for (sd = 0; sd < 2u; sd++) {
        const uint32_t *col = colour[sd];
        const int skip = side[sd].lamp && lamp_ok;

        if (half[sd].hidden) continue;      /* a hidden half is not sent */
        user_clip(&h_obj, &half[sd]);
        pvr_prim(&h_obj, sizeof h_obj);
        for (st = 0, k = 0; st < room.strips; k += room.slen[st], st++) {
            if (skip && shade_strip[st]) continue;
            for (i = 0; i < room.slen[st]; i++) {
                const unsigned x = room.idx[k + i];

                put(i + 1u == room.slen[st], scr[x][0], scr[x][1], scr[x][2], 0.0f, 0.0f, col[x]);
            }
        }
        if (skip && side[sd].slots && v->liner) {           /* the lit inside: what a slit shows, in place of the room behind */
            const unsigned lr = (unsigned)(255.0f * lamp.inside * lamp.warm[0]), lg = (unsigned)(255.0f * lamp.inside * lamp.warm[1]), lb = (unsigned)(255.0f * lamp.inside * lamp.warm[2]);
            const uint32_t lc = 0xff000000u | ((lr > 255u ? 255u : lr) << 16) | ((lg > 255u ? 255u : lg) << 8) | (lb > 255u ? 255u : lb);

            for (i = 0; i < AO4_LAMP_T; i++) {
                const uint16_t *t = lamp.tri[i];

                if (!tri_ok(liner_scr[t[0]], liner_scr[t[1]], liner_scr[t[2]])) continue;
                for (k = 0; k < 3u; k++) put(k == 2u, liner_scr[t[k]][0], liner_scr[t[k]][1], liner_scr[t[k]][2], 0.0f, 0.0f, lc);
            }
        }
        flush();
    }
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    for (sd = 0; sd < 2u; sd++) {           /* the halo, a half at a time */
        if (half[sd].hidden || !side[sd].lamp || !lamp_ok) continue;
        user_clip(&h_trdummy, &half[sd]);
        pvr_prim(&h_halo, sizeof h_halo);
        for (i = 0; i < AO4_LAMP_HALO_T; i++) {
            const uint8_t *t = halo_tri[i];

            if (!tri_ok(halo_scr[t[0]], halo_scr[t[1]], halo_scr[t[2]])) continue;
            for (k = 0; k < 3u; k++) put(k == 2u, halo_scr[t[k]][0], halo_scr[t[k]][1], halo_scr[t[k]][2], 0.0f, 0.0f, halo_col[t[k]]);
            c->halo_tris++;
        }
        flush();
    }
    if (!sh4der_split_at_end(v->div, SH4DER_SPLIT_COLS)) {
        flush(); pvr_prim(&h_dim, sizeof h_dim);
        rect(sh4der_split_x(v->div) - 1.0f, 0.0f, sh4der_split_x(v->div) + 1.0f, 480.0f, 10.0f, 0, 0, 0, 0, 0xff101010u);
    }
    for (sd = 0; sd < 2u; sd++) {           /* each half's name, centred on it */
        const unsigned n = (unsigned)strlen(mode_name[v->mode][(v->swap ? 1u : 0u) ^ sd]);
        const int x = sh4der_split_label_x(&half[sd], n * 8u + 4u);

        if (x >= 0) text_put(0u, sd * 32u, sd * 32u + n, (float)x + 2.0f, 52.0f, NULL);
    }
    text_put(1u, 0u, (unsigned)strlen(text_ln[1]), 64.0f, 372.0f, NULL);
    text_put(2u, 0u, (unsigned)strlen(text_ln[2]), 64.0f, 392.0f, NULL);
    text_put(3u, 0u, (unsigned)strlen(text_ln[3]), 64.0f, 412.0f, NULL);
    if (fx_live && !fx_hidden)              /* above the costs; one line sits on the lower row */
        for (i = 0; i < fx_now.lines && i < 2u; i++) text_put(4u + i, 0u, (unsigned)strlen(text_ln[4u + i]), 64.0f, 352.0f - 20.0f * (float)(fx_now.lines - 1u - i), fx_mark);
    flush();
    pvr_list_finish();
    if (pt_on) {                            /* the shade: the punch-through list. Opened every frame it exists, and never empty: */
        pvr_list_begin(PVR_LIST_PT_POLY);   /* a triangle off the screen's left edge first, so the list is the same with the lamp off */
        pvr_prim(&h_ptdummy, sizeof h_ptdummy);
        put(0, -64.0f, 8.0f, 0.001f, 0.0f, 0.0f, 0xff000000u);
        put(0, -64.0f, 40.0f, 0.001f, 0.0f, 0.0f, 0xff000000u);
        put(1, -32.0f, 8.0f, 0.001f, 0.0f, 0.0f, 0xff000000u);
        flush();
        for (sd = 0; sd < 2u; sd++) {
            if (half[sd].hidden || !side[sd].lamp || !lamp_ok) continue;
            user_clip(&h_ptdummy, &half[sd]);
            pvr_prim(&h_shade[side[sd].slots ? 1 : 0], sizeof h_shade[0]);
            for (i = 0; i < AO4_LAMP_T; i++) {
                const uint16_t *t = lamp.tri[i];

                if (!tri_ok(shade_scr[t[0]], shade_scr[t[1]], shade_scr[t[2]])) continue;
                for (k = 0; k < 3u; k++) {
                    const unsigned x = t[k];

                    cur_oargb = shade_off[x];
                    put(k == 2u, shade_scr[x][0], shade_scr[x][1], shade_scr[x][2], lamp.uv[x][0], lamp.uv[x][1], shade_base[x]);
                }
                c->pt_tris++;
            }
            flush();
            cur_oargb = 0u;
        }
        pvr_list_finish();
    }
    c->verts = sent;
    if (!scene_close()) return 0;
    c->pvr = last_render_ns; c->vbuf = last_vbuf;
    c->frame = cyc() - c_frame;
    return 1;
}

typedef struct sums { cost_t c; unsigned n, n_lamp, n_pl; } sums_t;

static void sums_add(sums_t *s, const cost_t *c) {
    s->c.light += c->light; s->c.frame += c->frame; s->c.pvr += c->pvr;
    s->c.verts = c->verts; s->c.vbuf = c->vbuf; s->c.pt_tris = c->pt_tris; s->c.halo_tris = c->halo_tris;
    s->n++;
    if (c->have_lamp) { s->c.dim += c->dim; s->c.shade += c->shade; s->c.halo += c->halo; s->n_lamp++; }
    if (c->have_pl) { s->c.pl += c->pl; s->c.pl_verts = c->pl_verts; s->n_pl++; }
}

static void sums_mean(const sums_t *s, cost_t *m) {
    const unsigned n = s->n ? s->n : 1u, nl = s->n_lamp ? s->n_lamp : 1u, np = s->n_pl ? s->n_pl : 1u;

    *m = s->c;
    m->light /= n; m->frame /= n; m->pvr /= n; m->dim /= nl; m->shade /= nl; m->halo /= nl; m->pl /= np;
}

#define RUN_STAGES 8
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

/* The text of this view and these costs (a mean the caller keeps). */
static void labels(const view_t *v, const cost_t *c) {
    const side_t sl = mode_side[v->mode][v->swap ? 1 : 0], sr = mode_side[v->mode][v->swap ? 0 : 1];
    sh4der_split_half_t hl, hr;
    fx_entry_t e[6];
    int lamp_on, slots, light;

    sh4der_split_halves(v->div, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &hl, &hr);
    lamp_on = lamp_ok && ((!hl.hidden && sl.lamp) || (!hr.hidden && sr.lamp));
    slots = (!hl.hidden && sl.lamp && sl.slots) || (!hr.hidden && sr.lamp && sr.slots);
    light = (!hl.hidden && sl.lamp && sl.light) || (!hr.hidden && sr.lamp && sr.light);
    text_set(0u, "%-32.32s%-32.32s", mode_name[v->mode][v->swap ? 1 : 0], mode_name[v->mode][v->swap ? 0 : 1]);
    if (lamp_on) text_set(1u, "SH4 us: dim %u shade %u halo %u light %u (%u v)", us_of(c->dim), us_of(c->shade), us_of(c->halo), us_of(c->pl), c->pl_verts);
    else text_set(1u, "SH4 us: the room's light %u; the lamp is off", us_of(c->light));
    text_set(2u, "frame %u.%u ms PVR %u us %u v TA %uK%s%s", us_of(c->frame) / 1000u, us_of(c->frame) % 1000u / 100u, (unsigned)(c->pvr / 1000u), c->verts,
             (unsigned)((c->vbuf + 512u) / 1024u), hl.hidden || hr.hidden ? " HALF HIDDEN: NOT SENT" : "", lamp_ok ? "" : " NO LAMP IN THE ROOM");
    if (run_stage >= 0) text_set(3u, "MEASURING stage %u of %u: any button = live view now", (unsigned)run_stage + 1u, (unsigned)RUN_STAGES);
    else text_set(3u, "X compare Y swap A cam B hens LT/RT div dn inside%s START pg", v->liner ? " ON" : "");
    fx_format_merged(&fx_now, e, lamp_fx_live(lamp_on, slots, light, e));
    text_set(4u, "%s", fx_now.line[0]);
    text_set(5u, "%s", fx_now.line[1]);
}

/* ---- start, stop ------------------------------------------------------------------- */

/* with_pt: the punch-through list has a bin (the lamp's shade needs it). */
static int start(int with_pt) {
    pvr_init_params_t init = pvr_default_params;
    unsigned i;

    init.opb_sizes[PVR_LIST_OP_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_OP_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_TR_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_PT_POLY] = with_pt ? PVR_BINSIZE_16 : PVR_BINSIZE_0;
    init.vertex_buf_size = 768 * 1024;
    init.autosort_disabled = 1;
    init.opb_overflow_count = 4;
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return 0; }
    pt_on = with_pt;
    pvr_set_bg_color(AO4_BACKDROP_R, AO4_BACKDROP_G, AO4_BACKDROP_B);
    text_mem = pvr_mem_malloc(sizeof text_px);
    slot_mem[0] = pvr_mem_malloc(sizeof slot_px);
    slot_mem[1] = pvr_mem_malloc(sizeof slot_px);
    if (!text_mem || !slot_mem[0] || !slot_mem[1]) { say("video memory allocation FAIL"); pvr_shutdown(); return 0; }
    for (i = 0; i < 2u; i++) { ao4_lamp_pattern(slot_px, (int)i); pvr_txr_load(slot_px, slot_mem[i], sizeof slot_px); }
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

/* ---- the loop: the run that counts, and the live view ------------------------------------------ */

/* the run that counts, a stage: the comparison, the divider, the camera near, the PVR it runs on (0: no
   punch-through list). */
typedef struct stage { uint8_t mode, div, close, pt, swap; const char *name; } stage_t;
#define AUTO_STAGES 8u
static const stage_t auto_stage[AUTO_STAGES] = {
    { MODE_BASE, 10, 0, 0, 0, "off, 2 lists" },                    /* the same picture on a PVR without the third list .. */
    { MODE_BASE, 10, 0, 1, 0, "off, 3 lists" },                    /* .. and with it, one triangle off the screen in it */
    { 0, 10, 0, 1, 1, "ON | OFF" },                             /* swapped: the lamp stands left of the middle from the room's eye */
    { 1, 10, 0, 1, 0, "SLOTS" },
    { 2, 10, 0, 1, 0, "ROOM LIGHT" },
    { 0, 10, 1, 1, 0, "OFF | ON near" },
    { 0, 0, 0, 1, 0, "ON alone" },                                 /* the divider at the left end: the left half is not sent */
    { 0, 20, 0, 1, 0, "OFF alone" },                               /* and at the right end */
};
static sums_t auto_sum[AUTO_STAGES];
static unsigned auto_done;

static void view_default(view_t *v) { memset(v, 0, sizeof *v); v->div = sh4der_split_middle(SH4DER_SPLIT_COLS); v->hens = 1; }

/* Frames: the run's stages first .. last on this PVR (auto_run 1), or the live view until START (0). */
static void loop(int auto_run, unsigned first, unsigned last) {
    view_t v;
    sums_t win;
    cost_t c, shown;
    sh4der_split_step_t trig = { 0, 0u };
    unsigned held = ~0u, frames = 0, stage = first;
    uint64_t t_prev = ns(), t_stage = ns();

    view_default(&v);
    fx_live = !auto_run;
    memset(&win, 0, sizeof win);
    memset(&shown, 0, sizeof shown);
    ao4_hens_reset(&hens);
    if (!auto_run && pad_from >= 0) stage = (unsigned)pad_from;      /* the live view a button opened: as the stage it stopped */
    if (auto_run || pad_from >= 0) { v.mode = auto_stage[stage].mode; v.div = auto_stage[stage].div; v.close = auto_stage[stage].close; v.swap = auto_stage[stage].swap; }
    pad_from = -1;
    run_stage = auto_run ? (int)stage : -1;
    labels(&v, &shown);
    text_upload();
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;
        const uint64_t t_now = ns();
        const float dt = (float)(t_now - t_prev) * 1.0e-9f;
        int changed = 0;

        step("lamp frame ");
        t_prev = t_now; held = now;
        v.t += dt < 0.1f ? dt : 0.1f;
        if (auto_run) {
            if (pad_hit()) { pad_stop = 1; pad_from = (int)stage; break; }   /* the stage in hand is not counted as done */
            if (t_now - t_stage > (uint64_t)LAMP_AUTO_S * 1000000000ull) {
                t_stage = t_now; auto_done = stage + 1u; stage++; changed = 1;
                if (stage > last) break;
                v.mode = auto_stage[stage].mode; v.div = auto_stage[stage].div; v.close = auto_stage[stage].close; v.swap = auto_stage[stage].swap;
                run_stage = (int)stage;
            }
        } else {
            const unsigned d = st ? sh4der_split_step(&trig, v.div, SH4DER_SPLIT_COLS, st->ltrig > 128, st->rtrig > 128) : v.div;

            if (st) {
                if (st->joyx > 24 || st->joyx < -24) v.turn += (float)st->joyx * (1.0f / 128.0f) * dt * 1.2f;
                if (st->joyy > 24 || st->joyy < -24) v.lift -= (float)st->joyy * (1.0f / 128.0f) * dt * 4.0f;   /* the stick up lifts the eye */
                if (v.lift < -4.5f) v.lift = -4.5f;
                if (v.lift > 7.0f) v.lift = 7.0f;
                if (v.turn > 0.6f) v.turn = 0.6f;
                if (v.turn < -0.6f) v.turn = -0.6f;
            }
            if (d != v.div) { v.div = d; changed = 1; }
            if ((hit & CONT_START) && frames > 10u) break;
            if (hit & CONT_X) { v.mode = (v.mode + 1u) % MODES; changed = 1; }
            if (hit & CONT_Y) { v.swap = !v.swap; changed = 1; }
            if (hit & CONT_A) { v.close = !v.close; changed = 1; }
            if (hit & CONT_DPAD_DOWN) { v.liner = !v.liner; changed = 1; }
            if ((hit & CONT_B) && (now & CONT_DPAD_UP)) fx_hide_chord(1, 1, &fx_hidden);          /* up held, then B: the effect labels */
            else if (hit & CONT_B) { v.hens = !v.hens; changed = 1; }
        }
        if (changed) memset(&win, 0, sizeof win);
        ao4_hens_show(&room, &hens, v.hens);
        if (v.hens) { ao4_hens_step(&hens, dt < 0.1f ? dt : 0.1f); ao4_hens_place(&room, &hens); }
        if (!frame(&v, &c)) { say("%s: NO RENDER END at frame %u (stage %u, %s)", auto_run ? "auto" : "live", frames, stage, mode_name[v.mode][1]); break; }
        frames++;
        sums_add(&win, &c);
        if (auto_run && win.n > 2u) sums_add(&auto_sum[stage], &c);
        if (changed || win.n >= 30u) {      /* the text: when the view changes, and the costs' mean every 30 frames */
            if (win.n >= 30u || !shown.frame) sums_mean(&win, &shown);
            if (win.n >= 30u) memset(&win, 0, sizeof win);
            labels(&v, &shown);
            text_upload();                  /* the render is done (scene_close): nothing reads the texture now */
        }
    }
    run_stage = -1;
}

/* ---- the run ---------------------------------------------------------------------- */

static int fault_on;

static void report(void) {
    pg_plot_t *g;
    cost_t m[AUTO_STAGES];
    char rev[32];
    unsigned i;

    suite_rev_text(rev, sizeof rev);
    for (i = 0; i < AUTO_STAGES; i++) sums_mean(&auto_sum[i], &m[i]);
    say("ao-lamp %s: the lamp look. A stage's means; us unless said.", LAMP_STR(LAMP_TAG));
    say("%s", rev);
    say("Effect labels: off in the measured stages, on in the live");
    say(" view (d-pad up held + B hides them).");
    if (!lamp_ok) say("NO LAMP FOUND IN THE ROOM'S MODEL: every stage is the plain room");
    if (auto_done < AUTO_STAGES && pad_stop) say("RUN STOPPED BY THE PAD AT STAGE %u: stages %u.. not measured", auto_done, auto_done);
    else if (auto_done < AUTO_STAGES) say("THE RUN DID NOT FINISH: %u of %u stages", auto_done, AUTO_STAGES);
    say("stage           frame PVR   verts  TA K  frames");
    for (i = 0; i < auto_done; i++)
        say("%u %-13s %5u %5u %6u %4u %6u", i, auto_stage[i].name, us_of(m[i].frame), (unsigned)(m[i].pvr / 1000u), m[i].verts, (unsigned)((m[i].vbuf + 512u) / 1024u), auto_sum[i].n);
    if (auto_done >= 5u) {
        say("SH-4 a frame with the lamp on (stage 2): the room's light %u,", us_of(m[2].light));
        say(" dimming %u, the shade %u (%u v), the halo %u (%u v)", us_of(m[2].dim), us_of(m[2].shade), (unsigned)AO4_LAMP_V, us_of(m[2].halo), (unsigned)AO4_LAMP_HALO_V);
        say("Point light (stage 4): %u us for %u vertices = %u ns each", us_of(m[4].pl), m[4].pl_verts,
            m[4].pl_verts ? (unsigned)((m[4].pl * 1000000ull / CYC_PER_MS) / m[4].pl_verts) : 0u);
        say("Sent: shade %u triangles (punch-through), halo %u a half", m[2].pt_tris, m[2].halo_tris);
        say("DERIVED the third list, unused: frame %+d, PVR %+d (1 - 0)", (int)us_of(m[1].frame) - (int)us_of(m[0].frame), (int)(m[1].pvr / 1000u) - (int)(m[0].pvr / 1000u));
        say("DERIVED the lamp in a half: frame %+d, PVR %+d (2 - 1)", (int)us_of(m[2].frame) - (int)us_of(m[1].frame), (int)(m[2].pvr / 1000u) - (int)(m[1].pvr / 1000u));
        say("DERIVED the slots: frame %+d, PVR %+d (3 - 2; 3 has two lamps)", (int)us_of(m[3].frame) - (int)us_of(m[2].frame), (int)(m[3].pvr / 1000u) - (int)(m[2].pvr / 1000u));
    }
    if (auto_done >= AUTO_STAGES) {
        say("DERIVED the lamp at full width: frame %+d, PVR %+d (6 - 7)", (int)us_of(m[6].frame) - (int)us_of(m[7].frame), (int)(m[6].pvr / 1000u) - (int)(m[7].pvr / 1000u));
        say("A half hidden is not sent: 6 and 7 send one half each.");
    }
    say("LAMP ON is a whole shade; the slots are in stage 3 alone.");
    say("One run, one console. The picture is not on this page:");
    say(" photograph the live view (X: the three comparisons).");
    g = pg_plot("ao-lamp: a frame, us", "stage", "us", 1);
    pg_cat(g, "2 lists"); pg_cat(g, "3 lists"); pg_cat(g, "ON|OFF"); pg_cat(g, "SLOTS"); pg_cat(g, "LIGHT"); pg_cat(g, "near"); pg_cat(g, "ON"); pg_cat(g, "OFF");
    pg_series(g, "frame"); for (i = 0; i < AUTO_STAGES; i++) pg_point(g, (float)i, i < auto_done ? (float)us_of(m[i].frame) : 0.0f);
    pg_series(g, "PVR"); for (i = 0; i < AUTO_STAGES; i++) pg_point(g, (float)i, i < auto_done ? (float)(m[i].pvr / 1000u) : 0.0f);
}

/* The room, the hens, the lamp: once. */
static void build(void) {
    unsigned k, st, i;
    int found = -1;

    ao4_build_room(&room);
    ao4_hens_add(&room, &hens);
    for (k = 0; k < ROOM_OBJS; k++) if (!strcmp(room_obj[k].name, "lamp")) found = (int)k;
    lamp_ok = found >= 0 && ao4_lamp_find(&lamp, &room_v[room_obj[found].v0], room_obj[found].vn);
    if (!lamp_ok) return;
    ao4_lamp_build(&lamp);
    sh4der_bloom_rings_tris(AO4_LAMP_HALO_RINGS, AO4_LAMP_HALO_SEG, halo_tri);
    for (k = 0; k < ROOM_SURFS; k++) for (i = 0; i < 3u; i++) { planes[k].o[i] = room_surf[k].o[i]; planes[k].n[i] = room_surf[k].n[i]; }
    for (st = 0, k = 0; st < room.strips; k += room.slen[st], st++) {      /* the model's own shade: triangles near the lamp's axis, above its middle */
        const float *a = room.p[room.idx[k]], *b = room.p[room.idx[k + 1u]], *c = room.p[room.idx[k + 2u]];

        if (room.slen[st] != 3u) continue;
        if (ao4_lamp_radius_(&lamp, a) > 0.4f || ao4_lamp_radius_(&lamp, b) > 0.4f || ao4_lamp_radius_(&lamp, c) > 0.4f) continue;
        shade_strip[st] = (uint8_t)ao4_lamp_is_shade(&lamp, a, b, c);
    }
}

int main(void) {
    unsigned page = 0, pages, held = ~0u;
    int redraw = 1;

    suite_init();                           /* the suite disc: the menu's binary into RAM, before anything else */
    vid_set_mode(DM_640x480, PM_RGB565);
    pg_init();
    perf_cntr_start(PRFC1, PMCR_ELAPSED_TIME_MODE, PMCR_COUNT_CPU_CYCLES);
    build();
    if (start(0)) {                         /* the first stage: a PVR without the punch-through list */
        if (!fault_on) { fault_on = 1; if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused"); }
        loop(1, 0u, 0u);
        stop("auto 2 lists");
    }
    if (!pad_stop && auto_done == 1u && start(1)) {      /* the rest: with it */
        loop(1, 1u, AUTO_STAGES - 1u);
        if (pad_stop) loop(0, 0u, 0u);      /* a button during the run: the live view at once, the pages after it */
        stop("auto end");
    } else if (pad_stop && start(1)) {      /* a button in the first stage: the live view on a PVR with the third list */
        loop(0, 0u, 0u);
        stop("live end");
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
            if (start(1)) { loop(0, 0u, 0u); stop("live end"); }
            vid_set_mode(DM_640x480, PM_RGB565);
            held = ~0u;
            redraw = 1;
        }
        else if (suite_tick(now, page, pages)) { page++; redraw = 1; }
        if (redraw) { pg_draw(page % pages, "AO LAMP", suite_keys_text("START live view")); fx_page(lamp_fx_page, LAMP_FX_PAGE_N); redraw = 0; }
        thd_sleep(20);
    }
    return 0;
}
