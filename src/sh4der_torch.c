/* SH4DER-PVR: the flashlight hotspot by carving

   sh4der_torch.c
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

#include <stdint.h>
#include "../include/sh4der_torch.h"

/* The spokes' unit circle (16 spokes, 22.5 degrees apart), so a frame does no sin / cos. */
static const float unit_c[SH4DER_TORCH_SPOKES + 1] = {
    1.0f, 0.9238795f, 0.7071068f, 0.3826834f, 0.0f, -0.3826834f, -0.7071068f, -0.9238795f, -1.0f,
    -0.9238795f, -0.7071068f, -0.3826834f, 0.0f, 0.3826834f, 0.7071068f, 0.9238795f, 1.0f };
static const float unit_s[SH4DER_TORCH_SPOKES + 1] = {
    0.0f, 0.3826834f, 0.7071068f, 0.9238795f, 1.0f, 0.9238795f, 0.7071068f, 0.3826834f, 0.0f,
    -0.3826834f, -0.7071068f, -0.9238795f, -1.0f, -0.9238795f, -0.7071068f, -0.3826834f, 0.0f };

typedef enum { BAND_BOOST, BAND_CARVE } band_kind_t;

/* Pack three floats (clamped to 0..1) into an opaque ARGB8888 colour. */
static uint32_t argb_of(float r, float g, float b) {
    const uint32_t R = (uint32_t)(sh4der_sat_(r) * 255.0f + 0.5f), G = (uint32_t)(sh4der_sat_(g) * 255.0f + 0.5f),
                   B = (uint32_t)(sh4der_sat_(b) * 255.0f + 0.5f);
    return 0xff000000u | R << 16 | G << 8 | B;
}

/* A band of a strip between radii ua (inner) and ub (outer): I0 O0 I1 O1 .. IS OS, S + 1 spokes wrap to the first, so the
   triangles (I_j, O_j, I_j+1) and (O_j, I_j+1, O_j+1) close the ring. The strip's last vertex carries eol. The inner radius of
   the boost's first band is 0: its vertices all sit on the centre (degenerate triangles, no area). */
static unsigned band(sh4der_torch_vert_t *out, float cx, float cy, float rx, float ry, float ua, float ub, band_kind_t kind, float k, float tint_g, float tint_b, float hot_u) {
    unsigned n = 0, j;
    for (j = 0; j <= SH4DER_TORCH_SPOKES; j++) {
        unsigned s;
        for (s = 0; s < 2; s++) {
            const float u = s ? ub : ua;
            sh4der_torch_vert_t *v = &out[n++];
            float r, g, b;
            v->x = cx + rx * u * unit_c[j];
            v->y = cy + ry * u * unit_s[j];
            v->eol = 0;
            if (kind == BAND_BOOST) {
                const float c = sh4der_torch_boost_at_u(u, k, hot_u);
                r = c;
                g = c * tint_g;
                b = c * tint_b;
            } else {
                r = g = b = sh4der_torch_carve_at(u, k);
            }
            v->argb = argb_of(r, g, b);
        }
    }
    out[n - 1].eol = 1;
    return n;
}

/* Boost mesh with the hot spot at SH4DER_TORCH_HOT_U and the given green and blue tint. Writes into out[] and returns the
   vertex count. Maths only, so any list. */
unsigned sh4der_torch_boost_mesh_tint(sh4der_torch_vert_t out[SH4DER_TORCH_VERTS], float cx, float cy, float rx, float ry, float power, float tint_g, float tint_b) {
    return sh4der_torch_boost_mesh_core(out, cx, cy, rx, ry, power, tint_g, tint_b, SH4DER_TORCH_HOT_U);
}

/* Boost mesh: SH4DER_TORCH_RINGS bands from the centre out, each band written by band(). hot_u is passed to
   sh4der_torch_boost_at_u as the hot spot position. Writes into out[] and returns the vertex count. Maths only, so any list. */
unsigned sh4der_torch_boost_mesh_core(sh4der_torch_vert_t out[SH4DER_TORCH_VERTS], float cx, float cy, float rx, float ry, float power, float tint_g, float tint_b, float hot_u) {
    unsigned n = 0, k;
    for (k = 0; k < SH4DER_TORCH_RINGS; k++) {
        const float ua = (float)k * (1.0f / (float)SH4DER_TORCH_RINGS), ub = (float)(k + 1) * (1.0f / (float)SH4DER_TORCH_RINGS);
        n += band(&out[n], cx, cy, rx, ry, ua, ub, BAND_BOOST, power, tint_g, tint_b, hot_u);
    }
    return n;
}

/* Boost mesh with the default tint (SH4DER_TORCH_TINT_G and SH4DER_TORCH_TINT_B). Maths only. */
unsigned sh4der_torch_boost_mesh(sh4der_torch_vert_t out[SH4DER_TORCH_VERTS], float cx, float cy, float rx, float ry, float power) {
    return sh4der_torch_boost_mesh_tint(out, cx, cy, rx, ry, power, SH4DER_TORCH_TINT_G, SH4DER_TORCH_TINT_B);
}

/* Carve mesh out to SH4DER_TORCH_UMAX, the grey (equal r, g, b) bands of carve_mesh_to. Maths only. */
unsigned sh4der_torch_carve_mesh(sh4der_torch_vert_t out[SH4DER_TORCH_VERTS], float cx, float cy, float rx, float ry, float k) {
    return sh4der_torch_carve_mesh_to(out, cx, cy, rx, ry, k, SH4DER_TORCH_UMAX);
}

/* Carve mesh from the feather start out to umax, in SH4DER_TORCH_BANDS bands. Writes into out[] and returns the vertex
   count. Maths only, so any list. */
unsigned sh4der_torch_carve_mesh_to(sh4der_torch_vert_t out[SH4DER_TORCH_VERTS], float cx, float cy, float rx, float ry, float k, float umax) {
    /* feather start, edge, then uniform out to umax: BANDS bands */
    float cu[SH4DER_TORCH_BANDS + 1];
    unsigned n = 0, i;
    cu[0] = SH4DER_TORCH_EDGE_IN;
    for (i = 1; i <= SH4DER_TORCH_BANDS; i++)
        cu[i] = SH4DER_TORCH_EDGE_OUT + (float)(i - 1) * (umax - SH4DER_TORCH_EDGE_OUT) * (1.0f / (float)(SH4DER_TORCH_BANDS - 1));
    for (i = 0; i < SH4DER_TORCH_BANDS; i++)
        n += band(&out[n], cx, cy, rx, ry, cu[i], cu[i + 1], BAND_CARVE, k, 1.0f, 1.0f, 0.0f);
    return n;
}

#ifdef _arch_dreamcast
/* KOS: the two headers (TR list, blend per layer as the look kinds in sh4der_look.h) and the strip draw. Only built for the
   Dreamcast. The header goes in by pvr_prim, the vertices by direct render. */
#include <dc/pvr.h>

/* hdr[0]: carve, MULTIPLY (src ZERO, dst DESTCOLOR); hdr[1]: boost, BOOST (src DESTCOLOR, dst ONE). Fog off, no depth write. */
void sh4der_torch_headers(pvr_poly_hdr_t hdr[2]) {
    static const int src_of[2] = { PVR_BLEND_ZERO, PVR_BLEND_DESTCOLOR };
    static const int dst_of[2] = { PVR_BLEND_DESTCOLOR, PVR_BLEND_ONE };
    int k;
    for (k = 0; k < 2; k++) {
        pvr_poly_cxt_t c;
        pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
        c.gen.culling = PVR_CULLING_NONE;
        c.gen.fog_type = PVR_FOG_DISABLE;
        c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        c.depth.write = PVR_DEPTHWRITE_DISABLE;
        c.blend.src = src_of[k];
        c.blend.dst = dst_of[k];
        pvr_poly_compile(&hdr[k], &c);
    }
}

/* Draw one mesh on the TR list: each band a strip (EOL on its last vertex), one header for the layer. hdr is one of the two
   from sh4der_torch_headers. z is the layer's depth in the TR sort. n = 0 draws nothing. */
void sh4der_torch_draw(const pvr_poly_hdr_t *hdr, const sh4der_torch_vert_t *v, unsigned n, float z) {
    unsigned i;
    if (!n)
        return;
    pvr_prim((void *)hdr, sizeof(*hdr));
    for (i = 0; i < n; i++) {   /* KOS direct render: each vertex written straight into the store queue (no copy through pvr_prim) */
        pvr_vertex_t *p = pvr_dr_target();
        p->flags = v[i].eol ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        p->x = v[i].x;
        p->y = v[i].y;
        p->z = z;
        p->u = p->v = 0.0f;
        p->argb = v[i].argb;
        p->oargb = 0;
        pvr_dr_commit(p);
    }
}
#endif
