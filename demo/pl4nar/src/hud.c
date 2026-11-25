/* HUD drawing over gfx (2D quads, translucent list). Depth keys 9 .. 10: in front of the world (1 / w <= 1 / GFX_NEAR).
 * SH4ZAM: shz_sincosf (map arrow), shz_saturatef. */
#include "hud.h"
#include "gfx.h"
#include "assets.h"

#define HUD_Z      9.0f
#define CELL_U     (8.0f / (float)TEX_FONT_SIZE)
#define CELL_V     (16.0f / (float)TEX_FONT_SIZE)

static void glyphs(float x, float y, float scale, uint32_t argb, const char *s, float z) {
    for (; *s; s++, x += 8.0f * scale) {
        const unsigned c = (unsigned char)*s;
        if (c <= 32u || c > 126u) continue;
        {
            const float u = (float)((c - 32u) % 16u) * CELL_U, v = (float)((c - 32u) / 16u) * CELL_V;
            gfx_quad2d(x, y, x + 8.0f * scale, y + 16.0f * scale, z, u, v, u + CELL_U, v + CELL_V, argb);
        }
    }
}

void hud_text(float x, float y, float scale, uint32_t argb, const char *s) {
    gfx_bind(GFX_MAT_FONT);
#if !defined(PL4NAR_HILLS) || defined(HUD_TEXT_SHADOW)   /* the hills build: one pass (the shadow pass doubled the text's triangles) */
    glyphs(x + scale, y + scale, scale, 0xc0000000u, s, HUD_Z);
#endif
    glyphs(x, y, scale, argb, s, HUD_Z + 0.5f);
}

void hud_bar(float x, float y, float w, float h, float frac, uint32_t argb, const char *label) {
    gfx_bind(GFX_MAT_HUD);
    gfx_quad2d(x - 2.0f, y - 2.0f, x + w + 2.0f, y + h + 2.0f, HUD_Z, 0.0f, 0.0f, 0.0f, 0.0f, 0xa0000000u);
    gfx_quad2d(x, y, x + w * shz_saturatef(frac), y + h, HUD_Z + 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, argb);
    hud_text(x + w + 8.0f, y - 4.0f, 1.0f, 0xffe0e0e0u, label);
}

void hud_map(float x, float y, float size, float nx, float nz, float heading) {
    const float cx = x + size * 0.5f, road = size * 0.16f;
    const float py = y + size - nz * size, px = cx - nx * road;        /* +X of the world is the left of a +Z street */
    const shz_sincos_t sc = shz_sincosf(heading);
    gfx_vert_t a, b, c;
    unsigned i;
    gfx_bind(GFX_MAT_HUD);
    gfx_quad2d(x - 2.0f, y - 2.0f, x + size + 2.0f, y + size + 2.0f, HUD_Z, 0.0f, 0.0f, 0.0f, 0.0f, 0xa0101418u);
    gfx_quad2d(cx - road, y, cx + road, y + size, HUD_Z + 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0xff505860u);
    for (i = 0; i < 4u; i++) {                                         /* the cross streets */
        const float yy = y + size - (float)i * 0.25f * size;
        gfx_quad2d(x, yy - (i ? 2.0f : 4.0f), x + size, yy + (i ? 2.0f : 0.0f), HUD_Z + 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0xff505860u);
    }
    /* the car: an arrow, nose along the heading (screen up = +Z, screen left = +X) */
    gfx_vert2d(&a, shz_fmaf(sc.sin, -7.0f, px), shz_fmaf(sc.cos, -7.0f, py), HUD_Z + 0.6f, 0.0f, 0.0f, 0xffffe040u);
    gfx_vert2d(&b, shz_fmaf(sc.sin - sc.cos, 4.0f, px), shz_fmaf(sc.sin + sc.cos, 4.0f, py), HUD_Z + 0.6f, 0.0f, 0.0f, 0xffffe040u);
    gfx_vert2d(&c, shz_fmaf(sc.sin + sc.cos, 4.0f, px), shz_fmaf(sc.cos - sc.sin, 4.0f, py), HUD_Z + 0.6f, 0.0f, 0.0f, 0xffffe040u);
    gfx_tri(&a, &b, &c);
}
