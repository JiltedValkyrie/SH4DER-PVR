/* SH4DER-PVR: a few moving shadows, the PVR draws the map (route A), the KallistiOS pass

   src/sh4der_psm_a.c
   Version: v0.2
   The effect: up to four characters cast a soft-edged-by-filtering shadow on a patch of ground, from one light.
   Developed for: a third-person game's characters on a floor (no self-shadow).
   Use it for: people, animals, props that move; not a body shadowing itself, not two lights.

   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/* The maths and the accounting are sh4der_psm_a.h (host tested). This file is the PVR side, Dreamcast only: video memory for
   the target (size x (row x 32 + size) x 2 B: 48 KB for 128 at row 2), three headers (the caster's, the layer's; the white
   ground and the border use the caster's), and the render target pass, a scene of its own, with the tile table cut to its
   tiles. Nothing is allocated per frame. Per frame, TA, ESTIMATE: 3 vertices (96 B) a caster triangle after clipping, at most
   2,200 x 96 B = 211 KB for a full pass, one header, two quads (8 vertices) for the ground and the border strips, 4 more.
   Both guards of sh4der_guard.h: the layer's header is refused if it was never compiled, and a triangle with no area in the
   map is not sent (sh4der_psm_a_tri). Compiles to nothing off the Dreamcast.

   Order inside a frame (README "The psm route A pass"): fit, begin, casters, end, settle; then the screen's scene.
   - SH4DER_PSM_A_CUT_PASS: begin waits for the render in flight before cutting both tables, settle waits for the pass's
     render and restores them. Render-done waits go through sh4der_pvrx_wait_render_done_poll followed unconditionally by
     pvr_wait_render_done (returns at once when idle, DERIVED from reading KallistiOS pvr_scene.c, NOT RUN; sleeps only on a missed sh4der_pvrx_scene_finish or 100 ms timeout)
     and scenes end with sh4der_pvrx_scene_finish; pvr_wait_ready stays sleeping so any stall separates ready from
     render-done.
   - SH4DER_PSM_A_CUT_HOLD: KallistiOS flips tile tables at every render start; after pvr_wait_ready, PVR_ISP_TILEMAT_ADDR
     names the table of the render in flight, so the pass's table is the other one. Only the pass's table is cut and left
     cut across frames (no render-done wait in begin or settle); HOLD requires the pass to be every second render (a third
     render per frame, such as a mirror pass, puts another scene through the held table every other frame). Call
     sh4der_psm_a_release when the order breaks (a frame without a pass, another scene before the screen, exit, or before
     pvr_shutdown). */

#ifdef _arch_dreamcast

#include <kos.h>
#include <string.h>

#include "sh4der_psm_a.h"
#include "sh4der_guard.h"
#include "sh4der_pvrx.h"

#define PSM_A_WHITE 0xffffffffu

static unsigned psm_a_tiles(unsigned size) { return (size + 31u) / 32u; }

static void psm_a_wait_done(void) {
    pvr_wait_ready();                                       /* stays sleeping: a console stall separates ready from render-done */
    (void)sh4der_pvrx_wait_render_done_poll(SH4DER_PVRX_WAIT_US);
    pvr_wait_render_done();                                 /* unconditional backstop: returns at once when idle (DERIVED from reading pvr_scene.c, NOT RUN), sleeps only on a missed scene_finish or 100 ms timeout */
}

static unsigned psm_a_unhold(sh4der_psm_a_target_t *t) {
    unsigned bad = 0u;

    if (t->cut.on) {
        psm_a_wait_done();
        bad = sh4der_tilecut_end(&t->cut);
        t->cut_bad += bad;
        t->cut_on = 0;
        t->cut_held = -1;
    }
    return bad;
}

static void psm_a_vtx(float x, float y, float z, uint32_t argb, int eol) {
    pvr_vertex_t v;

    v.flags = eol ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    v.x = x; v.y = y; v.z = z;
    v.u = 0.0f; v.v = 0.0f;
    v.argb = argb; v.oargb = 0u;
    pvr_prim(&v, sizeof v);
}

/* A rectangle as one strip: z in the target is 1 / w, larger is nearer. */
static void psm_a_quad(float x, float y, float w, float h, float z, uint32_t argb) {
    psm_a_vtx(x, y, z, argb, 0);
    psm_a_vtx(x + w, y, z, argb, 0);
    psm_a_vtx(x, y + h, z, argb, 0);
    psm_a_vtx(x + w, y + h, z, argb, 1);
}

int sh4der_psm_a_target_init(sh4der_psm_a_target_t *t, const sh4der_psm_a_cfg_t *cfg) {
    pvr_poly_cxt_t c;
    unsigned lines;

    if (!t || !cfg) return 0;
    memset(t, 0, sizeof *t);
    t->cut_held = -1;
    t->cfg = *cfg;
    if (cfg->cut < SH4DER_PSM_A_CUT_NONE || cfg->cut > SH4DER_PSM_A_CUT_HOLD) return 0;
    if (cfg->size < 32u || cfg->size > 1024u || (cfg->size & 31u) || cfg->tw == 0u || cfg->rows == 0u) return 0;
    if (cfg->row < cfg->row0 || cfg->row - cfg->row0 + psm_a_tiles(cfg->size) > cfg->rows) return 0;     /* the pass must sit inside the table's rows */
    lines = cfg->row * 32u + cfg->size;
    t->bytes = lines * cfg->size * 2u;
    t->base = (void *)pvr_mem_malloc(t->bytes);
    if (!t->base) { t->bytes = 0u; return 0; }
    t->tex = (uint8_t *)t->base + cfg->row * 32u * cfg->size * 2u;

    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                 /* the map pass: flat colour, no culling (a caster is seen from either side) */
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.shading = PVR_SHADE_FLAT;
    pvr_poly_compile(&t->hdr_caster, &c);

    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)cfg->size, (int)cfg->size, (pvr_ptr_t)t->tex, PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.shading = PVR_SHADE_GOURAUD;
    c.txr.env = PVR_TXRENV_MODULATE;
    c.txr.uv_clamp = PVR_UVCLAMP_UV;                        /* outside the map is its white border: lit */
    c.blend.src = PVR_BLEND_DESTCOLOR; c.blend.dst = PVR_BLEND_ZERO;     /* the screen times the target */
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;               /* the layer has the ground's own depth */
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&t->hdr_layer, &c);

    sh4der_tilecut_init_rows(&t->cut, cfg->tw, cfg->rows, cfg->row0);
    t->ready = 1;
    return 1;
}

void sh4der_psm_a_target_free(sh4der_psm_a_target_t *t) {
    if (!t || !t->base) return;
    psm_a_wait_done();
    if (t->cut.on) { sh4der_tilecut_end(&t->cut); t->cut_on = 0; t->cut_held = -1; }
    pvr_mem_free((pvr_ptr_t)t->base);
    memset(t, 0, sizeof *t);
    t->cut_held = -1;
}

void sh4der_psm_a_target_forget(sh4der_psm_a_target_t *t) {
    if (!t) return;
    memset(t, 0, sizeof *t);                                /* nothing to write back: the table and the target are gone with the old PVR */
    t->cut_held = -1;
}

unsigned sh4der_psm_a_release(sh4der_psm_a_target_t *t) {
    unsigned bad = 0u;

    if (!t || !t->ready || t->open) return 0u;
    bad = t->cut_bad;
    t->cut_bad = 0u;
    if (t->cut.on) {
        psm_a_wait_done();
        bad += sh4der_tilecut_end(&t->cut);
        t->cut_on = 0;
        t->cut_held = -1;
    }
    return bad;
}

void sh4der_psm_a_find(sh4der_psm_a_target_t *t) {
    if (t && t->ready && t->cfg.cut && !t->cut.on) sh4der_tilecut_find(&t->cut);
}

int sh4der_psm_a_begin(sh4der_psm_a_target_t *t, sh4der_psm_a_t *a) {
    float n;
    unsigned tiles;

    if (!t || !t->ready || t->open) return 0;
    if (!a || !a->built || a->size != t->cfg.size) {
        psm_a_unhold(t);                                    /* never leave a held table cut when no pass will run */
        return 0;
    }
    n = (float)t->cfg.size;
    tiles = psm_a_tiles(t->cfg.size);
    if (t->cfg.cut == SH4DER_PSM_A_CUT_PASS && t->cut.n >= t->cut.need) {
        psm_a_wait_done();                                  /* never rewrite the table under a render that reads it */
        t->cut_on = sh4der_tilecut_begin_at(&t->cut, tiles, tiles, t->cfg.row);
        if (!t->cut_on) t->cut_refused++;
    } else if (t->cfg.cut == SH4DER_PSM_A_CUT_HOLD && t->cut.n >= t->cut.need) {
        if (pvr_wait_ready() < 0) {                         /* TA did not become ready: do not trust PVR_ISP_TILEMAT_ADDR */
            psm_a_unhold(t);
            t->cut_refused++;
        } else {
            const volatile uint32_t *active = sh4der_tilecut_active();
            const int cur = sh4der_tilecut_index(&t->cut, active);
            const int next = sh4der_tilecut_other(&t->cut, active);

            if (cur < 0 || next < 0) {
                psm_a_unhold(t);
                t->cut_refused++;
            } else {
                if (t->cut.tbl_on[cur]) {                   /* order broke: the render in flight read a cut table */
                    t->cut_broken++;
                    (void)sh4der_pvrx_wait_render_done_poll(SH4DER_PVRX_WAIT_US);
                    pvr_wait_render_done();
                    t->cut_bad += sh4der_tilecut_end_one(&t->cut, (unsigned)cur);
                    if (t->cut_held == cur) t->cut_held = -1;
                }
                if (sh4der_tilecut_is_cut_at(&t->cut, (unsigned)next, tiles, tiles, t->cfg.row)) {
                    t->cut_on = 1;                          /* already held on the pass's own table: no wait, no write */
                    t->cut_held = next;
                } else {
                    if (t->cut.tbl_on[next]) t->cut_bad += sh4der_tilecut_end_one(&t->cut, (unsigned)next);
                    t->cut_on = sh4der_tilecut_begin_one_at(&t->cut, (unsigned)next, tiles, tiles, t->cfg.row);
                    t->cut_held = t->cut_on ? next : -1;
                    if (!t->cut_on) t->cut_refused++;
                }
            }
        }
    }
    if (pvr_scene_begin_rtt((pvr_ptr_t)t->base, t->cfg.size, t->cfg.row * 32u + t->cfg.size, t->cfg.size) < 0) {
        if (t->cut.on) {
            if (t->cfg.cut == SH4DER_PSM_A_CUT_HOLD) psm_a_unhold(t);
            else { sh4der_tilecut_end(&t->cut); t->cut_on = 0; t->cut_held = -1; }
        }
        return 0;
    }
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(&t->hdr_caster, sizeof t->hdr_caster);
    psm_a_quad(0.0f, a->y0, n, n, 0.1f, PSM_A_WHITE);       /* the lit ground, the farthest thing */
    t->open = 1;
    return 1;
}

int sh4der_psm_a_caster(sh4der_psm_a_target_t *t, sh4der_psm_a_t *a, const sh4der_psm_a_caster_t *c) {
    sh4der_psm_a_tri_t out[SH4DER_PSM_PVR_MAX_TRIS];
    uint32_t argb;
    unsigned i, m;

    if (!t || !a || !c || !t->open || !c->pos || !c->idx) return 0;
    if (!sh4der_psm_a_accept(a, c->nt, c->max_tris)) return 0;
    argb = sh4der_psm_a_argb(a, c->strength);
    for (i = 0; i < c->nt; i++) {
        const unsigned n = sh4der_psm_a_tri(a, c->pos[c->idx[i][0]], c->pos[c->idx[i][1]], c->pos[c->idx[i][2]], out);

        for (m = 0; m < n; m++) {
            psm_a_vtx(out[m].v[0].x, out[m].v[0].y, SH4DER_PSM_A_CAST_Z, argb, 0);
            psm_a_vtx(out[m].v[1].x, out[m].v[1].y, SH4DER_PSM_A_CAST_Z, argb, 0);
            psm_a_vtx(out[m].v[2].x, out[m].v[2].y, SH4DER_PSM_A_CAST_Z, argb, 1);
        }
    }
    return 1;
}

void sh4der_psm_a_end(sh4der_psm_a_target_t *t, sh4der_psm_a_t *a) {
    float n;

    if (!t || !a || !t->open) return;
    n = (float)t->cfg.size;
    psm_a_quad(0.0f, a->y0, n, 1.0f, 2.0f, PSM_A_WHITE);             /* the border, one texel wide, nearest: a lookup outside the map is lit */
    psm_a_quad(0.0f, a->y0 + n - 1.0f, n, 1.0f, 2.0f, PSM_A_WHITE);
    psm_a_quad(0.0f, a->y0, 1.0f, n, 2.0f, PSM_A_WHITE);
    psm_a_quad(n - 1.0f, a->y0, 1.0f, n, 2.0f, PSM_A_WHITE);
    pvr_list_finish();
    sh4der_pvrx_scene_finish();
}

unsigned sh4der_psm_a_settle(sh4der_psm_a_target_t *t) {
    unsigned bad = 0u;

    if (!t || !t->open) return 0u;
    bad = t->cut_bad;
    t->cut_bad = 0u;
    if (t->cfg.cut == SH4DER_PSM_A_CUT_HOLD && t->cut_on) {
        t->cut_on = 0;                                      /* the pass's table stays cut; the screen renders through the other table */
        t->open = 0;
        return bad;
    }
    psm_a_wait_done();                                      /* the render has read the table and written the map */
    if (t->cut_on) { bad += sh4der_tilecut_end(&t->cut); t->cut_on = 0; t->cut_held = -1; }
    else if (t->cfg.cut && t->cut.n < t->cut.need) sh4der_tilecut_find(&t->cut);   /* the last render was this pass: the table it went through */
    t->open = 0;
    return bad;
}

const pvr_poly_hdr_t *sh4der_psm_a_layer_header(const sh4der_psm_a_target_t *t) {
    if (!t || !t->ready || !sh4der_poly_hdr_ok((const uint32_t *)&t->hdr_layer)) return NULL;
    return &t->hdr_layer;
}

int sh4der_psm_a_layer_vertex(const sh4der_psm_a_t *a, const float p[3], float sx, float sy, float sz, int eol, pvr_vertex_t *out) {
    float u, v;

    if (!a || !p || !out || !sh4der_psm_a_uv(a, p, &u, &v)) return 0;
    out->flags = eol ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    out->x = sx; out->y = sy; out->z = sz;
    out->u = u; out->v = v;
    out->argb = PSM_A_WHITE; out->oargb = 0u;
    return 1;
}

#endif /* _arch_dreamcast */
