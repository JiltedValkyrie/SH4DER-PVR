/* SH4DER-PVR: ao4, what goes into the shadow target (demo/psm/ao)

   demo/psm/ao/ao4_draw.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The triangles of the map pass, walked once for the Dreamcast program (which sends them to the tile accelerator) and
   the host preview (which fills them in software): the sun's casters, opaque, in the shadow's grey; then k sky
   directions of the same casters as a blended black (../ao_layer.h). Everything is drawn ONE SIDE (AO4_CULL): the
   scenes' casters are closed and wound one way (test/ao4_scene_test.c holds that), so one side is the same shadow with
   half the vertices, and plain blending then darkens once a caster and direction.

   The caller gives `emit`: one call a triangle, in target pixels. Needs ao4_scene.h first. On the Dreamcast the
   transform is one FTRV a vertex (the matrix in XMTRX, as ../psm_scene.h's psm_xf); it leaves that matrix there. */

#ifndef AO4_DRAW_H
#define AO4_DRAW_H

#define AO4_CULL 1                          /* the side that is kept: the sign of a caster triangle's area in the target */

typedef void (*ao4_emit_t)(void *user, const float a[2], const float b[2], const float c[2], float z, uint32_t argb, int blend);

/* the ground's place in the target as texture coordinates */
static inline float ao4_ground_u(float x) { return 0.5f * (x * (1.0f / AO4_SPAN)) + 0.5f; }
static inline float ao4_ground_v(float z) { return 0.5f * (z * (1.0f / AO4_SPAN)) + 0.5f; }

static float ao4_xy_[AO4_CV_MAX][2];
static uint8_t ao4_ok_[AO4_CV_MAX];

/* The casters slid along `slope` onto the ground and into the target, the triangles of side `cull` (0: both).
   Returns the triangles given to emit. */
static inline unsigned ao4_draw_dir(const ao4_scene_t *s, const float slope[2], float z, uint32_t argb, int cull, int blend, ao4_emit_t emit, void *user) {
    const float half = 0.5f * (float)AO4_SIZE;
    float m[16];
    unsigned i, n = 0;

    ao_slide_map(ao4_map, slope, 0.0f, m);
#ifndef SH4DER_CORE_NO_SH4ZAM
    {
        const shz_vec4_t r0 = shz_vec4_init(m[0], m[4], m[8], m[12]), r1 = shz_vec4_init(m[1], m[5], m[9], m[13]);
        const shz_vec4_t r2 = shz_vec4_init(m[2], m[6], m[10], m[14]), r3 = shz_vec4_init(m[3], m[7], m[11], m[15]);

        shz_xmtrx_load_rows_4x4(&r0, &r1, &r2, &r3);
    }
#endif
    for (i = 0; i < s->cvn; i++) {
        float h[4];
#ifndef SH4DER_CORE_NO_SH4ZAM
        const shz_vec4_t v = shz_xmtrx_transform_vec4(shz_vec4_init(s->cv[i][0], s->cv[i][1], s->cv[i][2], 1.0f));

        h[0] = v.x; h[1] = v.y; h[2] = v.z; h[3] = v.w;
#else
        unsigned c;

        for (c = 0; c < 4u; c++) h[c] = s->cv[i][0] * m[c] + s->cv[i][1] * m[4u + c] + s->cv[i][2] * m[8u + c] + m[12u + c];
#endif
        ao4_ok_[i] = (uint8_t)ao_px(h, 1.0f, half, ao4_xy_[i]);   /* a uniform map: w is 1 */
    }
    for (i = 0; i < s->ctn; i++) {
        const unsigned a = s->ct[i][0], b = s->ct[i][1], c = s->ct[i][2];

        if (!(ao4_ok_[a] & ao4_ok_[b] & ao4_ok_[c])) continue;
        if (!ao_tri_ok(ao4_xy_[a], ao4_xy_[b], ao4_xy_[c], (float)AO4_SIZE, s->ct_open[i] ? 0 : cull)) continue;
        emit(user, ao4_xy_[a], ao4_xy_[b], ao4_xy_[c], z, argb, blend);
        n++;
    }
    return n;
}

/* The sun's casters alone: opaque, the shadow's grey, depth 1.0. Nothing for AO ONLY (no sun). Returns triangles. */
static inline unsigned ao4_draw_sun(const ao4_scene_t *s, unsigned look, ao4_emit_t emit, void *user) {
    const uint32_t g = (uint32_t)(sh4der_sat_(ao4_shadow_share(s, look)) * 255.0f + 0.5f);
    const float slope[2] = { s->sun_slope[0], s->sun_slope[1] };

    if (look == AO4_AO_ONLY || s->no_sun_shadow) return 0;
    return ao4_draw_dir(s, slope, 1.0f, 0xff000000u | (g << 16) | (g << 8) | g, AO4_CULL, 0, emit, user);
}

/* The k sky directions (k from ao_count), blended, their slide `reach` times ao_layer.h's (a steeper direction: the
   darkening stays nearer the caster's foot). Returns triangles. */
static inline unsigned ao4_draw_sky(const ao4_scene_t *s, unsigned k, int exaggerate, float reach, ao4_emit_t emit, void *user) {
    const uint32_t argb = ao4_map_argb(k, exaggerate);
    unsigned d, n = 0;

    for (d = 0; d < k; d++) {
        float slope[2];

        ao_slope(k, d, slope);
        slope[0] *= reach; slope[1] *= reach;
        n += ao4_draw_dir(s, slope, ao_depth(d), argb, AO4_CULL, 1, emit, user);
    }
    return n;
}

/* The whole map pass but its white clear and border. Returns triangles. */
static inline unsigned ao4_draw_map(const ao4_scene_t *s, unsigned k, unsigned look, int exaggerate, float reach, ao4_emit_t emit, void *user) {
    const unsigned n = ao4_draw_sun(s, look, emit, user);

    return n + ao4_draw_sky(s, ao_count(k), exaggerate, reach, emit, user);
}

#endif /* AO4_DRAW_H */
