/* gfx front end: near clip in clip space, back-face cull by the signed screen area, zero-area guard, off-target reject.
 * SH4ZAM: shz_vec2_cross (signed area), shz_lerpf / shz_invf (clip), shz_divf (clip parameter). */
#include "gfx.h"
#include "sh4der_guard.h"

static float    s_vw = GFX_SCREEN_W, s_vh = GFX_SCREEN_H;
static float    s_cull;                    /* 0 none, else the sign of twice the screen area of a front face */
static unsigned s_sent;
#ifdef GFX_COUNT_XFORM
unsigned gfx_xform_n;
#endif

void gfx_init(void) { gfx_be_init(); }
void gfx_shutdown(void) { gfx_be_shutdown(); }
int  gfx_reflect_resize(unsigned w, unsigned h) { return gfx_be_reflect_resize(w, h); }
unsigned gfx_reflect_bytes(void) { return gfx_be_reflect_bytes(); }
float gfx_reflect_u_scale(void) { return gfx_be_reflect_u_scale(); }
int  gfx_reflect_begin(void) { s_sent = 0u; return gfx_be_reflect_begin(); }
void gfx_reflect_end(void) { gfx_be_reflect_end(); }
void gfx_frame_begin(void) { s_sent = 0u; gfx_be_frame_begin(); }
void gfx_list_translucent(void) { gfx_be_list_translucent(); }
void gfx_frame_end(void) { gfx_be_frame_end(); }
void gfx_tile_cut(unsigned cw, unsigned ch) { gfx_be_tile_cut(cw, ch); }
void gfx_tile_cut_stats(gfx_cut_stats_t *o) { gfx_be_tile_cut_stats(o); }
void gfx_viewport(float w, float h) { s_vw = w; s_vh = h; }
void gfx_bind(int mat) { gfx_be_bind(mat); }
void gfx_clip(const unsigned *tiles) { gfx_be_clip(tiles); }
void gfx_clear_colour(uint32_t argb) { gfx_be_clear_colour(argb); }
unsigned gfx_tris_sent(void) { return s_sent; }

/* A right-handed world, counter-clockwise front faces, a screen with y down: a front face has a NEGATIVE cross product
 * (b - a) x (c - a) on screen. A mirrored world reverses it. */
void gfx_cull(int mode) { s_cull = mode == GFX_CULL_BACK ? -1.0f : mode == GFX_CULL_MIRROR ? 1.0f : 0.0f; }

/* ---- the window (gfx_window): a triangle cut to 0 .. w, 0 .. h and clip w >= near, then moved ---------------------- */
static float s_wox, s_woy, s_ww, s_wh, s_wnear;    /* s_ww <= 0: no window */
void gfx_window(float ox, float oy, float w, float h, float near_w) { s_wox = ox; s_woy = oy; s_ww = w; s_wh = h; s_wnear = near_w < GFX_NEAR ? GFX_NEAR : near_w; }

/* A vertex's distance inside plane p (0 near, 1 left, 2 right, 3 top, 4 bottom), in clip space: >= 0 is inside. */
static inline float win_d(const gfx_vert_t *v, unsigned p) {
    return p == 0u ? v->w - s_wnear : p == 1u ? v->cx : p == 2u ? shz_fmaf(s_ww, v->w, -v->cx) : p == 3u ? v->cy : shz_fmaf(s_wh, v->w, -v->cy);
}
static void win_tri(const gfx_vert_t *a, const gfx_vert_t *b, const gfx_vert_t *c) {
    gfx_vert_t buf[2][8];
    const gfx_vert_t *in = buf[0];
    unsigned n = 3u, p, i, cur = 0u;
    buf[0][0] = *a; buf[0][1] = *b; buf[0][2] = *c;
    for (p = 0; p < 5u; p++) {
        float d[8];
        unsigned outside = 0u, m = 0u;
        gfx_vert_t *out;
        for (i = 0; i < n; i++) { d[i] = win_d(&in[i], p); outside += d[i] < 0.0f; }
        if (outside == n) return;
        if (!outside) continue;
        out = buf[cur ^ 1u];
        for (i = 0; i < n; i++) {
            const unsigned j = (i + 1u) % n;
            if (d[i] >= 0.0f) out[m++] = in[i];
            if ((d[i] >= 0.0f) != (d[j] >= 0.0f)) {            /* the edge crosses the plane: the point on it */
                const float t = shz_divf(d[i], d[i] - d[j]);
                const unsigned ti = (unsigned)(t * 256.0f);
                gfx_vert_t *o = &out[m++];
                o->cx = shz_lerpf(in[i].cx, in[j].cx, t); o->cy = shz_lerpf(in[i].cy, in[j].cy, t); o->w = shz_lerpf(in[i].w, in[j].w, t);
                o->u = shz_lerpf(in[i].u, in[j].u, t); o->v = shz_lerpf(in[i].v, in[j].v, t);
                o->argb = gfx_mix(in[i].argb, in[j].argb, ti); o->oargb = gfx_mix(in[i].oargb, in[j].oargb, ti);
                o->clip = 0u;
            }
        }
        cur ^= 1u; in = buf[cur]; n = m;                       /* a triangle against five planes: 8 points at most */
        if (n < 3u) return;
    }
    {
        gfx_vert_t *v = buf[cur];
        if (in != v) { v[0] = in[0]; v[1] = in[1]; v[2] = in[2]; }
        for (i = 0; i < n; i++) {
            const float iw = shz_invf(v[i].w);
            v[i].x = shz_fmaf(v[i].cx, iw, s_wox); v[i].y = shz_fmaf(v[i].cy, iw, s_woy); v[i].z = iw;
        }
        for (i = 1; i + 1u < n; i++) {                          /* a fan; a piece with no area is not sent */
            const float a2 = shz_vec2_cross(shz_vec2_init(v[i].x - v[0].x, v[i].y - v[0].y), shz_vec2_init(v[i + 1u].x - v[0].x, v[i + 1u].y - v[0].y));
            if (!(shz_fabsf(a2) >= GFX_MIN_AREA2)) continue;
            gfx_be_tri(&v[0], &v[i], &v[i + 1u]);
            s_sent++;
        }
    }
}

static void out_tri(const gfx_vert_t *a, const gfx_vert_t *b, const gfx_vert_t *c) {
    const float area2 = shz_vec2_cross(shz_vec2_init(b->x - a->x, b->y - a->y), shz_vec2_init(c->x - a->x, c->y - a->y));
    if (s_ww > 0.0f) {                                         /* a window: the cull by the sign alone, then the cut */
        if (s_cull != 0.0f ? !(area2 * s_cull >= GFX_MIN_AREA2) : !(shz_fabsf(area2) >= GFX_MIN_AREA2)) return;
        win_tri(a, b, c);
        return;
    }
    if (s_cull != 0.0f) { if (!(area2 * s_cull >= GFX_MIN_AREA2)) return; }     /* written so that a NaN is dropped too */
    else {                                                     /* no area: the PVR would draw a line (the library's guard). cx, cy, w is
                                                                  (x w, y w, w), so the guard's x / z is the pixel. With a cull mode the
                                                                  signed area above has already dropped it: the guard has no sign */
        const float pa[3] = { a->cx, a->cy, a->w }, pb[3] = { b->cx, b->cy, b->w }, pc[3] = { c->cx, c->cy, c->w };
        if (!sh4der_tri_area_ok(pa, pb, pc, GFX_NEAR, GFX_MIN_AREA2)) return;
    }
    if ((a->x < 0.0f && b->x < 0.0f && c->x < 0.0f) || (a->x > s_vw && b->x > s_vw && c->x > s_vw) ||
        (a->y < 0.0f && b->y < 0.0f && c->y < 0.0f) || (a->y > s_vh && b->y > s_vh && c->y > s_vh)) return;
    gfx_be_tri(a, b, c);
    s_sent++;
}

/* The point where edge in -> out crosses w = GFX_NEAR. */
static void cut(gfx_vert_t *o, const gfx_vert_t *in, const gfx_vert_t *out) {
    const float t = shz_divf(GFX_NEAR - in->w, out->w - in->w);
    const unsigned ti = (unsigned)(t * 256.0f);
    const float iw = shz_invf(GFX_NEAR);
    o->cx = shz_lerpf(in->cx, out->cx, t);
    o->cy = shz_lerpf(in->cy, out->cy, t);
    o->w = GFX_NEAR;
    o->u = shz_lerpf(in->u, out->u, t);
    o->v = shz_lerpf(in->v, out->v, t);
    o->argb = gfx_mix(in->argb, out->argb, ti);
    o->oargb = gfx_mix(in->oargb, out->oargb, ti);
    o->x = o->cx * iw; o->y = o->cy * iw; o->z = iw;          /* hand: scalar products */
    o->clip = 0u;
}

void gfx_tri(const gfx_vert_t *a, const gfx_vert_t *b, const gfx_vert_t *c) {
    const unsigned n = a->clip + b->clip + c->clip;
    if (__builtin_expect(n == 0u, 1)) { out_tri(a, b, c); return; }
    if (n == 3u) return;
    {
        const gfx_vert_t *v[3] = { a, b, c };
        gfx_vert_t p, q;
        unsigned i = 0u;
        if (n == 2u) {                                         /* one corner in front: a smaller triangle, same winding */
            while (v[i]->clip) i++;
            cut(&p, v[i], v[(i + 1u) % 3u]);
            cut(&q, v[i], v[(i + 2u) % 3u]);
            out_tri(v[i], &p, &q);
        } else {                                               /* one corner behind: a quad */
            while (!v[i]->clip) i++;
            cut(&p, v[(i + 1u) % 3u], v[i]);
            cut(&q, v[(i + 2u) % 3u], v[i]);
            out_tri(&p, v[(i + 1u) % 3u], v[(i + 2u) % 3u]);
            out_tri(&p, v[(i + 2u) % 3u], &q);
        }
    }
}

void gfx_quad2d(float x0, float y0, float x1, float y1, float z, float u0, float v0, float u1, float v1, uint32_t argb) {
    gfx_vert_t q[4];
    gfx_vert2d(&q[0], x0, y0, z, u0, v0, argb);
    gfx_vert2d(&q[1], x1, y0, z, u1, v0, argb);
    gfx_vert2d(&q[2], x1, y1, z, u1, v1, argb);
    gfx_vert2d(&q[3], x0, y1, z, u0, v1, argb);
    gfx_tri(&q[0], &q[1], &q[2]);
    gfx_tri(&q[0], &q[2], &q[3]);
}
