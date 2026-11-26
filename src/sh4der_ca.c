/* SH4DER-PVR: the frame texture and chromatic aberration, Dreamcast side

   sh4der_ca.c
   Version: v0.50
   The effect: splits the picture's colours toward the screen's edges, as a
     cheap lens does: red, green and blue fringes in the corners, none in the
     middle. To do it the game is first drawn into a texture, which this module
     also provides.
   Developed for: the SH4DER demo disc, then handed to a first-person shooter.
   Use it for: a camera or scope's colour fringe, a hit or glitch distortion,
     a dream or VHS look. The frame texture alone renders a game at 320 x 240
     for speed, or feeds a screen transition.

   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

#include "../include/sh4der_ca.h"
#include "../include/sh4der_guard.h"
#include <string.h>

#ifdef _arch_dreamcast

/* 1 when hdr was compiled into a polygon header (sh4der_poly_hdr_ok): a header that is not must never reach the tile
   accelerator. The words are read through a copy, so no wider pointer is cast. */
static int ca_header_ok(const pvr_poly_hdr_t *hdr) {
    uint32_t words[8];
    memcpy(words, hdr, sizeof(words));
    return sh4der_poly_hdr_ok(words);
}

/* Frees every frame texture in g and clears g->n. Safe with NULL. */
void sh4der_ca_shutdown(sh4der_ca_gpu_t *g) {
    unsigned i;
    if (!g)
        return;
    for (i = 0; i < SH4DER_CA_TEXTURES; i++)
        if (g->tex[i]) {
            pvr_mem_free(g->tex[i]);
            g->tex[i] = NULL;
        }
    g->n = 0;
}

/* Holds n frame textures, each in PVR memory: half size when half is non-zero, else full size. n is capped at SH4DER_CA_TEXTURES.
   An existing set of the same size and count is kept. On failure nothing is kept, short_kb is set to the KB still missing (or to one
   texture's KB when the free memory is in pieces), and 0 is returned. On success the texture stride is set to the render width. */
int sh4der_ca_setup(sh4der_ca_gpu_t *g, int half, unsigned n) {
    unsigned i, bytes;
    if (!g)
        return 0;
    half = half ? 1 : 0;
    if (n > SH4DER_CA_TEXTURES)
        n = SH4DER_CA_TEXTURES;
    if (g->n == n && g->half == half && (n == 0 || g->tex[0])) {   /* already so */
        g->short_kb = 0;
        return 1;
    }
    sh4der_ca_shutdown(g);
    g->half = half;
    sh4der_ca_size(half, &g->size);
    g->short_kb = 0;
    bytes = sh4der_ca_bytes(&g->size);
    for (i = 0; i < n; i++) {
        g->tex[i] = pvr_mem_malloc(bytes);
        if (!g->tex[i]) {   /* short: what the rest would need against what is free now; nothing is kept */
            const unsigned need = (n - i) * bytes, have = (unsigned)pvr_mem_available();
            g->short_kb = sh4der_ca_short_kb(need, have);
            if (!g->short_kb)
                g->short_kb = (bytes + 1023u) / 1024u;   /* free in pieces, none large enough: say one texture */
            sh4der_ca_shutdown(g);
            return 0;
        }
    }
    g->n = n;
    if (n)
        pvr_txr_set_stride(g->size.w);   /* the render targets' pitch as textures */
    return 1;
}

/* Starts a render into frame texture i at the texture's own size. Returns the result of pvr_scene_begin_rtt, or -1 when i is out of
   range or its texture is not held. The caller ends the render. */
int sh4der_ca_begin(const sh4der_ca_gpu_t *g, unsigned i) {
    if (!g || i >= g->n || !g->tex[i])
        return -1;
    return pvr_scene_begin_rtt(g->tex[i], g->size.w, g->size.h, g->size.w);   /* KOS: a sized render to texture, the stride its width */
}

/* Compiles a header that draws frame texture i in list. The texture is RGB565, linear (not twiddled), with a 32-bit stride. Culling is
   off, and depth always passes with no write. Texture alpha is off and src / dst are set only in the translucent list. */
static void ca_header(const sh4der_ca_gpu_t *g, unsigned i, pvr_poly_hdr_t *hdr, int list, int filter, int env, int src, int dst) {
    pvr_poly_cxt_t c;
    pvr_poly_cxt_txr(&c, list, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED | PVR_TXRFMT_X32_STRIDE, (int)g->size.tex_w, (int)g->size.tex_h, g->tex[i], filter);
    c.gen.culling = PVR_CULLING_NONE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.txr.env = env;
    if (list == PVR_LIST_TR_POLY) {
        c.txr.alpha = PVR_TXRALPHA_DISABLE;
        c.blend.src = src;
        c.blend.dst = dst;
    }
    pvr_poly_compile(hdr, &c);
}

/* Puts frame texture i on screen as one 640 x 480 quad at (dx, dy), scaled by scale, at depth z. alpha 1 gives an opaque quad in the
   opaque list. Under 1 it is a translucent quad in the translucent list, blended SRC_ALPHA / INV_SRC_ALPHA at that share. Half-size
   textures are filtered and use texel centres. Full size maps the picture's own edges 1 : 1. Vertices are written straight into the
   store queue by KOS direct render (pvr_dr_target / pvr_dr_commit), so do not upload a texture between the vertices of one quad. */
void sh4der_ca_quad(const sh4der_ca_gpu_t *g, unsigned i, float alpha, float dx, float dy, float z, float scale) {
    pvr_poly_hdr_t hdr;
    const int blend = alpha < 1.0f;
    float u0, v0, u1, v1;
    int q;
    if (!g || i >= g->n || !g->tex[i])
        return;
    if (!sh4der_finite_(alpha) || !sh4der_finite_(dx) || !sh4der_finite_(dy) || !sh4der_finite_(z) || !sh4der_finite_(scale) || !(scale > 0.0f))
        return;   /* a number that is not one, or a quad of no area: nothing is sent */
    ca_header(g, i, &hdr, blend ? PVR_LIST_TR_POLY : PVR_LIST_OP_POLY, g->half ? PVR_FILTER_BILINEAR : PVR_FILTER_NONE,
              blend ? PVR_TXRENV_MODULATEALPHA : PVR_TXRENV_REPLACE, PVR_BLEND_SRCALPHA, PVR_BLEND_INVSRCALPHA);
    if (g->half) {   /* filtered: texel centres, the texels past the picture are not its */
        sh4der_ca_uv(&g->size, 0.0f, 0.0f, &u0, &v0);
        sh4der_ca_uv(&g->size, 1.0f, 1.0f, &u1, &v1);
    } else {         /* 1 : 1: the picture's own edges */
        u0 = 0.0f; v0 = 0.0f;
        u1 = (float)g->size.w / (float)g->size.tex_w; v1 = (float)g->size.h / (float)g->size.tex_h;
    }
    if (!ca_header_ok(&hdr))
        return;
    pvr_prim(&hdr, sizeof(hdr));
    for (q = 0; q < 4; q++) {   /* KOS direct render: straight into the store queue */
        pvr_vertex_t *v = pvr_dr_target();
        v->flags = q == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v->x = (dx + ((q & 1) ? 640.0f : 0.0f)) * scale;
        v->y = (dy + ((q & 2) ? 480.0f : 0.0f)) * scale;
        v->z = z;
        v->u = (q & 1) ? u1 : u0;
        v->v = (q & 2) ? v1 : v0;
        v->argb = ((uint32_t)(sh4der_sat_(alpha) * 255.0f) << 24) | 0x00ffffffu;
        v->oargb = 0;
        pvr_dr_commit(v);
    }
}

/* Draws one colour channel (chan) of frame texture i over a w x h area, at depth z. It is SH4DER_CA_MESH_Y strips, each of
   SH4DER_CA_MESH_X + 1 columns of two vertices, with one list header per strip. Each vertex's texture coordinates come from
   sh4der_ca_sample, which moves the sample point (the aberration). Green goes in the opaque list. Red and blue go in the translucent
   list, added (ONE, ONE). Draws nothing when w or h is not above 0. Vertices come from KOS direct render, the same rule as
   sh4der_ca_quad. */
void sh4der_ca_channel(const sh4der_ca_gpu_t *g, unsigned i, int chan, float strength, float w, float h, float z) {
    pvr_poly_hdr_t hdr;
    const int add = chan != SH4DER_CA_GREEN;
    float k, inv_diag2;
    int gx, gy, e;
    if (!g || i >= g->n || !g->tex[i] || !(w > 0.0f) || !(h > 0.0f))
        return;
    if (!sh4der_finite_(w) || !sh4der_finite_(h) || !sh4der_finite_(z) || !sh4der_finite_(strength))
        return;   /* a number that is not one: nothing is sent (w and h above 0 give every cell an area) */
    ca_header(g, i, &hdr, add ? PVR_LIST_TR_POLY : PVR_LIST_OP_POLY, PVR_FILTER_BILINEAR, PVR_TXRENV_MODULATE, PVR_BLEND_ONE, PVR_BLEND_ONE);
    k = sh4der_ca_push(chan, strength, w, h);
    inv_diag2 = sh4der_div_(4.0f, w * w + h * h);
    if (!ca_header_ok(&hdr))
        return;
    for (gy = 0; gy < SH4DER_CA_MESH_Y; gy++) {
        pvr_prim(&hdr, sizeof(hdr));
        for (gx = 0; gx <= SH4DER_CA_MESH_X; gx++)
            for (e = 0; e < 2; e++) {
                const float cu = (float)gx * (1.0f / (float)SH4DER_CA_MESH_X), cv = (float)(gy + e) * (1.0f / (float)SH4DER_CA_MESH_Y);
                float pu, pv;
                pvr_vertex_t *v = pvr_dr_target();
                sh4der_ca_sample(k, sh4der_ca_r2(cu, cv, w, h, inv_diag2), cu, cv, &pu, &pv);
                v->flags = gx == SH4DER_CA_MESH_X && e == 1 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                v->x = cu * w;
                v->y = cv * h;
                v->z = z;
                sh4der_ca_uv(&g->size, pu, pv, &v->u, &v->v);
                v->argb = sh4der_ca_argb(chan);
                v->oargb = 0;
                pvr_dr_commit(v);
            }
    }
}

/* Draws one colour channel of frame texture i as bands between the client's rows, every row moved sideways by its
   shift. One strip a column of cells, running down the screen: SH4DER_CA_ROWS_MESH_X strips of two vertices a row, cut
   where two rows share a height (a break). Every triangle has area: a strip only spans rows whose heights rise, and a
   cell is (w + 2 x overscan) / SH4DER_CA_ROWS_MESH_X wide, above 0; a shift slants a band, it cannot flatten one. The
   table is checked whole before the first vertex is sent. KOS direct render, the same rule as sh4der_ca_quad. */
unsigned sh4der_ca_channel_rows(const sh4der_ca_gpu_t *g, unsigned i, int chan, float strength, float w, float h, float z,
                                const float *row_y, const float *row_shift, unsigned n, float overscan) {
    pvr_poly_hdr_t hdr;
    const int add = chan != SH4DER_CA_GREEN;
    float k, inv_diag2, wide, inv_h;
    unsigned r, tris = 0;
    int gx, e;

    if (!g || i >= g->n || !g->tex[i] || !row_y || !row_shift || n < 2u || !(w > 0.0f) || !(h > 0.0f)) {
        return 0;
    }
    if (!sh4der_finite_(w) || !sh4der_finite_(h) || !sh4der_finite_(z) || !sh4der_finite_(strength) || !sh4der_finite_(overscan) || overscan < 0.0f) {
        return 0;
    }
    for (r = 0; r < n; r++) {
        if (!sh4der_finite_(row_y[r]) || !sh4der_finite_(row_shift[r]) || (r > 0 && row_y[r] < row_y[r - 1u])) {
            return 0;   /* a number that is not one, or a row above the one before: nothing is sent */
        }
    }
    ca_header(g, i, &hdr, add ? PVR_LIST_TR_POLY : PVR_LIST_OP_POLY, PVR_FILTER_BILINEAR, PVR_TXRENV_MODULATE, PVR_BLEND_ONE, PVR_BLEND_ONE);
    if (!ca_header_ok(&hdr)) {
        return 0;
    }
    k = sh4der_ca_push(chan, strength, w, h);
    inv_diag2 = sh4der_div_(4.0f, w * w + h * h);
    wide = sh4der_fma_(2.0f, overscan, w);              /* the drawn picture's width, pixels */
    inv_h = sh4der_div_(1.0f, h);
    for (gx = 0; gx < SH4DER_CA_ROWS_MESH_X; gx++) {
        r = 0;
        while (r + 1u < n) {
            unsigned last = r + 1u;                     /* the last row of the strip that starts at row r */
            unsigned q;

            if (!(row_y[last] > row_y[r])) {
                r++;                                    /* a break, or a band of no height: no strip across it */
                continue;
            }
            while (last + 1u < n && row_y[last + 1u] > row_y[last]) {
                last++;
            }
            pvr_prim(&hdr, sizeof(hdr));
            for (q = r; q <= last; q++) {
                for (e = 0; e < 2; e++) {
                    const float cu = (float)(gx + e) * (1.0f / (float)SH4DER_CA_ROWS_MESH_X);
                    const float cv = row_y[q] * inv_h;
                    float pu, pv;
                    pvr_vertex_t *v = pvr_dr_target();

                    sh4der_ca_sample(k, sh4der_ca_r2(cu, cv, w, h, inv_diag2), cu, cv, &pu, &pv);
                    v->flags = q == last && e == 1 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                    v->x = sh4der_fma_(cu, wide, row_shift[q] - overscan);
                    v->y = row_y[q];
                    v->z = z;
                    sh4der_ca_uv(&g->size, pu, pv, &v->u, &v->v);
                    v->argb = sh4der_ca_argb(chan);
                    v->oargb = 0;
                    pvr_dr_commit(v);
                }
            }
            tris += 2u * (last - r);
            r = last;                                   /* the next strip can start at the row after a break */
        }
    }
    return tris;
}

#endif /* _arch_dreamcast */
