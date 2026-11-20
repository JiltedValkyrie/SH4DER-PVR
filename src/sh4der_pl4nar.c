/* SH4DER-PVR: pl4nar, real-time planar reflection, the KallistiOS part

   sh4der_pl4nar.c
   Version: v0.10
   The effect: a car's paint and glass show the street going by; a wet road, a
     polished floor, a pool or a shop window shows the world upside down in it.
   Developed for: a driving demo, a street reflected on a car body.
   Use it for: car paint, glass, chrome, water, wet ground, polished floors,
     mirrors: anything that lies in, or near, one plane.

   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/* The maths is sh4der_pl4nar.h (host tested). This file is the PVR side: the render target and the layer's polygon
   header. KallistiOS path: pvr_mem_malloc (the target, w x h x 2 B, the only memory held), pvr_scene_begin_rtt (a
   sized render-to-texture scene: the PVR renders w x h pixels at a pitch of w straight into texture memory, and the
   render starts when the lists are in, so the screen's scene queues behind it and no frame is lost), pvr_scene_finish,
   pvr_poly_cxt_txr + pvr_poly_compile (the header). pvr_scene_begin_rtt is not in every KallistiOS: define
   SH4DER_PL4NAR_HAVE_RTT where it is. Without it the scene is pvr_scene_begin_txr, whose rows are 1024 pixels apart:
   the target is then 1024 x h (256 x 128 of picture costs 256 KB) and the layer's u is scaled by w / 1024
   (target.u_scale). The PVR renders one scene at a time: the screen's scene waits for this one, and this one for
   the screen's last, so the time a client sees around the pass is mostly that queue, not this code. No maths here.
   Compiles to nothing off the Dreamcast. */

#ifdef _arch_dreamcast

#include <kos.h>
#include "sh4der_pl4nar.h"

static int pl4nar_pow2(unsigned x) { return x && !(x & (x - 1u)); }

int sh4der_pl4nar_target_init(sh4der_pl4nar_target_t *t, unsigned w, unsigned h) {
    if (!t) return 0;
    t->tex = NULL; t->w = t->h = t->bytes = t->pitch = 0u; t->u_scale = 1.0f;
    if (!pl4nar_pow2(w) || !pl4nar_pow2(h) || w < 8u || h < 8u || w > 1024u || h > 1024u) return 0;
#ifdef SH4DER_PL4NAR_HAVE_RTT
    t->pitch = w;
#else
    t->pitch = 1024u;
#endif
    t->tex = (void *)pvr_mem_malloc(t->pitch * h * 2u);
    if (!t->tex) { t->pitch = 0u; return 0; }
    t->w = w; t->h = h; t->bytes = t->pitch * h * 2u;
    t->u_scale = (float)w * (1.0f / (float)t->pitch);      /* hand: one divide at load, both powers of two */
    return 1;
}

void sh4der_pl4nar_target_free(sh4der_pl4nar_target_t *t) {
    if (!t || !t->tex) return;
    pvr_wait_ready();               /* the last scene's lists are with the renderer */
    pvr_wait_render_done();         /* and it has finished reading and writing the target */
    pvr_mem_free((pvr_ptr_t)t->tex);
    t->tex = NULL; t->w = t->h = t->bytes = t->pitch = 0u; t->u_scale = 1.0f;
}

int sh4der_pl4nar_target_begin(const sh4der_pl4nar_target_t *t) {
    if (!t || !t->tex) return 0;
#ifdef SH4DER_PL4NAR_HAVE_RTT
    if (pvr_scene_begin_rtt((pvr_ptr_t)t->tex, t->w, t->h, t->w) < 0) return 0;
#else
    {
        uint32_t rx = t->w, ry = t->h;

        pvr_scene_begin_txr((pvr_ptr_t)t->tex, &rx, &ry);
    }
#endif
    return 1;
}

void sh4der_pl4nar_target_end(void) { pvr_scene_finish(); }

int sh4der_pl4nar_pass(const sh4der_pl4nar_target_t *t, const sh4der_pl4nar_view_t *view, sh4der_pl4nar_draw_fn draw, void *user) {
    if (!view || !draw || !sh4der_pl4nar_target_begin(t)) return 0;
    sh4der_pl4nar_begin(view);
    draw(view, user);
    sh4der_pl4nar_target_end();
    return 1;
}

int sh4der_pl4nar_layer_header(const sh4der_pl4nar_target_t *t, void *hdr, unsigned additive) {
    pvr_poly_cxt_t c;

    if (!hdr) return 0;
    if (!t || !t->tex) { memset(hdr, 0, sizeof(pvr_poly_hdr_t)); return 0; }
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)t->pitch, (int)t->h, (pvr_ptr_t)t->tex, PVR_FILTER_BILINEAR);
    c.gen.alpha = PVR_ALPHA_ENABLE;
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.shading = PVR_SHADE_GOURAUD;               /* the share changes across a face */
    c.txr.alpha = PVR_TXRALPHA_DISABLE;              /* RGB565 has no alpha: the texel's alpha is 1 */
    c.txr.env = PVR_TXRENV_MODULATEALPHA;            /* texel x vertex colour, alpha = vertex alpha */
    c.txr.uv_clamp = PVR_UVCLAMP_UV;
    c.blend.src = PVR_BLEND_SRCALPHA;
    c.blend.dst = additive ? PVR_BLEND_ONE : PVR_BLEND_INVSRCALPHA;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;        /* the layer has the opaque surface's own depth */
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile((pvr_poly_hdr_t *)hdr, &c);
    return 1;
}

#endif /* _arch_dreamcast */
