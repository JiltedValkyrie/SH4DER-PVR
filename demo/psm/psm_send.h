/* SH4DER-PVR: what the shadow demo sends the tile accelerator, walked once for the console and the host (demo/psm)

   demo/psm/psm_send.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   psm_dc.c sends the frame through these walks (the sky, the ground, the models, route A's layer, the text, route A's map
   pass) and the host test runs the same walks with nothing sent, so the count the budget test holds is the count the
   console sends. The walks go through a sink (psm_sink_t): a header waits for its part's first vertex (a part with no vertex
   sends no header), each vertex is counted, and a strip's end adds its bytes in the TA's parameter buffer.

   The parameter bytes are an estimate from the PVR's documented format, not a measurement: the TA keeps a strip as pieces of
   up to six triangles, each piece its own 3-word header and its eight vertices (the two before it again); a vertex is
   x, y, z, then u, v (2 words) when textured, the colour, the offset colour when the header has one. KallistiOS reads the real
   figure (pvr_get_stats, vtx_buffer_used); the readout shows both. KallistiOS puts the background polygon (60 bytes) after the
   last vertex when the render starts, so a full buffer puts it past the end, into the tile bins.

   SHED: psm_send_plan counts the frame before it is sent and, when it would pass PSM_TA_SHARE of the vertex buffer, drops
   parts for that frame: 1 the hills, 2 the model triangles wholly past PSM_SHED_W of view depth, 3 route A's layer, 4 the
   ground cells wholly past PSM_SHED_W. The host's check (psm_sink_t.chk): every strip of 3 or more with its end flag, no
   header inside a strip, the coordinates finite, and the tile-bin entries a list (bins, an upper bound: each piece in every
   32 x 32 tile its triangles' boxes touch). psm_diag_line is the readout's line 11: the frame, the buffer, SHED.

   The includer defines PSM_SINK_HDR(kind) and PSM_SINK_VTX(eol, x, y, z, u, v, argb, oargb) for its store queue before
   including this; without them nothing is sent. Needs psm_scene.h and psm_sky.h before it. */

#ifndef PSM_SEND_H
#define PSM_SEND_H

#ifndef PSM_SINK_HDR
#define PSM_SINK_HDR(kind) ((void)(kind))
#define PSM_SINK_VTX(eol, x, y, z, u, v, argb, oargb) ((void)0)
#endif

/* the headers the demo sends, by part */
enum {
    PSM_HK_SKY, PSM_HK_SKYCOL, PSM_HK_GROUND, PSM_HK_GROUND_FOG, PSM_HK_LAYER, PSM_HK_MODEL, PSM_HK_MODEL_FOG, PSM_HK_FLAT,
    PSM_HK_TEXT, PSM_HK_DIM, PSM_HK_INSET_A, PSM_HK_INSET_B, PSM_HK_N
};
/* the TA's words a vertex under each header: xyz 3, uv 2 when textured, colour 1, offset 1 when specular */
static const uint8_t psm_hk_words[PSM_HK_N] = { 7, 4, 6, 6, 6, 4, 4, 4, 6, 4, 6, 6 };
static const uint8_t psm_hk_tr[PSM_HK_N] = { 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1 };   /* 1: the translucent list */
static const char *const psm_hk_name[PSM_HK_N] = { "sky", "skycol", "ground", "ground_fog", "layer", "model", "model_fog", "flat", "text", "dim", "inset_a", "inset_b" };

#define PSM_TA_VBUF      (1024u * 1024u)  /* psm_dc.c's vertex buffer, bytes a frame (pvr_init_params_t.vertex_buf_size) */
#define PSM_TA_SHARE     0.90f            /* SHED starts when the frame's estimate passes this share of it */
#define PSM_TA_BKG       64u              /* KallistiOS's background polygon after the last vertex, rounded up */
#define PSM_SHED_MAX     4u
#define PSM_SHED_W       40.0f            /* SHED 2 and 4: what is wholly past this view depth, metres */
#define PSM_TILES_X      20u              /* 640 x 480 in 32 x 32 tiles */
#define PSM_TILES_Y      15u
#define PSM_TILES        (PSM_TILES_X * PSM_TILES_Y)
#define PSM_OPB_WORDS    16u              /* KallistiOS's default bin block (PVR_BINSIZE_16) for both lists */
#define PSM_OPB_EXTRA    4u               /* psm_dc.c's opb_overflow_count */

/* host: the tile-bin entries of a frame, by list and tile */
typedef struct psm_bins {
    uint32_t last[2][PSM_TILES];          /* the piece that wrote the tile's last entry */
    uint32_t n[2][PSM_TILES];
    uint32_t id;
} psm_bins_t;

typedef struct psm_sink {
    int live;                             /* 1: through PSM_SINK_HDR / PSM_SINK_VTX; 0: counted only */
    int chk;                              /* 1: the host's checks and bins */
    uint8_t hdr_ok[PSM_HK_N];             /* 0: the header cannot be sent (never compiled), its part is not sent */
    unsigned shed;
    int pend, kind;                       /* the header waiting for its first vertex (-1: none); the one sent last */
    unsigned run;                         /* vertices of the open strip */
    unsigned verts, strips, hdrs, pieces;
    uint32_t bytes;                       /* the TA's parameter bytes, estimated */
    unsigned kv[PSM_HK_N];                /* vertices by header */
    /* the checks */
    unsigned bad_short, bad_open, bad_coord;
    float max_xy;
    float px[2], py[2];
    psm_bins_t *bins;
} psm_sink_t;

/* A sink for a frame or a pass: counts zero, every header allowed (the caller clears what it cannot send). */
static inline void psm_sink_reset(psm_sink_t *k, int live, int chk, unsigned shed, psm_bins_t *bins) {
    memset(k, 0, sizeof *k);
    memset(k->hdr_ok, 1, sizeof k->hdr_ok);
    k->live = live; k->chk = chk; k->shed = shed; k->bins = bins;
    k->pend = -1; k->kind = -1;
    if (bins) memset(bins, 0, sizeof *bins);
}

/* A part begins under header `kind`: 1 when it may be sent. A strip still open here is a fault. */
static inline int psm_sink_hdr(psm_sink_t *k, int kind) {
    if (k->run) { k->bad_open++; k->run = 0; }
    if (!k->hdr_ok[kind]) { k->pend = -1; return 0; }
    k->pend = kind;
    return 1;
}

/* A list ends: no strip may be open. */
static inline void psm_sink_list_end(psm_sink_t *k) {
    if (k->run) { k->bad_open++; k->run = 0; }
    k->pend = -1;
}

static inline void psm_sink_bin_(psm_sink_t *k, float x, float y) {
    psm_bins_t *b = k->bins;
    const unsigned t = k->run - 3u;               /* the strip's triangle index: this vertex closes it */
    float x0 = x, x1 = x, y0 = y, y1 = y;
    int tx0, tx1, ty0, ty1, i, j;
    const int list = psm_hk_tr[k->kind];

    if (t % 6u == 0u) b->id++;                    /* a new piece of the strip */
    for (i = 0; i < 2; i++) {
        if (k->px[i] < x0) x0 = k->px[i];
        if (k->px[i] > x1) x1 = k->px[i];
        if (k->py[i] < y0) y0 = k->py[i];
        if (k->py[i] > y1) y1 = k->py[i];
    }
    if (x1 < 0.0f || y1 < 0.0f || x0 >= PSM_SCR_W || y0 >= PSM_SCR_H) return;
    tx0 = x0 < 0.0f ? 0 : (int)(x0 * (1.0f / 32.0f)); tx1 = x1 >= PSM_SCR_W ? (int)PSM_TILES_X - 1 : (int)(x1 * (1.0f / 32.0f));
    ty0 = y0 < 0.0f ? 0 : (int)(y0 * (1.0f / 32.0f)); ty1 = y1 >= PSM_SCR_H ? (int)PSM_TILES_Y - 1 : (int)(y1 * (1.0f / 32.0f));
    for (j = ty0; j <= ty1; j++)
        for (i = tx0; i <= tx1; i++) {
            const unsigned c = (unsigned)j * PSM_TILES_X + (unsigned)i;

            if (b->last[list][c] != b->id) { b->last[list][c] = b->id; b->n[list][c]++; }
        }
}

static inline void psm_sink_chk_(psm_sink_t *k, float x, float y, float z) {
    const float ax = x < 0.0f ? -x : x, ay = y < 0.0f ? -y : y;

    if (!sh4der_finite_(x) || !sh4der_finite_(y) || !sh4der_finite_(z) || !(z > 0.0f)) k->bad_coord++;
    if (ax > k->max_xy) k->max_xy = ax;
    if (ay > k->max_xy) k->max_xy = ay;
    if (k->bins && k->run >= 3u) psm_sink_bin_(k, x, y);
    k->px[0] = k->px[1]; k->py[0] = k->py[1];
    k->px[1] = x; k->py[1] = y;
}

/* the strip ends: its pieces and their bytes */
static inline void psm_sink_eol_(psm_sink_t *k) {
    if (k->run < 3u) k->bad_short++;
    else {
        const unsigned t = k->run - 2u, p = (t + 5u) / 6u;

        k->bytes += 12u * p + (t + 2u * p) * 4u * psm_hk_words[k->kind];
        k->pieces += p;
    }
    k->strips++;
    k->run = 0;
}

static inline void psm_sink_vtx(psm_sink_t *k, int eol, float x, float y, float z, float u, float v, uint32_t argb, uint32_t oargb) {
    if (k->pend >= 0) {
        if (k->live) PSM_SINK_HDR(k->pend);
        k->kind = k->pend; k->pend = -1; k->hdrs++;
    }
    if (k->kind < 0) { k->bad_open++; return; }    /* a vertex with no header: never sent */
    if (k->live) PSM_SINK_VTX(eol, x, y, z, u, v, argb, oargb);
    k->verts++; k->run++; k->kv[k->kind]++;
    if (k->chk) psm_sink_chk_(k, x, y, z);
    if (eol) psm_sink_eol_(k);
    (void)u; (void)v; (void)argb; (void)oargb;
}

static inline void psm_send_quad(psm_sink_t *k, int kind, float x, float y, float w, float h, float z, float u0, float v0, float u1, float v1, uint32_t argb) {
    if (!psm_sink_hdr(k, kind)) return;
    psm_sink_vtx(k, 0, x, y, z, u0, v0, argb, 0u);
    psm_sink_vtx(k, 0, x + w, y, z, u1, v0, argb, 0u);
    psm_sink_vtx(k, 0, x, y + h, z, u0, v1, argb, 0u);
    psm_sink_vtx(k, 1, x + w, y + h, z, u1, v1, argb, 0u);
}

/* ---- the opaque list ----------------------------------------------------------------------------------------------- */

/* the air (psm_sky_build ran): the band under its header, then the cap and the hills under theirs; SHED 1 leaves the hills */
static inline void psm_send_sky(psm_sink_t *k, const psm_sky_out_t *sky) {
    unsigned i, end;

    if (sky->n_band && psm_sink_hdr(k, PSM_HK_SKY))
        for (i = 0; i < sky->n_band; i++) psm_sink_vtx(k, sky->eol[i], sky->v[i].x, sky->v[i].y, sky->v[i].z, sky->v[i].u, sky->v[i].v, sky->v[i].argb, sky->v[i].oargb);
    end = sky->n_band + sky->n_cap + (k->shed >= 1u ? 0u : sky->n_hill);
    if (end > sky->n_band && psm_sink_hdr(k, PSM_HK_SKYCOL))
        for (i = sky->n_band; i < end; i++) psm_sink_vtx(k, sky->eol[i], sky->v[i].x, sky->v[i].y, sky->v[i].z, sky->v[i].u, sky->v[i].v, sky->v[i].argb, sky->v[i].oargb);
}

/* the cell (r, c) is sent: psm_cell_ok, and under SHED 4 not wholly past PSM_SHED_W */
static inline int psm_send_cell_(const psm_sink_t *k, const psm_frame_t *f, unsigned r, unsigned c, int layer) {
    if (!psm_cell_ok(f, r, c, layer)) return 0;
    if (k->shed >= 4u) {
        const unsigned a = r * f->gcols + c, d = a + f->gcols;

        if (f->gclip[a][2] > PSM_SHED_W && f->gclip[a + 1u][2] > PSM_SHED_W && f->gclip[d][2] > PSM_SHED_W && f->gclip[d + 1u][2] > PSM_SHED_W) return 0;
    }
    return 1;
}

/* The ground as strips, a ring at a time, a strip broken where a cell is not sent. layer 0: the ground texture, lit (fogged
   under DRAW DISTANCE). layer 1: route A's multiply layer, the target's coordinates, white. Returns the vertices sent. */
static inline unsigned psm_send_ground(psm_sink_t *k, const psm_frame_t *f, const psm_scene_t *s, int layer) {
    const unsigned before = k->verts;
    unsigned r, c;

    if (!psm_sink_hdr(k, layer ? PSM_HK_LAYER : f->dist > 0.0f ? PSM_HK_GROUND_FOG : PSM_HK_GROUND)) return 0;
    for (r = 0; r + 1u < f->grows; r++) {
        c = 0;
        while (c + 1u < f->gcols) {
            unsigned end, j;

            if (!psm_send_cell_(k, f, r, c, layer)) { c++; continue; }
            end = c + 1u;                                   /* spokes c .. end: cells c .. end - 1 */
            while (end + 1u < f->gcols && psm_send_cell_(k, f, r, end, layer)) end++;
            for (j = c; j <= end; j++) {                    /* the outer ring's vertex, then the inner's */
                const unsigned o = (r + 1u) * f->gcols + j, i = r * f->gcols + j;

                if (layer) {
                    psm_sink_vtx(k, 0, f->gscr[o][0], f->gscr[o][1], f->gscr[o][2], f->guv[o][0], f->guv[o][1], 0xffffffffu, 0u);
                    psm_sink_vtx(k, j == end, f->gscr[i][0], f->gscr[i][1], f->gscr[i][2], f->guv[i][0], f->guv[i][1], 0xffffffffu, 0u);
                } else {
                    psm_sink_vtx(k, 0, f->gscr[o][0], f->gscr[o][1], f->gscr[o][2], f->gpos[o][0] * s->ground_uv, f->gpos[o][2] * s->ground_uv, f->gcol[o], 0u);
                    psm_sink_vtx(k, j == end, f->gscr[i][0], f->gscr[i][1], f->gscr[i][2], f->gpos[i][0] * s->ground_uv, f->gpos[i][2] * s->ground_uv, f->gcol[i], 0u);
                }
            }
            c = end;
        }
    }
    return k->verts - before;
}

/* model triangle i goes in a strip: placed with area (mtri 1), and under SHED 2 not wholly past PSM_SHED_W */
static inline int psm_send_mtri_(const psm_sink_t *k, const psm_frame_t *f, const psm_scene_t *s, unsigned i) {
    if (f->mtri[i] != 1u) return 0;
    if (k->shed >= 2u && f->mclip[s->mt[i][0]][2] > PSM_SHED_W && f->mclip[s->mt[i][1]][2] > PSM_SHED_W && f->mclip[s->mt[i][2]][2] > PSM_SHED_W) return 0;
    return 1;
}

/* The models: the runs of strip triangles as strips (the first's corners 0 and 1, then each one's 2), and those across the
   near plane cut there (psm_clip_near). */
static inline void psm_send_models(psm_sink_t *k, const psm_frame_t *f, const psm_scene_t *s) {
    unsigned i, j, end;

    if (!psm_sink_hdr(k, f->dist > 0.0f ? PSM_HK_MODEL_FOG : PSM_HK_MODEL)) return;
    for (i = 0; i < s->mtn; i++) {
        const unsigned a = s->mt[i][0], b = s->mt[i][1];

        if (f->mtri[i] == 2u) {
            psm_cv_t cv[4];
            const unsigned n = psm_clip_near(f, s, i, cv);

            for (j = 0; j < n; j++) psm_sink_vtx(k, j + 1u == n, cv[j].x, cv[j].y, cv[j].iw, 0.0f, 0.0f, cv[j].argb, 0u);
            continue;
        }
        if (!psm_send_mtri_(k, f, s, i)) continue;
        end = i;
        while (end + 1u < s->mtn && !s->mstart[end + 1u] && psm_send_mtri_(k, f, s, end + 1u)) end++;
        psm_sink_vtx(k, 0, f->mscr[a][0], f->mscr[a][1], f->mscr[a][2], 0.0f, 0.0f, f->mcol[a], 0u);
        psm_sink_vtx(k, 0, f->mscr[b][0], f->mscr[b][1], f->mscr[b][2], 0.0f, 0.0f, f->mcol[b], 0u);
        for (j = i; j <= end; j++) {
            const unsigned e = s->mt[j][2];

            psm_sink_vtx(k, j == end, f->mscr[e][0], f->mscr[e][1], f->mscr[e][2], 0.0f, 0.0f, f->mcol[e], 0u);
        }
        i = end;
    }
}

/* the opaque list: the air (when it is on), the ground, the models */
static inline void psm_send_op(psm_sink_t *k, const psm_frame_t *f, const psm_scene_t *s, const psm_sky_out_t *sky) {
    if (sky) psm_send_sky(k, sky);
    psm_send_ground(k, f, s, 0);
    psm_send_models(k, f, s);
    psm_sink_list_end(k);
}

/* ---- the translucent list ------------------------------------------------------------------------------------------- */

#define PSM_INSET_PX 128u

/* one line of the text texture at (x, y) on screen, on a dark strip */
static inline void psm_send_text_line(psm_sink_t *k, unsigned line, float x, float y) {
    const float v0 = (float)(line * 16u) * (1.0f / (float)PSM_TEXT_H), v1 = (float)(line * 16u + 16u) * (1.0f / (float)PSM_TEXT_H);

    psm_send_quad(k, PSM_HK_DIM, x - 4.0f, y - 1.0f, (float)PSM_TEXT_W + 8.0f, 18.0f, 900.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0x90000000u);
    psm_send_quad(k, PSM_HK_TEXT, x, y, (float)PSM_TEXT_W, 16.0f, 1000.0f, 0.0f, v0, 1.0f, v1, 0xffffffffu);
}

/* Part of a text line (columns c0 .. c1) at x, y in a colour, on no strip: the panel's text. */
static inline void psm_send_text_cols(psm_sink_t *k, unsigned line, unsigned c0, unsigned c1, float x, float y, uint32_t argb) {
    const float v0 = (float)(line * 16u) * (1.0f / (float)PSM_TEXT_H), v1 = (float)(line * 16u + 16u) * (1.0f / (float)PSM_TEXT_H);
    const float u0 = (float)(c0 * 8u) * (1.0f / (float)PSM_TEXT_W), u1 = (float)(c1 * 8u) * (1.0f / (float)PSM_TEXT_W);

    psm_send_quad(k, PSM_HK_TEXT, x + (float)(c0 * 8u), y, (float)((c1 - c0) * 8u), 16.0f, 1000.0f, u0, v0, u1, v1, argb);
}

/* The settings panel (psm_panel_lines wrote its text): the fill, the title, the tabs and the open one's underline, the
   hairlines, the chosen row's bar, the rows, the scrollbar, the hints. About 20 quads. */
static inline void psm_send_panel(psm_sink_t *k, const psm_state_t *st) {
    psm_panel_t p;
    unsigned i;

    psm_panel_layout(st, &p);
    psm_send_quad(k, PSM_HK_DIM, PSM_PANEL_X0, PSM_PANEL_Y0, PSM_PANEL_X1 - PSM_PANEL_X0, PSM_PANEL_Y1 - PSM_PANEL_Y0, 800.0f, 0.0f, 0.0f, 0.0f, 0.0f, PSM_PANEL_FILL);
    psm_send_text_cols(k, PSM_PANEL_LN_TITLE, 0u, PSM_PANEL_COLS, PSM_PANEL_TX, PSM_PANEL_TITLE_Y, PSM_PANEL_WHITE);
    for (i = 0; i < PSM_GRPS; i++)
        psm_send_text_cols(k, PSM_PANEL_LN_TABS, p.tab_c0[i], p.tab_c1[i], PSM_PANEL_TX, PSM_PANEL_TABS_Y, (int)i == st->grp ? PSM_PANEL_WHITE : PSM_PANEL_MUTE);
    if (st->grp >= 0 && st->grp < (int)PSM_GRPS)
        psm_send_quad(k, PSM_HK_DIM, PSM_PANEL_TX + (float)(p.tab_c0[st->grp] * 8u), PSM_PANEL_TABS_Y + 18.0f, (float)((p.tab_c1[st->grp] - p.tab_c0[st->grp]) * 8u), 2.0f,
                      850.0f, 0.0f, 0.0f, 0.0f, 0.0f, PSM_PANEL_ACCENT);
    psm_send_quad(k, PSM_HK_DIM, PSM_PANEL_X0, PSM_PANEL_HAIR1_Y, PSM_PANEL_X1 - PSM_PANEL_X0, 1.0f, 850.0f, 0.0f, 0.0f, 0.0f, 0.0f, PSM_PANEL_HAIR);
    psm_send_quad(k, PSM_HK_DIM, PSM_PANEL_X0, PSM_PANEL_HAIR2_Y, PSM_PANEL_X1 - PSM_PANEL_X0, 1.0f, 850.0f, 0.0f, 0.0f, 0.0f, 0.0f, PSM_PANEL_HAIR);
    for (i = 0; i < p.shown; i++) {
        const float y = PSM_PANEL_ROW_Y + (float)i * PSM_PANEL_PITCH;
        const int sel = p.top + i == p.sel;

        if (sel) psm_send_quad(k, PSM_HK_DIM, PSM_PANEL_X0 + 8.0f, y - 1.0f, PSM_PANEL_X1 - PSM_PANEL_X0 - 24.0f, 18.0f, 850.0f, 0.0f, 0.0f, 0.0f, 0.0f, PSM_PANEL_BAR);
        psm_send_text_cols(k, 1u + i, 0u, PSM_PANEL_COLS, PSM_PANEL_TX, y, sel ? PSM_PANEL_WHITE : PSM_PANEL_TEXT);
    }
    if (p.n > PSM_PANEL_ROWS) {                                          /* the scrollbar: the track, the window on it */
        const float x = PSM_PANEL_X1 - 10.0f, h = (float)PSM_PANEL_ROWS * PSM_PANEL_PITCH;

        psm_send_quad(k, PSM_HK_DIM, x, PSM_PANEL_ROW_Y, 3.0f, h, 850.0f, 0.0f, 0.0f, 0.0f, 0.0f, PSM_PANEL_TRACK);
        psm_send_quad(k, PSM_HK_DIM, x, PSM_PANEL_ROW_Y + h * (float)p.top / (float)p.n, 3.0f, h * (float)p.shown / (float)p.n, 860.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                      PSM_PANEL_MUTE);
    }
    psm_send_text_cols(k, PSM_PANEL_LN_FOOT, 0u, PSM_PANEL_COLS, PSM_PANEL_TX, PSM_PANEL_FOOT_Y, PSM_PANEL_MUTE);
}

/* The readout's lines where they go, the inset, and the heartbeat: a square in the top right corner that moves a step every
   frame the program sends (beat: the frame's number), so a photo of a stopped picture says whether frames still came. */
static inline void psm_send_hud(psm_sink_t *k, const psm_state_t *st, int a_drawn, unsigned beat) {
    const float base = 420.0f;
    unsigned i;

    psm_send_text_line(k, 0u, 16.0f, 24.0f);
    psm_send_text_line(k, 11u, 16.0f, 42.0f);                                  /* the frame, the vertex buffer, SHED */
    if (st->menu) psm_send_panel(k, st);                                       /* open: the panel has lines 1 .. 9, 12, 13 */
    else {
        psm_send_text_line(k, 9u, 16.0f, 60.0f);                               /* the controls */
        psm_send_text_line(k, 1u, 16.0f, base - (st->readout ? 5.0f * 18.0f : 18.0f));
        if (st->readout) {
            for (i = 2; i < 6u; i++) psm_send_text_line(k, i, 16.0f, base - (float)(6u - i) * 18.0f);
            psm_send_text_line(k, 10u, 16.0f, base - 6.0f * 18.0f);
        }
        psm_send_text_line(k, 6u, 16.0f, base);
        if (st->split) psm_send_text_line(k, 7u, 16.0f, base + 18.0f);
    }
    if (st->inset) {
        const float x = PSM_SCR_W - 16.0f - (float)PSM_INSET_PX, y = 290.0f - (float)PSM_INSET_PX;

        if (st->route != PSM_ROUTE_A) psm_send_quad(k, PSM_HK_INSET_B, x, y, (float)PSM_INSET_PX, (float)PSM_INSET_PX, 1000.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0xffffffffu);
        else if (a_drawn) psm_send_quad(k, PSM_HK_INSET_A, x, y, (float)PSM_INSET_PX, (float)PSM_INSET_PX, 1000.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0xffffffffu);
    }
    psm_send_quad(k, PSM_HK_DIM, PSM_SCR_W - 40.0f + (float)(beat & 7u) * 3.0f, 26.0f, 10.0f, 10.0f, 1000.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                  (beat & 8u) ? 0xffffd040u : 0xff40d0ffu);
}

/* the translucent list: route A's layer (SHED 3 leaves it), the readout. Returns the layer's vertices. */
static inline unsigned psm_send_tr(psm_sink_t *k, const psm_frame_t *f, const psm_scene_t *s, const psm_state_t *st, int a_drawn, unsigned beat) {
    const unsigned layer_v = a_drawn && k->shed < 3u ? psm_send_ground(k, f, s, 1) : 0u;

    psm_send_hud(k, st, a_drawn, beat);
    psm_sink_list_end(k);
    return layer_v;
}

/* The SHED level for this frame: the frame counted at each level from 0 until it fits PSM_TA_SHARE of vbuf bytes (the
   background polygon included); PSM_SHED_MAX when nothing fits. `k` is left with the chosen level's count. */
static inline unsigned psm_send_plan(psm_sink_t *k, const psm_frame_t *f, const psm_scene_t *s, const psm_state_t *st, const psm_sky_out_t *sky,
                                     int a_drawn, const uint8_t hdr_ok[PSM_HK_N], uint32_t vbuf) {
    const uint32_t cap = (uint32_t)((float)vbuf * PSM_TA_SHARE);
    unsigned shed;

    for (shed = 0; ; shed++) {
        psm_sink_reset(k, 0, 0, shed, NULL);
        memcpy(k->hdr_ok, hdr_ok, sizeof k->hdr_ok);
        psm_send_op(k, f, s, sky);
        psm_send_tr(k, f, s, st, a_drawn, 0u);
        if (k->bytes + PSM_TA_BKG <= cap || shed == PSM_SHED_MAX) return shed;
    }
}

/* The SHED level for the NEXT frame from what this one sent, with no dry count: `bytes` is the real send's own count
   (psm_sink_t.bytes after the frame's two lists, at level z->shed). One level up when the frame was over PSM_TA_SHARE
   of vbuf. One down when this frame plus what the level saved when it was taken (z->gap, measured the frame after the
   step up) would be under PSM_SHED_BACK of vbuf: a level is left only when the one under it would fit, so a still view
   does not flip. Against psm_send_plan: a frame late and a level a frame, for none of its cost (the plan walks the whole
   send once more every frame: 6 to 15 ms in the town, wiki PVR-195). A frame that stays under the share is sent the
   same. Zero the state at load. */
#define PSM_SHED_BACK 0.80f
typedef struct psm_lazy {
    unsigned shed;                  /* the level to send the next frame at */
    int rose;                       /* the last call stepped up: the next one measures what that saved */
    uint32_t over;                  /* the bytes of the frame that made it step up */
    uint32_t gap[PSM_SHED_MAX];     /* gap[i]: bytes level i + 1 saved against level i when it was last taken */
} psm_lazy_t;

static inline unsigned psm_send_plan_next(psm_lazy_t *z, uint32_t bytes, uint32_t vbuf) {
    const uint32_t cap = (uint32_t)((float)vbuf * PSM_TA_SHARE), back = (uint32_t)((float)vbuf * PSM_SHED_BACK);

    if (z->rose) { z->gap[z->shed - 1u] = z->over > bytes ? z->over - bytes : 0u; z->rose = 0; }
    if (bytes + PSM_TA_BKG > cap) {
        if (z->shed < PSM_SHED_MAX) { z->over = bytes; z->rose = 1; z->shed++; }
    } else if (z->shed && bytes + z->gap[z->shed - 1u] + PSM_TA_BKG <= back) z->shed--;
    return z->shed;
}

/* ---- route A's map pass (its own scene, into the target) ----------------------------------------------------------- */

/* The casters into the target: white first (far), each caster triangle in the shadow's colour, then a white frame one texel
   wide (nearest). A caster object off the map's square (f->con) is not clipped or sent. Sets f->tris and f->drawn; returns
   the triangles sent. psm_frame_a_casters first. */
static inline unsigned psm_send_map(psm_sink_t *k, psm_frame_t *f, const psm_scene_t *s, const psm_state_t *st, unsigned size, sh4der_psm_pvr_vtx_t out[][3]) {
    const float n = (float)size;
    const uint32_t grey = psm_pack_rgb(f->shadow_rgb[0], f->shadow_rgb[1], f->shadow_rgb[2]), white = 0xffffffffu;
    unsigned i, j, m, t, tris = 0;

    psm_send_quad(k, PSM_HK_FLAT, 0.0f, 0.0f, n, n, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, white);
    if (f->built && psm_sink_hdr(k, PSM_HK_FLAT)) {
        for (j = 0; j < s->con; j++)
            for (i = s->co[j].t0; i < s->co[j].t0 + s->co[j].tn; i++) {
                if (!psm_caster_on(s, i, st->movers_only)) continue;           /* CASTERS: movers only leaves the static ones out */
                f->tris++;
                if (!f->con[j]) continue;                                      /* the object is off the map's square */
                t = psm_pvr_tris_h(f->ma[s->ct[i][0]], f->ma[s->ct[i][1]], f->ma[s->ct[i][2]], size, out);
                for (m = 0; m < t; m++) {
                    const float area = (out[m][1].x - out[m][0].x) * (out[m][2].y - out[m][0].y) - (out[m][2].x - out[m][0].x) * (out[m][1].y - out[m][0].y);

                    if (!(area > 1.0e-3f || area < -1.0e-3f)) continue;        /* no area in the map: not sent */
                    psm_sink_vtx(k, 0, out[m][0].x, out[m][0].y, 1.0f, 0.0f, 0.0f, grey, 0u);
                    psm_sink_vtx(k, 0, out[m][1].x, out[m][1].y, 1.0f, 0.0f, 0.0f, grey, 0u);
                    psm_sink_vtx(k, 1, out[m][2].x, out[m][2].y, 1.0f, 0.0f, 0.0f, grey, 0u);
                    tris++;
                }
            }
    }
    f->drawn = tris;
    psm_send_quad(k, PSM_HK_FLAT, 0.0f, 0.0f, n, 1.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, white);
    psm_send_quad(k, PSM_HK_FLAT, 0.0f, n - 1.0f, n, 1.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, white);
    psm_send_quad(k, PSM_HK_FLAT, 0.0f, 0.0f, 1.0f, n, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, white);
    psm_send_quad(k, PSM_HK_FLAT, n - 1.0f, 0.0f, 1.0f, n, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, white);
    psm_sink_list_end(k);
    return tris;
}

/* ---- the tile bins (host) -------------------------------------------------------------------------------------------- */

/* The bin blocks a frame takes past each tile's first one, against the overflow pool KallistiOS makes (PSM_OPB_EXTRA x the
   first blocks of every tile and list). A block holds PSM_OPB_WORDS - 1 entries and a link; the list's end takes an entry. */
typedef struct psm_bin_use {
    unsigned worst_tile[2];               /* the most entries in one tile, by list */
    unsigned extra_blocks, pool_blocks;
} psm_bin_use_t;

static inline void psm_bins_use(const psm_bins_t *b, psm_bin_use_t *u) {
    unsigned l, t;

    memset(u, 0, sizeof *u);
    u->pool_blocks = PSM_OPB_EXTRA * PSM_TILES * 2u;
    for (l = 0; l < 2u; l++)
        for (t = 0; t < PSM_TILES; t++) {
            const unsigned n = b->n[l][t], blocks = (n + 1u + (PSM_OPB_WORDS - 2u)) / (PSM_OPB_WORDS - 1u);

            if (n > u->worst_tile[l]) u->worst_tile[l] = n;
            if (blocks > 1u) u->extra_blocks += blocks - 1u;
        }
}

/* ---- the readout's line 11 ------------------------------------------------------------------------------------------ */

/* The frame, the vertex buffer and SHED, so a photo of a stopped picture says how full the buffer was. The breadcrumbs of
   what was being done are psm_fault.h's. */
typedef struct psm_diag {
    unsigned frame;                       /* frames sent since boot */
    uint32_t est, used, used_max, vbuf;   /* bytes: this frame's estimate, KallistiOS's last and largest frame, the buffer */
    unsigned shed;
} psm_diag_t;

static inline unsigned psm_pct_(uint32_t a, uint32_t b) { return b ? (unsigned)(((uint64_t)a * 100u + b / 2u) / b) : 0u; }

static inline void psm_diag_line(const psm_diag_t *d, char ln[PSM_HUD_LINES][PSM_HUD_COLS + 1u]) {
    snprintf(ln[11], PSM_HUD_COLS + 1u, "F%06u TA est %4uK used %4uK max %4uK %3u%% of %uK SHED%u", d->frame % 1000000u,
             (unsigned)(d->est / 1024u), (unsigned)(d->used / 1024u), (unsigned)(d->used_max / 1024u), psm_pct_(d->used_max, d->vbuf),
             (unsigned)(d->vbuf / 1024u), d->shed);
}

#endif /* PSM_SEND_H */
