/* SH4DER-PVR: the bloom effect

   sh4der_bloom.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_bloom.h
    \brief   A soft additive glow round a blade (or any point), made of a few
             feathered polygons: the Dreamcast's cheap bloom.

    The PVR has no post-process blur; a few untextured polygons added over the
    frame are its bloom. Their soft edge is SH4DOW-DC's feather, sampled at the
    vertices and spread by Gouraud shading.

    - The ribbon: one strip along the blade, facing the camera about the blade's
      axis. 3 rows (the root, the blade tip, a tail past the tip at 0) by 5
      columns (edge, mid, centre, mid, edge), the feather across. Each vertex
      takes the blade's own tubes at that point (the swirl brightens that side),
      the pulse, and a boost where the blocker passes (the bright knot).
    - The disc: a feathered disc facing the camera (centre and RIM rim vertices,
      one pointing down the blade): the tip bloom, a lens flare: full when the
      tip points at the eye, a low floor side-on. sh4der_bloom_disc() is
      general (a centre, a radius, a peak) for any glow point.

    sh4der_bloom_blade() fills a mesh (positions in the blade's space, a level
    per vertex); sh4der_bloom_argb() puts the levels on a colour ramp. Draw the
    SH4DER_BLOOM_TRIS triangles of sh4der_bloom_tris additively (src alpha /
    one), depth tested, no depth write, with the blade.

    The effect: puts a soft halo of light round a bright blade, so it seems to
      light the air about it. It does not read the frame: a few feathered
      polygons are added over the picture, 24 triangles.
    Developed for: the glow round the energy sword in Halo: Dreamcast Evolved.
    Use it for: a cheap glow round one bright thing: a blade, a laser bolt, a
      lamp, a muzzle flash, a pick-up.

    \author Jilted Valkyrie
    \author Cortana (Claude, an AI by Anthropic): the effects' maths, the PVR paths, the tests
*/

#ifndef SH4DER_BLOOM_H
#define SH4DER_BLOOM_H

#include "sh4der_blade.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SH4DER_BLOOM_ROWS   3                     /**< \brief Ribbon rows: root, blade tip, tail. */
#define SH4DER_BLOOM_COLS   5                     /**< \brief Ribbon columns: edge, mid, centre, mid, edge. */
#define SH4DER_BLOOM_RIM    8                     /**< \brief Tip disc rim vertices (8: no hexagonal flat beyond the tip). */
#define SH4DER_BLOOM_DISC_MAX 24                  /**< \brief Rim vertices sh4der_bloom_disc_n() takes at most (a round glow: 16). */
#define SH4DER_BLOOM_VERTS  24                    /**< \brief ROWS x COLS + 1 + RIM. */
#define SH4DER_BLOOM_TRIS   24                    /**< \brief (ROWS-1) x (COLS-1) x 2 + RIM: the budget, 24 for one blade. */

/** \name Default tuning: a glowing blade (sh4der_bloom_params_default())
    Lengths in the blade's own units (its node space).
    @{ */
#define SH4DER_BLOOM_X0          -0.065f  /**< \brief Ribbon root along the axis: the blade's base, at level ROOT (0: no edge). */
#define SH4DER_BLOOM_X1          0.35f    /**< \brief The blade tip along the axis (the disc's centre). */
#define SH4DER_BLOOM_TAIL        0.10f    /**< \brief Ribbon tail past the tip (level 0 there). */
#define SH4DER_BLOOM_HALF_WIDTH  0.0875f  /**< \brief Ribbon half width (2.5 x tube A's edge). */
#define SH4DER_BLOOM_INNER       0.012f   /**< \brief Full level within this distance of the axis. */
#define SH4DER_BLOOM_ROOT        0.0f     /**< \brief Ribbon level at the root: 0, it fades in from the blade's base. */
#define SH4DER_BLOOM_TIP         0.90f    /**< \brief Ribbon level at the blade tip: way more at the top. */
#define SH4DER_BLOOM_TUBE_FLOOR  0.5f     /**< \brief Share of the ribbon with no tube near. */
#define SH4DER_BLOOM_BOOST       0.4f     /**< \brief Level the blocker's knot adds. */
#define SH4DER_BLOOM_BOOST_REACH 0.10f    /**< \brief The knot's reach along the axis. */
#define SH4DER_BLOOM_DISC_RADIUS 0.14f    /**< \brief Tip disc radius. */
#define SH4DER_BLOOM_DISC_PEAK   0.90f    /**< \brief Tip disc level at its centre, the tip pointing at the eye. */
#define SH4DER_BLOOM_FLARE_FLOOR 0.15f    /**< \brief Tip disc share side-on: a lens flare, strong only toward the view axis. */
#define SH4DER_BLOOM_FLARE_COS   0.5f     /**< \brief The flare rises from the floor once the tip is within 60 degrees of the eye (cos). */
/** @} */

/** \name The MAX WHITE test preset (sh4der_bloom_params_max())
    @{ */
#define SH4DER_BLOOM_MAX_TAIL        0.14f
#define SH4DER_BLOOM_MAX_HALF_WIDTH  0.11f
#define SH4DER_BLOOM_MAX_INNER       0.015f
#define SH4DER_BLOOM_MAX_TIP         0.80f
#define SH4DER_BLOOM_MAX_BOOST       0.8f
#define SH4DER_BLOOM_MAX_BOOST_REACH 0.12f
#define SH4DER_BLOOM_MAX_DISC_RADIUS 0.24f
#define SH4DER_BLOOM_MAX_DISC_PEAK   1.8f
#define SH4DER_BLOOM_MAX_FLARE_FLOOR 0.5f
/** @} */

typedef struct sh4der_bloom_params {
    float x0, x1, tail;         /**< \brief Root, blade tip and tail length along the axis. */
    float half_width, inner;    /**< \brief Ribbon half width and its full-level core. */
    float root, tip;            /**< \brief Ribbon levels at the root and the blade tip (0 at the tail). */
    float tube_floor;           /**< \brief Share of the ribbon with no tube near, 0..1. */
    float boost, boost_reach;   /**< \brief The blocker's knot: level added, reach along the axis. */
    float disc_radius, disc_peak;   /**< \brief Tip disc radius and centre level. */
    float flare_floor, flare_cos;   /**< \brief The tip disc as a lens flare: x (floor + (1 - floor) smoothstep((cos - flare_cos) / (1 - flare_cos))),
                                         cos = the blade axis . the unit way to the eye. */
} sh4der_bloom_params_t;

/** \brief One bloom: positions in the blade's space and a level per vertex (before the colour; it may pass 1). */
typedef struct sh4der_bloom_mesh {
    float xyz[SH4DER_BLOOM_VERTS][3];
    float level[SH4DER_BLOOM_VERTS];
} sh4der_bloom_mesh_t;

/** \brief The triangles: the ribbon's 16 (row-major quads), then the disc's 6 (centre = vertex 15). */
extern const uint8_t sh4der_bloom_tris[SH4DER_BLOOM_TRIS][3];

/** \brief The default blade bloom tuning (the SH4DER_BLOOM_* values). */
void sh4der_bloom_params_default(sh4der_bloom_params_t *params);

/** \brief The MAX WHITE test (the SH4DER_BLOOM_MAX_* values over the default). */
void sh4der_bloom_params_max(sh4der_bloom_params_t *params);

/** \brief Validate params: 1 when sh4der_bloom_blade() would accept them. */
int sh4der_bloom_params_valid(const sh4der_bloom_params_t *params);

/** \brief A feathered disc facing `eye`: xyz[0] the centre at level `peak`, xyz[1..RIM] the rim at 0.
    Any glow point (the tip bloom, a light on a hologram's chest).
    \param xyz Output: 1 + RIM positions. xyz[0] is the centre.
    \param level Output: one level per position (the rim is 0).
    \param centre Disc centre, in the blade's space.
    \param radius Disc radius, same units.
    \param eye Camera position, same space: the disc faces it.
    \param peak Level at the centre. */
void sh4der_bloom_disc(float xyz[][3], float level[], const float centre[3], float radius, const float eye[3], float peak);

/** \brief The same disc with `rim` rim vertices (3..SH4DER_BLOOM_DISC_MAX; clamped): rounder, rim triangles
    (0, k, k + 1). The tip disc is sh4der_bloom_disc (6, one vertex down the blade); a round glow takes 16. */
void sh4der_bloom_disc_n(float xyz[][3], float level[], const float centre[3], float radius, const float eye[3], float peak, unsigned rim);

/** \brief A round disc with a soft edge of share `feather` (0: a hard edge .. 1: soft from the centre; clamped): the centre and an inner ring at
    (1 - feather) x radius at `peak`, the outer ring at `radius` at 0. 1 + 2 x rim vertices: 0 the centre, 1..rim the inner ring, rim + 1..2 rim the
    outer (rim clamped to 3..SH4DER_BLOOM_DISC_MAX); the triangles from sh4der_bloom_disc_feather_tris(). */
void sh4der_bloom_disc_feather(float xyz[][3], float level[], const float centre[3], float radius, float feather, const float eye[3], float peak,
                               unsigned rim);

/** \brief sh4der_bloom_disc_feather()'s triangles into tris (3 x rim of them: the centre's fan, then the band); the count. */
unsigned sh4der_bloom_disc_feather_tris(unsigned rim, uint8_t tris[][3]);

/** \brief The blade's bloom this frame: the ribbon and the tip disc, facing `eye` (the camera in the blade's space).
    `blade` and `frame` are the blade effect's params and this instance's frame (the tubes, the blocker, the pulse,
    the fade). Returns 0 and leaves every level 0 on bad params. */
int sh4der_bloom_blade(sh4der_bloom_mesh_t *mesh, const sh4der_bloom_params_t *params, const sh4der_blade_params_t *blade,
                       const sh4der_blade_frame_t *frame, const float eye[3]);

/** \brief Packed colours of the mesh on the ramp (sh4der_ramp_colours()), alpha `a`, out_stride bytes apart. */
void sh4der_bloom_argb(const sh4der_bloom_mesh_t *mesh, const float edge[3], const float core[3], uint8_t a, uint32_t *out, size_t out_stride);

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_BLOOM_H */
