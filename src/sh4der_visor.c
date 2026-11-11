/* SH4DER-PVR: visor modes, KOS draw code

   sh4der_visor.c
   The effect: shows the world as through a device: green night vision, amber,
     or a heat picture, with grain and scanlines, fading in and out.
   Developed for: Halo: Dreamcast Evolved.
   Use it for: night-vision goggles, a thermal scope, a security-camera or
     old-monitor look, a robot's point of view.

   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE.

   Dreamcast only. The maths is in include/sh4der_visor.h (host-tested).
*/

#include "sh4der_visor.h"

#ifdef _arch_dreamcast

/* The screen as a strip of two triangles, in the frame's pixels. */
static void visor_quad(float u0, float v0, float u1, float v1, uint32_t argb, float z, int textured) {
    static const float xs[4] = { 0.0f, SH4DER_FOCUS_SCREEN_W, 0.0f, SH4DER_FOCUS_SCREEN_W };
    static const float ys[4] = { 0.0f, 0.0f, SH4DER_FOCUS_SCREEN_H, SH4DER_FOCUS_SCREEN_H };
    int i;
    pvr_vertex_t pv;
    for (i = 0; i < 4; i++) {
        pv.flags = i == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        pv.x = xs[i];
        pv.y = ys[i];
        pv.z = z;
        pv.u = i & 1 ? u1 : u0;
        pv.v = i & 2 ? v1 : v0;
        pv.argb = argb;
        pv.oargb = 0;
        if (!textured) {   /* untextured vertices carry no uv: keep the colour where it is read */
            pv.u = 0.0f;
            pv.v = 0.0f;
        }
        pvr_prim(&pv, sizeof(pv));
    }
}

/* Compile an untextured TR header with culling off. src and dst are the blend factors, as PVR_BLEND_* values. */
static void compile_col(pvr_poly_hdr_t *hdr, int src, int dst) {
    pvr_poly_cxt_t cxt;
    pvr_poly_cxt_col(&cxt, PVR_LIST_TR_POLY);
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.blend.src = (pvr_blend_mode_t)src;
    cxt.blend.dst = (pvr_blend_mode_t)dst;
    pvr_poly_compile(hdr, &cxt);
}

/* Make the headers and the two video memory textures (grain, scanline) for the visor. Returns 0 on success. Returns -1
   when the focus init fails or video memory is short; a texture that was taken is freed again in that case. */
int sh4der_visor_init(sh4der_visor_gpu_t *g) {
    pvr_poly_cxt_t cxt;
    if (sh4der_focus_init(&g->thermal) != 0)
        return -1;
    /* the look's layers: multiply, boost, add, invert (look.h's kinds, in order) */
    compile_col(&g->layer[SH4DER_LOOK_MULTIPLY], PVR_BLEND_ZERO, PVR_BLEND_DESTCOLOR);
    compile_col(&g->layer[SH4DER_LOOK_BOOST], PVR_BLEND_DESTCOLOR, PVR_BLEND_ONE);
    compile_col(&g->layer[SH4DER_LOOK_ADD], PVR_BLEND_ONE, PVR_BLEND_ONE);
    compile_col(&g->layer[SH4DER_LOOK_INVERT], PVR_BLEND_INVDESTCOLOR, PVR_BLEND_ZERO);
    g->grain_tex = pvr_mem_malloc(SH4DER_VISOR_GRAIN_BYTES);
    g->scan_tex = pvr_mem_malloc(SH4DER_VISOR_SCAN_BYTES);
    if (!g->grain_tex || !g->scan_tex) {   /* no video memory: give back the one that was got */
        if (g->grain_tex) pvr_mem_free(g->grain_tex);
        if (g->scan_tex) pvr_mem_free(g->scan_tex);
        g->grain_tex = NULL;
        g->scan_tex = NULL;
        return -1;
    }
    /* grain and scanline are multiplied over the frame: texture x destination */
    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, SH4DER_VISOR_GRAIN_TEX, SH4DER_VISOR_GRAIN_TEX,
                     g->grain_tex, PVR_FILTER_BILINEAR);
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.blend.src = PVR_BLEND_ZERO;
    cxt.blend.dst = PVR_BLEND_DESTCOLOR;
    pvr_poly_compile(&g->grain_hdr, &cxt);
    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, SH4DER_VISOR_SCAN_TEX, SH4DER_VISOR_SCAN_TEX,
                     g->scan_tex, PVR_FILTER_NONE);
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.blend.src = PVR_BLEND_ZERO;
    cxt.blend.dst = PVR_BLEND_DESTCOLOR;
    pvr_poly_compile(&g->scan_hdr, &cxt);
    /* vignette: black with alpha, over the frame */
    compile_col(&g->vig_hdr, PVR_BLEND_SRCALPHA, PVR_BLEND_INVSRCALPHA);
    return 0;
}

/* Draw n look layers in order, each as one full-screen colour quad on the TR list with its blend header. Each layer
   blends with the frame as it stands after the layers before it. A layer whose kind is not one of the four is skipped. */
void sh4der_visor_draw_grade(const sh4der_visor_gpu_t *g, const sh4der_look_layer_t *l, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if ((unsigned)l[i].kind > (unsigned)SH4DER_LOOK_INVERT)   /* not one of the four blends: no header for it */
            continue;
        pvr_prim(&g->layer[l[i].kind], sizeof(pvr_poly_hdr_t));
        visor_quad(0.0f, 0.0f, 0.0f, 0.0f, sh4der_look_argb(&l[i]), SH4DER_VISOR_Z, 0);
    }
}

/* Draw the thermal layer over the full screen on the TR list: the focus thermal texture, with alpha set from amount
   (clamped to 0..1). */
void sh4der_visor_draw_thermal(const sh4der_visor_gpu_t *g, float amount) {
    const uint32_t a = (uint32_t)(sh4der_sat_(amount) * 255.0f + 0.5f);
    pvr_prim(&g->thermal.hdr, sizeof(pvr_poly_hdr_t));
    visor_quad(0.0f, 0.0f, sh4der_focus_uv_max_u(), sh4der_focus_uv_max_v(), (a << 24) | 0x00ffffffu, SH4DER_VISOR_Z, 1);
}

/* Draw the grain over the full screen on the TR list, multiplied over the frame. The texture offset changes with frame,
   so the grain moves from frame to frame. */
void sh4der_visor_draw_grain(const sh4der_visor_gpu_t *g, unsigned frame) {
    /* scroll the tile across the screen: 640 / 32 = 20 tiles across, 480 / 32 = 15 down */
    const float du = (float)(frame % 97u) * SH4DER_VISOR_GRAIN_SPEED_U;
    const float dv = (float)(frame % 89u) * SH4DER_VISOR_GRAIN_SPEED_V;
    pvr_prim(&g->grain_hdr, sizeof(pvr_poly_hdr_t));
    visor_quad(du, dv, du + SH4DER_FOCUS_SCREEN_W / SH4DER_VISOR_GRAIN_TEX, dv + SH4DER_FOCUS_SCREEN_H / SH4DER_VISOR_GRAIN_TEX,
               0xffffffffu, SH4DER_VISOR_Z, 1);
}

/* Draw the scanline texture over the full screen on the TR list, multiplied over the frame. It repeats down the screen. */
void sh4der_visor_draw_scanline(const sh4der_visor_gpu_t *g) {
    const float tiles = SH4DER_FOCUS_SCREEN_H / (float)(SH4DER_VISOR_SCAN_TEX * SH4DER_VISOR_SCAN_PX);
    pvr_prim(&g->scan_hdr, sizeof(pvr_poly_hdr_t));
    visor_quad(0.0f, 0.0f, 1.0f, tiles, 0xffffffffu, SH4DER_VISOR_Z, 1);
}

/* Draw the vignette over the frame on the TR list: black with alpha, from the disc mask. The mask is a grid of
   SH4DER_FOCUS_GRID_Y rows, each a strip of SH4DER_FOCUS_GRID_X + 1 columns, two vertices a column. Untextured. Per-vertex
   alpha comes from strength and amount. */
void sh4der_visor_draw_vignette(const sh4der_visor_gpu_t *g, float strength, float amount) {
    sh4der_focus_mask_t m;
    unsigned i, j, k;
    pvr_vertex_t pv;

    m.shape = SH4DER_FOCUS_DISC;
    m.cx = SH4DER_FOCUS_SCREEN_W * 0.5f;
    m.cy = SH4DER_FOCUS_SCREEN_H * 0.5f;
    m.hw = SH4DER_VISOR_VIG_R;
    m.hh = 0.0f;
    m.feather = SH4DER_VISOR_VIG_FEATHER;
    pvr_prim(&g->vig_hdr, sizeof(pvr_poly_hdr_t));
    for (j = 0; j < SH4DER_FOCUS_GRID_Y; j++) {
        for (i = 0; i <= SH4DER_FOCUS_GRID_X; i++) {
            for (k = 0; k < 2; k++) {
                sh4der_focus_vtx_t v;
                sh4der_focus_vertex(&m, i, j, k, &v);
                pv.flags = v.eol ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                pv.x = v.x;
                pv.y = v.y;
                pv.z = SH4DER_VISOR_Z;
                pv.u = 0.0f;
                pv.v = 0.0f;
                pv.argb = sh4der_visor_vignette_argb(v.argb, strength, amount);
                pv.oargb = 0;
                pvr_prim(&pv, sizeof(pv));
            }
        }
    }
}

#endif /* _arch_dreamcast */
