/* SH4DER-PVR: fake depth of field

   sh4der_focus.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_focus.h
    \brief   Fake depth of field: a blurred copy of the frame drawn over the
             sharp one through a vertex-alpha mesh that keeps a region sharp.

    The PVR has no depth read-back, so there is no real depth of field. This
    header is the maths for the fake:

    - The blurred copy: the grid sh4der_look reads (SH4DER_FOCUS_SRC_W x _H, RGB565,
      the same samples, so no second read) is box-blurred on
      the CPU (sh4der_focus_box565) into a power-of-two texture
      (SH4DER_FOCUS_TEX_W x _H, linear, RGB565). The grid sits in its top-left.
    - The mask: sh4der_focus_sharp is 1 where the frame stays sharp (a disc
      for a scope or zoom, a band for tilt-shift, a rect for a close-up
      subject) and falls to 0 over SH4DER_FOCUS_FEATHER pixels.
    - The mesh: sh4der_focus_vertex gives one vertex of a strip mesh over the
      frame. Its alpha is the blur amount, 1 - sharp, so the translucent
      textured mesh is blurred where the mask is 0 and sharp where it is 1.
      It is drawn with blend SRC_ALPHA, INV_SRC_ALPHA over the sharp frame.

    Order, per frame: sh4der_focus_box565 on the grid, sh4der_focus_upload,
    then sh4der_focus_submit.

    The client draws the mesh (src/sh4der_focus.c, Dreamcast only) after the
    frame and before the HUD.

    Version: v0.10
    The effect: keeps one part of the picture sharp and lets the rest go soft,
      as a camera focused on a face does. The soft part is a small blurred copy
      of the frame.
    Developed for: close-ups in Halo: Dreamcast Evolved's cinematics.
    Use it for: dialogue close-ups, a blurred game behind a pause menu, the
      soft rim of a sniper scope, a tilt-shift toy look.

    \author Jilted Valkyrie
    \author Cortana (Claude, an AI by Anthropic): the effects' maths, the PVR paths, the tests
*/

#ifndef SH4DER_FOCUS_H
#define SH4DER_FOCUS_H

#include <stdint.h>
#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_FOCUS_SRC_W     80      /**< \brief Sample grid across (the grid sh4der_look reads). */
#define SH4DER_FOCUS_SRC_H     60      /**< \brief Sample grid down. */
#define SH4DER_FOCUS_TEX_W     128     /**< \brief Blur texture width (PVR: power of 2; the grid is the top-left). */
#define SH4DER_FOCUS_TEX_H     64      /**< \brief Blur texture height (power of 2). */
#define SH4DER_FOCUS_GRID_X    16      /**< \brief Mesh cells across the frame. */
#define SH4DER_FOCUS_GRID_Y    12      /**< \brief Mesh cells down the frame. */
#define SH4DER_FOCUS_SCREEN_W  640.0f  /**< \brief Frame width, pixels. */
#define SH4DER_FOCUS_SCREEN_H  480.0f  /**< \brief Frame height, pixels. */
#define SH4DER_FOCUS_FEATHER   48.0f   /**< \brief Mask falloff, pixels. */
#define SH4DER_FOCUS_MAX       1.0f    /**< \brief Blur at full (0..1). */
#define SH4DER_FOCUS_DISC_R    120.0f  /**< \brief Scope disc radius, pixels. */
#define SH4DER_FOCUS_BAND_H    60.0f   /**< \brief Tilt-shift band half height, pixels. */
#define SH4DER_FOCUS_RECT_W    100.0f  /**< \brief Subject rect half width, pixels. */
#define SH4DER_FOCUS_RECT_H    80.0f   /**< \brief Subject rect half height, pixels. */
#define SH4DER_FOCUS_Z         1.0f    /**< \brief Mesh depth (the list's sort value is the client's). */
/** @} */

/** \name Sizes
    @{ */
#define SH4DER_FOCUS_TEX_BYTES (SH4DER_FOCUS_TEX_W * SH4DER_FOCUS_TEX_H * 2)   /**< \brief Blur texture, RGB565: 16384 B. */
#define SH4DER_FOCUS_ROW_VERTS (2 * (SH4DER_FOCUS_GRID_X + 1))                  /**< \brief Vertices per strip (one row). */
#define SH4DER_FOCUS_VERTS     (SH4DER_FOCUS_ROW_VERTS * SH4DER_FOCUS_GRID_Y)   /**< \brief Strip vertices per frame: 408. */
#define SH4DER_FOCUS_TRIS      (2 * SH4DER_FOCUS_GRID_X * SH4DER_FOCUS_GRID_Y) /**< \brief Triangles per frame: 384. */
/** @} */

enum {
    SH4DER_FOCUS_DISC,   /**< \brief Sharp inside a disc: scope, zoom. */
    SH4DER_FOCUS_BAND,   /**< \brief Sharp inside a horizontal band: tilt-shift. */
    SH4DER_FOCUS_RECT    /**< \brief Sharp inside a rect: close-up subject. */
};

/** \brief The sharp region. Disc: hw = radius. Band: hh = half height. Rect: hw, hh = half sizes. Centre (cx, cy) in pixels. */
typedef struct sh4der_focus_mask {
    int shape;                  /**< \brief SH4DER_FOCUS_DISC, _BAND or _RECT. */
    float cx, cy;               /**< \brief Centre, screen pixels. */
    float hw, hh;               /**< \brief Disc: hw is the radius. Band: hh is the half height. Rect: the half sizes. Pixels. */
    float feather;   /**< \brief Falloff width, pixels (> 0). */
} sh4der_focus_mask_t;

/** \brief The default mask for `shape` (SH4DER_FOCUS_DISC, _BAND or _RECT), centred on the frame, at the shipped sizes and feather. */
static inline void sh4der_focus_mask_default(sh4der_focus_mask_t *m, int shape) {
    m->shape = shape;
    m->cx = SH4DER_FOCUS_SCREEN_W * 0.5f;
    m->cy = SH4DER_FOCUS_SCREEN_H * 0.5f;
    m->hw = shape == SH4DER_FOCUS_BAND ? 0.0f : shape == SH4DER_FOCUS_RECT ? SH4DER_FOCUS_RECT_W : SH4DER_FOCUS_DISC_R;
    m->hh = shape == SH4DER_FOCUS_BAND ? SH4DER_FOCUS_BAND_H : shape == SH4DER_FOCUS_RECT ? SH4DER_FOCUS_RECT_H : 0.0f;
    m->feather = SH4DER_FOCUS_FEATHER;
}

/** \brief The sharp weight 0..1 at screen pixel (x, y), y down from the top left. Each shape is a column feather (sh4der_feather_) on the squared distance. */
static inline float sh4der_focus_sharp(const sh4der_focus_mask_t *m, float x, float y) {
    const float dx = x - m->cx, dy = y - m->cy;
    switch (m->shape) {
        case SH4DER_FOCUS_DISC:
            return sh4der_feather_(sh4der_len2_(dx, dy), m->hw, m->hw + m->feather);
        case SH4DER_FOCUS_BAND:
            return sh4der_feather_(dy * dy, m->hh, m->hh + m->feather);
        default:
            return sh4der_feather_(dx * dx, m->hw, m->hw + m->feather) * sh4der_feather_(dy * dy, m->hh, m->hh + m->feather);
    }
}

/** \brief A vertex of the focus mesh. eol: last vertex of its strip (one grid row). */
typedef struct sh4der_focus_vtx {
    float x, y, u, v;   /**< \brief Screen pixels (x, y), then blur texture coordinates (u, v). At the far edge (u, v) reach SRC / TEX, not 1. */
    uint32_t argb;   /**< \brief White, alpha = blur amount (ARGB8888). */
    int eol;         /**< \brief 1: last vertex of its grid row. */
} sh4der_focus_vtx_t;

/** \brief Vertex k (0 or 1: grid row j or j + 1) of column i in row j. Strips go (i, j), (i, j + 1), (i + 1, j) ... so the
    strip is a triangle strip; the last vertex of each row is eol. */
static inline void sh4der_focus_vertex(const sh4der_focus_mask_t *m, unsigned i, unsigned j, unsigned k, sh4der_focus_vtx_t *v) {
    const float cw = SH4DER_FOCUS_SCREEN_W * (1.0f / SH4DER_FOCUS_GRID_X);
    const float ch = SH4DER_FOCUS_SCREEN_H * (1.0f / SH4DER_FOCUS_GRID_Y);
    const float x = (float)i * cw, y = (float)(j + k) * ch;
    const float blur = SH4DER_FOCUS_MAX * (1.0f - sh4der_focus_sharp(m, x, y));
    const uint32_t a = (uint32_t)(sh4der_sat_(blur) * 255.0f + 0.5f);
    v->x = x;
    v->y = y;
    /* texel = screen pixel x SRC / SCREEN, over the texture size */
    v->u = x * ((float)SH4DER_FOCUS_SRC_W / SH4DER_FOCUS_SCREEN_W) * (1.0f / SH4DER_FOCUS_TEX_W);
    v->v = y * ((float)SH4DER_FOCUS_SRC_H / SH4DER_FOCUS_SCREEN_H) * (1.0f / SH4DER_FOCUS_TEX_H);
    v->argb = (a << 24) | 0x00ffffffu;
    v->eol = (i == SH4DER_FOCUS_GRID_X && k == 1);
}

/** \brief The whole mesh into `out` (SH4DER_FOCUS_VERTS): host check and reference for the client's loop. Returns the count. */
static inline unsigned sh4der_focus_mesh(const sh4der_focus_mask_t *m, sh4der_focus_vtx_t out[SH4DER_FOCUS_VERTS]) {
    unsigned i, j, k, n = 0;
    for (j = 0; j < SH4DER_FOCUS_GRID_Y; j++)
        for (i = 0; i <= SH4DER_FOCUS_GRID_X; i++)
            for (k = 0; k < 2; k++)
                sh4der_focus_vertex(m, i, j, k, &out[n++]);
    return n;
}

/** \brief /9 of a 3x3 sum (sum at most 567): ((t + 4) x 7282) >> 16, exact on 0..567 (test). No divide: SH-4 divides in software. */
static inline unsigned sh4der_focus_div9_(unsigned t) { return ((t + 4u) * 7282u) >> 16; }

/** \brief One row's horizontal 3-tap sums per channel (r 5 bits, g 6 bits, b 5 bits, edge clamped). */
static inline void sh4der_focus_hsum_(const uint16_t *src, unsigned y, uint16_t out[SH4DER_FOCUS_SRC_W][3]) {
    const uint16_t *row = src + y * SH4DER_FOCUS_SRC_W;
    unsigned x;
    for (x = 0; x < SH4DER_FOCUS_SRC_W; x++) {
        const unsigned xm = x ? x - 1 : 0, xp = x + 1 < SH4DER_FOCUS_SRC_W ? x + 1 : SH4DER_FOCUS_SRC_W - 1;
        const unsigned a = row[xm], b = row[x], c = row[xp];
        out[x][0] = (uint16_t)((a >> 11) + (b >> 11) + (c >> 11));
        out[x][1] = (uint16_t)(((a >> 5) & 63u) + ((b >> 5) & 63u) + ((c >> 5) & 63u));
        out[x][2] = (uint16_t)((a & 31u) + (b & 31u) + (c & 31u));
    }
}

/** \brief Copy the grid's edge into the texture's padding (column SRC_W.., row SRC_H..) so bilinear sampling at the grid's
    edge reads the edge, not garbage. dst is TEX_W x TEX_H RGB565. */
static inline void sh4der_focus_pad_(uint16_t *dst) {
    unsigned x, y;
    for (y = 0; y < SH4DER_FOCUS_SRC_H; y++)
        for (x = SH4DER_FOCUS_SRC_W; x < SH4DER_FOCUS_TEX_W; x++)
            dst[y * SH4DER_FOCUS_TEX_W + x] = dst[y * SH4DER_FOCUS_TEX_W + SH4DER_FOCUS_SRC_W - 1];
    for (y = SH4DER_FOCUS_SRC_H; y < SH4DER_FOCUS_TEX_H; y++)
        for (x = 0; x < SH4DER_FOCUS_TEX_W; x++)
            dst[y * SH4DER_FOCUS_TEX_W + x] = dst[(SH4DER_FOCUS_SRC_H - 1) * SH4DER_FOCUS_TEX_W + x];
}

/** \brief The 3x3 box blur of the grid `src` (SRC_W x SRC_H RGB565, row-major) into `dst` (TEX_W x TEX_H, the grid in its
    top-left, padded). Separable: three row sums in a ring (RAM 1440 B), then the vertical sum. Edge clamped. */
static inline void sh4der_focus_box565(const uint16_t *src, uint16_t *dst) {
    uint16_t hs[3][SH4DER_FOCUS_SRC_W][3];
    unsigned x, y;
    sh4der_focus_hsum_(src, 0, hs[0]);
    for (y = 0; y < SH4DER_FOCUS_SRC_H; y++) {
        const unsigned yp = y + 1 < SH4DER_FOCUS_SRC_H ? y + 1 : SH4DER_FOCUS_SRC_H - 1;
        const unsigned ym = y ? y - 1 : 0;
        sh4der_focus_hsum_(src, yp, hs[yp % 3]);
        for (x = 0; x < SH4DER_FOCUS_SRC_W; x++) {
            const unsigned r = hs[ym % 3][x][0] + hs[y % 3][x][0] + hs[yp % 3][x][0];
            const unsigned g = hs[ym % 3][x][1] + hs[y % 3][x][1] + hs[yp % 3][x][1];
            const unsigned b = hs[ym % 3][x][2] + hs[y % 3][x][2] + hs[yp % 3][x][2];
            dst[y * SH4DER_FOCUS_TEX_W + x] = (uint16_t)((sh4der_focus_div9_(r) << 11) | (sh4der_focus_div9_(g) << 5) | sh4der_focus_div9_(b));
        }
    }
    sh4der_focus_pad_(dst);
}

/** \brief The sampled part's uv max (grid / texture), for a client that draws a flat quad instead of the mesh. */
static inline float sh4der_focus_uv_max_u(void) { return (float)SH4DER_FOCUS_SRC_W / (float)SH4DER_FOCUS_TEX_W; }
static inline float sh4der_focus_uv_max_v(void) { return (float)SH4DER_FOCUS_SRC_H / (float)SH4DER_FOCUS_TEX_H; }

#ifdef _arch_dreamcast
/* KOS draw code (src/sh4der_focus.c): the texture, its compiled header, and the mesh submitted to the translucent list. */
#include <dc/pvr.h>

typedef struct sh4der_focus_gpu {
    pvr_ptr_t tex;          /**< \brief Blur texture in VRAM: SH4DER_FOCUS_TEX_BYTES, linear RGB565. */
    pvr_poly_hdr_t hdr;     /**< \brief Textured, translucent (SRC_ALPHA, INV_SRC_ALPHA), no culling. */
} sh4der_focus_gpu_t;

/** \brief Allocate the texture and compile the header. -1: no VRAM. */
int sh4der_focus_init(sh4der_focus_gpu_t *g);
/** \brief Load a blurred grid (sh4der_focus_box565's output) into the texture. */
void sh4der_focus_upload(sh4der_focus_gpu_t *g, const uint16_t *blurred);
/** \brief Submit the mesh for `m` into the translucent list (between pvr_list_begin / end). z: the list's depth value. */
void sh4der_focus_submit(const sh4der_focus_gpu_t *g, const sh4der_focus_mask_t *m, float z);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_FOCUS_H */
