/* gfx back end for the host preview: a plain software rasteriser with the PVR's rules that matter here
 * (z = 1 / w, greater-or-equal depth test, perspective-correct u, v and colour, texel x colour + offset colour,
 * src-alpha blend for the translucent list, a render target that becomes a texture). Reads the SAME baked texture
 * bytes as the Dreamcast build (twiddled mip chain, level offsets), so a wrong bake shows here.
 * Host only: plain C maths. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gfx.h"
#include "../src/assets.h"
#include "gfx_host.h"

typedef struct { uint8_t *rgb; float *z; int w, h; } target_t;
typedef struct { const uint8_t *rgba; int w, h, nearest; } tex_t;

static target_t s_scr, s_rt, *s_t;
static tex_t    s_tex[GFX_MAT_COUNT];
static int      s_mat, s_translucent;
static uint8_t *s_rt_rgba;
static int      s_clip_on, s_clip[4];              /* gfx_clip: pixels x0, y0, x1, y1 (end excluded) for the reflection materials */
void gfx_be_clip(const unsigned *tiles) {
    s_clip_on = tiles != NULL;
    if (tiles) { s_clip[0] = (int)tiles[0] * 32; s_clip[1] = (int)tiles[1] * 32; s_clip[2] = ((int)tiles[2] + 1) * 32; s_clip[3] = ((int)tiles[3] + 1) * 32; }
}                         /* the reflection target as a texture (snapshot at scene end) */

static unsigned untwid(unsigned x, unsigned y, unsigned bits) {
    unsigned o = 0, i;
    for (i = 0; i < bits; i++) { o |= ((y >> i) & 1u) << (2u * i); o |= ((x >> i) & 1u) << (2u * i + 1u); }
    return o;
}

static uint8_t *decode_mip565(const uint16_t *words, int size) {
    uint8_t *o = malloc((size_t)size * size * 4);
    unsigned bits = 0, base = 3u, s, x, y;
    for (s = 1; s < (unsigned)size; s <<= 1) { base += s * s; bits++; }
    for (y = 0; y < (unsigned)size; y++)
        for (x = 0; x < (unsigned)size; x++) {
            const uint16_t p = words[base + untwid(x, y, bits)];
            uint8_t *d = o + (y * (unsigned)size + x) * 4u;
            d[0] = (uint8_t)(((p >> 11) & 31u) * 255u / 31u); d[1] = (uint8_t)(((p >> 5) & 63u) * 255u / 63u);
            d[2] = (uint8_t)((p & 31u) * 255u / 31u); d[3] = 255;
        }
    return o;
}

/* 4 bits a texel, twiddled, the even texel in the low half of a byte; 16 RGB565 colours. */
static uint8_t *decode_pal4(const uint8_t *nib, const uint16_t *pal, int size) {
    uint8_t *o = malloc((size_t)size * size * 4);
    unsigned bits = 0, s, x, y;
    for (s = 1; s < (unsigned)size; s <<= 1) bits++;
    for (y = 0; y < (unsigned)size; y++)
        for (x = 0; x < (unsigned)size; x++) {
            const unsigned t = untwid(x, y, bits);
            const uint16_t p = pal[(nib[t >> 1] >> ((t & 1u) * 4u)) & 15u];
            uint8_t *d = o + (y * (unsigned)size + x) * 4u;
            d[0] = (uint8_t)(((p >> 11) & 31u) * 255u / 31u); d[1] = (uint8_t)(((p >> 5) & 63u) * 255u / 63u);
            d[2] = (uint8_t)((p & 31u) * 255u / 31u); d[3] = 255;
        }
    return o;
}

static uint8_t *decode_565_rows(const uint16_t *words, int size) {
    uint8_t *o = malloc((size_t)size * size * 4);
    int i;
    for (i = 0; i < size * size; i++) {
        const uint16_t p = words[i];
        o[i * 4] = (uint8_t)(((p >> 11) & 31u) * 255u / 31u); o[i * 4 + 1] = (uint8_t)(((p >> 5) & 63u) * 255u / 63u);
        o[i * 4 + 2] = (uint8_t)((p & 31u) * 255u / 31u); o[i * 4 + 3] = 255;
    }
    return o;
}

static uint8_t *decode_1555(const uint16_t *words, int size) {
    uint8_t *o = malloc((size_t)size * size * 4);
    int i;
    for (i = 0; i < size * size; i++) {
        const uint16_t p = words[i];
        o[i * 4] = (uint8_t)(((p >> 10) & 31u) * 255u / 31u); o[i * 4 + 1] = (uint8_t)(((p >> 5) & 31u) * 255u / 31u);
        o[i * 4 + 2] = (uint8_t)((p & 31u) * 255u / 31u); o[i * 4 + 3] = (p & 0x8000u) ? 255 : 0;
    }
    return o;
}

static void target_alloc(target_t *t, int w, int h) {
    free(t->rgb); free(t->z);
    t->rgb = calloc((size_t)w * h, 3); t->z = calloc((size_t)w * h, sizeof(float));
    t->w = w; t->h = h;
}

static void target_clear(target_t *t, unsigned r, unsigned g, unsigned b) {
    int i;
    for (i = 0; i < t->w * t->h; i++) { t->rgb[i * 3] = (uint8_t)r; t->rgb[i * 3 + 1] = (uint8_t)g; t->rgb[i * 3 + 2] = (uint8_t)b; t->z[i] = 0.0f; }
}

void gfx_be_init(void) {
    static const uint16_t *const fac[4] = { tex_facade0, tex_facade1, tex_facade2, tex_facade3 };
    int i;
    target_alloc(&s_scr, 640, 480);
    for (i = 0; i < 4; i++) { s_tex[GFX_MAT_FACADE0 + i].rgba = decode_mip565(fac[i], TEX_FACADE_SIZE); s_tex[GFX_MAT_FACADE0 + i].w = s_tex[GFX_MAT_FACADE0 + i].h = TEX_FACADE_SIZE; }
    s_tex[GFX_MAT_ROAD].rgba = decode_mip565(tex_road, TEX_ROAD_SIZE); s_tex[GFX_MAT_ROAD].w = s_tex[GFX_MAT_ROAD].h = TEX_ROAD_SIZE;
    {
        static const uint8_t *const pt[TEX_P_N] = { tex_p_facade0, tex_p_facade1, tex_p_facade2, tex_p_facade3, tex_p_road };
        static const uint16_t *const pp[TEX_P_N] = { pal_facade0, pal_facade1, pal_facade2, pal_facade3, pal_road };
        for (i = 0; i < TEX_P_N; i++) {
            tex_t *t = &s_tex[GFX_MAT_P_FACADE0 + i];
            t->rgba = decode_pal4(pt[i], pp[i], TEX_P_SIZE); t->w = t->h = TEX_P_SIZE; t->nearest = 1;
        }
    }
    s_tex[GFX_MAT_GLOW].rgba = decode_565_rows(tex_glow, TEX_GLOW_SIZE); s_tex[GFX_MAT_GLOW].w = s_tex[GFX_MAT_GLOW].h = TEX_GLOW_SIZE;
    s_tex[GFX_MAT_FONT].rgba = decode_1555(tex_font, TEX_FONT_SIZE); s_tex[GFX_MAT_FONT].w = s_tex[GFX_MAT_FONT].h = TEX_FONT_SIZE;
    s_t = &s_scr;
}
void gfx_be_shutdown(void) {}

int gfx_be_reflect_resize(unsigned w, unsigned h) {
    target_alloc(&s_rt, (int)w, (int)h);
    free(s_rt_rgba);
    s_rt_rgba = calloc((size_t)w * h, 4);
    s_tex[GFX_MAT_REFLECT].rgba = s_rt_rgba; s_tex[GFX_MAT_REFLECT].w = (int)w; s_tex[GFX_MAT_REFLECT].h = (int)h;
    s_tex[GFX_MAT_REFLECT_ADD] = s_tex[GFX_MAT_REFLECT];
    s_tex[GFX_MAT_SHADOW] = s_tex[GFX_MAT_REFLECT];
    return 0;
}
unsigned gfx_be_reflect_bytes(void) { return (unsigned)(s_rt.w * s_rt.h * 2); }
float gfx_be_reflect_u_scale(void) { return 1.0f; }
int  gfx_be_reflect_begin(void) { s_t = &s_rt; s_translucent = 0; target_clear(s_t, 0, 0, 0); return 0; }
void gfx_be_reflect_end(void) {
    int i;
    for (i = 0; i < s_rt.w * s_rt.h; i++) {        /* through RGB565, as the PVR stores it */
        s_rt_rgba[i * 4] = (uint8_t)(s_rt.rgb[i * 3] & 0xf8u); s_rt_rgba[i * 4 + 1] = (uint8_t)(s_rt.rgb[i * 3 + 1] & 0xfcu);
        s_rt_rgba[i * 4 + 2] = (uint8_t)(s_rt.rgb[i * 3 + 2] & 0xf8u); s_rt_rgba[i * 4 + 3] = 255;
    }
    s_t = &s_scr;
}
static uint32_t s_clear = 0xffc4ced6u;
void gfx_be_clear_colour(uint32_t argb) { s_clear = argb; }
void gfx_be_frame_begin(void) { s_t = &s_scr; s_translucent = 0; target_clear(s_t, (s_clear >> 16) & 255u, (s_clear >> 8) & 255u, s_clear & 255u); }
void gfx_be_list_translucent(void) { s_translucent = 1; }
void gfx_be_frame_end(void) {}
void gfx_be_bind(int mat) { s_mat = mat; }

static void texel(const tex_t *t, float u, float v, int clamp, int bilinear, float out[4]) {
    float fx = u * (float)t->w - 0.5f, fy = v * (float)t->h - 0.5f;
    int x0, y0, i, k;
    float wx, wy, acc[4] = { 0, 0, 0, 0 };
    if (!bilinear) { fx += 0.5f; fy += 0.5f; }
    x0 = (int)floorf(fx); y0 = (int)floorf(fy);
    wx = bilinear ? fx - (float)x0 : 0.0f; wy = bilinear ? fy - (float)y0 : 0.0f;
    for (i = 0; i < 4; i++) {
        int x = x0 + (i & 1), y = y0 + (i >> 1);
        const float w = ((i & 1) ? wx : 1.0f - wx) * ((i >> 1) ? wy : 1.0f - wy);
        if (clamp) { x = x < 0 ? 0 : x >= t->w ? t->w - 1 : x; y = y < 0 ? 0 : y >= t->h ? t->h - 1 : y; }
        else { x = ((x % t->w) + t->w) % t->w; y = ((y % t->h) + t->h) % t->h; }
        for (k = 0; k < 4; k++) acc[k] += w * (float)t->rgba[(y * t->w + x) * 4 + k];
    }
    for (k = 0; k < 4; k++) out[k] = acc[k] * (1.0f / 255.0f);
}

static float edge(float ax, float ay, float bx, float by, float px, float py) { return (bx - ax) * (py - ay) - (by - ay) * (px - ax); }

void gfx_be_tri(const gfx_vert_t *a, const gfx_vert_t *b, const gfx_vert_t *c) {
    const tex_t *tx = s_tex[s_mat].rgba ? &s_tex[s_mat] : NULL;
    const float area = edge(a->x, a->y, b->x, b->y, c->x, c->y);
    const int refl = s_mat == GFX_MAT_REFLECT || s_mat == GFX_MAT_REFLECT_ADD || s_mat == GFX_MAT_SHADOW;
    const int mul = s_mat == GFX_MAT_SHADOW || s_mat == GFX_MAT_MUL;   /* DESTCOLOR x ZERO: the frame times texel x colour */
    const int added = s_mat == GFX_MAT_REFLECT_ADD || s_mat == GFX_MAT_ADD || s_mat == GFX_MAT_GLOW;
    int x0, x1, y0, y1, x, y, k;
    float mnx = fminf(a->x, fminf(b->x, c->x)), mxx = fmaxf(a->x, fmaxf(b->x, c->x));
    float mny = fminf(a->y, fminf(b->y, c->y)), mxy = fmaxf(a->y, fmaxf(b->y, c->y));
    if (area == 0.0f) return;
    if (s_clip_on && (s_mat == GFX_MAT_REFLECT || s_mat == GFX_MAT_REFLECT_ADD)) {     /* the user clip, inside */
        if (mnx < (float)s_clip[0]) mnx = (float)s_clip[0];
        if (mxx > (float)(s_clip[2] - 1)) mxx = (float)(s_clip[2] - 1);
        if (mny < (float)s_clip[1]) mny = (float)s_clip[1];
        if (mxy > (float)(s_clip[3] - 1)) mxy = (float)(s_clip[3] - 1);
        if (mnx > mxx || mny > mxy) return;
    }
    x0 = mnx < 0 ? 0 : (int)mnx; x1 = mxx > (float)(s_t->w - 1) ? s_t->w - 1 : (int)mxx;
    y0 = mny < 0 ? 0 : (int)mny; y1 = mxy > (float)(s_t->h - 1) ? s_t->h - 1 : (int)mxy;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            const float px = (float)x + 0.5f, py = (float)y + 0.5f;
            float l0 = edge(b->x, b->y, c->x, c->y, px, py) / area, l1 = edge(c->x, c->y, a->x, a->y, px, py) / area, l2;
            float z, pa, pb, pc, col[4], off[3], t[4] = { 1, 1, 1, 1 }, out[3], alpha;
            uint8_t *d;
            l2 = 1.0f - l0 - l1;
            if (l0 < 0.0f || l1 < 0.0f || l2 < 0.0f) continue;
            z = l0 * a->z + l1 * b->z + l2 * c->z;
            d = s_t->rgb + (y * s_t->w + x) * 3;
            if (z < s_t->z[y * s_t->w + x]) continue;
            pa = l0 * a->z / z; pb = l1 * b->z / z; pc = l2 * c->z / z;       /* perspective-correct weights */
            for (k = 0; k < 4; k++) {
                const int sh = (3 - k) * 8;      /* k: a r g b from the top byte down -> col[0..3] = a, r, g, b */
                col[k] = (pa * (float)((a->argb >> sh) & 255u) + pb * (float)((b->argb >> sh) & 255u) + pc * (float)((c->argb >> sh) & 255u)) * (1.0f / 255.0f);
            }
            for (k = 0; k < 3; k++) {
                const int sh = (2 - k) * 8;
                off[k] = (pa * (float)((a->oargb >> sh) & 255u) + pb * (float)((b->oargb >> sh) & 255u) + pc * (float)((c->oargb >> sh) & 255u)) * (1.0f / 255.0f);
            }
            if (tx) texel(tx, pa * a->u + pb * b->u + pc * c->u, pa * a->v + pb * b->v + pc * c->v, refl || s_mat == GFX_MAT_FONT || s_mat == GFX_MAT_GLOW,
                          s_mat != GFX_MAT_FONT && !tx->nearest, t);
            alpha = 1.0f;
            if (s_translucent) alpha = s_mat == GFX_MAT_GLOW ? 1.0f : col[0] * (s_mat == GFX_MAT_FONT ? t[3] : 1.0f);
            for (k = 0; k < 3; k++) {
                float v = t[k] * col[k + 1] + (tx && !refl && s_mat != GFX_MAT_FONT ? off[k] : 0.0f);
                v = v > 1.0f ? 1.0f : v;
                if (mul) { out[k] = v * (float)d[k] * (1.0f / 255.0f); continue; }
                out[k] = v * alpha + (float)d[k] * (1.0f / 255.0f) * (added ? 1.0f : 1.0f - alpha);
                out[k] = out[k] > 1.0f ? 1.0f : out[k];
            }
            d[0] = (uint8_t)(out[0] * 255.0f + 0.5f); d[1] = (uint8_t)(out[1] * 255.0f + 0.5f); d[2] = (uint8_t)(out[2] * 255.0f + 0.5f);
            if (!s_translucent) s_t->z[y * s_t->w + x] = z;
        }
}

int gfx_host_save(const char *path, int reflect_target) {
    const target_t *t = reflect_target ? &s_rt : &s_scr;
    FILE *f = fopen(path, "wb");
    if (!f || !t->rgb) return -1;
    fprintf(f, "P6\n%d %d\n255\n", t->w, t->h);
    fwrite(t->rgb, 3, (size_t)t->w * t->h, f);
    fclose(f);
    return 0;
}
