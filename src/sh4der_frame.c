/* SH4DER-PVR: the frame read, Dreamcast side

   sh4der_frame.c
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

#include "../include/sh4der_frame.h"

#ifdef _arch_dreamcast

/* Reads the frame on screen, from the PVR frame-buffer address (masked to PVR RAM size). This is not the render target, which may be
   half drawn. raw gets a GRID_X x GRID_Y grid of RGB565 samples. dark, if not NULL, gets the min565 of each sample and its neighbour
   3 right and 3 down. Returns the stamp id at pixel (1, 1). Reads nothing and returns 0 when fb_w or fb_h is below 2. Reads go
   straight to video memory. */
unsigned sh4der_frame_read(uint16_t raw[SH4DER_FRAME_GRID_X * SH4DER_FRAME_GRID_Y], uint16_t *dark, unsigned fb_w, unsigned fb_h) {
    /* The scan-out register: the frame the screen shows now (the render target may be half drawn). */
    const uintptr_t frame = PVR_GET(PVR_FB_ADDR) & (PVR_RAM_SIZE - 1u);
    const volatile uint16_t *fb = (const volatile uint16_t *)((uintptr_t)PVR_RAM_BASE + frame);
    unsigned x, y;
    if (fb_w < 2u || fb_h < 2u)   /* no frame: nothing read, stamp 0 */
        return 0;
    for (y = 0; y < SH4DER_FRAME_GRID_Y; y++) {
        const unsigned ry = fb_h * (2u * y + 1u) / (2u * SH4DER_FRAME_GRID_Y), dy = ry + 3u < fb_h ? ry + 3u : fb_h - 1u;
        const volatile uint16_t *row = fb + ry * fb_w, *row2 = fb + dy * fb_w;   /* the second sample: 3 right, 3 down, never past the frame */
        for (x = 0; x < SH4DER_FRAME_GRID_X; x++) {
            const unsigned px = fb_w * (2u * x + 1u) / (2u * SH4DER_FRAME_GRID_X), qx = px + 3u < fb_w ? px + 3u : fb_w - 1u;
            const uint16_t p = row[px];
            raw[y * SH4DER_FRAME_GRID_X + x] = p;
            if (dark)
                dark[y * SH4DER_FRAME_GRID_X + x] = sh4der_frame_min565(p, row2[qx]);
        }
    }
    return sh4der_frame_stamp_id(fb[fb_w + 1u]);   /* the block's pixel (1, 1) */
}

/* Draws the stamp block in the top-left corner: SH4DER_FRAME_STAMP_PX square, at depth z, in the colour for id. It goes in the
   translucent list with blend ONE / ZERO, so the block replaces what is under it. Depth always passes. The header is compiled on the
   first call and kept in static storage. Vertices come from KOS direct render. */
void sh4der_frame_stamp_draw(unsigned id, float z) {
    static pvr_poly_hdr_t hdr;
    static int ready;
    const uint32_t argb = sh4der_frame_stamp_argb(id);
    int q;
    if (!ready) {
        pvr_poly_cxt_t c;
        pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
        c.gen.culling = PVR_CULLING_NONE;
        c.gen.fog_type = PVR_FOG_DISABLE;
        c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        c.depth.write = PVR_DEPTHWRITE_DISABLE;
        c.blend.src = PVR_BLEND_ONE;
        c.blend.dst = PVR_BLEND_ZERO;
        pvr_poly_compile(&hdr, &c);
        ready = 1;
    }
    pvr_prim(&hdr, sizeof(hdr));
    for (q = 0; q < 4; q++) {   /* KOS direct render: straight into the store queue */
        pvr_vertex_t *v = pvr_dr_target();
        v->flags = q == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v->x = (q & 1) ? (float)SH4DER_FRAME_STAMP_PX : 0.0f;
        v->y = (q & 2) ? (float)SH4DER_FRAME_STAMP_PX : 0.0f;
        v->z = z;
        v->u = v->v = 0.0f;
        v->argb = argb;
        v->oargb = 0;
        pvr_dr_commit(v);
    }
}

#endif /* _arch_dreamcast */
