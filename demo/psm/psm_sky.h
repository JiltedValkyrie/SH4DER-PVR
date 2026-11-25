/* SH4DER-PVR: the town's air (demo/psm, TOWN_LOOK.md section 3): a textured sky band with a cap, a ring of far hills and a
   treeline, and the quadratic fog tied to DRAW DISTANCE

   demo/psm/psm_sky.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   The band: PSM_SKY_SEGS segments round the eye at PSM_SKY_R, rows at psm_sky_el (-4 to 40 degrees), the texture of
   demo/psm/tools/bake_sky.py (Poly Haven kloofendal_48d_partly_cloudy_puresky, CC0) with u turned so the side of its clouds
   that faces its own sun faces ours. The rows at and under el 0 are warmed toward the sun (x (1, 0.95, 0.84), cos^3 of the
   bearing to it) and fogged per vertex (offset alpha 1 at el 0, 0 at el 6). The cap: plain colour from the band's low-passed
   top row up to the zenith. The hills: two rings of strips, the far hills at PSM_HILL_R and the treeline in front, their
   bases at -3 degrees in the fog colour, so the ground's cut at DRAW DISTANCE meets them in fog.

   All of it is placed through the camera's rows (psm_place_pt_) each frame and sent as strips; a triangle that crosses the
   near plane is cut there (psm_sky_tri_). Nothing here draws: psm_sky_build fills psm_sky_out_t with the vertices in send
   order, which psm_dc.c sends and the host test reads (test/psm_sky_test.c).

   The fog: start 0.43 D, end D, f = ((d - start) / (end - start))^2, written into the PVR's 129 entry table the way
   KallistiOS's pvr_fog_table_custom reads it (psm_fog_table). Its colour: 0.75 x the band's horizon row averaged over the
   view's bearings + 0.25 x the land (0.30, 0.36, 0.26). Needs psm_scene.h before it and the baked psm_sky_tex.h on the path. */

#ifndef PSM_SKY_AIR_H
#define PSM_SKY_AIR_H

#include "psm_sky_tex.h"

#define PSM_SKY_SEGS     32u          /* band and cap segments round 360 degrees */
#define PSM_SKY_ROWS     6u           /* band vertex rows (psm_sky_el) */
#define PSM_SKY_R        1000.0f      /* the band's radius: behind everything (1 / W = 0.001) */
#define PSM_HILL_SEGS    64u
#define PSM_HILL_R       300.0f       /* the far hills */
#define PSM_TREE_R       280.0f       /* the treeline, in front of them */
#define PSM_HILL_BASE    (-3.0f)      /* both rings' bases, degrees: under the ground's cut at -atan(1.7 / D) */
#define PSM_SKY_OUT_MAX  3072u        /* vertices sent at most: every quad cut (band 5 x 32 x 8, cap 32 x 4, hills 2 x 64 x 8) */

static const float psm_sky_el[PSM_SKY_ROWS] = { PSM_SKY_EL0, 0.0f, 6.0f, 16.0f, 28.0f, PSM_SKY_EL1 };
static const float psm_sky_land[3] = { 0.30f, 0.36f, 0.26f };
static const float psm_sky_warm[3] = { 1.0f, 0.95f, 0.84f };

/* a vertex as sent: screen x, y, 1 / W; u, v; colour; offset colour (its alpha is the vertex fog) */
typedef struct psm_skyv {
    float x, y, z, u, v;
    uint32_t argb, oargb;
} psm_skyv_t;

/* a placed vertex: clip X, Y, W and what is interpolated with it (r, g, b, fog in 0 .. 1) */
typedef struct psm_skyp {
    float clip[3], scr[3], u, v, c[4];
} psm_skyp_t;

typedef struct psm_sky_out {
    psm_skyv_t v[PSM_SKY_OUT_MAX];     /* the band, then the cap, then the hills: three runs, three headers */
    uint8_t eol[PSM_SKY_OUT_MAX];
    unsigned n_band, n_cap, n_hill;   /* the cap starts at n_band, the hills at n_band + n_cap */
    unsigned cut;                     /* triangles cut at the near plane this frame */
    float fog[3];                     /* the fog colour this frame */
    float horizon[3];                 /* the band's horizon row averaged over the view's bearings */
    float u_off;                      /* the band's u turn */
} psm_sky_out_t;

/* ---- the fog ------------------------------------------------------------------------------------------------- */

/* the fog at view depth d (metres) for DRAW DISTANCE dist: 0 up to 0.43 dist, 1 from dist, quadratic between */
static inline float psm_fog_at(float dist, float d) {
    const float s = PSM_FOG_START * dist;
    float t;

    if (!(dist > 0.0f)) return 0.0f;
    t = sh4der_clamp_((d - s) / (dist - s), 0.0f, 1.0f);
    return t * t;
}

/* The 129 values for pvr_fog_table_custom after pvr_fog_far_depth(dist). The PVR scales 1 / W by the far depth and reads the
   table at that value's exponent and top four mantissa bits: entry k is depth dist / t(k), t(k) = 2^(k >> 4) x (16 + (k & 15)) / 16
   (KallistiOS pvr_fog_tables.h, inverse_w_depth, with entry 0 the far depth itself). */
static inline void psm_fog_table(float dist, float tbl[129]) {
    unsigned k;

    for (k = 0; k < 129u; k++) {
        const float t = (float)(1u << (k >> 4)) * (float)(16u + (k & 15u)) * (1.0f / 16.0f);

        tbl[k] = psm_fog_at(dist, dist / t);
    }
}

/* The fog the table gives at depth d: the entry pair round it, linearly (the PVR interpolates between an entry's two bytes). */
static inline float psm_fog_table_at(const float tbl[129], float dist, float d) {
    const float x = dist * sh4der_recip_pos_(d);   /* the scaled 1 / W */
    unsigned e = 0, k;
    float m, f;

    if (!(x >= 1.0f)) return tbl[0];
    while (e < 7u && x >= (float)(2u << e)) e++;
    m = x / (float)(1u << e) * 16.0f - 16.0f;       /* 0 .. 16 within the octave */
    k = e * 16u + (unsigned)m;
    if (k >= 128u) return tbl[128];
    f = m - (float)(unsigned)m;
    return tbl[k] + (tbl[k + 1u] - tbl[k]) * f;
}

/* ---- the rings ----------------------------------------------------------------------------------------------- */

static inline float psm_sky_frac_(float x) { return x - floorf(x); }

/* a low-passed ring of PSM_SKY_BINS colours at u (0 .. 1, wraps), linearly */
static inline void psm_sky_ring_(const float (*ring)[3], float u, float out[3]) {
    const float b = psm_sky_frac_(u) * (float)PSM_SKY_BINS - 0.5f;
    const float fb = floorf(b), t = b - fb;
    const unsigned i0 = ((unsigned)((int)fb + (int)PSM_SKY_BINS)) % PSM_SKY_BINS, i1 = (i0 + 1u) % PSM_SKY_BINS;
    unsigned c;

    for (c = 0; c < 3u; c++) out[c] = ring[i0][c] + (ring[i1][c] - ring[i0][c]) * t;
}

/* the texture's u at compass bearing a (degrees) with the sun at sun_az: the HDR's sun column faces the sun */
static inline float psm_sky_u(float a, float u_off) { return a * (1.0f / 360.0f) + u_off; }
static inline float psm_sky_u_off(float sun_az) { return psm_sky_frac_(PSM_SKY_SUN_U - sun_az * (1.0f / 360.0f)); }

/* the far hills' top (degrees) at bearing a: smooth, 0.5 .. 3.2 */
static inline float psm_hill_top(float a) {
    const float r = a * 0.0174533f;
    const float n = 0.5f + 0.30f * sinf(2.0f * r + 0.7f) + 0.12f * sinf(5.0f * r + 2.1f) + 0.06f * sinf(11.0f * r + 4.0f);   /* 0.02 .. 0.98 */

    return 0.5f + 2.7f * sh4der_clamp_(n, 0.0f, 1.0f);
}

/* the treeline's top at segment k: crowns on the even vertices, dips on the odd, a field (no trees) now and then; 0.25 .. 1.5 */
static inline float psm_tree_top(unsigned k) {
    const unsigned h = (k * 2654435761u) >> 24;      /* 0 .. 255, fixed per segment */

    if ((k % 23u) < 3u) return 0.25f;                /* a field: three segments of open ground */
    return (k & 1u) ? 0.55f + 0.2f * (float)(h & 63u) / 63.0f : 1.0f + 0.5f * (float)h / 255.0f;
}

static inline uint32_t psm_sky_argb_(const float c[4]) {
    return psm_pack_rgb(c[0], c[1], c[2]);
}

/* a placed vertex: the eye plus radius r in the direction (bearing a, elevation e, degrees) */
static inline void psm_sky_place_(const psm_xf_t *xf, const float eye[3], float a, float e, float r, psm_skyp_t *p) {
    float sa, ca, se, ce, w[3];

    sh4der_sincos_(a * 0.0174533f, &sa, &ca);
    sh4der_sincos_(e * 0.0174533f, &se, &ce);
    w[0] = eye[0] + r * sa * ce; w[1] = eye[1] + r * se; w[2] = eye[2] - r * ca * ce;
    psm_place_pt_(xf, w, p->clip, p->scr);
}

static inline void psm_sky_emit_(psm_sky_out_t *o, unsigned *n, const psm_skyp_t *p, int eol) {
    psm_skyv_t *v;

    if (*n >= PSM_SKY_OUT_MAX) return;
    v = &o->v[*n];
    v->x = p->scr[0]; v->y = p->scr[1]; v->z = p->scr[2];
    v->u = p->u; v->v = p->v;
    v->argb = psm_sky_argb_(p->c);
    v->oargb = (uint32_t)(sh4der_sat_(p->c[3]) * 255.0f + 0.5f) << 24;
    o->eol[*n] = (uint8_t)eol;
    (*n)++;
}

/* One triangle with a corner at or behind the near plane: the part in front (W >= 1.01 near), cut in clip space where
   everything is linear, sent as a strip of 3 or 4 (1, 0, 2, 3, as psm_clip_near). Returns the triangles sent. */
static inline unsigned psm_sky_tri_(psm_sky_out_t *o, unsigned *n, const psm_skyp_t *a, const psm_skyp_t *b, const psm_skyp_t *c) {
    const psm_skyp_t *in[3] = { a, b, c };
    const float wc = 1.01f * PSM_NEAR;
    psm_skyp_t p[4];
    unsigned i, k = 0;

    for (i = 0; i < 3u; i++) {
        const psm_skyp_t *s = in[i], *e = in[(i + 1u) % 3u];
        const int s_in = s->clip[2] >= wc, e_in = e->clip[2] >= wc;

        if (s_in) p[k++] = *s;
        if (s_in != e_in && k < 4u) {
            const float t = (wc - s->clip[2]) / (e->clip[2] - s->clip[2]);
            psm_skyp_t *q = &p[k++];
            unsigned j;

            for (j = 0; j < 3u; j++) q->clip[j] = s->clip[j] + (e->clip[j] - s->clip[j]) * t;
            for (j = 0; j < 4u; j++) q->c[j] = s->c[j] + (e->c[j] - s->c[j]) * t;
            q->u = s->u + (e->u - s->u) * t; q->v = s->v + (e->v - s->v) * t;
            q->scr[2] = 1.0f / q->clip[2];
            q->scr[0] = (q->clip[0] * q->scr[2] * 0.5f + 0.5f) * PSM_SCR_W;
            q->scr[1] = (0.5f - q->clip[1] * q->scr[2] * 0.5f) * PSM_SCR_H;
        }
    }
    if (k < 3u) return 0u;
    psm_sky_emit_(o, n, &p[1], 0);
    psm_sky_emit_(o, n, &p[0], 0);
    psm_sky_emit_(o, n, &p[2], k == 3u);
    if (k == 4u) psm_sky_emit_(o, n, &p[3], 1);
    return k - 2u;
}

/* A grid of rows x cols placed vertices (row 0 the bottom) as strips, a row at a time: a run of quads with every corner in
   front goes as one strip (upper, lower vertex per column); a quad with a corner at or behind the near plane ends the run
   and goes as its two triangles, cut. A quad wholly behind is not sent. */
/* quad c of a grid row: 0 wholly past one side of the view (each side plane is linear in clip space, so this holds for
   corners behind the eye too), 1 every corner in front of the near plane, 2 to be cut */
static inline unsigned psm_sky_quad_(const psm_skyp_t *lo, const psm_skyp_t *up, unsigned c) {
    const float wc = 1.01f * PSM_NEAR;

    if (!psm_cell_seen_(lo[c].clip, lo[c + 1u].clip, up[c].clip, up[c + 1u].clip)) return 0u;
    return lo[c].clip[2] >= wc && up[c].clip[2] >= wc && lo[c + 1u].clip[2] >= wc && up[c + 1u].clip[2] >= wc ? 1u : 2u;
}

static inline void psm_sky_grid_(psm_sky_out_t *o, unsigned *n, const psm_skyp_t *g, unsigned rows, unsigned cols) {
    unsigned r, c;

    for (r = 0; r + 1u < rows; r++) {
        const psm_skyp_t *lo = g + r * cols, *up = lo + cols;

        c = 0;
        while (c + 1u < cols) {
            const unsigned q = psm_sky_quad_(lo, up, c);
            unsigned e = c, k;

            if (q == 1u) {                                 /* quads c .. e - 1: one strip */
                while (e + 1u < cols && psm_sky_quad_(lo, up, e) == 1u) e++;
                for (k = c; k <= e; k++) {
                    psm_sky_emit_(o, n, &up[k], 0);
                    psm_sky_emit_(o, n, &lo[k], k == e);
                }
                c = e;
                continue;
            }
            if (q == 2u) {
                o->cut += psm_sky_tri_(o, n, &up[c], &lo[c], &up[c + 1u]) ? 1u : 0u;
                o->cut += psm_sky_tri_(o, n, &lo[c], &up[c + 1u], &lo[c + 1u]) ? 1u : 0u;
            }
            c++;
        }
    }
}

/* This frame's sky, cap and hills, placed and in send order. The frame's camera rows and eye (psm_frame_setup first), the
   sun's azimuth and elevation from the state. */
static inline void psm_sky_build(psm_sky_out_t *o, const psm_frame_t *f, const psm_state_t *st) {
    static psm_skyp_t g[PSM_SKY_ROWS * (PSM_SKY_SEGS + 1u)], h[2u * (PSM_HILL_SEGS + 1u)], apex;
    const float half = 37.6f;                       /* the view's half width in bearing: atan(tan 30 x 4 / 3) */
    float yaw = st->yaw * 57.29578f, top[3], sa, ca;
    psm_xf_t xf;
    unsigned r, k, n = 0;

    o->cut = 0u;
    o->u_off = psm_sky_u_off(st->az);
    psm_xf_load(&xf, f->rows);

    /* the fog colour: the horizon row over nine bearings across the view */
    o->horizon[0] = o->horizon[1] = o->horizon[2] = 0.0f;
    for (k = 0; k < 9u; k++) {
        float c3[3];

        psm_sky_ring_(psm_sky_horizon, psm_sky_u(yaw - half + 2.0f * half * (float)k / 8.0f, o->u_off), c3);
        for (r = 0; r < 3u; r++) o->horizon[r] += c3[r] * (1.0f / 9.0f);
    }
    for (r = 0; r < 3u; r++) o->fog[r] = 0.75f * o->horizon[r] + 0.25f * psm_sky_land[r];

    /* the band */
    for (r = 0; r < PSM_SKY_ROWS; r++)
        for (k = 0; k <= PSM_SKY_SEGS; k++) {
            psm_skyp_t *p = &g[r * (PSM_SKY_SEGS + 1u) + k];
            const float a = 360.0f * (float)k / (float)PSM_SKY_SEGS, e = psm_sky_el[r];
            float w = 0.0f;
            unsigned c;

            psm_sky_place_(&xf, f->cam.eye, a, e, PSM_SKY_R, p);
            p->u = (float)k / (float)PSM_SKY_SEGS + o->u_off;
            p->v = (PSM_SKY_EL1 - e) * (1.0f / (PSM_SKY_EL1 - PSM_SKY_EL0));
            if (e <= 6.0f) {
                sh4der_sincos_((a - st->az) * 0.0174533f, &sa, &ca);
                w = ca > 0.0f ? ca * ca * ca : 0.0f;
                if (e > 0.0f) w *= 1.0f - e / 12.0f;       /* el 6: half */
            }
            for (c = 0; c < 3u; c++) p->c[c] = 1.0f + (psm_sky_warm[c] - 1.0f) * w;
            p->c[3] = sh4der_clamp_(1.0f - e / 6.0f, 0.0f, 1.0f);   /* the vertex fog: 1 at and under el 0, 0 from el 6 */
        }
    psm_sky_grid_(o, &n, g, PSM_SKY_ROWS, PSM_SKY_SEGS + 1u);
    o->n_band = n;

    /* the cap: the band's top ring to the zenith, a triangle a segment */
    psm_sky_place_(&xf, f->cam.eye, 0.0f, 90.0f, PSM_SKY_R, &apex);
    apex.u = apex.v = 0.0f;
    apex.c[0] = psm_sky_zenith[0]; apex.c[1] = psm_sky_zenith[1]; apex.c[2] = psm_sky_zenith[2]; apex.c[3] = 0.0f;
    for (k = 0; k < PSM_SKY_SEGS; k++) {
        psm_skyp_t a = g[(PSM_SKY_ROWS - 1u) * (PSM_SKY_SEGS + 1u) + k], b = g[(PSM_SKY_ROWS - 1u) * (PSM_SKY_SEGS + 1u) + k + 1u];

        psm_sky_ring_(psm_sky_top, a.u, top); a.c[0] = top[0]; a.c[1] = top[1]; a.c[2] = top[2]; a.c[3] = 0.0f;
        psm_sky_ring_(psm_sky_top, b.u, top); b.c[0] = top[0]; b.c[1] = top[1]; b.c[2] = top[2]; b.c[3] = 0.0f;
        if (!psm_cell_seen_(a.clip, b.clip, apex.clip, apex.clip)) continue;     /* wholly past one side of the view */
        if (a.clip[2] >= 1.01f * PSM_NEAR && b.clip[2] >= 1.01f * PSM_NEAR && apex.clip[2] >= 1.01f * PSM_NEAR) {
            psm_sky_emit_(o, &n, &a, 0); psm_sky_emit_(o, &n, &b, 0); psm_sky_emit_(o, &n, &apex, 1);
        } else
            o->cut += psm_sky_tri_(o, &n, &a, &b, &apex) ? 1u : 0u;
    }
    o->n_cap = n - o->n_band;

    /* the hills: far hills, then the treeline in front; bases in the fog colour */
    for (r = 0; r < 2u; r++) {
        const float rad = r ? PSM_TREE_R : PSM_HILL_R;

        for (k = 0; k <= PSM_HILL_SEGS; k++) {
            psm_skyp_t *b = &h[k], *t = &h[PSM_HILL_SEGS + 1u + k];
            const float a = 360.0f * (float)k / (float)PSM_HILL_SEGS;
            float hz[3];
            unsigned c;

            psm_sky_place_(&xf, f->cam.eye, a, PSM_HILL_BASE, rad, b);
            psm_sky_place_(&xf, f->cam.eye, a, r ? psm_tree_top(k % PSM_HILL_SEGS) : psm_hill_top(a), rad, t);
            psm_sky_ring_(psm_sky_horizon, psm_sky_u(a, o->u_off), hz);
            for (c = 0; c < 3u; c++) {
                static const float hill[3] = { 0.36f, 0.44f, 0.40f }, tree[3] = { 0.20f, 0.27f, 0.18f };

                b->c[c] = o->fog[c];
                t->c[c] = r ? 0.5f * o->fog[c] + 0.5f * tree[c] : 0.70f * hz[c] + 0.30f * hill[c];
            }
            b->c[3] = t->c[3] = 0.0f;
            b->u = b->v = t->u = t->v = 0.0f;
        }
        psm_sky_grid_(o, &n, h, 2u, PSM_HILL_SEGS + 1u);
    }
    o->n_hill = n - o->n_band - o->n_cap;
}

#endif /* PSM_SKY_AIR_H */
