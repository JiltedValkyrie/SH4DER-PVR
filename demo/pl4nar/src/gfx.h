/* gfx: a small triangle layer. One front end (gfx.c: near clip, cull, zero-area guard, off-target reject) over a back end:
 * the PVR through KallistiOS direct rendering (gfx_pvr.c) or a software rasteriser for the host preview (host/gfx_host.c).
 * Maths: SH4ZAM (XMTRX transform, shz_invf, shz_lerpf, shz_vec2_cross). */
#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#include <sh4zam/shz_sh4zam.h>

#define GFX_NEAR      0.30f        /* near clip, metres (clip w) */
#define GFX_SCREEN_W  640.0f
#define GFX_SCREEN_H  480.0f
#define GFX_MIN_AREA2 0.02f        /* twice the screen area (px^2) under which a triangle is not sent: the PVR draws it as a line */

enum {
    GFX_MAT_FLAT = 0,              /* opaque, vertex colour */
    GFX_MAT_FACADE0, GFX_MAT_FACADE1, GFX_MAT_FACADE2, GFX_MAT_FACADE3,   /* opaque, textured, offset colour (haze) */
    GFX_MAT_ROAD,
    GFX_MAT_REFLECT,               /* translucent: the reflection texture, vertex alpha (src alpha / 1 - src alpha: glass) */
    GFX_MAT_REFLECT_ADD,           /* the same, added (src alpha / one): a clear coat over paint */
    GFX_MAT_HUD,                   /* translucent, vertex colour */
    GFX_MAT_FONT,                  /* translucent, font texture */
    GFX_MAT_ADD,                   /* translucent, vertex colour, added (src alpha / one): light pools */
    GFX_MAT_GLOW,                  /* translucent, the glow texture x vertex colour, added: halos */
    GFX_MAT_P_FACADE0, GFX_MAT_P_FACADE1, GFX_MAT_P_FACADE2, GFX_MAT_P_FACADE3,   /* the same world, 64 x 64, 16 colours, point sampled */
    GFX_MAT_P_ROAD,
    GFX_MAT_SHADOW,                /* translucent: the reflection target's shadow tiles x vertex colour, multiplied into what is
                                      there (DESTCOLOR x ZERO): white keeps the ground, grey darkens it */
    GFX_MAT_MUL,                   /* translucent, vertex colour multiplied into what is there (DESTCOLOR x ZERO): baked occlusion */
    GFX_MAT_COUNT
};
enum { GFX_CULL_NONE = 0, GFX_CULL_BACK = 1, GFX_CULL_MIRROR = 2 };

typedef struct gfx_vert {          /* 48 B */
    float    x, y, z;              /* screen pixels, z = 1 / w (larger = nearer) */
    float    cx, cy, w;            /* clip space (x * w, y * w, w): what the near clipper cuts */
    float    u, v;
    uint32_t argb, oargb;          /* colour, offset colour */
    uint32_t clip;                 /* 1 = behind the near plane: x, y, z are not set */
    uint32_t pad;
} gfx_vert_t;

/* World (or model) point -> vertex position fields. XMTRX = screen x projection x view (x model). */
static inline void gfx_xform(gfx_vert_t *o, shz_vec3_t p) {
    const shz_vec4_t c = shz_xmtrx_transform_vec4(shz_vec3_vec4(p, 1.0f));
    o->cx = c.x; o->cy = c.y; o->w = c.w;
    if (c.w >= GFX_NEAR) {
        const float iw = shz_invf(c.w);
        o->x = c.x * iw; o->y = c.y * iw; o->z = iw;      /* hand: scalar products, no SH4ZAM op */
        o->clip = 0u;
    } else o->clip = 1u;
}
/* A 2D vertex (HUD): z = depth key, larger = in front. */
static inline void gfx_vert2d(gfx_vert_t *o, float x, float y, float z, float u, float v, uint32_t argb) {
    o->x = x; o->y = y; o->z = z; o->cx = x; o->cy = y; o->w = 1.0f; o->u = u; o->v = v; o->argb = argb; o->oargb = 0u; o->clip = 0u;
}

void gfx_init(void);
void gfx_shutdown(void);
/* Frame: [reflection scene] then the screen scene; in a scene the opaque list, then gfx_list_translucent(). */
int  gfx_reflect_resize(unsigned w, unsigned h);          /* (re)allocates the reflection target; 0 = ok */
unsigned gfx_reflect_bytes(void);
float gfx_reflect_u_scale(void);                          /* multiply a lookup's u by this (the target's picture width / its texture width) */
int  gfx_reflect_begin(void);                             /* 0 = the scene is open on the reflection target */
void gfx_reflect_end(void);
void gfx_frame_begin(void);
void gfx_list_translucent(void);
void gfx_frame_end(void);
/* The tile cut (a stage, demo/psm/ao/tile_cut.h): the next reflection passes go through the screen's top left cw x ch
   tiles alone, not the whole 20 x 15; 0 x 0 off (the default). A cut that is refused runs the pass uncut and is counted;
   a table that does not read back after a pass turns the cut off for good, restarts the PVR and is counted (failed). */
typedef struct gfx_cut_stats {
    uint32_t passes;               /* reflection passes that ran cut */
    uint32_t refused;              /* passes asked for a cut and run uncut */
    uint32_t failed;               /* tables that did not read back: the PVR was restarted, the cut is off */
    uint32_t bad_words;            /* words that did not read back, all failures */
    unsigned tables;               /* tile tables found (2 is both) */
    unsigned cw, ch;               /* the cut asked for */
} gfx_cut_stats_t;
void gfx_tile_cut(unsigned cw, unsigned ch);
void gfx_tile_cut_stats(gfx_cut_stats_t *o);

void gfx_clear_colour(uint32_t argb);                     /* what shows where nothing is drawn (the next screen scene) */
void gfx_viewport(float w, float h);                      /* triangles wholly outside 0 .. w, 0 .. h are dropped */
void gfx_cull(int mode);                                  /* GFX_CULL_*: MIRROR for a mirrored world (winding reversed) */
void gfx_bind(int mat);
void gfx_tri(const gfx_vert_t *a, const gfx_vert_t *b, const gfx_vert_t *c);
/* The user clip (translucent list only): tiles = { tx0, ty0, tx1, ty1 } in 32 pixel tiles, both ends counted, sends the
   clip into the open list and makes the reflection materials (GFX_MAT_REFLECT, _ADD) bound after it keep only that
   rectangle; NULL: they draw everywhere again. A change of clip is preceded by a dummy polygon off the screen (the core
   takes a new clip one polygon early: Sega's bug list, CORE 18; the sequence of demo/psm/ao/ao4_spike.c). */
void gfx_clip(const unsigned *tiles);
void gfx_quad2d(float x0, float y0, float x1, float y1, float z, float u0, float v0, float u1, float v1, uint32_t argb);
unsigned gfx_tris_sent(void);                             /* since the last scene begin */

/* Back end (gfx_pvr.c / host/gfx_host.c). */
void gfx_be_init(void);
void gfx_be_shutdown(void);
void gfx_be_bind(int mat);
void gfx_be_clear_colour(uint32_t argb);
void gfx_be_tri(const gfx_vert_t *a, const gfx_vert_t *b, const gfx_vert_t *c);
void gfx_be_clip(const unsigned *tiles);
int  gfx_be_reflect_resize(unsigned w, unsigned h);
unsigned gfx_be_reflect_bytes(void);
float gfx_be_reflect_u_scale(void);
int  gfx_be_reflect_begin(void);
void gfx_be_reflect_end(void);
void gfx_be_frame_begin(void);
void gfx_be_list_translucent(void);
void gfx_be_frame_end(void);
void gfx_be_tile_cut(unsigned cw, unsigned ch);
void gfx_be_tile_cut_stats(gfx_cut_stats_t *o);

static inline uint32_t gfx_rgb(unsigned a, unsigned r, unsigned g, unsigned b) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}
/* a + (b - a) * t per channel, t = 0 .. 256 */
static inline uint32_t gfx_mix(uint32_t a, uint32_t b, unsigned t) {
    const uint32_t rb = (((a & 0x00ff00ffu) * (256u - t) + (b & 0x00ff00ffu) * t) >> 8) & 0x00ff00ffu;
    const uint32_t ag = ((((a >> 8) & 0x00ff00ffu) * (256u - t) + ((b >> 8) & 0x00ff00ffu) * t)) & 0xff00ff00u;
    return rb | ag;
}
/* colour x tint per channel (tint 255 = no change); alpha kept */
static inline uint32_t gfx_mod(uint32_t c, uint32_t t) {
    const uint32_t r = (((c >> 16) & 255u) * (((t >> 16) & 255u) + 1u)) >> 8;
    const uint32_t g = (((c >> 8) & 255u) * (((t >> 8) & 255u) + 1u)) >> 8;
    const uint32_t b = ((c & 255u) * ((t & 255u) + 1u)) >> 8;
    return (c & 0xff000000u) | (r << 16) | (g << 8) | b;
}
/* colour x k (0 .. 256) on r, g, b; alpha kept */
static inline uint32_t gfx_scale(uint32_t c, unsigned k) {
    const uint32_t rb = (((c & 0x00ff00ffu) * k) >> 8) & 0x00ff00ffu;
    const uint32_t g = (((c & 0x0000ff00u) * k) >> 8) & 0x0000ff00u;
    return (c & 0xff000000u) | rb | g;
}
#endif
