/* SH4DER-PVR: fake depth of field, KOS draw code

   sh4der_focus.c
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE.

   Dreamcast only. The maths is in include/sh4der_focus.h (host-tested).
*/

#include "sh4der_focus.h"

#ifdef _arch_dreamcast

int sh4der_focus_init(sh4der_focus_gpu_t *g) {
    pvr_poly_cxt_t cxt;

    g->tex = pvr_mem_malloc(SH4DER_FOCUS_TEX_BYTES);
    if (!g->tex)
        return -1;
    /* linear (non-twiddled) RGB565: loaded with a plain copy, no twiddle pass */
    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED,
                     SH4DER_FOCUS_TEX_W, SH4DER_FOCUS_TEX_H, g->tex, PVR_FILTER_BILINEAR);
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.blend.src = PVR_BLEND_SRCALPHA;
    cxt.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&g->hdr, &cxt);
    return 0;
}

void sh4der_focus_upload(sh4der_focus_gpu_t *g, const uint16_t *blurred) {
    pvr_txr_load(blurred, g->tex, SH4DER_FOCUS_TEX_BYTES);
}

void sh4der_focus_submit(const sh4der_focus_gpu_t *g, const sh4der_focus_mask_t *m, float z) {
    unsigned i, j, k;
    pvr_vertex_t pv;

    pvr_prim(&g->hdr, sizeof(g->hdr));
    for (j = 0; j < SH4DER_FOCUS_GRID_Y; j++) {
        for (i = 0; i <= SH4DER_FOCUS_GRID_X; i++) {
            for (k = 0; k < 2; k++) {
                sh4der_focus_vtx_t v;
                sh4der_focus_vertex(m, i, j, k, &v);
                pv.flags = v.eol ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                pv.x = v.x;
                pv.y = v.y;
                pv.z = z;
                pv.u = v.u;
                pv.v = v.v;
                pv.argb = v.argb;
                pv.oargb = 0;
                pvr_prim(&pv, sizeof(pv));
            }
        }
    }
}

#endif /* _arch_dreamcast */
