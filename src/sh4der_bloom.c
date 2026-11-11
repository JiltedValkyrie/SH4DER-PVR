/* SH4DER-PVR: the bloom effect

   sh4der_bloom.c
   The effect: puts a soft halo of light round a bright blade, so it seems to
     light the air about it. It does not read the frame: a few feathered
     polygons are added over the picture, 24 triangles.
   Developed for: the glow round the energy sword in Halo: Dreamcast Evolved.
   Use it for: a cheap glow round one bright thing: a blade, a laser bolt, a
     lamp, a muzzle flash, a pick-up.

   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE.
*/

#include "sh4der_bloom.h"

/* cos and sin of the rim angles 2 pi (k + 1/2) / 8: no rim vertex and no flat side on the blade's axis (eight sides read round) */
static const float rim[SH4DER_BLOOM_RIM][2] = {
    { 0.9238795f, 0.3826834f }, { 0.3826834f, 0.9238795f }, { -0.3826834f, 0.9238795f }, { -0.9238795f, 0.3826834f },
    { -0.9238795f, -0.3826834f }, { -0.3826834f, -0.9238795f }, { 0.3826834f, -0.9238795f }, { 0.9238795f, -0.3826834f },
};

#define R0 (SH4DER_BLOOM_ROWS * SH4DER_BLOOM_COLS)   /* the disc's centre vertex */
/* Triangle indices into the blade mesh. The ribbon is ROWS x COLS (3 x 5) vertices, numbered r * COLS + c, so 0..14. Its 2 x 4 cells
   give 16 triangles. Then 8 fan triangles from the disc centre R0 to the rim vertices R0 + 1 .. R0 + 8. Index data only: the caller
   draws with it. */
const uint8_t sh4der_bloom_tris[SH4DER_BLOOM_TRIS][3] = {
    { 0, 1, 5 }, { 1, 6, 5 }, { 1, 2, 6 }, { 2, 7, 6 }, { 2, 3, 7 }, { 3, 8, 7 }, { 3, 4, 8 }, { 4, 9, 8 },
    { 5, 6, 10 }, { 6, 11, 10 }, { 6, 7, 11 }, { 7, 12, 11 }, { 7, 8, 12 }, { 8, 13, 12 }, { 8, 9, 13 }, { 9, 14, 13 },
    { R0, R0 + 1, R0 + 2 }, { R0, R0 + 2, R0 + 3 }, { R0, R0 + 3, R0 + 4 }, { R0, R0 + 4, R0 + 5 }, { R0, R0 + 5, R0 + 6 }, { R0, R0 + 6, R0 + 7 },
    { R0, R0 + 7, R0 + 8 }, { R0, R0 + 8, R0 + 1 },
};

/* Fills p with the default bloom look from SH4DER_BLOOM_* in the header. Maths only: no PVR calls. */
void sh4der_bloom_params_default(sh4der_bloom_params_t *p) {
    p->x0 = SH4DER_BLOOM_X0;
    p->x1 = SH4DER_BLOOM_X1;
    p->tail = SH4DER_BLOOM_TAIL;
    p->half_width = SH4DER_BLOOM_HALF_WIDTH;
    p->inner = SH4DER_BLOOM_INNER;
    p->root = SH4DER_BLOOM_ROOT;
    p->tip = SH4DER_BLOOM_TIP;
    p->tube_floor = SH4DER_BLOOM_TUBE_FLOOR;
    p->boost = SH4DER_BLOOM_BOOST;
    p->boost_reach = SH4DER_BLOOM_BOOST_REACH;
    p->disc_radius = SH4DER_BLOOM_DISC_RADIUS;
    p->disc_peak = SH4DER_BLOOM_DISC_PEAK;
    p->flare_floor = SH4DER_BLOOM_FLARE_FLOOR;
    p->flare_cos = SH4DER_BLOOM_FLARE_COS;
}

/* Fills p as sh4der_bloom_params_default does, then with the MAX_* values for the tail, width, inner, tip, boost, disc and flare floor.
   x0, x1, root, tube_floor and flare_cos keep their default values. */
void sh4der_bloom_params_max(sh4der_bloom_params_t *p) {
    sh4der_bloom_params_default(p);
    p->tail = SH4DER_BLOOM_MAX_TAIL;
    p->half_width = SH4DER_BLOOM_MAX_HALF_WIDTH;
    p->inner = SH4DER_BLOOM_MAX_INNER;
    p->tip = SH4DER_BLOOM_MAX_TIP;
    p->boost = SH4DER_BLOOM_MAX_BOOST;
    p->boost_reach = SH4DER_BLOOM_MAX_BOOST_REACH;
    p->disc_radius = SH4DER_BLOOM_MAX_DISC_RADIUS;
    p->disc_peak = SH4DER_BLOOM_MAX_DISC_PEAK;
    p->flare_floor = SH4DER_BLOOM_MAX_FLARE_FLOOR;
}

/* finite and above 0 */
static int positive(float x) {
    return sh4der_finite_(x) && x > 0.0f;
}

static int level_ok(float x) {   /* a level may pass 1 (the colour clamps): 0..4 */
    return sh4der_finite_(x) && x >= 0.0f && x <= 4.0f;
}

/* Returns 1 when every bloom parameter is usable: x1 > x0, tail and half_width above 0, inner from 0 up to below half_width, root, tip,
   boost and disc_peak in 0..4, tube_floor and flare_floor in 0..1, boost_reach and disc_radius above 0, and flare_cos in -1 (inclusive)
   to 1 (exclusive). Otherwise returns 0. */
int sh4der_bloom_params_valid(const sh4der_bloom_params_t *p) {
    return p && sh4der_finite_(p->x0) && sh4der_finite_(p->x1) && p->x1 > p->x0 && positive(p->tail) && positive(p->half_width) &&
           sh4der_finite_(p->inner) && p->inner >= 0.0f && p->inner < p->half_width && level_ok(p->root) && level_ok(p->tip) &&
           sh4der_unit_(p->tube_floor) && level_ok(p->boost) && positive(p->boost_reach) && positive(p->disc_radius) && level_ok(p->disc_peak) &&
           sh4der_unit_(p->flare_floor) && sh4der_finite_(p->flare_cos) && p->flare_cos >= -1.0f && p->flare_cos < 1.0f;
}

/* (0, -v.z, v.y) unit: across the axis x and facing v; any unit y-z vector when v lies on the axis. */
static void across(const float v[3], float w[3]) {
    w[0] = 0.0f;
    w[1] = -v[2];
    w[2] = v[1];
    if (sh4der_len2_(w[1], w[2]) <= 1e-12f) {
        w[1] = 1.0f;
        w[2] = 0.0f;
    }
    sh4der_norm3_(w);
}

/* A disc of the given radius at centre, facing eye. Vertex 0 is the centre at level peak; the SH4DER_BLOOM_RIM rim vertices 1 .. SH4DER_BLOOM_RIM
   are at level 0. The disc lies in the plane across the view direction, so it faces the eye. A view straight down the x axis uses a
   fixed across vector. Writes xyz and level only: the caller draws them through sh4der_bloom_argb. Maths only. */
void sh4der_bloom_disc(float xyz[][3], float level[], const float centre[3], float radius, const float eye[3], float peak) {
    float v[3] = { eye[0] - centre[0], eye[1] - centre[1], eye[2] - centre[2] }, w[3], u[3];
    int k, j;

    sh4der_norm3_(v);
    if (v[0] > 0.999f || v[0] < -0.999f) {   /* looking down the axis: any across vector */
        w[0] = 0.0f; w[1] = 1.0f; w[2] = 0.0f;
    } else {
        across(v, w);
    }
    sh4der_cross_(w, v, u);
    sh4der_norm3_(u);
    for (j = 0; j < 3; j++)
        xyz[0][j] = centre[j];
    level[0] = peak;
    for (k = 0; k < SH4DER_BLOOM_RIM; k++) {
        for (j = 0; j < 3; j++)
            xyz[1 + k][j] = centre[j] + radius * (rim[k][0] * w[j] + rim[k][1] * u[j]);
        level[1 + k] = 0.0f;
    }
}

/* As sh4der_bloom_disc, with rim_n rim vertices (1 .. rim_n) spaced evenly: the k-th at angle 2 pi (k + 1/2) / rim_n. rim_n below 3
   writes nothing. Above SH4DER_BLOOM_DISC_MAX it is clamped. xyz and level need 1 + rim_n entries. Maths only. */
void sh4der_bloom_disc_n(float xyz[][3], float level[], const float centre[3], float radius, const float eye[3], float peak, unsigned rim_n) {
    float v[3] = { eye[0] - centre[0], eye[1] - centre[1], eye[2] - centre[2] }, w[3], u[3];
    unsigned k;
    int j;

    if (rim_n < 3)   /* no disc: nothing is written (the caller sized its arrays for the rim it asked for) */
        return;
    if (rim_n > SH4DER_BLOOM_DISC_MAX)
        rim_n = SH4DER_BLOOM_DISC_MAX;
    sh4der_norm3_(v);
    if (v[0] > 0.999f || v[0] < -0.999f) {
        w[0] = 0.0f; w[1] = 1.0f; w[2] = 0.0f;
    } else {
        across(v, w);
    }
    sh4der_cross_(w, v, u);
    sh4der_norm3_(u);
    for (j = 0; j < 3; j++)
        xyz[0][j] = centre[j];
    level[0] = peak;
    for (k = 0; k < rim_n; k++) {
        float s, c;

        sh4der_sincos_(6.2831853f * ((float)k + 0.5f) * sh4der_recip_pos_((float)rim_n), &s, &c);
        for (j = 0; j < 3; j++)
            xyz[1 + k][j] = centre[j] + radius * (c * w[j] + s * u[j]);
        level[1 + k] = 0.0f;
    }
}

/* A feathered disc with rim_n rim vertices. Vertex 0 is the centre at level peak. Vertices 1 .. rim_n form an inner ring at level peak,
   at radius (1 - feather) x radius. Vertices rim_n + 1 .. 2 rim_n form the outer ring at radius, at level 0. feather is clamped to
   0..1 (NaN reads as 1, which puts the inner ring at the centre). rim_n below 3 writes nothing; above SH4DER_BLOOM_DISC_MAX it is
   clamped. xyz and level need 1 + 2 rim_n entries. Maths only. Index it with sh4der_bloom_disc_feather_tris. */
void sh4der_bloom_disc_feather(float xyz[][3], float level[], const float centre[3], float radius, float feather, const float eye[3], float peak,
                               unsigned rim_n) {
    float outer[1 + SH4DER_BLOOM_DISC_MAX][3], lv[1 + SH4DER_BLOOM_DISC_MAX];
    const float in = 1.0f - (sh4der_finite_(feather) ? sh4der_sat_(feather) : 1.0f);
    unsigned k;
    int j;

    if (rim_n < 3)   /* no disc: nothing is written (the caller sized its arrays for the rim it asked for) */
        return;
    if (rim_n > SH4DER_BLOOM_DISC_MAX)
        rim_n = SH4DER_BLOOM_DISC_MAX;
    sh4der_bloom_disc_n(outer, lv, centre, radius, eye, peak, rim_n);
    for (j = 0; j < 3; j++)
        xyz[0][j] = centre[j];
    level[0] = peak;
    for (k = 1; k <= rim_n; k++) {
        for (j = 0; j < 3; j++) {
            xyz[k][j] = sh4der_lerp_(centre[j], outer[k][j], in);   /* the inner ring: full level out to (1 - feather) x radius */
            xyz[rim_n + k][j] = outer[k][j];
        }
        level[k] = peak;
        level[rim_n + k] = 0.0f;
    }
}

/* Triangle indices for the layout of sh4der_bloom_disc_feather: a fan from vertex 0 over the inner ring (rim_n triangles), then a band
   from the inner ring to the outer ring (2 rim_n triangles). Returns the count written, 3 rim_n. rim_n below 3 returns 0. Above
   SH4DER_BLOOM_DISC_MAX it is clamped. Index data only. */
unsigned sh4der_bloom_disc_feather_tris(unsigned rim_n, uint8_t tris[][3]) {
    unsigned k, n = 0;

    if (rim_n < 3)   /* no disc: nothing is written (the caller sized its arrays for the rim it asked for) */
        return 0;
    if (rim_n > SH4DER_BLOOM_DISC_MAX)
        rim_n = SH4DER_BLOOM_DISC_MAX;
    for (k = 0; k < rim_n; k++) {   /* the centre's fan */
        tris[n][0] = 0; tris[n][1] = (uint8_t)(1 + k); tris[n][2] = (uint8_t)(1 + (k + 1) % rim_n);
        n++;
    }
    for (k = 0; k < rim_n; k++) {   /* the band from the inner ring to the outer */
        const uint8_t a = (uint8_t)(1 + k), b = (uint8_t)(1 + (k + 1) % rim_n);

        tris[n][0] = a; tris[n][1] = (uint8_t)(rim_n + a); tris[n][2] = b;
        n++;
        tris[n][0] = b; tris[n][1] = (uint8_t)(rim_n + a); tris[n][2] = (uint8_t)(rim_n + b);
        n++;
    }
    return n;
}

/* The blade's tubes at (y, z): the brightest tube's feathered weight (sh4der_blade_intensity's tube term). */
static float tube_at(const sh4der_blade_frame_t *f, float y, float z) {
    float tube = 0.0f;
    int i;

    for (i = 0; i < f->tube_count; i++) {
        const float d2 = sh4der_len2_(y - f->tube_y[i], z - f->tube_z[i]);

        if (d2 < f->tube_outer2[i]) {
            const float s = f->tube_weight[i] * sh4der_falloff_(d2, f->tube_inner[i], f->tube_inv_feather[i]);

            if (s > tube)
                tube = s;
        }
    }
    return tube;
}

/* The blocker's knot at x along the axis (the brightest blocker). */
static float knot_at(const sh4der_blade_frame_t *f, const sh4der_bloom_params_t *p, float x) {
    float kb = 0.0f;
    int i;

    for (i = 0; i < f->blocker_count; i++) {
        const float dx = x - f->blk_x[i], s = sh4der_feather_(dx * dx, 0.0f, p->boost_reach);

        if (s > kb)
            kb = s;
    }
    return kb;
}

/* The blade's bloom mesh for this frame, written into m. A ribbon of ROWS x COLS vertices runs along x from x0 to x1, and on to
   x1 + tail. Each level comes from the blade's tube profile, its glow and the blocker knot. Then the lens-flare disc sits at x1 (the
   tip), facing eye. f comes from sh4der_blade_prepare for the same blade. eye is the viewer's position in the blade's space. Returns
   1 when written. Returns 0 and zeroes m when p or blade is invalid. Maths only: the caller draws m with sh4der_bloom_argb. */
int sh4der_bloom_blade(sh4der_bloom_mesh_t *m, const sh4der_bloom_params_t *p, const sh4der_blade_params_t *blade,
                       const sh4der_blade_frame_t *f, const float eye[3]) {
    float v[3], w[3], xs[SH4DER_BLOOM_ROWS], along[SH4DER_BLOOM_ROWS], centre[3];
    int r, c, i;

    if (!sh4der_bloom_params_valid(p) || !sh4der_blade_params_valid(blade)) {
        for (i = 0; i < SH4DER_BLOOM_VERTS; i++) {
            m->xyz[i][0] = m->xyz[i][1] = m->xyz[i][2] = 0.0f;
            m->level[i] = 0.0f;
        }
        return 0;
    }
    v[0] = eye[0] - 0.5f * (p->x0 + p->x1);
    v[1] = eye[1] - blade->axis[0];
    v[2] = eye[2] - blade->axis[1];
    across(v, w);
    xs[0] = p->x0; xs[1] = p->x1; xs[2] = p->x1 + p->tail;
    along[0] = p->root; along[1] = p->tip; along[2] = 0.0f;
    for (r = 0, i = 0; r < SH4DER_BLOOM_ROWS; r++) {
        /* the knot rides the ribbon's own along level (0 at the root and the tail): no edge where the ribbon starts or ends */
        const float kb = p->tip > 0.0f ? p->boost * knot_at(f, p, xs[r]) * along[r] * sh4der_recip_pos_(p->tip) : 0.0f;

        for (c = 0; c < SH4DER_BLOOM_COLS; c++, i++) {
            const float off = (float)(c - 2) * 0.5f * p->half_width;   /* -W, -W/2, 0, W/2, W */
            const float y = blade->axis[0] + w[1] * off, z = blade->axis[1] + w[2] * off;
            const float prof = sh4der_feather_(off * off, p->inner, p->half_width);
            const float tube = p->tube_floor + (1.0f - p->tube_floor) * tube_at(f, y, z);

            m->xyz[i][0] = xs[r];
            m->xyz[i][1] = y;
            m->xyz[i][2] = z;
            m->level[i] = prof * f->fade * (f->glow * along[r] * tube + kb);
        }
    }
    centre[0] = p->x1;
    centre[1] = blade->axis[0];
    centre[2] = blade->axis[1];
    v[0] = eye[0] - centre[0];   /* the lens flare: the blade (+x) against the unit way to the eye */
    v[1] = eye[1] - centre[1];
    v[2] = eye[2] - centre[2];
    sh4der_norm3_(v);
    const float flare = sh4der_lerp_(p->flare_floor, 1.0f, sh4dow_smooth01((v[0] - p->flare_cos) * sh4der_recip_pos_(1.0f - p->flare_cos)));
    sh4der_bloom_disc(&m->xyz[R0], &m->level[R0], centre, p->disc_radius, eye,
                      f->fade * flare * (f->glow * p->disc_peak + p->boost * knot_at(f, p, p->x1)));
    return 1;
}

/* ARGB words for all SH4DER_BLOOM_VERTS vertices of m: the ramp between edge and core at each vertex's level, alpha a. out_stride is
   in bytes. No PVR calls: the caller picks the list and blend. */
void sh4der_bloom_argb(const sh4der_bloom_mesh_t *m, const float edge[3], const float core[3], uint8_t a, uint32_t *out, size_t out_stride) {
    uint8_t *o = (uint8_t *)out;
    int i;

    for (i = 0; i < SH4DER_BLOOM_VERTS; i++, o += out_stride) {
        float c[3];

        sh4der_ramp_(edge, core, m->level[i], c);
        *(uint32_t *)o = (uint32_t)a << 24 | (uint32_t)(c[0] * 255.0f + 0.5f) << 16 | (uint32_t)(c[1] * 255.0f + 0.5f) << 8 |
                         (uint32_t)(c[2] * 255.0f + 0.5f);
    }
}
