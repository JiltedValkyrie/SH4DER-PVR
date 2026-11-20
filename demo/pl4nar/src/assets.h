/* Baked assets (tools/bake.py -> build/gen/assets_gen.c). Models: palette colour per vertex, no texture. */
#ifndef ASSETS_H
#define ASSETS_H
#include <stdint.h>

enum { CLS_TRIM = 0, CLS_PAINT = 1, CLS_GLASS = 2, CLS_TYRE = 3, CLS_LAMP = 4 };

typedef struct mdl_vert {          /* 32 B */
    float    p[3];                 /* model space, metres */
    float    n[3];                 /* unit normal */
    uint32_t rgb;                  /* 0xRRGGBB */
    uint32_t cls;                  /* CLS_* */
} mdl_vert_t;

typedef struct mdl {
    const mdl_vert_t *v;
    const uint16_t   *idx;         /* triangles, counter-clockwise seen from outside (right-handed) */
    uint32_t          nv, ni;
    float             radius;      /* bounding sphere about the model origin */
    float             lo[3], hi[3];
} mdl_t;

extern const mdl_t mdl_player_body, mdl_player_wheels;
extern const float mdl_player_body_sn[][3];      /* per vertex: the smoothed normal the reflection uses (lighting keeps the flat one) */
extern const mdl_t mdl_taxi_body, mdl_taxi_wheels, mdl_police_body, mdl_police_wheels, mdl_van_body, mdl_van_wheels;
extern const mdl_t mdl_lamp, mdl_pole, mdl_signal, mdl_cone, mdl_dumpster;

/* RGB565, twiddled, mip levels (PVR layout). */
#define TEX_FACADE_SIZE 128
#define TEX_FACADE_N    4
#define TEX_ROAD_SIZE   256
extern const uint16_t tex_facade0[], tex_facade1[], tex_facade2[], tex_facade3[];
extern const uint16_t tex_road[];
/* ARGB1555, row order, 128 x 128: ASCII 32 .. 127 in 8 x 16 cells (cell 127 = solid block). */
/* The low look: 64 x 64, 4 bits a texel (twiddled, the even texel in the low half of a byte: 2,048 B), and its
   16 colours as RGB565. Facades 0 .. 3, then the road. */
#define TEX_P_SIZE      64
#define TEX_P_N         5
extern const uint8_t  tex_p_facade0[], tex_p_facade1[], tex_p_facade2[], tex_p_facade3[], tex_p_road[];
extern const uint16_t pal_facade0[16], pal_facade1[16], pal_facade2[16], pal_facade3[16], pal_road[16];
/* RGB565, row order, 32 x 32: a round glow, white in the middle, black at the rim (added: black adds nothing). */
#define TEX_GLOW_SIZE   32
extern const uint16_t tex_glow[];
#define TEX_FONT_SIZE   128
extern const uint16_t tex_font[];

static inline uint32_t tex_mip_bytes(uint32_t size) {       /* bytes of a square 16-bit twiddled mip chain */
    uint32_t words = 3u, s;
    for (s = 1u; s < size; s <<= 1) words += s * s;
    return (words + size * size) * 2u;
}
#endif
