/* SH4DER-PVR: motes, the KallistiOS draw

   sh4der_motes.c
   Version: v0.10
   The effect: a steady drift of embers, dust or ash fixed in the world, hard and small.
   Developed for: embers over a burning deck.
   Use it for: embers, dust in a shaft, pollen, snow, ash, falling sparks, floating seeds.

   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/* The field maths and the sprite quads are sh4der_motes.h (host tested). This file is the PVR side: two polygon
   headers (64 B, the only memory it holds) and 4 vertices a sprite through pvr_prim. The masked sprites go in the
   punch-through list over one 16 x 16 mask (512 B VRAM, hard 1-bit dots, no sorting); the flat ones (2 - 3 px) go in
   the opaque list untextured (no VRAM). Depth test on and depth write on in both, so a speck is hidden by the world
   and hides what is behind it. Per frame, TA: one header + 4 vertices x 32 B a sprite (96 sprites: 12.3 KB). Both
   guards of sh4der_guard.h: an uncompiled header draws nothing, a triangle with no screen area is not sent. Compiles
   to nothing off the Dreamcast. */

#ifdef _arch_dreamcast

#include <kos.h>
#include "sh4der_motes.h"
#include "sh4der_guard.h"

static pvr_poly_hdr_t motes_hdr_pt __attribute__((aligned(32)));     /* the compiled punch-through, textured header */
static pvr_poly_hdr_t motes_hdr_op __attribute__((aligned(32)));     /* the compiled opaque, untextured header */

int sh4der_motes_init_masked(void *texture, int txr_format) {
    pvr_poly_cxt_t c;

    if (!texture) return 0;
    pvr_poly_cxt_txr(&c, PVR_LIST_PT_POLY, txr_format, (int)SH4DER_MOTES_MASK_SIDE, (int)SH4DER_MOTES_MASK_SIDE, (pvr_ptr_t)texture, PVR_FILTER_NONE);
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;       /* hidden behind the world */
    c.depth.write = PVR_DEPTHWRITE_ENABLE;          /* punch-through is opaque where it draws */
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&motes_hdr_pt, &c);
    return sh4der_poly_hdr_ok((const uint32_t *)(const void *)&motes_hdr_pt);
}

int sh4der_motes_init_flat(void) {
    pvr_poly_cxt_t c;

    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    c.depth.write = PVR_DEPTHWRITE_ENABLE;
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&motes_hdr_op, &c);
    return sh4der_poly_hdr_ok((const uint32_t *)(const void *)&motes_hdr_op);
}

/* Sends one quad (strip order 0 1 2 3) if both of its triangles have screen area; returns 1 when sent. */
static int send_quad(const sh4der_spark_vtx_t q[4], const sh4der_spark_view_t *view, const sh4der_spark_screen_t *screen) {
    float p[4][3];
    int k;

    for (k = 0; k < 4; k++) { p[k][0] = q[k].x; p[k][1] = q[k].y; p[k][2] = q[k].z; }
    if (!sh4der_tri_area_ok(p[0], p[1], p[2], view->near_z, SH4DER_GUARD_MIN_AREA2) ||
        !sh4der_tri_area_ok(p[1], p[2], p[3], view->near_z, SH4DER_GUARD_MIN_AREA2)) return 0;   /* a quad: both halves or nothing */
    for (k = 0; k < 4; k++) if (q[k].z < view->near_z) return 0;
    for (k = 0; k < 4; k++) {
        pvr_vertex_t v;
        const float iz = shz_invf(q[k].z);

        v.flags = k == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = shz_fmaf(q[k].x * iz, view->focal_px, screen->cx);
        v.y = shz_fmaf(q[k].y * iz, -screen->focal_y_px, screen->cy);
        v.z = iz;
        v.u = q[k].u; v.v = q[k].v;
        v.argb = q[k].argb; v.oargb = 0;
        pvr_prim(&v, sizeof(v));
    }
    return 1;
}

/* The shared loop: sends the sprites that pass `want`, behind the header `hdr` (sent once, before the first quad). */
static unsigned draw_list(const pvr_poly_hdr_t *hdr, const sh4der_motes_draw_t *d, unsigned n, int only_flat, int only_masked,
                          const sh4der_spark_view_t *view, const sh4der_spark_screen_t *screen) {
    unsigned i, sent = 0;
    int hdr_sent = 0;

    if (!d || !view || !screen || !sh4der_poly_hdr_ok((const uint32_t *)(const void *)hdr)) return 0;
    for (i = 0; i < n; i++) {
        sh4der_spark_vtx_t q[4];

        if ((only_flat && !d[i].flat) || (only_masked && d[i].flat)) continue;
        if (!sh4der_motes_quad(&d[i], q)) continue;
        if (!hdr_sent) { pvr_prim((void *)hdr, sizeof(*hdr)); hdr_sent = 1; }
        sent += (unsigned)send_quad(q, view, screen);
    }
    return sent;
}

unsigned sh4der_motes_draw_masked(const sh4der_motes_draw_t *d, unsigned n, int all, const sh4der_spark_view_t *view,
                                  const sh4der_spark_screen_t *screen) {
    return draw_list(&motes_hdr_pt, d, n, 0, !all, view, screen);
}

unsigned sh4der_motes_draw_flat(const sh4der_motes_draw_t *d, unsigned n, const sh4der_spark_view_t *view,
                                const sh4der_spark_screen_t *screen) {
    return draw_list(&motes_hdr_op, d, n, 1, 0, view, screen);
}

#endif /* _arch_dreamcast */
