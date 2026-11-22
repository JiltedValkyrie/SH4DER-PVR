/* SH4DER-PVR: pooled additive sparks, the KallistiOS draw

   sh4der_spark.c
   Version: v0.10
   The effect: a gun flashes, a tracer draws to its hit and is gone, sparks fly
     off a wall and fall.
   Developed for: a third-person shooter's weapon effects.
   Use it for: muzzle flashes, tracers, impact sparks, a welding torch, a
     sparkler, fireflies, any few dozen bright short-lived things.

   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/* The pool and the quad maths are sh4der_spark.h (host tested). This file is the PVR side: one polygon header (32 B, the only
   memory it holds; the atlas is the caller's) and 4 vertices a live quad through pvr_prim, on the caller's translucent list.
   Per frame, TA: one header + at most SH4DER_SPARK_MAX x 4 vertices (24: 96 vertices, 3,104 B). One layer, ONE / ONE: the
   quads add, so their order does not matter and nothing is sorted here. Both guards of sh4der_guard.h: an uncompiled header
   draws nothing, a triangle with no screen area is not sent (a streak seen end on, a billboard far away). Compiles to nothing
   off the Dreamcast. */

#ifdef _arch_dreamcast

#include <kos.h>
#include "sh4der_spark.h"
#include "sh4der_guard.h"

static pvr_poly_hdr_t spark_hdr __attribute__((aligned(32)));

int sh4der_spark_init(void *texture, int width, int height, int txr_format) {
    pvr_poly_cxt_t c;

    if (!texture) { memset(&spark_hdr, 0, sizeof(spark_hdr)); return 0; }
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, txr_format, width, height, (pvr_ptr_t)texture, PVR_FILTER_BILINEAR);
    c.blend.src = PVR_BLEND_ONE;                    /* additive, no alpha: black adds nothing */
    c.blend.dst = PVR_BLEND_ONE;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;       /* hidden behind the world */
    c.depth.write = PVR_DEPTHWRITE_DISABLE;         /* and never hides anything itself */
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.shading = PVR_SHADE_GOURAUD;              /* a TAIL0 streak fades along its length */
    c.txr.env = PVR_TXRENV_MODULATE;                /* texture x vertex colour */
    c.txr.uv_clamp = PVR_UVCLAMP_UV;
    pvr_poly_compile(&spark_hdr, &c);
    return 1;
}

unsigned sh4der_spark_draw(const sh4der_spark_pool_t *pool, const sh4der_spark_view_t *view, const sh4der_spark_screen_t *screen) {
    unsigned i, sent = 0;
    int k, hdr_sent = 0;

    if (!pool || !view || !screen || !pool->live || !sh4der_poly_hdr_ok((const uint32_t *)(const void *)&spark_hdr)) return 0;
    for (i = 0; i < SH4DER_SPARK_MAX; i++) {
        sh4der_spark_vtx_t q[4];
        float p[4][3];
        int t0, t1;

        if (!sh4der_spark_quad(&pool->s[i], view, q)) continue;
        for (k = 0; k < 4; k++) { p[k][0] = q[k].x; p[k][1] = q[k].y; p[k][2] = q[k].z; }
        t0 = sh4der_tri_area_ok(p[0], p[1], p[2], view->near_z, SH4DER_GUARD_MIN_AREA2);
        t1 = sh4der_tri_area_ok(p[1], p[2], p[3], view->near_z, SH4DER_GUARD_MIN_AREA2);
        if (!t0 || !t1) continue;                   /* a quad: both halves or nothing (a lone half is a sliver) */
        for (k = 0; k < 4; k++) if (q[k].z < view->near_z) break;   /* never: sh4der_spark_quad keeps every corner at or past the near plane */
        if (k < 4) continue;
        if (!hdr_sent) { pvr_prim(&spark_hdr, sizeof(spark_hdr)); hdr_sent = 1; }
        for (k = 0; k < 4; k++) {
            pvr_vertex_t v;
            const float iz = shz_invf(q[k].z);

            v.flags = k == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            v.x = shz_fmaf(q[k].x * iz, view->focal_px, screen->cx);
            v.y = shz_fmaf(q[k].y * iz, -screen->focal_y_px, screen->cy);
            v.z = sh4der_spark_depth(screen, iz);       /* z_base + z_scale / z: the client's own depth scale */
            v.u = q[k].u; v.v = q[k].v;
            v.argb = q[k].argb; v.oargb = 0;
            pvr_prim(&v, sizeof(v));
        }
        sent++;
    }
    return sent;
}

#endif /* _arch_dreamcast */
