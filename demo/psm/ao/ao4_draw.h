/* SH4DER-PVR: ao4, what goes into the shadow target (demo/psm/ao)

   demo/psm/ao/ao4_draw.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The triangles of the map pass, walked once for the Dreamcast program (which sends them to the tile accelerator) and
   the host preview (which fills them in software): the sun's casters, opaque, in the shadow's grey; then k sky
   directions of the same casters as a blended black (../ao_layer.h, the sender in include/sh4der_aosky.h). Everything is
   drawn ONE SIDE (AO4_CULL): the scenes' casters are closed and wound one way (test/ao4_scene_test.c holds that), so one
   side is the same shadow with half the vertices, and plain blending then darkens once a caster and direction.

   The caller gives `emit`: one call a triangle, in target pixels. Needs ao4_scene.h first. On the Dreamcast the
   transform is one FTRV a vertex (the matrix in XMTRX, as ../psm_scene.h's psm_xf); it leaves that matrix there. */

#ifndef AO4_DRAW_H
#define AO4_DRAW_H

#define AO4_CULL 1                          /* the side that is kept: the sign of a caster triangle's area in the target */

typedef sh4der_aosky_emit_t ao4_emit_t;

/* the ground's place in the target as texture coordinates */
static inline float ao4_ground_u(float x) { return 0.5f * (x * (1.0f / AO4_SPAN)) + 0.5f; }
static inline float ao4_ground_v(float z) { return 0.5f * (z * (1.0f / AO4_SPAN)) + 0.5f; }

/* this demo's scratch for sh4der_aosky_dir (the library keeps none): the ao4 scenes' vertices, at most AO4_CV_MAX */
static float ao4_xy_[AO4_CV_MAX][2];
static uint8_t ao4_ok_[AO4_CV_MAX];

/* the scene's casters as the library's sender reads them */
static inline sh4der_aosky_casters_t ao4_casters_(const ao4_scene_t *s) {
    sh4der_aosky_casters_t c;

    c.v = s->cv; c.vn = s->cvn;
    c.t = s->ct; c.tn = s->ctn;
    c.open = s->ct_open;
    return c;
}

/* The casters slid along `slope` onto the ground and into the target, the triangles of side `cull` (0: both).
   Returns the triangles given to emit. */
static inline unsigned ao4_draw_dir(const ao4_scene_t *s, const float slope[2], float z, uint32_t argb, int cull, int blend, ao4_emit_t emit, void *user) {
    const sh4der_aosky_casters_t c = ao4_casters_(s);

    return sh4der_aosky_dir(&c, ao4_map, slope, 0.0f, AO4_SIZE, z, argb, cull, blend, ao4_xy_, ao4_ok_, emit, user);
}

/* The sun's casters alone: opaque, the shadow's grey, depth 1.0. Nothing for AO ONLY (no sun). Returns triangles. */
static inline unsigned ao4_draw_sun(const ao4_scene_t *s, unsigned look, ao4_emit_t emit, void *user) {
    const uint32_t g = (uint32_t)(sh4der_sat_(ao4_shadow_share(s, look)) * 255.0f + 0.5f);
    const float slope[2] = { s->sun_slope[0], s->sun_slope[1] };

    if (look == AO4_AO_ONLY || s->no_sun_shadow) return 0;
    return ao4_draw_dir(s, slope, 1.0f, 0xff000000u | (g << 16) | (g << 8) | g, AO4_CULL, 0, emit, user);
}

/* The k sky directions (k from sh4der_aosky_count), blended, their slide `reach` times ao_layer.h's (a steeper direction:
   the darkening stays nearer the caster's foot). Returns triangles. */
static inline unsigned ao4_draw_sky(const ao4_scene_t *s, unsigned k, int exaggerate, float reach, ao4_emit_t emit, void *user) {
    const uint32_t argb = ao4_map_argb(k, exaggerate);
    unsigned d, n = 0;

    for (d = 0; d < k; d++) {
        float slope[2];

        sh4der_aosky_slope(k, d, slope);
        slope[0] *= reach; slope[1] *= reach;
        n += ao4_draw_dir(s, slope, sh4der_aosky_depth(d), argb, AO4_CULL, 1, emit, user);
    }
    return n;
}

/* The whole map pass but its white clear and border. Returns triangles. */
static inline unsigned ao4_draw_map(const ao4_scene_t *s, unsigned k, unsigned look, int exaggerate, float reach, ao4_emit_t emit, void *user) {
    const unsigned n = ao4_draw_sun(s, look, emit, user);

    return n + ao4_draw_sky(s, sh4der_aosky_count(k), exaggerate, reach, emit, user);
}

#endif /* AO4_DRAW_H */
