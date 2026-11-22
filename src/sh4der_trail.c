/* SH4DER-PVR: the feed route's KOS draw code

   src/sh4der_trail.c
   Version: v0.10
   The effect: smears the picture along the camera's motion, so a fast turn
     blurs. The frame before is laid over this one, moved by how far the camera
     turned.
   Developed for: motion blur and the temporal blend in Halo: Dreamcast
     Evolved, on the screen's own front buffer, with no frame texture.
   Use it for: a speed blur while boosting, stunned or drunken vision, ghost
     trails behind fast things.

   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

#include "sh4der_trail.h"

#ifdef _arch_dreamcast

#include <dc/video.h>

/* The column mask: 8 x 8 ARGB1555, alpha 1 on even columns, 0 on odd, black. The texture is stored twiddled (the PVR's
   Morton-order texel layout, where x and y bits are interleaved), so the words below are in storage order, not row order:
   in that order each pair of equal values is one texel pair, which is why the pattern reads 1, 1, 0, 0 in memory yet gives
   alternating columns on screen. 0x8000 = alpha 1, black; 0x0000 = alpha 0, black. */
static const uint16_t trail_mask_data[SH4DER_TRAIL_MASK_BYTES / 2] __attribute__((aligned(4))) = {
    0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000,
    0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000,
    0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000,
    0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000,
};

/* Set up the trail: copy the column mask into video memory, turn dithering off, set the vertical scale to 1.0.
   Returns the mask texture, or NULL when video memory is short (sh4der_trail_mask then draws nothing). Uses the PVR,
   so call it after the PVR is up. */
pvr_ptr_t sh4der_trail_setup(void) {
    pvr_ptr_t mask = pvr_mem_malloc(SH4DER_TRAIL_MASK_BYTES);
    if (!mask) return NULL;
    pvr_txr_load(trail_mask_data, mask, SH4DER_TRAIL_MASK_BYTES);
    vid_set_dithering(false);          /* dithering would add a pattern to the front buffer that every later copy would re-blend and amplify */
    pvr_set_vertical_scale(1.0f);      /* 1.0 = no vertical stretch, so the buffer's rows map 1:1 onto the copy's texture rows */
    return mask;
}

/* Read the front buffer into s: its address, the 64-bit half it maps to, and a texture stride of 640 for the copy passes.
   Call it before sh4der_trail_draw or sh4der_trail_draw_taa, so the copies read the buffer that holds the last frame. */
void sh4der_trail_src_get(sh4der_trail_src_t *s) {
    s->fb = pvr_get_front_buffer();
    s->flat = 0;
    /* hi_chip: 1 when the front buffer lies in the upper half of video memory (its address has the PVR_RAM_SIZE bit set). The two
       halves of the 64-bit view are the two memory chips, and the column swap runs in opposite directions on them, so
       trail_copy_pass picks its u offset from this flag. flat = 0: the offsets are applied (set it to 1 afterwards to turn them off). */
    s->hi_chip = ((uintptr_t)s->fb & PVR_RAM_SIZE) != 0;
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

/* Draw a full-screen quad of one flat colour on the TR list, untextured, with the given blend factors. */
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

/* First pass of the trail: a full-screen colour quad on the TR list. The alpha byte of argb is cleared before the quad is
   drawn. z is the depth in the TR sort. */
void sh4der_trail_keep(uint32_t argb, float z) {
    /* DESTCOLOR in the destination slot is the source colour: the frame x the colour per channel; alpha 0 clears the old destination alpha */
    trail_flat(PVR_BLEND_ZERO, PVR_BLEND_DESTCOLOR, argb & 0x00ffffffu, z);
}

/* Second pass of the trail: the column mask, repeated over the screen (UVs run 0 .. 80 across and 0 .. 60 down, so the 8 x 8 mask repeats), on the TR list.
   Textured (ARGB1555, 8 x 8, point sampled, REPLACE). Draws nothing when mask is NULL. */
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

/* One column parity of the history, moved (dx, dy): the feed route's copy.

   Background. The "feed route" reads the picture already on screen (the front buffer) as a texture, so the previous frame is
   available as history for 0 bytes of extra video memory. Read back as a texture through the PVR's 64-bit view of video
   memory, the two 32-bit halves of each 64-bit word come out in the wrong order on alternate columns (see sh4der_trail.h).
   A naive copy would therefore have every other column swapped with its neighbour.

   What this function does about it. A copy is drawn twice (see sh4der_trail_copy), once for the even columns and once for the
   odd ones. Which columns a pass touches is chosen by the destination alpha that sh4der_trail_mask wrote (alpha 1 on even
   columns, 0 on odd): the even pass blends with DESTALPHA, the odd pass with INVDESTALPHA, so each pass only lands on its own
   columns. Each pass also shifts the texture coordinate u by a tiny amount, uo, that lands the sample on the correct
   column:
     - uo is 1/1024 of the texture width (one texel of the 1024 wide texture) in magnitude. The texture is 1024 wide even
       though the picture is 640, because the front buffer's row pitch is read as 1024 here.
     - even pass: 0 on the low half of the buffer, +2/1024 on the high half (hi_chip), the two texel step that undoes the
       swap there;
     - odd pass: +1/1024 on the high half, -1/1024 on the low half, the one texel step in the direction that undoes the swap.
     - flat (s->flat set by the caller) turns the offsets off for a setup in which they would shift the picture instead.
   Without the offsets, or with the wrong sign, the history appears one or two pixels sideways in alternate columns, which
   shows as comb-like fringes on every edge. Without the two passes and the mask, the same swap shows on every column.

   The quad is drawn as two halves of 320 pixels each (the loop over `half`) instead of one 640 wide quad: the screen x of a
   half is half x 320 plus its own 0 or 320 corner, and the second half samples one texel (1/1024) further down in v. v runs
   0 to 960/1024 for the 480 screen rows (two texture rows per screen row). These last details come from the library's own
   hardware measurements of the front buffer's layout; they cannot be derived from the code alone or checked on the host,
   so treat them as measured constants, not as something this file proves. */
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

/* Copy the history moved by (dx, dy): the even-column pass at z, then the odd-column pass at z + 1. argb's alpha
   byte is cleared, so the passes leave the mask's alpha alone. */
void sh4der_trail_copy(const sh4der_trail_src_t *s, int dx, int dy, uint32_t argb, float z) {
    const uint32_t col = argb & 0x00ffffffu;   /* alpha 0: the passes leave the mask's alpha as it is */
    trail_copy_pass(s, 0, dx, dy, col, z);
    trail_copy_pass(s, 1, dx, dy, col, z + 1.0f);
}

/* The full trail on the TR list, in this order: keep at z, mask at z + 1, then up to SH4DER_TRAIL_COPIES copies at
   z + 2 + 2k. Draws nothing when weight is 0 or copies is 0 or less. Call sh4der_trail_src_get first. */
void sh4der_trail_draw(const sh4der_trail_src_t *s, pvr_ptr_t mask, const sh4der_trail_frame_t *f, float z) {
    if (!(f->weight > 0.0f) || f->copies <= 0) return;
    sh4der_trail_keep(sh4der_taa_keep_argb(f->weight), z);
    sh4der_trail_mask(mask, z + 1.0f);
    for (int k = 0; k < f->copies && k < SH4DER_TRAIL_COPIES; k++)
        sh4der_trail_copy(s, f->dx[k], f->dy[k], sh4der_taa_add_argb(f->copy_w), z + 2.0f + 2.0f * (float)k);
}

/* The TAA form of the trail: keep at z, mask at z + 1, then one copy at z + 2, moved by copy_px. Draws nothing when
   weight is 0 or cut is set. Call sh4der_trail_src_get first. */
void sh4der_trail_draw_taa(const sh4der_trail_src_t *s, pvr_ptr_t mask, const sh4der_taa_frame_t *f, float z) {
    if (!(f->weight > 0.0f) || f->cut) return;
    sh4der_trail_keep(sh4der_taa_keep_argb(f->weight), z);
    sh4der_trail_mask(mask, z + 1.0f);
    sh4der_trail_copy(s, f->copy_px[0], f->copy_px[1], sh4der_taa_add_argb(f->weight), z + 2.0f);
}

#endif /* _arch_dreamcast */
