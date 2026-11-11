/* SH4DER-PVR: the frame texture and chromatic aberration, Dreamcast side

   sh4der_ca.c
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

#include "../include/sh4der_ca.h"

#ifdef _arch_dreamcast

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

int sh4der_ca_begin(const sh4der_ca_gpu_t *g, unsigned i) {
    if (!g || i >= g->n || !g->tex[i])
        return -1;
    return pvr_scene_begin_rtt(g->tex[i], g->size.w, g->size.h, g->size.w);   /* KOS: a sized render to texture, the stride its width */
}

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

void sh4der_ca_quad(const sh4der_ca_gpu_t *g, unsigned i, float alpha, float dx, float dy, float z, float scale) {
    pvr_poly_hdr_t hdr;
    const int blend = alpha < 1.0f;
    float u0, v0, u1, v1;
    int q;
    if (!g || i >= g->n || !g->tex[i])
        return;
    ca_header(g, i, &hdr, blend ? PVR_LIST_TR_POLY : PVR_LIST_OP_POLY, g->half ? PVR_FILTER_BILINEAR : PVR_FILTER_NONE,
              blend ? PVR_TXRENV_MODULATEALPHA : PVR_TXRENV_REPLACE, PVR_BLEND_SRCALPHA, PVR_BLEND_INVSRCALPHA);
    if (g->half) {   /* filtered: texel centres, the texels past the picture are not its */
        sh4der_ca_uv(&g->size, 0.0f, 0.0f, &u0, &v0);
        sh4der_ca_uv(&g->size, 1.0f, 1.0f, &u1, &v1);
    } else {         /* 1 : 1: the picture's own edges */
        u0 = 0.0f; v0 = 0.0f;
        u1 = (float)g->size.w / (float)g->size.tex_w; v1 = (float)g->size.h / (float)g->size.tex_h;
    }
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

void sh4der_ca_channel(const sh4der_ca_gpu_t *g, unsigned i, int chan, float strength, float w, float h, float z) {
    pvr_poly_hdr_t hdr;
    const int add = chan != SH4DER_CA_GREEN;
    float k, inv_diag2;
    int gx, gy, e;
    if (!g || i >= g->n || !g->tex[i] || !(w > 0.0f) || !(h > 0.0f))
        return;
    ca_header(g, i, &hdr, add ? PVR_LIST_TR_POLY : PVR_LIST_OP_POLY, PVR_FILTER_BILINEAR, PVR_TXRENV_MODULATE, PVR_BLEND_ONE, PVR_BLEND_ONE);
    k = sh4der_ca_push(chan, strength, w, h);
    inv_diag2 = sh4der_div_(4.0f, w * w + h * h);
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

#endif /* _arch_dreamcast */
