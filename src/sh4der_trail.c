/* SH4DER-PVR: the feed route's KOS draw code

   src/sh4der_trail.c
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

#include "sh4der_trail.h"

#ifdef _arch_dreamcast

#include <dc/video.h>

/* The column mask: 8 x 8 ARGB1555, alpha 1 on even columns, 0 on odd, black, pre-twiddled (the feed route's table). */
static const uint16_t trail_mask_data[SH4DER_TRAIL_MASK_BYTES / 2] __attribute__((aligned(4))) = {
    0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000,
    0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000,
    0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000,
    0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000,
};

pvr_ptr_t sh4der_trail_setup(void) {
    pvr_ptr_t mask = pvr_mem_malloc(SH4DER_TRAIL_MASK_BYTES);
    if (!mask) return NULL;
    pvr_txr_load(trail_mask_data, mask, SH4DER_TRAIL_MASK_BYTES);
    vid_set_dithering(false);
    pvr_set_vertical_scale(1.0f);
    return mask;
}

void sh4der_trail_src_get(sh4der_trail_src_t *s) {
    s->fb = pvr_get_front_buffer();
    s->flat = 0;
    s->hi_chip = ((uintptr_t)s->fb & PVR_RAM_SIZE) != 0;   /* which half of the 64-bit view the front buffer maps to */
    pvr_txr_set_stride(640);
}

/* A full-screen quad in the translucent list. Only the header's blend is the caller's choice. */
static void trail_quad_hdr(pvr_poly_cxt_t *c, pvr_poly_hdr_t *hdr, int src, int dst) {
    c->gen.culling = PVR_CULLING_NONE;
    c->gen.fog_type = PVR_FOG_DISABLE;
    c->depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c->depth.write = PVR_DEPTHWRITE_DISABLE;
    c->blend.src = src;
    c->blend.dst = dst;
    pvr_poly_compile(hdr, c);
}

static void trail_flat(int src, int dst, uint32_t argb, float z) {
    pvr_poly_cxt_t c;
    pvr_poly_hdr_t hdr;
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    trail_quad_hdr(&c, &hdr, src, dst);
    pvr_prim(&hdr, sizeof(hdr));
    pvr_vertex_t v = { 0 };
    v.z = z;
    v.argb = argb;
    for (int k = 0; k < 4; k++) {
        v.flags = k == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = (k & 1) ? 640.0f : 0.0f;
        v.y = (k & 2) ? 480.0f : 0.0f;
        pvr_prim(&v, sizeof(v));
    }
}

void sh4der_trail_keep(uint32_t argb, float z) {
    /* DESTCOLOR in the destination slot is the source colour: the frame x the colour per channel; alpha 0 clears the old destination alpha */
    trail_flat(PVR_BLEND_ZERO, PVR_BLEND_DESTCOLOR, argb & 0x00ffffffu, z);
}

void sh4der_trail_mask(pvr_ptr_t mask, float z) {
    pvr_poly_cxt_t c;
    pvr_poly_hdr_t hdr;
    if (!mask)   /* no texture (the setup found no video memory): nothing to draw */
        return;
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555, 8, 8, mask, PVR_FILTER_NONE);
    c.txr.alpha = PVR_TXRALPHA_ENABLE;
    c.txr.env = PVR_TXRENV_REPLACE;
    trail_quad_hdr(&c, &hdr, PVR_BLEND_ONE, PVR_BLEND_ONE);   /* + black; the destination alpha becomes the mask's */
    pvr_prim(&hdr, sizeof(hdr));
    pvr_vertex_t v = { 0 };
    v.z = z;
    v.argb = 0xffffffffu;
    for (int k = 0; k < 4; k++) {
        v.flags = k == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = (k & 1) ? 640.0f : 0.0f;
        v.y = (k & 2) ? 480.0f : 0.0f;
        v.u = (k & 1) ? 640.0f / 8.0f : 0.0f;
        v.v = (k & 2) ? 480.0f / 8.0f : 0.0f;
        pvr_prim(&v, sizeof(v));
    }
}

/* One column parity of the history, moved (dx, dy): the feed route's copy (halves of 320, the u offsets for parity and chip). */
static void trail_copy_pass(const sh4der_trail_src_t *s, int odd, int dx, int dy, uint32_t argb, float z) {
    pvr_poly_cxt_t c;
    pvr_poly_hdr_t hdr;
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED | PVR_TXRFMT_X32_STRIDE, 1024, 1024, s->fb, PVR_FILTER_NONE);
    c.txr.alpha = PVR_TXRALPHA_DISABLE;
    c.txr.env = PVR_TXRENV_MODULATEALPHA;
    c.blend.src = odd ? PVR_BLEND_INVDESTALPHA : PVR_BLEND_DESTALPHA;
    trail_quad_hdr(&c, &hdr, c.blend.src, PVR_BLEND_ONE);
    pvr_prim(&hdr, sizeof(hdr));
    const float k1024 = 1.0f / 1024.0f;
    const float uo = s->flat ? 0.0f : odd ? (s->hi_chip ? k1024 : -k1024) : (s->hi_chip ? 2.0f * k1024 : 0.0f);
    pvr_vertex_t v = { 0 };
    v.z = z;
    v.argb = argb;
    for (int half = 0; half < 2; half++)
        for (int k = 0; k < 4; k++) {
            v.flags = k == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            v.x = (float)(half * 320) + ((k & 1) ? 320.0f : 0.0f) + (float)dx;
            v.y = ((k & 2) ? 480.0f : 0.0f) + (float)dy;
            v.u = ((k & 1) ? 640.0f * k1024 : 0.0f) + uo;
            v.v = ((k & 2) ? 960.0f * k1024 : 0.0f) + (float)half * k1024;
            pvr_prim(&v, sizeof(v));
        }
}

void sh4der_trail_copy(const sh4der_trail_src_t *s, int dx, int dy, uint32_t argb, float z) {
    const uint32_t col = argb & 0x00ffffffu;   /* alpha 0: the passes leave the mask's alpha as it is */
    trail_copy_pass(s, 0, dx, dy, col, z);
    trail_copy_pass(s, 1, dx, dy, col, z + 1.0f);
}

void sh4der_trail_draw(const sh4der_trail_src_t *s, pvr_ptr_t mask, const sh4der_trail_frame_t *f, float z) {
    if (!(f->weight > 0.0f) || f->copies <= 0) return;
    sh4der_trail_keep(sh4der_taa_keep_argb(f->weight), z);
    sh4der_trail_mask(mask, z + 1.0f);
    for (int k = 0; k < f->copies && k < SH4DER_TRAIL_COPIES; k++)
        sh4der_trail_copy(s, f->dx[k], f->dy[k], sh4der_taa_add_argb(f->copy_w), z + 2.0f + 2.0f * (float)k);
}

void sh4der_trail_draw_taa(const sh4der_trail_src_t *s, pvr_ptr_t mask, const sh4der_taa_frame_t *f, float z) {
    if (!(f->weight > 0.0f) || f->cut) return;
    sh4der_trail_keep(sh4der_taa_keep_argb(f->weight), z);
    sh4der_trail_mask(mask, z + 1.0f);
    sh4der_trail_copy(s, f->copy_px[0], f->copy_px[1], sh4der_taa_add_argb(f->weight), z + 2.0f);
}

#endif /* _arch_dreamcast */
