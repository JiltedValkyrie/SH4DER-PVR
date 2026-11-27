/* SH4DER-PVR: corr-a, an endless corridor for the PVR's texture filters at grazing angles, Dreamcast demo and test program

   demo/psm/corr/corr_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The eye walks down a corridor that never ends (corr_scene.h): a stone floor, brick walls, a plank ceiling, pillars, a
   gate and torches, 1 m cells, the tiles of Retro Textures Fantasy (Kenney, CC0: CREDITS.md) upscaled to 256 x 256 (the
   floor and the walls; 128 x 128 by the size key) with full mip chains, RGB565, twiddled. A floor and walls seen edge
   on far off are where a mip map blurs most and shimmers most: this program shows the PVR's filters there, one or two
   at once (the screen split at a divider, sh4der_split.h, each half its own mode), with their costs.

   The modes, the guard and the labels are corr_modes.h's. Trilinear is two passes of the same vertices: pass A
   (PVR_FILTER_TRILINEAR1) in the opaque list, pass B (PVR_FILTER_TRILINEAR2) in the translucent list, ONE x ONE, depth
   GEQUAL, no depth write (the hardware manual, System Architecture 3.4.7.2.3; KallistiOS names the two passes only).
   Every statement about the filters' look and the PVR's mip choice is OPEN until a console run.

   It runs by itself first (CORR_STAGES stages, CORR_AUTO_S seconds each, the first CORR_SETTLE_S not counted, every
   frame after that counted; each stage starts the walk at the same place, at 3 m/s with the sway, so the stages see the
   same corridor), then the result pages. A key pressed in a stage acts and marks that stage TOUCHED on the page.
   START on the pages: the live view.

     X  the left mode (the whole screen with no split)     Y  the right mode     B + X  split on / off
     B + LT / RT  the divider     D-pad up / down  the bias selector     D-pad left / right  speed 1, 3, 6, 12 m/s
     A  stop / walk     B + Y  floor and walls 256 / 128     B + START  the effect labels     START  the pages
     stick  turn     LT / RT  step left / right

   The counters (count-only, nothing is dropped for them): the most filtered opaque (and translucent) polygons whose
   screen boxes cover one 32 x 32 bin, for CORE 26 (PVR-071: the 8th filtered opaque polygon in a tile, REPORTED from
   skmp's model, for non-twiddled bump textures; nothing here is one). The guards: a filtered header on a non-twiddled
   or bump texture is refused (a); every header is checked by sh4der_poly_hdr_ok before it is sent (b); every triangle by
   sh4der_tri_area_ok (c); every wait on the PVR is polled on the timer and given up after LIMIT_NS (d), counted.
   The fault catcher (include/sh4der_fault.h) is in. A plain program: on a disc of its own it ends in its page loop; in the suite
   (../psm_suite.h) it goes back to the menu. Only a real console's run is evidence. */

#include <kos.h>
#include <stdarg.h>
#include <stdio.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"
#include "../../../include/sh4der_suite.h"
#include "corr_scene.h"
#include "corr_tex_data.h"                   /* generated: tools/corr_bake.py */

KOS_INIT_FLAGS(INIT_DEFAULT);

#ifndef CORR_TAG
#define CORR_TAG untagged
#endif
#define CORR_STR_(x) #x
#define CORR_STR(x) CORR_STR_(x)
#ifndef CORR_AUTO_S
#define CORR_AUTO_S 8u
#endif
#define CORR_SETTLE_S 2u
#define CORR_AUTO_SPEED 1u                   /* 3 m/s */

#define PG_PREFIX "corr"
#define PG_LINES_MAX 160u
#define PG_TOP 2u
#include "../../../include/sh4der_page.h"
#include "../../../include/sh4der_fxlabel.h"
#include "sh4der_split.h"
#include "corr_modes.h"
#define say pg_say
static sh4der_fault_crumbs_t crumbs;

static void step(const char *name) { sh4der_fault_beat(&crumbs); sh4der_fault_stage(&crumbs, name); crumbs.frame++; }

#define LIMIT_NS 200000000ull
static inline uint64_t ns(void) { const uint64_t n = timer_ns_gettime64(); return n - n / 1000000000ull * 2502400ull; }   /* ao4: the TMU2 reload step out */
static unsigned us_of(uint64_t t) { return (unsigned)((t + 500u) / 1000u); }

/* ---- memory, headers, the vertex run --------------------------------------------------------------------------------- */

enum { TX_FLOOR256, TX_WALL256, TX_FLOOR128, TX_WALL128, TX_CEIL, TX_DOOR, TXS };
static const uint16_t *const tex_src[TXS] = { corr_tex_floor256, corr_tex_wall256, corr_tex_floor128, corr_tex_wall128, corr_tex_ceil128, corr_tex_door128 };
static const uint32_t tex_bytes[TXS] = { sizeof corr_tex_floor256, sizeof corr_tex_wall256, sizeof corr_tex_floor128, sizeof corr_tex_wall128,
                                         sizeof corr_tex_ceil128, sizeof corr_tex_door128 };
static const unsigned tex_side[TXS] = { CORR_TEX_FLOOR256_SIDE, CORR_TEX_WALL256_SIDE, CORR_TEX_FLOOR128_SIDE, CORR_TEX_WALL128_SIDE,
                                        CORR_TEX_CEIL128_SIDE, CORR_TEX_DOOR128_SIDE };

enum { HV_NEAREST, HV_BILINEAR, HV_TRI_A, HV_TRI_B, HV_BIAS0, HVS = HV_BIAS0 + 15 };   /* HV_BIAS0 + code - 1: bias code 1 .. 15 */

#define RUN 256u
#define TEXT_W 512u
#define TEXT_H 128u
#define TEXT_LINES 6u                        /* 4, 5: the effect labels */

static pvr_ptr_t tex_mem[TXS], text_mem;
static pvr_poly_hdr_t h_tex[TXS][HVS], h_flat, h_flame, h_text, h_dim, h_dum_op, h_dum_tr;
static unsigned refused;                     /* (a) */
static pvr_vertex_t vrun[RUN] __attribute__((aligned(32)));
static unsigned run_n;
static uint16_t text_px[TEXT_W * TEXT_H] __attribute__((aligned(32)));
static char text_ln[TEXT_LINES][65];
static int text_dirty;
static fx_text_t fx_t;
static int fx_on, fx_hidden;

static void flush(void) { if (run_n) { pvr_prim(vrun, run_n * sizeof vrun[0]); run_n = 0; } }

static inline void put(int eol, float x, float y, float z, float u, float v, uint32_t argb) {
    pvr_vertex_t *o;

    if (run_n == RUN) flush();
    o = &vrun[run_n++];
    o->flags = eol ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    o->x = x; o->y = y; o->z = z; o->u = u; o->v = v; o->argb = argb; o->oargb = 0u;
}

static void rect(float x0, float y0, float x1, float y1, float z, float u0, float v0, float u1, float v1, uint32_t argb) {
    put(0, x0, y1, z, u0, v1, argb); put(0, x0, y0, z, u0, v0, argb); put(0, x1, y1, z, u1, v1, argb); put(1, x1, y0, z, u1, v0, argb);
}

static unsigned hdr_sent, hdr_bad;           /* a frame's; (b) */

static int hdr(const pvr_poly_hdr_t *h) {    /* (b): a header that was never compiled (or was refused) is not sent */
    if (!sh4der_poly_hdr_ok((const uint32_t *)(const void *)h)) { hdr_bad++; return 0; }
    flush(); pvr_prim((void *)h, sizeof *h); hdr_sent++;
    return 1;
}

static void make_tex_hdr(unsigned t, unsigned hv) {
    pvr_poly_cxt_t c;
    const int filter = hv == HV_NEAREST ? PVR_FILTER_NEAREST : hv == HV_TRI_A ? PVR_FILTER_TRILINEAR1 : hv == HV_TRI_B ? PVR_FILTER_TRILINEAR2 : PVR_FILTER_BILINEAR;
    const int tr = hv == HV_TRI_B;

    memset(&h_tex[t][hv], 0, sizeof h_tex[t][hv]);
    if (!corr_filter_ok(CORR_FMT_TWIDDLED | CORR_FMT_MIPMAP, filter != PVR_FILTER_NEAREST)) { refused++; return; }   /* (a) */
    pvr_poly_cxt_txr(&c, tr ? PVR_LIST_TR_POLY : PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_TWIDDLED, (int)tex_side[t], (int)tex_side[t], tex_mem[t], filter);
    c.gen.culling = PVR_CULLING_NONE; c.gen.shading = PVR_SHADE_GOURAUD; c.gen.clip_mode = PVR_USERCLIP_INSIDE; c.gen.fog_type = PVR_FOG_TABLE;
    c.txr.env = PVR_TXRENV_MODULATE; c.txr.mipmap = true;
    c.txr.mipmap_bias = hv >= HV_BIAS0 ? (pvr_mip_bias_t)(hv - HV_BIAS0 + 1u) : PVR_MIPBIAS_NORMAL;
    if (tr) {                                /* trilinear pass B: added over pass A */
        c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ONE;
        c.depth.comparison = PVR_DEPTHCMP_GEQUAL; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    }
    pvr_poly_compile(&h_tex[t][hv], &c);
}

static void headers(void) {
    pvr_poly_cxt_t c;
    unsigned t, hv;

    refused = 0;
    for (t = 0; t < TXS; t++) for (hv = 0; hv < HVS; hv++) make_tex_hdr(t, hv);
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                       /* the torch brackets */
    c.gen.culling = PVR_CULLING_NONE; c.gen.fog_type = PVR_FOG_TABLE;
    pvr_poly_compile(&h_flat, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                       /* the flames: added */
    c.gen.culling = PVR_CULLING_NONE; c.gen.shading = PVR_SHADE_GOURAUD; c.gen.fog_type = PVR_FOG_TABLE;
    c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ONE; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_flame, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                       /* the polygon before a change of clip (CORE 18) */
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&h_dum_op, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    c.gen.culling = PVR_CULLING_NONE; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_dum_tr, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, (int)TEXT_W, (int)TEXT_H, text_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE; c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_text, &c);                                /* point sampled: (a) allows it on a non-twiddled texture */
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    c.gen.culling = PVR_CULLING_NONE; c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_dim, &c);
}

/* The user clip rectangle in tiles (ao4's): a polygon that does not matter goes first and takes the early change
   (CORE 18: a change of clip takes effect one polygon too soon). */
static void user_clip(const pvr_poly_hdr_t *dummy, unsigned tx0, unsigned ty0, unsigned tx1, unsigned ty1) {
    static uint32_t cmd[8] __attribute__((aligned(32)));

    flush();
    pvr_prim((void *)(uintptr_t)dummy, sizeof *dummy);
    put(0, -64.0f, 8.0f, 0.001f, 0.0f, 0.0f, 0xff000000u);
    put(0, -64.0f, 40.0f, 0.001f, 0.0f, 0.0f, 0xff000000u);
    put(1, -32.0f, 8.0f, 0.001f, 0.0f, 0.0f, 0xff000000u);
    flush();
    cmd[0] = PVR_CMD_USERCLIP; cmd[1] = cmd[2] = cmd[3] = 0u;
    cmd[4] = tx0; cmd[5] = ty0; cmd[6] = tx1; cmd[7] = ty1;
    pvr_prim(cmd, sizeof cmd);
}

/* ---- the text (hero1's) --------------------------------------------------------------------------------------------- */

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
                    (l >= 4u && fx_marked(&fx_t, l - 4u, i) ? 0x0fd0u : 0x0fffu));
        }
    pvr_txr_load(text_px, text_mem, sizeof text_px);
    text_dirty = 0;
}

static void text_line(unsigned line, float y) {
    const float v0 = (float)(line * 16u) / (float)TEXT_H, v1 = (float)(line * 16u + 16u) / (float)TEXT_H;

    hdr(&h_dim); rect(14.0f, y - 1.0f, 14.0f + 512.0f + 4.0f, y + 17.0f, 10.0f, 0, 0, 0, 0, 0xa0000000u);
    hdr(&h_text); rect(16.0f, y, 16.0f + 512.0f, y + 16.0f, 11.0f, 0.0f, v0, 1.0f, v1, 0xffffffffu);
}

/* ---- scenes on the PVR (hero1's polled waits) ----------------------------------------------------------------------- */

static asic_evt_handler_entry_t rd_prev;
static volatile uint32_t rd_seen;
static int rd_hooked;
static unsigned giveups;                     /* (d), the whole run */

static void rd_evt(uint32_t code, void *data) { (void)data; if (rd_prev.hdl) rd_prev.hdl(code, rd_prev.data); rd_seen = 1u; }

static int wait_ready(void) {
    const uint64_t t0 = ns();

    while (pvr_check_ready() < 0) if (ns() - t0 > LIMIT_NS) { giveups++; return 0; }
    return 1;
}

static int wait_render(void) {
    const uint64_t t0 = ns();
    unsigned t;

    if (rd_hooked) { while (!rd_seen) if (ns() - t0 > LIMIT_NS) { giveups++; return 0; } return 1; }
    for (t = 0; t < 20u && pvr_wait_render_done() < 0; t++) sh4der_fault_beat(&crumbs);
    if (t == 20u) { giveups++; return 0; }
    return 1;
}

/* ---- the state ------------------------------------------------------------------------------------------------------ */

static const float speeds[4] = { 1.0f, 3.0f, 6.0f, 12.0f };

typedef struct view {
    unsigned mode[2];                        /* the left half (the whole screen with no split), the right */
    unsigned sel;                            /* the bias selector (corr_modes.h) */
    unsigned d;                              /* the divider */
    unsigned speed;
    int split, big, walking;
    corr_eye_t eye;
    float t, steer;
} view_t;

typedef struct cost {
    uint64_t frame, sh4, xf, bins, pvr;      /* ns */
    unsigned polys, tris, hdrs, ta, bin_op, bin_tr, dropped, hdr_bad;
} cost_t;

static corr_kind_t kinds[CORR_KINDS];
static corr_list_t list, list_u;            /* list_u: the same frame with no far cap (the count only, once a stage) */
static corr_bins_t bins_op, bins_tr;
static uint16_t bucket[6][CORR_POLY_MAX];    /* the list by texture kind: floor, wall, ceiling, door, flat, flame */
static unsigned bucket_n[6];
static uint8_t pcode[CORR_POLY_MAX];         /* a polygon's bias code, this frame */
static unsigned tris_sent;

static unsigned eff_mode(unsigned m) { return m == CORR_M_RAMP && !corr_ramp_draw(NULL) ? CORR_M_BILINEAR : m; }

static unsigned tex_of(const view_t *v, unsigned kind) {
    return kind == CORR_T_FLOOR ? (v->big ? TX_FLOOR256 : TX_FLOOR128) : kind == CORR_T_WALL ? (v->big ? TX_WALL256 : TX_WALL128) : kind == CORR_T_CEIL ? TX_CEIL : TX_DOOR;
}

static void send_poly(const corr_poly_t *p) {
    unsigned j;

    for (j = 0; j < p->n; j++) put(j + 1u == p->n, p->x[j], p->y[j], p->z[j], p->u[j], p->v[j], p->argb[j]);
    tris_sent += p->n - 2u;
}

/* One half's textured polygons (columns c0 .. c1 of bins): the opaque list's pass, or (tr) trilinear's pass B. */
static void send_half(const view_t *v, unsigned mode, unsigned c0, unsigned c1, int tr) {
    unsigned kind, i;

    mode = eff_mode(mode);
    if (tr && mode != CORR_M_TRILINEAR) return;
    for (kind = 0; kind < CORR_T_TEXTURES; kind++) {
        const unsigned t = tex_of(v, kind);

        if (mode == CORR_M_BIAS) {          /* batched by code: one header a code that has polygons */
            unsigned code;

            for (code = 1; code <= 15u; code++) {
                int open = 0;

                for (i = 0; i < bucket_n[kind]; i++) {
                    const corr_poly_t *p = &list.p[bucket[kind][i]];

                    if (pcode[bucket[kind][i]] != code || !corr_poly_in_cols(p, c0, c1)) continue;
                    if (!open) { if (!hdr(&h_tex[t][HV_BIAS0 + code - 1u])) break; open = 1; }
                    send_poly(p);
                }
            }
        } else {
            const unsigned hv = mode == CORR_M_NEAREST ? HV_NEAREST : mode == CORR_M_BILINEAR ? HV_BILINEAR : tr ? HV_TRI_B : HV_TRI_A;
            int open = 0;

            for (i = 0; i < bucket_n[kind]; i++) {
                const corr_poly_t *p = &list.p[bucket[kind][i]];

                if (!corr_poly_in_cols(p, c0, c1)) continue;
                if (!open) { if (!hdr(&h_tex[t][hv])) break; open = 1; }
                send_poly(p);
            }
        }
    }
}

static void count_half(const corr_list_t *l, unsigned mode, unsigned c0, unsigned c1) {   /* the bins, count-only */
    unsigned i;

    mode = eff_mode(mode);
    if (mode == CORR_M_NEAREST) return;
    for (i = 0; i < l->n; i++) {
        const corr_poly_t *p = &l->p[i];

        if (!CORR_TEXTURED(p->tex) || !corr_poly_in_cols(p, c0, c1)) continue;
        corr_bins_add_cols(&bins_op, p, c0, c1);
        if (mode == CORR_M_TRILINEAR) corr_bins_add_cols(&bins_tr, p, c0, c1);
    }
}

/* What the first design (no far cap) would put on one bin, this eye, this mode: count-only, outside the timed frames. */
static unsigned uncapped_bins(const view_t *v) {
    sh4der_split_half_t half[2];
    unsigned h, nh;

    corr_frame(kinds, &v->eye, 0, CORR_NO_CAP, &list_u);
    if (v->split) { sh4der_split_halves(v->d, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &half[0], &half[1]); nh = 2u; }
    else { half[0].tx0 = 0u; half[0].tx1 = SH4DER_SPLIT_COLS - 1u; half[0].hidden = 0; nh = 1u; }
    corr_bins_clear(&bins_op); corr_bins_clear(&bins_tr);
    for (h = 0; h < nh; h++) if (!half[h].hidden) count_half(&list_u, v->mode[h], half[h].tx0, half[h].tx1);
    return corr_bins_max(&bins_op);
}

static int frame(view_t *v, cost_t *c, float dt) {
    const uint64_t t0 = ns();
    sh4der_split_half_t half[2];
    unsigned h, i, nh;
    const int any_bias = eff_mode(v->mode[0]) == CORR_M_BIAS || (v->split && eff_mode(v->mode[1]) == CORR_M_BIAS);
    uint64_t tw, t1, t2;
    pvr_stats_t st;

    if (v->walking) v->eye.s = corr_walk(v->eye.s, speeds[v->speed], dt);
    v->t += dt;
    if (v->t > 20.0f) v->t -= 20.0f;                          /* the sway's period: 20 s */
    v->eye.yaw = corr_sway(v->t) + v->steer;
    corr_frame(kinds, &v->eye, any_bias && !corr_sel(v->sel)->fixed, CORR_CAP, &list);
    memset(bucket_n, 0, sizeof bucket_n);
    for (i = 0; i < list.n; i++) {
        const unsigned k = list.p[i].tex;

        bucket[k][bucket_n[k]++] = (uint16_t)i;
        if (any_bias) pcode[i] = (uint8_t)corr_sel_code(corr_sel(v->sel), list.p[i].ndv);
    }
    c->xf = ns() - t0;
    if (v->split) { sh4der_split_halves(v->d, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &half[0], &half[1]); nh = 2u; }
    else { half[0].tx0 = half[0].ty0 = 0u; half[0].tx1 = SH4DER_SPLIT_COLS - 1u; half[0].ty1 = SH4DER_SPLIT_ROWS - 1u; half[0].hidden = 0; nh = 1u; }
    tw = ns();
    if (!wait_ready()) return 0;
    tw = ns() - tw;
    hdr_sent = hdr_bad = 0; tris_sent = 0;
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    for (h = 0; h < nh; h++)
        if (!half[h].hidden) { user_clip(&h_dum_op, half[h].tx0, half[h].ty0, half[h].tx1, half[h].ty1); send_half(v, v->mode[h], half[h].tx0, half[h].tx1, 0); }
    if (bucket_n[CORR_T_FLAT] && hdr(&h_flat)) for (i = 0; i < bucket_n[CORR_T_FLAT]; i++) send_poly(&list.p[bucket[CORR_T_FLAT][i]]);
    flush();
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    for (h = 0; h < nh; h++)
        if (!half[h].hidden && eff_mode(v->mode[h]) == CORR_M_TRILINEAR) {
            user_clip(&h_dum_tr, half[h].tx0, half[h].ty0, half[h].tx1, half[h].ty1); send_half(v, v->mode[h], half[h].tx0, half[h].tx1, 1);
        }
    if (bucket_n[CORR_T_FLAME] && hdr(&h_flame)) for (i = 0; i < bucket_n[CORR_T_FLAME]; i++) send_poly(&list.p[bucket[CORR_T_FLAME][i]]);
    if (v->split && !sh4der_split_at_end(v->d, SH4DER_SPLIT_COLS) && hdr(&h_dim)) rect(sh4der_split_x(v->d) - 1.0f, 0.0f, sh4der_split_x(v->d) + 1.0f, 480.0f, 12.0f, 0, 0, 0, 0, 0xc0ffffffu);
    text_line(0u, 16.0f); text_line(1u, 34.0f); text_line(2u, 52.0f);
    if (fx_on) text_line(3u, 446.0f);
    if (fx_on && !fx_hidden && fx_t.lines > 0u) { text_line(4u, 408.0f); if (fx_t.lines > 1u) text_line(5u, 426.0f); }
    flush();
    pvr_list_finish();
    rd_seen = 0;
    t1 = ns();
    pvr_scene_finish();
    t2 = ns();                                                /* the bins while the PVR renders: count-only, timed apart */
    corr_bins_clear(&bins_op); corr_bins_clear(&bins_tr);
    for (h = 0; h < nh; h++) if (!half[h].hidden) count_half(&list, v->mode[h], half[h].tx0, half[h].tx1);
    c->bin_op = corr_bins_max(&bins_op); c->bin_tr = corr_bins_max(&bins_tr);
    c->bins = ns() - t2;
    c->sh4 = (t1 - t0) - tw;
    if (!wait_ready() || !wait_render()) return 0;            /* hero1's order: ready, then render done */
    if (pvr_get_stats(&st) < 0) return 0;
    c->pvr = st.rnd_last_time; c->ta = (unsigned)st.vtx_buffer_used;
    c->polys = list.n; c->tris = tris_sent; c->hdrs = hdr_sent; c->hdr_bad = hdr_bad; c->dropped = list.dropped;
    return 1;
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
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    pvr_fog_table_color(1.0f, 0.0f, 0.0f, 0.0f);
    pvr_fog_table_linear(24.0f, 52.0f);                       /* black by the ring's far end: no hole at the end */
    text_mem = pvr_mem_malloc(sizeof text_px);
    for (t = 0; t < TXS; t++) tex_mem[t] = pvr_mem_malloc(tex_bytes[t]);
    for (t = 0; t < TXS; t++) if (!tex_mem[t]) break;
    if (!text_mem || t < TXS) { say("video memory allocation FAIL"); pvr_shutdown(); return 0; }
    for (t = 0; t < TXS; t++) pvr_txr_load(tex_src[t], tex_mem[t], tex_bytes[t]);
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

/* ---- the text on the screen ----------------------------------------------------------------------------------------- */

static const char *mode_text(const view_t *v, unsigned m) {
    static char s[2][24];
    static unsigned k;

    k ^= 1u;
    if (m == CORR_M_BIAS) snprintf(s[k], sizeof s[k], "bias %s", corr_sel(v->sel)->name);
    else snprintf(s[k], sizeof s[k], "%s", corr_mode_name(m));
    return s[k];
}

static void labels(const view_t *v, const cost_t *c, const char *stage) {
    const unsigned f = us_of(c->frame), s = us_of(c->sh4), p = us_of(c->pvr);

    if (v->split) text_set(0u, "%s | %s  %s", mode_text(v, v->mode[0]), mode_text(v, v->mode[1]), stage);
    else text_set(0u, "%s  %s", mode_text(v, v->mode[0]), stage);
    text_set(1u, "fr %u.%u SH4 %u.%u PVR %u.%u ms tri %u TA %uK hdr %u", f / 1000u, f % 1000u / 100u, s / 1000u, s % 1000u / 100u,
             p / 1000u, p % 1000u / 100u, c->tris, (c->ta + 512u) / 1024u, c->hdrs);
    text_set(2u, "bin OP %u TR %u  %u m/s %s %s  ref %u bad %u drop %u wait %u", c->bin_op, c->bin_tr, (unsigned)speeds[v->speed],
             v->big ? "256" : "128", v->walking ? "" : "STOP", refused, c->hdr_bad, c->dropped, giveups);
    text_set(3u, "X/Y mode B+X split B+LT/RT div ^v bias <> speed A stop B+Y size");
    memset(&fx_t, 0, sizeof fx_t);
    corr_fx_live(&fx_t, v->mode[0], corr_sel(v->sel)->fixed != 0u, v->split, v->mode[1], corr_sel(v->sel)->fixed != 0u);
    text_set(4u, "%s", fx_t.line[0]); text_set(5u, "%s", fx_t.line[1]);
}

/* ---- the loop, the autorun ---------------------------------------------------------------------------------------- */

#define CORR_STAGES 11u
typedef struct stage { uint8_t l, r, split, sel, big; const char *name; } stage_t;
static const stage_t stages[CORR_STAGES] = {
    { CORR_M_NEAREST, 0, 0, 0, 1, "nearest" },
    { CORR_M_BILINEAR, 0, 0, 0, 1, "bilinear" },
    { CORR_M_TRILINEAR, 0, 0, 0, 1, "trilinear" },
    { CORR_M_BIAS, 0, 0, CORR_SEL_SLOPE1_L2, 1, "bias slope 1.0 l2" },
    { CORR_M_BIAS, 0, 0, CORR_SEL_SLOPE1_L1, 1, "bias slope 1.0 l1" },
    { CORR_M_BIAS, 0, 0, CORR_SEL_FIXED050, 1, "bias fixed 0.50" },
    { CORR_M_BILINEAR, CORR_M_TRILINEAR, 1, 0, 1, "split bil | tri" },
    { CORR_M_BILINEAR, CORR_M_BIAS, 1, CORR_SEL_SLOPE1_L2, 1, "split bil | bias" },
    { CORR_M_NEAREST, CORR_M_BILINEAR, 1, 0, 1, "split near | bil" },
    { CORR_M_BILINEAR, 0, 0, 0, 0, "bilinear 128" },
    { CORR_M_TRILINEAR, 0, 0, 0, 0, "trilinear 128" },
};

typedef struct sums {
    uint64_t frame, sh4, xf, bins, pvr, ta;
    uint32_t polys, tris, hdrs, n, frame_max, ta_max;
    unsigned bin_op, bin_tr, dropped, hdr_bad;
    unsigned bin_unc;                        /* the first design's count, on one frame at the settle's end; 0: not taken */
    int touched, unc_done;
} sums_t;
static sums_t sums[CORR_STAGES];
static unsigned auto_done;

static void sums_add(sums_t *s, const cost_t *c) {
    s->frame += c->frame; s->sh4 += c->sh4; s->xf += c->xf; s->bins += c->bins; s->pvr += c->pvr; s->ta += c->ta;
    s->polys += c->polys; s->tris += c->tris; s->hdrs += c->hdrs; s->n++;
    if (us_of(c->frame) > s->frame_max) s->frame_max = us_of(c->frame);
    if (c->ta > s->ta_max) s->ta_max = c->ta;
    if (c->bin_op > s->bin_op) s->bin_op = c->bin_op;
    if (c->bin_tr > s->bin_tr) s->bin_tr = c->bin_tr;
    s->dropped += c->dropped; s->hdr_bad += c->hdr_bad;
}

static void view_stage(view_t *v, unsigned k) {
    const stage_t *s = &stages[k];

    v->mode[0] = s->l; v->mode[1] = s->r; v->split = s->split; v->sel = s->sel; v->big = s->big;
    v->d = sh4der_split_middle(SH4DER_SPLIT_COLS); v->speed = CORR_AUTO_SPEED; v->walking = 1;
    v->eye.s = 2.0f; v->eye.x = 0.0f; v->eye.yaw = 0.0f; v->t = 0.0f; v->steer = 0.0f;
}

static sh4der_split_step_t sstep;

static void loop(int auto_run) {
    view_t v;
    cost_t c, shown;
    sums_t win;
    unsigned held = ~0u, frames = 0, stage = 0;
    uint64_t t_prev = ns(), t_stage = ns();

    memset(&v, 0, sizeof v); memset(&c, 0, sizeof c); memset(&shown, 0, sizeof shown); memset(&win, 0, sizeof win);
    view_stage(&v, 1u);
    if (auto_run) { view_stage(&v, 0u); memset(sums, 0, sizeof sums); auto_done = 0; }
    fx_on = !auto_run;                                        /* the labels and the key line: live view only */
    labels(&v, &shown, auto_run ? stages[0].name : "live");
    text_upload();
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;
        const int b_held = (now & CONT_B) != 0u;
        const uint64_t t_now = ns();
        float dt = (float)(t_now - t_prev) * 1.0e-9f;
        int changed = 0, touched = 0;

        step("corr frame ");
        c.frame = t_now - t_prev;
        t_prev = t_now; held = now;
        if (dt > 0.1f) dt = 0.1f;
        if (auto_run && t_now - t_stage > (uint64_t)CORR_AUTO_S * 1000000000ull) {
            t_stage = t_now; changed = 1;
            if (++stage >= CORR_STAGES) { auto_done = stage; break; }
            view_stage(&v, stage);
        }
        if (!auto_run && (hit & CONT_START) && !b_held && frames > 10u) break;
        if (b_held) {
            if (hit & CONT_X) { v.split = !v.split; changed = 1; }
            if (hit & CONT_Y) { v.big = !v.big; changed = 1; }
            if (fx_hide_chord(1, (hit & CONT_START) != 0u, &fx_hidden)) changed = 1;
            if (st && v.split) {
                const unsigned d = sh4der_split_step(&sstep, v.d, SH4DER_SPLIT_COLS, st->ltrig > 128, st->rtrig > 128);

                if (d != v.d) { v.d = d; changed = 1; }
            }
            if (hit & (CONT_X | CONT_Y | CONT_START)) touched = 1;
            if (st && (st->ltrig > 128 || st->rtrig > 128)) touched = 1;
        } else {
            if (hit & CONT_X) { v.mode[0] = corr_mode_next(v.mode[0]); changed = 1; }
            if (hit & CONT_Y) { v.mode[1] = corr_mode_next(v.mode[1]); changed = 1; }
            if (hit & CONT_A) { v.walking = !v.walking; changed = 1; }
            if (st) {
                if (st->ltrig > 24) v.eye.x -= (float)st->ltrig * (1.0f / 255.0f) * dt * 1.5f;
                if (st->rtrig > 24) v.eye.x += (float)st->rtrig * (1.0f / 255.0f) * dt * 1.5f;
                v.eye.x = sh4der_clamp_(v.eye.x, -1.0f, 1.0f);
                if (st->ltrig > 24 || st->rtrig > 24) touched = 1;
            }
            if (hit & (CONT_X | CONT_Y | CONT_A)) touched = 1;
        }
        if (hit & CONT_DPAD_UP) { v.sel = (v.sel + 1u) % CORR_SEL_N; changed = touched = 1; }
        if (hit & CONT_DPAD_DOWN) { v.sel = (v.sel + CORR_SEL_N - 1u) % CORR_SEL_N; changed = touched = 1; }
        if (hit & CONT_DPAD_RIGHT) { if (v.speed < 3u) v.speed++; changed = touched = 1; }
        if (hit & CONT_DPAD_LEFT) { if (v.speed) v.speed--; changed = touched = 1; }
        if (st && (st->joyx > 24 || st->joyx < -24)) { v.steer += (float)st->joyx * (1.0f / 128.0f) * dt * 1.2f; touched = 1; }
        v.steer = sh4der_clamp_(v.steer, -0.6f, 0.6f);
        if (auto_run && touched) sums[stage].touched = 1;
        if (changed) memset(&win, 0, sizeof win);
        if (!frame(&v, &c, dt)) { say("%s: NO RENDER END at frame %u (stage %u)", auto_run ? "auto" : "live", frames, stage + 1u); break; }
        if (frames == 0u) c.frame = 0u;
        frames++;
        if (auto_run && !sums[stage].unc_done && t_now - t_stage > (uint64_t)CORR_SETTLE_S * 1000000000ull - 250000000ull) {
            sums[stage].bin_unc = uncapped_bins(&v);          /* in the settle: the next frame's interval is not counted */
            sums[stage].unc_done = 1;
        }
        if (auto_run && t_now - t_stage > (uint64_t)CORR_SETTLE_S * 1000000000ull && c.frame) sums_add(&sums[stage], &c);
        sums_add(&win, &c);
        if (changed || win.n >= 30u) {
            if (win.n) {
                shown.frame = win.frame / win.n; shown.sh4 = win.sh4 / win.n; shown.pvr = win.pvr / win.n;
                shown.tris = win.tris / win.n; shown.ta = (unsigned)(win.ta / win.n); shown.hdrs = win.hdrs / win.n;
                shown.bin_op = win.bin_op; shown.bin_tr = win.bin_tr; shown.dropped = win.dropped; shown.hdr_bad = win.hdr_bad;
            }
            if (win.n >= 30u) memset(&win, 0, sizeof win);
            labels(&v, &shown, auto_run ? stages[stage].name : "live");
            text_upload();                                    /* after the render: the text texture is not in use */
        }
    }
}

/* ---- the pages ----------------------------------------------------------------------------------------------------- */

static float ms_(uint64_t t) { return (float)us_of(t) * 1.0e-3f; }

static void report(void) {
    pg_plot_t *g;
    char rev[32];
    unsigned k, worst = 0;

    suite_rev_text(rev, sizeof rev);
    say("corr %s: the PVR's texture filters down an endless corridor.", CORR_STR(CORR_TAG));
    say("%s. %u s a stage, the first %u s not counted, every frame after", rev, CORR_AUTO_S, CORR_SETTLE_S);
    say("ms; fr frame, SH4 CPU less the ready wait, PVR render (pvr_get_stats)");
    say("tri sent, TA KB a frame (mean), hdr headers, bO/bT most filtered");
    say("opaque/translucent polygon boxes on one 32x32 bin (max, count-only)");
    if (auto_done < CORR_STAGES) say("THE RUN DID NOT FINISH: the lines below are of what ran");
    say("#  stage              fr   SH4   PVR   tri  TA hdr bO bT   n");
    for (k = 0; k < CORR_STAGES; k++) {
        const sums_t *s = &sums[k];
        const unsigned n = s->n ? s->n : 1u;

        say("%-2u %-17s %5.2f %5.2f %5.2f %5u %3u %3u %2u %2u %4u%s", k + 1u, stages[k].name, (double)ms_(s->frame / n), (double)ms_(s->sh4 / n),
            (double)ms_(s->pvr / n), s->tris / n, (unsigned)((s->ta / n + 512u) / 1024u), s->hdrs / n, s->bin_op, s->bin_tr, s->n, s->touched ? " TOUCHED" : "");
        if (s->bin_op > worst) worst = s->bin_op;
    }
    say("#  stage              fr max  xf+bucket  bins TApk drop bad uncap");
    for (k = 0; k < CORR_STAGES; k++) {
        const sums_t *s = &sums[k];
        const unsigned n = s->n ? s->n : 1u;

        say("%-2u %-17s %6.2f  %9.2f %5.2f %4u %4u %3u %5u", k + 1u, stages[k].name, (double)s->frame_max * 1e-3, (double)ms_(s->xf / n),
            (double)ms_(s->bins / n), (unsigned)((s->ta_max + 512u) / 1024u), s->dropped, s->hdr_bad, s->bin_unc);
    }
    say("TOUCHED: a key was pressed in that stage. Waits given up (200 ms): %u", giveups);
    say("guard (a) refused headers: %u (filtered on non-twiddled/bump)", refused);
    say("CORE 26 (PVR-071, REPORTED skmp): 8th filtered OP poly a tile,");
    say("  non-twiddled bump textures. Here all twiddled+mip. Most a bin: %u%s", worst, worst > 7u ? " (OVER 7: for Lead)" : "");
    say("  bin counts are box upper bounds, count-only, nothing dropped");
    say("FAR CAP (corr-a2): a segment 16 m or more ahead is one quad a");
    say("  surface, same texture repeat; the far centre has far fewer");
    say("  polygons than the first design. bO: capped (every frame, max);");
    say("  uncap: the first design's count on one frame at the settle end.");
    say("TRILINEAR as used: pass A PVR_FILTER_TRILINEAR1, OP list (ONE x");
    say("  ZERO); pass B PVR_FILTER_TRILINEAR2, TR list, blend ONE x ONE,");
    say("  depth GEQUAL, no depth write, same vertices; MODULATE, table fog.");
    say("  From the hardware manual 3.4.7.2.3; KOS pvr_header.h names the");
    say("  two passes only. RESULT OPEN until read off the screen.");
    say("BIAS: per polygon, header D adjust; slope from sh4der_fakeaf.h");
    say("  faf_bias_code(|n.v|, strength, least): strength held to 0..1,");
    say("  never over 1.00 (code 4), never under 'least' (l1 0.25, l2 0.50).");
    say("  Batched: one header a code that has polygons. OPEN.");
    say("RAMP blend: a hook only (corr_ramp_draw returns 0: drawn bilinear).");
    say("Far quads: light baked at their corners; T-junctions at 16 m.");
    say("Textures: tiles NEAREST-upscaled to 256 (floor, wall) or 128,");
    say("  box mips to 1x1, RGB565 twiddled. Fog black 24..52 m.");
    say("Frames are serial: SH-4 sends, then waits for the render.");
    g = pg_plot("corr: frame and PVR render a stage, ms", "stage", "ms", 1);
    for (k = 0; k < CORR_STAGES; k++) { char s[4]; snprintf(s, sizeof s, "%u", k + 1u); pg_cat(g, s); }
    pg_series(g, "frame"); for (k = 0; k < CORR_STAGES; k++) pg_point(g, (float)k, ms_(sums[k].frame / (sums[k].n ? sums[k].n : 1u)));
    pg_series(g, "PVR"); for (k = 0; k < CORR_STAGES; k++) pg_point(g, (float)k, ms_(sums[k].pvr / (sums[k].n ? sums[k].n : 1u)));
}

int main(void) {
    unsigned page = 0, pages, held = ~0u;
    int redraw = 1, fault_on = 0;

    suite_init();
    vid_set_mode(DM_640x480, PM_RGB565);
    pg_init();
    vid_clear(0, 0, 0);
    pg_text(PG_X0, PG_Y0, PG_WHITE, "corr: building the corridor ...");
    corr_build(kinds);
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
        if (hit & CONT_START) {
            if (start()) { loop(0); stop("live end"); }
            vid_set_mode(DM_640x480, PM_RGB565);
            held = ~0u; suite_page_forget(); redraw = 1;
        }
        else if (suite_page(now, &page, pages)) redraw = 1;   /* A next, B before; on the last page hold A 2 s, or A+B+X+Y anywhere: the menu */
        if (redraw) { pg_draw(page % pages, "CORR " CORR_STR(CORR_TAG), suite_keys_text("START live view")); fx_page(corr_fx_page, CORR_FX_PAGE_N); redraw = 0; }
        thd_sleep(20);
    }
    return 0;
}
