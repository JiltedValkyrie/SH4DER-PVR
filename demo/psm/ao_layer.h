/* SH4DER-PVR: ambient occlusion on the ground from the PVR's own shadow target (demo/psm, AO method 2): the town's sink

   demo/psm/ao_layer.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   The maths (the directions, the slide, the target pixels, the one side, the header) moved to
   include/sh4der_aosky.h on 2026-11-26; read its top comment for the method and its status (the ledger rows it cites).
   What stays here is the town's part: psm_ao2_send, which sends the casters of the town's scene through psm_send.h's sink.

   Old names: the aliases below are the names this file had before the move (ao_* for sh4der_aosky_*, AO_* for
   SH4DER_AOSKY_*). New code uses the sh4der_aosky names; the aliases only keep a caller that was not switched compiling.

   Route A has the PVR draw the casters into a render target as the sun sees them, and lays the target over the ground as a
   multiply. The sky directions draw the same casters into THE SAME target K more times, each as a faint black, so the ground
   gets darker where the sky is blocked. The method and its costs: AO_METHOD.md. */

#ifndef PSM_AO_LAYER_H
#define PSM_AO_LAYER_H

#include <stdint.h>
#include "sh4der_aosky.h"

/* Old names, for callers not yet switched (see above). */
#define AO_DIRS_MAX   SH4DER_AOSKY_DIRS_MAX
#define AO_COT_EL     SH4DER_AOSKY_COT_EL
#define AO_FLOOR      SH4DER_AOSKY_FLOOR
#define AO_Z0         SH4DER_AOSKY_Z0
#define AO_ZSTEP      SH4DER_AOSKY_ZSTEP
#define AO_XY_LIMIT   SH4DER_AOSKY_XY_LIMIT
#define AO_AREA_MIN   SH4DER_AOSKY_AREA_MIN
#define ao_count      sh4der_aosky_count
#define ao_slope      sh4der_aosky_slope
#define ao_alpha      sh4der_aosky_alpha
#define ao_argb       sh4der_aosky_argb
#define ao_level      sh4der_aosky_level
#define ao_depth      sh4der_aosky_depth
#define ao_slide      sh4der_aosky_slide
#define ao_slide_map  sh4der_aosky_slide_map
#define ao_px         sh4der_aosky_px
#define ao_tri_side   sh4der_aosky_tri_side
#define ao_tri_ok     sh4der_aosky_tri_ok
#define ao_layer_header sh4der_aosky_header

/* ---- the town (psm_send.h first) --------------------------------------------------------------------------------

   psm_dc.c, in map_pass under PSM_AO_DEMO, after psm_send_map's opaque list is finished and with the translucent list
   of the same scene open:   ao_v = psm_ao2_send(&sink, &frame, &scene, target_size, k);   then pvr_list_finish.
   Before including this: #define PSM_AO2_KIND to the sink's header kind for these triangles (a new PSM_HK_ entry in
   psm_send.h: 4 words a vertex, translucent), and have PSM_SINK_HDR send psm_ao2_hdr[psm_ao2_unite] for it.
   psm_ao2_init() once after pvr_init. psm_ao2_send leaves its last matrix in XMTRX. */
#ifdef PSM_SEND_H

#ifndef PSM_AO2_KIND
#define PSM_AO2_KIND PSM_HK_FLAT            /* counts as 4 words a vertex; a real build names its own kind */
#endif
#ifndef PSM_AO2_GROUND_Y
#define PSM_AO2_GROUND_Y 0.0f
#endif

static int psm_ao2_unite __attribute__((unused)) = 0;             /* 0: plain blending (the ledger's runs); 1: the depth union, which FAILS on the PVR (PVR-127) */
static int psm_ao2_cull __attribute__((unused));                   /* 0 both sides; 1 or -1: one sign of area only */
static float psm_ao2_strength = 1.0f;         /* 0 .. 1 of the darkening */
static float psm_ao2_xy_[PSM_CV_MAX][2];
static uint8_t psm_ao2_ok_[PSM_CV_MAX];

#ifdef _arch_dreamcast
static pvr_poly_hdr_t psm_ao2_hdr[2];       /* [0] plain blending, [1] the depth union */

static inline int psm_ao2_init(void) {
    sh4der_aosky_header(&psm_ao2_hdr[0], 0);
    sh4der_aosky_header(&psm_ao2_hdr[1], 1);
    return 1;
}
#endif

/* The casters as k sky directions see them, into the target of `size`: every caster triangle of an object on the
   map's square (f->con), movers and static alike. Returns the vertices sent (3 a triangle). psm_frame_a_casters ran. */
static inline unsigned psm_ao2_send(psm_sink_t *sink, const psm_frame_t *f, const psm_scene_t *s, unsigned size, unsigned k) {
    const unsigned before = sink->verts, n = sh4der_aosky_count(k);
    const float half = 0.5f * (float)size, fsize = (float)size;
    const uint32_t argb = sh4der_aosky_argb(n, psm_ao2_strength);
    unsigned d, i, j;

    if (!n || !f->built || !psm_sink_hdr(sink, PSM_AO2_KIND)) return 0;
    for (d = 0; d < n; d++) {
        const float z = sh4der_aosky_depth(d);
        float slope[2], m[16];
        psm_xf_t xf;

        sh4der_aosky_slope(n, d, slope);
        sh4der_aosky_slide_map(f->map.m, slope, PSM_AO2_GROUND_Y, m);
        psm_xf_load(&xf, m);
        for (i = 0; i < s->cvn; i++) {
            float h[4];

            psm_xf_apply(&xf, s->cv[i], h);
            psm_ao2_ok_[i] = (uint8_t)(h[3] > 1.0e-6f && sh4der_aosky_px(h, sh4der_recip_pos_(h[3]), half, psm_ao2_xy_[i]));
        }
        for (j = 0; j < s->con; j++) {
            if (!f->con[j]) continue;
            for (i = s->co[j].t0; i < s->co[j].t0 + s->co[j].tn; i++) {
                const unsigned a = s->ct[i][0], b = s->ct[i][1], c = s->ct[i][2];

                if (!(psm_ao2_ok_[a] & psm_ao2_ok_[b] & psm_ao2_ok_[c])) continue;
                if (!sh4der_aosky_tri_ok(psm_ao2_xy_[a], psm_ao2_xy_[b], psm_ao2_xy_[c], fsize, psm_ao2_cull)) continue;
                psm_sink_vtx(sink, 0, psm_ao2_xy_[a][0], psm_ao2_xy_[a][1], z, 0.0f, 0.0f, argb, 0u);
                psm_sink_vtx(sink, 0, psm_ao2_xy_[b][0], psm_ao2_xy_[b][1], z, 0.0f, 0.0f, argb, 0u);
                psm_sink_vtx(sink, 1, psm_ao2_xy_[c][0], psm_ao2_xy_[c][1], z, 0.0f, 0.0f, argb, 0u);
            }
        }
    }
    psm_sink_list_end(sink);
    return sink->verts - before;
}

#endif /* PSM_SEND_H */

#endif /* PSM_AO_LAYER_H */
