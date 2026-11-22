/* SH4DER-PVR: branching bolts, the KallistiOS draw

   sh4der_bolt.c
   Version: v0.10
   The effect: thin jagged strands run down a surface, branching, and the pattern shifts a few times a second.
   Developed for: lightning around a large ship.
   Use it for: lightning, an arc, a spark gap, a discharge, cracks of light, a lit root system.

   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/* The bolt state and the quad maths are sh4der_bolt.h (host tested). This file is the PVR side: one polygon header (32 B, the only memory it
   holds) and 4 vertices a quad through pvr_prim, on the caller's translucent list. Per frame, TA: one header + at most
   8 x (12 + 3 x 6) core quads and 8 x 12 halo quads (default look: 36 + 24 quads, 240 vertices, 7.7 KB). One layer, ONE / ONE: the quads add, so
   their order does not matter and nothing is sorted here. Both guards of sh4der_guard.h: an uncompiled header draws nothing, a triangle with no screen
   area is not sent. Compiles to nothing off the Dreamcast. */

#ifdef _arch_dreamcast

#include <kos.h>
#include "sh4der_bolt.h"
#include "sh4der_guard.h"

static pvr_poly_hdr_t bolt_hdr __attribute__((aligned(32)));   /* the compiled additive, untextured header */

/* Builds the one polygon header the bolts use: untextured, Gouraud, additive (ONE / ONE), depth tested but never written, no culling.
   Call once after pvr_init, before the first sh4der_bolt_draw. Takes no arguments and allocates nothing.
   Returns 1 when the compiled header passes the sh4der_guard.h check, 0 when it does not (then sh4der_bolt_draw draws nothing). */
int sh4der_bolt_init_draw(void) {
    pvr_poly_cxt_t c;

    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    c.blend.src = PVR_BLEND_ONE;                    /* additive, no alpha: black adds nothing */
    c.blend.dst = PVR_BLEND_ONE;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;       /* hidden behind the world */
    c.depth.write = PVR_DEPTHWRITE_DISABLE;         /* and never hides anything itself */
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.shading = PVR_SHADE_GOURAUD;
    pvr_poly_compile(&bolt_hdr, &c);
    return sh4der_poly_hdr_ok((const uint32_t *)(const void *)&bolt_hdr);
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
        v.z = sh4der_spark_depth(screen, iz);       /* z_base + z_scale / z: the client's own depth scale */
        v.u = 0.0f; v.v = 0.0f;
        v.argb = q[k].argb; v.oargb = 0;
        pvr_prim(&v, sizeof(v));
    }
    return 1;
}

/* Sends every visible bolt quad to the translucent list: first the wide halo ribbons, then the thin bright cores.
   b is the bolt state, p its parameters (must pass sh4der_bolt_params_valid), anchors the strand start points and directions,
   view the camera (focal length in pixels, near plane) and screen the screen centre and depth scale (all in the caller's units).
   Call inside the translucent list of an open scene. Returns the number of quads actually sent (units: quads, not vertices).
   Bad input (any NULL pointer, invalid parameters, or an uncompiled header from a failed init) draws nothing and returns 0.
   A quad with no screen area, or one behind the near plane, is skipped and not counted. */
unsigned sh4der_bolt_draw(const sh4der_bolt_t *b, const sh4der_bolt_params_t *p, const sh4der_bolt_anchor_t *anchors, const sh4der_spark_view_t *view,
                          const sh4der_spark_screen_t *screen) {
    unsigned k, n, pass, sent = 0;
    int hdr_sent = 0;

    if (!b || !anchors || !view || !screen || !sh4der_bolt_params_valid(p) || !sh4der_poly_hdr_ok((const uint32_t *)(const void *)&bolt_hdr)) return 0;
    n = sh4der_bolt_segments(p);
    for (pass = 0; pass < 2; pass++) {              /* the halo ribbons first, then the cores on top (the blend adds, the order only keeps the TA list tidy) */
        for (k = 0; k < n; k++) {
            sh4der_spark_vtx_t q[4];

            if (!sh4der_bolt_quad(b, p, anchors, k, pass == 0, view, q)) continue;
            if (!hdr_sent) { pvr_prim(&bolt_hdr, sizeof(bolt_hdr)); hdr_sent = 1; }
            sent += (unsigned)send_quad(q, view, screen);
        }
    }
    return sent;
}

#endif /* _arch_dreamcast */
