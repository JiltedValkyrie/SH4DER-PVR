/* SH4DER-PVR: fake anisotropic filtering for the PVR: ranges by distance with a ramp between them, and a mip bias by slope

   demo/psm/fakeaf.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   A test's helper, not yet the library's. NOTHING HERE HAS RUN ON A CONSOLE: every statement about what the PVR does
   with it is an ESTIMATE, and the numbers in the default table are invented starting values.

   WHAT IS MEASURED ABOUT THE PVR'S OWN CHOICE (the mip test, one console, one run; wiki PVR-167 to PVR-169): the mip
   level is chosen a pixel; it follows the LARGER of the two footprint axes (texels a pixel along the view, on a
   floor), reading that level or one above it: two levels a doubling of distance down a floor, one on a wall facing
   the eye. So a floor is blurred ACROSS the view by as many levels as its squeeze: the case a sharper bias and the
   stretched copies are for. Trilinear drawn as pass A opaque and pass B added gives a smooth ramp between levels.
   The squeeze grows with the SQUARE of the distance, so the ranges' edges are a square root of two apart, not a
   doubling: faf_table_from puts them where the floor's texels a pixel along the view are 2, 4, 8.

   THE PROBLEM. A floor seen at a grazing angle squeezes its texels along the view and not across it. One mip level
   cannot suit both: the picture is blurred across or it shimmers along the view. Anisotropic filtering filters along
   the view only. The PVR has none.

   TWO WAYS, each a mode of the corridor test, to be compared on a split screen:

   BIAS (cheap). The PVR's mip bias is a field of the polygon header (0.25 .. 3.75 in steps of 0.25; 1.00 is none; under
   1.00 is sharper). faf_bias_code gives a polygon its bias from its slope to the eye. No pass and no fill is added:
   the polygons are batched by bias, a header a bias. A step can show where two neighbours' biases differ.

   RANGES (the main mode, the owner's design). The ground is cut by distance into ranges (a table, faf_range_t): each
   range has its own source, and across the start of each range lies a zone where it fades in over the range before.
   A source is the base texture with a mip bias, or a STRETCHED copy: the texture box-filtered and halved along the
   travel direction only (faf_stretch: 256 x 128, 256 x 64, 256 x 32 from 256 x 256), which is what anisotropic
   filtering buys: sharp across, filtered along. A stretched copy has no mip levels (the PVR's mip maps are square),
   so a range should not span much more than a doubling of distance. Three of them are 112 KB of video memory for a
   256 x 256 16-bit texture (a VQ copy would be smaller: not done).
   "Distance" is the depth along the view (the dot of the point's offset from the eye with the eye's forward): what
   the PVR's 1 / w carries.

   The ramp in a zone, two ways:
     3V  vertex ramp. The mesh has vertices on both edges of every zone (faf_edges, faf_cuts: a long quad is cut each
         frame; the edges move with the eye). Outside a zone a piece is drawn once, opaque, with its range's source.
         Inside the zone of range k it is drawn opaque with range k's source and again in the translucent list with
         range k - 1's, vertex alpha faf_alpha: 255 at the zone's near edge, 0 at its far edge, source alpha x
         1 - source alpha. No destination alpha.
     3D  destination-alpha ramp (the layer of the depth fade test d1), for a mesh that is not cut. The sharp source
         opaque; then, a coarser range: alpha cleared, the ramp added as alpha from a 1-D texture looked up by
         distance (faf_ramp_u, faf_ramp_texel), the coarser source through destination alpha. NOT BUILT here: only its
         two helpers, so it can follow.

   The fill a mode adds is estimated for a corridor by faf_fill_3v and faf_fill_3d: a model of areas on the screen,
   not a measurement. */

#ifndef FAKEAF_H
#define FAKEAF_H

#include <stdint.h>

#include "sh4der_core.h"                    /* the library's maths switch: SH4ZAM on the console, plain C on a host */

#define FAF_RANGES_MAX 4u
#define FAF_EDGES_MAX (2u * (FAF_RANGES_MAX - 1u))
#define FAF_BIAS_NONE 4u                    /* the PVR's code for 1.00: a code is the bias x 4, 1 .. 15 */

typedef struct faf_range {
    float from;                             /* metres: where this range starts to fade in (range 0: 0) */
    float ramp;                             /* metres: the zone's width; from `from + ramp` on it is alone (range 0: 0) */
    uint8_t bias;                           /* the mip bias code of its polygons, 1 .. 15; FAF_BIAS_NONE for none */
    uint8_t shift;                          /* its source: 0 the base texture; 1, 2, 3 the copy halved that often along v */
} faf_range_t;

/* The starting table: invented values for an eye about 1.6 m above a floor. The edges double with distance because
   the squeeze does. To be tuned on the screen. */
static inline unsigned faf_table_default(faf_range_t t[FAF_RANGES_MAX]) {
    static const faf_range_t d[FAF_RANGES_MAX] = { { 0.0f, 0.0f, 4u, 0u }, { 6.0f, 2.0f, 3u, 1u }, { 14.0f, 4.0f, 2u, 2u }, { 30.0f, 8.0f, 2u, 3u } };
    unsigned k;

    for (k = 0; k < FAF_RANGES_MAX; k++) t[k] = d[k];
    return FAF_RANGES_MAX;
}

/* The table for a floor from its own numbers: the eye `eye_h` metres above it looking level, focal length `focal`
   pixels, `texels_m` texels of the base texture a metre along the view. Along the view a pixel then covers
   texels_m z z / (focal eye_h) texels at distance z (the larger axis: the one the PVR's level follows, PVR-169).
   Range k = 1 .. n - 1 (the copy halved k times) starts where that is 2^k: a copy is then never minified by more
   than about two along the view inside its own range. A zone is `share` (0 .. 1) of the way to the next start.
   Past the last range's doubling a copy undersamples: the caller may end the table with a range of the base texture
   and a sharper bias (shift 0, a bias under FAF_BIAS_NONE). Returns n, or 0 when the numbers cannot be used. */
static inline unsigned faf_table_from(faf_range_t *t, unsigned n, float focal, float eye_h, float texels_m, float share) {
    const float root2 = 1.41421356f;
    float z;
    unsigned k;

    if (!t || n < 1u || n > FAF_RANGES_MAX || !(focal > 0.0f) || !(eye_h > 0.0f) || !(texels_m > 0.0f)) return 0u;
    if (!(share >= 0.05f)) share = 0.05f;
    if (share > 1.0f) share = 1.0f;
    t[0].from = 0.0f; t[0].ramp = 0.0f; t[0].bias = (uint8_t)FAF_BIAS_NONE; t[0].shift = 0u;
    z = sh4der_sqrt_(2.0f * focal * eye_h * sh4der_recip_pos_(texels_m));       /* where a pixel covers 2 texels along the view */
    for (k = 1; k < n; k++, z *= root2) {
        t[k].from = z; t[k].ramp = z * (root2 - 1.0f) * share;
        t[k].bias = (uint8_t)FAF_BIAS_NONE; t[k].shift = (uint8_t)(k > 3u ? 3u : k);
    }
    return n;
}

/* 1 when a table can be used: 1 .. FAF_RANGES_MAX ranges, range 0 from 0 with no zone, every later zone at least
   1 cm wide and ended before the next one starts, biases 1 .. 15, shifts 0 .. 3. A live tuning step that would break
   this is refused by the caller. */
static inline int faf_table_ok(const faf_range_t *t, unsigned n) {
    unsigned k;

    if (!t || n < 1u || n > FAF_RANGES_MAX) return 0;
    if (t[0].from != 0.0f || t[0].ramp != 0.0f) return 0;
    for (k = 0; k < n; k++) {
        if (t[k].bias < 1u || t[k].bias > 15u || t[k].shift > 3u) return 0;
        if (k && !(t[k].ramp >= 0.01f)) return 0;
        if (k && !(t[k].from >= t[k - 1u].from + t[k - 1u].ramp)) return 0;
        if (k && !(t[k].from > 0.0f)) return 0;
    }
    return 1;
}

/* How far range r has faded in at a distance, 0 .. 1. Range 0: 1 everywhere. */
static inline float faf_in(const faf_range_t *r, float dist) {
    float f;

    if (!(r->ramp > 0.0f)) return dist >= r->from ? 1.0f : 0.0f;
    f = (dist - r->from) * sh4der_recip_pos_(r->ramp);
    return f <= 0.0f ? 0.0f : f >= 1.0f ? 1.0f : f;
}

/* The share of range k in the picture at a distance, 0 .. 1: the shares of all ranges add to 1. */
static inline float faf_weight(const faf_range_t *t, unsigned n, unsigned k, float dist) {
    const float in = k ? faf_in(&t[k], dist) : 1.0f, next = k + 1u < n ? faf_in(&t[k + 1u], dist) : 0.0f;

    return in * (1.0f - next);              /* zones do not overlap (faf_table_ok): one of the two factors is 0 or 1 */
}

/* 3V: the vertex alpha of range k - 1's translucent piece inside the zone of range `r` = t[k]: 255 where the zone
   starts, 0 where it ends. */
static inline uint8_t faf_alpha(const faf_range_t *r, float dist) { return (uint8_t)((1.0f - faf_in(r, dist)) * 255.0f + 0.5f); }

/* The distances where a mesh must have vertices for 3V, rising: each zone's start and end. Returns how many (2 a
   range after the first). */
static inline unsigned faf_edges(const faf_range_t *t, unsigned n, float edge[FAF_EDGES_MAX]) {
    unsigned k, m = 0;

    for (k = 1; k < n && k < FAF_RANGES_MAX; k++) { edge[m++] = t[k].from; edge[m++] = t[k].from + t[k].ramp; }
    return m;
}

/* A segment from distance d0 to d1 (either may be the larger): the parameters 0 < s < 1 at which it crosses an edge,
   in the order met from d0. Returns how many; the pieces between them each lie in one range or one zone. A segment
   level with the screen (d0 == d1) has none. */
static inline unsigned faf_cuts(const float *edge, unsigned ne, float d0, float d1, float s[FAF_EDGES_MAX]) {
    const float span = d1 - d0;
    unsigned i, m = 0;

    if (span == 0.0f) return 0u;
    for (i = 0; i < ne && i < FAF_EDGES_MAX; i++) {
        const unsigned e = span > 0.0f ? i : ne - 1u - i;       /* rising edges are met in order going away, in reverse coming back */
        const float at = sh4der_div_(edge[e] - d0, span);

        if (at > 0.0f && at < 1.0f) s[m++] = at;
    }
    return m;
}

/* What draws a piece whose middle is at `dist`: *opaque is the range drawn in the opaque list; *blend the range drawn
   over it in the translucent list with faf_alpha(&t[*opaque], ..) at its vertices, or -1 for none (outside a zone). */
static inline void faf_piece(const faf_range_t *t, unsigned n, float dist, int *opaque, int *blend) {
    unsigned k, at = 0;

    for (k = 1; k < n; k++) if (dist >= t[k].from) at = k;
    *opaque = (int)at;
    *blend = at && dist < t[at].from + t[at].ramp ? (int)at - 1 : -1;
}

/* BIAS: the mip bias code of a polygon from its slope to the eye. ndv is |normal . direction to the eye| (1 facing,
   0 edge on); strength 0 .. 1 is how much of the correction to take. A grazing polygon is squeezed along the view by
   1 / ndv; taking the level for the geometric mean of the two axes instead of the squeezed one is a bias of
   sqrt(ndv). The PVR does pick its level from the squeezed axis (MEASURED, PVR-169); that the bias field then moves
   the level a polygon as documented is NOT measured, and the square root is a choice, not a law. Never above 1.00
   and never under `least` (a code, 1 .. 4): too sharp shimmers along the view. */
static inline uint8_t faf_bias_code(float ndv, float strength, unsigned least) {
    const float a = ndv <= 0.0f ? 0.0f : ndv >= 1.0f ? 1.0f : ndv;
    const float bias = 1.0f - (strength <= 0.0f ? 0.0f : strength >= 1.0f ? 1.0f : strength) * (1.0f - sh4der_sqrt_(a));
    unsigned code = (unsigned)(bias * 4.0f + 0.5f);

    if (least < 1u) least = 1u;
    if (least > FAF_BIAS_NONE) least = FAF_BIAS_NONE;
    if (code < least) code = least;
    if (code > FAF_BIAS_NONE) code = FAF_BIAS_NONE;
    return (uint8_t)code;
}

/* A stretched copy: `src` is w x h RGB565, rows in order (not twiddled); `dst` gets w x (h >> shift), each row the
   mean of 2^shift rows of src, a channel at a time, rounded. Nothing is filtered along a row. At load, once. Returns
   the copy's rows; 0 when the arguments cannot be used. */
static inline unsigned faf_stretch(const uint16_t *src, unsigned w, unsigned h, unsigned shift, uint16_t *dst) {
    const unsigned rows = h >> shift, n = 1u << shift;
    unsigned x, y, i;

    if (!src || !dst || !w || shift > 3u || !rows) return 0u;
    for (y = 0; y < rows; y++)
        for (x = 0; x < w; x++) {
            unsigned r = 0, g = 0, b = 0;

            for (i = 0; i < n; i++) {
                const uint16_t p = src[(y * n + i) * w + x];

                r += p >> 11; g += (p >> 5) & 63u; b += p & 31u;
            }
            dst[y * w + x] = (uint16_t)((((r + n / 2u) >> shift) << 11) | (((g + n / 2u) >> shift) << 5) | ((b + n / 2u) >> shift));
        }
    return rows;
}

/* The video memory of a texture's stretched copies for shifts 1 .. `shifts`, bytes, 16 bit. */
static inline uint32_t faf_stretch_bytes(unsigned w, unsigned h, unsigned shifts) {
    uint32_t sum = 0;
    unsigned k;

    for (k = 1; k <= shifts && k <= 3u; k++) sum += (uint32_t)w * (h >> k) * 2u;
    return sum;
}

/* 3D (not built yet): the ramp texture's coordinate at a distance for range r, 0 at its zone's start and 1 at its
   end, to be clamped by the texture's clamp mode; and texel i of n of that texture, ARGB4444: black, alpha rising
   (16 steps: all 4444 has). */
static inline float faf_ramp_u(const faf_range_t *r, float dist) { return (dist - r->from) * sh4der_recip_pos_(r->ramp); }
static inline uint16_t faf_ramp_texel(unsigned i, unsigned n) { return (uint16_t)(((n > 1u ? i * 15u / (n - 1u) : 15u) & 15u) << 12); }

/* ---- the fill a mode adds: a model for a corridor, not a measurement ------------------------------------------ */

/* The share of a 640 x 480 screen that shows what lies beyond `dist` in a corridor seen along its axis from its
   middle: half width a, half height b metres, focal length f pixels. */
static inline float faf_beyond(float f, float a, float b, float dist) {
    const float inv = sh4der_recip_pos_(dist > 0.01f ? dist : 0.01f);
    float hx = f * a * inv, hy = f * b * inv;

    if (hx > 320.0f) hx = 320.0f;
    if (hy > 240.0f) hy = 240.0f;
    return hx * hy * (1.0f / (320.0f * 240.0f));
}

/* 3V: the translucent fill, in screens: the zones alone, each drawn once more. */
static inline float faf_fill_3v(const faf_range_t *t, unsigned n, float f, float a, float b) {
    float sum = 0.0f;
    unsigned k;

    for (k = 1; k < n; k++) sum += faf_beyond(f, a, b, t[k].from) - faf_beyond(f, a, b, t[k].from + t[k].ramp);
    return sum;
}

/* 3D: three layers (clear, ramp, the coarser source) over everything beyond each range's start. */
static inline float faf_fill_3d(const faf_range_t *t, unsigned n, float f, float a, float b) {
    float sum = 0.0f;
    unsigned k;

    for (k = 1; k < n; k++) sum += 3.0f * faf_beyond(f, a, b, t[k].from);
    return sum;
}

#endif /* FAKEAF_H */
