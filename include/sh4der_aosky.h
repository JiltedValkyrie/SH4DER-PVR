/* SH4DER-PVR: occlusion of the sky drawn into a shadow target (the PVR's map pass, AO method 2)

   include/sh4der_aosky.h
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).

   RECOMMENDED (from the wiki ledger, one console; wiki/limits/header-recommended.md)
   Start with: SH4DER_AOSKY_DIRS_MAX 8 (the most directions a call may take; sh4der_aosky_count rounds k to 0, 1, 2, 4, 8);
               k 4 directions, one side, plain blending (sh4der_aosky_header(h, 0)): the start the ao4 scenes ran
               (PVR-140); SH4DER_AOSKY_FLOOR 0.5f (what is left of the ground's light with every direction blocked)
   Cost:       PVR, one sky direction in a 256 x 256 target: 451 us (scene A), 347 us (scene B) of PVR (PVR-140: DERIVED
               from the MEASURED target passes). One side, 1,164 caster triangles: 753 us a direction (PVR-130, DERIVED)
   SH-4:       the send 651 to 852 ns a vertex in the ao4 scenes (SH4-078, MEASURED; the per-vertex figure DERIVED);
               ao0 measured 522 ns a vertex, 1,824 us for 1,164 triangles, both sides (SH4-071)
   Frame:      the town at K 4 added +10.0 ms of frame (PVR-135, MEASURED; ao1, not the ao4 scenes)
   Status:     PROVEN for the start above: the ao4 scenes ran it (PVR-138, PVR-152, PVR-153, PVR-176, PVR-186, ao4-d to
               ao4-j, one console). The depth union (sh4der_aosky_header(h, 1)) FAILED on the PVR (PVR-127, MEASURED, cause
               OPEN): not the start. This copy of the code is ADVISED until one console run of it: the host hashes of the
               ao4 scenes match the code those rows measured (test/aosky_test.c, the ao4 scene hashes in the move notes)
   Do:         draw one side (PVR-128: the same darkening as both sides for half the vertices; SH4-072: half the sending,
               the same work), and cap k at what the frame can pay (about 0.35 to 0.45 ms of PVR a direction, DERIVED)
   Do not:     use the depth union (PVR-127); send a warped map (not handled: a vertex with w <= 0 drops its triangle,
               nothing is clipped); draw this over a receiver that is not the flat ground
   Maturity:   PROTOTYPE v0.1 (proposed: maturity.md has no row for this method yet)

   The sun's casters are drawn once into the shadow target (the sun's grey). This header draws the same casters K more
   times, each time as another sky direction sees them, each time as a faint black. Where the shadows pile up the
   ground gets darker: occlusion of the sky in K steps. No new target and no new pass: the ground layer that carries
   the sun's shadow carries this too.

   THE SLIDE. On a flat ground at ground_y, a point p seen from sky direction d is p slid along -d onto the ground:
   x' = x - sx (y - y0), z' = z - sz (y - y0), y' = y0, with (sx, sz) the slope. That is one matrix (sh4der_aosky_slide),
   and "slide, then the map" is one matrix (sh4der_aosky_slide_map): a direction costs one transform a caster vertex.
   THE DIRECTIONS. sh4der_aosky_slope(k, i, s): k azimuths round the sky at 60 degrees of elevation (cot = COT_EL), the
   first at 22.5 degrees, so that no direction runs along a wall of an axis-aligned town.
   THE GREY. Each direction multiplies what is in the target by 1 - sh4der_aosky_alpha(k): k directions all blocked leave
   SH4DER_AOSKY_FLOOR of the ground's light (sh4der_aosky_level gives the model's level after n blocks).
   ONE SIDE. A closed mesh covers every texel of its shadow an even number of times, once by a face toward the direction
   and once by a face away from it. Sending only the triangles of one sign of area (cull 1 or -1) draws the same shadow
   with half the vertices. It needs every caster closed and wound one way; a caster that is not closed sets its open
   flag and is drawn on both sides. cull 0 sends both sides.
   THE DEPTH UNION (sh4der_aosky_header(h, 1), depth "greater" with write, depth sh4der_aosky_depth(i)) was meant to give
   one darkening a direction even where two triangles overlap. On the PVR it does not (PVR-127). It stays for a client that
   wants to test it; ao4 and the town use unite 0 (plain blending).
   Limits: the receiver is the flat ground only (no walls, no movers, no self-shadowing). The map is uniform (w is 1 in
   the target); a vertex behind a warped map's singularity (w <= 0) drops its triangle. Nothing is clipped.

   How it differs from the other occlusion code:
     sh4der_aobake.h    a per-vertex bake at load: rays at exact spheres, boxes and a plane; one byte a vertex that the
                        light multiplies. No target, no draw, nothing a frame.
     sh4der_ao.h        crease darkening from the angle between faces, and a screen cavity cue. No rays, no target.
     sh4der_psm_pvr.h   the sun's shadow target: the casters' triangles for the target and the ground's lookup
                        coordinates. This header draws its sky directions INTO THAT SAME target, after the sun's triangles.

   Call order:
     1. At load (under KallistiOS): sh4der_aosky_header(&h, 0) for the directions' translucent header.
     2. Each frame, in the map pass after the sun's casters: for d < k, sh4der_aosky_slope(k, d, s) with the reach
        you want, then sh4der_aosky_dir(&casters, map, s, ground_y, size, z, sh4der_aosky_argb(k, strength), cull, 1,
        xy, ok, emit, user). z is sh4der_aosky_depth(d) for the union, any depth for unite 0.
     3. The caller's xy (float[vn][2]) and ok (uint8_t[vn]) are its own scratch: no file-scope statics in this header.
     4. sh4der_aosky_dir leaves its matrix in XMTRX on the Dreamcast. Load your own matrix again before you transform
        anything else.

   Maths: SH4ZAM on the Dreamcast (the matrix row load and the transform of sh4der_aosky_dir); plain C under
   SH4DER_CORE_NO_SH4ZAM, which is the same loop without the matrix registers. Both paths are in test/aosky_test.c.
   Header only, all static inline. The caller owns every array.

   Version: v0.1 (the maths of demo/psm/ao_layer.h, the sender of demo/psm/ao/ao4_draw.h, moved unchanged).
*/

/** \file    sh4der_aosky.h
    \brief   Ambient occlusion of the sky, drawn as K slid shadows into the sun's shadow target.

    The maths is a row-form matrix per direction (h = (p, 1) x M, element [row * 4 + column], as sh4der_psm_t.m) and the
    target pixels of each triangle. A direction's triangles are handed to the caller's emit callback; the caller draws
    them into its target with its own list and blend.

    The tuning macros are the start values of the map pass (README "Aosky tuning"). */

#ifndef SH4DER_AOSKY_H
#define SH4DER_AOSKY_H

#include <stdint.h>
#include "sh4der_core.h"

#define SH4DER_AOSKY_DIRS_MAX 8u            /* the most sky directions a call may take: the ring has eight */
#define SH4DER_AOSKY_COT_EL   0.57735027f   /* cot(60 degrees): metres of ground a metre of height reaches (a constant) */
#define SH4DER_AOSKY_FLOOR    0.5f          /* what is left of the ground's light with every direction blocked */
#define SH4DER_AOSKY_Z0       1.25f         /* direction 0's depth in the map pass; the sun's casters 1.0, the border 2.0 */
#define SH4DER_AOSKY_ZSTEP    0.0625f       /* a direction's step nearer (exact in a float) */
#define SH4DER_AOSKY_XY_LIMIT 2048.0f       /* a triangle with a corner farther than this from the target is left out */
#define SH4DER_AOSKY_AREA_MIN 1.0e-3f       /* target pixels squared: under this a triangle is not sent */

/* The caller's casters: closed shapes wound one way, in world space (the sun's casters, the same arrays). */
typedef struct sh4der_aosky_casters {
    const float (*v)[3]; unsigned vn;       /* vertices */
    const uint16_t (*t)[3]; unsigned tn;    /* triangles, indices into v */
    const uint8_t *open;                    /* NULL, or 1 a triangle whose caster is not closed: both its sides are drawn */
} sh4der_aosky_casters_t;

/* One triangle in target pixels at depth z, with its vertex colour and the caller's blend. */
typedef void (*sh4der_aosky_emit_t)(void *user, const float a[2], const float b[2], const float c[2], float z, uint32_t argb, int blend);

/* The directions used for a request of k: 0, 1, 2, 4 or 8 (another k rounds down). */
static inline unsigned sh4der_aosky_count(unsigned k) { return k >= 8u ? 8u : k >= 4u ? 4u : k >= 2u ? 2u : k; }

/* (cos, sin) of 22.5 + 45 i degrees */
static const float sh4der_aosky_ring_[SH4DER_AOSKY_DIRS_MAX][2] = {
    {  0.92387953f,  0.38268343f }, {  0.38268343f,  0.92387953f }, { -0.38268343f,  0.92387953f }, { -0.92387953f,  0.38268343f },
    { -0.92387953f, -0.38268343f }, { -0.38268343f, -0.92387953f }, {  0.38268343f, -0.92387953f }, {  0.92387953f, -0.38268343f }
};

/* Direction i of k (k from sh4der_aosky_count, i < k): the slide's slope (sx, sz), ground metres a metre of height. The
   sky direction itself is (sx, 1, sz) normalised. */
static inline void sh4der_aosky_slope(unsigned k, unsigned i, float s[2]) {
    const unsigned j = k ? (i * (SH4DER_AOSKY_DIRS_MAX / k)) & (SH4DER_AOSKY_DIRS_MAX - 1u) : 0u;

    s[0] = SH4DER_AOSKY_COT_EL * sh4der_aosky_ring_[j][0];
    s[1] = SH4DER_AOSKY_COT_EL * sh4der_aosky_ring_[j][1];
}

/* 1 - FLOOR ^ (1 / k): the alpha of a direction's black when k directions share the darkening */
static inline float sh4der_aosky_alpha(unsigned k) {
    static const float a[SH4DER_AOSKY_DIRS_MAX + 1u] = { 0.0f, 0.5f, 0.29289322f, 0.20629947f, 0.15910358f, 0.12944944f, 0.10910129f, 0.09427634f, 0.08299596f };

    return a[k > SH4DER_AOSKY_DIRS_MAX ? SH4DER_AOSKY_DIRS_MAX : k];
}

/* The vertex colour of a direction's triangles: black, alpha = alpha(k) x strength (0 .. 1). */
static inline uint32_t sh4der_aosky_argb(unsigned k, float strength) {
    const float a = sh4der_aosky_alpha(k) * (strength < 0.0f ? 0.0f : strength > 1.0f ? 1.0f : strength);

    return (uint32_t)(a * 255.0f + 0.5f) << 24;
}

/* What a target texel holds after n of the k directions covered it, as a share of what was there (the model the ao0
   image's level count is read against): the PVR's blend at 8 bits a channel is not modelled. */
static inline float sh4der_aosky_level(unsigned k, unsigned n) {
    float l = 1.0f;
    const float keep = 1.0f - sh4der_aosky_alpha(k);

    while (n--) l *= keep;
    return l;
}

static inline float sh4der_aosky_depth(unsigned i) { return SH4DER_AOSKY_Z0 + SH4DER_AOSKY_ZSTEP * (float)i; }

/* The slide as a row-form matrix (h = (p, 1) x S, element [row * 4 + column]). */
static inline void sh4der_aosky_slide(const float s[2], float ground_y, float S[16]) {
    unsigned i;

    for (i = 0; i < 16u; i++) S[i] = 0.0f;
    S[0] = 1.0f; S[10] = 1.0f; S[15] = 1.0f;
    S[4] = -s[0]; S[6] = -s[1];                               /* y's row: what a metre of height moves x and z */
    S[12] = s[0] * ground_y; S[13] = ground_y; S[14] = s[1] * ground_y;
}

/* out = S x map: a point slid, then through the map. out may not be map. */
static inline void sh4der_aosky_slide_map(const float map[16], const float s[2], float ground_y, float out[16]) {
    float S[16];
    unsigned r, c;

    sh4der_aosky_slide(s, ground_y, S);
    for (r = 0; r < 4u; r++)
        for (c = 0; c < 4u; c++)
            out[r * 4u + c] = S[r * 4u] * map[c] + S[r * 4u + 1u] * map[4u + c] + S[r * 4u + 2u] * map[8u + c] + S[r * 4u + 3u] * map[12u + c];
}

/* A homogeneous map point as target pixels ((X / W + 1) x size / 2, the PVR map's rule). 0 when W is not in front of the
   map's singularity or the point is farther than SH4DER_AOSKY_XY_LIMIT from the target. `inv_w` is 1 / h[3], the caller's
   (a uniform map: 1). */
static inline int sh4der_aosky_px(const float h[4], float inv_w, float half, float xy[2]) {
    if (!(h[3] > 1.0e-6f)) return 0;
    xy[0] = (h[0] * inv_w + 1.0f) * half;
    xy[1] = (h[1] * inv_w + 1.0f) * half;
    return xy[0] > -SH4DER_AOSKY_XY_LIMIT && xy[0] < SH4DER_AOSKY_XY_LIMIT + 2.0f * half &&
           xy[1] > -SH4DER_AOSKY_XY_LIMIT && xy[1] < SH4DER_AOSKY_XY_LIMIT + 2.0f * half;
}

/* The side of a triangle worth sending: 0 when it has no area or its box misses the target (0 .. size), else 1 or -1,
   the sign of its area in the target. */
static inline int sh4der_aosky_tri_side(const float a[2], const float b[2], const float c[2], float size) {
    const float area = (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]);

    if (!(area > SH4DER_AOSKY_AREA_MIN || area < -SH4DER_AOSKY_AREA_MIN)) return 0;
    if ((a[0] < 0.0f && b[0] < 0.0f && c[0] < 0.0f) || (a[0] > size && b[0] > size && c[0] > size)) return 0;
    if ((a[1] < 0.0f && b[1] < 0.0f && c[1] < 0.0f) || (a[1] > size && b[1] > size && c[1] > size)) return 0;
    return area > 0.0f ? 1 : -1;
}

/* 1 when the triangle is sent under `cull`: 0 both sides; 1 or -1 that side only (see ONE SIDE above). */
static inline int sh4der_aosky_tri_ok(const float a[2], const float b[2], const float c[2], float size, int cull) {
    const int side = sh4der_aosky_tri_side(a, b, c, size);

    return side != 0 && (cull == 0 || side == cull);
}

/* The casters as the directions see them, into a target of `size` pixels (a square), for a uniform map (w is 1). For
   direction slope (sx, sz) over ground_y and a row-form map: the caster vertices slid and mapped, one transform a vertex,
   then each triangle whose three corners are in front and whose side is `cull` (0: both; the open flag overrides it to
   0) goes to emit. Returns the triangles emitted. The caller's xy (float[vn][2]) and ok (uint8_t[vn]) hold the vertices
   of this call (any scratch of at least vn). Leaves its matrix in XMTRX on the Dreamcast (see the header's call order). */
static inline unsigned sh4der_aosky_dir(const sh4der_aosky_casters_t *c, const float map[16], const float slope[2], float ground_y,
                                        unsigned size, float z, uint32_t argb, int cull, int blend,
                                        float (*xy)[2], uint8_t *ok, sh4der_aosky_emit_t emit, void *user) {
    const float half = 0.5f * (float)size;
    float m[16];
    unsigned i, n = 0;

    sh4der_aosky_slide_map(map, slope, ground_y, m);
#ifndef SH4DER_CORE_NO_SH4ZAM
    {
        const shz_vec4_t r0 = shz_vec4_init(m[0], m[4], m[8], m[12]), r1 = shz_vec4_init(m[1], m[5], m[9], m[13]);
        const shz_vec4_t r2 = shz_vec4_init(m[2], m[6], m[10], m[14]), r3 = shz_vec4_init(m[3], m[7], m[11], m[15]);

        shz_xmtrx_load_rows_4x4(&r0, &r1, &r2, &r3);
    }
#endif
    for (i = 0; i < c->vn; i++) {
        float h[4];
#ifndef SH4DER_CORE_NO_SH4ZAM
        const shz_vec4_t v = shz_xmtrx_transform_vec4(shz_vec4_init(c->v[i][0], c->v[i][1], c->v[i][2], 1.0f));

        h[0] = v.x; h[1] = v.y; h[2] = v.z; h[3] = v.w;
#else
        unsigned k;

        for (k = 0; k < 4u; k++) h[k] = c->v[i][0] * m[k] + c->v[i][1] * m[4u + k] + c->v[i][2] * m[8u + k] + m[12u + k];
#endif
        ok[i] = (uint8_t)sh4der_aosky_px(h, 1.0f, half, xy[i]);   /* a uniform map: w is 1 */
    }
    for (i = 0; i < c->tn; i++) {
        const unsigned a = c->t[i][0], b = c->t[i][1], d = c->t[i][2];

        if (!(ok[a] & ok[b] & ok[d])) continue;
        if (!sh4der_aosky_tri_ok(xy[a], xy[b], xy[d], (float)size, c->open && c->open[i] ? 0 : cull)) continue;
        emit(user, xy[a], xy[b], xy[d], z, argb, blend);
        n++;
    }
    return n;
}

#ifdef _arch_dreamcast
/* The header of the directions' triangles, for the translucent list of the map pass: untextured, no culling, the black
   blended by its alpha. unite 1: depth "greater" and depth write (one shadow a direction; FAILS on the PVR, PVR-127); 0:
   every triangle blends (what ao4 and the town run). */
static inline void sh4der_aosky_header(pvr_poly_hdr_t *h, int unite) {
    pvr_poly_cxt_t c;

    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    c.blend.src = PVR_BLEND_SRCALPHA; c.blend.dst = PVR_BLEND_INVSRCALPHA;
    c.depth.comparison = unite ? PVR_DEPTHCMP_GREATER : PVR_DEPTHCMP_GEQUAL;   /* both fail under the white border */
    c.depth.write = unite ? PVR_DEPTHWRITE_ENABLE : PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(h, &c);
}
#endif

#endif /* SH4DER_AOSKY_H */
