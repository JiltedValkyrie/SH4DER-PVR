/* SH4DER-PVR: frame warps, the KOS draw code

   sh4der_warp.c
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_warp.c
    \brief   The warps' draw calls on the PVR (KOS). The maths is sh4der_warp.h.

    The texture is the frame (RGB565, 512 x 256, 320 x 240 used, pitch 320:
    the plane's target). The screen scale (sx, sy) maps frame pixels to
    screen pixels. Under a host build this file has no code: the draw calls
    exist only under _arch_dreamcast.

    \author Jilted Valkyrie
*/

#include "../include/sh4der_warp.h"

#ifdef _arch_dreamcast
#include <dc/pvr.h>

/* A context for the frame texture (the plane's format). `add`: TR list, ONE / ONE; else opaque. Point sampled when filter is NONE. */
static void warp_cxt_(pvr_poly_cxt_t *c, pvr_list_t list, pvr_ptr_t tex, int env, int add, int filter) {
    pvr_poly_cxt_txr(c, list, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED | PVR_TXRFMT_X32_STRIDE, SH4DER_WARP_TEX_W, SH4DER_WARP_TEX_H, tex, filter);
    c->gen.culling = PVR_CULLING_NONE;
    c->gen.fog_type = PVR_FOG_DISABLE;
    c->depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c->depth.write = PVR_DEPTHWRITE_DISABLE;
    c->txr.alpha = PVR_TXRALPHA_DISABLE;
    c->txr.env = env;
    if (add) {
        c->blend.src = PVR_BLEND_ONE;
        c->blend.dst = PVR_BLEND_ONE;
    }
}

/* One vertex: a grid point scaled to the screen, its sample. */
static void warp_vtx_(const sh4der_warp_vtx_t *p, float sx, float sy, float z, uint32_t argb, uint32_t flags) {
    pvr_vertex_t v = { 0 };
    v.flags = flags;
    v.x = p->x * sx;
    v.y = p->y * sy;
    v.z = z;
    v.u = p->u;
    v.v = p->v;
    v.argb = argb;
    pvr_prim(&v, sizeof(v));
}

/* One corner of a flat quad (k: bit 0 x, bit 1 y), at frame pixels (fx, fy), the UV (u, v). */
static void warp_corner_(float fx, float fy, float u, float v, float sx, float sy, float z, uint32_t argb, int last) {
    pvr_vertex_t p = { 0 };
    p.flags = last ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    p.x = fx * sx;
    p.y = fy * sy;
    p.z = z;
    p.u = u;
    p.v = v;
    p.argb = argb;
    pvr_prim(&p, sizeof(p));
}

/** \brief The warp: the frame drawn through an nx x ny grid (nx, ny <= the fine grid), opaque, as triangle strips, one per row.
    z: the depth (ALWAYS). argb: the vertex colour (white: the texture as it is). About 2 (nx + 1) ny vertices, 2 nx ny triangles. */
void sh4der_warp_draw_mesh(pvr_ptr_t tex, const sh4der_warp_frame_t *f, unsigned nx, unsigned ny, float sx, float sy, float z, uint32_t argb) {
    static sh4der_warp_vtx_t rows[2][SH4DER_WARP_FINE_X + 1];   /* two rows of the grid: 1.3 KB of RAM */
    pvr_poly_cxt_t c;
    pvr_poly_hdr_t hdr;
    unsigned i, j;
    if (nx < 1u || ny < 1u || nx > SH4DER_WARP_FINE_X || ny > SH4DER_WARP_FINE_Y)
        return;
    warp_cxt_(&c, PVR_LIST_OP_POLY, tex, PVR_TXRENV_REPLACE, 0, PVR_FILTER_BILINEAR);
    pvr_poly_compile(&hdr, &c);
    pvr_prim(&hdr, sizeof(hdr));
    sh4der_warp_row(f, nx, ny, 0u, rows[0]);
    for (j = 0; j < ny; j++) {
        sh4der_warp_row(f, nx, ny, j + 1u, rows[(j + 1u) & 1u]);
        for (i = 0; i <= nx; i++) {
            warp_vtx_(&rows[j & 1u][i], sx, sy, z, argb, PVR_CMD_VERTEX);
            warp_vtx_(&rows[(j + 1u) & 1u][i], sx, sy, z, argb, i == nx ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX);
        }
    }
}

/** \brief The radial blur: k copies of the frame zoomed about (cx, cy) (frame pixels), weights summing to 255 (brightness holds).
    Copy 0 is opaque and sharp (it covers the screen); copies 1 .. k-1 are TR ADD (ONE / ONE) at z + 0.25 i, after it. strength 0: the frame. */
void sh4der_warp_draw_blur(pvr_ptr_t tex, unsigned k, float strength, float cx, float cy, float sx, float sy, float z) {
    const float u1 = (float)SH4DER_WARP_FRAME_W * (1.0f / SH4DER_WARP_TEX_W);
    const float v1 = (float)SH4DER_WARP_FRAME_H * (1.0f / SH4DER_WARP_TEX_H);
    unsigned i, q;
    if (k < 1u || k > SH4DER_WARP_BLUR_COPIES)
        return;
    for (i = 0; i < k; i++) {
        pvr_poly_cxt_t c;
        pvr_poly_hdr_t hdr;
        const float s = sh4der_warp_blur_scale(i, k, strength);
        const uint32_t w = sh4der_warp_blur_weight(i, k);
        const uint32_t argb = 0xff000000u | (w << 16) | (w << 8) | w;
        const int add = i > 0u;
        warp_cxt_(&c, add ? PVR_LIST_TR_POLY : PVR_LIST_OP_POLY, tex, PVR_TXRENV_MODULATE, add, PVR_FILTER_BILINEAR);
        pvr_poly_compile(&hdr, &c);
        pvr_prim(&hdr, sizeof(hdr));
        for (q = 0; q < 4u; q++) {
            const float fx = (q & 1u) ? (float)SH4DER_WARP_FRAME_W : 0.0f, fy = (q & 2u) ? (float)SH4DER_WARP_FRAME_H : 0.0f;
            const float u = (q & 1u) ? u1 : 0.0f, v = (q & 2u) ? v1 : 0.0f;
            warp_corner_(cx + (fx - cx) * s, cy + (fy - cy) * s, u, v, sx, sy, z + 0.25f * (float)i, argb, q == 3u);
        }
    }
}

/** \brief The pixelate pass: each block (SH4DER_WARP_PIXEL_CELL) drawn flat with its centre texel, point sampled, opaque.
    4 vertices a block, (FRAME_W / CELL) (FRAME_H / CELL) blocks: 300 quads at 16 px, 1200 at 8 px. */
void sh4der_warp_draw_pixel(pvr_ptr_t tex, float sx, float sy, float z) {
    const unsigned c_px = SH4DER_WARP_PIXEL_CELL;
    const unsigned bw = SH4DER_WARP_FRAME_W / c_px, bh = SH4DER_WARP_FRAME_H / c_px;
    pvr_poly_cxt_t c;
    pvr_poly_hdr_t hdr;
    unsigned bx, by, q;
    warp_cxt_(&c, PVR_LIST_OP_POLY, tex, PVR_TXRENV_REPLACE, 0, PVR_FILTER_NONE);
    pvr_poly_compile(&hdr, &c);
    pvr_prim(&hdr, sizeof(hdr));
    for (by = 0; by < bh; by++)
        for (bx = 0; bx < bw; bx++) {
            float uv[2];
            const float x0 = (float)(bx * c_px), y0 = (float)(by * c_px), x1 = x0 + (float)c_px, y1 = y0 + (float)c_px;
            sh4der_warp_pixel_uv(bx, by, uv);
            for (q = 0; q < 4u; q++)
                warp_corner_((q & 1u) ? x1 : x0, (q & 2u) ? y1 : y0, uv[0], uv[1], sx, sy, z, 0xffffffffu, q == 3u);
        }
}

/** \brief The dither: the tile (sh4der_warp_dither_tile, 8 x 8 ARGB4444, loaded twiddled with pvr_txr_load) multiplied over the
    frame, TR list: src ZERO, dst DESTCOLOR (the frame x the tile). The tile repeats: the UVs run 0 .. FRAME_W / 8. */
void sh4der_warp_draw_dither(pvr_ptr_t tile, float sx, float sy, float z) {
    pvr_poly_cxt_t c;
    pvr_poly_hdr_t hdr;
    unsigned q;
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444, 8, 8, tile, PVR_FILTER_NONE);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.txr.alpha = PVR_TXRALPHA_DISABLE;
    c.txr.env = PVR_TXRENV_REPLACE;
    c.blend.src = PVR_BLEND_ZERO;
    c.blend.dst = PVR_BLEND_DESTCOLOR;
    pvr_poly_compile(&hdr, &c);
    pvr_prim(&hdr, sizeof(hdr));
    for (q = 0; q < 4u; q++) {
        const float fx = (q & 1u) ? (float)SH4DER_WARP_FRAME_W : 0.0f, fy = (q & 2u) ? (float)SH4DER_WARP_FRAME_H : 0.0f;
        warp_corner_(fx, fy, fx * (1.0f / 8.0f), fy * (1.0f / 8.0f), sx, sy, z, 0xffffffffu, q == 3u);
    }
}

#endif /* _arch_dreamcast */
