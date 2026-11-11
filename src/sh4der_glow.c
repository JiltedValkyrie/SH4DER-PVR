/* SH4DER-PVR: the glow effect, Dreamcast draw code

   sh4der_glow.c
   The effect: makes bright things spill light. Lamps and the sun bleed a soft
     glow over what is round them, and the sun throws streaks of light across
     the screen (god rays).
   Developed for: the sun and the lamps in Halo: Dreamcast Evolved, proven on
     the demo disc.
   Use it for: sun rays through a window or between trees, glowing lamps, neon
     signs, lava, head lamps at night, a white-hot explosion.

   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

#include <string.h>
#include "../include/sh4der_glow.h"

#ifdef _arch_dreamcast

/* One quad: four vertices at (x, y), w by h, depth z, with per-corner texture coordinates u and v and colour argb. Written with
   pvr_prim (copies, not direct render). The caller passes the header. */
static void glow_quad(const pvr_poly_hdr_t *hdr, float x, float y, float w, float h, float z,
                      const float u[4], const float v[4], const uint32_t argb[4]) {
    int q;
    pvr_prim(hdr, sizeof(*hdr));
    for (q = 0; q < 4; q++) {
        pvr_vertex_t vt;
        vt.flags = q == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        vt.x = x + ((q & 1) ? w : 0.0f);
        vt.y = y + ((q & 2) ? h : 0.0f);
        vt.z = z;
        vt.u = u[q];
        vt.v = v[q];
        vt.argb = argb[q];
        vt.oargb = 0;
        pvr_prim(&vt, sizeof(vt));
    }
}

/* Frees both glow textures and clears the pointers. Safe after a failed init. */
void sh4der_glow_free(sh4der_glow_t *g) {
    if (g->big) pvr_mem_free(g->big);
    if (g->small) pvr_mem_free(g->small);
    g->big = g->small = NULL;
}

/* Compiles a glow texture header for the translucent list: RGB565 linear texture, bilinear filter, no culling, depth always passes with
   no write, ADD blend (ONE / ONE), texture coordinates clamped. */
static void glow_header_txr(pvr_poly_hdr_t *hdr, pvr_ptr_t tex, int w, int h) {
    pvr_poly_cxt_t c;
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, w, h, tex, PVR_FILTER_BILINEAR);
    c.txr.env = PVR_TXRENV_MODULATE;   /* texel x vertex colour: the colour is the layer's weight */
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_ONE;
    c.blend.dst = PVR_BLEND_ONE;   /* ADD */
    c.txr.uv_clamp = PVR_UVCLAMP_UV;
    pvr_poly_compile(hdr, &c);
}

/* Allocates the big and small glow textures, fills the lookup table, and compiles the two texture headers and the darken header.
   Returns 0, and frees what it took, when allocation fails. Call once at setup. */
int sh4der_glow_init(sh4der_glow_t *g) {
    pvr_poly_cxt_t c;
    memset(g, 0, sizeof(*g));
    g->big = pvr_mem_malloc(SH4DER_GLOW_TEX_W * SH4DER_GLOW_TEX_H * 2);
    g->small = pvr_mem_malloc(SH4DER_GLOW_SMALL_TEX_W * SH4DER_GLOW_SMALL_TEX_H * 2);
    if (!g->big || !g->small) {
        sh4der_glow_free(g);
        return 0;
    }
    sh4der_glow_lut(g->lut);
    glow_header_txr(&g->hdr_big, g->big, SH4DER_GLOW_TEX_W, SH4DER_GLOW_TEX_H);
    glow_header_txr(&g->hdr_small, g->small, SH4DER_GLOW_SMALL_TEX_W, SH4DER_GLOW_SMALL_TEX_H);
    /* MULTIPLY: frame x colour (src ZERO, dst DESTCOLOR). */
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_ZERO;
    c.blend.dst = PVR_BLEND_DESTCOLOR;
    pvr_poly_compile(&g->hdr_darken, &c);
    return 1;
}

/* Per frame. grid holds GRID_X x GRID_Y RGB565 luma samples, converted through the lookup table. When keep is set, each channel
   keeps keep / 256 of the old texel and takes the rest from the new one. Writes the big stage, shrinks it into the small stage, and
   loads both textures with pvr_txr_load. Returns the mean luma of the new grid, 0..1. Call once per frame, after the list's wait
   (see sh4der_glow.h). */
float sh4der_glow_upload(sh4der_glow_t *g, const uint16_t grid[SH4DER_GLOW_GRID_X * SH4DER_GLOW_GRID_Y]) {
    unsigned long sum = 0;
    unsigned x, y;
    for (y = 0; y < SH4DER_GLOW_GRID_Y; y++)
        for (x = 0; x < SH4DER_GLOW_GRID_X; x++) {
            uint16_t t = sh4der_glow_texel(grid[y * SH4DER_GLOW_GRID_X + x], g->lut);
            if (g->keep) {   /* eased toward the new texel, per channel */
                const unsigned o = g->stage[y * SH4DER_GLOW_TEX_W + x], k = g->keep, n = 256u - k;
                t = (uint16_t)(((((o >> 11) & 31u) * k + ((t >> 11) & 31u) * n + 128u) >> 8) << 11 | ((((o >> 5) & 63u) * k + ((t >> 5) & 63u) * n + 128u) >> 8) << 5 |
                               (((o & 31u) * k + (t & 31u) * n + 128u) >> 8));
            }
            g->stage[y * SH4DER_GLOW_TEX_W + x] = t;
            sum += sh4der_glow_luma8_(t);
        }
    sh4der_glow_shrink4(g->stage, SH4DER_GLOW_TEX_W, g->stage_small, SH4DER_GLOW_SMALL_TEX_W);
    pvr_txr_load(g->stage, g->big, SH4DER_GLOW_TEX_W * SH4DER_GLOW_TEX_H * 2);
    pvr_txr_load(g->stage_small, g->small, SH4DER_GLOW_SMALL_TEX_W * SH4DER_GLOW_SMALL_TEX_H * 2);
    return sh4der_div_((float)sum, 255.0f * (float)(SH4DER_GLOW_GRID_X * SH4DER_GLOW_GRID_Y));
}

/* Draws the wide halo over the rectangle, in the translucent list. First, if darken > 0, a multiply layer of 1 - darken at depth z.
   Then the big map at weight big, at z + 0.25, and the small shrink at weight small, at z + 0.5. Both ADD. radius splits strength
   between the two (sh4der_glow_halo). A layer with weight 0 is skipped. */
void sh4der_glow_draw_halo(const sh4der_glow_t *g, float x, float y, float w, float h, float z, float strength, float radius, float darken) {
    static const float cu[4] = { 0.0f, 1.0f, 0.0f, 1.0f }, cv[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    float u[4], v[4], big, small;
    uint32_t col[4];
    int q;
    if (darken > 0.0f) {
        const uint32_t m = sh4der_glow_argb(1.0f - darken);
        for (q = 0; q < 4; q++) col[q] = m;
        glow_quad(&g->hdr_darken, x, y, w, h, z, cu, cv, col);
    }
    sh4der_glow_halo(strength, radius, &big, &small);
    if (big > 0.0f) {
        for (q = 0; q < 4; q++) {
            sh4der_glow_ray_uv(1.0f, 0.5f, 0.5f, cu[q], cv[q], &u[q], &v[q]);   /* s = 1: the plain map */
            col[q] = sh4der_glow_argb(big);
        }
        glow_quad(&g->hdr_big, x, y, w, h, z + 0.25f, u, v, col);   /* the map's own texture coordinates (80 x 60 cells of 128 x 64), not 0..1: 0..1 drew the glow shrunk toward the top left */
    }
    if (small > 0.0f) {
        /* The shrink covers the same frame: its own texel mapping (20 x 15 cells in 32 x 16). */
        for (q = 0; q < 4; q++) {
            u[q] = sh4der_glow_tex_(cu[q], SH4DER_GLOW_SMALL_X, SH4DER_GLOW_SMALL_TEX_W);
            v[q] = sh4der_glow_tex_(cv[q], SH4DER_GLOW_SMALL_Y, SH4DER_GLOW_SMALL_TEX_H);
            col[q] = sh4der_glow_argb(small);
        }
        glow_quad(&g->hdr_small, x, y, w, h, z + 0.5f, u, v, col);
    }
}

/* One copy of the map over the rectangle, scaled by s about (su, sv), as a SH4DER_GLOW_MESH_X x _Y grid of strips: the fade is by each vertex's own distance from the
   sun, so a sun in the middle of the screen keeps its rays (four corners alone gave the corners' fade: about none there). i: the copy's number (its weight), 0: `flat`
   on every vertex (a shaft copy). */
/* Copies are drawn in the translucent list with the big map's header, which is ADD. */
static void glow_copy(const sh4der_glow_t *g, float x, float y, float w, float h, float z, float s, float su, float sv, int i, float sun_fade, float share, float flat) {
    int gx, gy, k;
    for (gy = 0; gy < SH4DER_GLOW_MESH_Y; gy++) {
        pvr_prim((void *)&g->hdr_big, sizeof(g->hdr_big));
        for (gx = 0; gx <= SH4DER_GLOW_MESH_X; gx++)
            for (k = 0; k < 2; k++) {
                const float cu = (float)gx * (1.0f / (float)SH4DER_GLOW_MESH_X), cv = (float)(gy + k) * (1.0f / (float)SH4DER_GLOW_MESH_Y);
                pvr_vertex_t vt;
                vt.flags = gx == SH4DER_GLOW_MESH_X && k == 1 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                vt.x = x + cu * w;
                vt.y = y + cv * h;
                vt.z = z;
                sh4der_glow_ray_uv(s, su, sv, cu, cv, &vt.u, &vt.v);
                vt.argb = sh4der_glow_argb(i ? sh4der_glow_ray_colour(i, su, sv, cu, cv, sun_fade, share) : flat);
                vt.oargb = 0;
                pvr_prim(&vt, sizeof(vt));
            }
    }
}

/* Adds rays from the sun at (su, sv), its position in the map. rays is clamped to 1 .. 6. Ray copy i is at depth z + 0.05 i and scaled by
   sh4der_glow_ray_scale(i). Each copy's weight comes from the sun's fade and share. Draws nothing when sun_fade x share is 0 or below
   (sun off screen or unlit). Translucent list. */
void sh4der_glow_draw_rays(const sh4der_glow_t *g, float x, float y, float w, float h, float z, float su, float sv, float share, int rays) {
    const float sun_fade = sh4der_glow_sun_fade(su, sv);
    int i;
    if (rays < 1) rays = 1;
    if (rays > 6) rays = 6;
    if (!(sun_fade * share > 0.0f)) return;   /* sun off screen or unlit: nothing to add */
    for (i = 1; i <= rays; i++)
        glow_copy(g, x, y, w, h, z + 0.05f * (float)i, sh4der_glow_ray_scale(i), su, sv, i, sun_fade, share, 0.0f);
}

/* Adds one shaft copy at scale about (au, av). Its flat colour is set by weight, clamped to 0..1. Draws nothing when weight is 0 or
   below. Translucent list, ADD. */
void sh4der_glow_draw_shaft(const sh4der_glow_t *g, float x, float y, float w, float h, float z, float au, float av, float scale, float weight) {
    if (!(weight > 0.0f)) return;
    glow_copy(g, x, y, w, h, z, scale, au, av, 0, 0.0f, 0.0f, sh4der_sat_(weight));
}

#endif /* _arch_dreamcast */
