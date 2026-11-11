/* SH4DER-PVR: the lens effects, the KallistiOS draw

   sh4der_lens.c
   The effect: adds what a camera lens and film add to a picture: darker
     corners, moving grain, a marker that points at where a hit came from, a
     white flash. None of it reads the frame.
   Developed for: Halo: Dreamcast Evolved.
   Use it for: a horror game's dark corners, film grain in a cutscene, damage
     direction markers in a shooter, lightning or a flash grenade.

   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/* The maths is sh4der_lens.h (host tested). This file is the PVR side: the grain tile (8 KB VRAM, the only memory the lens
   takes), four polygon headers and one vertex strip per overlay. Per frame, TA: the vignette 9 bands x 66 vertices (594, 288
   triangles), the grain 8 vertices, a hit 4 per wedge, the flash 4. Layers: vignette 1 (multiply), grain 2 (multiply, boost), flash
   1 (additive or alpha). Compiles to nothing off the Dreamcast. */

#ifdef _arch_dreamcast

#include <kos.h>
#include <string.h>
#include "sh4der_lens.h"

static pvr_ptr_t grain_mem;
static pvr_poly_hdr_t vig_hdr, grain_mul_hdr, grain_boost_hdr, hit_hdr, flash_add_hdr, flash_alpha_hdr;
static uint8_t lens_ready;       /* 0 not made, 1 made, 2 no VRAM: not tried again until the next PVR start */
static float grain_boost;        /* the boost that gives back the tile's own mean (sh4der_lens_grain_boost) */
static sh4der_lens_spokes_t spokes;   /* the vignette's rim table, set by sh4der_lens_vignette_setup */
static sh4der_lens_vig_t vig;    /* the shape (the screen size) */
static int vig_ok;

/* Write one vertex with pvr_prim. Used on the TR list only. z is the key the TR autosort uses; oargb is the offset colour
   (0 when unused). */
static void vtx(float x, float y, float z, uint32_t argb, uint32_t oargb, uint32_t flags, float u, float v_) {
    pvr_vertex_t p = { 0 };
    p.flags = flags; p.x = x; p.y = y; p.z = z; p.u = u; p.v = v_; p.argb = argb; p.oargb = oargb;
    pvr_prim(&p, sizeof(p));
}

/* Settings shared by every lens header: no depth test or write, no culling, Gouraud. The caller sets the blend. */
static void cxt_lens(pvr_poly_cxt_t *c) {
    c->depth.comparison = PVR_DEPTHCMP_ALWAYS;   /* an overlay: no depth test, no depth write */
    c->depth.write = PVR_DEPTHWRITE_DISABLE;
    c->gen.culling = PVR_CULLING_NONE;
    c->gen.shading = PVR_SHADE_GOURAUD;
}

/* Make the grain tile and the six poly headers once per PVR start. Returns 1 when ready, 0 when VRAM is short; the lens then
   draws what it can. The tile is ARGB4444 texels, loaded one row at a time with pvr_txr_load. The headers, all on the TR list:
   grain multiply, grain boost, vignette multiply, wedge alpha, flash alpha, flash additive. Blends are set at each compile. */
int sh4der_lens_init(void) {
    const uint32_t t = SH4DER_LENS_GRAIN_TILE, texels = t * t;
    uint16_t row[SH4DER_LENS_GRAIN_TILE] __attribute__((aligned(32)));
    uint32_t x, y, sum = 0;
    pvr_poly_cxt_t c;
    if (lens_ready) return lens_ready == 1;
    lens_ready = 2;
    if (!(grain_mem = pvr_mem_malloc(texels * 2u))) {
        printf("LENS: grain off, VRAM %lu KB free\n", (unsigned long)(pvr_mem_available() / 1024u));
        grain_mem = NULL;
    } else {
        for (y = 0; y < t; y++) {
            for (x = 0; x < t; x++) {
                const unsigned lv = sh4der_lens_grain_level(y * t + x, SH4DER_LENS_GRAIN_SEED);
                row[x] = sh4der_lens_grain_argb4444(lv);
                sum += lv;
            }
            pvr_txr_load(row, (uint8_t *)grain_mem + y * sizeof(row), sizeof(row));
        }
        grain_boost = sh4der_lens_grain_boost(SH4DER_LENS_GRAIN_AMP, sh4der_lens_grain_mean(sum, texels));
        pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, t, t, grain_mem, PVR_FILTER_NONE);
        cxt_lens(&c);
        c.blend.src = PVR_BLEND_DESTCOLOR;   /* multiply: frame x texel x vertex colour + offset colour */
        c.blend.dst = PVR_BLEND_ZERO;
        c.txr.env = PVR_TXRENV_MODULATE;
        c.gen.specular = PVR_SPECULAR_ENABLE;   /* the offset colour (1 - a) rides in oargb */
        pvr_poly_compile(&grain_mul_hdr, &c);
    }
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
    lens_ready = 1;
    return 1;
}

/* Forget the VRAM at a PVR start (the reset clears VRAM): the next init makes the tile again. */
void sh4der_lens_forget(void) {
    lens_ready = 0;
    grain_mem = NULL;
    vig_ok = 0;
}

/* Build the vignette's rim table for a w x h screen. Maths only, so any list. Call again when the screen size changes.
   Returns 1 when the table is ready. */
int sh4der_lens_vignette_setup(float w, float h) {
    sh4der_lens_vig_default(&vig, w, h);
    vig_ok = sh4der_lens_vig_spokes(&vig, &spokes);
    return vig_ok;
}

/* Draw the vignette on the TR list. One header (blend src DESTCOLOR, dst ZERO, so the vertex colour multiplies the frame),
   then one strip per band of the rim table. Untextured. Draws nothing until sh4der_lens_vignette_setup has returned 1. */
void sh4der_lens_vignette_draw(void) {
    sh4der_lens_vtx_t band[2 * (SH4DER_LENS_VIG_SPOKES + 1)];
    const unsigned per = 2u * (spokes.n + 1u);
    unsigned b, j, k;
    if (!vig_ok) return;
    pvr_prim((void *)&vig_hdr, sizeof(vig_hdr));
    for (b = 0; b <= vig.rings; b++) {
        if (!sh4der_lens_vig_band(&vig, &spokes, b, band, per)) return;
        for (j = 0; j < per; j++) {
            k = j == per - 1u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            vtx(band[j].x, band[j].y, SH4DER_LENS_Z_VIG, band[j].argb, 0u, k, 0.0f, 0.0f);
        }
    }
}

/* Draw the grain on the TR list: the tile at this frame's offset over the w x h screen. Two quads, both full screen: the
   multiply first (vertex colour is the amplitude a, offset colour is 1 - a), then the boost (vertex colour is the boost).
   Draws nothing when the tile is missing (no VRAM at init). */
void sh4der_lens_grain(uint32_t frame, float w, float h) {
    float uv[4];
    const uint32_t a = (uint32_t)(SH4DER_LENS_GRAIN_AMP * 255.0f + 0.5f);
    const uint32_t g = 0xff000000u | (a << 16) | (a << 8) | a;             /* vertex colour: the amplitude a */
    const uint32_t o = 0xff000000u | ((255u - a) << 16) | ((255u - a) << 8) | (255u - a);   /* offset: 1 - a */
    const uint32_t bo = sh4der_lens_argb_(1.0f, grain_boost, grain_boost, grain_boost);
    if (!grain_mem) return;
    sh4der_lens_grain_uv(frame, w, h, uv);
    pvr_prim((void *)&grain_mul_hdr, sizeof(grain_mul_hdr));
    vtx(0.0f, 0.0f, SH4DER_LENS_Z_GRAIN, g, o, PVR_CMD_VERTEX, uv[0], uv[1]);
    vtx(w, 0.0f, SH4DER_LENS_Z_GRAIN, g, o, PVR_CMD_VERTEX, uv[2], uv[1]);
    vtx(0.0f, h, SH4DER_LENS_Z_GRAIN, g, o, PVR_CMD_VERTEX, uv[0], uv[3]);
    vtx(w, h, SH4DER_LENS_Z_GRAIN, g, o, PVR_CMD_VERTEX_EOL, uv[2], uv[3]);
    pvr_prim((void *)&grain_boost_hdr, sizeof(grain_boost_hdr));
    vtx(0.0f, 0.0f, SH4DER_LENS_Z_GRAIN, bo, 0u, PVR_CMD_VERTEX, 0.0f, 0.0f);
    vtx(w, 0.0f, SH4DER_LENS_Z_GRAIN, bo, 0u, PVR_CMD_VERTEX, 0.0f, 0.0f);
    vtx(0.0f, h, SH4DER_LENS_Z_GRAIN, bo, 0u, PVR_CMD_VERTEX, 0.0f, 0.0f);
    vtx(w, h, SH4DER_LENS_Z_GRAIN, bo, 0u, PVR_CMD_VERTEX_EOL, 0.0f, 0.0f);
}

/* Draw the hit wedges (damage indicators) on the TR list with alpha blend (src SRCALPHA, dst INVSRCALPHA). One quad per
   slot that is on; the alpha comes from the slot's age. Slots that are off are skipped. */
void sh4der_lens_hits_draw(const sh4der_lens_hit_t *slots, unsigned n, float w, float h) {
    unsigned i, j;
    sh4der_lens_vtx_t q[4];
    pvr_prim((void *)&hit_hdr, sizeof(hit_hdr));
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
    pvr_prim((void *)(additive ? &flash_add_hdr : &flash_alpha_hdr), sizeof(flash_add_hdr));
    vtx(0.0f, 0.0f, SH4DER_LENS_Z_FLASH, c, 0u, PVR_CMD_VERTEX, 0.0f, 0.0f);
    vtx(w, 0.0f, SH4DER_LENS_Z_FLASH, c, 0u, PVR_CMD_VERTEX, 0.0f, 0.0f);
    vtx(0.0f, h, SH4DER_LENS_Z_FLASH, c, 0u, PVR_CMD_VERTEX, 0.0f, 0.0f);
    vtx(w, h, SH4DER_LENS_Z_FLASH, c, 0u, PVR_CMD_VERTEX_EOL, 0.0f, 0.0f);
}

#endif /* _arch_dreamcast */
