/* SH4DER-PVR: the lens effects, the KallistiOS draw

   sh4der_lens.c
   Version: v0.10
   The effect: adds what a camera lens and film add to a picture: darker
     corners, moving grain, a marker that points at where a hit came from, a
     white flash. None of it reads the frame.
   Developed for: a first-person shooter.
   Use it for: a horror game's dark corners, film grain in a cutscene, damage
     direction markers in a shooter, lightning or a flash grenade.

   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/* The maths is sh4der_lens.h (host tested). This file is the PVR side: the first form's grain tile (8 KB VRAM, the only
   memory the lens takes by itself), five untextured polygon headers (vignette, grain boost, hit, flash additive, flash
   alpha), a textured multiply header a grain, and one vertex strip per overlay. Per frame, TA: the vignette 9 bands x 66
   vertices (594, 288 triangles), the grain 8 vertices, a hit 4 per wedge, the flash 4. Layers: vignette 1 (multiply),
   grain 2 (multiply, boost; 1 with neutral off), flash 1 (additive or alpha). A grain of the caller's own
   (sh4der_lens_grain_t) carries its own header and tile. Every draw checks its header was compiled before it sends
   it (sh4der_guard.h): a draw before its setup sends nothing. Compiles to nothing off the Dreamcast. */

#ifdef _arch_dreamcast

#include <kos.h>
#include <string.h>
#include "sh4der_lens.h"
#include "sh4der_pvrx.h"
#include "sh4der_guard.h"

_Static_assert(sizeof(sh4der_lens_grain_t) == 64, "sh4der_lens_grain_t is 64 bytes");
_Static_assert(sizeof(pvr_poly_hdr_t) == 32, "a polygon header is 32 bytes");

static sh4der_lens_grain_t grain0;   /* the first form's grain: 64 x 64, MONO, SH4DER_LENS_GRAIN_SEED, JUMP */
static pvr_poly_hdr_t vig_hdr, grain_boost_hdr, hit_hdr, flash_add_hdr, flash_alpha_hdr;
static uint8_t hdrs_ready;       /* the five untextured headers are compiled */
static uint8_t lens_ready;       /* 0 not made, 1 made (with or without the tile) */
static sh4der_lens_spokes_t spokes;   /* the vignette's rim table, set by sh4der_lens_vignette_setup or _shape */
static sh4der_lens_vig_t vig;    /* the shape */
static int vig_ok;

/* Write one vertex with pvr_prim. Used on the TR list only. z is the key the TR autosort uses; oargb is the offset colour
   (0 when unused). */
static void vtx(float x, float y, float z, uint32_t argb, uint32_t oargb, uint32_t flags, float u, float v_) {
    pvr_vertex_t p = { 0 };
    p.flags = flags; p.x = x; p.y = y; p.z = z; p.u = u; p.v = v_; p.argb = argb; p.oargb = oargb;
    pvr_prim(&p, sizeof(p));
}

/* Send a polygon header after checking it was compiled (a zeroed header is "end of list" to the tile accelerator and
   hangs a real console). h: 32 bytes, 32-byte aligned. Returns 1 when sent; on 0 the caller sends no vertices. */
static int hdr_send(const void *h) {
    uint32_t words[8];
    memcpy(words, h, sizeof(words));                 /* word access through a copy: no cast of a wider pointer */
    if (!sh4der_poly_hdr_ok(words)) return 0;
    pvr_prim((void *)h, 32);
    return 1;
}

/* Settings shared by every lens header: no depth test or write, no culling, Gouraud. The caller sets the blend. */
static void cxt_lens(pvr_poly_cxt_t *c) {
    c->depth.comparison = PVR_DEPTHCMP_ALWAYS;   /* an overlay: no depth test, no depth write */
    c->depth.write = PVR_DEPTHWRITE_DISABLE;
    c->gen.culling = PVR_CULLING_NONE;
    c->gen.shading = PVR_SHADE_GOURAUD;
}

/* The five untextured headers, all on the TR list, once: grain boost, vignette multiply, wedge alpha, flash alpha, flash
   additive. No video memory. */
void sh4der_lens_init_headers(void) {
    pvr_poly_cxt_t c;
    if (hdrs_ready) return;
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    cxt_lens(&c);
    c.blend.src = PVR_BLEND_DESTCOLOR;   /* frame x (1 + colour): the grain's mean back */
    c.blend.dst = PVR_BLEND_ONE;
    pvr_poly_compile(&grain_boost_hdr, &c);
    c.blend.src = PVR_BLEND_DESTCOLOR;   /* multiply by the vertex colour */
    c.blend.dst = PVR_BLEND_ZERO;
    pvr_poly_compile(&vig_hdr, &c);
    c.blend.src = PVR_BLEND_SRCALPHA;    /* the wedge: alpha on the edge, 0 inside */
    c.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&hit_hdr, &c);
    c.blend.src = PVR_BLEND_SRCALPHA;    /* the flash: alpha */
    c.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&flash_alpha_hdr, &c);
    c.blend.src = PVR_BLEND_ONE;         /* the flash: additive */
    c.blend.dst = PVR_BLEND_ONE;
    pvr_poly_compile(&flash_add_hdr, &c);
    hdrs_ready = 1;
}

/* A grain's multiply header for a tile in video memory: frame x (texel x vertex colour + offset colour). ARGB4444, not
   twiddled, repeating, unfiltered: a texel to px pixels. */
static void grain_header(sh4der_lens_grain_t *g) {
    pvr_poly_hdr_t h;
    pvr_poly_cxt_t c;
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, g->tile, g->tile, (pvr_ptr_t)g->tex, PVR_FILTER_NONE);
    cxt_lens(&c);
    c.blend.src = PVR_BLEND_DESTCOLOR;   /* multiply: frame x texel x vertex colour + offset colour */
    c.blend.dst = PVR_BLEND_ZERO;
    c.txr.env = PVR_TXRENV_MODULATE;
    c.gen.specular = PVR_SPECULAR_ENABLE;   /* the offset colour (1 - a) rides in oargb */
    pvr_poly_compile(&h, &c);
    memcpy(g->hdr, &h, sizeof(g->hdr));
}

/* Zero a grain and take the caller's size: 0 for a bad one. */
static int grain_begin(sh4der_lens_grain_t *g, unsigned tile, unsigned px) {
    if (!g) return 0;
    memset(g, 0, sizeof(*g));
    if (!sh4der_lens_grain_tile_ok(tile) || px < 1u || px > 2u) return 0;
    g->tile = (uint16_t)tile; g->px = (uint8_t)px;
    g->move = SH4DER_LENS_MOVE_JUMP; g->step = 1u;
    g->mean[0] = g->mean[1] = g->mean[2] = 0.5f;
    return 1;
}

/* Allocate the tile and upload it a row at a time from a 32-byte aligned row on the stack: the made texels of `kind`
   (texels NULL), or the caller's. Fills the mean. 0 (nothing kept) when video memory is short. */
static int grain_upload(sh4der_lens_grain_t *g, const uint16_t *texels, uint32_t seed, unsigned kind) {
    const uint32_t t = g->tile;
    uint16_t row[SH4DER_LENS_GRAIN_TILE_MAX] __attribute__((aligned(32)));
    uint32_t sum[3] = { 0u, 0u, 0u }, x, y, c;
    if (!(g->tex = pvr_mem_malloc(t * t * 2u))) return 0;
    for (y = 0; y < t; y++) {
        for (x = 0; x < t; x++) {
            unsigned lv[3];
            if (texels) {
                row[x] = texels[y * t + x];
                lv[0] = (row[x] >> 8) & 15u; lv[1] = (row[x] >> 4) & 15u; lv[2] = row[x] & 15u;
            } else {
                row[x] = sh4der_lens_grain_texel(y * t + x, seed, kind, lv);
            }
            sum[0] += lv[0]; sum[1] += lv[1]; sum[2] += lv[2];
        }
        pvr_txr_load(row, (uint8_t *)g->tex + y * t * 2u, t * 2u);
    }
    for (c = 0; c < 3u; c++) g->mean[c] = sh4der_lens_grain_mean(sum[c], t * t);
    g->owned = 1u;
    return 1;
}

int sh4der_lens_grain_make(sh4der_lens_grain_t *g, unsigned tile, unsigned px, uint32_t seed, unsigned kind) {
    if (!grain_begin(g, tile, px) || kind > SH4DER_LENS_GRAIN_SOFT) return 0;
    g->kind = (uint8_t)kind;
    if (!grain_upload(g, NULL, seed, kind)) { memset(g, 0, sizeof(*g)); return 0; }
    sh4der_lens_init_headers();
    grain_header(g);
    g->ready = 1u;
    return 1;
}

int sh4der_lens_grain_load(sh4der_lens_grain_t *g, const uint16_t *texels, unsigned tile, unsigned px) {
    if (!grain_begin(g, tile, px) || !texels) return 0;
    if (!grain_upload(g, texels, 0u, SH4DER_LENS_GRAIN_MONO)) { memset(g, 0, sizeof(*g)); return 0; }
    sh4der_lens_init_headers();
    grain_header(g);
    g->ready = 1u;
    return 1;
}

int sh4der_lens_grain_use(sh4der_lens_grain_t *g, void *tex, unsigned tile, unsigned px, const float mean[3]) {
    if (!grain_begin(g, tile, px) || !tex) return 0;
    g->tex = tex;
    if (mean) { g->mean[0] = mean[0]; g->mean[1] = mean[1]; g->mean[2] = mean[2]; }
    sh4der_lens_init_headers();
    grain_header(g);
    g->ready = 1u;
    return 1;
}

void sh4der_lens_grain_move(sh4der_lens_grain_t *g, unsigned move, unsigned step) {
    if (!g || move > SH4DER_LENS_MOVE_STILL) return;
    g->move = (uint8_t)move;
    g->step = (uint8_t)(step > 255u ? 255u : step);
}

unsigned sh4der_lens_grain_draw(const sh4der_lens_grain_t *g, uint32_t frame, float x0, float y0, float x1, float y1, float z, float amp, int neutral) {
    float uv[4];
    uint32_t a, o, bo;
    if (!g || !g->ready || !(x1 > x0) || !(y1 > y0)) return 0;
    sh4der_lens_grain_colours(amp, g->mean, &a, &o, &bo);
    sh4der_lens_grain_uv_ex(g->move, frame, g->step, g->tile, g->px, x1 - x0, y1 - y0, uv);
    if (!hdr_send(g->hdr)) return 0;
    vtx(x0, y0, z, a, o, PVR_CMD_VERTEX, uv[0], uv[1]);
    vtx(x1, y0, z, a, o, PVR_CMD_VERTEX, uv[2], uv[1]);
    vtx(x0, y1, z, a, o, PVR_CMD_VERTEX, uv[0], uv[3]);
    vtx(x1, y1, z, a, o, PVR_CMD_VERTEX_EOL, uv[2], uv[3]);
    if (!neutral || !hdr_send(&grain_boost_hdr)) return 2u;
    vtx(x0, y0, z, bo, 0u, PVR_CMD_VERTEX, 0.0f, 0.0f);
    vtx(x1, y0, z, bo, 0u, PVR_CMD_VERTEX, 0.0f, 0.0f);
    vtx(x0, y1, z, bo, 0u, PVR_CMD_VERTEX, 0.0f, 0.0f);
    vtx(x1, y1, z, bo, 0u, PVR_CMD_VERTEX_EOL, 0.0f, 0.0f);
    return 4u;
}

void sh4der_lens_grain_free(sh4der_lens_grain_t *g) {
    if (!g) return;
    if (g->owned && g->tex) {
        sh4der_pvrx_wait_idle();   /* the last scene's lists are with the renderer, and it has finished: reading the tile (the client's hook decides sleep or poll) */
        pvr_mem_free((pvr_ptr_t)g->tex);
    }
    memset(g, 0, sizeof(*g));
}

void sh4der_lens_grain_forget(sh4der_lens_grain_t *g) {
    if (g) memset(g, 0, sizeof(*g));
}

/* Make the first form's grain tile and the headers once per PVR start. Returns 1 once the headers are compiled, also when
   video memory was short and there is no tile (the lens then draws what it can, without grain; sh4der_lens_ready says
   which). The tile is ARGB4444 texels, loaded one row at a time with pvr_txr_load. */
int sh4der_lens_init(void) {
    if (lens_ready) return 1;
    if (!sh4der_lens_grain_make(&grain0, SH4DER_LENS_GRAIN_TILE, SH4DER_LENS_GRAIN_PX, SH4DER_LENS_GRAIN_SEED, SH4DER_LENS_GRAIN_MONO))
        printf("LENS: grain off, VRAM %lu KB free\n", (unsigned long)(pvr_mem_available() / 1024u));
    sh4der_lens_init_headers();
    lens_ready = 1;
    return 1;
}

int sh4der_lens_ready(void) { return lens_ready && grain0.ready; }

/* Forget the VRAM at a PVR start (the reset clears VRAM): the next init makes the tile again. */
void sh4der_lens_forget(void) {
    lens_ready = 0;
    sh4der_lens_grain_forget(&grain0);
    vig_ok = 0;
}

/* Build the vignette's rim table for a w x h screen. Maths only, so any list. Call again when the screen size changes.
   Returns 1 when the table is ready. */
int sh4der_lens_vignette_setup(float w, float h) {
    sh4der_lens_vig_default(&vig, w, h);
    vig_ok = sh4der_lens_vig_spokes(&vig, &spokes);
    return vig_ok;
}

int sh4der_lens_vignette_shape(const sh4der_lens_vig_t *shape) {
    if (!shape) return 0;
    vig = *shape;
    vig_ok = sh4der_lens_vig_spokes(&vig, &spokes);
    return vig_ok;
}

/* The vignette on the TR list. One header (blend src DESTCOLOR, dst ZERO, so the vertex colour multiplies the frame),
   then one strip per band of the rim table. Untextured. moved: add (ox, oy); the first form adds nothing. */
static unsigned vignette(int moved, float ox, float oy, float z) {
    sh4der_lens_vtx_t band[2 * (SH4DER_LENS_SPOKES_MAX + 1)];
    const unsigned per = 2u * (spokes.n + 1u);
    unsigned b, j, k, tris = 0;
    if (!vig_ok || !hdr_send(&vig_hdr)) return 0;
    for (b = 0; b <= vig.rings; b++) {
        if (!sh4der_lens_vig_band(&vig, &spokes, b, band, per)) return tris;
        for (j = 0; j < per; j++) {
            k = j == per - 1u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            if (moved) vtx(band[j].x + ox, band[j].y + oy, z, band[j].argb, 0u, k, 0.0f, 0.0f);
            else vtx(band[j].x, band[j].y, z, band[j].argb, 0u, k, 0.0f, 0.0f);
        }
        tris += per - 2u;
    }
    return tris;
}

/* Draw the vignette on the TR list. Draws nothing until sh4der_lens_vignette_setup (or _shape) has returned 1 and the
   headers are compiled (sh4der_lens_init). */
void sh4der_lens_vignette_draw(void) { (void)vignette(0, 0.0f, 0.0f, SH4DER_LENS_Z_VIG); }

unsigned sh4der_lens_vignette_draw_at(float ox, float oy, float z) { return vignette(1, ox, oy, z); }

unsigned sh4der_lens_vignette_mesh_draw(const sh4der_lens_vtx_t *mesh, unsigned verts, unsigned per, float ox, float oy, float z) {
    unsigned i, k;
    if (!mesh || per < 4u || !verts || verts % per || !hdr_send(&vig_hdr)) return 0;
    for (i = 0; i < verts; i += per)
        for (k = 0; k < per; k++)
            vtx(mesh[i + k].x + ox, mesh[i + k].y + oy, z, mesh[i + k].argb, 0u, k == per - 1u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX, 0.0f, 0.0f);
    return (verts / per) * (per - 2u);
}

/* Draw the grain on the TR list: the first form's tile at this frame's offset over the w x h screen. Two quads, both full
   screen: the multiply first (vertex colour is the amplitude a, offset colour is 1 - a), then the boost (vertex colour is
   the boost). Draws nothing when the tile is missing (no VRAM at init, or no init). */
void sh4der_lens_grain(uint32_t frame, float w, float h) {
    (void)sh4der_lens_grain_draw(&grain0, frame, 0.0f, 0.0f, w, h, SH4DER_LENS_Z_GRAIN, SH4DER_LENS_GRAIN_AMP, 1);
}

/* Draw the hit wedges (damage indicators) on the TR list with alpha blend (src SRCALPHA, dst INVSRCALPHA). One quad per
   slot that is on; the alpha comes from the slot's age. Slots that are off are skipped. */
void sh4der_lens_hits_draw(const sh4der_lens_hit_t *slots, unsigned n, float w, float h) {
    unsigned i, j;
    sh4der_lens_vtx_t q[4];
    if (!hdr_send(&hit_hdr)) return;
    for (i = 0; i < n; i++) {
        if (!slots[i].on) continue;
        sh4der_lens_hit_quad(&slots[i], w, h, sh4der_lens_hit_alpha(slots[i].age), q);
        for (j = 0; j < 4u; j++)
            vtx(q[j].x, q[j].y, SH4DER_LENS_Z_HIT, q[j].argb, 0u, j == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX, 0.0f, 0.0f);
    }
}

/* Draw the flash on the TR list: one full-screen quad at its level and colour rgb. additive picks the ONE / ONE header,
   otherwise the alpha header. Nothing is drawn when the level is 0. */
void sh4der_lens_flash_draw(const sh4der_lens_flash_t *f, const float rgb[3], int additive, float w, float h) {
    const uint32_t c = sh4der_lens_flash_argb(f->level, rgb, additive);
    if (!(f->level > 0.0f)) return;
    if (!hdr_send(additive ? &flash_add_hdr : &flash_alpha_hdr)) return;
    vtx(0.0f, 0.0f, SH4DER_LENS_Z_FLASH, c, 0u, PVR_CMD_VERTEX, 0.0f, 0.0f);
    vtx(w, 0.0f, SH4DER_LENS_Z_FLASH, c, 0u, PVR_CMD_VERTEX, 0.0f, 0.0f);
    vtx(0.0f, h, SH4DER_LENS_Z_FLASH, c, 0u, PVR_CMD_VERTEX, 0.0f, 0.0f);
    vtx(w, h, SH4DER_LENS_Z_FLASH, c, 0u, PVR_CMD_VERTEX_EOL, 0.0f, 0.0f);
}

#endif /* _arch_dreamcast */
