/* SH4DER-PVR: shadow shafts, Dreamcast side

   sh4der_shafts.c
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

#include "../include/sh4der_shafts.h"

#ifdef _arch_dreamcast

/* Make the shaft texture (RGB565, SH4DER_GLOW_TEX_W x SH4DER_GLOW_TEX_H) and its header. The header is on the TR list,
   bilinear, with src ZERO and dst DESTCOLOR. Returns 0 on success, -1 when g is NULL or video memory is short. */
int sh4der_shafts_init(sh4der_shafts_gpu_t *g) {
    pvr_poly_cxt_t c;
    if (!g)
        return -1;
    g->tex = pvr_mem_malloc(SH4DER_GLOW_TEX_W * SH4DER_GLOW_TEX_H * 2);
    if (!g->tex)
        return -1;
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, SH4DER_GLOW_TEX_W, SH4DER_GLOW_TEX_H, g->tex, PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.txr.env = PVR_TXRENV_REPLACE;
    c.txr.uv_clamp = PVR_UVCLAMP_UV;
    c.blend.src = PVR_BLEND_ZERO;        /* multiply: the frame x the texel */
    c.blend.dst = PVR_BLEND_DESTCOLOR;
    pvr_poly_compile(&g->hdr, &c);
    return 0;
}

/* Free the shaft texture and clear the pointer. Safe with NULL, or after an earlier shutdown. */
void sh4der_shafts_shutdown(sh4der_shafts_gpu_t *g) {
    if (g && g->tex) {
        pvr_mem_free(g->tex);
        g->tex = NULL;
    }
}

/* Load a RGB565 map into the shaft texture. Does nothing when g, its texture or map is NULL. Call it before
   sh4der_shafts_draw for the frame that uses the map. */
void sh4der_shafts_upload(const sh4der_shafts_gpu_t *g, const uint16_t *map) {
    if (g && g->tex && map)
        pvr_txr_load(map, g->tex, SH4DER_GLOW_TEX_W * SH4DER_GLOW_TEX_H * 2);
}

/* Draw the shafts over the w x h screen at depth z, on the TR list, with the header from init. The mesh is
   SH4DER_GLOW_MESH_Y strips. Each strip's header goes in with pvr_prim, then its vertices are written by direct render.
   Texture coordinates come from sh4der_glow_ray_uv; the header's uv clamp keeps samples at the map's edge. */
void sh4der_shafts_draw(const sh4der_shafts_gpu_t *g, float su, float sv, float scale, float w, float h, float z) {
    int gx, gy, e;
    if (!g || !g->tex)
        return;
    for (gy = 0; gy < SH4DER_GLOW_MESH_Y; gy++) {
        pvr_prim((void *)&g->hdr, sizeof(g->hdr));
        for (gx = 0; gx <= SH4DER_GLOW_MESH_X; gx++)
            for (e = 0; e < 2; e++) {
                const float cu = (float)gx * (1.0f / (float)SH4DER_GLOW_MESH_X), cv = (float)(gy + e) * (1.0f / (float)SH4DER_GLOW_MESH_Y);
                pvr_vertex_t *v = pvr_dr_target();   /* KOS direct render */
                v->flags = gx == SH4DER_GLOW_MESH_X && e == 1 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                v->x = cu * w;
                v->y = cv * h;
                v->z = z;
                sh4der_glow_ray_uv(scale, su, sv, cu, cv, &v->u, &v->v);
                v->argb = 0xffffffffu;
                v->oargb = 0;
                pvr_dr_commit(v);
            }
    }
}

#endif /* _arch_dreamcast */
