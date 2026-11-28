/* SH4DER-PVR: a few moving shadows, the PVR draws the map (route A): the pass's accounting and maths

   include/sh4der_psm_a.h, src/sh4der_psm_a.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).

   RECOMMENDED (from the wiki ledger, one console; wiki/limits/header-recommended.md)
   Start with: SH4DER_PSM_A_SIZE = 128 (a 128 x 128 map, 32 KB, over an 8 m square: SH4DER_PSM_A_SQUARE_M); at most
               SH4DER_PSM_A_CASTERS = 4 casters and SH4DER_PSM_A_TRIS = 2,200 triangles a pass; the pass cut to its own
               tiles (4 x 4 for 128); the receiver one non-overlapping piece, a few centimetres above the floor
   Cost:       ra2 (one console, MEASURED): map pass PVR 3.2 ms uncut -> 0.2 ms cut at 128 and 0.8 ms at 256, same
               picture; frame 16.6 ms uncut -> 20.0 ms cut (worst 25 ms, pass total 19.9 ms). DERIVED from reading
               KallistiOS's source (to be tested by ra5): the 20.0 ms stall comes from two sleeping
               pvr_wait_render_done waits on 10 ms scheduler ticks (same loss as PVR-117). Polled waits via
               pvrx (SH4DER_PSM_A_CUT_PASS + sh4der_pvrx_wait_hook(1)) and held single-table cut (SH4DER_PSM_A_CUT_HOLD):
               NOT RUN on a console yet. Town route: SH-4 13.3 to 13.6 ms against 40.2 to 43.9 ms for the SH-4 depth map
               (PVR-108 to PVR-113, PVR-197). ESTIMATE: TA 3 vertices (96 B) a clipped caster triangle, at most 211 KB
   Status:     PROTOTYPE v0.2. Maths and accounting host tested (test/psm_a_test.c); per-table tile cut host tested
               (test/tilecut_test.c). Per-pass cut with sleeping waits ran on one console (ra2: PVR fell, frame stalled).
               Polled waits via pvrx and held single-table cut (SH4DER_PSM_A_CUT_HOLD) are NOT RUN on a console yet;
               compacted table with row 0 left out is NOT RUN on a console yet
   Do:         for SH4DER_PSM_A_CUT_PASS without the 20 ms tick stall, call sh4der_pvrx_wait_hook(1) after every pvr_init,
               sh4der_pvrx_scene_finish for every screen scene, and sh4der_pvrx_wait_hook(0) before pvr_shutdown;
               for SH4DER_PSM_A_CUT_HOLD, call sh4der_psm_a_release before any frame without a pass, before another
               scene that would use the pass's table, at exit, and before pvr_shutdown; keep the receiver's geometry
               away from the casters' own floor contact if you want no dark line at the foot
   Do not:     call pvr_scene_finish directly while sh4der_pvrx_wait_hook(1) is on; open a screen scene on a no-pass
               frame without calling sh4der_psm_a_release when SH4DER_PSM_A_CUT_HOLD is active; expect a body to shadow
               itself (no depth compare: use sh4der_psm_raster.h); run the pass in a menu screen (RGB888P); send a
               caster the accounting refused
   Maturity:   PROTOTYPE v0.2

   WHAT IT IS. A few characters (at most four) cast a shadow on a flat ground patch near them. The casters are drawn
   from the light, flat dark on white, by the PVR into a small render target (the pass); the ground patch is drawn a
   second time with that target as its texture, multiplied over the screen (the layer). The SH-4 only transforms and
   clips the casters' triangles (sh4der_psm_pvr.h) and gives each receiver vertex its map coordinates. There is NO depth
   compare: a body does not shadow itself, and one map has one light. Each caster has a strength 0 .. 1 (its grey is
   white at 0, the full shadow level at 1): a cross-fade with a blob shadow, or a dying character, with no second pass.

   THE MAP is the library's uniform map (sh4der_psm.h) for a directional light, fitted to a square patch of ground and
   held still (sh4der_psm_build_uniform_held): the fit's extents are kept and its centre moves by whole texels, so a
   moving camera or patch moves no texel and an edge does not crawl. sh4der_psm_a_fit takes the patch's centre and half
   side. The fit is a square on a plane perpendicular to the light, so it takes the patch's diagonal: an 8 m patch with a
   sun 64 degrees up gives a texel of 9.8 cm (HOST, test/psm_a_test.c) on 128 texels. Read the map's size from
   sh4der_psm_a_texel_m.

   THE PASS (src/sh4der_psm_a.c, Dreamcast only), once a frame, before the screen's scene:
       sh4der_psm_a_fit(&a, light, centre, half, y_lo, y_hi);          the map for this frame
       sh4der_psm_a_begin(&t, &a)                                      opens the target's scene and its opaque list
       sh4der_psm_a_caster(&t, &a, &caster)                            a caster, or 0 when the accounting refuses it
       sh4der_psm_a_end(&t, &a);                                       the border, the scene's end (sh4der_pvrx_scene_finish)
       sh4der_psm_a_settle(&t);                                        waits + restores (CUT_PASS), or leaves the pass's table cut (CUT_HOLD)
   then in the screen's translucent list, after the ground: sh4der_psm_a_layer_header, and each receiver vertex through
   sh4der_psm_a_layer_vertex (the map coordinates and white; the PVR multiplies it over what is on screen).
   With SH4DER_PSM_A_CUT_HOLD, call sh4der_psm_a_release(&t) before any frame without a pass, before another scene that
   would use the pass's table, at exit, or before pvr_shutdown. With sh4der_pvrx_wait_hook(1) active, finish every
   screen scene with sh4der_pvrx_scene_finish (AGENTS.md "Call order: pvrx"); pvr_wait_ready stays sleeping so any
   console stall separates ready from render-done.

   THE PASS'S PLACE ON THE SCREEN. A pass is a scene of its own, drawn at tile row cfg.row (y = 32 x row) with the
   render base set so the texture is exactly size x size from there. That row is for a screen whose tile table has had
   rows left out (a 640 x 360 band shown inside the 640 x 480 signal, rows 1 to 13 kept): the pass keeps clear of row 0
   and of the clip's partial rows, and the tile cut (sh4der_tilecut.h, row origin and a table with row0 1) makes the PVR
   visit only the target's tiles. KallistiOS sets a render target's pixel clip to the target's own size (pvr_misc.c,
   read at KallistiOS 568c3404), not the screen's clip, so a screen clip narrowed by the program does
   not cut the pass; the render height is cfg.row x 32 + size for that reason.

   ACCOUNTING (sh4der_psm_a_accept): at most SH4DER_PSM_A_CASTERS casters and SH4DER_PSM_A_TRIS triangles a pass, and a
   caster over its own cap (caster.max_tris, 0 for none) is refused: its owner draws a blob. A refused caster costs
   nothing and counts in a.refused.

   A caster's triangles are WORLD positions in the space the map was fitted in (a caster in a baked pose: one matrix
   applied by the caller; a skinned one: its posed vertices once, in that space). The index list is uint16 triples. The
   first caster drawn wins an overlap (equal depth), so send the strongest first. */

#ifndef SH4DER_PSM_A_H
#define SH4DER_PSM_A_H

#include <stdint.h>
#include <string.h>

#include "sh4der_psm_pvr.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_PSM_A_SIZE        128u      /**< \brief The map's side in texels (a power of two, 32 to 1024). */
#define SH4DER_PSM_A_SQUARE_M    8.0f      /**< \brief The receiver patch's side, metres, for a 128 map. */
#define SH4DER_PSM_A_CASTERS     4u        /**< \brief Casters a pass. */
#define SH4DER_PSM_A_TRIS        2200u     /**< \brief World triangles a pass. */
#define SH4DER_PSM_A_HOLD_COS    0.99999f  /**< \brief The light is taken again when it moves about a quarter of a degree. */
#define SH4DER_PSM_A_AREA_MIN    1.0e-3f   /**< \brief A triangle in the map with less area than this (texels squared, doubled) is not sent. */
#define SH4DER_PSM_A_CAST_Z      1.0f      /**< \brief A caster's depth in the target (the white ground is 0.1, the border 2.0). */
/** @} */

/** \brief How the pass uses the tile table cut (`sh4der_tilecut.h`). */
enum {
    SH4DER_PSM_A_CUT_NONE = 0,  /**< No cut: the pass renders through the screen's whole tile table. */
    SH4DER_PSM_A_CUT_PASS = 1,  /**< Cut both tables for the pass and restore after (waits for render done twice; poll via pvrx). */
    SH4DER_PSM_A_CUT_HOLD = 2   /**< Cut only the pass's own table and leave it cut (no render-done wait in begin/settle; release on order break). */
};

/** \brief What a pass is for and where it sits. `sh4der_psm_a_cfg_default` fills it for an ordinary screen. */
typedef struct sh4der_psm_a_cfg {
    unsigned size;      /**< the map's side, texels */
    unsigned row;       /**< the tile row the pass is drawn at (y = 32 x row): 0 on an ordinary screen, 2 on a 640 x 360 band's compacted table */
    unsigned tw;        /**< the screen's tile columns (20) */
    unsigned rows;      /**< rows an entry column of its tile table holds (15, or 13 when rows 1 to 13 are kept) */
    unsigned row0;      /**< the number of the first row in the table (0, or 1) */
    int cut;            /**< SH4DER_PSM_A_CUT_NONE (0), _CUT_PASS (1), or _CUT_HOLD (2) */
} sh4der_psm_a_cfg_t;

static inline void sh4der_psm_a_cfg_default(sh4der_psm_a_cfg_t *c) {
    c->size = SH4DER_PSM_A_SIZE; c->row = 0u; c->tw = 20u; c->rows = 15u; c->row0 = 0u; c->cut = SH4DER_PSM_A_CUT_PASS;
}

/** \brief A caster: its triangles in the map's space, and how dark it casts. */
typedef struct sh4der_psm_a_caster {
    const float (*pos)[3];       /**< vertex positions */
    const uint16_t (*idx)[3];    /**< triangles, indices into pos */
    unsigned nt;                 /**< triangles */
    float strength;              /**< 0 .. 1: the share of the full shadow it casts */
    unsigned max_tris;           /**< refused when nt is more than this; 0 for no cap */
} sh4der_psm_a_caster_t;

/** \brief A frame's map and its accounting. Zero it once (hold included), then `sh4der_psm_a_fit` and `sh4der_psm_a_begin_frame`. */
typedef struct sh4der_psm_a {
    sh4der_psm_t map;            /**< this frame's map */
    sh4der_psm_hold_t hold;      /**< what the fit keeps from frame to frame */
    unsigned size;               /**< the map's side in texels */
    float y0;                    /**< the pass's first line on the render, pixels (32 x row): added to every map y */
    float shadow[3];             /**< the full shadow's level a channel, 0 .. 1 */
    unsigned casters, tris;      /**< accepted this pass */
    unsigned refused;            /**< refused by the accounting this pass */
    unsigned sent_tris;          /**< triangles the PVR was given (after clipping) */
    unsigned dropped;            /**< world triangles that gave nothing: outside the map, or no area in it */
    int built;                   /**< 1: the map is fitted for this frame */
} sh4der_psm_a_t;

/** \brief One triangle the PVR draws: map pixels, y with the pass's origin added. */
typedef struct sh4der_psm_a_tri {
    sh4der_psm_pvr_vtx_t v[3];
} sh4der_psm_a_tri_t;

/** \brief Set the pass's size and place, and clear the counts (the hold too). */
static inline void sh4der_psm_a_init(sh4der_psm_a_t *a, unsigned size, unsigned row) {
    memset(a, 0, sizeof *a);
    a->size = size;
    a->y0 = 32.0f * (float)row;
}

/** \brief A new frame: the counts back to 0, and the map unfitted until `sh4der_psm_a_fit`. */
static inline void sh4der_psm_a_begin_frame(sh4der_psm_a_t *a, const float shadow[3]) {
    a->casters = a->tris = a->refused = a->sent_tris = a->dropped = 0u;
    a->built = 0;
    a->shadow[0] = sh4der_sat_(shadow[0]); a->shadow[1] = sh4der_sat_(shadow[1]); a->shadow[2] = sh4der_sat_(shadow[2]);
}

/** \brief The full shadow's level of a lit surface: the ambient share, ambient / (ambient + sun x its cosine). A level
    a channel (a tinted light): sun_cos is the sun's strength times the floor's cosine. */
static inline float sh4der_psm_a_shadow_level(float ambient, float sun_cos) {
    const float sum = ambient + sun_cos;

    return sum > 1.0e-6f ? sh4der_sat_(sh4der_div_(ambient, sum)) : 1.0f;
}

/** \brief The map for this frame: the uniform map over a square patch of floor, held still. `light` is the direction from
    the surface toward the light, `centre` the patch's middle on the floor, `half` its half side, and `y_lo` .. `y_hi`
    the height window above `centre` that receives (the floor slab). 0 for a bad input. The patch's own points are all
    the fit sees: a caster outside the map's square is clipped away, whatever its height. */
static inline int sh4der_psm_a_fit(sh4der_psm_a_t *a, const float light[3], const float centre[3], float half, float y_lo, float y_hi) {
    float pts[8][3];
    unsigned i;

    if (!a) return 0;
    a->built = 0;
    if (!light || !centre || !(half > 0.0f) || !(y_hi >= y_lo) || a->size < 2u) return 0;
    for (i = 0; i < 8u; i++) {
        pts[i][0] = centre[0] + ((i & 1u) ? half : -half);
        pts[i][1] = centre[1] + ((i & 2u) ? y_hi : y_lo);
        pts[i][2] = centre[2] + ((i & 4u) ? half : -half);
    }
    a->built = sh4der_psm_build_uniform_held(&a->map, light, &pts[0][0], 8u, a->size, &a->hold, SH4DER_PSM_A_HOLD_COS);
    return a->built;
}

/** \brief The map's texel on a perpendicular to the light, metres (the held fit's two extents over the size; the larger
    side). 0 before the first fit. A texel on the floor is this over the cosine of the light's angle to the floor's normal. */
static inline float sh4der_psm_a_texel_m(const sh4der_psm_a_t *a) {
    const float e = a->hold.ext[0] > a->hold.ext[1] ? a->hold.ext[0] : a->hold.ext[1];

    return a->hold.ok && a->size ? sh4der_div_(2.0f * e, (float)a->size) : 0.0f;
}

/** \brief 1 and the counts taken when the pass can take a caster of `nt` triangles and `max_tris` cap (0 for none); else
    0 and a.refused counts it. Nothing is drawn by this call. */
static inline int sh4der_psm_a_accept(sh4der_psm_a_t *a, unsigned nt, unsigned max_tris) {
    if (!a->built || !nt || a->casters >= SH4DER_PSM_A_CASTERS || a->tris + nt > SH4DER_PSM_A_TRIS || (max_tris && nt > max_tris)) {
        a->refused++;
        return 0;
    }
    a->casters++; a->tris += nt;
    return 1;
}

/** \brief A caster's colour in the map, ARGB: white at strength 0, the shadow level at 1, a channel at a time. */
static inline uint32_t sh4der_psm_a_argb(const sh4der_psm_a_t *a, float strength) {
    const float s = sh4der_sat_(strength);
    unsigned c[3], k;

    for (k = 0; k < 3u; k++) c[k] = (unsigned)(255.0f * (1.0f - s * (1.0f - a->shadow[k])) + 0.5f);
    return 0xff000000u | (c[0] << 16) | (c[1] << 8) | c[2];
}

/** \brief One world triangle into the triangles the PVR draws (at most SH4DER_PSM_PVR_MAX_TRIS): clipped to the map, in
    pixels with the pass's origin added to y, those with no area left out. Returns the count (0: none of it is in the map).
    `out` holds SH4DER_PSM_PVR_MAX_TRIS. Culling must be off when they are drawn. Counts a.sent_tris, and a.dropped when none. */
static inline unsigned sh4der_psm_a_tri(sh4der_psm_a_t *a, const float p0[3], const float p1[3], const float p2[3], sh4der_psm_a_tri_t out[]) {
    sh4der_psm_pvr_vtx_t raw[SH4DER_PSM_PVR_MAX_TRIS][3];
    unsigned n, m, k, kept = 0;

    if (!a->built) return 0;
    n = sh4der_psm_pvr_tris(&a->map, a->size, p0, p1, p2, raw);
    for (m = 0; m < n; m++) {
        const float area = (raw[m][1].x - raw[m][0].x) * (raw[m][2].y - raw[m][0].y) - (raw[m][2].x - raw[m][0].x) * (raw[m][1].y - raw[m][0].y);

        if (!(area > SH4DER_PSM_A_AREA_MIN || area < -SH4DER_PSM_A_AREA_MIN)) continue;
        for (k = 0; k < 3u; k++) {
            out[kept].v[k].x = raw[m][k].x;
            out[kept].v[k].y = raw[m][k].y + a->y0;
            out[kept].v[k].invw = raw[m][k].invw;
        }
        kept++;
    }
    if (kept) a->sent_tris += kept; else a->dropped++;
    return kept;
}

/** \brief A receiver vertex's texture coordinates in the map target, 0 .. 1 (the texture is the size x size part of the
    target, so the pass's origin is not in them). 0 at the light's singularity (a uniform map never is). */
static inline int sh4der_psm_a_uv(const sh4der_psm_a_t *a, const float p[3], float *u, float *v) {
    if (!a->built) return 0;
    return sh4der_psm_pvr_uv(&a->map, p, u, v);
}

/** \brief 1 when a world point lands inside the map (its lookup is inside the target, not on the white border): a receiver
    triangle with no vertex inside may be left out of the layer. */
static inline int sh4der_psm_a_covers(const sh4der_psm_a_t *a, const float p[3]) {
    float u, v;

    return sh4der_psm_a_uv(a, p, &u, &v) && u > 0.0f && u < 1.0f && v > 0.0f && v < 1.0f;
}

#ifdef __cplusplus
}
#endif

/* ---- the KallistiOS pass (src/sh4der_psm_a.c, Dreamcast only) ---------------------------------------------------- */

#ifdef _arch_dreamcast

#include <kos.h>
#include "sh4der_tilecut.h"

/** \brief The render target, the three polygon headers and the tile cut of a pass. One per program. */
typedef struct sh4der_psm_a_target {
    sh4der_psm_a_cfg_t cfg;
    void *base;                      /**< the allocation: the render base (row x 32 + size lines of size texels) */
    void *tex;                       /**< the map's texture: base + row x 32 lines */
    unsigned bytes;                  /**< the allocation's bytes */
    int ready;                       /**< 1 after a good init */
    int open;                        /**< 1 between begin and settle */
    int cut_on;                      /**< 1 when the open pass is cut (per-pass or held) */
    int cut_held;                    /**< table index (0 or 1) held cut in SH4DER_PSM_A_CUT_HOLD, or -1 */
    unsigned cut_refused;            /**< passes that ran uncut because the cut was refused */
    unsigned cut_broken;             /**< times held mode found the in-flight table cut (order broke) and restored it */
    unsigned cut_bad;                /**< bad restore words from a recovery inside begin, returned by settle */
    pvr_poly_hdr_t hdr_caster, hdr_layer;
    sh4der_tilecut_t cut;
} sh4der_psm_a_target_t;

/** \brief Video memory for the target (size x (row x 32 + size) x 2 bytes) and the headers; the tile cut set up. 1 on
    success, 0 with no video memory (draw no shadow). Call after pvr_init, with no render running. */
int sh4der_psm_a_target_init(sh4der_psm_a_target_t *t, const sh4der_psm_a_cfg_t *cfg);

/** \brief Free the target. Waits for the PVR and restores any held tile table first. */
void sh4der_psm_a_target_free(sh4der_psm_a_target_t *t);

/** \brief The PVR was shut down and started again (a video mode change): the target's video memory and the tile tables are
    gone. Forgets the target WITHOUT freeing it (the memory died with the old PVR); call sh4der_psm_a_target_init again
    afterwards (it makes a fresh cut state), then sh4der_psm_a_find after the next render. The pass must not be open. */
void sh4der_psm_a_target_forget(sh4der_psm_a_target_t *t);

/** \brief Restore any tile table held cut by SH4DER_PSM_A_CUT_HOLD (waits for the PVR first). Call before opening a
    screen scene on a frame with no pass, before another scene that would use the pass's table, before exit, or before
    pvr_shutdown. If forgotten, the screen renders through the cut table so only the pass's few tiles are drawn and the
    rest of that buffer is stale; the library sees it only at the next begin (cut_broken). HOLD requires the pass to be
    every second render: a third render per frame (such as a mirror pass) puts another scene through the held table every
    other frame. Returns words still different after restore (0: all well; other than 0: shut the PVR down and pvr_init
    again). No-op returning 0 when no table is cut. */
unsigned sh4der_psm_a_release(sh4der_psm_a_target_t *t);

/** \brief After a screen scene's render: finds the tile table that render went through (sh4der_psm_a_settle finds the pass's
    own after an uncut pass: KallistiOS flips tables a render, so the two come from different renders). Until both are known, call it each frame
    until `t->cut.n` is the number of tables, with no cut on. */
void sh4der_psm_a_find(sh4der_psm_a_target_t *t);

/** \brief Open the pass: the cut (when on and every table is found), the target's scene, the opaque list, the white
    ground. `a` must be fitted and began (sh4der_psm_a_begin_frame). 0 when nothing was opened (draw no layer; any held
    table is restored first so the screen scene does not render through a cut table). */
int sh4der_psm_a_begin(sh4der_psm_a_target_t *t, sh4der_psm_a_t *a);

/** \brief One caster: accepted by the accounting, then its triangles sent. 1 when drawn, 0 when refused. */
int sh4der_psm_a_caster(sh4der_psm_a_target_t *t, sh4der_psm_a_t *a, const sh4der_psm_a_caster_t *c);

/** \brief Close the pass: the white border (a lookup outside the map is lit), the list, the scene (sh4der_pvrx_scene_finish). */
void sh4der_psm_a_end(sh4der_psm_a_target_t *t, sh4der_psm_a_t *a);

/** \brief Finish the pass: in CUT_PASS or uncut, waits for the render (sh4der_pvrx_wait_render_done_poll) and puts the
    tile tables back; in CUT_HOLD with the pass's table cut, leaves it cut without waiting (the screen scene uses the
    other table, and KallistiOS serialises ISP renders). Returns table words that did not read back as taken (0: all
    well; other than 0: shut the PVR down and start it again). Call before the screen's scene is opened. */
unsigned sh4der_psm_a_settle(sh4der_psm_a_target_t *t);

/** \brief The layer's polygon header, compiled at init: translucent list, screen x map, filtered, clamped, depth test
    greater-or-equal with no write. Send it with pvr_prim before the receiver's vertices, after the ground. */
const pvr_poly_hdr_t *sh4der_psm_a_layer_header(const sh4der_psm_a_target_t *t);

/** \brief Fill a receiver vertex: world position `p` for the map coordinates, (sx, sy, sz) the screen vertex, `eol` the
    strip's last vertex. White colour. 0 when the map coordinates are not there (send no triangle with it). */
int sh4der_psm_a_layer_vertex(const sh4der_psm_a_t *a, const float p[3], float sx, float sy, float sz, int eol, pvr_vertex_t *out);

#endif /* _arch_dreamcast */

#endif /* SH4DER_PSM_A_H */
