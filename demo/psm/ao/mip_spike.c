/* SH4DER-PVR: mip, can the PVR's choice of mip level be read as a depth a pixel? Dreamcast test

   demo/psm/ao/mip_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The idea (the owner's): the PVR picks a texture's mip level from how small its texels are on the screen, and that
   grows with depth. A texture whose levels are flat, different greys (mip_model.h) then writes a number that depends
   on depth into the frame, and trilinear filtering should blend neighbouring levels into a ramp. If the choice is made
   a pixel, that is a camera depth reading with no second transform; if it is made a triangle, it is not.
   ALL OF THAT IS AN ESTIMATE: this program finds out. Nothing here has run on a console.

   A floor from 1 m to 60 m, the eye 0.75 m above it looking level, drawn into a 320 x 240 render target, read back
   by PVR-DMA (as d1 does), one column of it (x = COL) compared with the depth the SH-4 knows for each row. Ten
   passes, a picture of each held on the screen, then the pages:
     T1  is the level chosen a pixel or a triangle?   0 point sampled, one quad      1 bilinear, one quad
                                                      2 bilinear, 8 strips           3 bilinear, 16 strips
     T2  trilinear                                    4 pass A alone   5 pass B alone   6 A (opaque) + B (translucent, added)
     T3  depth or slope?                              7 point sampled, the floor tilted 0.15
                                                      8 point sampled, the floor's left half + four walls facing the
                                                        eye at 3, 6, 12, 24 m (the same texels a metre)
     T4  how the level falls with depth               (from 0 and 6, against the three guesses of mip_model.h)
     T5  what a pass costs                            9 bilinear with mip mapping off: the base; each pass's PVR time

   HOW TRILINEAR IS SENT is itself a question: the two filter modes "pass A" and "pass B" are drawn alone (4, 5) and
   as the pair the documents describe (6: A in the opaque list, the same quad again in the translucent list with B,
   ONE x ONE). The page prints all three; none is assumed right.

   THE BILINEAR HANG (wiki PVR-071, REPORTED: counted a tile, the 8th opaque quad) is kept away from: at most
   QUAD_CAP filtered opaque quads in a row of tiles; a strip past it is not sent and is counted on the page. One
   translucent quad a pass at most. Every wait is polled and none is longer than LIMIT_NS: a hang would show as a
   timed out wait on the page, not as a stuck console. The fault catcher is in.

   A button or a trigger skips a held picture. Then the pages (A / B). It writes PVR registers and video memory only,
   and the PVR-DMA registers c4 and d1 write; the mip bias is 1.00 (the PVR's "no bias").
   Build: python demo/psm/ao/build.py --spike mip --tag <new tag>. A plain ELF: it also runs from the suite's menu. */

#include <kos.h>
#include <stdarg.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"
#include "../psm_suite.h"
#include "mip_model.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#ifndef MIP_TAG
#define MIP_TAG untagged
#endif
#define MIP_STR_(x) #x
#define MIP_STR(x) MIP_STR_(x)
#ifndef MIP_HOLD_S
#define MIP_HOLD_S 4u                       /* seconds a pass's picture is held on the screen */
#endif
#define CYC_PER_MS 200006u                  /* 200.006 MHz, MEASURED (bench s2) */

#define PG_PREFIX "mip"
#define PG_LINES_MAX 160u
#include "../../../include/sh4der_page.h"

#define say pg_say
static sh4der_fault_crumbs_t crumbs;

static void step(const char *name) {
    sh4der_fault_beat(&crumbs);
    sh4der_fault_stage(&crumbs, name);
    crumbs.frame++;
}

#define LIMIT_NS 200000000ull
static inline uint64_t ns(void) { const uint64_t n = timer_ns_gettime64(); return n - n / 1000000000ull * 2502400ull; }   /* KallistiOS's clock, its reload step taken out */

static void wait_ns(uint64_t t) {
    const uint64_t t0 = ns();

    while (ns() - t0 < t) { }
}

/* ---- memory, headers ----------------------------------------------------------------- */

#define TW 512u                             /* the target as a texture */
#define TH 256u
#define MW 320u                             /* what is drawn of it */
#define MH 240u
#define RUN 64u
#define TEXT_W 512u
#define TEXT_H 32u
#define COL 152u                            /* the column read: on the floor at every depth, in its left half */
#define QUAD_CAP 7u                         /* filtered opaque quads in a row of tiles, at most */

enum { F_POINT = 0, F_BILIN, F_TRIA, F_TRIB, F_NOMIP, F_N };

static pvr_ptr_t tgt_mem, mip_mem, text_mem;
static pvr_poly_hdr_t h_op[F_N], h_tr_b, h_dummy, h_tshow, h_text, h_dim;
static pvr_vertex_t vrun[RUN] __attribute__((aligned(32)));
static unsigned run_n, sent;
static uint16_t tgt_ram[TW * TH] __attribute__((aligned(32)));
static uint16_t chain[((MIP_BYTES + 31u) & ~31u) / 2u] __attribute__((aligned(32)));
static uint16_t text_px[TEXT_W * TEXT_H] __attribute__((aligned(32)));
static char text_ln[2][65];

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
    o->x = x; o->y = y; o->z = z; o->u = u; o->v = v; o->argb = argb; o->oargb = 0u;
    sent++;
}

static void rect(float x0, float y0, float x1, float y1, float z, float u0, float v0, float u1, float v1, uint32_t argb) {
    put(0, x0, y1, z, u0, v1, argb);
    put(0, x0, y0, z, u0, v0, argb);
    put(0, x1, y1, z, u1, v1, argb);
    put(1, x1, y0, z, u1, v0, argb);
}

static void headers(void) {
    static const int filter[F_N] = { PVR_FILTER_NEAREST, PVR_FILTER_BILINEAR, PVR_FILTER_TRILINEAR1, PVR_FILTER_TRILINEAR2, PVR_FILTER_BILINEAR };
    pvr_poly_cxt_t c;
    unsigned k;

    for (k = 0; k < F_N; k++) {             /* the floor: the texel as it is, the level the PVR picks */
        pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_TWIDDLED, (int)MIP_SIZE, (int)MIP_SIZE, mip_mem, filter[k]);
        c.gen.culling = PVR_CULLING_NONE; c.txr.env = PVR_TXRENV_REPLACE;
        c.txr.mipmap = k == F_NOMIP ? PVR_MIPMAP_DISABLE : PVR_MIPMAP_ENABLE; c.txr.mipmap_bias = PVR_MIPBIAS_NORMAL;
        pvr_poly_compile(&h_op[k], &c);
    }
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_TWIDDLED, (int)MIP_SIZE, (int)MIP_SIZE, mip_mem, PVR_FILTER_TRILINEAR2);
    c.gen.culling = PVR_CULLING_NONE; c.txr.env = PVR_TXRENV_REPLACE;      /* trilinear's second pass: the same quad, added */
    c.txr.mipmap = PVR_MIPMAP_ENABLE; c.txr.mipmap_bias = PVR_MIPBIAS_NORMAL;
    c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ONE;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_tr_b, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                      /* the translucent list is never sent empty */
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

/* ---- the caption ---------------------------------------------------------------------------- */

static void text_upload(void) {             /* call with no render running */
    unsigned l, i, x, y;

    memset(text_px, 0, sizeof text_px);
    for (l = 0; l < 2u; l++)
        for (i = 0; i < 64u && text_ln[l][i]; i++) {
            const unsigned c = (unsigned char)text_ln[l][i];
            const uint8_t (*gl)[PG_CW] = pg_glyph[c >= 32u && c < 127u ? c - 32u : 0u];

            for (y = 0; y < PG_CH; y++)
                for (x = 0; x < PG_CW; x++) text_px[(l * 16u + y) * TEXT_W + i * 8u + x] = (uint16_t)(((gl[y][x] >> 4) << 12) | 0x0fffu);
        }
    pvr_txr_load(text_px, text_mem, sizeof text_px);
}

static void text_put(unsigned line, float x, float y) {
    const unsigned n = (unsigned)strlen(text_ln[line]);
    const float v0 = (float)(line * 16u) * (1.0f / (float)TEXT_H), v1 = (float)(line * 16u + 16u) * (1.0f / (float)TEXT_H);

    if (!n) return;
    flush(); pvr_prim(&h_dim, sizeof h_dim);
    rect(x - 2.0f, y - 1.0f, x + (float)(n * 8u) + 2.0f, y + 17.0f, 10.0f, 0, 0, 0, 0, 0xa0000000u);
    flush(); pvr_prim(&h_text, sizeof h_text);
    rect(x, y, x + (float)(n * 8u), y + 16.0f, 11.0f, 0.0f, v0, (float)(n * 8u) * (1.0f / (float)TEXT_W), v1, 0xffffffffu);
    flush();
}

/* ---- scenes on the PVR --------------------------------------------------------------- */

static uint64_t last_render_ns;
static asic_evt_handler_entry_t rd_prev;
static volatile uint32_t rd_seen;
static int rd_hooked;
static unsigned timeouts;                   /* waits that ran out: a hang would be counted here */

static void rd_evt(uint32_t code, void *data) {
    (void)data;
    if (rd_prev.hdl) rd_prev.hdl(code, rd_prev.data);
    rd_seen = 1u;
}

static int wait_ready(void) {
    const uint64_t t0 = ns();

    while (pvr_check_ready() < 0) if (ns() - t0 > LIMIT_NS) { timeouts++; return 0; }
    return 1;
}

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
        while (!rd_seen) if (ns() - t0 > LIMIT_NS) { timeouts++; return 0; }
    } else {
        for (t = 0; t < 20u && pvr_wait_render_done() < 0; t++) sh4der_fault_beat(&crumbs);
        if (t == 20u) { timeouts++; return 0; }
    }
    if (pvr_get_stats(&st) < 0) return 0;
    last_render_ns = st.rnd_last_time;
    return 1;
}

/* ---- the target to RAM: PVR-DMA, as d1's and c4's fetch ------------------------------------------- */

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

/* ---- the passes ------------------------------------------------------------------------- */

typedef struct stage {
    uint8_t filter, strips, pass_b, walls;  /* the opaque header; quads along the floor; trilinear's B added; the walls */
    float tilt;
    const char *name;
} stage_t;

#define STAGES 10u
#define S_POINT 0u
#define S_BILIN 1u
#define S_TRI 6u
#define S_TILT 7u
#define S_WALLS 8u
static const stage_t stage[STAGES] = {
    { F_POINT, 1, 0, 0, 0.0f, "point" }, { F_BILIN, 1, 0, 0, 0.0f, "bilinear" }, { F_BILIN, 8, 0, 0, 0.0f, "bil 8 str" }, { F_BILIN, 16, 0, 0, 0.0f, "bil 16 st" },
    { F_TRIA, 1, 0, 0, 0.0f, "tri A" }, { F_TRIB, 1, 0, 0, 0.0f, "tri B" }, { F_TRIA, 1, 1, 0, 0.0f, "tri A+B" },
    { F_POINT, 1, 0, 0, 0.15f, "tilted" }, { F_POINT, 1, 0, 1, 0.0f, "walls" }, { F_NOMIP, 1, 0, 0, 0.0f, "mip off" },
};

#define DEPTHS 8u
static const float depth_at[DEPTHS] = { 1.1f, 1.5f, 2.0f, 3.0f, 4.5f, 8.0f, 16.0f, 32.0f };
#define WALLS 4u
static const float wall_d[WALLS] = { 3.0f, 6.0f, 12.0f, 24.0f };      /* between two levels by the MIN guess: 1.6, 2.6, 3.6, 4.6 */

typedef struct result {
    int ran;
    unsigned rows, lo, hi, distinct, jump;  /* the column: rows on the floor; the codes' range; how many; the largest step a row */
    unsigned fit[MIP_RULES];                /* rows whose code is the whole level a guess gives */
    unsigned at[DEPTHS], wall[WALLS];       /* the code at the depths; at each wall's middle */
    unsigned fwall[WALLS];                  /* the floor's code at the walls' depths */
    unsigned pvr_us, verts, dropped, tile_max;
} result_t;
static result_t res[STAGES];
static unsigned tile_max_all, dropped_all;

/* The floor between depths za (near) and zb, x from -MIP_W to xr metres. */
static void floor_quad(float za, float zb, float xr, float tilt, float sl) {
    const float ia = sh4der_recip_pos_(za), ib = sh4der_recip_pos_(zb);
    const float ya = mip_floor_y(za, tilt), yb = mip_floor_y(zb, tilt);
    const float ul = -MIP_W * (1.0f / MIP_S), ur = xr * (1.0f / MIP_S), va = za * sl * (1.0f / MIP_S), vb = zb * sl * (1.0f / MIP_S);

    put(0, MIP_CX - MIP_F * MIP_W * ia, ya, ia, ul, va, 0xffffffffu);
    put(0, MIP_CX - MIP_F * MIP_W * ib, yb, ib, ul, vb, 0xffffffffu);
    put(0, MIP_CX + MIP_F * xr * ia, ya, ia, ur, va, 0xffffffffu);
    put(1, MIP_CX + MIP_F * xr * ib, yb, ib, ur, vb, 0xffffffffu);
}

/* The floor as `strips` quads of equal height on the screen, nearest first. A quad that would be the (QUAD_CAP + 1)th
   in a row of tiles is not sent. */
static void floor_send(const stage_t *s, result_t *r, unsigned tiles[8]) {
    const float sl = sh4der_sqrt_(1.0f + s->tilt * s->tilt), y1 = mip_floor_y(MIP_Z1, s->tilt), dy = (240.0f - y1) * sh4der_recip_pos_((float)s->strips);
    const float xr = s->walls ? 0.0f : MIP_W;
    unsigned i, t;

    for (i = 0; i < s->strips; i++) {
        const float ya = 240.0f - dy * (float)i, yb = i + 1u == s->strips ? y1 : ya - dy;
        const unsigned t0 = (unsigned)yb / 32u, t1 = ((unsigned)ya - (ya > (float)(unsigned)ya ? 0u : 1u)) / 32u;
        int full = 0;

        for (t = t0; t <= t1 && t < 8u; t++) if (tiles[t] >= QUAD_CAP) full = 1;
        if (full) { r->dropped++; continue; }
        for (t = t0; t <= t1 && t < 8u; t++) tiles[t]++;
        floor_quad(i ? mip_floor_z(ya, s->tilt) : MIP_Z0, i + 1u == s->strips ? MIP_Z1 : mip_floor_z(yb, s->tilt), xr, s->tilt, sl);
    }
}

#define WALL_X0 200.0f
#define WALL_X1 296.0f
#define WALL_Y(j) (126.0f + 28.0f * (float)(j))
#define WALL_H 20.0f

/* One pass into the target. 0 when a wait ran out. */
static int pass(unsigned k, result_t *r) {
    const stage_t *s = &stage[k];
    unsigned tiles[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }, j, t;

    r->dropped = 0;
    if (!scene_open(1)) return 0;
    sent = 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(&h_op[s->filter], sizeof h_op[0]);
    floor_send(s, r, tiles);
    if (s->walls)
        for (j = 0; j < WALLS; j++) {       /* a wall facing the eye at depth d: the same texels a metre as the floor */
            const float d = wall_d[j], k_uv = d * (1.0f / (MIP_F * MIP_S)), y0 = WALL_Y(j), y1 = y0 + WALL_H;

            for (t = (unsigned)y0 / 32u; t <= (unsigned)(y1 - 1.0f) / 32u && t < 8u; t++) tiles[t]++;
            rect(WALL_X0, y0, WALL_X1, y1, sh4der_recip_pos_(d), (WALL_X0 - MIP_CX) * k_uv, (y0 - MIP_CY) * k_uv, (WALL_X1 - MIP_CX) * k_uv, (y1 - MIP_CY) * k_uv, 0xffffffffu);
        }
    flush();
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    pvr_prim(&h_dummy, sizeof h_dummy);     /* a triangle off the target's left edge: the list is never empty */
    put(0, -64.0f, 8.0f, 0.001f, 0.0f, 0.0f, 0xff000000u);
    put(0, -64.0f, 40.0f, 0.001f, 0.0f, 0.0f, 0xff000000u);
    put(1, -32.0f, 8.0f, 0.001f, 0.0f, 0.0f, 0xff000000u);
    flush();
    if (s->pass_b) {
        pvr_prim(&h_tr_b, sizeof h_tr_b);
        floor_quad(MIP_Z0, MIP_Z1, MIP_W, s->tilt, sh4der_sqrt_(1.0f + s->tilt * s->tilt));
        flush();
    }
    pvr_list_finish();
    r->verts = sent;
    for (t = 0, r->tile_max = 0; t < 8u; t++) if (tiles[t] > r->tile_max) r->tile_max = tiles[t];
    return scene_close();
}

/* The column of the pass just fetched. */
static void read_column(unsigned k, result_t *r) {
    const stage_t *s = &stage[k];
    const unsigned y_first = (unsigned)mip_floor_y(MIP_Z1, s->tilt) + 1u;
    uint8_t seen[64];
    unsigned y, i, prev = 0;

    memset(seen, 0, sizeof seen);
    r->lo = 63u; r->hi = 0u; r->rows = 0; r->distinct = 0; r->jump = 0;
    memset(r->fit, 0, sizeof r->fit);
    for (y = y_first; y < MH; y++) {
        const unsigned c = mip_code(AT(COL, y));
        const float z = mip_floor_z((float)y + 0.5f, s->tilt);

        if (c < r->lo) r->lo = c;
        if (c > r->hi) r->hi = c;
        if (!seen[c]) { seen[c] = 1; r->distinct++; }
        if (r->rows && (c > prev ? c - prev : prev - c) > r->jump) r->jump = c > prev ? c - prev : prev - c;
        for (i = 0; i < MIP_RULES; i++) r->fit[i] += c == MIP_STEP * (mip_guess(i, z, s->tilt) + 1u);
        prev = c;
        r->rows++;
    }
    for (i = 0; i < DEPTHS; i++) {
        const unsigned row = (unsigned)mip_floor_y(depth_at[i], s->tilt);

        r->at[i] = row < MH ? mip_code(AT(COL, row)) : 0u;
    }
    for (i = 0; i < WALLS; i++) {
        const unsigned row = (unsigned)mip_floor_y(wall_d[i], s->tilt);

        r->fwall[i] = row < MH ? mip_code(AT(COL, row)) : 0u;
    }
    for (i = 0; i < WALLS; i++) r->wall[i] = s->walls ? mip_code(AT((unsigned)((WALL_X0 + WALL_X1) * 0.5f), (unsigned)(WALL_Y(i) + WALL_H * 0.5f))) : 0u;
}

/* 1 at a button or a trigger newly pressed. The first call takes what is held as old. */
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

/* The target held on the screen with its caption: the picture to photograph. */
static void show_target(unsigned k) {
    const uint64_t t0 = ns();

    snprintf(text_ln[0], sizeof text_ln[0], "mip %u of %u: %s. Brighter = a smaller mip level", k + 1u, (unsigned)STAGES, stage[k].name);
    snprintf(text_ln[1], sizeof text_ln[1], "codes %u..%u, %u distinct. A button skips", res[k].lo, res[k].hi, res[k].distinct);
    text_upload();
    while (ns() - t0 < (uint64_t)MIP_HOLD_S * 1000000000ull) {
        step("mip show   ");
        if (pad_hit()) return;
        if (!scene_open(0)) return;
        sent = 0;
        pvr_list_begin(PVR_LIST_OP_POLY);
        pvr_prim(&h_tshow, sizeof h_tshow);
        rect(0.0f, 0.0f, 640.0f, 480.0f, 1.0f, 0.0f, 0.0f, (float)MW / (float)TW, (float)MH / (float)TH, 0xffffffffu);
        flush();
        pvr_list_finish();
        pvr_list_begin(PVR_LIST_TR_POLY);
        text_put(0u, 64.0f, 52.0f);
        text_put(1u, 64.0f, 412.0f);
        pvr_list_finish();
        if (!scene_close()) return;
    }
}

#define REPEATS 4u                          /* a pass is drawn this often; the PVR's time is the mean of all but the first */

static void run(void) {
    unsigned k, n;

    for (k = 0; k < STAGES; k++) {
        result_t *r = &res[k];
        uint64_t sum = 0;

        step("mip pass   ");
        for (n = 0; n < REPEATS; n++) {
            if (!pass(k, r)) break;
            if (n) sum += last_render_ns;
        }
        if (n < REPEATS) { say("pass %u (%s): A WAIT RAN OUT at draw %u of %u", k, stage[k].name, n + 1u, REPEATS); continue; }
        r->pvr_us = (unsigned)(sum / (REPEATS - 1u) / 1000u);
        fetch(FETCH_BYTES);
        read_column(k, r);
        r->ran = 1;
        if (r->tile_max > tile_max_all) tile_max_all = r->tile_max;
        dropped_all += r->dropped;
        show_target(k);
    }
}

/* ---- the pages ----------------------------------------------------------------------------- */

/* A code as a level x 10: 7 a level, level 0 at 7. 0 (nothing drawn) prints as "--". */
static void lv(char *out, size_t size, unsigned code) {
    if (code < MIP_STEP) snprintf(out, size, "--");
    else snprintf(out, size, "%u", (code - MIP_STEP) * 10u / MIP_STEP);
}

static void stage_lines(unsigned k) {
    const result_t *r = &res[k];
    char line[80], v[12];
    unsigned i, n;

    if (!r->ran) { say("%u %-9s NOT READ", k, stage[k].name); return; }
    say("%u %-9s codes %u..%u, %u distinct, biggest step %u", k, stage[k].name, r->lo, r->hi, r->distinct, r->jump);
    for (i = 0, n = (unsigned)snprintf(line, sizeof line, "  "); i < DEPTHS; i++) { lv(v, sizeof v, r->at[i]); n += (unsigned)snprintf(line + n, sizeof line - n, " %4s", v); }
    say("%s", line);
}

static void report(void) {
    const result_t *p = &res[S_POINT], *b = &res[S_BILIN], *t = &res[S_TRI], *tl = &res[S_TILT], *w = &res[S_WALLS];
    pg_plot_t *g;
    char rev[32], line[80], v[12];
    unsigned i, k, n;

    suite_rev_text(rev, sizeof rev);
    say("mip %s: is the PVR's mip level a depth reading a pixel?", MIP_STR(MIP_TAG));
    say("%s. A floor 1..60 m, eye 0.75 m up, 320x240 target.", rev);
    say("Texture: level k is flat green 7(k+1) of 63; read back, col %u.", COL);
    say("ESTIMATE until read here: that the level follows depth at all.");
    say("T1 A PIXEL OR A TRIANGLE? one quad, then strips. Per pass:");
    say(" codes; levels x10 at 1.1 1.5 2 3 4.5 8 16 32 m (-- none)");
    for (k = 0; k < 4u; k++) stage_lines(k);
    if (p->ran && b->ran) {
        if (p->distinct > 1u || b->distinct > 1u) say("T1 RESULT: THE LEVEL CHANGES ALONG ONE QUAD: finer than a triangle");
        else say("T1 RESULT: ONE LEVEL FOR THE WHOLE QUAD: chosen a triangle (or pair)");
        say(" strips: 8 give %u distinct codes, 16 give %u (one quad: %u)", res[2].distinct, res[3].distinct, b->distinct);
    }

    pg_break();
    say("T2 TRILINEAR: pass A alone, pass B alone, A opaque + B added");
    say(" codes; levels x10 at 1.1 1.5 2 3 4.5 8 16 32 m (-- none)");
    for (k = 4u; k < 7u; k++) stage_lines(k);
    if (t->ran && b->ran) {
        say("T2 RESULT: A+B %u distinct codes, biggest step %u a row;", t->distinct, t->jump);
        say(" bilinear %u distinct, biggest step %u. 7 codes = one level:", b->distinct, b->jump);
        say(" a smooth ramp steps by 1 or 2; a level's edge steps by 7.");
        say(" (the 565 target holds 7 steps a level: no finer ramp shows)");
    }

    pg_break();
    say("T3 DEPTH OR SLOPE? point sampled. Levels x10 at the 8 depths:");
    stage_lines(S_POINT);
    stage_lines(S_TILT);
    if (w->ran && p->ran) {
        n = (unsigned)snprintf(line, sizeof line, "walls facing the eye at 3 6 12 24 m:");
        for (i = 0; i < WALLS; i++) { lv(v, sizeof v, w->wall[i]); n += (unsigned)snprintf(line + n, sizeof line - n, " %s", v); }
        say("%s", line);
        n = (unsigned)snprintf(line, sizeof line, "the floor at those depths:          ");
        for (i = 0; i < WALLS; i++) { lv(v, sizeof v, p->fwall[i]); n += (unsigned)snprintf(line + n, sizeof line - n, " %s", v); }
        say("%s", line);
        say(" (a wall has the same texels a metre as the floor; if depth");
        say("  alone set the level, a wall would read as the floor there)");
    }
    say("WHICH RULE? rows of the column whose code is the whole level a");
    say(" guess gives (MAX: the larger of texels a pixel across / down;");
    say(" MIN: the smaller; AREA: their mean). ESTIMATES, to be chosen:");
    if (p->ran) say(" flat floor, %u rows: MAX %u  MIN %u  AREA %u", p->rows, p->fit[MIP_MAX], p->fit[MIP_MIN], p->fit[MIP_AREA]);
    if (tl->ran) say(" tilted 0.15, %u rows: MAX %u  MIN %u  AREA %u", tl->rows, tl->fit[MIP_MAX], tl->fit[MIP_MIN], tl->fit[MIP_AREA]);
    if (w->ran) say(" walls: the MIN guess says levels x10 of 10 20 30 40");

    pg_break();
    say("T4 THE LEVEL AGAINST DEPTH (levels x10; guesses whole levels)");
    say("depth m  point  bilin  tri A+B   MAX MIN AREA");
    for (i = 0; i < DEPTHS; i++) {
        char a[12], c[12], d[12];

        lv(a, sizeof a, p->at[i]); lv(c, sizeof c, b->at[i]); lv(d, sizeof d, t->at[i]);
        say("%4u.%u   %5s  %5s  %7s   %3u %3u %4u", (unsigned)depth_at[i], (unsigned)(depth_at[i] * 10.0f + 0.5f) % 10u, a, c, d,
            mip_guess(MIP_MAX, depth_at[i], 0.0f), mip_guess(MIP_MIN, depth_at[i], 0.0f), mip_guess(MIP_AREA, depth_at[i], 0.0f));
    }
    say("A level a doubling of depth is the MIN guess (log2 depth);");
    say("two levels a doubling is MAX (depth squared: down the floor).");

    pg_break();
    say("T5 COST: the PVR's render of one pass, 320x240 target, us");
    say(" (mean of 3 draws; the whole target, no tile cut tried)");
    say("pass         PVR us  verts  quads/tile row  dropped");
    for (k = 0; k < STAGES; k++)
        if (res[k].ran) say("%u %-9s %7u %6u %10u %10u", k, stage[k].name, res[k].pvr_us, res[k].verts, res[k].tile_max, res[k].dropped);
    if (res[9].ran && b->ran) say("DERIVED mip mapping adds to a bilinear pass: %+d us (1 - 9)", (int)b->pvr_us - (int)res[9].pvr_us);
    if (res[9].ran && t->ran) say("DERIVED trilinear A+B against bilinear: %+d us (6 - 1)", (int)t->pvr_us - (int)b->pvr_us);
    say("THE BILINEAR HANG (PVR-071): waits that ran out: %u.", timeouts);
    say(" At most %u filtered opaque quads in a row of tiles (cap %u),", tile_max_all, QUAD_CAP);
    say(" %u strips not sent for it. No hang seen = none at these counts", dropped_all);
    say(" only. One run, one console.");

    g = pg_plot("mip: level x10 against depth, m", "depth m", "level x10", 0);
    pg_series(g, "point"); for (i = 0; i < DEPTHS; i++) pg_point(g, depth_at[i], p->at[i] >= MIP_STEP ? (float)((p->at[i] - MIP_STEP) * 10u / MIP_STEP) : 0.0f);
    pg_series(g, "tri A+B"); for (i = 0; i < DEPTHS; i++) pg_point(g, depth_at[i], t->at[i] >= MIP_STEP ? (float)((t->at[i] - MIP_STEP) * 10u / MIP_STEP) : 0.0f);
}

/* ---- start, stop --------------------------------------------------------------------------- */

static int start(void) {
    pvr_init_params_t init = pvr_default_params;

    init.opb_sizes[PVR_LIST_OP_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_OP_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_TR_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_PT_POLY] = PVR_BINSIZE_0;
    init.vertex_buf_size = 256 * 1024;
    init.autosort_disabled = 1;             /* trilinear's B is added after the dummy, in the order sent */
    init.opb_overflow_count = 4;
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return 0; }
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    tgt_mem = pvr_mem_malloc(TW * TH * 2u);
    mip_mem = pvr_mem_malloc(sizeof chain);
    text_mem = pvr_mem_malloc(sizeof text_px);
    if (!tgt_mem || !mip_mem || !text_mem) { say("video memory allocation FAIL"); pvr_shutdown(); return 0; }
    pvr_sq_set16(tgt_mem, 0u, TW * TH * 2u, PVR_DMA_VRAM64);
    memset(chain, 0, sizeof chain);
    mip_fill(chain);
    pvr_txr_load(chain, mip_mem, sizeof chain);
    headers();
    memset(text_ln, 0, sizeof text_ln);
    text_upload();
    vid_set_dithering(false);               /* a code must reach the target as it is (wiki PVR-023) */
    rd_prev = asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, rd_evt, NULL);
    rd_hooked = rd_prev.hdl != NULL;
    if (!rd_hooked) asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, NULL, NULL);
    return 1;
}

static void stop(const char *name) {
    step(name);
    if (rd_hooked) { asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, rd_prev.hdl, rd_prev.data); rd_hooked = 0; }
    vid_set_dithering(true);
    pvr_shutdown();
}

int main(void) {
    unsigned page = 0, pages, held = ~0u;
    int redraw = 1;

    suite_init();                           /* on the suite's disc: the menu's binary into RAM, before anything else */
    vid_set_mode(DM_640x480, PM_RGB565);
    pg_init();
    if (start()) {
        if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused");
        run();
        stop("mip end");
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
        else if (suite_tick(now, page, pages)) { page++; redraw = 1; }
        if (redraw) { pg_draw(page % pages, "MIP DEPTH", suite_keys_text("A/B pages")); redraw = 0; }
        thd_sleep(20);
    }
    return 0;
}
