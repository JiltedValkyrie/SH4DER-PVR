/* SH4DER-PVR: the subtract spike, the Dreamcast test program

   demo/psm/compare/sub_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A test program for a real console. Spike c1 showed the PVR can compare a map with a receiver's depth by blending:
   exact at 8 bits, about 11 layers, and 6 bits when the PVR draws the map itself. This one asks whether the compare
   can be one to three layers at 8 bits with a map the PVR draws (sub_model.h has the ideas and their models). It runs
   its steps once and prints numbers on result pages (A next, B back) and on the serial line ("sub:").

   The test picture is c1's: the map's depth M runs down y, the receiver's R across x, a level a pixel, so the shadow
   should begin at x = y + 1 on every row. "err a..b" is where it began against that; "found" rows with an edge (255
   ideal); "rag" rows with more than one edge; "btw" pixels neither lit nor shadow. A cyclic compare is only read
   within 100 columns of the diagonal.

     1 BUMP    the bump unit as a subtract: texel azimuth = M - 64, Q = R, K1 128, K3 127, so the unit's level is
               128 + 127 sin(M - R). Read as alpha (a layer that multiplies a white frame by source alpha) and, to
               see where the level also goes, as colour.
               a  Q given per vertex across one quad: is Q interpolated, or one value a polygon
               b  Q in 32 strips, one value each (right even if Q is per polygon)
               c  the same from a texture that is not twiddled
               d  the same from a render target the PVR drew the codes into
               c and d are NOT in the first run. Sega's own errata list says the CORE hangs on a bump texture that
               is not twiddled. They run only when START is pressed on the result pages, after every other line has
               been shown and sent to the serial line; photograph the pages first.
     2 WRITE   the frame buffer write's own threshold: c1's difference R - M left in destination alpha, the target
               written as ARGB1555 with the alpha threshold at 1, 2 and 128, and once as ARGB4444. The write control
               register is set after the lists are sent and put back after the render.
     5 PUNCH   the punch-through test: a plain 4444 texture with an alpha ramp at references 64, 128, 192; texture
               alpha x vertex alpha (c1's test, which drew everything); the bump unit's level.
     6 FOG     the fog table as a depth code the PVR makes from the pixel's own 1 / w: where a ramp table puts each
               octave, and one octave spread over 256 pixels.

   (3, the two-digit compare, is a host model only: test/sub_model_test.c.)

   Only PVR registers and video memory are written. The fault catcher (include/sh4der_fault.h) is in; every wait is bounded.
   Every number comes from the run that prints it; only a real console's run is evidence.
   Build: python demo/psm/compare/build.py --spike sub.
*/

#include <kos.h>
#include <stdarg.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"
#include "sub_model.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

/* ---- the result pages ----------------------------------------------------------- */

#define COLS 50u
#define ROWS 17u
#define LINES_MAX 120u
static char lines[LINES_MAX][COLS + 1u];
static unsigned nlines;
static sh4der_fault_crumbs_t crumbs;

/* A line for the pages and the serial line. A line over the page's width goes on, indented, on the next. */
static void say(const char *fmt, ...) {
    char big[200];
    const char *s = big;
    size_t left;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(big, sizeof big, fmt, ap);
    va_end(ap);
    printf("sub: %s\n", big);
    for (left = strlen(big); nlines < LINES_MAX; ) {
        const int cont = s != big;
        const size_t room = cont ? COLS - 3u : COLS, n = left < room ? left : room;

        snprintf(lines[nlines++], COLS + 1u, "%s%.*s", cont ? "   " : "", (int)n, s);
        s += n; left -= n;
        if (!left) break;
    }
}

static void page_draw(unsigned page, unsigned pages) {
    char head[96];
    unsigned i;

    vid_clear(0, 0, 0);
    snprintf(head, sizeof head, "PVR SUBTRACT c3b p%u/%u   A next  B back", page + 1u, pages);
    bfont_draw_str(vram_s + 16u * 640u + 20u, 640, true, head);
    for (i = 0; i < ROWS && page * ROWS + i < nlines; i++)
        bfont_draw_str(vram_s + (16u + 24u * (i + 2u)) * 640u + 20u, 640, true, lines[page * ROWS + i]);
}

static void step(const char *name) {
    sh4der_fault_beat(&crumbs);
    sh4der_fault_stage(&crumbs, name);
    crumbs.frame++;
}

/* ---- video memory ------------------------------------------------------------------ */

#define TGT_ROWS 264u                      /* rows allocated for the 256-wide target; 258 are drawn (two spare: route AB, result s3) */
#define BG565 0x001fu                      /* the background: pure blue, a colour no test draws */

enum { BANK_GREY = 0, BANK_ALPHA = 1, BANK_WHITE_ALPHA = 2 };
enum { TEX_NONE = 0, TEX_PAL = 1 };

static pvr_ptr_t ramp_mem, bump_tw_mem, bump_raw_mem, code_mem, a4_mem, tgt_mem;
static uint8_t ramp_px[CMP_N * CMP_N];
static uint16_t tex16[CMP_N * CMP_N] __attribute__((aligned(32)));
static uint16_t px[CMP_N * CMP_N];         /* the square as read back, RGB565 or the target's own format */
static uint8_t img[CMP_N * CMP_N], model[CMP_N * CMP_N];

static int load(void) {
    unsigned i, x, y;

    ramp_mem = pvr_mem_malloc(sizeof ramp_px);
    bump_tw_mem = pvr_mem_malloc(sizeof tex16);
    bump_raw_mem = pvr_mem_malloc(sizeof tex16);
    code_mem = pvr_mem_malloc(sizeof tex16);
    a4_mem = pvr_mem_malloc(sizeof tex16);
    tgt_mem = pvr_mem_malloc(CMP_N * TGT_ROWS * 2u);
    if (!ramp_mem || !bump_tw_mem || !bump_raw_mem || !code_mem || !a4_mem || !tgt_mem) return 0;

    for (y = 0; y < CMP_N; y++) for (x = 0; x < CMP_N; x++) ramp_px[y * CMP_N + x] = (uint8_t)x;
    pvr_txr_load_ex(ramp_px, ramp_mem, CMP_N, CMP_N, PVR_TXRLOAD_8BPP);
    /* The bump texel for map depth y: S (the high byte) 0, R (the low byte) = y less a quarter turn. As RGB565 the
       same 16 bits are red 0, green 0 .. 7, blue 0 .. 31: what the PVR must draw for a target to be this texture. */
    for (y = 0; y < CMP_N; y++) for (x = 0; x < CMP_N; x++) tex16[y * CMP_N + x] = (uint16_t)sub_bump_texel(y);
    pvr_txr_load_ex(tex16, bump_tw_mem, CMP_N, CMP_N, PVR_TXRLOAD_16BPP);     /* twiddled on the way in */
    pvr_txr_load(tex16, bump_raw_mem, sizeof tex16);
    pvr_txr_load(tex16, code_mem, sizeof tex16);
    for (y = 0; y < CMP_N; y++) for (x = 0; x < CMP_N; x++) tex16[y * CMP_N + x] = (uint16_t)(((x >> 4) << 12) | 0x0fffu);   /* alpha x / 16, white */
    pvr_txr_load(tex16, a4_mem, sizeof tex16);

    pvr_set_pal_format(PVR_PAL_ARGB8888);
    for (i = 0; i < 256u; i++) {
        pvr_set_pal_entry(BANK_GREY * 256u + i, 0xff000000u | (i << 16) | (i << 8) | i);
        pvr_set_pal_entry(BANK_ALPHA * 256u + i, i << 24);
        pvr_set_pal_entry(BANK_WHITE_ALPHA * 256u + i, (i << 24) | 0x00ffffffu);
    }
    return 1;
}

/* ---- headers and quads ---------------------------------------------------------------- */

/* A layer's header: the list, untextured or the paletted ramp through a bank, the blend, the texture environment.
   Every layer passes the depth test and writes no depth: order is the order sent. */
static void mk(pvr_poly_hdr_t *h, pvr_list_t list, int tex, unsigned bank, pvr_blend_mode_t src, pvr_blend_mode_t dst, pvr_txr_shading_mode_t env) {
    pvr_poly_cxt_t c;

    if (tex == TEX_PAL) pvr_poly_cxt_txr(&c, list, PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(bank), (int)CMP_N, (int)CMP_N, ramp_mem, PVR_FILTER_NEAREST);
    else pvr_poly_cxt_col(&c, list);
    if (tex != TEX_NONE) { c.txr.env = env; c.txr.alpha = PVR_TXRALPHA_ENABLE; }
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.alpha = list != PVR_LIST_OP_POLY;
    c.blend.src = src; c.blend.dst = dst;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = list == PVR_LIST_OP_POLY ? PVR_DEPTHWRITE_ENABLE : PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(h, &c);
}

/* A header for a 256 x 256 16-bit texture at `mem` in `fmt` (with its twiddle flag), offset colour on. */
static void mk_txr(pvr_poly_hdr_t *h, pvr_list_t list, uint32_t fmt, pvr_ptr_t mem, pvr_blend_mode_t src, pvr_blend_mode_t dst) {
    pvr_poly_cxt_t c;

    pvr_poly_cxt_txr(&c, list, fmt, (int)CMP_N, (int)CMP_N, mem, PVR_FILTER_NEAREST);
    c.txr.env = PVR_TXRENV_REPLACE; c.txr.alpha = PVR_TXRALPHA_ENABLE;
    c.gen.specular = 1;                                          /* the offset colour carries K1, K2, K3 and Q */
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.alpha = list != PVR_LIST_OP_POLY;
    c.blend.src = src; c.blend.dst = dst;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = list == PVR_LIST_OP_POLY ? PVR_DEPTHWRITE_ENABLE : PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(h, &c);
}

enum { UV_NONE = 0, UV_X = 1, UV_X_BACK = 2, UV_Y = 3, UV_XY = 4 };

/* A rectangle. `uv`: the ramp's level runs with x, against x, with y; or u with x and v with y (a picture laid on
   the square). Colours: top, bottom. Offset colours: left, right (the bump unit's K and Q). */
static void quad(const pvr_poly_hdr_t *h, float x0, float y0, float x1, float y1, int uv, uint32_t ct, uint32_t cb, uint32_t ol, uint32_t or_, float zl, float zr) {
    pvr_vertex_t v;
    unsigned i;

    pvr_prim((void *)h, sizeof *h);
    memset(&v, 0, sizeof v);
    for (i = 0; i < 4u; i++) {
        const int right = (i & 2u) != 0, top = (i & 1u) != 0;
        const float fx = (right ? x1 : x0) / (float)CMP_N, fy = (top ? y0 : y1) / (float)CMP_N;

        v.flags = i == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = right ? x1 : x0; v.y = top ? y0 : y1; v.z = right ? zr : zl;
        v.argb = top ? ct : cb;
        v.oargb = right ? or_ : ol;
        v.u = 0.0f; v.v = 0.5f;
        if (uv == UV_X) v.u = fx;
        else if (uv == UV_X_BACK) v.u = 1.0f - fx;
        else if (uv == UV_Y) v.u = fy;
        else if (uv == UV_XY) { v.u = fx; v.v = fy; }
        pvr_prim(&v, sizeof v);
    }
}

#define WHITE 0xffffffffu
#define SQ(h, uv, c) quad(h, 0.0f, 0.0f, 256.0f, 256.0f, uv, c, c, 0u, 0u, 1.0f, 1.0f)
#define KQ(q) ((SUB_K1 << 24) | (SUB_K3 << 8) | ((q) & 255u))    /* K1, K2 = 0, K3, Q */

/* ---- passes ----------------------------------------------------------------------- */

static uint32_t render_addr;

static int scene_open(int target) {
    pvr_wait_ready();
    if (target) return pvr_scene_begin_rtt(tgt_mem, CMP_N, 258u, CMP_N) >= 0;
    pvr_scene_begin();
    return 1;
}

static int scene_close(void) {
    unsigned t;

    pvr_scene_finish();
    for (t = 0; t < 20u && pvr_wait_ready() < 0; t++) sh4der_fault_beat(&crumbs);
    if (t == 20u) return 0;
    for (t = 0; t < 20u && pvr_wait_render_done() < 0; t++) sh4der_fault_beat(&crumbs);
    render_addr = PVR_GET(PVR_RENDER_ADDR);
    return t < 20u;
}

/* The 256 x 256 square of the screen frame just rendered, or of the target, as its 16-bit pixels. */
static void read_px(int target) {
    const volatile uint16_t *fb = target ? (const volatile uint16_t *)tgt_mem : (const volatile uint16_t *)(uintptr_t)(0xa5000000u | (render_addr & 0x00ffffffu));
    const unsigned stride = target ? CMP_N : 640u;
    unsigned x, y;

    for (y = 0; y < CMP_N; y++) for (x = 0; x < CMP_N; x++) px[y * CMP_N + x] = fb[y * stride + x];
}

static unsigned green8(unsigned p) { const unsigned g = (p >> 5) & 63u; return (g << 2) | (g >> 4); }

static void img_green(void) {
    unsigned i;

    for (i = 0; i < CMP_N * CMP_N; i++) img[i] = (uint8_t)green8(px[i]);
}

static void say_diag(const char *tag, unsigned lit, unsigned shadow, const char *note) {
    cmp_stats_t st;

    sub_diag(img, lit, shadow, 8u, &st);
    say("%s err %+d..%+d found %u rag %u btw %u%s", tag, st.min_err, st.max_err, st.found, st.ragged, st.between, note);
}

/* The model's picture for Q in `strips` strips (0: Q = x), lit where the level is 128 or more; its error range as text. */
static const char *model_note(unsigned strips) {
    static char note[40];
    cmp_stats_t st;
    unsigned x, y;

    for (y = 0; y < CMP_N; y++) {
        for (x = 0; x < CMP_N; x++) {
            const unsigned w = strips ? CMP_N / strips : 1u, q = strips ? x / w * w + w / 2u : x;

            model[y * CMP_N + x] = sub_bump_level(sub_bump_texel(y), q) >= 128u ? 0u : 255u;
        }
    }
    sub_diag(model, 0u, 255u, 8u, &st);
    snprintf(note, sizeof note, " (model %+d..%+d)", st.min_err, st.max_err);
    return note;
}

/* ---- 1. the bump unit as a subtract ----------------------------------------------------- */

enum { OUT_RGB = 0, OUT_ALPHA = 1, OUT_BINARY = 2 };

/* The bump quad(s): Q across x, per vertex over one quad (strips 0) or one value a strip. */
static void bump_quads(const pvr_poly_hdr_t *h, unsigned strips) {
    unsigned k;

    if (!strips) { quad(h, 0.5f, 0.0f, 255.5f, 256.0f, UV_XY, WHITE, WHITE, KQ(0u), KQ(255u), 1.0f, 1.0f); return; }   /* Q = x at each pixel's centre */
    for (k = 0; k < strips; k++) {
        const float w = (float)(CMP_N / strips);
        const uint32_t kq = KQ(k * (CMP_N / strips) + CMP_N / strips / 2u);

        quad(h, w * (float)k, 0.0f, w * (float)(k + 1u), 256.0f, UV_XY, WHITE, WHITE, kq, kq, 1.0f, 1.0f);
    }
}

/* One pass of the bump test into the screen, read back into img (green as 8 bits). OUT_RGB: the unit's polygon as it
   is, opaque. OUT_ALPHA: a white frame times the polygon's alpha. OUT_BINARY: that, then + 127, inverted, doubled 8
   times: 0 where the level was 128 or more, 255 below. */
static int bump_pass(uint32_t fmt, pvr_ptr_t mem, unsigned strips, int out) {
    pvr_poly_hdr_t hb, hw, hadd, hinv, hdbl;
    unsigned i;

    sh4der_fault_beat(&crumbs);
    mk(&hw, PVR_LIST_OP_POLY, TEX_NONE, 0u, PVR_BLEND_ONE, PVR_BLEND_ZERO, PVR_TXRENV_REPLACE);
    mk(&hadd, PVR_LIST_TR_POLY, TEX_NONE, 0u, PVR_BLEND_ONE, PVR_BLEND_ONE, PVR_TXRENV_REPLACE);
    mk(&hinv, PVR_LIST_TR_POLY, TEX_NONE, 0u, PVR_BLEND_INVDESTCOLOR, PVR_BLEND_ZERO, PVR_TXRENV_REPLACE);
    mk(&hdbl, PVR_LIST_TR_POLY, TEX_NONE, 0u, PVR_BLEND_DESTCOLOR, PVR_BLEND_ONE, PVR_TXRENV_REPLACE);
    if (out == OUT_RGB) mk_txr(&hb, PVR_LIST_OP_POLY, fmt, mem, PVR_BLEND_ONE, PVR_BLEND_ZERO);
    else mk_txr(&hb, PVR_LIST_TR_POLY, fmt, mem, PVR_BLEND_ZERO, PVR_BLEND_SRCALPHA);       /* frame x source alpha */
    if (!scene_open(0)) return 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    if (out == OUT_RGB) bump_quads(&hb, strips); else SQ(&hw, UV_NONE, WHITE);
    pvr_list_finish();
    if (out != OUT_RGB) {
        pvr_list_begin(PVR_LIST_TR_POLY);
        bump_quads(&hb, strips);
        if (out == OUT_BINARY) {
            SQ(&hadd, UV_NONE, 0xff7f7f7fu);
            SQ(&hinv, UV_NONE, WHITE);
            for (i = 0; i < 8u; i++) SQ(&hdbl, UV_NONE, WHITE);
        }
        pvr_list_finish();
    }
    if (!scene_close()) return 0;
    read_px(0);
    img_green();
    return 1;
}

static void say_row(const char *tag) {
    const uint8_t *r = img + 128u * CMP_N;

    say("%s x32 96 124 128 132 160 224: %u %u %u %u %u %u %u", tag, r[32], r[96], r[124], r[128], r[132], r[160], r[224]);
}

/* Draw the codes into the target with the PVR: the RGB565 picture whose 16 bits are the bump texels. Returns the
   pixels of the 256 rows whose 16 bits are not the texel wanted, -1 when the pass did not come back. */
static int target_codes(void) {
    pvr_poly_hdr_t h;
    const volatile uint16_t *t = (const volatile uint16_t *)tgt_mem;
    pvr_poly_cxt_t c;
    unsigned x, y;
    int wrong = 0;

    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)CMP_N, (int)CMP_N, code_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE; c.txr.env = PVR_TXRENV_REPLACE;
    pvr_poly_compile(&h, &c);
    if (!scene_open(1)) return -1;
    pvr_list_begin(PVR_LIST_OP_POLY);
    quad(&h, 0.0f, 0.0f, 256.0f, 258.0f, UV_XY, WHITE, WHITE, 0u, 0u, 1.0f, 1.0f);
    pvr_list_finish();
    if (!scene_close()) return -1;
    for (y = 0; y < CMP_N; y++) for (x = 0; x < CMP_N; x++) wrong += t[y * CMP_N + x] != sub_bump_texel(y);
    return wrong;
}

static void bump(unsigned from, unsigned to) {
    static const struct { const char *tag; int raster, target; unsigned strips; } v[4] = {
        { "1a Q by vertex:", 0, 0, 0u }, { "1b Q 32 strips:", 0, 0, 32u }, { "1c raster txr:", 1, 0, 32u }, { "1d PVR target:", 1, 1, 32u } };
    unsigned i;

    if (!from) say("1 BUMP level 128+127sin(M-R); A = via alpha");
    for (i = from; i < to; i++) {
        const uint32_t fmt = PVR_TXRFMT_BUMP | (v[i].raster ? PVR_TXRFMT_NONTWIDDLED : 0u);
        const pvr_ptr_t mem = v[i].target ? tgt_mem : v[i].raster ? bump_raw_mem : bump_tw_mem;

        step(v[i].tag);
        if (v[i].target) {
            const int wrong = target_codes();

            if (wrong < 0) { say("1d target pass: NO RENDER END"); continue; }
            say("1d codes the PVR drew: %d of 65536 px wrong", wrong);
        }
        if (i < 2u) {
            if (!bump_pass(fmt, mem, v[i].strips, OUT_RGB)) { say("%s NO RENDER END", v[i].tag); continue; }
            say_row(i ? "1b RGB" : "1a RGB");
            if (!bump_pass(fmt, mem, v[i].strips, OUT_ALPHA)) { say("%s NO RENDER END", v[i].tag); continue; }
            say_row(i ? "1b A  " : "1a A  ");
        }
        if (!bump_pass(fmt, mem, v[i].strips, OUT_BINARY)) { say("%s NO RENDER END", v[i].tag); continue; }
        say_diag(v[i].tag, 0u, 255u, model_note(v[i].strips));
    }
}

/* ---- 2. the threshold at the frame buffer write ------------------------------------------ */

#define CFG2_MASK 0x00ff0007u              /* alpha threshold (bits 23-16) and pack mode (bits 2-0) */
#define PACK_4444 2u
#define PACK_1555 3u

/* c1's difference R - M (saturating) left in destination alpha, into the target written in `pack` with the alpha
   threshold `thr`. 1 when the pass came back; *seen the register as read while it was set. */
static int write_pass(unsigned pack, unsigned thr, uint32_t *seen) {
    pvr_poly_hdr_t h0, hz, hadd, hinv;
    const uint32_t saved = PVR_GET(PVR_FB_CFG_2);
    int ok;

    sh4der_fault_beat(&crumbs);
    mk(&h0, PVR_LIST_OP_POLY, TEX_NONE, 0u, PVR_BLEND_ONE, PVR_BLEND_ZERO, PVR_TXRENV_REPLACE);
    mk(&hz, PVR_LIST_TR_POLY, TEX_NONE, 0u, PVR_BLEND_ZERO, PVR_BLEND_DESTCOLOR, PVR_TXRENV_REPLACE);
    mk(&hadd, PVR_LIST_TR_POLY, TEX_PAL, BANK_ALPHA, PVR_BLEND_ONE, PVR_BLEND_ONE, PVR_TXRENV_REPLACE);
    mk(&hinv, PVR_LIST_TR_POLY, TEX_NONE, 0u, PVR_BLEND_INVDESTALPHA, PVR_BLEND_INVDESTCOLOR, PVR_TXRENV_REPLACE);
    if (!scene_open(1)) return 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    quad(&h0, 0.0f, 0.0f, 256.0f, 258.0f, UV_NONE, WHITE, WHITE, 0u, 0u, 1.0f, 1.0f);
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    SQ(&hz, UV_NONE, 0x00ffffffu);                               /* alpha to 0 */
    SQ(&hadd, UV_X_BACK, WHITE);                                 /* alpha = 255 - R */
    SQ(&hadd, UV_Y, WHITE);                                      /* + M, saturating */
    SQ(&hinv, UV_NONE, 0xff000000u);                             /* alpha = R - M where positive */
    pvr_list_finish();
    PVR_SET(PVR_FB_CFG_2, (saved & ~CFG2_MASK) | (thr << 16) | pack);          /* KallistiOS sets this register only when the video mode is set */
    *seen = PVR_GET(PVR_FB_CFG_2);
    ok = scene_close();
    PVR_SET(PVR_FB_CFG_2, saved);
    if (ok) read_px(1);
    return ok;
}

static void fb_write(void) {
    static const unsigned thr[3] = { 1u, 2u, 128u };
    uint32_t seen = 0;
    cmp_stats_t st;
    unsigned i, k, first = 999u;

    say("2 WRITE alpha >= thr at the 1555 target write");
    for (k = 0; k < 3u; k++) {
        char name[16];

        snprintf(name, sizeof name, "2 thr %u", thr[k]);
        step(name);
        if (!write_pass(PACK_1555, thr[k], &seen)) { say("%s: NO RENDER END", name); continue; }
        for (i = 0; i < CMP_N * CMP_N; i++) img[i] = (px[i] & 0x8000u) ? 255u : 0u;
        cmp_boundary(img, CMP_N, 0u, 255u, 8u, &st);
        say("%s: err %+d..%+d found %u rag %u (model %+d)", name, st.min_err, st.max_err, st.found, st.ragged, (int)thr[k] - 1);
    }
    say("   register while set: %08x", (unsigned)seen);
    step("2 4444");
    if (!write_pass(PACK_4444, 0u, &seen)) { say("2 4444: NO RENDER END"); return; }
    for (i = 0; i < CMP_N; i++) if ((px[i] >> 12) && first == 999u) first = i;
    say("2 4444 row 0: alpha over 0 from x=%u (16); at x=255: %u (15); px %04x", first, (unsigned)(px[255] >> 12), (unsigned)px[255]);
}

/* ---- 5. punch-through ---------------------------------------------------------------------- */

/* The pixels of the square that are not the background, after a pass. img: 0 where drawn, 255 where not. */
static unsigned drawn_px(void) {
    unsigned i, n = 0;

    for (i = 0; i < CMP_N * CMP_N; i++) { const int d = px[i] != BG565; img[i] = d ? 0u : 255u; n += (unsigned)d; }
    return n;
}

static int punch_pass(const pvr_poly_hdr_t *h, unsigned ref, int kind) {
    sh4der_fault_beat(&crumbs);
    PVR_SET(PVR_PT_ALPHA_REF, ref);
    if (!scene_open(0)) return 0;
    pvr_list_begin(PVR_LIST_PT_POLY);
    if (kind == 0) quad(h, 0.0f, 0.0f, 256.0f, 256.0f, UV_XY, WHITE, WHITE, 0u, 0u, 1.0f, 1.0f);
    else if (kind == 1) quad(h, 0.0f, 0.0f, 256.0f, 256.0f, UV_X, 0xffffffffu, 0x00ffffffu, 0u, 0u, 1.0f, 1.0f);   /* texture alpha x, vertex alpha 255 - y */
    else bump_quads(h, 32u);
    pvr_list_finish();
    PVR_SET(PVR_PT_ALPHA_REF, ref);
    if (!scene_close()) return 0;
    read_px(0);
    return 1;
}

static void punch(void) {
    static const unsigned refs[3] = { 64u, 128u, 192u };
    pvr_poly_hdr_t h;
    pvr_poly_cxt_t c;
    unsigned n[3] = { 0, 0, 0 }, diff[CMP_RULES], k, x, y;
    int rule;

    say("5 PUNCH px drawn of 65536");
    step("5a 4444");
    pvr_poly_cxt_txr(&c, PVR_LIST_PT_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, (int)CMP_N, (int)CMP_N, a4_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE;                            /* otherwise KallistiOS's own settings for the list */
    pvr_poly_compile(&h, &c);
    for (k = 0; k < 3u; k++) { if (!punch_pass(&h, refs[k], 0)) { say("5a: NO RENDER END"); return; } n[k] = drawn_px(); }
    say("5a 4444 ref 64 128 192: %u %u %u", n[0], n[1], n[2]);
    say("   (>= ref: 49152 32768 16384; alpha 1 only: 4096 x3)");
    say("   reference register reads %02x", (unsigned)(PVR_GET(PVR_PT_ALPHA_REF) & 255u));

    step("5b product");
    mk(&h, PVR_LIST_PT_POLY, TEX_PAL, BANK_WHITE_ALPHA, PVR_BLEND_ONE, PVR_BLEND_ZERO, PVR_TXRENV_MODULATEALPHA);
    if (!punch_pass(&h, 64u, 1)) { say("5b: NO RENDER END"); return; }
    n[0] = drawn_px();
    for (rule = 0; rule < (int)CMP_RULES; rule++) {
        for (y = 0; y < CMP_N; y++) for (x = 0; x < CMP_N; x++) model[y * CMP_N + x] = (uint8_t)!cmp_punch_pixel(x, y, 64u, rule);
        diff[rule] = cmp_count_diff(img, model);
    }
    say("5b product ref 64: %u drawn; differ from the 4 model rules: %u %u %u %u", n[0], diff[0], diff[1], diff[2], diff[3]);

    step("5c bump");
    mk_txr(&h, PVR_LIST_PT_POLY, PVR_TXRFMT_BUMP, bump_tw_mem, PVR_BLEND_ONE, PVR_BLEND_ZERO);
    if (!punch_pass(&h, 128u, 2)) { say("5c: NO RENDER END"); return; }
    n[0] = drawn_px();
    say("5c bump ref 128, 32 strips: %u drawn", n[0]);
    say_diag("  ", 0u, 255u, model_note(32u));
}

/* ---- 6. the fog table as a depth code -------------------------------------------------------- */

/* Load the fog table from 129 levels, t[0] for the smallest scaled 1 / w: each register holds its own level and the
   next one's (the layout KallistiOS's own loaders write). Only between renders. */
static void fog_load(const uint8_t *t) {
    unsigned i;

    for (i = 0; i < 128u; i++) PVR_SET(PVR_FOG_TABLE_BASE + 4u * i, ((uint32_t)t[i] << 8) | t[i + 1u]);
}

static int fog_pass(float zl, float zr) {
    pvr_poly_hdr_t h;
    pvr_poly_cxt_t c;

    sh4der_fault_beat(&crumbs);
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_TABLE;
    pvr_poly_compile(&h, &c);
    if (!scene_open(0)) return 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    quad(&h, 0.0f, 0.0f, 256.0f, 256.0f, UV_NONE, 0xff000000u, 0xff000000u, 0u, 0u, zl, zr);      /* black: the pixel is the fog level */
    pvr_list_finish();
    if (!scene_close()) return 0;
    read_px(0);
    img_green();
    return 1;
}

static void fog(void) {
    static const unsigned xs[8] = { 1u, 3u, 7u, 15u, 31u, 63u, 127u, 255u };
    const uint8_t *r = img + 128u * CMP_N;
    uint8_t t[129];
    unsigned m[8], i, back = 0, dev = 0;

    say("6 FOG table level from the pixel's own 1/w");
    step("6a octaves");
    pvr_fog_table_color(1.0f, 1.0f, 1.0f, 1.0f);
    pvr_fog_far_depth(255.99f);                                  /* the scale: 1 / w of 1 is the table's top */
    for (i = 0; i < 129u; i++) t[i] = (uint8_t)(i < 128u ? 2u * i : 255u);
    fog_load(t);
    if (!fog_pass(0.0f, 1.0f)) { say("6a: NO RENDER END"); return; }
    for (i = 0; i < 8u; i++) {                                   /* the model: scaled 1 / w = x + 0.5, entry = 16 log2 */
        const float s = (float)xs[i] + 0.5f;
        unsigned oct = 0;

        while ((float)(2u << oct) <= s) oct++;
        m[i] = (unsigned)(2.0f * (16.0f * (float)oct + 16.0f * (s / (float)(1u << oct) - 1.0f)));
    }
    say("6a ramp table, x 1 3 7 15 31 63 127 255: %u %u %u %u %u %u %u %u", r[1], r[3], r[7], r[15], r[31], r[63], r[127], r[255]);
    say("   model: %u %u %u %u %u %u %u %u", m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7]);

    step("6b one octave");
    for (i = 0; i < 129u; i++) t[i] = (uint8_t)(i < 112u ? 0u : (i - 112u) * 255u / 16u);
    fog_load(t);
    if (!fog_pass(0.5f, 1.0f)) { say("6b: NO RENDER END"); return; }
    for (i = 1; i < CMP_N; i++) {
        const unsigned want = green8((i >> 2) << 5), d = r[i] > want ? r[i] - want : want - r[i];

        back += r[i] < r[i - 1u];
        if (d > dev) dev = d;
    }
    say("6b top octave over 256 px, x 0 64 128 192 255: %u %u %u %u %u", r[0], r[64], r[128], r[192], r[255]);
    say("   (model 0 65 130 195 255) steps back %u, most off the line %u levels", back, dev);
    for (i = 0; i < 129u; i++) t[i] = 0;
    fog_load(t);
}

/* ---- the run -------------------------------------------------------------------------- */

/* Start the PVR and load the textures. 0 when it could not. */
static int start(void) {
    pvr_init_params_t init = pvr_default_params;

    init.opb_sizes[PVR_LIST_OP_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_OP_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_TR_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_PT_POLY] = PVR_BINSIZE_16;
    init.vertex_buf_size = 256 * 1024;
    init.autosort_disabled = 1;                                  /* translucent layers in the order they are sent */
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return 0; }
    pvr_set_bg_color(0.0f, 0.0f, 1.0f);
    if (!load()) { say("video memory allocation FAIL"); pvr_shutdown(); return 0; }
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
    if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused");
    bump(0u, 2u);
    fb_write();
    punch();
    fog();
    stop("done");
    say("START: bump textures not twiddled (1c 1d). Sega's errata: may hang. Photograph first.");
}

/* The two tests the errata warn about, on START. */
static void run_risky(void) {
    say("1c 1d: started");
    if (!start()) return;
    bump(2u, 4u);
    stop("done 2");
    say("1c 1d: ended");
}

static void cont_reset_btn_callback_(uint8_t addr, uint32_t btns) {
    (void)addr;
    (void)btns;
    arch_exit();
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    cont_btn_callback(0, CONT_RESET_BUTTONS, cont_reset_btn_callback_);
    unsigned page = 0, pages, held = ~0u;
    int risky = 0;

    run();
    vid_set_mode(DM_640x480, PM_RGB565);
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;

        sh4der_fault_beat(&crumbs);
        pages = (nlines + ROWS - 1u) / ROWS;
        if (!pages) pages = 1u;
        if (held == ~0u) page_draw(page, pages);
        else if (hit & CONT_A) { page = (page + 1u) % pages; page_draw(page, pages); }
        else if (hit & CONT_B) { page = (page + pages - 1u) % pages; page_draw(page, pages); }
        else if ((hit & CONT_START) && !risky) {
            risky = 1;
            run_risky();
            vid_set_mode(DM_640x480, PM_RGB565);
            pages = (nlines + ROWS - 1u) / ROWS;
            page = pages - 1u;
            page_draw(page, pages);
        }
        held = now;
        thd_sleep(20);
    }
    return 0;
}
