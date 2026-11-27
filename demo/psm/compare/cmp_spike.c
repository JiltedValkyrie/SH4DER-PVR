/* SH4DER-PVR: the PVR-side compare spike, the Dreamcast test program

   demo/psm/compare/cmp_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A test program for a real console. It asks whether the PVR can make the shadow decision itself (cmp_model.h has
   the idea and the model), one step a question, and prints numbers on result pages (A next, B back) and on the
   serial line ("cmp:" lines). Each step draws into the screen pass over a 256 x 256 square and reads the frame buffer
   back with the SH-4, as the route AB spike's dither test did.

     1 BLEND   the compare by saturating adds, invert and eight doublings. a: an 8-bit paletted ramp against an
               8-bit paletted map. b: against an RGB565 map (what the PVR itself can draw). c: the receiver's depth
               from Gouraud vertex colour instead of a ramp texture.           -> routes C1 and F
     2 ALPHA   the same compare in the frame's destination alpha, the colour left alone until a last layer
               multiplies it by 1 - alpha.                                     -> route C2
     3 LAYERS  the PVR's render time (pvr_get_stats) for 0, 1, 2, 4, 8 full translucent layers, on the 640 x 480
               screen and in a 320 x 240 render target. A count is only tried if the one before it finished.
     4 PUNCH   the punch-through alpha test on texture alpha x vertex alpha.     -> route C3
     5 PALETTE a palette entry changed between two draws of one scene.           -> route D
     6 VOLUME  a modifier volume (cheap shadow) over a sloped quad.              -> route E

   The boundary numbers: R runs across x and M down y, a level a pixel, so the shadow should begin at x = y + 1 on
   every row. "err a..b" is where it did begin, against that, over the rows; "rag" rows with more than one edge;
   "btw" pixels that are neither lit nor shadow.

   The fault catcher (include/sh4der_fault.h) is in: a hang or an exception shows its own screen, with the step it was in.
   Every number comes from the run that prints it. Only a real console's run is evidence.
   Build: python demo/psm/compare/build.py.
*/

#include <kos.h>
#include <stdarg.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"
#include "cmp_model.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

/* ---- the result pages ----------------------------------------------------------- */

#define COLS 50u
#define ROWS 17u
#define LINES_MAX 80u
static char lines[LINES_MAX][COLS + 1u];
static unsigned nlines;
static sh4der_fault_crumbs_t crumbs;

static void say(const char *fmt, ...) {
    char big[160];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(big, sizeof big, fmt, ap);
    va_end(ap);
    printf("cmp: %s\n", big);
    if (nlines < LINES_MAX) { memcpy(lines[nlines], big, COLS); lines[nlines][COLS] = 0; nlines++; }
}

static void page_draw(unsigned page, unsigned pages) {
    char head[96];
    unsigned i;

    vid_clear(0, 0, 0);
    snprintf(head, sizeof head, "PVR COMPARE c1 p%u/%u   A next  B back", page + 1u, pages);
    bfont_draw_str(vram_s + 16u * 640u + 20u, 640, true, head);
    for (i = 0; i < ROWS && page * ROWS + i < nlines; i++)
        bfont_draw_str(vram_s + (16u + 24u * (i + 2u)) * 640u + 20u, 640, true, lines[page * ROWS + i]);
}

/* A step begins: the heartbeat, and its name in the breadcrumbs for the fault screen. */
static void step(const char *name) {
    sh4der_fault_beat(&crumbs);
    sh4der_fault_stage(&crumbs, name);
    crumbs.frame++;
}

/* ---- textures, palette, headers --------------------------------------------------- */

static pvr_ptr_t ramp_mem, map_mem, rtt_mem;
static uint8_t ramp_px[CMP_N * CMP_N];
static uint16_t map_px[CMP_N * CMP_N] __attribute__((aligned(32)));
static uint8_t img[CMP_N * CMP_N], model[CMP_N * CMP_N];
static uint16_t raw[2];

enum { TEX_NONE = 0, TEX_PAL = 1, TEX_565 = 2 };
enum { BANK_GREY = 0, BANK_ALPHA = 1, BANK_WHITE_ALPHA = 2, BANK_SWAP = 3 };

static int load(void) {
    unsigned i, x, y;

    ramp_mem = pvr_mem_malloc(sizeof ramp_px);
    map_mem = pvr_mem_malloc(sizeof map_px);
    rtt_mem = pvr_mem_malloc(320u * 240u * 2u);
    if (!ramp_mem || !map_mem || !rtt_mem) return 0;
    for (y = 0; y < CMP_N; y++) {
        for (x = 0; x < CMP_N; x++) {
            ramp_px[y * CMP_N + x] = (uint8_t)x;                                  /* the texel is its own column */
            map_px[y * CMP_N + x] = (uint16_t)(((x >> 3) << 11) | ((x >> 2) << 5) | (x >> 3));   /* the same as RGB565 grey */
        }
    }
    pvr_txr_load_ex(ramp_px, ramp_mem, CMP_N, CMP_N, PVR_TXRLOAD_8BPP);          /* twiddled on the way in */
    pvr_txr_load(map_px, map_mem, sizeof map_px);
    pvr_set_pal_format(PVR_PAL_ARGB8888);
    for (i = 0; i < 256u; i++) {
        pvr_set_pal_entry(BANK_GREY * 256u + i, 0xff000000u | (i << 16) | (i << 8) | i);   /* level i as a grey */
        pvr_set_pal_entry(BANK_ALPHA * 256u + i, i << 24);                                 /* level i as alpha, no colour */
        pvr_set_pal_entry(BANK_WHITE_ALPHA * 256u + i, (i << 24) | 0x00ffffffu);           /* level i as alpha, white */
        pvr_set_pal_entry(BANK_SWAP * 256u + i, 0xff000000u);
    }
    return 1;
}

/* A header for one layer: the list, the texture (none, the paletted ramp through a bank, or the RGB565 map), the
   blend, the texture environment. Every layer passes the depth test and writes no depth: order is the order sent. */
static void mk(pvr_poly_hdr_t *h, pvr_list_t list, int tex, unsigned bank, pvr_blend_mode_t src, pvr_blend_mode_t dst, pvr_txr_shading_mode_t env) {
    pvr_poly_cxt_t c;

    if (tex == TEX_PAL) pvr_poly_cxt_txr(&c, list, PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(bank), (int)CMP_N, (int)CMP_N, ramp_mem, PVR_FILTER_NEAREST);
    else if (tex == TEX_565) pvr_poly_cxt_txr(&c, list, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)CMP_N, (int)CMP_N, map_mem, PVR_FILTER_NEAREST);
    else pvr_poly_cxt_col(&c, list);
    if (tex != TEX_NONE) { c.txr.env = env; c.txr.alpha = PVR_TXRALPHA_ENABLE; }
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.alpha = list != PVR_LIST_OP_POLY;
    c.blend.src = src; c.blend.dst = dst;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = list == PVR_LIST_OP_POLY ? PVR_DEPTHWRITE_ENABLE : PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(h, &c);
}

enum { UV_NONE = 0, UV_X = 1, UV_X_BACK = 2, UV_Y = 3, UV_FIXED = 4 };

/* A rectangle. `uv`: the ramp's level runs with x (level = x), against x (level = 255 - x), with y, or is the one
   level `fixed`. Colours: top left, bottom left, top right, bottom right. Depth zl at the left, zr at the right. */
static void quad(const pvr_poly_hdr_t *h, float x0, float y0, float x1, float y1, int uv, unsigned fixed,
                 uint32_t ctl, uint32_t cbl, uint32_t ctr, uint32_t cbr, float zl, float zr) {
    pvr_vertex_t v;
    unsigned i;

    pvr_prim((void *)h, sizeof *h);
    memset(&v, 0, sizeof v);
    for (i = 0; i < 4u; i++) {                                   /* bottom left, top left, bottom right, top right */
        const int right = (i & 2u) != 0, top = (i & 1u) != 0;
        const float fx = right ? (x1 - x0) / (float)CMP_N : 0.0f, fy = top ? 0.0f : (y1 - y0) / (float)CMP_N;

        v.flags = i == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = right ? x1 : x0; v.y = top ? y0 : y1; v.z = right ? zr : zl;
        v.argb = right ? (top ? ctr : cbr) : (top ? ctl : cbl);
        v.v = 0.5f;
        if (uv == UV_X) v.u = fx;
        else if (uv == UV_X_BACK) v.u = 1.0f - fx;
        else if (uv == UV_Y) v.u = fy;
        else if (uv == UV_FIXED) v.u = ((float)fixed + 0.5f) / (float)CMP_N;
        pvr_prim(&v, sizeof v);
    }
}

#define WHITE 0xffffffffu
#define SQ(h, uv, c) quad(h, 0.0f, 0.0f, 256.0f, 256.0f, uv, 0u, c, c, c, c, 1.0f, 1.0f)

/* ---- passes ----------------------------------------------------------------------- */

static uint32_t render_addr;

/* Open a pass: the screen, or a w x h render target. 0 when KallistiOS refused it. */
static int scene_open(pvr_ptr_t mem, unsigned w, unsigned h) {
    pvr_wait_ready();
    if (mem) return pvr_scene_begin_rtt(mem, w, h, w) >= 0;
    pvr_scene_begin();
    return 1;
}

/* Close it and wait for the render to start and to end. 1 when both happened. */
static int scene_close(void) {
    unsigned t;

    pvr_scene_finish();
    for (t = 0; t < 20u && pvr_wait_ready() < 0; t++) sh4der_fault_beat(&crumbs);
    if (t == 20u) return 0;
    for (t = 0; t < 20u && pvr_wait_render_done() < 0; t++) sh4der_fault_beat(&crumbs);
    render_addr = PVR_GET(PVR_RENDER_ADDR);
    return t < 20u;
}

static uint64_t render_ns(void) {
    pvr_stats_t st;

    return pvr_get_stats(&st) < 0 ? 0 : st.rnd_last_time;
}

/* The 256 x 256 square of the frame just rendered, green channel, as 8 bits (6 bits kept, spread as a texel's are). */
static void read_square(void) {
    const volatile uint16_t *fb = (const volatile uint16_t *)(uintptr_t)(0xa5000000u | (render_addr & 0x00ffffffu));
    unsigned x, y;

    for (y = 0; y < CMP_N; y++) {
        for (x = 0; x < CMP_N; x++) {
            const unsigned g = (fb[y * 640u + x] >> 5) & 63u;

            img[y * CMP_N + x] = (uint8_t)((g << 2) | (g >> 4));
        }
    }
    raw[0] = fb[128u * 640u + 32u];
    raw[1] = fb[128u * 640u + 96u];
}

static void say_boundary(const char *tag, unsigned lit, unsigned shadow) {
    cmp_stats_t st;

    cmp_boundary(img, CMP_N, lit, shadow, 8u, &st);
    say("%s err %+d..%+d found %u rag %u btw %u", tag, st.min_err, st.max_err, st.found, st.ragged, st.between);
    if (st.found < 250u) say("   lit %u shadow %u of 65536 px", st.lit, st.shadow);
}

/* ---- 1. the blend compare ---------------------------------------------------------- */

static void blend(const char *tag, int map_tex, int gouraud) {
    pvr_poly_hdr_t ha, hb, hc, hd;
    unsigned i;

    step(tag);
    mk(&ha, PVR_LIST_OP_POLY, gouraud ? TEX_NONE : TEX_PAL, BANK_GREY, PVR_BLEND_ONE, PVR_BLEND_ZERO, PVR_TXRENV_REPLACE);
    mk(&hb, PVR_LIST_TR_POLY, map_tex, BANK_GREY, PVR_BLEND_ONE, PVR_BLEND_ONE, PVR_TXRENV_REPLACE);
    mk(&hc, PVR_LIST_TR_POLY, TEX_NONE, 0u, PVR_BLEND_INVDESTCOLOR, PVR_BLEND_ZERO, PVR_TXRENV_REPLACE);
    mk(&hd, PVR_LIST_TR_POLY, TEX_NONE, 0u, PVR_BLEND_DESTCOLOR, PVR_BLEND_ONE, PVR_TXRENV_REPLACE);
    if (!scene_open(NULL, 0u, 0u)) { say("%s NOT DRAWN", tag); return; }
    pvr_list_begin(PVR_LIST_OP_POLY);
    if (gouraud) quad(&ha, 0.0f, 0.0f, 256.0f, 256.0f, UV_NONE, 0u, WHITE, WHITE, 0xff000000u, 0xff000000u, 1.0f, 1.0f);   /* a: 255 - R by colour */
    else SQ(&ha, UV_X_BACK, WHITE);                              /* a: 255 - R by the ramp */
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    SQ(&hb, UV_Y, WHITE);                                        /* b: + M, saturating */
    SQ(&hc, UV_NONE, WHITE);                                     /* c: 1 - that */
    for (i = 0; i < 8u; i++) SQ(&hd, UV_NONE, WHITE);            /* d: doubled eight times */
    pvr_list_finish();
    if (!scene_close()) { say("%s NO RENDER END", tag); return; }
    read_square();
    say_boundary(tag, 0u, 255u);
}

/* ---- 2. the same in destination alpha ---------------------------------------------- */

static void alpha(void) {
    pvr_poly_hdr_t h0, hz, hadd, hinv, hdbl, hfin;
    unsigned i;

    step("2 alpha");
    mk(&h0, PVR_LIST_OP_POLY, TEX_NONE, 0u, PVR_BLEND_ONE, PVR_BLEND_ZERO, PVR_TXRENV_REPLACE);
    mk(&hz, PVR_LIST_TR_POLY, TEX_NONE, 0u, PVR_BLEND_ZERO, PVR_BLEND_DESTCOLOR, PVR_TXRENV_REPLACE);        /* dst x (1,1,1,0) */
    mk(&hadd, PVR_LIST_TR_POLY, TEX_PAL, BANK_ALPHA, PVR_BLEND_ONE, PVR_BLEND_ONE, PVR_TXRENV_REPLACE);      /* + (0,0,0,level) */
    mk(&hinv, PVR_LIST_TR_POLY, TEX_NONE, 0u, PVR_BLEND_INVDESTALPHA, PVR_BLEND_INVDESTCOLOR, PVR_TXRENV_REPLACE);   /* alpha = 1 - alpha */
    mk(&hdbl, PVR_LIST_TR_POLY, TEX_NONE, 0u, PVR_BLEND_DESTALPHA, PVR_BLEND_ONE, PVR_TXRENV_REPLACE);       /* alpha doubled */
    mk(&hfin, PVR_LIST_TR_POLY, TEX_NONE, 0u, PVR_BLEND_ZERO, PVR_BLEND_INVDESTALPHA, PVR_TXRENV_REPLACE);   /* colour x (1 - alpha) */
    if (!scene_open(NULL, 0u, 0u)) { say("2 NOT DRAWN"); return; }
    pvr_list_begin(PVR_LIST_OP_POLY);
    SQ(&h0, UV_NONE, 0xffc8c8c8u);                               /* the picture: grey 200 */
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    SQ(&hz, UV_NONE, 0x00ffffffu);                               /* alpha to 0, colour kept */
    SQ(&hadd, UV_X_BACK, WHITE);                                 /* alpha = 255 - R */
    SQ(&hadd, UV_Y, WHITE);                                      /* + M */
    SQ(&hinv, UV_NONE, 0xff000000u);
    for (i = 0; i < 8u; i++) SQ(&hdbl, UV_NONE, 0xff000000u);
    SQ(&hfin, UV_NONE, 0xff000000u);
    pvr_list_finish();
    if (!scene_close()) { say("2 NO RENDER END"); return; }
    read_square();
    say_boundary("2 dst alpha:", 200u, 0u);
}

/* ---- 3. what layers cost ------------------------------------------------------------- */

/* The render time, in us, of a pass of `n` full translucent layers over an opaque one (0: none came back). */
static unsigned long layers_us(pvr_ptr_t mem, unsigned w, unsigned h, unsigned n) {
    pvr_poly_hdr_t h0, h1;
    uint64_t lo = ~0ull, t;
    unsigned i, r;

    mk(&h0, PVR_LIST_OP_POLY, TEX_NONE, 0u, PVR_BLEND_ONE, PVR_BLEND_ZERO, PVR_TXRENV_REPLACE);
    mk(&h1, PVR_LIST_TR_POLY, TEX_NONE, 0u, PVR_BLEND_ONE, PVR_BLEND_ONE, PVR_TXRENV_REPLACE);
    for (r = 0; r < 2u; r++) {
        sh4der_fault_beat(&crumbs);
        if (!scene_open(mem, w, h)) return 0;
        pvr_list_begin(PVR_LIST_OP_POLY);
        quad(&h0, 0.0f, 0.0f, (float)w, (float)h, UV_NONE, 0u, 0xff202020u, 0xff202020u, 0xff202020u, 0xff202020u, 1.0f, 1.0f);
        pvr_list_finish();
        pvr_list_begin(PVR_LIST_TR_POLY);
        for (i = 0; i < n; i++) quad(&h1, 0.0f, 0.0f, (float)w, (float)h, UV_NONE, 0u, 0xff010101u, 0xff010101u, 0xff010101u, 0xff010101u, 1.0f, 1.0f);
        pvr_list_finish();
        if (!scene_close()) return 0;
        t = render_ns();
        if (t < lo) lo = t;
    }
    return (unsigned long)(lo / 1000u);
}

static void layers(const char *tag, pvr_ptr_t mem, unsigned w, unsigned h) {
    static const unsigned counts[5] = { 0u, 1u, 2u, 4u, 8u };
    unsigned long us[5] = { 0, 0, 0, 0, 0 };
    unsigned i, done = 0;

    step(tag);
    for (i = 0; i < 5u; i++) {                                   /* climb only while the last count came back, and in good time */
        us[i] = layers_us(mem, w, h, counts[i]);
        if (!us[i]) break;
        done++;
        if (us[i] > 100000ul) break;
    }
    say("%s 0:%lu 1:%lu 2:%lu", tag, us[0], us[1], us[2]);
    say("   4:%lu 8:%lu us (%u of 5 came back)", us[3], us[4], done);
}

/* ---- 4. punch-through on a product ---------------------------------------------------- */

static void punch(void) {
    pvr_poly_hdr_t h;
    unsigned x, y, drawn = 0, diff[CMP_RULES];
    int rule;

    step("4 punch");
    mk(&h, PVR_LIST_PT_POLY, TEX_PAL, BANK_WHITE_ALPHA, PVR_BLEND_ONE, PVR_BLEND_ZERO, PVR_TXRENV_MODULATEALPHA);   /* a pixel that passes is white */
    PVR_SET(PVR_PT_ALPHA_REF, 64);
    if (!scene_open(NULL, 0u, 0u)) { say("4 NOT DRAWN"); return; }
    pvr_list_begin(PVR_LIST_PT_POLY);
    quad(&h, 0.0f, 0.0f, 256.0f, 256.0f, UV_X, 0u, 0xffffffffu, 0x00ffffffu, 0xffffffffu, 0x00ffffffu, 1.0f, 1.0f);   /* texture alpha x, vertex alpha 255 - y */
    pvr_list_finish();
    if (!scene_close()) { say("4 NO RENDER END"); return; }
    read_square();
    for (x = 0; x < CMP_N * CMP_N; x++) { img[x] = img[x] > 127u; drawn += img[x]; }
    for (rule = 0; rule < (int)CMP_RULES; rule++) {
        for (y = 0; y < CMP_N; y++) for (x = 0; x < CMP_N; x++) model[y * CMP_N + x] = (uint8_t)cmp_punch_pixel(x, y, 64u, rule);
        diff[rule] = cmp_count_diff(img, model);
    }
    say("4 punch: %u px drawn, ref 64", drawn);
    say("   differ from model rules: %u %u %u %u", diff[0], diff[1], diff[2], diff[3]);
}

/* ---- 5. the palette, changed inside a scene ------------------------------------------- */

static void palette(void) {
    pvr_poly_hdr_t h;

    step("5 palette");
    mk(&h, PVR_LIST_OP_POLY, TEX_PAL, BANK_SWAP, PVR_BLEND_ONE, PVR_BLEND_ZERO, PVR_TXRENV_REPLACE);
    if (!scene_open(NULL, 0u, 0u)) { say("5 NOT DRAWN"); return; }
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_set_pal_entry(BANK_SWAP * 256u + 10u, 0xffff0000u);      /* red while A is sent */
    quad(&h, 0.0f, 0.0f, 64.0f, 256.0f, UV_FIXED, 10u, WHITE, WHITE, WHITE, WHITE, 1.0f, 1.0f);
    pvr_set_pal_entry(BANK_SWAP * 256u + 10u, 0xff0000ffu);      /* blue while B is sent */
    quad(&h, 64.0f, 0.0f, 128.0f, 256.0f, UV_FIXED, 10u, WHITE, WHITE, WHITE, WHITE, 1.0f, 1.0f);
    pvr_list_finish();
    if (!scene_close()) { say("5 NO RENDER END"); return; }
    read_square();
    say("5 palette red for A, blue for B: A=%04x B=%04x", raw[0], raw[1]);
    say("   (f800 red, 001f blue; same = one palette a scene)");
}

/* ---- 6. a modifier volume --------------------------------------------------------------- */

static void mod_tri(float ax, float ay, float bx, float by, float cx, float cy, float z) {
    pvr_modifier_vol_t m;

    memset(&m, 0, sizeof m);
    m.flags = PVR_CMD_VERTEX_EOL;
    m.ax = ax; m.ay = ay; m.az = z;
    m.bx = bx; m.by = by; m.bz = z;
    m.cx = cx; m.cy = cy; m.cz = z;
    pvr_prim(&m, sizeof m);
}

/* The sloped white quad, with the box's near and far faces as a volume (`with`) or without. Returns the render time
   in us, 0 when the pass did not come back. The box's four side faces are edge-on to the screen (no area), so they
   are not sent. */
static unsigned long volume_pass(int with) {
    const float lo = (float)CMP_VOL_LO, hi = (float)CMP_VOL_HI;
    pvr_poly_cxt_t c;
    pvr_poly_hdr_t h;
    pvr_mod_hdr_t mh, mlast;

    sh4der_fault_beat(&crumbs);
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.modifier_mode = PVR_MODIFIER_CHEAP_SHADOW;
    c.fmt.modifier = PVR_MODIFIER_ENABLE;
    pvr_poly_compile(&h, &c);
    pvr_mod_compile(&mh, PVR_LIST_OP_MOD, PVR_MODIFIER_OTHER_POLY, PVR_CULLING_NONE);
    pvr_mod_compile(&mlast, PVR_LIST_OP_MOD, PVR_MODIFIER_INCLUDE_LAST_POLY, PVR_CULLING_NONE);
    if (!scene_open(NULL, 0u, 0u)) return 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    quad(&h, 0.0f, 0.0f, 256.0f, 256.0f, UV_NONE, 0u, WHITE, WHITE, WHITE, WHITE, CMP_VOL_Z0, CMP_VOL_Z1);
    pvr_list_finish();
    if (with) {
        pvr_list_begin(PVR_LIST_OP_MOD);
        pvr_prim(&mh, sizeof mh);
        mod_tri(lo, hi, lo, lo, hi, hi, CMP_VOL_ZB);             /* the near face */
        mod_tri(lo, lo, hi, hi, hi, lo, CMP_VOL_ZB);
        mod_tri(lo, hi, lo, lo, hi, hi, CMP_VOL_ZA);             /* the far face */
        pvr_prim(&mlast, sizeof mlast);
        mod_tri(lo, lo, hi, hi, hi, lo, CMP_VOL_ZA);
        pvr_list_finish();
    }
    if (!scene_close()) return 0;
    return (unsigned long)(render_ns() / 1000u);
}

static void volume(void) {
    unsigned x, y, n = 0, x0 = 999u, x1 = 0u, y0 = 999u, y1 = 0u;
    unsigned long plain, with;

    step("6 volume");
    pvr_set_shadow_scale(true, 0.5f);
    plain = volume_pass(0);
    with = volume_pass(1);
    if (!with) { say("6 NOT DRAWN"); return; }
    read_square();
    for (y = 0; y < CMP_N; y++) {
        for (x = 0; x < CMP_N; x++) {
            const int sh = img[y * CMP_N + x] < 190u;            /* white 255 outside, about 127 inside */

            img[y * CMP_N + x] = (uint8_t)sh;
            model[y * CMP_N + x] = (uint8_t)cmp_volume_pixel(x, y);
            if (!sh) continue;
            n++;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
    }
    say("6 volume: %u px in shadow (model 11008)", n);
    say("   %u differ; box x%u-%u y%u-%u (85-170 64-191)", cmp_count_diff(img, model), x0, x1, y0, y1);
    say("   render %lu us with, %lu us without", with, plain);
    pvr_set_shadow_scale(false, 1.0f);
}

/* ---- the run -------------------------------------------------------------------------- */

static void run(void) {
    pvr_init_params_t init = pvr_default_params;

    init.opb_sizes[PVR_LIST_OP_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_OP_MOD] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_TR_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_PT_POLY] = PVR_BINSIZE_16;
    init.vertex_buf_size = 256 * 1024;
    init.autosort_disabled = 1;                                  /* translucent layers in the order they are sent */
    say("numbers below: measured by this run");
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return; }
    if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused");
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    if (!load()) { say("video memory allocation FAIL"); pvr_shutdown(); return; }
    vid_set_dithering(false);

    say("1 BLEND compare: boundary vs ideal, levels");
    blend("1a 8-bit map: ", TEX_PAL, 0);
    blend("1b 565 map:   ", TEX_565, 0);
    blend("1c gouraud R: ", TEX_PAL, 1);
    alpha();
    say("3 LAYERS render us by count of full layers");
    layers("3 screen:", NULL, 640u, 480u);
    layers("3 320x240:", rtt_mem, 320u, 240u);
    punch();
    palette();
    volume();

    step("done");
    vid_set_dithering(true);
    pvr_mem_free(ramp_mem); pvr_mem_free(map_mem); pvr_mem_free(rtt_mem);
    pvr_shutdown();
}

int main(void) {
    unsigned page = 0, pages, held = ~0u;

    run();
    vid_set_mode(DM_640x480, PM_RGB565);
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;

        sh4der_fault_beat(&crumbs);
        pages = (nlines + ROWS - 1u) / ROWS;
        if (!pages) pages = 1u;
        if (held == ~0u) page_draw(page, pages);                 /* the first pass: nothing counts as a press */
        else if (hit & CONT_A) { page = (page + 1u) % pages; page_draw(page, pages); }
        else if (hit & CONT_B) { page = (page + pages - 1u) % pages; page_draw(page, pages); }
        held = now;
        thd_sleep(20);
    }
    return 0;
}
