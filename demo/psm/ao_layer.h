/* SH4DER-PVR: ambient occlusion on the ground from the PVR's own shadow target (demo/psm, AO method 2)

   demo/psm/ao_layer.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   Route A has the PVR draw the casters into a render target as the sun sees them, and lays the target over the ground
   as a multiply. This file draws the casters into THE SAME target K more times, each time as another direction of the
   sky sees them, each time as a faint black. Where those shadows pile up the ground gets darker: occlusion of the sky,
   in K steps. No new target, no new layer on the screen, no new pass; the ground layer that carries the sun's shadow
   carries this too. The method and its costs: AO_METHOD.md.

   THE SLIDE. On a flat ground at height y0 the shadow of a point p from sky direction d is p slid along -d onto the
   ground: x' = x - sx (y - y0), z' = z - sz (y - y0), y' = y0, with (sx, sz) = (dx, dz) / dy. That is a matrix
   (ao_slide), and "slide, then the map" is one matrix (ao_slide_map), so a direction costs one transform a caster
   vertex, like the sun's. The directions (ao_slope) are K azimuths round the sky at 60 degrees of elevation, the first
   at 22.5 degrees so that no direction runs along a wall of an axis-aligned town: a 6 m wall darkens 3.5 m of ground.

   ONE SHADOW A DIRECTION. Two triangles of one direction that overlap must darken the target once, not twice. Every
   triangle of direction i is sent at the one depth ao_depth(i), with the depth test "greater" and depth write on, in a
   translucent list that is drawn in the order sent (pvr_init_params_t.autosort_disabled): the first triangle at a pixel
   writes the depth, the next one of the same direction fails the test there, the next direction is nearer and passes.
   The depths sit between route A's casters (1.0) and its white border (2.0), so the border stays white.
   WHETHER THE PVR DOES THIS IN A RENDER-TO-TEXTURE PASS IS NOT KNOWN (the ao0 image is the test). The other header
   (unite 0) blends every triangle: overlaps darken more than once; rougher, never lighter than the union.

   ONE SIDE. A closed mesh covers every texel of its shadow an even number of times: once by a face toward the
   direction, once by a face away from it. Sending only the triangles of one sign of area in the target (cull 1 or -1)
   draws the same shadow with half the vertices, and on a closed convex caster it is the union with no depth trick. It
   needs every caster closed and wound one way; which sign is "toward" depends on the mesh's winding and the map's
   handedness and is the same for every direction (the slide and a uniform map keep orientation). cull 0 sends both.

   THE GREY. Each direction multiplies what is in the target by 1 - ao_alpha(k): k directions all blocked leave
   AO_FLOOR (0.5) of the ground's light. The whole ground colour is multiplied, the sun's share too.

   Limits: the receiver is the flat ground only (no walls, no movers, no self-shadowing). A vertex behind a warped map's
   singularity (w <= 0) drops its triangle; nothing is clipped, so this is for a uniform map first.

   Parts: the maths (plain C, host tested: ao/test/ao_layer_test.c); ao_layer_header for KallistiOS (_arch_dreamcast);
   psm_ao2_* for the town when psm_send.h is included first (Junior's psm_dc.c calls them).

   Nothing here has run on a console. */

#ifndef PSM_AO_LAYER_H
#define PSM_AO_LAYER_H

#include <stdint.h>

#define AO_DIRS_MAX   8u
#define AO_COT_EL     0.57735027f           /* cot(60 degrees): metres of ground a metre of height reaches */
#define AO_FLOOR      0.5f                  /* what is left of the ground's light with every direction blocked */
#define AO_Z0         1.25f                 /* direction 0's depth in the map pass; route A: casters 1.0, border 2.0 */
#define AO_ZSTEP      0.0625f               /* a direction's step nearer (exact in a float) */
#define AO_XY_LIMIT   2048.0f               /* a triangle with a corner farther than this from the target is left out */
#define AO_AREA_MIN   1.0e-3f               /* target pixels squared: under this a triangle is not sent */

/* The directions used for a request of k: 0, 1, 2, 4 or 8 (another k rounds down). */
static inline unsigned ao_count(unsigned k) { return k >= 8u ? 8u : k >= 4u ? 4u : k >= 2u ? 2u : k; }

/* (cos, sin) of 22.5 + 45 i degrees */
static const float ao_ring_[AO_DIRS_MAX][2] = {
    {  0.92387953f,  0.38268343f }, {  0.38268343f,  0.92387953f }, { -0.38268343f,  0.92387953f }, { -0.92387953f,  0.38268343f },
    { -0.92387953f, -0.38268343f }, { -0.38268343f, -0.92387953f }, {  0.38268343f, -0.92387953f }, {  0.92387953f, -0.38268343f }
};

/* Direction i of k (k from ao_count, i < k): the slide's slope (sx, sz), ground metres a metre of height. The sky
   direction itself is (sx, 1, sz) normalised. */
static inline void ao_slope(unsigned k, unsigned i, float s[2]) {
    const unsigned j = k ? (i * (AO_DIRS_MAX / k)) & (AO_DIRS_MAX - 1u) : 0u;

    s[0] = AO_COT_EL * ao_ring_[j][0];
    s[1] = AO_COT_EL * ao_ring_[j][1];
}

/* 1 - AO_FLOOR ^ (1 / k): the alpha of a direction's black when k directions share the darkening */
static inline float ao_alpha(unsigned k) {
    static const float a[AO_DIRS_MAX + 1u] = { 0.0f, 0.5f, 0.29289322f, 0.20629947f, 0.15910358f, 0.12944944f, 0.10910129f, 0.09427634f, 0.08299596f };

    return a[k > AO_DIRS_MAX ? AO_DIRS_MAX : k];
}

/* The vertex colour of a direction's triangles: black, alpha = ao_alpha(k) x strength (0 .. 1). */
static inline uint32_t ao_argb(unsigned k, float strength) {
    const float a = ao_alpha(k) * (strength < 0.0f ? 0.0f : strength > 1.0f ? 1.0f : strength);

    return (uint32_t)(a * 255.0f + 0.5f) << 24;
}

/* What a target texel holds after n of the k directions covered it, as a share of what was there (the model the ao0
   image's level count is read against): the PVR's blend at 8 bits a channel is not modelled. */
static inline float ao_level(unsigned k, unsigned n) {
    float l = 1.0f;
    const float keep = 1.0f - ao_alpha(k);

    while (n--) l *= keep;
    return l;
}

static inline float ao_depth(unsigned i) { return AO_Z0 + AO_ZSTEP * (float)i; }

/* The slide as a row-form matrix (h = (p, 1) x S, element [row * 4 + column], as psm_xf_t and sh4der_psm_t.m). */
static inline void ao_slide(const float s[2], float ground_y, float S[16]) {
    unsigned i;

    for (i = 0; i < 16u; i++) S[i] = 0.0f;
    S[0] = 1.0f; S[10] = 1.0f; S[15] = 1.0f;
    S[4] = -s[0]; S[6] = -s[1];                               /* y's row: what a metre of height moves x and z */
    S[12] = s[0] * ground_y; S[13] = ground_y; S[14] = s[1] * ground_y;
}

/* out = S x map: a point slid, then through the map. out may not be map. */
static inline void ao_slide_map(const float map[16], const float s[2], float ground_y, float out[16]) {
    float S[16];
    unsigned r, c;

    ao_slide(s, ground_y, S);
    for (r = 0; r < 4u; r++)
        for (c = 0; c < 4u; c++)
            out[r * 4u + c] = S[r * 4u] * map[c] + S[r * 4u + 1u] * map[4u + c] + S[r * 4u + 2u] * map[8u + c] + S[r * 4u + 3u] * map[12u + c];
}

/* A homogeneous map point as target pixels (sh4der_psm_pvr.h's rule: (X / W + 1) x size / 2). 0 when W is not in
   front of the map's singularity or the point is farther than AO_XY_LIMIT from the target. `inv_w` is 1 / h[3], the
   caller's (a uniform map: 1). */
static inline int ao_px(const float h[4], float inv_w, float half, float xy[2]) {
    if (!(h[3] > 1.0e-6f)) return 0;
    xy[0] = (h[0] * inv_w + 1.0f) * half;
    xy[1] = (h[1] * inv_w + 1.0f) * half;
    return xy[0] > -AO_XY_LIMIT && xy[0] < AO_XY_LIMIT + 2.0f * half && xy[1] > -AO_XY_LIMIT && xy[1] < AO_XY_LIMIT + 2.0f * half;
}

/* The side of a triangle worth sending: 0 when it has no area or its box misses the target (0 .. size), else 1 or -1,
   the sign of its area in the target. */
static inline int ao_tri_side(const float a[2], const float b[2], const float c[2], float size) {
    const float area = (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]);

    if (!(area > AO_AREA_MIN || area < -AO_AREA_MIN)) return 0;
    if ((a[0] < 0.0f && b[0] < 0.0f && c[0] < 0.0f) || (a[0] > size && b[0] > size && c[0] > size)) return 0;
    if ((a[1] < 0.0f && b[1] < 0.0f && c[1] < 0.0f) || (a[1] > size && b[1] > size && c[1] > size)) return 0;
    return area > 0.0f ? 1 : -1;
}

/* 1 when the triangle is sent under `cull`: 0 both sides; 1 or -1 that side only (see ONE SIDE above). */
static inline int ao_tri_ok(const float a[2], const float b[2], const float c[2], float size, int cull) {
    const int side = ao_tri_side(a, b, c, size);

    return side != 0 && (cull == 0 || side == cull);
}

#ifdef _arch_dreamcast
/* The header of the directions' triangles, for the translucent list of the map pass: untextured, no culling, the black
   blended by its alpha. unite 1: depth "greater" and depth write (one shadow a direction); 0: every triangle blends. */
static inline void ao_layer_header(pvr_poly_hdr_t *h, int unite) {
    pvr_poly_cxt_t c;

    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    c.blend.src = PVR_BLEND_SRCALPHA; c.blend.dst = PVR_BLEND_INVSRCALPHA;
    c.depth.comparison = unite ? PVR_DEPTHCMP_GREATER : PVR_DEPTHCMP_GEQUAL;   /* both fail under the white border */
    c.depth.write = unite ? PVR_DEPTHWRITE_ENABLE : PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(h, &c);
}
#endif

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

static int psm_ao2_unite __attribute__((unused)) = 1;              /* 1: one shadow a direction (the depth union); 0: plain blending */
static int psm_ao2_cull __attribute__((unused));                   /* 0 both sides; 1 or -1: one sign of area only */
static float psm_ao2_strength = 1.0f;         /* 0 .. 1 of the darkening */
static float psm_ao2_xy_[PSM_CV_MAX][2];
static uint8_t psm_ao2_ok_[PSM_CV_MAX];

#ifdef _arch_dreamcast
static pvr_poly_hdr_t psm_ao2_hdr[2];       /* [0] plain blending, [1] the depth union */

static inline int psm_ao2_init(void) {
    ao_layer_header(&psm_ao2_hdr[0], 0);
    ao_layer_header(&psm_ao2_hdr[1], 1);
    return 1;
}
#endif

/* The casters as k sky directions see them, into the target of `size`: every caster triangle of an object on the
   map's square (f->con), movers and static alike. Returns the vertices sent (3 a triangle). psm_frame_a_casters ran. */
static inline unsigned psm_ao2_send(psm_sink_t *sink, const psm_frame_t *f, const psm_scene_t *s, unsigned size, unsigned k) {
    const unsigned before = sink->verts, n = ao_count(k);
    const float half = 0.5f * (float)size, fsize = (float)size;
    const uint32_t argb = ao_argb(n, psm_ao2_strength);
    unsigned d, i, j;

    if (!n || !f->built || !psm_sink_hdr(sink, PSM_AO2_KIND)) return 0;
    for (d = 0; d < n; d++) {
        const float z = ao_depth(d);
        float slope[2], m[16];
        psm_xf_t xf;

        ao_slope(n, d, slope);
        ao_slide_map(f->map.m, slope, PSM_AO2_GROUND_Y, m);
        psm_xf_load(&xf, m);
        for (i = 0; i < s->cvn; i++) {
            float h[4];

            psm_xf_apply(&xf, s->cv[i], h);
            psm_ao2_ok_[i] = (uint8_t)(h[3] > 1.0e-6f && ao_px(h, sh4der_recip_pos_(h[3]), half, psm_ao2_xy_[i]));
        }
        for (j = 0; j < s->con; j++) {
            if (!f->con[j]) continue;
            for (i = s->co[j].t0; i < s->co[j].t0 + s->co[j].tn; i++) {
                const unsigned a = s->ct[i][0], b = s->ct[i][1], c = s->ct[i][2];

                if (!(psm_ao2_ok_[a] & psm_ao2_ok_[b] & psm_ao2_ok_[c])) continue;
                if (!ao_tri_ok(psm_ao2_xy_[a], psm_ao2_xy_[b], psm_ao2_xy_[c], fsize, psm_ao2_cull)) continue;
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
