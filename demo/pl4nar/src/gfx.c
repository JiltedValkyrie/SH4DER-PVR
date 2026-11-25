/* gfx front end: near clip in clip space, back-face cull by the signed screen area, zero-area guard, off-target reject.
 * SH4ZAM: shz_vec2_cross (signed area), shz_lerpf / shz_invf (clip), shz_divf (clip parameter). */
#include "gfx.h"
#include "sh4der_guard.h"

static float    s_vw = GFX_SCREEN_W, s_vh = GFX_SCREEN_H;
static float    s_cull;                    /* 0 none, else the sign of twice the screen area of a front face */
static unsigned s_sent;

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
void gfx_viewport(float w, float h) { s_vw = w; s_vh = h; }
void gfx_bind(int mat) { gfx_be_bind(mat); }
void gfx_clip(const unsigned *tiles) { gfx_be_clip(tiles); }
void gfx_clear_colour(uint32_t argb) { gfx_be_clear_colour(argb); }
unsigned gfx_tris_sent(void) { return s_sent; }

/* A right-handed world, counter-clockwise front faces, a screen with y down: a front face has a NEGATIVE cross product
 * (b - a) x (c - a) on screen. A mirrored world reverses it. */
void gfx_cull(int mode) { s_cull = mode == GFX_CULL_BACK ? -1.0f : mode == GFX_CULL_MIRROR ? 1.0f : 0.0f; }

static void out_tri(const gfx_vert_t *a, const gfx_vert_t *b, const gfx_vert_t *c) {
    const float area2 = shz_vec2_cross(shz_vec2_init(b->x - a->x, b->y - a->y), shz_vec2_init(c->x - a->x, c->y - a->y));
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
